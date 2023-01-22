#include "mongoose.h"

namespace ArduinoOcpp {
class AOcppMongooseClient;
}

void server_initialize(ArduinoOcpp::AOcppMongooseClient *osock);

void http_serve(struct mg_connection *c, int ev, void *ev_data, void *fn_data);
