#include "mongoose/mongoose.h"
#include <ArduinoOcpp/Core/OcppSocket.h>
#include <ArduinoOcpp/Core/OcppServer.h> //for typedef ReceiveTXTcallback
#include <ArduinoOcpp/Platform.h>

#include <string>

class AoMongooseAdapter : public ArduinoOcpp::OcppSocket {
private:
    struct mg_mgr *mgr {nullptr};
    struct mg_connection *websocket {nullptr};
    std::string backend_url;
    std::string cbId;
    std::string url; //url = backend_url + '/' + cbId
    std::string ca;
    std::string basic_auth;
    std::string basic_auth64;
    decltype(ao_tick_ms()) last_status_dbg_msg {0}, last_recv {0};
    decltype(ao_tick_ms()) last_reconnection_attempt {-1UL / 2UL};
    unsigned long hb_interval = 0; //heartbeat intervall in ms. 0 sets hb off
    decltype(ao_tick_ms()) last_hb {0};
    bool connection_established {false};
    ArduinoOcpp::ReceiveTXTcallback receiveTXTcallback = [] (const char *, size_t) {return false;};

    void maintainWsConn();

public:
    AoMongooseAdapter(struct mg_mgr *mgr);

    ~AoMongooseAdapter();

    void loop() override;

    bool sendTXT(std::string &out) override;

    void setReceiveTXTcallback(ArduinoOcpp::ReceiveTXTcallback &receiveTXT) override {
        this->receiveTXTcallback = receiveTXT;
    }

    ArduinoOcpp::ReceiveTXTcallback &getReceiveTXTcallback() {
        return receiveTXTcallback;
    }

    void setUrl(const char *backend_url_cstr, const char *cbId_cstr);

    const char *getUrl() {
        return url.c_str();
    }

    const char *getUrlBackend() {return backend_url.c_str();}
    const char *getChargeBoxId() {return cbId.c_str();}

    void setCa(const char *ca);

    const char *getCa() {
        return ca.c_str();
    }

    void setAuthToken(const char *basic_auth);

    const char *getAuthToken() {return basic_auth.c_str();}
    const char *getAuthTokenBase64() {
        return basic_auth64.c_str();
    }

    void setHeartbeatInterval(unsigned long hb_interval_ms) {
        this->hb_interval = hb_interval_ms;
    }

    void setConnectionEstablished(bool established);

    void updateRcvTimer();
};

extern AoMongooseAdapter osock;
