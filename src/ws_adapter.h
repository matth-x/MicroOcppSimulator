#include "mongoose/mongoose.h"
#include <ArduinoOcpp/Core/OcppSocket.h>

#include <string>
#include <memory>

/*
 * If you prefer not to have the TLS-certificate managed by OCPP, store it into
 * a file on the flash filesystem, define the following build flag as 1 and
 * pass the filename to the constructor instead of a default plain-text certificate.
*/
#ifndef AO_CA_CERT_USE_FILE
#define AO_CA_CERT_USE_FILE 0
#endif

namespace ArduinoOcpp {
class FilesystemAdapter;
template<class T> class Configuration;
}

class AoMongooseAdapter : public ArduinoOcpp::OcppSocket {
private:
    struct mg_mgr *mgr {nullptr};
    struct mg_connection *websocket {nullptr};
    std::string backend_url;
    std::string cb_id;
    std::string url; //url = backend_url + '/' + cb_id
    std::string auth_key;
    std::string basic_auth64;
    std::string ca_cert;
    std::shared_ptr<ArduinoOcpp::Configuration<const char*>> setting_backend_url;
    std::shared_ptr<ArduinoOcpp::Configuration<const char*>> setting_cb_id;
    std::shared_ptr<ArduinoOcpp::Configuration<const char*>> setting_auth_key;
#if !AO_CA_CERT_USE_FILE
    std::shared_ptr<ArduinoOcpp::Configuration<const char*>> setting_ca_cert;
#endif
    unsigned long last_status_dbg_msg {0}, last_recv {0};
    unsigned long last_reconnection_attempt {-1UL / 2UL};
    std::shared_ptr<ArduinoOcpp::Configuration<int>> ws_ping_interval = 0; //heartbeat intervall in s. 0 sets hb off
    unsigned long last_hb {0};
    bool connection_established {false};
    ArduinoOcpp::ReceiveTXTcallback receiveTXTcallback = [] (const char *, size_t) {return false;};

    bool credentials_changed {true}; //set credentials to be reloaded
    void reload_credentials();

    void maintainWsConn();

public:
    AoMongooseAdapter(struct mg_mgr *mgr, 
            const char *backend_url_default = nullptr, 
            const char *charge_box_id_default = nullptr,
            const char *auth_key_default = nullptr,
            const char *CA_cert_default = nullptr, //if AO_CA_CERT_USE_FILE, then pass the filename, otherwise the plain-text CA_cert
            std::shared_ptr<ArduinoOcpp::FilesystemAdapter> filesystem = nullptr);

    ~AoMongooseAdapter();

    void loop() override;

    bool sendTXT(std::string &out) override;

    void setReceiveTXTcallback(ArduinoOcpp::ReceiveTXTcallback &receiveTXT) override {
        this->receiveTXTcallback = receiveTXT;
    }

    ArduinoOcpp::ReceiveTXTcallback &getReceiveTXTcallback() {
        return receiveTXTcallback;
    }

    void setBackendUrl(const char *backend_url);
    void setChargeBoxId(const char *cb_id);
    void setAuthKey(const char *auth_key);
    void setCaCert(const char *ca_cert); //if AO_CA_CERT_USE_FILE, then pass the filename, otherwise the plain-text CA_cert

    const char *getBackendUrl() {return backend_url.c_str();}
    const char *getChargeBoxId() {return cb_id.c_str();}
    const char *getAuthKey() {return auth_key.c_str();}
    const char *getCaCert() {return ca_cert.c_str();}

    const char *getUrl() {return url.c_str();}

    void setConnectionEstablished(bool established);

    void updateRcvTimer();
};
