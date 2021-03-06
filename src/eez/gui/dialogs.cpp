/* / mcu / sound.h
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

#if OPTION_DISPLAY

#include <eez/gui/dialogs.h>

#include <math.h>
#include <string.h>

#include <eez/gui/app_context.h>
#include <eez/system.h>
#include <eez/util.h>

// TODO this must be removed from here
#include <eez/modules/psu/psu.h>

#include <eez/modules/psu/gui/psu.h>
#include <eez/sound.h>

namespace eez {
namespace gui {

Value g_alertMessage;
Value g_alertMessage2;
Value g_alertMessage3;
Value g_progress;

////////////////////////////////////////////////////////////////////////////////

void showAsyncOperationInProgress(const char *message, void (*checkStatus)()) {
    data::set(data::Cursor(), DATA_ID_ALERT_MESSAGE, data::Value(message), 0);
    g_appContext->m_checkAsyncOperationStatus = checkStatus;
    pushPage(PAGE_ID_ASYNC_OPERATION_IN_PROGRESS);
}

void hideAsyncOperationInProgress() {
    popPage();
}

////////////////////////////////////////////////////////////////////////////////

void pushToastMessage(ToastMessagePage *toastMessage) {
    pushPage(INTERNAL_PAGE_ID_TOAST_MESSAGE, toastMessage);
}

void infoMessage(const char *message) {
    if (osThreadGetId() != g_guiTaskHandle) {
        eez::psu::gui::g_psuAppContext.infoMessage(message);
    } else {
        pushToastMessage(ToastMessagePage::create(INFO_TOAST, message));
    }
}

void infoMessage(data::Value value) {
    pushToastMessage(ToastMessagePage::create(INFO_TOAST, value));
}


void infoMessage(const char *message1, const char *message2) {
    pushToastMessage(ToastMessagePage::create(INFO_TOAST, message1, message2));
}

void errorMessage(const char *message) {
    pushToastMessage(ToastMessagePage::create(ERROR_TOAST, message));
    sound::playBeep();
}

void errorMessage(const char *message1, const char *message2) {
    pushToastMessage(ToastMessagePage::create(ERROR_TOAST, message1, message2));
    sound::playBeep();
}

void errorMessage(const char *message1, const char *message2, const char *message3) {
    pushToastMessage(ToastMessagePage::create(ERROR_TOAST, message1, message2, message3));
    sound::playBeep();
}
void errorMessage(data::Value value) {
    pushToastMessage(ToastMessagePage::create(ERROR_TOAST, value));
    sound::playBeep();
}

void errorMessageWithAction(data::Value value, void (*action)(int param), const char *actionLabel, int actionParam) {
    pushToastMessage(ToastMessagePage::create(ERROR_TOAST, value, action, actionLabel, actionParam));
    sound::playBeep();
}

void errorMessageWithAction(const char *message, void (*action)(), const char *actionLabel) {
    pushToastMessage(ToastMessagePage::create(ERROR_TOAST, message, action, actionLabel));
    sound::playBeep();
}

////////////////////////////////////////////////////////////////////////////////

void yesNoDialog(int yesNoPageId, const char *message, void (*yes_callback)(),
                 void (*no_callback)(), void (*cancel_callback)()) {
    data::set(data::Cursor(), DATA_ID_ALERT_MESSAGE, data::Value(message), 0);

    g_appContext->m_dialogYesCallback = yes_callback;
    g_appContext->m_dialogNoCallback = no_callback;
    g_appContext->m_dialogCancelCallback = cancel_callback;

    pushPage(yesNoPageId);
}

void yesNoLater(const char *message, void (*yes_callback)(), void (*no_callback)(),
                void (*later_callback)()) {
    data::set(data::Cursor(), DATA_ID_ALERT_MESSAGE, data::Value(message), 0);

    g_appContext->m_dialogYesCallback = yes_callback;
    g_appContext->m_dialogNoCallback = no_callback;
    g_appContext->m_dialogLaterCallback = later_callback;

    pushPage(PAGE_ID_YES_NO_LATER);
}

void areYouSure(void (*yes_callback)()) {
    yesNoDialog(PAGE_ID_YES_NO, "Are you sure?", yes_callback, 0, 0);
}

void areYouSureWithMessage(const char *message, void (*yes_callback)()) {
    yesNoDialog(PAGE_ID_ARE_YOU_SURE_WITH_MESSAGE, message, yes_callback, 0, 0);
}

////////////////////////////////////////////////////////////////////////////////

void dialogYes() {
    auto callback = g_appContext->m_dialogYesCallback;

    popPage();

    if (callback) {
        callback();
    }
}

void dialogNo() {
    auto callback = g_appContext->m_dialogNoCallback;

    popPage();

    if (callback) {
        callback();
    }
}

void dialogCancel() {
    auto callback = g_appContext->m_dialogCancelCallback;

    popPage();

    if (callback) {
        callback();
    }
}

void dialogOk() {
    dialogYes();
}

void dialogLater() {
    auto callback = g_appContext->m_dialogLaterCallback;

    popPage();

    if (callback) {
        callback();
    }
}

////////////////////////////////////////////////////////////////////////////////

void showMenu(AppContext *appContext, const char *message, MenuType menuType, const char **menuItems, void(*callback)(int)) {
    if (menuType == MENU_TYPE_BUTTON) {
        pushPage(INTERNAL_PAGE_ID_MENU_WITH_BUTTONS, MenuWithButtonsPage::create(appContext, message, menuItems, callback));
    }
}

} // namespace gui
} // namespace eez

#endif