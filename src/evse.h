// matth-x/MicroOcppSimulator
// Copyright Matthias Akstaller 2022 - 2024
// GPL-3.0 License

#ifndef EVSE_H
#define EVSE_H

#include <array>
#include <string>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Version.h>

#define SIMULATOR_FN MO_FILENAME_PREFIX "simulator.jsn"

class Evse {
private:
    const unsigned int connectorId;

    std::shared_ptr<MicroOcpp::Configuration> trackEvPluggedBool;
    std::string trackEvPluggedKey;
    std::shared_ptr<MicroOcpp::Configuration> trackEvsePluggedBool;
    std::string trackEvsePluggedKey;
    std::shared_ptr<MicroOcpp::Configuration> trackEvReadyBool;
    std::string trackEvReadyKey;
    std::shared_ptr<MicroOcpp::Configuration> trackEvseReadyBool;
    std::string trackEvseReadyKey;

    const float SIMULATE_POWER_CONST = 11000.f;
    float simulate_power = 0;
    float limit_power = 11000.f;
    const float SIMULATE_ENERGY_DELTA_MS = SIMULATE_POWER_CONST / (3600.f * 1000.f);
    unsigned long simulate_energy_track_time = 0;
    float simulate_energy = 0;

    std::string status;
public:
    Evse(unsigned int connectorId);

    void setup();

    void loop();

    void presentNfcTag(const char *uid);

#if MO_ENABLE_V201
    bool presentNfcTag(const char *uid, const char *type);
#endif //MO_ENABLE_V201

    void setEvPlugged(bool plugged);

    bool getEvPlugged();

    void setEvsePlugged(bool plugged);
    
    bool getEvsePlugged();

    void setEvReady(bool ready);

    bool getEvReady();

    void setEvseReady(bool ready);

    bool getEvseReady();

    const char *getSessionIdTag();
    int getTransactionId();
    bool chargingPermitted();

    bool isCharging() {
        return chargingPermitted() && trackEvReadyBool->getBool();
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

extern std::array<Evse, MO_NUMCONNECTORS - 1> connectors;

#endif
