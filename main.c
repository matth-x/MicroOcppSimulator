#include "mongoose/mongoose.h"
#include <stdio.h>

static void fn(struct mg_connection* c, int ev, void* ev_data, void* fn_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_serve_opts opts = { .root_dir = "./public" };
        opts.extra_headers = "Content-Type: text/html\r\nContent-Encoding: gzip\r\n";
        mg_http_serve_file(c, ev_data, "public/index.html.gz", &opts);
    }
}

int main() {
  struct mg_mgr mgr;                                
  mg_mgr_init(&mgr);
  mg_http_listen(&mgr, "0.0.0.0:8000", fn, NULL);     // Create listening connection
  for (;;) mg_mgr_poll(&mgr, 1000);                   // Block forever
  return 0;
}