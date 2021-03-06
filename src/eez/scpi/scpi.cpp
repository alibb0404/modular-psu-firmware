/*
* EEZ Generic Firmware
* Copyright (C) 2019-present, Envox d.o.o.
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
* along with this program.  If not, see http://www.gnu.org/licenses.
*/

#include <stdio.h> // sprintf

#include <eez/scpi/scpi.h>

#include <eez/system.h>
#include <eez/sound.h>
#include <eez/mp.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/list_program.h>
#include <eez/modules/psu/serial_psu.h>
#if OPTION_ETHERNET
#include <eez/modules/psu/ethernet.h>
#include <eez/modules/psu/ntp.h>
#endif
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/profile.h>
#if OPTION_SD_CARD
#include <eez/modules/psu/sd_card.h>
#include <eez/libs/sd_fat/sd_fat.h>
#include <eez/modules/psu/dlog_record.h>
#include <eez/modules/psu/dlog_view.h>
#include <eez/libs/image/jpeg.h>
#endif
#include <eez/modules/psu/scpi/psu.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/ontime.h>
#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/file_manager.h>

#include <eez/gui/data.h>
#include <eez/gui/dialogs.h>

using namespace eez::psu;
using namespace eez::psu::scpi;

namespace eez {
namespace scpi {

void mainLoop(const void *);

osThreadId g_scpiTaskHandle;

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif

osThreadDef(g_scpiTask, mainLoop, osPriorityNormal, 0, 8192);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic pop
#endif

osMessageQDef(g_scpiMessageQueue, SCPI_QUEUE_SIZE, uint32_t);
osMessageQId g_scpiMessageQueueId;

char g_listFilePath[CH_MAX][MAX_PATH_LENGTH];

uint32_t g_timer1LastTickCount;

bool onSystemStateChanged() {
    if (eez::g_systemState == eez::SystemState::BOOTING) {
        if (eez::g_systemStatePhase == 0) {
            g_scpiMessageQueueId = osMessageCreate(osMessageQ(g_scpiMessageQueue), NULL);
            return false;
        } else {
            g_timer1LastTickCount = micros();
            g_scpiTaskHandle = osThreadCreate(osThread(g_scpiTask), nullptr);
        }
    }

    return true;
}

void oneIter();

void mainLoop(const void *) {
#ifdef __EMSCRIPTEN__
    oneIter();
#else
    while (1) {
        oneIter();
    }
#endif
}

void oneIter() {
    osEvent event = osMessageGet(g_scpiMessageQueueId, 25);
    if (event.status == osEventMessage) {
    	uint32_t message = event.value.v;
    	uint32_t target = SCPI_QUEUE_MESSAGE_TARGET(message);
    	uint32_t type = SCPI_QUEUE_MESSAGE_TYPE(message);
    	uint32_t param = SCPI_QUEUE_MESSAGE_PARAM(message);
        if (target == SCPI_QUEUE_MESSAGE_TARGET_SERIAL) {
            serial::onQueueMessage(type, param);
        }
#if OPTION_ETHERNET
        else if (target == SCPI_QUEUE_MESSAGE_TARGET_ETHERNET) {
            ethernet::onQueueMessage(type, param);
        }
#endif  
        else if (target == SCPI_QUEUE_MESSAGE_TARGET_MP) {
            mp::onQueueMessage(type, param);
        } else if (target == SCPI_QUEUE_MESSAGE_TARGET_NONE) {
            if (type == SCPI_QUEUE_MESSAGE_TYPE_SAVE_LIST) {
                int err;
                if (!eez::psu::list::saveList(param, &g_listFilePath[param][0], &err)) {
                    generateError(err);
                }
            } else if (type == SCPI_QUEUE_MESSAGE_TYPE_DELETE_PROFILE_LISTS) {
                profile::deleteProfileLists(param);
            }
#if defined(EEZ_PLATFORM_STM32) && OPTION_SD_CARD
			else if (type == SCPI_QUEUE_MESSAGE_TYPE_SD_DETECT_IRQ) {
				eez::psu::sd_card::onSdDetectInterruptHandler();
			}
#endif
#if OPTION_SD_CARD
			else if (type == SCPI_QUEUE_MESSAGE_DLOG_FILE_WRITE) {
				eez::psu::dlog_record::fileWrite();
			}
            else if (type == SCPI_QUEUE_MESSAGE_DLOG_TOGGLE) {
                eez::psu::dlog_record::toggle();
            } else if (type == SCPI_QUEUE_MESSAGE_DLOG_SHOW_FILE) {
                eez::psu::dlog_view::openFile(nullptr);
            } else if (type == SCPI_QUEUE_MESSAGE_DLOG_LOAD_BLOCK) {
                eez::psu::dlog_view::loadBlock();
            } else if (type == SCPI_QUEUE_MESSAGE_ABORT_DOWNLOADING) {
                abortDownloading();
            } else if (type == SCPI_QUEUE_MESSAGE_SCREENSHOT) {
                if (!sd_card::isMounted(nullptr)) {
                    return;
                }

                sound::playShutter();

                const uint8_t *screenshotPixels = mcu::display::takeScreenshot();

                unsigned char* imageData;
                size_t imageDataSize;

                if (jpegEncode(screenshotPixels, &imageData, &imageDataSize)) {
                    event_queue::pushEvent(SCPI_ERROR_OUT_OF_MEMORY_FOR_REQ_OP);
                    return;
                }

                char filePath[40];
                uint8_t year, month, day, hour, minute, second;
                datetime::getDate(year, month, day);
                datetime::getTime(hour, minute, second);
                sprintf(filePath, "%s/%d_%02d_%02d-%02d_%02d_%02d.jpg",
                    SCREENSHOTS_DIR,
                    (int)(year + 2000), (int)month, (int)day,
                    (int)hour, (int)minute, (int)second);

                File file;
                if (file.open(filePath, FILE_CREATE_ALWAYS | FILE_WRITE)) {
                    size_t written = file.write(imageData, imageDataSize);

                    file.close();

                    if (written == imageDataSize) {
                    	// success!
                    	event_queue::pushEvent(event_queue::EVENT_INFO_SCREENSHOT_SAVED);
                    } else {
                        int err;
                        sd_card::deleteFile(filePath, &err);
                        event_queue::pushEvent(SCPI_ERROR_MASS_STORAGE_ERROR);
                    }
                } else {
                    event_queue::pushEvent(SCPI_ERROR_FILE_NAME_NOT_FOUND);
                }
            } else if (type == SCPI_QUEUE_MESSAGE_FILE_MANAGER_LOAD_DIRECTORY) {
                file_manager::loadDirectory();
            } else if (type == SCPI_QUEUE_MESSAGE_FILE_MANAGER_UPLOAD_FILE) {
                file_manager::uploadFile();
            } else if (type == SCPI_QUEUE_MESSAGE_FILE_MANAGER_OPEN_IMAGE_FILE) {
                file_manager::openImageFile();
            } else if (type == SCPI_QUEUE_MESSAGE_FILE_MANAGER_DELETE_FILE) {
                file_manager::deleteFile();
            } else if (type == SCPI_QUEUE_MESSAGE_DLOG_UPLOAD_FILE) {
                dlog_view::uploadFile();
            }
#endif // OPTION_SD_CARD
        }
    } else {
    	uint32_t tickCount = micros();
    	int32_t diff = tickCount - g_timer1LastTickCount;

        event_queue::tick();

        sound::tick();

    	if (diff >= 1000000L) { // 1 sec
            g_timer1LastTickCount = tickCount;

    		profile::tick();

#if OPTION_ETHERNET
    		ntp::tick();
#endif

            ontime::g_mcuCounter.tick(tickCount);
            for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
                if (g_slots[slotIndex].moduleInfo->moduleType != MODULE_TYPE_NONE) {
                    ontime::g_moduleCounters[slotIndex].tick(tickCount);
                }
            }
    	}

        persist_conf::tick();

#if OPTION_SD_CARD
        sd_card::tick();
#endif

#ifdef DEBUG
        psu::debug::tick(tickCount);
#endif
    }
}

void resetContext(scpi_t *context) {
    scpi_psu_t *psuContext = (scpi_psu_t *)context->user_context;
    psuContext->selected_channel_index = 0;
#if OPTION_SD_CARD
    psuContext->currentDirectory[0] = 0;
#endif
    SCPI_ErrorClear(context);
}

void resetContext() {
    // SYST:ERR:COUN? 0
    if (serial::g_testResult == TEST_OK) {
        scpi::resetContext(&serial::g_scpiContext);
    }

#if OPTION_ETHERNET
    if (ethernet::g_testResult == TEST_OK) {
        scpi::resetContext(&ethernet::g_scpiContext);
    }
#endif
}

void generateError(int error) {
    if (serial::g_testResult == TEST_OK) {
        SCPI_ErrorPush(&serial::g_scpiContext, error);
    }
#if OPTION_ETHERNET
    if (ethernet::g_testResult == TEST_OK) {
        SCPI_ErrorPush(&ethernet::g_scpiContext, error);
    }
#endif
    event_queue::pushEvent(error);
}

}
}
