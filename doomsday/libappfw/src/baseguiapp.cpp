/** @file baseguiapp.cpp  Base class for GUI applications.
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

#include "de/BaseGuiApp"
#include "de/VRConfig"

namespace de {

DENG2_PIMPL_NOREF(BaseGuiApp)
{
    GLShaderBank shaders;
    VRConfig vr;
};

BaseGuiApp::BaseGuiApp(int &argc, char **argv)
    : GuiApp(argc, argv), d(new Instance)
{}

BaseGuiApp &BaseGuiApp::app()
{
    return static_cast<BaseGuiApp &>(App::app());
}

GLShaderBank &BaseGuiApp::shaders()
{
    return app().d->shaders;
}

VRConfig &BaseGuiApp::vr()
{
    return app().d->vr;
}

} // namespace de
