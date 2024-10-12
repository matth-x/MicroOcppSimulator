// matth-x/MicroOcppSimulator
// Copyright Matthias Akstaller 2022 - 2024
// GPL-3.0 License

#include "api.h"
#include "mongoose.h"

#include <MicroOcpp/Debug.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Model/ConnectorBase/EvseId.h>
#include <MicroOcpp/Model/Authorization/IdToken.h>

#include "evse.h"

//simple matching function; takes * as a wildcard
bool str_match(const char *query, const char *pattern) {
    size_t qi = 0, pi = 0;
    
    while (pattern[pi]) {
        if (query[qi] && query[qi] == pattern[pi]) {
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

int mocpp_api2_call(const char *uri_raw, size_t uri_raw_len, MicroOcpp::Method method, const char *query_raw, size_t query_raw_len, char *resp_body, size_t resp_body_size) {

    snprintf(resp_body, resp_body_size, "%s", "");
    
    struct mg_str uri = mg_str_n(uri_raw, uri_raw_len);
    struct mg_str query = mg_str_n(query_raw, query_raw_len);

    int evse_id = -1;
    int connector_id = -1;

    unsigned int num;
    struct mg_str evse_id_str = mg_http_var(query, mg_str("evse_id"));
    if (evse_id_str.buf) {
        if (!mg_str_to_num(evse_id_str, 10, &num, sizeof(num)) || num < 1 || num >= MO_NUM_EVSEID) {
            snprintf(resp_body, resp_body_size, "invalid connector_id");
            return 400;
        }
        evse_id = (int)num;
    }

    struct mg_str connector_id_str = mg_http_var(query, mg_str("connector_id"));
    if (connector_id_str.buf) {
        if (!mg_str_to_num(connector_id_str, 10, &num, sizeof(num)) || num != 1) {
            snprintf(resp_body, resp_body_size, "invalid connector_id");
            return 400;
        }
        connector_id = (int)num;
    }

    if (mg_match(uri, mg_str("/plugin"), NULL)) {
        if (method != MicroOcpp::Method::POST) {
            return 405;
        }
        if (evse_id < 0) {
            snprintf(resp_body, resp_body_size, "no action taken");
            return 200;
        } else {
            snprintf(resp_body, resp_body_size, "%s", connectors[evse_id-1].getEvPlugged() ? "EV already plugged" : "plugged in EV");
            connectors[evse_id-1].setEvPlugged(true);
            connectors[evse_id-1].setEvReady(true);
            connectors[evse_id-1].setEvseReady(true);
            return 200;
        }
    } else if (mg_match(uri, mg_str("/plugout"), NULL)) {
        if (method != MicroOcpp::Method::POST) {
            return 405;
        }
        if (evse_id < 0) {
            snprintf(resp_body, resp_body_size, "no action taken");
            return 200;
        } else {
            snprintf(resp_body, resp_body_size, "%s", connectors[evse_id-1].getEvPlugged() ? "EV already unplugged" : "unplug EV");
            connectors[evse_id-1].setEvPlugged(false);
            connectors[evse_id-1].setEvReady(false);
            connectors[evse_id-1].setEvseReady(false);
            return 200;
        }
    } else if (mg_match(uri, mg_str("/end"), NULL)) {
        if (method != MicroOcpp::Method::POST) {
            return 405;
        }
        bool trackEvReady = false;
        for (size_t i = 0; i < connectors.size(); i++) {
            trackEvReady |= connectors[i].getEvReady();
            connectors[i].setEvReady(false);
        }
        snprintf(resp_body, resp_body_size, "%s", trackEvReady ? "suspended EV" : "EV already suspended");
        return 200;
    } else if (mg_match(uri, mg_str("/state"), NULL)) {
        if (method != MicroOcpp::Method::POST) {
            return 405;
        }
        struct mg_str ready_str = mg_http_var(query, mg_str("ready"));
        bool ready = true;
        if (ready_str.buf) {
            if (mg_match(ready_str, mg_str("true"), NULL)) {
                ready = true;
            } else if (mg_match(ready_str, mg_str("false"), NULL)) {
                ready = false;
            } else {
                snprintf(resp_body, resp_body_size, "invalid ready");
                return 400;
            }
        }
        bool trackEvReady = false;
        for (size_t i = 0; i < connectors.size(); i++) {
            if (connectors[i].getEvPlugged()) {
                bool trackEvReady = connectors[i].getEvReady();
                connectors[i].setEvReady(ready);
                snprintf(resp_body, resp_body_size, "%s, %s", ready ? "EV suspended" : "EV not suspended", trackEvReady ? "suspended before" : "not suspended before");
                return 200;
            }
        }
        snprintf(resp_body, resp_body_size, "no action taken - EV not plugged");
        return 200;
    } else if (mg_match(uri, mg_str("/authorize"), NULL)) {
        if (method != MicroOcpp::Method::POST) {
            return 405;
        }
        struct mg_str id = mg_http_var(query, mg_str("id"));
        if (!id.buf) {
            snprintf(resp_body, resp_body_size, "missing id");
            return 400;
        }
        struct mg_str type = mg_http_var(query, mg_str("type"));
        if (!id.buf) {
            snprintf(resp_body, resp_body_size, "missing type");
            return 400;
        }

        int ret;
        char id_buf [MO_IDTOKEN_LEN_MAX + 1];
        ret = snprintf(id_buf, sizeof(id_buf), "%.*s", (int)id.len, id.buf);
        if (ret < 0 || ret >= sizeof(id_buf)) {
            snprintf(resp_body, resp_body_size, "invalid id");
            return 400;
        }
        char type_buf [128];
        ret = snprintf(type_buf, sizeof(type_buf), "%.*s", (int)type.len, type.buf);
        if (ret < 0 || ret >= sizeof(type_buf)) {
            snprintf(resp_body, resp_body_size, "invalid type");
            return 400;
        }

        if (evse_id <= 0) {
            snprintf(resp_body, resp_body_size, "invalid evse_id");
            return 400;
        }

        bool trackAuthActive = connectors[evse_id-1].getSessionIdTag();

        if (!connectors[evse_id-1].presentNfcTag(id_buf, type_buf)) {
            snprintf(resp_body, resp_body_size, "invalid id and / or type");
            return 400;
        }

        bool authActive = connectors[evse_id-1].getSessionIdTag();

        snprintf(resp_body, resp_body_size, "%s",
                !trackAuthActive && authActive ? "authorize in progress" : 
                trackAuthActive && !authActive ? "unauthorize in progress" : 
                trackAuthActive && authActive ?  "no action taken (EVSE still authorized)" : 
                                                 "no action taken (EVSE not authorized)");

        return 200;
    } else if (mg_match(uri, mg_str("/memory/info"), NULL)) {
        #if MO_OVERRIDE_ALLOCATION && MO_ENABLE_HEAP_PROFILER
        {
            if (method != MicroOcpp::Method::GET) {
                return 405;
            }

            int ret = mo_mem_write_stats_json(resp_body, resp_body_size);
            if (ret < 0 || ret >= resp_body_size) {
                snprintf(resp_body, resp_body_size, "internal error");
                return 500;
            }

            return 200;
        }
        #else
        {
            snprintf(resp_body, resp_body_size, "memory profiler disabled");
            return 404;
        }
        #endif
    } else if (mg_match(uri, mg_str("/memory/reset"), NULL)) {
        #if MO_OVERRIDE_ALLOCATION && MO_ENABLE_HEAP_PROFILER
        {
            if (method != MicroOcpp::Method::POST) {
                return 405;
            }

            MO_MEM_RESET();
            return 200;
        }
        #else
        {
            snprintf(resp_body, resp_body_size, "memory profiler disabled");
            return 404;
        }
        #endif

    }

    return 404;
}
