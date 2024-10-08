// matth-x/MicroOcppSimulator
// Copyright Matthias Akstaller 2022 - 2024
// GPL-3.0 License

#include "api.h"

#include <MicroOcpp/Debug.h>

#include "evse.h"

//simple matching function; takes * as a wildcard
bool str_match(const char *query, const char *pattern) {
    size_t qi = 0, pi = 0;
    
    while (query[qi] && pattern[pi]) {
        if (query[qi] == pattern[pi]) {
            qi++;
            pi++;
        } else if (pattern[pi] == '*') {
            while (pattern[pi] == '*') pi++;
            while (query[qi] != pattern[pi]) qi++;
        } else {
            break;
        }
    }

    return !query[qi] && !pattern[pi];
}

int mocpp_api_call(const char *endpoint, MicroOcpp::Method method, const char *body, char *resp_body, size_t resp_body_size) {
    
    MO_DBG_VERBOSE("process %s, %s: %s",
            endpoint,
            method == MicroOcpp::Method::GET ? "GET" :
            method == MicroOcpp::Method::POST ? "POST" : "error",
            body);
    
    int status = 500;
    StaticJsonDocument<512> response;
    if (resp_body_size >= sizeof("{}")) {
        sprintf(resp_body, "%s", "{}");
    }

    StaticJsonDocument<512> request;
    if (*body) {
        auto err = deserializeJson(request, body);
        if (err) {
            MO_DBG_WARN("malformatted body: %s", err.c_str());
            return 400;
        }
    }
    
    unsigned int connectorId = 0;

    if (strlen(endpoint) >= 11) {
        if (endpoint[11] == '1') {
            connectorId = 1;
        } else if (endpoint[11] == '2') {
            connectorId = 2;
        }
    }

    MO_DBG_VERBOSE("connectorId = %u", connectorId);

    Evse *evse = nullptr;
    if (connectorId >= 1 && connectorId < MO_NUMCONNECTORS) {
        evse = &connectors[connectorId-1];
    }

    //start different api endpoints
    if(str_match(endpoint, "/connectors")) {
        MO_DBG_VERBOSE("query connectors");
        response.add("1");
        response.add("2");
        status = 200;
    } else if(str_match(endpoint, "/connector/*/evse")){
        MO_DBG_VERBOSE("query evse");
        if (!evse) {
            return 404;
        }

        if (method == MicroOcpp::Method::POST) {
            if (request.containsKey("evPlugged")) {
                evse->setEvPlugged(request["evPlugged"]);
            }
            if (request.containsKey("evsePlugged")) {
            evse->setEvsePlugged(request["evsePlugged"]);
            }
            if (request.containsKey("evReady")) {
                evse->setEvReady(request["evReady"]);
            }
            if (request.containsKey("evseReady")) {
                evse->setEvseReady(request["evseReady"]);
            }
        }

        response["evPlugged"] = evse->getEvPlugged();
        response["evsePlugged"] = evse->getEvsePlugged();
        response["evReady"] = evse->getEvReady();
        response["evseReady"] = evse->getEvseReady();
        response["chargePointStatus"] = evse->getOcppStatus();
        status = 200;
    } else if(str_match(endpoint, "/connector/*/meter")){
        MO_DBG_VERBOSE("query meter");
        if (!evse) {
            return 404;
        }

        response["energy"] = evse->getEnergy();
        response["power"] = evse->getPower();
        response["current"] = evse->getCurrent();
        response["voltage"] = evse->getVoltage();
        status = 200;
    } else if(str_match(endpoint, "/connector/*/transaction")){
        MO_DBG_VERBOSE("query transaction");
        if (!evse) {
            return 404;
        }

        if (method == MicroOcpp::Method::POST) {
            if (request.containsKey("idTag")) {
                evse->presentNfcTag(request["idTag"] | "");
            }
        }
        response["idTag"] = evse->getSessionIdTag();
        response["transactionId"] = evse->getTransactionId();
        response["authorizationStatus"] = "";
        status = 200;
    } else if(str_match(endpoint, "/connector/*/smartcharging")){
        MO_DBG_VERBOSE("query smartcharging");
        if (!evse) {
            return 404;
        }

        response["maxPower"] = evse->getSmartChargingMaxPower();
        response["maxCurrent"] = evse->getSmartChargingMaxCurrent();
        status = 200;
    } else {
        return 404;
    }

    if (response.overflowed()) {
        return 500;
    }

    std::string out;
    serializeJson(response, out);
    if (out.length() >= resp_body_size) {
        return 500;
    }

    if (!out.empty()) {
        sprintf(resp_body, "%s", out.c_str());
    }

    return status;
}
