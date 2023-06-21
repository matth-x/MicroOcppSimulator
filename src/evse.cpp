#include "evse.h"
#include <ArduinoOcpp.h>
#include <ArduinoOcpp/Context.h>
#include <ArduinoOcpp/Model/Model.h>
#include <ArduinoOcpp/Model/Transactions/Transaction.h>
#include <ArduinoOcpp/Operations/StatusNotification.h>
#include <ArduinoOcpp/Debug.h>
#include <cstring>
#include <cstdlib>

#define SIMULATOR_FN AO_FILENAME_PREFIX "simulator.jsn"

Evse::Evse(unsigned int connectorId) : connectorId{connectorId} {

}

ArduinoOcpp::Connector *getConnector(unsigned int connectorId) {
    if (!getOcppContext()) {
        AO_DBG_ERR("unitialized");
        return nullptr;
    }
    return getOcppContext()->getModel().getConnector(connectorId);
}

void Evse::setup() {
    auto connector = getConnector(connectorId);
    if (!connector) {
        AO_DBG_ERR("invalid state");
        return;
    }

    char key [30] = {'\0'};

    snprintf(key, 30, "evPlugged_cId_%u", connectorId);
    trackEvPlugged = ArduinoOcpp::declareConfiguration(key, false, SIMULATOR_FN, false, false);
    snprintf(key, 30, "evReady_cId_%u", connectorId);
    trackEvReady = ArduinoOcpp::declareConfiguration(key, false, SIMULATOR_FN, false, false);
    snprintf(key, 30, "evseReady_cId_%u", connectorId);
    trackEvseReady = ArduinoOcpp::declareConfiguration(key, false, SIMULATOR_FN, false, false);

    connector->setConnectorPluggedSampler([this] () -> bool {
        return *trackEvPlugged; //return if J1772 is in State B or C
    });

    connector->setEvRequestsEnergySampler([this] () -> bool {
        return *trackEvReady; //return if J1772 is in State C
    });

    connector->setConnectorEnergizedSampler([this] () -> bool {
        return *trackEvseReady;
    });

    connector->addConnectorErrorCodeSampler([this] () -> const char* {
        const char *errorCode = nullptr; //if error is present, point to error code; any number of error code samplers can be added in this project
        return errorCode;
    });

    setEnergyMeterInput([this] () -> float {
        return simulate_energy;
    }, connectorId);

    setPowerMeterInput([this] () -> float {
        return simulate_power;
    }, connectorId);

    addMeterValueInput([this] () {
            return (int32_t) getCurrent();
        }, 
        "Current.Import",
        "A",
        "Outlet",
        nullptr,
        connectorId);
    
    addMeterValueInput([this] () {
            return (int32_t) getVoltage();
        }, 
        "Voltage",
        "V",
        nullptr,
        nullptr,
        connectorId);
    
    addMeterValueInput([this] () {
            return (int32_t) (simulate_power > 1.f ? 44.f : 0.f);
        }, 
        "SoC",
        nullptr,
        nullptr,
        nullptr,
        connectorId);

    setOnResetExecute([] (bool isHard) {
        exit(0);
    });

    setSmartChargingPowerOutput([this] (float limit) {
        AO_DBG_DEBUG("set limit: %f", limit);
        this->limit_power = limit;
    }, connectorId);
}

void Evse::loop() {
    if (auto connector = getConnector(connectorId)) {
        auto curStatus = connector->inferenceStatus();

        if (status.compare(ArduinoOcpp::Ocpp16::cstrFromOcppEveState(curStatus))) {
            status = ArduinoOcpp::Ocpp16::cstrFromOcppEveState(curStatus);
        }
    }


    bool simulate_isCharging = ocppPermitsCharge(connectorId) && *trackEvPlugged && *trackEvReady && *trackEvseReady;

    if (simulate_isCharging) {
        if (simulate_power >= 1.f) {
            simulate_energy += (float) (ao_tick_ms() - simulate_energy_track_time) * simulate_power * (0.001f / 3600.f);
        }

        simulate_power = SIMULATE_POWER_CONST;
        simulate_power = std::min(simulate_power, limit_power);
        simulate_power += (((ao_tick_ms() / 5000) * 3483947) % 20000) * 0.001f - 10.f;
        simulate_energy_track_time = ao_tick_ms();
    } else {
        simulate_power = 0.f;
    }

}

void Evse::presentNfcTag(const char *uid_cstr) {
    if (!uid_cstr) {
        AO_DBG_ERR("invalid argument");
        return;
    }
    std::string uid = uid_cstr;
    auto connector = getConnector(connectorId);
    if (!connector) {
        AO_DBG_ERR("invalid state");
        return;
    }

    if (connector->getTransaction() && connector->getTransaction()->isActive()) {
        if (!uid.compare(connector->getTransaction()->getIdTag())) {
            connector->endTransaction("Local");
        } else {
            AO_DBG_INFO("RFID card denied");
        }
    } else {
        connector->beginTransaction(uid.c_str());
    }
}

void Evse::setEvPlugged(bool plugged) {
    if (!trackEvPlugged) return;
    *trackEvPlugged = plugged;
    ArduinoOcpp::configuration_save();
}

bool Evse::getEvPlugged() {
    if (!trackEvPlugged) return false;
    return *trackEvPlugged;
}

void Evse::setEvReady(bool ready) {
    if (!trackEvReady) return;
    *trackEvReady = ready;
    ArduinoOcpp::configuration_save();
}

bool Evse::getEvReady() {
    if (!trackEvReady) return false;
    return *trackEvReady;
}

void Evse::setEvseReady(bool ready) {
    if (!trackEvseReady) return;
    *trackEvseReady = ready;
    ArduinoOcpp::configuration_save();
}

bool Evse::getEvseReady() {
    if (!trackEvseReady) return false;
    return *trackEvseReady;
}

const char *Evse::getSessionIdTag() {
    auto connector = getConnector(connectorId);
    if (!connector) {
        AO_DBG_ERR("invalid state");
        return nullptr;
    }
    return connector->getSessionIdTag();
}

int Evse::getTransactionId() {
    auto connector = getConnector(connectorId);
    if (!connector) {
        AO_DBG_ERR("invalid state");
        return -1;
    }
    return connector->getTransactionId();
}

bool Evse::chargingPermitted() {
    auto connector = getConnector(connectorId);
    if (!connector) {
        AO_DBG_ERR("invalid state");
        return false;
    }
    return connector->ocppPermitsCharge();
}

int Evse::getPower() {
    return (int) simulate_power;
}

float Evse::getVoltage() {
    if (getPower() > 1.f) {
        return 228.f + (((ao_tick_ms() / 5000) * 7484311) % 4000) * 0.001f;
    } else {
        return 0.f;
    }
}
