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

#include <eez/modules/psu/psu.h>

#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/io_pins.h>
#include <eez/modules/psu/list_program.h>
#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/trigger.h>
#include <eez/scpi/regs.h>
#include <eez/modules/psu/trigger.h>
#include <eez/system.h>

#if OPTION_SD_CARD
#include <eez/modules/psu/dlog_record.h>
#endif

namespace eez {
namespace psu {
namespace trigger {

static struct {
    float u;
    float i;
} g_levels[CH_MAX];

enum State { STATE_IDLE, STATE_INITIATED, STATE_TRIGGERED, STATE_EXECUTING };
static State g_state;
static uint32_t g_triggeredTime;

bool g_triggerInProgress[CH_MAX];

void setState(State newState) {
    if (g_state != newState) {
        if (newState == STATE_INITIATED) {
            for (int i = 0; i < CH_NUM; ++i) {
                Channel &channel = Channel::get(i);
                channel.setOperBits(OPER_ISUM_TRIG, true);
            }
        }

        if (g_state == STATE_INITIATED) {
            for (int i = 0; i < CH_NUM; ++i) {
                Channel &channel = Channel::get(i);
                channel.setOperBits(OPER_ISUM_TRIG, false);
            }
        }

        g_state = newState;
    }
}

void reset() {
    persist_conf::resetTrigger();

    setState(STATE_IDLE);
}

void init() {
    setState(STATE_IDLE);

    if (isContinuousInitializationEnabled()) {
        initiate();
    }
}

void setDelay(float delay) {
    persist_conf::setTriggerDelay(delay);
}

float getDelay() {
    return persist_conf::devConf.triggerDelay;
}

void setSource(Source source) {
    persist_conf::setTriggerSource(source);
}

Source getSource() {
    return (Source)persist_conf::devConf.triggerSource;
}

void setVoltage(Channel &channel, float value) {
    value = roundPrec(value, channel.getVoltageResolution());
    g_levels[channel.channelIndex].u = value;
}

float getVoltage(Channel &channel) {
    return g_levels[channel.channelIndex].u;
}

void setCurrent(Channel &channel, float value) {
    value = roundPrec(value, channel.getCurrentResolution(value));
    g_levels[channel.channelIndex].i = value;
}

float getCurrent(Channel &channel) {
    return g_levels[channel.channelIndex].i;
}

void check(uint32_t currentTime) {
    if (currentTime - g_triggeredTime > persist_conf::devConf.triggerDelay * 1000L) {
        startImmediately();
    }
}

int generateTrigger(Source source, bool checkImmediatelly) {
    bool seqTriggered = persist_conf::devConf.triggerSource == source && g_state == STATE_INITIATED;

#if OPTION_SD_CARD
    bool dlogTriggered = dlog_record::g_parameters.triggerSource == source && dlog_record::isInitiated();
#endif

    if (!seqTriggered) {
#if OPTION_SD_CARD
        if (!dlogTriggered) {
            return SCPI_ERROR_TRIGGER_IGNORED;
        }
#else
        return SCPI_ERROR_TRIGGER_IGNORED;
#endif
    }

#if OPTION_SD_CARD
    if (dlogTriggered) {
        dlog_record::triggerGenerated();
    }
#endif

    if (seqTriggered) {
        setState(STATE_TRIGGERED);

        g_triggeredTime = micros() / 1000;

        if (checkImmediatelly) {
            check(g_triggeredTime);
        }
    }

    return SCPI_RES_OK;
}

bool isTriggerFinished() {
    for (int i = 0; i < CH_NUM; ++i) {
        if (g_triggerInProgress[i]) {
            return false;
        }
    }
    return true;
}

void triggerFinished() {
    if (persist_conf::devConf.triggerContinuousInitializationEnabled) {
        setState(STATE_INITIATED);
    } else {
        setState(STATE_IDLE);
    }
}

void onTriggerFinished(Channel &channel) {
    if (channel.getVoltageTriggerMode() == TRIGGER_MODE_LIST) {
        int err;

        switch (channel.getTriggerOnListStop()) {
        case TRIGGER_ON_LIST_STOP_OUTPUT_OFF:
            channel_dispatcher::setVoltage(channel, 0);
            channel_dispatcher::setCurrent(channel, 0);
            channel_dispatcher::outputEnable(channel, false);
            break;
        case TRIGGER_ON_LIST_STOP_SET_TO_FIRST_STEP:
            if (!list::setListValue(channel, 0, &err)) {
                generateError(err);
            }
            break;
        case TRIGGER_ON_LIST_STOP_SET_TO_LAST_STEP:
            if (!list::setListValue(channel, list::maxListsSize(channel) - 1, &err)) {
                generateError(err);
            }
            break;
        case TRIGGER_ON_LIST_STOP_STANDBY:
            channel_dispatcher::setVoltage(channel, 0);
            channel_dispatcher::setCurrent(channel, 0);
            channel_dispatcher::outputEnable(channel, false);
            changePowerState(false);
            break;
        }
    }
}

void setTriggerFinished(Channel &channel) {
    if (channel.channelIndex < 2 && (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES || channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL)) {
        g_triggerInProgress[0] = false;
        g_triggerInProgress[1] = false;
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            if (Channel::get(i).flags.trackingEnabled) {
                g_triggerInProgress[i] = false;
            }
        }
    } else {
        g_triggerInProgress[channel.channelIndex] = false;
    }

    onTriggerFinished(channel);

    if (isTriggerFinished()) {
        triggerFinished();
    }
}

int checkTrigger() {
    bool onlyFixed = true;
    
    bool trackingChannelsChecked = false;

    for (int i = 0; i < CH_NUM; ++i) {
        Channel &channel = Channel::get(i);

        if (!channel.isOk()) {
            continue;
        }

        if (i == 1 && (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL || channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES)) {
            continue;
        }

        if (channel.flags.trackingEnabled) {
            if (trackingChannelsChecked) {
                continue;
            }
            trackingChannelsChecked = true;
        }

        if (channel.getVoltageTriggerMode() != channel.getCurrentTriggerMode()) {
            return SCPI_ERROR_INCOMPATIBLE_TRANSIENT_MODES;
        }

        if (channel.getVoltageTriggerMode() != TRIGGER_MODE_FIXED) {
            if (channel.isRemoteProgrammingEnabled()) {
                // TODO replace with more specific error
                return SCPI_ERROR_EXECUTION_ERROR;
            }

            if (channel.getVoltageTriggerMode() == TRIGGER_MODE_LIST) {
                if (list::isListEmpty(channel)) {
                    return SCPI_ERROR_LIST_IS_EMPTY;
                }

                if (!list::areListLengthsEquivalent(channel)) {
                    return SCPI_ERROR_LIST_LENGTHS_NOT_EQUIVALENT;
                }

                int err = list::checkLimits(i);
                if (err) {
                    return err;
                }
            } else {
                if (g_levels[i].u > channel_dispatcher::getULimit(channel)) {
                    return SCPI_ERROR_VOLTAGE_LIMIT_EXCEEDED;
                }

                if (g_levels[i].i > channel_dispatcher::getILimit(channel)) {
                    return SCPI_ERROR_CURRENT_LIMIT_EXCEEDED;
                }

                if (g_levels[i].u * g_levels[i].i > channel_dispatcher::getPowerLimit(channel)) {
                    return SCPI_ERROR_POWER_LIMIT_EXCEEDED;
                }
            }

            onlyFixed = false;
        }
    }

    if (onlyFixed) {
        return SCPI_ERROR_CANNOT_INITIATE_WHILE_IN_FIXED_MODE;
    }

    return 0;
}

int startImmediately() {
    int err = checkTrigger();
    if (err) {
        return err;
    }

    setState(STATE_EXECUTING);
    for (int i = 0; i < CH_NUM; ++i) {
        Channel &channel = Channel::get(i);
        if (channel.isOk()) {
            g_triggerInProgress[i] = true;
        }
    }

    io_pins::onTrigger();

    bool trackingChannelsStarted = false;

    channel_dispatcher::beginOutputEnableSequence();

    for (int i = 0; i < CH_NUM; ++i) {
        Channel &channel = Channel::get(i);

        if (!channel.isOk()) {
            continue;
        }

        if (i == 1 && (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL || channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES)) {
            continue;
        }

        if (channel.flags.trackingEnabled) {
            if (trackingChannelsStarted) {
                continue;
            }
            trackingChannelsStarted = true;
        }

        if (channel.getVoltageTriggerMode() == TRIGGER_MODE_LIST) {
            //channel_dispatcher::setVoltage(channel, 0);
            //channel_dispatcher::setCurrent(channel, 0);

            list::executionStart(channel);

            channel_dispatcher::outputEnable(
                channel, channel_dispatcher::getTriggerOutputState(channel));
        } else {
            if (channel.getVoltageTriggerMode() == TRIGGER_MODE_STEP) {
                channel_dispatcher::setVoltage(channel, g_levels[i].u);
                channel_dispatcher::setCurrent(channel, g_levels[i].i);

                channel_dispatcher::outputEnable(
                    channel, channel_dispatcher::getTriggerOutputState(channel));
            }

            setTriggerFinished(channel);
        }
    }

    channel_dispatcher::endOutputEnableSequence();

    return SCPI_RES_OK;
}

int initiate() {
    if (persist_conf::devConf.triggerSource == SOURCE_IMMEDIATE) {
        return startImmediately();
    } else {
        int err = checkTrigger();
        if (err) {
            return err;
        }
        setState(STATE_INITIATED);
    }
    return SCPI_RES_OK;
}

int enableInitiateContinuous(bool enable) {
    persist_conf::setTriggerContinuousInitializationEnabled(enable);
    if (enable) {
        return initiate();
    } else {
        return SCPI_RES_OK;
    }
}

bool isContinuousInitializationEnabled() {
    return persist_conf::devConf.triggerContinuousInitializationEnabled;
}

bool isIdle() {
    return g_state == STATE_IDLE;
}

bool isInitiated() {
    return g_state == STATE_INITIATED;
}

bool isTriggered() {
    return g_state == STATE_TRIGGERED;
}

void abort() {
    list::abort();
    setState(STATE_IDLE);
}

void tick(uint32_t tick_usec) {
    if (g_state == STATE_TRIGGERED) {
        check(tick_usec / 1000);
    }
}

} // namespace trigger
} // namespace psu
} // namespace eez
