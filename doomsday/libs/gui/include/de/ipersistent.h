/** @file ipersistent.h
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBAPPFW_IPERSISTENT_H
#define LIBAPPFW_IPERSISTENT_H

#include "libgui.h"

namespace de {

class PersistentState;

/**
 * Interface for objects whose state can be stored persistently using PersistentState.
 *
 * GuiWidget instances that implement IPersistent will automatically be saved and
 * restored when the widget is (de)initialized.
 *
 * @ingroup appfw
 */
class LIBGUI_PUBLIC IPersistent
{
public:
    virtual ~IPersistent() = default;

    virtual void operator >> (PersistentState &toState) const = 0;
    virtual void operator << (const PersistentState &fromState) = 0;
};

} // namespace de

#endif // LIBAPPFW_IPERSISTENT_H
