#include "evse.h"
#include <ArduinoOcpp.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/Authorization/AuthorizationService.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Debug.h>
#include <ArduinoOcpp/MessagesV16/StatusNotification.h>
#include <cstring>

Evse::Evse(unsigned int connectorId) : connectorId{connectorId} {

}

ArduinoOcpp::ConnectorStatus *getConnector(unsigned int connectorId) {
    if (!getOcppEngine()) {
        AO_DBG_ERR("unitialized");
        return nullptr;
    }
    return getOcppEngine()->getOcppModel().getConnectorStatus(connectorId);
}

void Evse::setup() {
    auto connector = getConnector(connectorId);
    if (!connector) {
        AO_DBG_ERR("invalid state");
        return;
    }

    connector->setConnectorPluggedSampler([this] () -> bool {
        return trackEvPlugged; //return if J1772 is in State B or C
    });

    connector->setEvRequestsEnergySampler([this] () -> bool {
        return trackEvReady; //return if J1772 is in State C
    });

    connector->addConnectorErrorCodeSampler([this] () -> const char* {
        const char *errorCode = nullptr; //if error is present, point to error code; any number of error code samplers can be added in this project
        return errorCode;
    });
}

void Evse::loop() {
    if (auto connector = getConnector(connectorId)) {
        auto curStatus = connector->inferenceStatus();

        if (status.compare(ArduinoOcpp::Ocpp16::cstrFromOcppEveState(curStatus))) {
            status = ArduinoOcpp::Ocpp16::cstrFromOcppEveState(curStatus);
        }
    }
}

void Evse::presentNfcTag(const char *uid_cstr) {
    if (!uid_cstr) {
        AO_DBG_ERR("invalid argument");
        return;
    }
    std::string uid = uid_cstr;
    auto connector = getOcppEngine()->getOcppModel().getConnectorStatus(connectorId);
    if (!connector) {
        AO_DBG_ERR("invalid state");
        return;
    }

    if (connector->getSessionIdTag()) {
        if (!uid.compare(connector->getSessionIdTag())) {
            connector->endSession("Local");
        } else {
            AO_DBG_INFO("RFID card denied");
        }
    } else if (getOcppEngine()->getOcppModel().getAuthorizationService() &&
                getOcppEngine()->getOcppModel().getAuthorizationService()->getLocalAuthorization(uid.c_str())) {
            
            AO_DBG_INFO("RFID tag locally authorized: %s", uid.c_str());
            connector->beginSession(uid.c_str());
    } else {

        authorize(uid.c_str(), [uid, connector] (JsonObject response) {
            if (!strcmp(response["idTagInfo"]["status"] | "", "Accepted")) {
                AO_DBG_INFO("RFID tag authorized: %s", uid.c_str());
                connector->beginSession(uid.c_str());
            } else {
                AO_DBG_INFO("RFID card denied for reason %s", response["idTagInfo"]["status"] | "undefined");
            }
        });
    }
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
