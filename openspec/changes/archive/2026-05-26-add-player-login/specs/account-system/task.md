# Account System - Task List

## 1. Proto 定义
- [已完成 ✅] 1.1 创建 account.proto: AccountDataReq/Resp, AccountSetReq/Resp 消息定义
- [已完成 ✅] 1.2 更新 internal.proto: 新增 AccountMsg 消息类型（Gate<->Game）
- [已完成 ✅] 1.3 更新消息 ID 常量: 在 internal_msg_ids.h 中添加账号相关消息 ID

## 2. DBMgr 扩展
- [已完成 ✅] 2.1 扩展 DataManager: 添加账号数据存储方法（get_account, set_account）
- [已完成 ✅] 2.2 扩展 DbMgrServer: 处理 AccountDataReq 和 AccountSetReq 消息
- [已完成 ✅] 2.3 更新 DBMgr CMakeLists.txt: 添加 account.pb.cc 依赖

## 3. Game Server 扩展
- [已完成 ✅] 3.1 扩展 DBMgrConnectionManager: 添加账号数据请求方法
- [已完成 ✅] 3.2 扩展 message_handler: 注册账号消息处理
- [已完成 ✅] 3.3 更新 Game Server CMakeLists.txt: 添加 account.pb.cc 依赖

## 4. Proto 生成与编译
- [已完成 ✅] 4.1 生成 protobuf 文件: account.pb.cc/h
- [已完成 ✅] 4.2 编译 DBMgr 验证
- [已完成 ✅] 4.3 编译 Game Server 验证
