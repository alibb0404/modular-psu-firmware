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
#include <assert.h>

#include <eez/modules/psu/psu.h>

#include <ctype.h>
#include <string.h>

#include <eez/modules/psu/gui/edit_mode_keypad.h>
#include <eez/modules/psu/gui/keypad.h>
#include <eez/modules/psu/gui/numeric_keypad.h>
#include <eez/modules/psu/gui/psu.h>
#include <eez/gui/widgets/text.h>
#include <eez/sound.h>
#include <eez/system.h>

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#define CONF_GUI_KEYPAD_CURSOR_BLINK_TIME 750000UL
#define CONF_GUI_KEYPAD_CURSOR_ON "|"
#define CONF_GUI_KEYPAD_CURSOR_OFF " "

#define CONF_GUI_KEYPAD_PASSWORD_LAST_CHAR_VISIBLE_DURATION 500000UL

namespace eez {
namespace psu {
namespace gui {

static Keypad g_keypadsPool[1];

Keypad* getFreeKeypad() {
    for (unsigned int i = 0; i < sizeof (g_keypadsPool) / sizeof(Keypad); ++i) {
        if (g_keypadsPool[i].m_isFree) {
            return &g_keypadsPool[i];
        }
    }
    assert(false);
    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////

Keypad *getActiveKeypad() {
    if (getActivePageId() == PAGE_ID_KEYPAD || getActivePageId() == PAGE_ID_NUMERIC_KEYPAD) {
        return (Keypad *)getActivePage();
    }
    
#if defined(EEZ_PLATFORM_SIMULATOR)
    if (getActivePageId() == PAGE_ID_FRONT_PANEL_NUMERIC_KEYPAD) {
        return (Keypad *)getActivePage();
    }
#endif

    if (getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD) {
        return edit_mode_keypad::g_keypad;
    }
    
    return 0;
}

NumericKeypad *getActiveNumericKeypad() {
    if (getActivePageId() == PAGE_ID_NUMERIC_KEYPAD) {
        return (NumericKeypad *)getActivePage();
    }
    
#if defined(EEZ_PLATFORM_SIMULATOR)
    if (getActivePageId() == PAGE_ID_FRONT_PANEL_NUMERIC_KEYPAD) {
        return (NumericKeypad *)getActivePage();
    }
#endif

    if (getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD) {
        return edit_mode_keypad::g_keypad;
    }
    
    return 0;
}


////////////////////////////////////////////////////////////////////////////////

void Keypad::pageAlloc() {
    m_isFree = false;
}

void Keypad::pageFree() {
    m_isFree = true;
}

void Keypad::init(const char *label_) {
    m_label[0] = 0;
    m_keypadText[0] = 0;
    m_okCallback = 0;
    m_cancelCallback = 0;
    m_keypadMode = KEYPAD_MODE_LOWERCASE;
    m_isPassword = false;

    if (label_) {
        strcpy(m_label, label_);
    } else {
        m_label[0] = 0;
    }

    m_lastCursorChangeTime = micros();
}

data::Value Keypad::getKeypadTextValue() {
    char *text = &m_stateText[getCurrentStateBufferIndex()][0];
    getKeypadText(text);
    return data::Value(text);
}

void Keypad::getKeypadText(char *text) {
    // text
    char *textPtr = text;

    if (*m_label) {
        strcpy(textPtr, m_label);
        textPtr += strlen(m_label);
    }

    if (m_isPassword) {
        int n = strlen(m_keypadText);
        if (n > 0) {
            int i;

            for (i = 0; i < n - 1; ++i) {
                *textPtr++ = '*';
            }

            uint32_t current_time = micros();
            if (current_time - m_lastKeyAppendTime <=
                CONF_GUI_KEYPAD_PASSWORD_LAST_CHAR_VISIBLE_DURATION) {
                *textPtr++ = m_keypadText[i];
            } else {
                *textPtr++ = '*';
            }

            *textPtr++ = 0;
        } else {
            *textPtr = 0;
        }
    } else {
        strcpy(textPtr, m_keypadText);
    }

    appendCursor(text);
}

void Keypad::start(const char *label, const char *text, int minChars_, int maxChars_, bool isPassword_, void (*ok)(char *), void (*cancel)()) {
    init(label);

    m_okCallback = ok;
    m_cancelCallback = cancel;

    m_minChars = minChars_;
    m_maxChars = maxChars_;
    m_isPassword = isPassword_;

    if (text) {
        strcpy(m_keypadText, text);
    } else {
        m_keypadText[0] = 0;
    }
    m_keypadMode = KEYPAD_MODE_LOWERCASE;
}

void Keypad::startPush(const char *label, const char *text, int minChars_, int maxChars_, bool isPassword_, void (*ok)(char *), void (*cancel)()) {
    Keypad *page = getFreeKeypad();
    page->start(label, text, minChars_, maxChars_, isPassword_, ok, cancel);
    pushPage(PAGE_ID_KEYPAD, page);
}

void Keypad::startReplace(const char *label, const char *text, int minChars_, int maxChars_, bool isPassword_, void (*ok)(char *), void (*cancel)()) {
    Keypad *page = getFreeKeypad();
    page->start(label, text, minChars_, maxChars_, isPassword_, ok, cancel);
    replacePage(PAGE_ID_KEYPAD, page);
}

void Keypad::appendChar(char c) {
    int n = strlen(m_keypadText);
    if (n < m_maxChars && (n + (*m_label ? strlen(m_label) : 0)) < MAX_KEYPAD_TEXT_LENGTH) {
        m_keypadText[n] = c;
        m_keypadText[n + 1] = 0;
        m_lastKeyAppendTime = micros();
    } else {
        sound::playBeep();
    }
}

void Keypad::key() {
    const TextWidgetSpecific *textWidget = GET_WIDGET_PROPERTY(getFoundWidgetAtDown().widget, specific, const TextWidgetSpecific *);
    key(GET_WIDGET_PROPERTY(textWidget, text, const char *)[0]);
}

void Keypad::key(char ch) {
    appendChar(ch);
}

void Keypad::space() {
    appendChar(' ');
}

void Keypad::back() {
    int n = strlen(m_keypadText);
    if (n > 0) {
        m_keypadText[n - 1] = 0;
    } else {
        sound::playBeep();
    }
}

void Keypad::clear() {
    m_keypadText[0] = 0;
}

void Keypad::sign() {
}

void Keypad::unit() {
}

void Keypad::option1() {
}

void Keypad::option2() {
}

void Keypad::setMaxValue() {
}

void Keypad::setMinValue() {
}

void Keypad::setDefValue() {
}

bool Keypad::isOkEnabled() {
    int n = strlen(m_keypadText);
    return n >= m_minChars && n <= m_maxChars;
}

void Keypad::ok() {
    m_okCallback(m_keypadText);
}

void Keypad::cancel() {
    if (m_cancelCallback) {
        m_cancelCallback();
    } else {
        popPage();
    }
}

void Keypad::appendCursor(char *text) {
    uint32_t current_time = micros();
    if (current_time - m_lastCursorChangeTime > CONF_GUI_KEYPAD_CURSOR_BLINK_TIME) {
        m_cursor = !m_cursor;
        m_lastCursorChangeTime = current_time;
    }

    if (m_cursor) {
        strcat(text, CONF_GUI_KEYPAD_CURSOR_ON);
    } else {
        strcat(text, CONF_GUI_KEYPAD_CURSOR_OFF);
    }
}

} // namespace gui
} // namespace psu
} // namespace eez

#endif
