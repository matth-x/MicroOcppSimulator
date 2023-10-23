#include "evse.h"
#include <MicroOcpp.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Operations/StatusNotification.h>
#include <MicroOcpp/Debug.h>
#include <cstring>
#include <cstdlib>

#define SIMULATOR_FN MO_FILENAME_PREFIX "simulator.jsn"

Evse::Evse(unsigned int connectorId) : connectorId{connectorId} {

}

MicroOcpp::Connector *getConnector(unsigned int connectorId) {
    if (!getOcppContext()) {
        MO_DBG_ERR("unitialized");
        return nullptr;
    }
    return getOcppContext()->getModel().getConnector(connectorId);
}

void Evse::setup() {
    auto connector = getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("invalid state");
        return;
    }

    char key [30] = {'\0'};

    snprintf(key, 30, "evPlugged_cId_%u", connectorId);
    trackEvPluggedKey = key;
    trackEvPluggedBool = MicroOcpp::declareConfiguration(trackEvPluggedKey.c_str(), false, SIMULATOR_FN, false, false, false);
    snprintf(key, 30, "evReady_cId_%u", connectorId);
    trackEvReadyKey = key;
    trackEvReadyBool = MicroOcpp::declareConfiguration(trackEvReadyKey.c_str(), false, SIMULATOR_FN, false, false, false);
    snprintf(key, 30, "evseReady_cId_%u", connectorId);
    trackEvseReadyKey = key;
    trackEvseReadyBool = MicroOcpp::declareConfiguration(trackEvseReadyKey.c_str(), false, SIMULATOR_FN, false, false, false);

    MicroOcpp::configuration_load(SIMULATOR_FN);

    connector->setConnectorPluggedInput([this] () -> bool {
        return trackEvPluggedBool->getBool(); //return if J1772 is in State B or C
    });

    connector->setEvReadyInput([this] () -> bool {
        return trackEvReadyBool->getBool(); //return if J1772 is in State C
    });

    connector->setEvseReadyInput([this] () -> bool {
        return trackEvseReadyBool->getBool();
    });

    connector->addErrorCodeInput([this] () -> const char* {
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
        MO_DBG_DEBUG("set limit: %f", limit);
        this->limit_power = limit;
    }, connectorId);
}

void Evse::loop() {
    if (auto connector = getConnector(connectorId)) {
        auto curStatus = connector->getStatus();

        if (status.compare(MicroOcpp::Ocpp16::cstrFromOcppEveState(curStatus))) {
            status = MicroOcpp::Ocpp16::cstrFromOcppEveState(curStatus);
        }
    }


    bool simulate_isCharging = ocppPermitsCharge(connectorId) && trackEvPluggedBool->getBool() && trackEvReadyBool->getBool() && trackEvseReadyBool->getBool();

    simulate_isCharging &= limit_power >= 720.f; //minimum charging current is 6A (720W for 120V grids) according to J1772

    if (simulate_isCharging) {
        if (simulate_power >= 1.f) {
            simulate_energy += (float) (mocpp_tick_ms() - simulate_energy_track_time) * simulate_power * (0.001f / 3600.f);
        }

        simulate_power = SIMULATE_POWER_CONST;
        simulate_power = std::min(simulate_power, limit_power);
        simulate_power += (((mocpp_tick_ms() / 5000) * 3483947) % 20000) * 0.001f - 10.f;
        simulate_energy_track_time = mocpp_tick_ms();
    } else {
        simulate_power = 0.f;
    }

}

void Evse::presentNfcTag(const char *uid_cstr) {
    if (!uid_cstr) {
        MO_DBG_ERR("invalid argument");
        return;
    }
    std::string uid = uid_cstr;
    auto connector = getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("invalid state");
        return;
    }

    if (connector->getTransaction() && connector->getTransaction()->isActive()) {
        if (!uid.compare(connector->getTransaction()->getIdTag())) {
            connector->endTransaction(uid.c_str());
        } else {
            MO_DBG_INFO("RFID card denied");
        }
    } else {
        connector->beginTransaction(uid.c_str());
    }
}

void Evse::setEvPlugged(bool plugged) {
    if (!trackEvPluggedBool) return;
    trackEvPluggedBool->setBool(plugged);
    MicroOcpp::configuration_save();
}

bool Evse::getEvPlugged() {
    if (!trackEvPluggedBool) return false;
    return trackEvPluggedBool->getBool();
}

void Evse::setEvReady(bool ready) {
    if (!trackEvReadyBool) return;
    trackEvReadyBool->setBool(ready);
    MicroOcpp::configuration_save();
}

bool Evse::getEvReady() {
    if (!trackEvReadyBool) return false;
    return trackEvReadyBool->getBool();
}

void Evse::setEvseReady(bool ready) {
    if (!trackEvseReadyBool) return;
    trackEvseReadyBool->setBool(ready);
    MicroOcpp::configuration_save();
}

bool Evse::getEvseReady() {
    if (!trackEvseReadyBool) return false;
    return trackEvseReadyBool->getBool();
}

const char *Evse::getSessionIdTag() {
    auto connector = getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("invalid state");
        return nullptr;
    }
    return connector->getTransaction() ? connector->getTransaction()->getIdTag() : nullptr;
}

int Evse::getTransactionId() {
    auto connector = getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("invalid state");
        return -1;
    }
    return connector->getTransaction() ? connector->getTransaction()->getTransactionId() : -1;
}

bool Evse::chargingPermitted() {
    auto connector = getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("invalid state");
        return false;
    }
    return connector->ocppPermitsCharge();
}

int Evse::getPower() {
    return (int) simulate_power;
}

float Evse::getVoltage() {
    if (getPower() > 1.f) {
        return 228.f + (((mocpp_tick_ms() / 5000) * 7484311) % 4000) * 0.001f;
    } else {
        return 0.f;
    }
}
