---
name: cpp-ai-coding-conventions
description: C++ 通用 AI 编码规范 + 项目设计模式：格式、命名、错误处理、日志、内存管理、Stub 模式、回调注入、ConnectionManager 等。
license: MIT
metadata:
  author: common
  version: "1.0"
---

# C++ AI 编码规范

> 本文档是 C++ 项目的**通用开发指导规范**。
> 适用于任何 C++ 项目的基础编码、代码风格和工程实践。

---

## 一、编码格式

- 换行符统一使用 **LF**（`\n`），**禁止使用 CRLF**（`\r\n`）
- 缩进统一使用 **4 空格**，**禁止使用 Tab**
- 每行最大宽度建议 **120 字符**，超过时合理换行
- 文件末尾保留一个空行
- 运算符两侧加空格，逗号后加空格：

```cpp
// ✅ 正确
int result = a + b;
func(arg1, arg2, arg3);

// ❌ 错误
int result=a+b;
func(arg1,arg2,arg3);
```

---

## 二、命名规范

### 2.1 通用规则

- 使用有意义的英文命名，**禁止**拼音或无意义缩写
- 常量全大写下划线分隔：`MAX_BUFFER_SIZE`
- 避免单字母变量（循环计数器 `i`, `j`, `k` 除外）

### 2.2 各类型命名约定

| 类型 | 风格 | 示例 |
|------|------|------|
| 命名空间 | 全小写下划线 | `network_core` |
| 类/结构体 | PascalCase | `TcpConnection` |
| 函数/方法 | snake_case 或 camelCase | `send_data()` / `sendData()` |
| 成员变量 | snake_case 带前缀 | `m_buffer` 或 `buffer_` |
| 局部变量 | snake_case | `packet_size` |
| 常量/宏 | 全大写下划线 | `MAX_RETRY_COUNT` |
| 枚举值 | 全大写下划线 | `STATE_CONNECTED` |
| 文件名 | snake_case | `tcp_server.cpp` |

> **项目内保持一致**：同一项目中选择一种风格后全局统一，不要混用。

---

## 三、头文件规范

### 3.1 Include Guard

使用 `#pragma once` 或传统 include guard，**二选一全局统一**：

```cpp
// 方式一（推荐）
#pragma once

// 方式二
#ifndef MODULE_FILENAME_H
#define MODULE_FILENAME_H
// ...
#endif  // MODULE_FILENAME_H
```

### 3.2 Include 顺序

按以下顺序分组，组间空行分隔：

```cpp
// 1. 对应头文件（.cpp 文件中）
#include "my_class.h"

// 2. C 标准库
#include <cstring>
#include <cstdint>

// 3. C++ 标准库
#include <string>
#include <vector>
#include <memory>

// 4. 第三方库
#include <event2/event.h>
#include <google/protobuf/message.h>

// 5. 项目内其他头文件
#include "common/logger.h"
#include "network/buffer.h"
```

### 3.3 前向声明

- 优先使用前向声明减少编译依赖
- 头文件中能用前向声明就不用 `#include`
- 在 `.cpp` 文件中再 `#include` 实际头文件

```cpp
// my_class.h
class TcpServer;  // 前向声明

class MyClass {
    TcpServer* m_server;  // 指针/引用可用前向声明
};
```

---

## 四、代码风格

### 4.1 Early Return

条件检查失败时应立即 return，**禁止**将后续逻辑放在 `else` 中：

```cpp
// ✅ 正确：early return，减少嵌套
bool process_packet(const Packet& pkt) {
    if (!validate_header(pkt)) {
        log_error("invalid header");
        return false;
    }

    if (!check_auth(pkt)) {
        log_error("auth failed");
        return false;
    }

    // 正常逻辑在顶层
    do_business(pkt);
    return true;
}

// ❌ 错误：不必要的 else 增加嵌套
bool process_packet(const Packet& pkt) {
    if (!validate_header(pkt)) {
        log_error("invalid header");
        return false;
    } else {
        if (!check_auth(pkt)) {
            log_error("auth failed");
            return false;
        } else {
            do_business(pkt);
            return true;
        }
    }
}
```

### 4.2 花括号风格

同一项目内统一使用一种风格，推荐 Allman 风格（独占一行）：

```cpp
// ✅ Allman 风格（推荐）
if (condition)
{
    do_something();
}
else
{
    do_other();
}

// 也可用 K&R 风格（项目内统一即可）
if (condition) {
    do_something();
} else {
    do_other();
}
```

### 4.3 switch 语句

- 每个 `case` 必须有 `break` 或 `return`，fall-through 需显式注释
- `default` 分支必须存在

```cpp
switch (state) {
    case STATE_IDLE:
        handle_idle();
        break;
    case STATE_CONNECTED:
        handle_connected();
        break;
    case STATE_ERROR:
        // fall-through intentional
    case STATE_TIMEOUT:
        handle_error();
        break;
    default:
        log_warning("unknown state");
        break;
}
```

---

## 五、错误处理

### 5.1 错误码

- 使用枚举类定义错误码，避免裸整数
- 错误码命名清晰，附带注释说明

```cpp
enum class ErrorCode : int32_t {
    SUCCESS         = 0,
    INVALID_PARAM   = 1,    // 参数不合法
    NETWORK_ERROR   = 2,    // 网络异常
    TIMEOUT         = 3,    // 超时
    AUTH_FAILED     = 4,    // 认证失败
};
```

### 5.2 返回值约定

- 同一项目统一使用一种错误传递方式（错误码 / 异常 / std::expected）
- 返回错误码时，输出参数通过指针或引用传递
- 关键函数检查返回值，**禁止**忽略错误

```cpp
// 方式一：错误码 + 输出参数
ErrorCode connect(const std::string& host, int port, Connection** out_conn);

// 方式二：bool 返回 + 日志
bool send_data(const void* data, size_t len) {
    if (!data || len == 0) {
        log_error("send_data, invalid params");
        return false;
    }
    // ...
}
```

### 5.3 异常使用

- 如果使用异常，明确哪些函数会抛异常
- 构造函数、资源获取用 RAII，避免资源泄漏
- **禁止**在析构函数中抛异常

---

## 六、内存管理

### 6.1 智能指针

- 优先使用智能指针，**禁止**裸 `new`/`delete`
- 独占所有权用 `std::unique_ptr`
- 共享所有权用 `std::shared_ptr`（谨慎使用，确认真正需要共享）
- 观察/弱引用用 `std::weak_ptr`

```cpp
// ✅ 正确
auto server = std::make_unique<TcpServer>(port);
auto handler = std::make_shared<PacketHandler>();

// ❌ 错误：裸指针管理
TcpServer* server = new TcpServer(port);
// ... 容易忘记 delete
```

### 6.2 RAII

- 文件句柄、锁、网络连接等资源必须用 RAII 封装
- 禁止手动 `open`/`close`、`lock`/`unlock` 配对

```cpp
// ✅ 正确：RAII 锁
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.push_back(item);
}  // 自动解锁

// ❌ 错误：手动锁
m_mutex.lock();
m_data.push_back(item);
m_mutex.unlock();  // 如果中间抛异常则死锁
```

---

## 七、日志规范

### 7.1 格式约定

- 日志消息以**方法名或模块名**开头，便于检索
- 使用格式化字符串，清晰打印关键变量

```cpp
// 推荐格式：方法名, 描述, key=value
LOG_INFO("on_connect, new client connected, fd=%d, addr=%s", fd, addr_str);
LOG_ERROR("send_data, send failed, fd=%d, errno=%d", fd, errno);
LOG_WARNING("check_timeout, connection slow, fd=%d, latency_ms=%d", fd, latency);
```

### 7.2 日志级别

| 级别 | 用途 |
|------|------|
| `DEBUG` | 开发调试信息，发布版本可关闭 |
| `INFO` | 正常业务流程的关键节点 |
| `WARNING` | 异常但可恢复的情况 |
| `ERROR` | 不应发生的错误，需要关注 |

### 7.3 注意事项

- 循环内高频日志用 `DEBUG` 级别或加频率限制
- 错误日志必须包含足够的上下文信息（fd、地址、错误码等）
- **禁止**在日志中打印敏感信息（密码、密钥、token）

---

## 八、const 正确性

- 能用 `const` 就用 `const`
- 函数参数只读时加 `const`
- 不修改成员的函数标记为 `const`
- 优先使用 `constexpr` 计算编译期常量

```cpp
class Connection {
public:
    // ✅ 正确：const 参数、const 成员函数
    bool send(const void* data, size_t len) const;
    int fd() const { return m_fd; }

private:
    int m_fd;
    std::string m_addr;
};
```

---

## 九、注释规范

### 9.1 文件头

```cpp
/**
 * @file tcp_server.h
 * @brief TCP 服务器实现
 */
```

### 9.2 函数注释

```cpp
/**
 * @brief 发送数据到指定连接
 * @param fd    目标连接的文件描述符
 * @param data  数据指针
 * @param len   数据长度（字节）
 * @return 成功返回 true，失败返回 false
 */
bool send_data(int fd, const void* data, size_t len);
```

### 9.3 注释原则

- 解释 **为什么**，而不是 **做了什么**（代码本身说明做了什么）
- 复杂逻辑、算法、workaround 必须加注释
- **禁止**注释掉的代码保留在提交中，直接删除（git 可追溯）

---

## 十、头文件与源文件组织

### 10.1 一个类一个文件（推荐）

- 头文件：`ClassName.h`（声明）
- 源文件：`ClassName.cpp`（实现）
- 文件名与类名一致

### 10.2 模块目录结构

```
module_name/
├── include/           # 公共头文件
│   └── module_name/
│       ├── class_a.h
│       └── class_b.h
├── src/               # 实现文件
│   ├── class_a.cpp
│   └── class_b.cpp
└── CMakeLists.txt     # 构建配置
```

---

## 十一、编译与构建

### 11.1 项目构建脚本

本项目使用 `tool/build_cpp14.bat` 进行 C++ 编译，基于 VS2022 + MSVC 的 C++14 标准。

**调用方式**：
```bash
cmd /c "D:\mb_workspace\farm_demo\tool\build_cpp14.bat" "D:\mb_workspace\farm_demo\scripts\server\<服务目录>"
```

**脚本行为**：
- 接收一个目录路径参数（C++ 服务的根目录）
- 调用 VS2022 BuildTools 的 `vcvars64.bat` 初始化编译环境
- 在目标目录下执行 `cl /std:c++14 src\main.cpp`
- 编译成功生成 `main.exe`，失败则输出错误信息

**约束**：
- 目标目录下必须存在 `src\main.cpp` 入口文件
- Windows 环境依赖 VS2022 BuildTools（路径已硬编码在脚本中）

### 11.2 通用构建规范

- 开启编译警告：`-Wall -Wextra -Werror`
- Debug 和 Release 配置分离
- 依赖管理使用包管理器（vcpkg/conan）或 git submodule

---

## 十二、功能开发 Checklist

新增功能时，按以下清单逐项检查：

- [ ] 头文件有 include guard（`#pragma once` 或 `#ifndef`）
- [ ] 命名符合项目统一规范
- [ ] 能用 `const` 的地方都用了 `const`
- [ ] 内存管理使用智能指针，无裸 `new`/`delete`
- [ ] 错误路径有日志且包含足够上下文
- [ ] 关键函数有注释说明参数和返回值
- [ ] 无编译警告（`-Wall -Wextra`）
- [ ] 资源获取使用 RAII

---

## 十三、项目通用设计模式

> 以下模式是本项目的核心架构约定，所有 C++ 开发都必须遵循。

### 13.1 Stub 模式

Stub 是 Game Server 内部的单点业务组件，每个 Stub 持有自己的依赖，通过回调注入与其他模块通信。

**已有 Stub：**
- `LoginStub` — 登录流程管理，依赖 RedisConnection、DBMgrConnectionManager
- `OnlineStub` — 在线状态查询，依赖 RedisConnection
- `GMStub` — GM 命令处理，依赖 DBMgrConnectionManager

**创建新 Stub 的模板：**
```cpp
// my_stub.h
class MyStub {
public:
    MyStub(RedisConnection* redis, DBMgrConnectionManager* dbmgr);
    void init();

private:
    RedisConnection* m_redis;
    DBMgrConnectionManager* m_dbmgr;
};

// game_server.cpp 构造函数中
m_my_stub = std::make_unique<MyStub>(m_redis.get(), m_dbmgr_conn_mgr.get());
```

### 13.2 回调注入

模块间通信使用 `std::function` 回调，不直接引用其他模块。

**典型签名：**
```cpp
using SendToGateFunc = std::function<void(uint32_t player_id, const std::string& data)>;
using AllocPlayerIdCallback = std::function<void(uint64_t player_id)>;
using OnlineQueryCallback = std::function<void(bool is_online)>;
```

**注入方式：** 构造函数或 setter 方法注入，运行时通过回调调用。

### 13.3 ConnectionManager 状态机

统一的数据库连接管理模式：

```
DISCONNECTED → CONNECTING → CONNECTED
                          → FAILED → FAILED_PERMANENT
```

- 后台重试线程在 FAILED 状态自动重连
- ready 回调通知上层连接就绪
- 应用于 MongoDB 和 Redis 连接

### 13.4 异步请求-响应

DBMgr 通信使用 request_id 匹配：

```cpp
// 发送时
uint64_t request_id = generate_request_id();
m_pending_requests[request_id] = callback;
send_to_dbmgr(request_id, data);

// 响应回来时
auto it = m_pending_requests.find(response.request_id());
if (it != m_pending_requests.end()) {
    it->second(response);
    m_pending_requests.erase(it);
}
```

单线程事件循环中不阻塞等待。

### 13.5 脏字段追踪

Player 实体使用双重标记：

```cpp
bool dirty_ = false;
std::unordered_set<std::string> dirty_fields_;

void set_gold(int gold) {
    m_gold = gold;
    dirty_ = true;
    dirty_fields_.insert("gold");
}
```

三种保存路径：
- `save_full()` — SET_ALL，定时器/断线时使用
- `save()` — 逐字段 SET，业务刷新时使用
- `save_field("gold")` — 单字段 SET，精确保存

脏标记在保存失败时不清除，下次定时器重试。

### 13.6 实体自治

Player 持有 `DBMgrConnectionManager*` 直接引用，自主管理保存定时器（libevent timer，5 分钟间隔），不依赖 PlayerManager 代为保存。

### 13.7 数据路由

```cpp
// 玩家数据路由
int target_dbmgr = player_id % dbmgr_count;

// 账户数据路由
int target_dbmgr = std::hash<std::string>{}(account_id) % dbmgr_count;
```
