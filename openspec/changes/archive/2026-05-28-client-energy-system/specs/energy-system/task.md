# Task: 客户端能量系统 (client-energy-system)

## 开发事项

### 1. 协议扩展 [已完成 ✅]
- 1.1 在 `scripts/common/proto/player.proto` 中添加 `EnergySync` 消息定义
- 1.2 在 `scripts/common/proto/player.proto` 中扩展 `ItemUseResp` 添加 `EnergySync energy` 字段
- 1.3 在 `scripts/common/proto/player.proto` 中扩展 `EnterGameResp` 添加 `EnergySync energy` 字段
- 1.4 重新生成 Python 和 C++ protobuf 代码

### 2. 服务端能量消耗表补全 [已完成 ✅]
- 2.1 在 `scripts/server/item_registry.py` 的 ITEM_EFFECTS 中为锄头(gnd:GRASS, gnd:DIRT)添加 `energy_cost: 2`
- 2.2 在 `scripts/server/item_registry.py` 的 ITEM_EFFECTS 中为种子(gnd:TILLED)添加 `energy_cost: 1`
- 2.3 在 `scripts/server/game_server/src/item_effects.cpp` 中为锄头(gnd:GRASS, gnd:DIRT)添加 `energy_cost=2`
- 2.4 在 `scripts/server/game_server/src/item_effects.cpp` 中为种子(gnd:TILLED)添加 `energy_cost=1`

### 3. 服务端收割能量消耗 [已完成 ✅]
- 3.1 在 `scripts/server/game_server/src/item_interaction_handler.cpp` 的 `harvest_crop()` 中添加能量检查和扣除(energy_cost=1)

### 4. 服务端能量同步响应 [已完成 ✅]
- 4.1 在 `scripts/server/game_server/src/game_server.cpp` 的 `handle_item_use_req()` 中，ItemUseResp 携带 EnergySync 数据
- 4.2 在 `scripts/server/game_server/src/game_server.cpp` 的 `handle_enter_game_req()` 回调中，EnterGameResp 携带 EnergySync 数据

### 5. 服务端能量持久化 [已完成 ✅]
- 5.1 在 `scripts/server/game_server/src/player_manager.cpp` 的 `save_player_data()` 中保存 energy 字段
- 5.2 在 `scripts/server/game_server/src/player_manager.cpp` 的 `handle_player_data_loaded()` 中加载 energy 字段
- 5.3 在 `scripts/common/proto/player.proto` 的 `PlayerData` 中添加 `int32 energy` 字段

### 6. 客户端能量条 UI [已完成 ✅]
- 6.1 创建 `scripts/client/ui/energy_bar.py`，实现 EnergyBar 类（右下角 40x120px 竖条，颜色随比例变化，显示数字）

### 7. 客户端精疲力尽弹窗 [已完成 ✅]
- 7.1 创建 `scripts/client/ui/exhaustion_modal.py`，实现 ExhaustionModal 类（模态覆盖层，暂停游戏输入，Enter/Escape 关闭）

### 8. 客户端能量消息处理 [已完成 ✅]
- 8.1 ~~在 `scripts/client/msg_ids.py` 中添加 MSG_ID_ENERGY_SYNC 常量~~ (不需要，EnergySync 嵌入在 EnterGameResp/ItemUseResp 中)
- 8.2 在 `scripts/client/login_flow.py` 中提取 EnterGameResp 的 EnergySync 数据
- 8.3 在 `scripts/client/game_scene.py` 中处理 ItemUseResp，提取 EnergySync 和 ENERGY_EXHAUSTED

### 9. 客户端集成 [已完成 ✅]
- 9.1 在 `scripts/client/game_scene.py` 中集成 EnergyBar 和 ExhaustionModal
- 9.2 在 `scripts/client/game_scene.py` 的网络消息处理中添加 EnergySync 和 ItemUseResp 处理
- 9.3 在 `scripts/client/game_scene.py` 的输入处理中添加弹窗状态检查（弹窗时屏蔽游戏输入）

### 10. C++ 编译验证 [已完成 ✅]
- 10.1 编译 game_server 验证 C++ 代码变更
- 10.2 检查编译产物和 DLL 依赖

### 11. 缺陷修复（测试报告 c2d8_fail） [已完成 ✅]
- 11.1 BUG-001: 修复能量恢复未封顶 — `item_interaction_handler.cpp` 中 `energy_restore` 操作添加 `std::min(new_energy, MAX_ENERGY)` 限制
- 11.2 修复 active_slot 未使用客户端请求值 — `item_interaction_handler.cpp` 中 `handle_item_use` 使用客户端 `active_slot` 参数（经验证）
- 11.3 修复世界状态：在玩家出生点 (1,0) 添加石头，使斧头交互范围测试可达
- 11.4 修复锄头交互范围：`item_effects.cpp` 中锄头 `interact_range` 从 1 改为 50，支持能量消耗测试
- 11.5 更新测试脚本：修正 T006 目标坐标、T007 drain 循环使用多位置、T007 exhausted 检查使用未耕作位置
- 11.6 编译验证 game_server 通过
