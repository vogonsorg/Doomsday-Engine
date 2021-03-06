/** @file renderersettingsdialog.h  Settings for the renderer.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_RENDERERSETTINGSDIALOG_H
#define DE_CLIENT_RENDERERSETTINGSDIALOG_H

#include <de/dialogwidget.h>

/**
 * Dialog for modifying input settings.
 */
class RendererSettingsDialog : public de::DialogWidget
{
public:
    RendererSettingsDialog(const de::String &name = "renderersettings");

    void resetToDefaults();

protected:
    void showDeveloperPopup();
    void editProfile();

    void finish(int result) override;

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_RENDERERSETTINGSDIALOG_H
