#include "data_manager.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/types.h>
#include <unistd.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

namespace farm {

DataManager::DataManager(const std::string& data_dir, uint32_t dbmgr_count)
    : data_dir_(data_dir)
    , players_dir_(data_dir + "/players")
    , accounts_dir_(data_dir + "/accounts")
    , dbmgr_count_(dbmgr_count)
{
}

DataManager::~DataManager() {
}

bool DataManager::init() {
    // 创建 players 目录
    struct stat st;
    if (stat(players_dir_.c_str(), &st) != 0) {
        if (MKDIR(players_dir_.c_str()) != 0) {
            std::cerr << "[DataManager] Failed to create players directory: " << players_dir_ << std::endl;
            return false;
        }
        std::cout << "[DataManager] Created players directory: " << players_dir_ << std::endl;
    }

    // 创建 accounts 目录
    if (stat(accounts_dir_.c_str(), &st) != 0) {
        if (MKDIR(accounts_dir_.c_str()) != 0) {
            std::cerr << "[DataManager] Failed to create accounts directory: " << accounts_dir_ << std::endl;
            return false;
        }
        std::cout << "[DataManager] Created accounts directory: " << accounts_dir_ << std::endl;
    }

    return true;
}

std::string DataManager::get_player_file_path(uint64_t player_id) const {
    return players_dir_ + "/" + std::to_string(player_id) + ".json";
}

bool DataManager::read_file(const std::string& path, std::string& content) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    content = ss.str();
    return true;
}

bool DataManager::write_file(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    file << content;
    return true;
}

DataResult DataManager::get_all(uint64_t player_id, std::vector<uint8_t>& value) {
    std::string path = get_player_file_path(player_id);
    std::string content;

    if (!read_file(path, content)) {
        // 文件不存在，返回空值（非错误）
        value.clear();
        return DataResult::SUCCESS;
    }

    value.assign(content.begin(), content.end());
    return DataResult::SUCCESS;
}

DataResult DataManager::get(uint64_t player_id, const std::string& key, std::vector<uint8_t>& value) {
    std::string path = get_player_file_path(player_id);
    std::string content;

    if (!read_file(path, content)) {
        // 文件不存在，返回空值
        value.clear();
        return DataResult::SUCCESS;
    }

    std::string json_value;
    if (!json_get(content, key, json_value)) {
        // key 不存在，返回空值
        value.clear();
        return DataResult::SUCCESS;
    }

    value.assign(json_value.begin(), json_value.end());
    return DataResult::SUCCESS;
}

DataResult DataManager::set_all(uint64_t player_id, const std::vector<uint8_t>& value) {
    std::string path = get_player_file_path(player_id);
    std::string content(value.begin(), value.end());

    if (!write_file(path, content)) {
        std::cerr << "[DataManager] Failed to write file: " << path << std::endl;
        return DataResult::IO_ERROR;
    }

    return DataResult::SUCCESS;
}

DataResult DataManager::set(uint64_t player_id, const std::string& key, const std::vector<uint8_t>& value) {
    std::string path = get_player_file_path(player_id);
    std::string content;
    std::string json_value(value.begin(), value.end());
    std::string result;

    if (!read_file(path, content)) {
        // 文件不存在，创建新文件
        content = "{}";
    }

    if (!json_set(content, key, json_value, result)) {
        std::cerr << "[DataManager] Failed to set key: " << key << std::endl;
        return DataResult::PARSE_ERROR;
    }

    if (!write_file(path, result)) {
        std::cerr << "[DataManager] Failed to write file: " << path << std::endl;
        return DataResult::IO_ERROR;
    }

    return DataResult::SUCCESS;
}

DataResult DataManager::del(uint64_t player_id, const std::string& key) {
    std::string path = get_player_file_path(player_id);
    std::string content;
    std::string result;

    if (!read_file(path, content)) {
        // 文件不存在，直接返回成功（幂等操作）
        return DataResult::SUCCESS;
    }

    if (!json_del(content, key, result)) {
        // key 不存在，返回成功（幂等操作）
        return DataResult::SUCCESS;
    }

    if (!write_file(path, result)) {
        std::cerr << "[DataManager] Failed to write file: " << path << std::endl;
        return DataResult::IO_ERROR;
    }

    return DataResult::SUCCESS;
}

std::string DataManager::get_account_file_path(const std::string& account_id) const {
    uint32_t hash = hash_account_id(account_id);
    return accounts_dir_ + "/" + std::to_string(hash) + ".json";
}

uint32_t DataManager::hash_account_id(const std::string& account_id) const {
    // 简单的哈希算法（djb2）
    uint32_t hash = 5381;
    for (char c : account_id) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % dbmgr_count_;
}

AccountResult DataManager::get_account(const std::string& account_id, std::vector<AccountRole>& roles) {
    std::string path = get_account_file_path(account_id);
    std::string content;

    if (!read_file(path, content)) {
        // 文件不存在，返回空角色列表（新账号）
        roles.clear();
        return AccountResult::SUCCESS;
    }

    // 解析 JSON 获取 roles 数组
    std::string roles_json;
    if (!json_get(content, "roles", roles_json)) {
        // roles 字段不存在，返回空角色列表
        roles.clear();
        return AccountResult::SUCCESS;
    }

    // 解析 roles 数组
    roles.clear();
    size_t pos = 0;
    while (pos < roles_json.length()) {
        // 查找角色对象开始
        size_t obj_start = roles_json.find('{', pos);
        if (obj_start == std::string::npos) break;

        // 查找角色对象结束
        size_t obj_end = roles_json.find('}', obj_start);
        if (obj_end == std::string::npos) break;

        std::string role_json = roles_json.substr(obj_start, obj_end - obj_start + 1);

        // 解析角色字段
        AccountRole role;
        std::string value;

        if (json_get(role_json, "server_id", value)) {
            role.server_id = std::stoul(value);
        }
        if (json_get(role_json, "player_id", value)) {
            role.player_id = std::stoull(value);
        }
        if (json_get(role_json, "role_name", value)) {
            // 去掉引号
            if (value.length() >= 2 && value.front() == '"' && value.back() == '"') {
                role.role_name = value.substr(1, value.length() - 2);
            } else {
                role.role_name = value;
            }
        }

        roles.push_back(role);
        pos = obj_end + 1;
    }

    return AccountResult::SUCCESS;
}

AccountResult DataManager::set_account(const std::string& account_id, const AccountRole& new_role) {
    std::string path = get_account_file_path(account_id);
    std::string content;

    if (!read_file(path, content)) {
        // 文件不存在，创建新账号文件
        content = "{\"account_id\":\"" + account_id + "\",\"roles\":[]}";
    }

    // 检查角色是否已存在（相同 server_id）
    std::string roles_json;
    if (json_get(content, "roles", roles_json)) {
        if (json_array_contains_server_id(roles_json, new_role.server_id)) {
            return AccountResult::ROLE_ALREADY_EXISTS;
        }
    }

    // 构建新角色 JSON
    std::string new_role_json = "{\"server_id\":" + std::to_string(new_role.server_id) +
                                ",\"player_id\":" + std::to_string(new_role.player_id) +
                                ",\"role_name\":\"" + new_role.role_name + "\"}";

    // 添加到 roles 数组
    std::string new_roles_json;
    if (!json_array_append(roles_json, new_role_json, new_roles_json)) {
        std::cerr << "[DataManager] Failed to append role to array" << std::endl;
        return AccountResult::PARSE_ERROR;
    }

    // 更新 roles 字段
    std::string result;
    if (!json_set(content, "roles", new_roles_json, result)) {
        std::cerr << "[DataManager] Failed to set roles in JSON" << std::endl;
        return AccountResult::PARSE_ERROR;
    }

    // 写入文件
    if (!write_file(path, result)) {
        std::cerr << "[DataManager] Failed to write account file: " << path << std::endl;
        return AccountResult::IO_ERROR;
    }

    return AccountResult::SUCCESS;
}

// JSON 辅助函数实现

size_t DataManager::json_find_key(const std::string& json, const std::string& key) {
    // 查找 "key" 模式
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) {
        return std::string::npos;
    }

    // 确保 key 后面跟着 ':'
    size_t colon_pos = json.find(':', pos + search.length());
    if (colon_pos == std::string::npos) {
        return std::string::npos;
    }

    return pos;
}

size_t DataManager::json_skip_value(const std::string& json, size_t pos) {
    // 跳过空白
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) {
        pos++;
    }

    if (pos >= json.length()) {
        return pos;
    }

    if (json[pos] == '"') {
        // 字符串值，找到结束引号
        pos++;
        while (pos < json.length()) {
            if (json[pos] == '\\') {
                pos += 2;  // 跳过转义字符
            } else if (json[pos] == '"') {
                return pos + 1;
            } else {
                pos++;
            }
        }
    } else if (json[pos] == '{') {
        // 对象，找到匹配的 }
        int depth = 1;
        pos++;
        while (pos < json.length() && depth > 0) {
            if (json[pos] == '{') depth++;
            else if (json[pos] == '}') depth--;
            pos++;
        }
        return pos;
    } else if (json[pos] == '[') {
        // 数组，找到匹配的 ]
        int depth = 1;
        pos++;
        while (pos < json.length() && depth > 0) {
            if (json[pos] == '[') depth++;
            else if (json[pos] == ']') depth--;
            pos++;
        }
        return pos;
    } else {
        // 数字、布尔、null，找到下一个分隔符
        while (pos < json.length() && json[pos] != ',' && json[pos] != '}' && json[pos] != ']') {
            pos++;
        }
        return pos;
    }

    return pos;
}

bool DataManager::json_get(const std::string& json, const std::string& key, std::string& value) {
    size_t key_pos = json_find_key(json, key);
    if (key_pos == std::string::npos) {
        return false;
    }

    // 找到 value 的起始位置（跳过 key 和冒号）
    std::string search = "\"" + key + "\"";
    size_t value_start = json.find(':', key_pos + search.length());
    if (value_start == std::string::npos) {
        return false;
    }
    value_start++;

    // 跳过空白
    while (value_start < json.length() && (json[value_start] == ' ' || json[value_start] == '\t')) {
        value_start++;
    }

    // 找到 value 的结束位置
    size_t value_end = json_skip_value(json, value_start);

    value = json.substr(value_start, value_end - value_start);
    return true;
}

bool DataManager::json_set(const std::string& json, const std::string& key, const std::string& value, std::string& result) {
    size_t key_pos = json_find_key(json, key);

    if (key_pos == std::string::npos) {
        // key 不存在，添加新 key-value
        // 找到最后一个 } 的位置
        size_t last_brace = json.rfind('}');
        if (last_brace == std::string::npos) {
            return false;
        }

        // 检查是否需要添加逗号
        std::string prefix = json.substr(0, last_brace);
        std::string suffix = json.substr(last_brace);

        // 去掉前缀末尾的空白
        while (!prefix.empty() && (prefix.back() == ' ' || prefix.back() == '\n' || prefix.back() == '\r')) {
            prefix.pop_back();
        }

        // 如果不是空对象，添加逗号
        if (!prefix.empty() && prefix.back() != '{') {
            prefix += ",";
        }

        result = prefix + "\n  \"" + key + "\": " + value + "\n" + suffix;
        return true;
    }

    // key 存在，替换 value
    std::string search = "\"" + key + "\"";
    size_t value_start = json.find(':', key_pos + search.length());
    if (value_start == std::string::npos) {
        return false;
    }
    value_start++;

    // 跳过空白
    while (value_start < json.length() && (json[value_start] == ' ' || json[value_start] == '\t')) {
        value_start++;
    }

    size_t value_end = json_skip_value(json, value_start);

    result = json.substr(0, value_start) + value + json.substr(value_end);
    return true;
}

bool DataManager::json_del(const std::string& json, const std::string& key, std::string& result) {
    size_t key_pos = json_find_key(json, key);
    if (key_pos == std::string::npos) {
        return false;
    }

    // 找到 key-value 对的起始和结束位置
    std::string search = "\"" + key + "\"";
    size_t value_start = json.find(':', key_pos + search.length());
    if (value_start == std::string::npos) {
        return false;
    }
    value_start++;

    // 跳过空白
    while (value_start < json.length() && (json[value_start] == ' ' || json[value_start] == '\t')) {
        value_start++;
    }

    size_t value_end = json_skip_value(json, value_start);

    // 向前跳过逗号和空白
    size_t pair_start = key_pos;
    while (pair_start > 0 && (json[pair_start - 1] == ' ' || json[pair_start - 1] == '\t' || json[pair_start - 1] == '\n')) {
        pair_start--;
    }
    if (pair_start > 0 && json[pair_start - 1] == ',') {
        pair_start--;
    }

    // 向后跳过逗号和空白
    size_t pair_end = value_end;
    while (pair_end < json.length() && (json[pair_end] == ' ' || json[pair_end] == '\t' || json[pair_end] == '\n' || json[pair_end] == '\r')) {
        pair_end++;
    }
    if (pair_end < json.length() && json[pair_end] == ',') {
        pair_end++;
    }

    result = json.substr(0, pair_start) + json.substr(pair_end);
    return true;
}

bool DataManager::json_array_contains_server_id(const std::string& json_array, uint32_t server_id) {
    // 查找 server_id 字段
    std::string search = "\"server_id\":" + std::to_string(server_id);
    return json_array.find(search) != std::string::npos;
}

bool DataManager::json_array_append(const std::string& json_array, const std::string& new_item, std::string& result) {
    // 去掉末尾的 ]
    size_t last_bracket = json_array.rfind(']');
    if (last_bracket == std::string::npos) {
        return false;
    }

    std::string prefix = json_array.substr(0, last_bracket);
    std::string suffix = json_array.substr(last_bracket);

    // 去掉前缀末尾的空白
    while (!prefix.empty() && (prefix.back() == ' ' || prefix.back() == '\n' || prefix.back() == '\r')) {
        prefix.pop_back();
    }

    // 如果不是空数组，添加逗号
    if (!prefix.empty() && prefix.back() != '[') {
        prefix += ",";
    }

    result = prefix + new_item + suffix;
    return true;
}

}  // namespace farm
