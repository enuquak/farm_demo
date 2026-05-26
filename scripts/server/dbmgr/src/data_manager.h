#pragma once

#include <string>
#include <cstdint>
#include <vector>

namespace farm {

// 数据操作结果码
enum class DataResult : int32_t {
    SUCCESS = 0,
    FILE_NOT_FOUND = 1,    // 玩家数据文件不存在
    KEY_NOT_FOUND = 2,     // 指定 key 不存在
    PARSE_ERROR = 3,       // JSON 解析错误
    IO_ERROR = 4,          // 文件读写错误
};

// 账号角色信息
struct AccountRole {
    uint32_t server_id;
    uint64_t player_id;
    std::string role_name;
};

// 账号数据操作结果码
enum class AccountResult : int32_t {
    SUCCESS = 0,
    ROLE_ALREADY_EXISTS = 1,  // 角色已存在
    IO_ERROR = 2,             // 文件读写错误
    PARSE_ERROR = 3,          // JSON 解析错误
};

class DataManager {
public:
    DataManager(const std::string& data_dir, uint32_t dbmgr_count = 1);
    ~DataManager();

    // 初始化数据目录（创建 players/ 和 accounts/ 目录）
    bool init();

    // GET_ALL: 读取玩家全部数据
    DataResult get_all(uint64_t player_id, std::vector<uint8_t>& value);

    // GET: 读取玩家指定 key
    DataResult get(uint64_t player_id, const std::string& key, std::vector<uint8_t>& value);

    // SET_ALL: 写入玩家全部数据
    DataResult set_all(uint64_t player_id, const std::vector<uint8_t>& value);

    // SET: 写入玩家指定 key
    DataResult set(uint64_t player_id, const std::string& key, const std::vector<uint8_t>& value);

    // DEL: 删除玩家指定 key
    DataResult del(uint64_t player_id, const std::string& key);

    // 账号数据操作
    // GET_ACCOUNT: 读取账号角色列表
    AccountResult get_account(const std::string& account_id, std::vector<AccountRole>& roles);

    // SET_ACCOUNT: 添加账号角色
    AccountResult set_account(const std::string& account_id, const AccountRole& new_role);

private:
    // 获取玩家 JSON 文件路径
    std::string get_player_file_path(uint64_t player_id) const;

    // 获取账号 JSON 文件路径
    std::string get_account_file_path(const std::string& account_id) const;

    // 计算账号哈希值
    uint32_t hash_account_id(const std::string& account_id) const;

    // 读取文件内容
    bool read_file(const std::string& path, std::string& content);

    // 写入文件内容
    bool write_file(const std::string& path, const std::string& content);

    // JSON 操作辅助函数
    // 从 JSON 字符串中提取指定 key 的值
    bool json_get(const std::string& json, const std::string& key, std::string& value);

    // 设置 JSON 字符串中指定 key 的值
    bool json_set(const std::string& json, const std::string& key, const std::string& value, std::string& result);

    // 删除 JSON 字符串中指定 key
    bool json_del(const std::string& json, const std::string& key, std::string& result);

    // 查找 JSON 中 key 的位置（返回 key 的起始位置，包含引号）
    size_t json_find_key(const std::string& json, const std::string& key);

    // 跳过 JSON 值（从值的起始位置返回值的结束位置）
    size_t json_skip_value(const std::string& json, size_t pos);

    // JSON 数组操作辅助函数
    bool json_array_contains_server_id(const std::string& json_array, uint32_t server_id);
    bool json_array_append(const std::string& json_array, const std::string& new_item, std::string& result);

    std::string data_dir_;
    std::string players_dir_;
    std::string accounts_dir_;
    uint32_t dbmgr_count_;
};

}  // namespace farm
