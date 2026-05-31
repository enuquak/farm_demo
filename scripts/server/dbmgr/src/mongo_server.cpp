#include "mongo_server.h"
#include "log_macros.h"

#include <mongoc/mongoc.h>
#include <nlohmann/json.hpp>
#include <fstream>

namespace farm {

MongoServer::MongoServer(MongoConnection& conn, const std::string& index_config_dir)
    : conn_(conn)
    , index_config_dir_(index_config_dir) {
}

bool MongoServer::init() {
    if (!conn_.is_connected()) {
        SPDLOG_ERROR("[MongoServer]MongoDB not connected");
        return false;
    }

    // 创建 players 集合索引
    if (!create_indexes("players")) {
        SPDLOG_ERROR("[MongoServer]Failed to create players indexes");
        return false;
    }

    // 创建 accounts 集合索引
    if (!create_indexes("accounts")) {
        SPDLOG_ERROR("[MongoServer]Failed to create accounts indexes");
        return false;
    }

    SPDLOG_INFO("[MongoServer]Initialization complete");
    return true;
}

bool MongoServer::create_indexes(const std::string& collection_name) {
    std::vector<bson_t*> indexes;
    if (!read_index_config(collection_name, indexes)) {
        SPDLOG_WARN("[MongoServer]No index config for collection: {}, skipping", collection_name);
        return true;  // 没有配置文件不算失败
    }

    mongoc_collection_t* collection = get_collection(collection_name);
    if (!collection) {
        SPDLOG_ERROR("[MongoServer]Failed to get collection: {}", collection_name);
        for (auto* idx : indexes) bson_destroy(idx);
        return false;
    }

    // 构建 index model 数组
    std::vector<mongoc_index_model_t*> models;
    for (auto* idx : indexes) {
        bson_iter_t iter;
        bson_t keys;
        bson_init(&keys);

        if (bson_iter_init_find(&iter, idx, "key")) {
            const uint8_t* key_data = nullptr;
            uint32_t key_len = 0;
            bson_iter_document(&iter, &key_len, &key_data);
            if (key_data && key_len > 0) {
                bson_init_static(&keys, key_data, key_len);
            }
        }

        bson_t opts;
        bson_init(&opts);
        if (bson_iter_init_find(&iter, idx, "unique")) {
            BSON_APPEND_BOOL(&opts, "unique", bson_iter_as_bool(&iter));
        }
        if (bson_iter_init_find(&iter, idx, "sparse")) {
            BSON_APPEND_BOOL(&opts, "sparse", bson_iter_as_bool(&iter));
        }
        if (bson_iter_init_find(&iter, idx, "expireAfterSeconds")) {
            BSON_APPEND_INT32(&opts, "expireAfterSeconds", bson_iter_int32(&iter));
        }

        mongoc_index_model_t* model = mongoc_index_model_new(&keys, &opts);
        models.push_back(model);

        bson_destroy(&opts);
        bson_destroy(&keys);
    }

    // 批量创建索引
    bson_error_t error;
    bson_t reply;
    bool ok = true;
    if (!models.empty()) {
        ok = mongoc_collection_create_indexes_with_opts(collection,
            models.data(), models.size(), nullptr, &reply, &error);
        bson_destroy(&reply);

        if (!ok) {
            SPDLOG_ERROR("[MongoServer]Failed to create indexes for {}: {}", collection_name, error.message);
        }
    }

    // 清理 models
    for (auto* m : models) {
        mongoc_index_model_destroy(m);
    }

    mongoc_collection_destroy(collection);

    for (auto* idx : indexes) {
        bson_destroy(idx);
    }

    return ok;
}

bool MongoServer::read_index_config(const std::string& collection_name, std::vector<bson_t*>& indexes) {
    std::string path = index_config_dir_ + "/" + collection_name + "_index.json";
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    nlohmann::json config;
    try {
        file >> config;
    } catch (const nlohmann::json::parse_error& e) {
        SPDLOG_ERROR("[MongoServer]Failed to parse index config {}: {}", path, e.what());
        return false;
    }

    if (!config.is_array()) {
        SPDLOG_ERROR("[MongoServer]Invalid index config format: {}", path);
        return false;
    }

    for (const auto& idx : config) {
        bson_t* index = bson_new();
        bson_t keys;
        bson_init(&keys);

        // 添加索引键
        std::string key = idx.value("key", "");
        if (key.empty()) {
            bson_destroy(index);
            bson_destroy(&keys);
            continue;
        }
        BSON_APPEND_INT32(&keys, key.c_str(), 1);

        // 构建索引文档
        BSON_APPEND_DOCUMENT(index, "key", &keys);
        bson_destroy(&keys);

        // 添加选项
        if (idx.contains("unique")) {
            BSON_APPEND_BOOL(index, "unique", idx["unique"].get<bool>());
        }
        if (idx.contains("sparse")) {
            BSON_APPEND_BOOL(index, "sparse", idx["sparse"].get<bool>());
        }
        if (idx.contains("expireAfterSeconds")) {
            BSON_APPEND_INT32(index, "expireAfterSeconds", idx["expireAfterSeconds"].get<int>());
        }

        indexes.push_back(index);
    }

    return !indexes.empty();
}

mongoc_collection_t* MongoServer::get_collection(const std::string& name) {
    // 从 URI 中提取数据库名
    std::string uri = "mongodb://localhost:27017/farm";  // 需要从配置获取
    mongoc_uri_t* mongoc_uri = mongoc_uri_new(uri.c_str());
    const char* db_name = mongoc_uri_get_database(mongoc_uri);

    mongoc_database_t* db = mongoc_client_get_database(conn_.client(), db_name);
    mongoc_collection_t* collection = mongoc_database_get_collection(db, name.c_str());

    mongoc_database_destroy(db);
    mongoc_uri_destroy(mongoc_uri);

    return collection;
}

DataResult MongoServer::get_all(uint64_t player_id, std::vector<uint8_t>& value) {
    mongoc_collection_t* collection = get_collection("players");
    if (!collection) return DataResult::IO_ERROR;

    bson_t filter;
    bson_init(&filter);
    BSON_APPEND_INT64(&filter, "player_id", static_cast<int64_t>(player_id));

    mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(collection, &filter, nullptr, nullptr);
    const bson_t* doc;
    bool found = false;

    if (mongoc_cursor_next(cursor, &doc)) {
        bson_iter_t iter;
        if (bson_iter_init_find(&iter, doc, "data")) {
            uint32_t len;
            const uint8_t* data;
            bson_iter_document(&iter, &len, &data);
            value.assign(data, data + len);
            found = true;
        }
    }

    mongoc_cursor_destroy(cursor);
    bson_destroy(&filter);
    mongoc_collection_destroy(collection);

    return found ? DataResult::SUCCESS : DataResult::KEY_NOT_FOUND;
}

DataResult MongoServer::get(uint64_t player_id, const std::string& key, std::vector<uint8_t>& value) {
    std::vector<uint8_t> all_data;
    DataResult result = get_all(player_id, all_data);
    if (result != DataResult::SUCCESS) return result;

    // 解析 JSON，提取指定 key
    try {
        std::string json_str(all_data.begin(), all_data.end());
        auto json = nlohmann::json::parse(json_str);
        if (json.contains(key)) {
            std::string val = json[key].dump();
            value.assign(val.begin(), val.end());
        } else {
            value.clear();
        }
    } catch (const nlohmann::json::exception& e) {
        SPDLOG_ERROR("[MongoServer]JSON parse error in get: {}", e.what());
        return DataResult::PARSE_ERROR;
    }

    return DataResult::SUCCESS;
}

DataResult MongoServer::set_all(uint64_t player_id, const std::vector<uint8_t>& value) {
    mongoc_collection_t* collection = get_collection("players");
    if (!collection) return DataResult::IO_ERROR;

    bson_t filter;
    bson_init(&filter);
    BSON_APPEND_INT64(&filter, "player_id", static_cast<int64_t>(player_id));

    // 解析 data 为 bson
    std::string data_str(value.begin(), value.end());
    bson_t* data_doc = bson_new_from_json(reinterpret_cast<const uint8_t*>(data_str.c_str()), -1, nullptr);
    if (!data_doc) {
        bson_destroy(&filter);
        mongoc_collection_destroy(collection);
        return DataResult::PARSE_ERROR;
    }

    bson_t update;
    bson_init(&update);
    bson_t set_doc;
    BSON_APPEND_DOCUMENT_BEGIN(&update, "$set", &set_doc);
    BSON_APPEND_INT64(&set_doc, "player_id", static_cast<int64_t>(player_id));
    BSON_APPEND_DOCUMENT(&set_doc, "data", data_doc);
    bson_append_document_end(&update, &set_doc);

    bson_error_t error;
    bson_t reply;
    bool ok = mongoc_collection_update_one(collection, &filter, &update, nullptr, &reply, &error);

    bson_destroy(&reply);
    bson_destroy(&update);
    bson_destroy(data_doc);
    bson_destroy(&filter);
    mongoc_collection_destroy(collection);

    if (!ok) {
        SPDLOG_ERROR("[MongoServer]set_all failed: {}", error.message);
        return DataResult::IO_ERROR;
    }

    return DataResult::SUCCESS;
}

DataResult MongoServer::set(uint64_t player_id, const std::string& key, const std::vector<uint8_t>& value) {
    mongoc_collection_t* collection = get_collection("players");
    if (!collection) return DataResult::IO_ERROR;

    bson_t filter;
    bson_init(&filter);
    BSON_APPEND_INT64(&filter, "player_id", static_cast<int64_t>(player_id));

    // 解析 value
    std::string val_str(value.begin(), value.end());
    bson_t* val_doc = bson_new_from_json(reinterpret_cast<const uint8_t*>(val_str.c_str()), -1, nullptr);
    if (!val_doc) {
        bson_destroy(&filter);
        mongoc_collection_destroy(collection);
        return DataResult::PARSE_ERROR;
    }

    // 构建 update: { $set: { "data.key": value } }
    std::string field_path = "data." + key;
    bson_t update;
    bson_init(&update);
    bson_t set_doc;
    BSON_APPEND_DOCUMENT_BEGIN(&update, "$set", &set_doc);
    BSON_APPEND_DOCUMENT(&set_doc, field_path.c_str(), val_doc);
    bson_append_document_end(&update, &set_doc);

    bson_error_t error;
    bson_t reply;
    bool ok = mongoc_collection_update_one(collection, &filter, &update, nullptr, &reply, &error);

    bson_destroy(&reply);
    bson_destroy(&update);
    bson_destroy(val_doc);
    bson_destroy(&filter);
    mongoc_collection_destroy(collection);

    if (!ok) {
        SPDLOG_ERROR("[MongoServer]set failed: {}", error.message);
        return DataResult::IO_ERROR;
    }

    return DataResult::SUCCESS;
}

DataResult MongoServer::del(uint64_t player_id, const std::string& key) {
    mongoc_collection_t* collection = get_collection("players");
    if (!collection) return DataResult::IO_ERROR;

    bson_t filter;
    bson_init(&filter);
    BSON_APPEND_INT64(&filter, "player_id", static_cast<int64_t>(player_id));

    // 构建 update: { $unset: { "data.key": "" } }
    std::string field_path = "data." + key;
    bson_t update;
    bson_init(&update);
    bson_t unset_doc;
    BSON_APPEND_DOCUMENT_BEGIN(&update, "$unset", &unset_doc);
    BSON_APPEND_UTF8(&unset_doc, field_path.c_str(), "");
    bson_append_document_end(&update, &unset_doc);

    bson_error_t error;
    bson_t reply;
    bool ok = mongoc_collection_update_one(collection, &filter, &update, nullptr, &reply, &error);

    bson_destroy(&reply);
    bson_destroy(&update);
    bson_destroy(&filter);
    mongoc_collection_destroy(collection);

    if (!ok) {
        SPDLOG_ERROR("[MongoServer]del failed: {}", error.message);
        return DataResult::IO_ERROR;
    }

    return DataResult::SUCCESS;
}

AccountResult MongoServer::get_account(const std::string& account_id, std::vector<AccountRole>& roles) {
    mongoc_collection_t* collection = get_collection("accounts");
    if (!collection) return AccountResult::IO_ERROR;

    bson_t filter;
    bson_init(&filter);
    BSON_APPEND_UTF8(&filter, "account_id", account_id.c_str());

    mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(collection, &filter, nullptr, nullptr);
    const bson_t* doc;
    roles.clear();

    if (mongoc_cursor_next(cursor, &doc)) {
        bson_iter_t iter;
        if (bson_iter_init_find(&iter, doc, "roles") && BSON_ITER_HOLDS_ARRAY(&iter)) {
            bson_iter_t array_iter;
            bson_iter_recurse(&iter, &array_iter);

            while (bson_iter_next(&array_iter)) {
                const uint8_t* role_data = nullptr;
                uint32_t role_data_len = 0;
                bson_iter_document(&array_iter, &role_data_len, &role_data);
                if (role_data && role_data_len > 0) {
                    bson_t role_doc;
                    bson_init_static(&role_doc, role_data, role_data_len);
                    AccountRole role;
                    bson_iter_t role_iter;
                    if (bson_iter_init_find(&role_iter, &role_doc, "server_id")) {
                        role.server_id = bson_iter_int32(&role_iter);
                    }
                    if (bson_iter_init_find(&role_iter, &role_doc, "player_id")) {
                        role.player_id = bson_iter_int64(&role_iter);
                    }
                    if (bson_iter_init_find(&role_iter, &role_doc, "role_name")) {
                        role.role_name = bson_iter_utf8(&role_iter, nullptr);
                    }
                    roles.push_back(role);
                }
            }
        }
    }

    mongoc_cursor_destroy(cursor);
    bson_destroy(&filter);
    mongoc_collection_destroy(collection);

    return AccountResult::SUCCESS;
}

AccountResult MongoServer::set_account(const std::string& account_id, const AccountRole& new_role) {
    mongoc_collection_t* collection = get_collection("accounts");
    if (!collection) return AccountResult::IO_ERROR;

    // 先检查角色是否已存在
    std::vector<AccountRole> existing_roles;
    AccountResult get_result = get_account(account_id, existing_roles);
    if (get_result != AccountResult::SUCCESS) {
        mongoc_collection_destroy(collection);
        return get_result;
    }

    for (const auto& role : existing_roles) {
        if (role.server_id == new_role.server_id) {
            mongoc_collection_destroy(collection);
            return AccountResult::ROLE_ALREADY_EXISTS;
        }
    }

    // 添加新角色
    bson_t filter;
    bson_init(&filter);
    BSON_APPEND_UTF8(&filter, "account_id", account_id.c_str());

    bson_t role_doc;
    bson_init(&role_doc);
    BSON_APPEND_INT32(&role_doc, "server_id", static_cast<int32_t>(new_role.server_id));
    BSON_APPEND_INT64(&role_doc, "player_id", static_cast<int64_t>(new_role.player_id));
    BSON_APPEND_UTF8(&role_doc, "role_name", new_role.role_name.c_str());

    bson_t update;
    bson_init(&update);
    bson_t push_doc;
    BSON_APPEND_DOCUMENT_BEGIN(&update, "$push", &push_doc);
    BSON_APPEND_DOCUMENT(&push_doc, "roles", &role_doc);
    bson_append_document_end(&update, &push_doc);

    bson_error_t error;
    bson_t reply;
    bool ok = mongoc_collection_update_one(collection, &filter, &update, nullptr, &reply, &error);

    bson_destroy(&reply);
    bson_destroy(&update);
    bson_destroy(&role_doc);
    bson_destroy(&filter);
    mongoc_collection_destroy(collection);

    if (!ok) {
        SPDLOG_ERROR("[MongoServer]set_account failed: {}", error.message);
        return AccountResult::IO_ERROR;
    }

    return AccountResult::SUCCESS;
}

}  // namespace farm
