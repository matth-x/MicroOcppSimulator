#include "mongoose/mongoose.h"

class AoMongooseAdapter;

void server_initialize(AoMongooseAdapter *osock);

void http_serve(struct mg_connection *c, int ev, void *ev_data, void *fn_data);
