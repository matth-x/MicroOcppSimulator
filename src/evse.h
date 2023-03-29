#ifndef EVSE_H
#define EVSE_H

#include <array>
#include <string>

class Evse {
private:
    const unsigned int connectorId;

    bool trackEvPlugged = false;
    bool trackEvReady = false;
    bool trackEvseReady = false;

    const float SIMULATE_POWER_CONST = 11000.f;
    float simulate_power = 0;
    float limit_power = 11000.f;
    const float SIMULATE_ENERGY_DELTA_MS = SIMULATE_POWER_CONST / (3600.f * 1000.f);
    ulong simulate_energy_track_time = 0;
    float simulate_energy = 0;

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

    void setEvseReady(bool ready) {
        trackEvseReady = ready;
    }

    bool getEvseReady() {return trackEvseReady;}

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

    int getEnergy() {
        return (int) simulate_energy;
    }

    int getPower();

    float getVoltage();

    float getCurrent() {
        float volts = getVoltage();
        if (volts <= 0.f) {
            return 0.f;
        }
        return 0.333f * (float) getPower() / volts;
    }

    int getSmartChargingMaxPower() {
        return limit_power;
    }

    float getSmartChargingMaxCurrent() {
        float volts = getVoltage();
        if (volts <= 0.f) {
            return 0.f;
        }
        return 0.333f * (float) getSmartChargingMaxPower() / volts;
    }

};

extern std::array<Evse, AO_NUMCONNECTORS - 1> connectors;

#endif
