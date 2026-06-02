#pragma once

#include "mongo_connection.h"
#include "data_manager.h"  // 复用 DataResult、AccountResult、AccountRole 定义

#include <string>
#include <vector>
#include <cstdint>

namespace farm {

class MongoServer {
public:
    MongoServer(MongoConnection& conn, const std::string& index_config_dir);

    // 初始化：读取索引配置，创建缺失索引
    bool init();

    // 玩家数据操作
    DataResult get_all(uint64_t player_id, std::vector<uint8_t>& value);
    DataResult get(uint64_t player_id, const std::string& key, std::vector<uint8_t>& value);
    DataResult set_all(uint64_t player_id, const std::vector<uint8_t>& value);
    DataResult set(uint64_t player_id, const std::string& key, const std::vector<uint8_t>& value);
    DataResult del(uint64_t player_id, const std::string& key);

    // 账号数据操作
    AccountResult get_account(const std::string& account_id, std::vector<AccountRole>& roles);
    AccountResult set_account(const std::string& account_id, const AccountRole& new_role);

    // 初始化 counters 集合（确保 player_id 计数器文档存在）
    bool init_counter();

    // 原子分配 N 个 player_id，返回起始 ID
    // 成功: 返回 start_id (>0), 失败: 返回 0
    int64_t alloc_player_ids(uint32_t count);

private:
    // 创建集合索引
    bool create_indexes(const std::string& collection_name);

    // 读取索引配置文件
    bool read_index_config(const std::string& collection_name, std::vector<bson_t*>& indexes);

    // 获取集合
    mongoc_collection_t* get_collection(const std::string& name);

    MongoConnection& conn_;
    std::string index_config_dir_;
};

}  // namespace farm
