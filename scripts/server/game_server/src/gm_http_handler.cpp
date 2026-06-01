#include "gm_http_handler.h"
#include "gm_stub.h"
#include "player_manager.h"
#include "player.h"
#include "log_macros.h"

#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <filesystem>

namespace farm {

using json = nlohmann::json;

GmHttpHandler::GmHttpHandler(GMStub* gm_stub, PlayerManager* player_mgr)
    : gm_stub_(gm_stub)
    , player_mgr_(player_mgr)
    , http_server_(nullptr)
    , port_(0)
{
}

GmHttpHandler::~GmHttpHandler() {
    stop();
}

bool GmHttpHandler::start(struct event_base* base, uint16_t port, const std::string& static_dir) {
    port_ = port;
    static_dir_ = static_dir;

    http_server_ = evhttp_new(base);
    if (!http_server_) {
        SPDLOG_ERROR("[GM-HTTP]Failed to create evhttp server");
        return false;
    }

    evhttp_set_gencb(http_server_, on_request, this);

    if (evhttp_bind_socket(http_server_, "0.0.0.0", port) != 0) {
        SPDLOG_ERROR("[GM-HTTP]Failed to bind HTTP server on port {}", port);
        evhttp_free(http_server_);
        http_server_ = nullptr;
        return false;
    }

    SPDLOG_INFO("[GM-HTTP]GM HTTP server listening on port {}", port);
    return true;
}

void GmHttpHandler::stop() {
    if (http_server_) {
        evhttp_free(http_server_);
        http_server_ = nullptr;
    }
}

void GmHttpHandler::on_request(struct evhttp_request* req, void* ctx) {
    auto* handler = static_cast<GmHttpHandler*>(ctx);
    handler->handle_request(req);
}

void GmHttpHandler::handle_request(struct evhttp_request* req) {
    const char* uri = evhttp_request_get_uri(req);
    std::string uri_str(uri ? uri : "");

    if (uri_str == "/gm" || uri_str == "/gm/") {
        handle_static_file(req);
        return;
    }

    if (uri_str == "/api/gm/exec" && evhttp_request_get_command(req) == EVHTTP_REQ_POST) {
        handle_api_exec(req);
        return;
    }
    if (uri_str == "/api/gm/batch_exec" && evhttp_request_get_command(req) == EVHTTP_REQ_POST) {
        handle_api_batch_exec(req);
        return;
    }
    if (uri_str == "/api/gm/commands") {
        handle_api_commands(req);
        return;
    }
    if (uri_str == "/api/gm/templates") {
        handle_api_templates(req);
        return;
    }
    if (uri_str.find("/api/gm/online_players") == 0) {
        handle_api_online_players(req);
        return;
    }
    if (uri_str == "/api/gm/servers") {
        handle_api_servers(req);
        return;
    }

    handle_static_file(req);
}

void GmHttpHandler::handle_api_exec(struct evhttp_request* req) {
    std::string body = read_request_body(req);
    if (body.empty()) {
        send_error(req, HTTP_BADREQUEST, -1, "请求体为空");
        return;
    }

    json request;
    try {
        request = json::parse(body);
    } catch (const json::parse_error& e) {
        send_error(req, HTTP_BADREQUEST, -1, std::string("JSON 解析失败: ") + e.what());
        return;
    }

    if (!request.contains("cmd") || !request["cmd"].is_string()) {
        send_error(req, HTTP_BADREQUEST, -1, "缺少 cmd 字段");
        return;
    }

    std::string cmd = request["cmd"].get<std::string>();
    json args = request.value("args", json::object());

    json result = gm_stub_->exec(cmd, args);
    send_json_response(req, HTTP_OK, result.dump());
}

void GmHttpHandler::handle_api_batch_exec(struct evhttp_request* req) {
    std::string body = read_request_body(req);
    if (body.empty()) {
        send_error(req, HTTP_BADREQUEST, -1, "请求体为空");
        return;
    }

    json request;
    try {
        request = json::parse(body);
    } catch (const json::parse_error& e) {
        send_error(req, HTTP_BADREQUEST, -1, std::string("JSON 解析失败: ") + e.what());
        return;
    }

    if (!request.contains("template") || !request["template"].is_string()) {
        send_error(req, HTTP_BADREQUEST, -1, "缺少 template 字段");
        return;
    }
    if (!request.contains("player_id") || !request["player_id"].is_number_unsigned()) {
        send_error(req, HTTP_BADREQUEST, -1, "缺少 player_id 字段");
        return;
    }

    std::string template_name = request["template"].get<std::string>();
    uint64_t player_id = request["player_id"].get<uint64_t>();
    uint32_t server_id = request.value("server_id", 1u);

    json result = gm_stub_->batch_exec(template_name, player_id, server_id);
    send_json_response(req, HTTP_OK, result.dump());
}

void GmHttpHandler::handle_api_commands(struct evhttp_request* req) {
    json result = gm_stub_->get_commands();
    send_json_response(req, HTTP_OK, result.dump());
}

void GmHttpHandler::handle_api_templates(struct evhttp_request* req) {
    json result = gm_stub_->get_templates();
    send_json_response(req, HTTP_OK, result.dump());
}

void GmHttpHandler::handle_api_online_players(struct evhttp_request* req) {
    auto players = player_mgr_->get_all_players();

    json players_json = json::array();
    for (const auto* player : players) {
        json p;
        p["player_id"] = player->player_id();
        p["role_name"] = player->get_role_name();
        p["level"] = player->get_level();
        players_json.push_back(p);
    }

    json result;
    result["code"] = 0;
    result["msg"] = "ok";
    result["players"] = players_json;
    result["count"] = players_json.size();

    send_json_response(req, HTTP_OK, result.dump());
}

void GmHttpHandler::handle_api_servers(struct evhttp_request* req) {
    json result;
    result["code"] = 0;
    result["servers"] = json::array({
        {{"server_id", 1}, {"name", "game_server_1"}, {"online", player_mgr_->player_count()}}
    });

    send_json_response(req, HTTP_OK, result.dump());
}

void GmHttpHandler::handle_static_file(struct evhttp_request* req) {
    std::string file_path = static_dir_ + "/gm.html";

    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        send_error(req, HTTP_NOTFOUND, -1, "文件不存在: " + file_path);
        return;
    }

    std::ostringstream content;
    content << file.rdbuf();
    std::string body = content.str();

    struct evbuffer* evb = evbuffer_new();
    evbuffer_add(evb, body.c_str(), body.size());

    struct evkeyvalq* headers = evhttp_request_get_output_headers(req);
    evhttp_add_header(headers, "Content-Type", "text/html; charset=utf-8");
    evhttp_add_header(headers, "Cache-Control", "no-cache");

    evhttp_send_reply(req, HTTP_OK, "OK", evb);
    evbuffer_free(evb);
}

std::string GmHttpHandler::read_request_body(struct evhttp_request* req) {
    struct evbuffer* input = evhttp_request_get_input_buffer(req);
    if (!input) return "";

    size_t len = evbuffer_get_length(input);
    if (len == 0) return "";

    std::string body(len, '\0');
    evbuffer_copyout(input, &body[0], len);
    return body;
}

void GmHttpHandler::send_json_response(struct evhttp_request* req, int code, const std::string& json_str) {
    struct evbuffer* evb = evbuffer_new();
    evbuffer_add(evb, json_str.c_str(), json_str.size());

    struct evkeyvalq* headers = evhttp_request_get_output_headers(req);
    evhttp_add_header(headers, "Content-Type", "application/json; charset=utf-8");
    evhttp_add_header(headers, "Access-Control-Allow-Origin", "*");

    evhttp_send_reply(req, code, "OK", evb);
    evbuffer_free(evb);
}

void GmHttpHandler::send_error(struct evhttp_request* req, int http_code, int app_code, const std::string& msg) {
    json result;
    result["code"] = app_code;
    result["msg"] = msg;
    send_json_response(req, http_code, result.dump());
}

}  // namespace farm
