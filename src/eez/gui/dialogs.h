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

#include <stdlib.h>

#include <eez/gui/data.h>

namespace eez {
namespace gui {

extern data::Value g_alertMessage;
extern data::Value g_alertMessage2;
extern data::Value g_alertMessage3;
extern data::Value g_progress;

void showAsyncOperationInProgress(const char *message, void (*checkStatus)() = 0);
void hideAsyncOperationInProgress();

void infoMessage(const char *message);
void infoMessage(data::Value value);
void infoMessage(const char *message1, const char *message2);
void errorMessage(const char *message);
void errorMessage(const char *message1, const char *message2);
void errorMessage(const char *message1, const char *message2, const char *message3);
void errorMessage(data::Value value);
void errorMessageWithAction(data::Value value, void (*action)(int param), const char *actionLabel, int actionParam);
void errorMessageWithAction(const char *message, void (*action)(), const char *actionLabel);

void yesNoDialog(int yesNoPageId, const char *message, void (*yes_callback)(),
                 void (*no_callback)(), void (*cancel_callback)());
void yesNoLater(const char *message, void (*yes_callback)(), void (*no_callback)(),
                void (*later_callback)() = 0);
void areYouSure(void (*yes_callback)());
void areYouSureWithMessage(const char *message, void (*yes_callback)());

void dialogYes();
void dialogNo();
void dialogCancel();
void dialogOk();
void dialogLater();

enum MenuType {
    MENU_TYPE_BUTTON
};

static const size_t MAX_MENU_ITEMS = 4;

void showMenu(AppContext *appContext, const char *message, MenuType menuType, const char **menuItems, void(*callback)(int));

} // namespace gui
} // namespace eez
