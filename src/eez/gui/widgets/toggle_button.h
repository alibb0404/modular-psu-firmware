/*
 * EEZ Modular Firmware
 * Copyright (C) 2015-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <eez/gui/widget.h>

namespace eez {
namespace gui {

struct ToggleButtonWidget {
#if OPTION_SDRAM    
    const char *text1;
#else
    uint32_t text1Offset;
#endif

#if OPTION_SDRAM    
    const char *text2;
#else
    uint32_t text2Offset;
#endif

};

void ToggleButtonWidget_fixPointers(Widget *widget);
void ToggleButtonWidget_draw(const WidgetCursor &widgetCursor);

} // namespace gui
} // namespace eez