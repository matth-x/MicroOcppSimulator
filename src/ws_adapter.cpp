#include "ws_adapter.h"
#include "mongoose/mongoose.h"
#include "base64.hpp"
#include <ArduinoOcpp/Platform.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Debug.h>

#define OCPP_CREDENTIALS_FN (AO_FILENAME_PREFIX "/ocpp-creds.jsn")

#define DEBUG_MSG_INTERVAL 5000UL
#define MG_WEBSOCKET_PING_INTERVAL_MS 5000UL
#define WS_UNRESPONSIVE_THRESHOLD_MS 5000UL
#define RECONNECT_AFTER 5000UL

void ws_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data);

AoMongooseAdapter::AoMongooseAdapter(struct mg_mgr *mgr) : mgr(mgr) {
    maintainWsConn();
}

AoMongooseAdapter::~AoMongooseAdapter() {
    if (websocket)
        AO_DBG_ERR("must close mg connection before destructing ocpp socket");
}

void AoMongooseAdapter::loop() {
    maintainWsConn();
}

bool AoMongooseAdapter::sendTXT(std::string &out) {
    if (!connection_established || !websocket) {
        return false;
    }
    size_t sent = mg_ws_send(websocket, out.c_str(), out.length(), WEBSOCKET_OP_TEXT);
    if (sent < out.length()) {
        AO_DBG_WARN("mg_ws_send did only accept %zu out of %zu bytes", sent, out.length());
        return false;
    } else {
        return true;
    }
}

void AoMongooseAdapter::maintainWsConn() {
    if (ao_tick_ms() - last_status_dbg_msg >= DEBUG_MSG_INTERVAL) {
        last_status_dbg_msg = ao_tick_ms();

        //WS successfully connected?
        if (!connection_established) {
            AO_DBG_DEBUG("WS unconnected");
        } else if (ao_tick_ms() - last_recv >= MG_WEBSOCKET_PING_INTERVAL_MS + WS_UNRESPONSIVE_THRESHOLD_MS) {
            //WS connected but unresponsive
            AO_DBG_DEBUG("WS unresponsive");
        }
    }

    if (url.empty()) {
        //OCPP connection should be closed
        if (websocket) {
            AO_DBG_INFO("Closing websocket");
            websocket->is_closing = 1; //mg will close WebSocket, callback will set websocket = nullptr
        }
        return;
    }

    if (hb_interval > 0 && websocket && ao_tick_ms() - last_hb >= hb_interval) {
        last_hb = ao_tick_ms();
        mg_ws_send(websocket, "", 0, WEBSOCKET_OP_PING);
    }

    if (websocket != nullptr) { //connection pointer != nullptr means that the socket is still open
        return;
    }

    if (ao_tick_ms() - last_reconnection_attempt < RECONNECT_AFTER) {
        return;
    }

    AO_DBG_DEBUG("(re-)connect to %s", url.c_str());

    last_reconnection_attempt = ao_tick_ms();

    websocket = mg_ws_connect(
        mgr, 
        url.c_str(), 
        ws_cb, 
        this, 
        "%s%s%s\r\n", "Sec-WebSocket-Protocol: ocpp1.6",
                      basic_auth64.empty() ? "" : "\r\nAuthorization: Basic ", 
                      basic_auth64.empty() ? "" : basic_auth64.c_str());     // Create client
}

void AoMongooseAdapter::setUrl(const char *backend_url_cstr, const char *cbId_cstr) {
    url.clear();

    if (!backend_url_cstr && !cbId_cstr) {
        AO_DBG_ERR("invalid arguments");
        return;
    }
    if (backend_url_cstr) {
        backend_url = backend_url_cstr;
        std::shared_ptr<ArduinoOcpp::Configuration<const char *>> ocpp_backend = ArduinoOcpp::declareConfiguration<const char *>(
            "AO_BackendUrl", "", OCPP_CREDENTIALS_FN, true, true, true, true);
        *ocpp_backend = backend_url_cstr;
    }
    if (cbId_cstr) {
        cbId = cbId_cstr;
        std::shared_ptr<ArduinoOcpp::Configuration<const char *>> ocpp_cbId = ArduinoOcpp::declareConfiguration<const char *>(
            "AO_ChargeBoxId", "", OCPP_CREDENTIALS_FN, true, true, true, true);
        *ocpp_cbId = cbId_cstr;
    }

    ArduinoOcpp::configuration_save();

    std::string basic_auth_cpy = basic_auth;
    setAuthToken(basic_auth_cpy.c_str()); //update basic auth token

    if (backend_url.empty()) {
        AO_DBG_DEBUG("empty URL closes connection");
    } else {
        if (cbId.empty()) {
            url = backend_url;
        } else {
            if (backend_url.back() != '/') {
                backend_url.append("/");
            }

            url = backend_url + cbId;
        }
    }

    if (websocket) {
        AO_DBG_INFO("Resetting WS");
        websocket->is_closing = 1; //mg will close WebSocket, callback will set websocket = nullptr
    }
}

void AoMongooseAdapter::setCa(const char *ca) {
    this->ca = ca;
    std::shared_ptr<ArduinoOcpp::Configuration<const char *>> ocpp_ca = ArduinoOcpp::declareConfiguration<const char *>(
        "AO_CaCert", "", OCPP_CREDENTIALS_FN, true, true, true, true);
    *ocpp_ca = ca;
    ArduinoOcpp::configuration_save();
    if (websocket) {
        AO_DBG_INFO("Resetting WS");
        websocket->is_closing = 1; //mg will close WebSocket, callback will set websocket = nullptr
    }
}

void AoMongooseAdapter::setAuthToken(const char *basic_auth_cstr) {

    basic_auth64.clear();

    if (!basic_auth_cstr) {
        AO_DBG_ERR("invalid argument");
        return;
    }
    basic_auth = basic_auth_cstr;
    std::shared_ptr<ArduinoOcpp::Configuration<const char *>> ocpp_auth = ArduinoOcpp::declareConfiguration<const char *>(
        "AO_BasicAuthKey", "", OCPP_CREDENTIALS_FN, true, true, true, true);
    *ocpp_auth = basic_auth_cstr;
    ArduinoOcpp::configuration_save();

    if (!basic_auth.empty()) {
        std::string token = cbId + ":" + basic_auth;

        AO_DBG_DEBUG("auth Token=%s", token.c_str());

        unsigned int base64_length = encode_base64_length(token.length());
        std::vector<unsigned char> base64 (base64_length + 1);

        // encode_base64() places a null terminator automatically, because the output is a string
        base64_length = encode_base64((const unsigned char*) token.c_str(), token.length(), &base64[0]);

        AO_DBG_DEBUG("auth64 len=%u, auth64 Token=%s", base64_length, &base64[0]);

        basic_auth64 = (const char*) &base64[0];
    } else {
        AO_DBG_DEBUG("reset Basic Auth token");
        (void) 0;
    }

    if (websocket) {
        AO_DBG_INFO("Resetting WS");
        websocket->is_closing = 1; //mg will close WebSocket, callback will set websocket = nullptr
    }
}

void AoMongooseAdapter::setConnectionEstablished(bool established) {
    connection_established = established;
    if (connection_established == false) {
        websocket = nullptr;
    }
}

void AoMongooseAdapter::updateRcvTimer() {
    last_recv = ao_tick_ms();
}

void ws_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    if (ev != 2) printf("[MG] Cb fn with event: %d\n", ev);
    AoMongooseAdapter *osock = reinterpret_cast<AoMongooseAdapter*>(fn_data);
    if (ev == MG_EV_ERROR) {
        // On error, log error message
        MG_ERROR(("%p %s", c->fd, (char *) ev_data));
    } else if (ev == MG_EV_CONNECT) {
        // If target URL is SSL/TLS, command client connection to use TLS
        if (mg_url_is_ssl(osock->getUrl())) {
            //struct mg_tls_opts opts = {.ca = "ca.pem"};
            struct mg_tls_opts opts = {.cert = osock->getCa()};
            mg_tls_init(c, &opts);
        } else {
            AO_DBG_WARN("Insecure connection (WS)");
        }
    } else if (ev == MG_EV_WS_OPEN) {
        // WS connection established. Perform MQTT login
        MG_INFO(("Connected to WS"));
        osock->setConnectionEstablished(true);
    } else if (ev == MG_EV_WS_MSG) {
        struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
        MG_INFO(("GOT %d bytes WS msg", (int) wm->data.len));
        if (!osock->getReceiveTXTcallback()((const char*) wm->data.ptr, wm->data.len)) {
            AO_DBG_WARN("processing input message failed");
        }
        osock->updateRcvTimer();
    } else if (ev == MG_EV_WS_CTL) {
        osock->updateRcvTimer();
    }

    if (ev == MG_EV_ERROR || ev == MG_EV_CLOSE) {
        MG_INFO(("Connected ended"));
        osock->setConnectionEstablished(false);
    }
}
