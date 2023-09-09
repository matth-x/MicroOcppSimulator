#ifndef MOCPP_NET_MONGOOSE_H
#define MOCPP_NET_MONGOOSE_H

#if MOCPP_NETLIB == MOCPP_NETLIB_MONGOOSE

#include "mongoose.h"

namespace MicroOcpp {
class MOcppMongooseClient;
}

void server_initialize(MicroOcpp::MOcppMongooseClient *osock);

void http_serve(struct mg_connection *c, int ev, void *ev_data, void *fn_data);

#endif //MOCPP_NETLIB == MOCPP_NETLIB_MONGOOSE

#endif
