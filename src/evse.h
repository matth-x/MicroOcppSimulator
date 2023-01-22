#ifndef EVSE_H
#define EVSE_H

#include <array>
#include <string>

class Evse {
private:
    const unsigned int connectorId;

    bool trackEvPlugged = false;
    bool trackEvReady = false;

    std::string status;
public:
    Evse(unsigned int connectorId);

    void setup();

    void loop();

    void presentNfcTag(const char *uid);

    void setEvPlugged(bool plugged) {
        trackEvPlugged = plugged;
    }

    bool getEvPlugged() {return trackEvPlugged;}

    void setEvReady(bool ready) {
        trackEvReady = ready;
    }

    bool getEvReady() {return trackEvReady;}

    const char *getSessionIdTag();
    int getTransactionId();
    bool chargingPermitted();

    bool isCharging() {
        return chargingPermitted() && trackEvReady;
    }

    const char *getOcppStatus() {
        return status.c_str();
    }

    unsigned int getConnectorId() {
        return connectorId;
    }

};

extern std::array<Evse, AO_NUMCONNECTORS - 1> connectors;

#endif
