#include <iostream>
#include "mongoose.h"
#include <ArduinoOcpp.h>
#include <ArduinoOcppMongooseClient.h>

#include "evse.h"
#include "webserver.h"


struct mg_mgr mgr;
ArduinoOcpp::AOcppMongooseClient *osock;

#if AO_NUMCONNECTORS == 3
std::array<Evse, AO_NUMCONNECTORS - 1> connectors {{1,2}};
#else
std::array<Evse, AO_NUMCONNECTORS - 1> connectors {{1}};
#endif

int main() {
    mg_log_set(MG_LL_INFO);                            
    mg_mgr_init(&mgr);

    mg_http_listen(&mgr, "0.0.0.0:8000", http_serve, NULL);     // Create listening connection

    auto filesystem = ArduinoOcpp::makeDefaultFilesystemAdapter(ArduinoOcpp::FilesystemOpt::Use_Mount_FormatOnFail);

    osock = new ArduinoOcpp::AOcppMongooseClient(&mgr,
        "ws://echo.websocket.events",
        "charger-01",
        "",
        "",
        filesystem);
    
    server_initialize(osock);
    ocpp_initialize(*osock,
            ChargerCredentials("Demo Charger", "My Company Ltd."),
            filesystem);

    for (unsigned int i = 0; i < connectors.size(); i++) {
        connectors[i].setup();
    }

    for (;;) {                    // Block forever
        mg_mgr_poll(&mgr, 100);
        ocpp_loop();
        for (unsigned int i = 0; i < connectors.size(); i++) {
            connectors[i].loop();
        }
    }

    delete osock;
    mg_mgr_free(&mgr);
    return 0;
}
