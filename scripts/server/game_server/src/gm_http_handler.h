#pragma once

#include <event2/http.h>
#include <string>
#include <cstdint>

namespace farm {

class GMStub;
class PlayerManager;

class GmHttpHandler {
public:
    GmHttpHandler(GMStub* gm_stub, PlayerManager* player_mgr);
    ~GmHttpHandler();

    // Start HTTP server on given port, serving static files from static_dir
    bool start(struct event_base* base, uint16_t port, const std::string& static_dir);

    // Stop HTTP server
    void stop();

private:
    // Static callback dispatched by evhttp
    static void on_request(struct evhttp_request* req, void* ctx);

    // Route dispatch
    void handle_request(struct evhttp_request* req);

    // API handlers
    void handle_api_exec(struct evhttp_request* req);
    void handle_api_batch_exec(struct evhttp_request* req);
    void handle_api_commands(struct evhttp_request* req);
    void handle_api_templates(struct evhttp_request* req);
    void handle_api_online_players(struct evhttp_request* req);
    void handle_api_servers(struct evhttp_request* req);

    // Static file serving
    void handle_static_file(struct evhttp_request* req);

    // Helpers
    std::string read_request_body(struct evhttp_request* req);
    void send_json_response(struct evhttp_request* req, int code, const std::string& json_str);
    void send_error(struct evhttp_request* req, int http_code, int app_code, const std::string& msg);

    GMStub* gm_stub_;
    PlayerManager* player_mgr_;
    struct evhttp* http_server_;
    uint16_t port_;
    std::string static_dir_;
};

}  // namespace farm
