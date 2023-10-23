#ifndef MO_NET_MONGOOSE_H
#define MO_NET_MONGOOSE_H

#if MO_NETLIB == MO_NETLIB_MONGOOSE

#include "mongoose.h"

namespace MicroOcpp {
class MOcppMongooseClient;
}

void server_initialize(MicroOcpp::MOcppMongooseClient *osock);

void http_serve(struct mg_connection *c, int ev, void *ev_data, void *fn_data);

#endif //MO_NETLIB == MO_NETLIB_MONGOOSE

#endif
