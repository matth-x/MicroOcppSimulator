// matth-x/MicroOcppSimulator
// Copyright Matthias Akstaller 2022 - 2024
// GPL-3.0 License

#include "net_wasm.h"

#include <string>
#include <memory>
#include <emscripten/websocket.h>

#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Debug.h>

#include "api.h"

#define DEBUG_MSG_INTERVAL 5000UL
#define WS_UNRESPONSIVE_THRESHOLD_MS 15000UL

using namespace MicroOcpp;

class WasmOcppConnection : public Connection {
private:
    EMSCRIPTEN_WEBSOCKET_T websocket;
    std::string backend_url;
    std::string cb_id;
    std::string url; //url = backend_url + '/' + cb_id
    std::string auth_key;
    std::string basic_auth64;
    std::shared_ptr<Configuration> setting_backend_url_str;
    std::shared_ptr<Configuration> setting_cb_id_str;
    std::shared_ptr<Configuration> setting_auth_key_str;
    unsigned long last_status_dbg_msg {0}, last_recv {0};
    std::shared_ptr<Configuration> reconnect_interval_int; //minimum time between two connect trials in s
    unsigned long last_reconnection_attempt {-1UL / 2UL};
    std::shared_ptr<Configuration> stale_timeout_int; //inactivity period after which the connection will be closed
    std::shared_ptr<Configuration> ws_ping_interval_int; //heartbeat intervall in s. 0 sets hb off
    unsigned long last_hb {0};
    bool connection_established {false};
    unsigned long last_connection_established {-1UL / 2UL};
    bool connection_closing {false};
    ReceiveTXTcallback receiveTXTcallback = [] (const char *, size_t) {return false;};

    void maintainWsConn() {
        if (mocpp_tick_ms() - last_status_dbg_msg >= DEBUG_MSG_INTERVAL) {
            last_status_dbg_msg = mocpp_tick_ms();

            //WS successfully connected?
            if (!isConnectionOpen()) {
                MO_DBG_DEBUG("WS unconnected");
            } else if (mocpp_tick_ms() - last_recv >= (ws_ping_interval_int && ws_ping_interval_int->getInt() > 0 ? (ws_ping_interval_int->getInt() * 1000UL) : 0UL) + WS_UNRESPONSIVE_THRESHOLD_MS) {
                //WS connected but unresponsive
                MO_DBG_DEBUG("WS unresponsive");
            }
        }

        if (websocket && isConnectionOpen() &&
                stale_timeout_int && stale_timeout_int->getInt() > 0 && mocpp_tick_ms() - last_recv >= (stale_timeout_int->getInt() * 1000UL)) {
            MO_DBG_INFO("connection %s -- stale, reconnect", url.c_str());
            reconnect();
            return;
        }

        if (websocket && isConnectionOpen() &&
                ws_ping_interval_int && ws_ping_interval_int->getInt() > 0 && mocpp_tick_ms() - last_hb >= (ws_ping_interval_int->getInt() * 1000UL)) {
            last_hb = mocpp_tick_ms();
            MO_DBG_VERBOSE("omit heartbeat");
        }

        if (websocket != NULL) { //connection pointer != NULL means that the socket is still open
            return;
        }

        if (url.empty()) {
            //cannot open OCPP connection: credentials missing
            return;
        }

        if (reconnect_interval_int && reconnect_interval_int->getInt() > 0 && mocpp_tick_ms() - last_reconnection_attempt < (reconnect_interval_int->getInt() * 1000UL)) {
            return;
        }

        MO_DBG_DEBUG("(re-)connect to %s", url.c_str());

        last_reconnection_attempt = mocpp_tick_ms();

        EmscriptenWebSocketCreateAttributes attr;
        emscripten_websocket_init_create_attributes(&attr);

        attr.url = url.c_str();
        attr.protocols = "ocpp1.6";
        attr.createOnMainThread = true;

        websocket = emscripten_websocket_new(&attr);
        if (websocket < 0) {
            MO_DBG_ERR("emscripten_websocket_new: %i", websocket);
            websocket = 0;
        }

        if (websocket <= 0) {
            return;
        }

        auto ret_open = emscripten_websocket_set_onopen_callback(
                websocket, 
                this,
                [] (int eventType, const EmscriptenWebSocketOpenEvent *websocketEvent, void *userData) -> EM_BOOL {
                    WasmOcppConnection *conn = reinterpret_cast<WasmOcppConnection*>(userData);
                    MO_DBG_DEBUG("on open eventType: %i", eventType);
                    MO_DBG_INFO("connection %s -- connected!", conn->getUrl());
                    conn->setConnectionOpen(true);
                    conn->updateRcvTimer();
                    return true;
                });
        if (ret_open < 0) {
            MO_DBG_ERR("emscripten_websocket_set_onopen_callback: %i", ret_open);
        }
        
        auto ret_message = emscripten_websocket_set_onmessage_callback(
                websocket,
                this,
                [] (int eventType, const EmscriptenWebSocketMessageEvent *websocketEvent, void *userData) -> EM_BOOL {
                    WasmOcppConnection *conn = reinterpret_cast<WasmOcppConnection*>(userData);
                    MO_DBG_DEBUG("evenType: %i", eventType);
                    MO_DBG_DEBUG("txt: %s", websocketEvent->data ? (const char*) websocketEvent->data : "undefined");
                    if (!conn->getReceiveTXTcallback()((const char*) websocketEvent->data, websocketEvent->numBytes)) {
                        MO_DBG_WARN("processing input message failed");
                    }
                    conn->updateRcvTimer();
                    return true;
                });
        if (ret_message < 0) {
            MO_DBG_ERR("emscripten_websocket_set_onmessage_callback: %i", ret_message);
        }

        auto ret_err = emscripten_websocket_set_onerror_callback(
                websocket, 
                this,
                [] (int eventType, const EmscriptenWebSocketErrorEvent *websocketEvent, void *userData) -> EM_BOOL {
                    WasmOcppConnection *conn = reinterpret_cast<WasmOcppConnection*>(userData);
                    MO_DBG_DEBUG("on error eventType: %i", eventType);
                    MO_DBG_INFO("connection %s -- %s", conn->getUrl(), "error");
                    conn->cleanConnection();
                    return true;
                });
        if (ret_open < 0) {
            MO_DBG_ERR("emscripten_websocket_set_onopen_callback: %i", ret_open);
        }

        auto ret_close = emscripten_websocket_set_onclose_callback(
                websocket, 
                this,
                [] (int eventType, const EmscriptenWebSocketCloseEvent *websocketEvent, void *userData) -> EM_BOOL {
                    WasmOcppConnection *conn = reinterpret_cast<WasmOcppConnection*>(userData);
                    MO_DBG_DEBUG("on close eventType: %i", eventType);
                    MO_DBG_INFO("connection %s -- %s", conn->getUrl(), "closed");
                    conn->cleanConnection();
                    return true;
                });
        if (ret_open < 0) {
            MO_DBG_ERR("emscripten_websocket_set_onopen_callback: %i", ret_open);
        }
    }

    void reconnect() {
        if (!websocket) {
            return;
        }
        auto ret = emscripten_websocket_close(websocket, 1000, "reconnect");
        if (ret < 0) {
            MO_DBG_ERR("emscripten_websocket_close: %i", ret);
        }
        setConnectionOpen(false);
    }

public:
    WasmOcppConnection(
            const char *backend_url_factory, 
            const char *charge_box_id_factory,
            const char *auth_key_factory) {

        setting_backend_url_str = declareConfiguration<const char*>(
            MO_CONFIG_EXT_PREFIX "BackendUrl", backend_url_factory, CONFIGURATION_VOLATILE, true, true);
        setting_cb_id_str = declareConfiguration<const char*>(
            MO_CONFIG_EXT_PREFIX "ChargeBoxId", charge_box_id_factory, CONFIGURATION_VOLATILE, true, true);
        setting_auth_key_str = declareConfiguration<const char*>(
            "AuthorizationKey", auth_key_factory, CONFIGURATION_VOLATILE, true, true);
        ws_ping_interval_int = declareConfiguration<int>(
            "WebSocketPingInterval", 5, CONFIGURATION_VOLATILE);
        reconnect_interval_int = declareConfiguration<int>(
            MO_CONFIG_EXT_PREFIX "ReconnectInterval", 10, CONFIGURATION_VOLATILE);
        stale_timeout_int = declareConfiguration<int>(
            MO_CONFIG_EXT_PREFIX "StaleTimeout", 300, CONFIGURATION_VOLATILE);

        reloadConfigs(); //load WS creds with configs values

        MO_DBG_DEBUG("connection initialized");

        maintainWsConn();
    }

    ~WasmOcppConnection() {
        if (websocket) {
            emscripten_websocket_delete(websocket);
            websocket = NULL;
        }
    }

    void loop() override {
        maintainWsConn();
    }

    bool sendTXT(const char *msg, size_t length) override {
        if (!websocket || !isConnectionOpen()) {
            return false;
        }

        if (auto ret = emscripten_websocket_send_utf8_text(websocket, msg) < 0) {
            MO_DBG_ERR("emscripten_websocket_send_utf8_text: %i", ret);
            return false;
        }

        return true;
    }

    void setReceiveTXTcallback(MicroOcpp::ReceiveTXTcallback &receiveTXT) override {
        this->receiveTXTcallback = receiveTXT;
    }

    MicroOcpp::ReceiveTXTcallback &getReceiveTXTcallback() {
        return receiveTXTcallback;
    }

    void setBackendUrl(const char *backend_url_cstr) {
        if (!backend_url_cstr) {
            MO_DBG_ERR("invalid argument");
            return;
        }

        if (setting_backend_url_str) {
            setting_backend_url_str->setString(backend_url_cstr);
            configuration_save();
        }
    }

    void setChargeBoxId(const char *cb_id_cstr) {
        if (!cb_id_cstr) {
            MO_DBG_ERR("invalid argument");
            return;
        }

        if (setting_cb_id_str) {
            setting_cb_id_str->setString(cb_id_cstr);
            configuration_save();
        }
    }

    void setAuthKey(const char *auth_key_cstr) {
        if (!auth_key_cstr) {
            MO_DBG_ERR("invalid argument");
            return;
        }

        if (setting_auth_key_str) {
            setting_auth_key_str->setString(auth_key_cstr);
            configuration_save();
        }
    }

    void reloadConfigs() {

        reconnect(); //closes WS connection; will be reopened in next maintainWsConn execution

        /*
        * reload WS credentials from configs
        */
        if (setting_backend_url_str) {
            backend_url = setting_backend_url_str->getString();
        }

        if (setting_cb_id_str) {
            cb_id = setting_cb_id_str->getString();
        }

        if (setting_auth_key_str) {
            auth_key = setting_auth_key_str->getString();
        }

        /*
        * determine new URL and auth token with updated WS credentials
        */

        url.clear();
        basic_auth64.clear();

        if (backend_url.empty()) {
            MO_DBG_DEBUG("empty URL closes connection");
            return;
        } else {
            url = backend_url;

            if (url.back() != '/' && !cb_id.empty()) {
                url.append("/");
            }

            url.append(cb_id);
        }

        if (!auth_key.empty()) {
            MO_DBG_WARN("WASM app does not support Securiy Profile 2 yet");
        } else {
            MO_DBG_DEBUG("no authentication");
            (void) 0;
        }
    }

    const char *getBackendUrl() {return backend_url.c_str();}
    const char *getChargeBoxId() {return cb_id.c_str();}
    const char *getAuthKey() {return auth_key.c_str();}

    const char *getUrl() {return url.c_str();}

    void setConnectionOpen(bool open) {
        if (open) {
            connection_established = true;
            last_connection_established = mocpp_tick_ms();
        } else {
            connection_closing = true;
        }
    }
    bool isConnectionOpen() {return connection_established && !connection_closing;}
    void cleanConnection() {
        connection_established = false;
        connection_closing = false;
        websocket = NULL;
    }

    void updateRcvTimer() {
        last_recv = mocpp_tick_ms();
    }
    unsigned long getLastRecv() override {
        return last_recv;
    }
    unsigned long getLastConnected() override {
        return last_connection_established;
    }

};

WasmOcppConnection *wasm_ocpp_connection_instance = nullptr;

MicroOcpp::Connection *wasm_ocpp_connection_init(const char *backend_url_default, const char *charge_box_id_default, const char *auth_key_default) {
    if (!wasm_ocpp_connection_instance) {
        wasm_ocpp_connection_instance = new WasmOcppConnection(backend_url_default, charge_box_id_default, auth_key_default);
    }

    return wasm_ocpp_connection_instance;
}

#define MO_WASM_RESP_BUF_SIZE 1024
char wasm_resp_buf [MO_WASM_RESP_BUF_SIZE] = {'\0'};

//exported to WebAssembly
extern "C" char* mocpp_wasm_api_call(const char *endpoint, const char *method, const char *body) {
    MO_DBG_DEBUG("API call: %s, %s, %s", endpoint, method, body);

    auto method_parsed = Method::UNDEFINED;
    if (!strcmp(method, "GET")) {
        method_parsed = Method::GET;
    } else if (!strcmp(method, "POST")) {
        method_parsed = Method::POST;
    }

    //handle endpoint /websocket
    if (!strcmp(endpoint, "/websocket")) {
        sprintf(wasm_resp_buf, "%s", "{}");
        if (!wasm_ocpp_connection_instance) {
            MO_DBG_ERR("no websocket instance");
            return nullptr;
        }
        StaticJsonDocument<512> request;
        if (*body) {
            auto err = deserializeJson(request, body);
            if (err) {
                MO_DBG_WARN("malformatted body: %s", err.c_str());
                return nullptr;
            }
        }

        auto webSocketPingInterval = declareConfiguration<int>("WebSocketPingInterval", 5, CONFIGURATION_VOLATILE);
        auto reconnectInterval = declareConfiguration<int>(MO_CONFIG_EXT_PREFIX "ReconnectInterval", 10, CONFIGURATION_VOLATILE);

        if (method_parsed == Method::POST) {
            if (request.containsKey("backendUrl")) {
                wasm_ocpp_connection_instance->setBackendUrl(request["backendUrl"] | "");
            }
            if (request.containsKey("chargeBoxId")) {
                wasm_ocpp_connection_instance->setChargeBoxId(request["chargeBoxId"] | "");
            }
            if (request.containsKey("authorizationKey")) {
                wasm_ocpp_connection_instance->setAuthKey(request["authorizationKey"] | "");
            }
            wasm_ocpp_connection_instance->reloadConfigs();
            if (request.containsKey("pingInterval")) {
                webSocketPingInterval->setInt(request["pingInterval"] | 0);
            }
            if (request.containsKey("reconnectInterval")) {
                reconnectInterval->setInt(request["reconnectInterval"] | 0);
            }
            if (request.containsKey("dnsUrl")) {
                MO_DBG_WARN("dnsUrl not implemented");
                (void)0;
            }
            MicroOcpp::configuration_save();
        }

        StaticJsonDocument<512> response;
        response["backendUrl"] = wasm_ocpp_connection_instance->getBackendUrl();
        response["chargeBoxId"] = wasm_ocpp_connection_instance->getChargeBoxId();
        response["authorizationKey"] = wasm_ocpp_connection_instance->getAuthKey();

        response["pingInterval"] = webSocketPingInterval ? webSocketPingInterval->getInt() : 0;
        response["reconnectInterval"] = reconnectInterval ? reconnectInterval->getInt() : 0;
        serializeJson(response, wasm_resp_buf, MO_WASM_RESP_BUF_SIZE);
        return wasm_resp_buf;
    }
    
    //forward all other endpoints to main API
    int status = mocpp_api_call(endpoint, method_parsed, body, wasm_resp_buf, MO_WASM_RESP_BUF_SIZE);

    if (status == 200) {
        //200: HTTP status code Success
        MO_DBG_DEBUG("API resp: %s", wasm_resp_buf);
        return wasm_resp_buf;
    } else {
        MO_DBG_DEBUG("API err: %i", status);
        return nullptr;
    }
}
