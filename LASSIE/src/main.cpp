#include "inst.hpp"

#include <QApplication>
#include <QFile>
#ifdef DISSCO_ENABLE_UI_LAYOUT_TESTS
#include <QDebug>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include "widgets/EventAttributesViewController.hpp"
#include "widgets/Modifiers.hpp"
#include "windows/EnvelopeLibraryWindow.hpp"
#endif
#include "widgets/ComboBoxWheelGuard.hpp"
#include "widgets/TextOverflowDisplayPolicy.hpp"
#include "windows/MainWindow.hpp"

void registerAllFunctions();

int main(int argc, char *argv[])
{
    // QSettings (used here and in the Updater) needs stable org/app
    // metadata; otherwise its on-disk location is platform-default and
    // varies between binaries. Set before QApplication is constructed.
    QCoreApplication::setOrganizationName("DISSCO");
    QCoreApplication::setOrganizationDomain("dissco.illinois.edu");
    QCoreApplication::setApplicationName("LASSIE");

    QApplication a(argc, argv);
    ComboBoxWheelGuard comboBoxWheelGuard(a);
    TextOverflowDisplayPolicy textOverflowDisplayPolicy(a);

#ifdef DISSCO_ENABLE_UI_LAYOUT_TESTS
    if (qEnvironmentVariableIsSet("DISSCO_TEST_ENVELOPE_LAYOUT")) {
        EnvelopeLibraryWindow window;
        window.ensurePolished();
        window.show();
        for (int i = 0; i < 3; ++i) {
            window.centralWidget()->layout()->activate();
            QApplication::processEvents();
        }

        QLabel* legend = window.findChild<QLabel*>("envelopeLegend");
        if (!legend) {
            qCritical() << "Envelope legend was not found";
            return 1;
        }

        const QRect legendInCentral(
            legend->mapTo(window.centralWidget(), QPoint()), legend->size());
        const int requiredHeight = legend->heightForWidth(legend->width());
        const bool fullyVisible =
            window.centralWidget()->rect().contains(legendInCentral)
            && legend->visibleRegion().contains(legend->rect())
            && legend->height() >= requiredHeight;

        if (!fullyVisible) {
            qCritical() << "Envelope legend is clipped:"
                        << "actual" << legend->size()
                        << "required height" << requiredHeight
                        << "visible region" << legend->visibleRegion();
        }

        return fullyVisible ? 0 : 1;
    }

    // Regression test for a modifier row's move-up/move-down buttons going
    // stale after a later modifier is added -- Tony Chai, 2026.
    if (qEnvironmentVariableIsSet("DISSCO_TEST_MODIFIER_ADD_REFRESH")) {
        ProjectManager* projectManager = Inst::get_project_manager();
        projectManager->set_curr_project(projectManager->create());

        EventAttributesViewController controller(nullptr);
        controller.show();
        controller.showAttributesOfEvent(top, 0);

        QPushButton* addButton =
            controller.findChild<QPushButton*>("addModifierButton");
        if (!addButton) {
            qCritical() << "addModifierButton was not found";
            return 1;
        }

        bool allCorrect = true;
        for (int addCount = 1; addCount <= 3; ++addCount) {
            addButton->click();
            // rebuildModifierRows() deleteLater()s the old rows -- flush
            // that before counting, or the about-to-be-destroyed ones are
            // still findable as children for one more event loop turn.
            QApplication::processEvents();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QApplication::processEvents();

            const QList<Modifiers*> rows =
                controller.findChildren<Modifiers*>();
            if (rows.size() != addCount) {
                qCritical() << "Expected" << addCount
                            << "modifier rows after add #" << addCount
                            << ", got" << rows.size();
                allCorrect = false;
                break;
            }
            for (int i = 0; i < rows.size(); ++i) {
                QPushButton* moveUp =
                    rows[i]->findChild<QPushButton*>("moveUpButton");
                QPushButton* moveDown =
                    rows[i]->findChild<QPushButton*>("moveDownButton");
                if (!moveUp || !moveDown) {
                    qCritical() << "Row" << i << "is missing move buttons";
                    allCorrect = false;
                    continue;
                }
                const bool expectUpEnabled = i > 0;
                const bool expectDownEnabled = i + 1 < rows.size();
                if (moveUp->isEnabled() != expectUpEnabled
                    || moveDown->isEnabled() != expectDownEnabled) {
                    qCritical()
                        << "Row" << i << "after add #" << addCount
                        << "has stale move-button state: up enabled ="
                        << moveUp->isEnabled() << "(expected"
                        << expectUpEnabled << "), down enabled ="
                        << moveDown->isEnabled() << "(expected"
                        << expectDownEnabled << ")";
                    allCorrect = false;
                }
            }
        }

        return allCorrect ? 0 : 1;
    }
#endif

    registerAllFunctions();
    Inst *m = Inst::instance();
    MainWindow *w = new MainWindow(m);
    w->show();
    return a.exec();
}
