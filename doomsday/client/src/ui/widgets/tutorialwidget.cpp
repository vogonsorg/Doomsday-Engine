/** @file tutorialdialog.cpp
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "ui/widgets/tutorialwidget.h"
#include "ui/clientwindow.h"
#include "ui/widgets/taskbarwidget.h"
#include "ui/widgets/inputbindingwidget.h"
#include "dd_version.h"
#include "dd_main.h"

#include <de/MessageDialog>
#include <de/SignalAction>

using namespace de;

DENG_GUI_PIMPL(TutorialWidget)
{
    enum Step {
        Welcome,
        TaskBar,
        DEMenu,
        ConfigMenus,
        ConsoleKey,
        Finish
    };

    Step current;
    LabelWidget *darken;
    MessageDialog *dlg;

    Instance(Public *i)
        : Base(i)
        , current(Welcome)
        , dlg(0)
    {
        self.add(darken = new LabelWidget);
        darken->set(Background(style().colors().colorf("altaccent") *
                               Vector4f(.3f, .3f, .3f, .55f)));
        darken->setOpacity(0);
    }

    void deinitStep()
    {
        if(dlg)
        {
            dlg->close(0);
            dlg = 0;
        }

        ClientWindow &win = ClientWindow::main();
        switch(current)
        {
        case DEMenu:
            win.taskBar().closeMainMenu();
            break;

        case ConfigMenus:
            win.taskBar().closeConfigMenu();
            break;

        default:
            break;
        }
    }

    void initStep(Step s)
    {
        deinitStep();

        if(s == Finish)
        {
            self.stop();
            return;
        }

        current = s;
        dlg = new MessageDialog;
        dlg->setDeleteAfterDismissed(true);
        dlg->setClickToClose(false);
        QObject::connect(dlg, SIGNAL(accepted(int)), thisPublic, SLOT(continueToNextStep()));
        QObject::connect(dlg, SIGNAL(rejected(int)), thisPublic, SLOT(stop()));
        dlg->buttons()
                << new DialogButtonItem(DialogWidget::Accept | DialogWidget::Default, tr("Continue"))
                << new DialogButtonItem(DialogWidget::Reject | DialogWidget::Action,  tr("Skip Tutorial"));

        // Insert the content for the dialog.
        ClientWindow &win = ClientWindow::main();
        switch(current)
        {
        case Welcome:
            dlg->title().setText(tr("Welcome to Doomsday"));
            dlg->message().setText(tr("%1 is a portable 2.5D game engine that allows "
                                      "you to play classic DOOM based games on modern platforms.\n\n"
                                      "This tutorial will introduce the central UI elements.")
                                   .arg(_E(b) DOOMSDAY_NICENAME _E(.)));
            dlg->setAnchor(self.rule().midX(), self.rule().top());
            dlg->setOpeningDirection(ui::Down);
            break;

        case TaskBar:
            dlg->title().setText(tr("Task Bar"));
            dlg->message().setText(tr("The task bar is where you find all the important features: loading "
                                      "and switching games, joining a multiplayer game, "
                                      "configuration settings, "
                                      "and a console command line for advanced users.\n\n"
                                      "Press %1 at any time to access the task bar.")
                                   .arg(_E(b)_E(D) "Shift-Esc" _E(.)_E(.)));

            win.taskBar().open();
            win.taskBar().closeMainMenu();
            win.taskBar().closeConfigMenu();
            dlg->setAnchor(self.rule().midX(), win.taskBar().rule().top());
            dlg->setOpeningDirection(ui::Up);
            break;

        case DEMenu:
            dlg->title().setText(tr("DE Menu"));
            dlg->message().setText(tr("Click in the bottom right corner to open the main menu of Doomsday. "
                                      "You can check for available updates, switch games, or look for "
                                      "ongoing multiplayer games."));
            win.taskBar().openMainMenu();
            dlg->setAnchorAndOpeningDirection(root().find("de-menu")->
                                              as<GuiWidget>().rule(), ui::Left);
            break;

        case ConfigMenus:
            dlg->title().setText(tr("Settings"));
            dlg->message().setText(tr("Configuration settings are under the Gear menu."));
            win.taskBar().openConfigMenu();
            dlg->setAnchorAndOpeningDirection(self.root().find("conf-menu")->
                                              as<GuiWidget>().rule(), ui::Left);
            break;

        case ConsoleKey: {
            dlg->title().setText(tr("Console"));
            String msg = tr("The console is a \"Quake style\" command line prompt where "
                            "you enter commands and change variable values. To get started, "
                            "try typing %1.").arg(_E(b) "help" _E(.));
            if(App_GameLoaded())
            {
                msg += "\n\nHere you can set a keyboard shortcut for accessing the console quickly. "
                        "Click in the box below and then press the key or key combination you "
                        "want to assign as the shortcut.";
                dlg->area().add(InputBindingWidget::newTaskBarShortcut());
            }
            dlg->message().setText(msg);
            dlg->setAnchor(win.taskBar().console().commandLine().rule().left() +
                           style().rules().rule("gap"),
                           win.taskBar().rule().top());
            dlg->setOpeningDirection(ui::Up);
            dlg->updateLayout();
            break; }

        default:
            break;
        }

        self.root().addOnTop(dlg);
        dlg->open();
    }
};

TutorialWidget::TutorialWidget()
    : GuiWidget("tutorial"), d(new Instance(this))
{}

void TutorialWidget::start()
{
    // Darken the entire view.
    d->darken->rule().setRect(rule());
    d->darken->setOpacity(1, 0.5);

    d->initStep(Instance::Welcome);
}

void TutorialWidget::stop()
{
    d->deinitStep();

    // Animate away and unfade darkening.
    d->darken->setOpacity(0, 0.5);
    QTimer::singleShot(500, this, SLOT(dismiss()));
}

void TutorialWidget::dismiss()
{
    hide();
    guiDeleteLater();
}

bool TutorialWidget::handleEvent(Event const &event)
{
    GuiWidget::handleEvent(event);

    // Eat everything!
    return true;
}

void TutorialWidget::continueToNextStep()
{
    d->initStep(Instance::Step(d->current + 1));
}