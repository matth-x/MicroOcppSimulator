// matth-x/MicroOcppSimulator
// Copyright Matthias Akstaller 2022 - 2024
// GPL-3.0 License

#include "evse.h"
#include <MicroOcpp.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Model/Transactions/TransactionService.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Model/Authorization/IdToken.h>
#include <MicroOcpp/Operations/StatusNotification.h>
#include <MicroOcpp/Version.h>
#include <MicroOcpp/Debug.h>
#include <cstring>
#include <cstdlib>

Evse::Evse(unsigned int connectorId) : connectorId{connectorId} {

}

void Evse::setup() {

#if MO_ENABLE_V201
    if (auto context = getOcppContext()) {
        if (context->getVersion().major == 2) {
            //load some example variables for testing

            if (auto varService = context->getModel().getVariableService()) {
                varService->declareVariable<bool>("AuthCtrlr", "LocalAuthorizeOffline", false, MicroOcpp::Variable::Mutability::ReadOnly, false);
            }
        }
    }
#endif

    char key [30] = {'\0'};

    snprintf(key, 30, "evPlugged_cId_%u", connectorId);
    trackEvPluggedKey = key;
    trackEvPluggedBool = MicroOcpp::declareConfiguration(trackEvPluggedKey.c_str(), false, SIMULATOR_FN, false, false, false);
    snprintf(key, 30, "evsePlugged_cId_%u", connectorId);
    trackEvsePluggedKey = key;
    trackEvsePluggedBool = MicroOcpp::declareConfiguration(trackEvsePluggedKey.c_str(), false, SIMULATOR_FN, false, false, false);
    snprintf(key, 30, "evReady_cId_%u", connectorId);
    trackEvReadyKey = key;
    trackEvReadyBool = MicroOcpp::declareConfiguration(trackEvReadyKey.c_str(), false, SIMULATOR_FN, false, false, false);
    snprintf(key, 30, "evseReady_cId_%u", connectorId);
    trackEvseReadyKey = key;
    trackEvseReadyBool = MicroOcpp::declareConfiguration(trackEvseReadyKey.c_str(), false, SIMULATOR_FN, false, false, false);

    MicroOcpp::configuration_load(SIMULATOR_FN);

    setConnectorPluggedInput([this] () -> bool {
        return trackEvPluggedBool->getBool(); //return if J1772 is in State B or C
    }, connectorId);

    setEvReadyInput([this] () -> bool {
        return trackEvReadyBool->getBool(); //return if J1772 is in State C
    }, connectorId);

    setEvseReadyInput([this] () -> bool {
        return trackEvseReadyBool->getBool();
    }, connectorId);

    addErrorCodeInput([this] () -> const char* {
        const char *errorCode = nullptr; //if error is present, point to error code; any number of error code samplers can be added in this project
        return errorCode;
    }, connectorId);

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
        if (limit >= 0.f) {
            MO_DBG_DEBUG("set limit: %f", limit);
            this->limit_power = limit;
        } else {
            // negative value means no limit defined
            this->limit_power = SIMULATE_POWER_CONST;
        }
    }, connectorId);
}

void Evse::loop() {

    auto curStatus = getChargePointStatus(connectorId);

    if (status.compare(MicroOcpp::cstrFromOcppEveState(curStatus))) {
        status = MicroOcpp::cstrFromOcppEveState(curStatus);
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

void Evse::presentNfcTag(const char *uid) {
    if (!uid) {
        MO_DBG_ERR("invalid argument");
        return;
    }

#if MO_ENABLE_V201
    if (auto context = getOcppContext()) {
        if (context->getVersion().major == 2) {
            presentNfcTag(uid, "ISO14443");
            return;
        }
    }
#endif

    if (isTransactionActive(connectorId)) {
        if (!strcmp(uid, getTransactionIdTag(connectorId))) {
            endTransaction(uid, "Local", connectorId);
        } else {
            MO_DBG_INFO("RFID card denied");
        }
    } else {
        beginTransaction(uid, connectorId);
    }
}

#if MO_ENABLE_V201
bool Evse::presentNfcTag(const char *uid, const char *type) {

    MicroOcpp::IdToken idToken {nullptr, MicroOcpp::IdToken::Type::UNDEFINED, "Simulator"};
    if (!idToken.parseCstr(uid, type)) {
        return false;
    }

    if (auto txService = getOcppContext()->getModel().getTransactionService()) {
        if (auto evse = txService->getEvse(connectorId)) {
            if (evse->getTransaction() && evse->getTransaction()->isAuthorizationActive) {
                evse->endAuthorization(idToken);
            } else {
                evse->beginAuthorization(idToken);
            }
            return true;
        }
    }
    return false;
}
#endif

void Evse::setEvPlugged(bool plugged) {
    if (!trackEvPluggedBool) return;
    trackEvPluggedBool->setBool(plugged);
    MicroOcpp::configuration_save();
}

bool Evse::getEvPlugged() {
    if (!trackEvPluggedBool) return false;
    return trackEvPluggedBool->getBool();
}

void Evse::setEvsePlugged(bool plugged) {
    if (!trackEvsePluggedBool) return;
    trackEvsePluggedBool->setBool(plugged);
    MicroOcpp::configuration_save();
}

bool Evse::getEvsePlugged() {
    if (!trackEvsePluggedBool) return false;
    return trackEvsePluggedBool->getBool();
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
    return getTransactionIdTag(connectorId) ? getTransactionIdTag(connectorId) : "";
}

int Evse::getTransactionId() {
    return getTransaction(connectorId) ? getTransaction(connectorId)->getTransactionId() : -1;
}

bool Evse::chargingPermitted() {
    return ocppPermitsCharge(connectorId);
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
