/** @file dialogwidget.h  Popup dialog.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_DIALOGWIDGET_H
#define DENG_CLIENT_DIALOGWIDGET_H

#include "popupwidget.h"
#include "scrollareawidget.h"
#include "menuwidget.h"

class GuiRootWidget;

/**
 * Popup dialog.
 *
 * The content area of a dialog is scrollable. A menu with buttons is placed in
 * the bottom of the dialog, for the actions available to the user.
 *
 * The contents of the dialog should be placed under the scroll area returned
 * by DialogWidget::content() and positioned in relation to its content rule.
 * When the dialog is set up, one must define the size of the content scroll
 * area (width and height rules).
 */
class DialogWidget : public PopupWidget
{
    Q_OBJECT

public:
    /**
     * Modality of the dialog. By default, dialogs are modal, meaning that
     * while they are open, no events can get past the dialog.
     */
    enum Modality {
        Modal,
        Nonmodal
    };

public:
    DialogWidget(de::String const &name = "");

    void setModality(Modality modality);

    Modality modality() const;

    ScrollAreaWidget &content();

    MenuWidget &buttons();

    /**
     * Shows the dialog and blocks execution until the dialog is closed --
     * another event loop is started for event processing. Call either accept()
     * or reject() to dismiss the dialog.
     *
     * @param root  Root where to execute the dialog.
     *
     * @return Result code.
     */
    int exec(GuiRootWidget &root);

    // Events.
    bool handleEvent(de::Event const &event);

public slots:
    void accept(int result = 1);
    void reject(int result = 0);

signals:
    void accepted(int result);
    void rejected(int result);

protected:
    void preparePopupForOpening();

    /**
     * Derived classes can override this to do additional tasks before
     * execution of the dialog begins. DialogWidget::prepare() must be called
     * from the overridding methods.
     */
    virtual void prepare();

    /**
     * Handles any tasks needed when the dialog is closing.
     * DialogWidget::finish() must be called from overridding methods.
     *
     * @param result  Result code.
     */
    virtual void finish(int result);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_DIALOGWIDGET_H
