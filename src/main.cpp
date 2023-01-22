#include "mongoose.h"
#include "webserver.h"
#include <ArduinoOcppMongooseClient.h>
#include "evse.h"
#include <iostream>

#include <ArduinoOcpp.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Core/FilesystemAdapter.h>
#include <ArduinoOcpp/Platform.h>

#define OCPP_CREDENTIALS_FN (AO_FILENAME_PREFIX "/ocpp-creds.jsn")

struct mg_mgr mgr;
ArduinoOcpp::AOcppMongooseClient *osock;

#if AO_NUMCONNECTORS == 3
std::array<Evse, AO_NUMCONNECTORS - 1> connectors {{1,2}};
#else
std::array<Evse, AO_NUMCONNECTORS - 1> connectors {{1}};
#endif

int main() {
    mg_log_set(MG_LL_DEBUG);                            
    mg_mgr_init(&mgr);
    
    mg_http_listen(&mgr, "0.0.0.0:8000", http_serve, NULL);     // Create listening connection

    std::shared_ptr<ArduinoOcpp::FilesystemAdapter> filesystem = 
        ArduinoOcpp::makeDefaultFilesystemAdapter(ArduinoOcpp::FilesystemOpt::Use_Mount_FormatOnFail);

    osock = new ArduinoOcpp::AOcppMongooseClient(&mgr,
        "ws://echo.websocket.events",
        "charger-01",
        "",
        "",
        filesystem);
    
    server_initialize(osock);
    OCPP_initialize(*osock);

    for (unsigned int i = 0; i < connectors.size(); i++) {
        connectors[i].setup();
    }

    bootNotification("Demo Client", "ArduinoOcpp");

    for (;;) {                    // Block forever
        mg_mgr_poll(&mgr, 100);
        OCPP_loop();
        for (unsigned int i = 0; i < connectors.size(); i++) {
            connectors[i].loop();
        }
    }

    delete osock;
    mg_mgr_free(&mgr);
    return 0;
}
