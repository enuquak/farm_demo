#pragma once

#include <string>
#include <mongoc/mongoc.h>

namespace farm {

class MongoConnection {
public:
    MongoConnection();
    ~MongoConnection();

    // 禁止拷贝
    MongoConnection(const MongoConnection&) = delete;
    MongoConnection& operator=(const MongoConnection&) = delete;

    // 连接 MongoDB
    bool connect(const std::string& uri);

    // 断开连接
    void disconnect();

    // 是否已连接
    bool is_connected() const;

    // 获取底层 client（供 MongoServer 使用）
    mongoc_client_t* client();

private:
    mongoc_client_t* client_ = nullptr;
    bool connected_ = false;
};

}  // namespace farm
