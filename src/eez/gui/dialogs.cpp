/* / mcu / sound.h
 * EEZ Generic Firmware
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

#include <eez/gui/dialogs.h>

#include <math.h>
#include <string.h>

#include <eez/gui/app_context.h>
#include <eez/system.h>
#include <eez/util.h>

// TODO this must be removed from here
#include <eez/apps/psu/psu.h>

#include <eez/apps/psu/gui/psu.h>
#include <eez/sound.h>

namespace eez {
namespace gui {

Value g_alertMessage;
Value g_alertMessage2;
Value g_alertMessage3;
Value g_progress;

char g_throbber[8] = { '|', '/', '-', '\\', '|', '/', '-', '\\' };

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

void showProgressPage(const char *message, void (*abortCallback)()) {
    data::set(data::Cursor(), DATA_ID_ALERT_MESSAGE, data::Value(message), 0);
    g_appContext->m_dialogCancelCallback = abortCallback;
    pushPage(PAGE_ID_PROGRESS);
}

bool updateProgressPage(size_t processedSoFar, size_t totalSize) {
    if (getActivePageId() == PAGE_ID_PROGRESS) {
        if (totalSize > 0) {
            g_progress = data::Value((int)round((processedSoFar * 1.0f / totalSize) * 100.0f),
                                     VALUE_TYPE_PERCENTAGE);
        } else {
            g_progress = data::Value((uint32_t)processedSoFar, VALUE_TYPE_SIZE);
        }
        return true;
    }
    return false;
}

void hideProgressPage() {
    if (getActivePageId() == PAGE_ID_PROGRESS) {
        popPage();
    }
}

////////////////////////////////////////////////////////////////////////////////

void setTextMessage(const char *message, unsigned int len) {
    strncpy(g_appContext->m_textMessage, message, len);
    g_appContext->m_textMessage[len] = 0;
    ++g_appContext->m_textMessageVersion;
    if (getActivePageId() != PAGE_ID_TEXT_MESSAGE) {
        pushPage(PAGE_ID_TEXT_MESSAGE);
    }
}

void clearTextMessage() {
    if (getActivePageId() == PAGE_ID_TEXT_MESSAGE) {
        popPage();
    }
}

const char *getTextMessage() {
    return g_appContext->m_textMessage;
}

uint8_t getTextMessageVersion() {
    return g_appContext->m_textMessageVersion;
}

////////////////////////////////////////////////////////////////////////////////

void infoMessage(const char *message, void (*callback)()) {
    pushPage(INTERNAL_PAGE_ID_INFO, new AlertMessagePage(INFO_ALERT, message, callback));
}

void infoMessage(const char *message1, const char *message2, void(*callback)()) {
    pushPage(INTERNAL_PAGE_ID_INFO, new AlertMessagePage(INFO_ALERT, message1, message2, callback));
}

void toastMessage(const char *message1, const char *message2, const char *message3, void (*callback)()) {
    pushPage(INTERNAL_PAGE_ID_INFO, new AlertMessagePage(TOAST_ALERT, message1, message2, message3, callback));
}

void errorMessage(const char *message, void (*callback)()) {
    pushPage(INTERNAL_PAGE_ID_INFO, new AlertMessagePage(ERROR_ALERT, message, callback));
    sound::playBeep();
}

void errorMessage(const char *message1, const char *message2, void (*callback)()) {
    pushPage(INTERNAL_PAGE_ID_INFO, new AlertMessagePage(ERROR_ALERT, message1, message2, callback));
    sound::playBeep();
}

void errorMessage(data::Value value, void (*callback)()) {
    pushPage(INTERNAL_PAGE_ID_INFO, new AlertMessagePage(ERROR_ALERT, value, callback));
    sound::playBeep();
}

void errorMessageWithAction(data::Value value, void (*ok_callback)(),
                            void (*action)(int param), const char *actionLabel, int actionParam) {
    g_appContext->m_errorMessageAction = action;
    data::set(data::Cursor(), DATA_ID_ALERT_MESSAGE_2, actionLabel, 0);
    g_appContext->m_errorMessageActionParam = actionParam;

    data::set(data::Cursor(), DATA_ID_ALERT_MESSAGE, value, 0);
    g_appContext->m_dialogYesCallback = ok_callback;
    pushPage(PAGE_ID_ERROR_ALERT_WITH_ACTION);

    sound::playBeep();
}

void errorMessageAction() {
    popPage();

    if (g_appContext->m_dialogYesCallback) {
        g_appContext->m_dialogYesCallback();
    }

    g_appContext->m_errorMessageAction(g_appContext->m_errorMessageActionParam);
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

} // namespace gui
} // namespace eez
