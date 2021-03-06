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

namespace eez {
namespace psu {
namespace devices {

struct Device {
    const char *deviceName;
    bool installed;
    TestResult *testResult;
};

extern Device devices[];
extern int numDevices;

bool anyFailed();
void getSelfTestResultString(char *, int MAX_LENGTH);

const char *getInstalledString(bool installed);
const char *getTestResultString(TestResult g_testResult);

} // namespace devices
} // namespace psu
} // namespace eez
