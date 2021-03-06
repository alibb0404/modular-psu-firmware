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

#if OPTION_DISPLAY

#include <eez/modules/psu/psu.h>

#include <string.h>

#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/gui/keypad.h>
#include <eez/modules/psu/gui/page_user_profiles.h>

#include <eez/modules/psu/gui/psu.h>
#include <eez/gui/dialogs.h>
#include <eez/gui/document.h>

#include <scpi/scpi.h>

using namespace eez::psu::gui;

namespace eez {
namespace psu {
namespace gui {

static int g_selectedProfileLocation = -1;

int UserProfilesPage::getSelectedProfileLocation() {
    if (getActivePageId() == PAGE_ID_USER_PROFILE_0_SETTINGS || getActivePageId() == PAGE_ID_USER_PROFILE_SETTINGS) {
        return g_selectedProfileLocation;
    }
    return -1;
}

void UserProfilesPage::showProfile() {
    g_selectedProfileLocation = getFoundWidgetAtDown().cursor.i;
    pushPage(g_selectedProfileLocation == 0 ? PAGE_ID_USER_PROFILE_0_SETTINGS : PAGE_ID_USER_PROFILE_SETTINGS);
}

void UserProfilesPage::toggleAutoRecall() {
    bool enable = persist_conf::isProfileAutoRecallEnabled() ? false : true;
    persist_conf::enableProfileAutoRecall(enable);
}

void UserProfilesPage::toggleIsAutoRecallLocation() {
    if (g_selectedProfileLocation == 0 || profile::isValid(g_selectedProfileLocation)) {
        persist_conf::setProfileAutoRecallLocation(g_selectedProfileLocation);
    }
}

void UserProfilesPage::recall() {
    if (g_selectedProfileLocation > 0 && profile::isValid(g_selectedProfileLocation)) {
        int err;
        if (profile::recall(g_selectedProfileLocation, &err)) {
            infoMessage("Profile parameters loaded");
        } else {
            if (err == SCPI_ERROR_PROFILE_MODULE_MISMATCH) {
                errorMessage("Module mismatch in profile!");
            } else {
                errorMessage("Failed!");
            }
        }
    }
}

void UserProfilesPage::onSaveFinish(char *remark, void (*callback)()) {
    callback();
    if (profile::saveAtLocation(g_selectedProfileLocation, remark)) {
        infoMessage("Current parameters saved");
    } else {
        errorMessage("EEPROM save failed!");
    }
}

void UserProfilesPage::onSaveEditRemarkOk(char *remark) {
    onSaveFinish(remark, popPage);
}

void UserProfilesPage::onSaveYes() {
    if (g_selectedProfileLocation > 0) {
        char remark[PROFILE_NAME_MAX_LENGTH + 1];
        profile::getSaveName(g_selectedProfileLocation, remark);

        Keypad::startPush(0, remark, 0, PROFILE_NAME_MAX_LENGTH, false, onSaveEditRemarkOk, 0);
    } else {
        onSaveFinish();
    }
}

void UserProfilesPage::save() {
    if (g_selectedProfileLocation > 0) {
        if (profile::isValid(g_selectedProfileLocation)) {
            areYouSure(onSaveYes);
        } else {
            onSaveYes();
        }
    }
}

void UserProfilesPage::onDeleteProfileYes() {
    if (g_selectedProfileLocation > 0 && profile::isValid(g_selectedProfileLocation)) {
        if (profile::deleteLocation(g_selectedProfileLocation)) {
            infoMessage("Profile deleted");
        } else {
            errorMessage("Failed!");
        }
    }
}

void UserProfilesPage::deleteProfile() {
    if (g_selectedProfileLocation > 0 && profile::isValid(g_selectedProfileLocation)) {
        areYouSure(onDeleteProfileYes);
    }
}

void UserProfilesPage::onEditRemarkOk(char *newRemark) {
    popPage();

    if (profile::setName(g_selectedProfileLocation, newRemark, strlen(newRemark))) {
        infoMessage("Remark changed");
    } else {
        errorMessage("Failed!");
    }
}

void UserProfilesPage::editRemark() {
    if (g_selectedProfileLocation > 0 && profile::isValid(g_selectedProfileLocation)) {
        char remark[PROFILE_NAME_MAX_LENGTH + 1];
        profile::getName(g_selectedProfileLocation, remark, sizeof(remark));
        Keypad::startPush(0, remark, 0, PROFILE_NAME_MAX_LENGTH, false, onEditRemarkOk, 0);
    }
}

} // namespace gui
} // namespace psu
} // namespace eez

#endif