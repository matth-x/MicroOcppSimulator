#include "ws_adapter.h"
#include "base64.hpp"
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Debug.h>

#define OCPP_CREDENTIALS_FN AO_FILENAME_PREFIX "/ocpp-creds.jsn"

#define DEBUG_MSG_INTERVAL 5000UL
#define WS_UNRESPONSIVE_THRESHOLD_MS 15000UL
#define RECONNECT_AFTER 60000UL

void ws_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data);

AoMongooseAdapter::AoMongooseAdapter(struct mg_mgr *mgr,
            const char *backend_url_default, 
            const char *charge_box_id_default,
            const char *auth_key_default,
            const char *CA_cert_default,
            std::shared_ptr<ArduinoOcpp::FilesystemAdapter> filesystem) : mgr(mgr) {
    
    const char *fn;
    bool write_permission;
    
    if (filesystem) {
        ArduinoOcpp::configuration_init(filesystem);

        //all credentials are persistent over reboots

        fn = OCPP_CREDENTIALS_FN;
        write_permission = true;
    } else {
        //make the credentials non-persistent
        AO_DBG_WARN("Credentials non-persistent. Use ArduinoOcpp::makeDefaultFilesystemAdapter(...) for persistency");

        fn = CONFIGURATION_VOLATILE OCPP_CREDENTIALS_FN;
        write_permission = false;
    }

    setting_backend_url = ArduinoOcpp::declareConfiguration<const char*>(
        "AO_BackendUrl", backend_url_default ? backend_url_default : "",
        fn, write_permission, true, true, true);
    setting_cb_id = ArduinoOcpp::declareConfiguration<const char*>(
        "AO_ChargeBoxId", charge_box_id_default ? charge_box_id_default : "",
        fn, write_permission, true, true, true);
    setting_auth_key = ArduinoOcpp::declareConfiguration<const char*>(
        "AuthorizationKey", auth_key_default ? auth_key_default : "",
        fn, write_permission, true, true, true);
#if !AO_CA_CERT_USE_FILE
    setting_ca_cert = ArduinoOcpp::declareConfiguration<const char*>(
        "AO_CaCert", CA_cert_default ? CA_cert_default : "",
        fn, write_permission, true, true, true);
#endif

    ws_ping_interval = ArduinoOcpp::declareConfiguration<int>(
        "WebSocketPingInterval", 5, fn, true, true, true, true);

    ArduinoOcpp::configuration_save();

    backend_url = setting_backend_url && *setting_backend_url ? *setting_backend_url : 
        (backend_url_default ? backend_url_default : "");
    cb_id = setting_cb_id && *setting_cb_id ? *setting_cb_id : 
        (charge_box_id_default ? charge_box_id_default : "");
    auth_key = setting_auth_key && *setting_auth_key ? *setting_auth_key : 
        (auth_key_default ? auth_key_default : "");
    
#if !AO_CA_CERT_USE_FILE
    ca_cert = setting_ca_cert && *setting_ca_cert ? *setting_ca_cert : 
        (CA_cert_default ? CA_cert_default : "");
#else
    ca_cert = CA_cert_default ? CA_cert_default : "";
#endif

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
        //flush broken package and wait for next retry
        (void)0;
    }

    return true;
}

void AoMongooseAdapter::maintainWsConn() {
    if (ao_tick_ms() - last_status_dbg_msg >= DEBUG_MSG_INTERVAL) {
        last_status_dbg_msg = ao_tick_ms();

        //WS successfully connected?
        if (!connection_established) {
            AO_DBG_DEBUG("WS unconnected");
        } else if (ao_tick_ms() - last_recv >= (ws_ping_interval && *ws_ping_interval > 0 ? (*ws_ping_interval * 1000UL) : 0UL) + WS_UNRESPONSIVE_THRESHOLD_MS) {
            //WS connected but unresponsive
            AO_DBG_DEBUG("WS unresponsive");
        }
    }

    if (backend_url.empty()) {
        //OCPP connection should be closed
        if (websocket) {
            AO_DBG_INFO("Closing websocket");
            websocket->is_closing = 1; //mg will close WebSocket, callback will set websocket = nullptr
        }
        return;
    }

    if (ws_ping_interval && *ws_ping_interval > 0 && websocket && ao_tick_ms() - last_hb >= (*ws_ping_interval * 1000UL)) {
        last_hb = ao_tick_ms();
        mg_ws_send(websocket, "", 0, WEBSOCKET_OP_PING);
    }

    if (websocket != nullptr) { //connection pointer != nullptr means that the socket is still open
        return;
    }

    if (ao_tick_ms() - last_reconnection_attempt < RECONNECT_AFTER) {
        return;
    }

    if (credentials_changed) {
        reload_credentials();
        credentials_changed = false;
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

void AoMongooseAdapter::reload_credentials() {
    url.clear();
    basic_auth64.clear();

    if (backend_url.empty()) {
        AO_DBG_DEBUG("empty URL closes connection");
        return;
    } else {
        if (cb_id.empty()) {
            url = backend_url;
        } else {
            if (backend_url.back() != '/') {
                backend_url.append("/");
            }

            url = backend_url + cb_id;
        }
    }

    if (!auth_key.empty()) {
        std::string token = cb_id + ":" + auth_key;

        AO_DBG_DEBUG("auth Token=%s", token.c_str());

        unsigned int base64_length = encode_base64_length(token.length());
        std::vector<unsigned char> base64 (base64_length + 1);

        // encode_base64() places a null terminator automatically, because the output is a string
        base64_length = encode_base64((const unsigned char*) token.c_str(), token.length(), &base64[0]);

        AO_DBG_DEBUG("auth64 len=%u, auth64 Token=%s", base64_length, &base64[0]);

        basic_auth64 = (const char*) &base64[0];
    } else {
        AO_DBG_DEBUG("no authentication");
        (void) 0;
    }
}

void AoMongooseAdapter::setBackendUrl(const char *backend_url_cstr) {
    if (!backend_url_cstr) {
        AO_DBG_ERR("invalid argument");
        return;
    }
    backend_url = backend_url_cstr;

    if (setting_backend_url) {
        *setting_backend_url = backend_url_cstr;
        ArduinoOcpp::configuration_save();
    }

    if (websocket) {
        websocket->is_closing = 1; //socket will be closed and connected again
    }

    credentials_changed = true; //reload composed credentials when reconnecting the next time
}

void AoMongooseAdapter::setChargeBoxId(const char *cb_id_cstr) {
    if (!cb_id_cstr) {
        AO_DBG_ERR("invalid argument");
        return;
    }
    cb_id = cb_id_cstr;

    if (setting_cb_id) {
        *setting_cb_id = cb_id_cstr;
        ArduinoOcpp::configuration_save();
    }

    if (websocket) {
        websocket->is_closing = 1; //socket will be closed and connected again
    }

    credentials_changed = true; //reload composed credentials when reconnecting the next time
}

void AoMongooseAdapter::setAuthKey(const char *auth_key_cstr) {
    if (!auth_key_cstr) {
        AO_DBG_ERR("invalid argument");
        return;
    }
    auth_key = auth_key_cstr;

    if (setting_auth_key) {
        *setting_auth_key = auth_key_cstr;
        ArduinoOcpp::configuration_save();
    }

    if (websocket) {
        websocket->is_closing = 1; //socket will be closed and connected again
    }

    credentials_changed = true; //reload composed credentials when reconnecting the next time
}

void AoMongooseAdapter::setCaCert(const char *ca_cert_cstr) {
    if (!ca_cert_cstr) {
        AO_DBG_ERR("invalid argument");
        return;
    }
    ca_cert = ca_cert_cstr;

#if !AO_CA_CERT_USE_FILE
    if (setting_ca_cert) {
        *setting_ca_cert = ca_cert_cstr;
        ArduinoOcpp::configuration_save();
    }
#endif

    if (websocket) {
        websocket->is_closing = 1; //socket will be closed and connected again
    }

    credentials_changed = true; //reload composed credentials when reconnecting the next time
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
#if AO_CA_CERT_USE_FILE
            struct mg_tls_opts opts = {.ca = osock->getCaCert()}; //CaCert is filename
#else
            struct mg_tls_opts opts = {.cert = osock->getCaCert()}; //CaCert is plain-text cert
#endif
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
