# Player Entity - Task List

## Task 1: 扩展 player.proto 添加新字段 [已完成 ✅]
- [x] 1.1 在 PlayerData message 中添加 `float pos_z = 9`（Z坐标）
- [x] 1.2 在 PlayerData message 中添加 `string scene_id = 10`（场景ID）
- [x] 1.3 重新生成 protobuf 代码

## Task 2: 扩展 PlayerBizData 数据结构 [已完成 ✅]
- [x] 2.1 在 PlayerBizData 中添加 `std::string role_name`
- [x] 2.2 在 PlayerBizData 中添加 `float pos_x, pos_y, pos_z`（坐标）
- [x] 2.3 在 PlayerBizData 中添加 `std::string scene_id`（场景ID）

## Task 3: 扩展 Player 类接口 [已完成 ✅]
- [x] 3.1 添加 role_name 的 getter/setter（get_role_name/set_role_name）
- [x] 3.2 添加 position 的 getter/setter（get_pos_x/set_pos_x, get_pos_y/set_pos_y, get_pos_z/set_pos_z）
- [x] 3.3 添加 scene_id 的 getter/setter（get_scene_id/set_scene_id）
- [x] 3.4 更新 init_default_data() 设置默认 position 和 scene_id

## Task 4: 修复 PlayerManager 数据加载逻辑 [已完成 ✅]
- [x] 4.1 修改 handle_player_data_loaded：使用 protobuf 解析 PlayerData（而非硬编码默认值）
- [x] 4.2 处理新玩家首次进入：当 DBMgr 返回空数据时初始化默认数据并保存

## Task 5: 修复 PlayerManager 数据保存逻辑 [已完成 ✅]
- [x] 5.1 修改 save_player_data：使用 protobuf 序列化 PlayerData（而非手动拼接 JSON）

## Task 6: 修复 GameServer EnterGameReq 处理 [已完成 ✅]
- [x] 6.1 修改 handle_enter_game_req：通过 PlayerManager::add_player_with_data_load 创建 Player 并加载数据
- [x] 6.2 在回调中构造 EnterGameResp 返回完整 PlayerData
- [x] 6.3 处理 DBMgr 不可用时回复失败

## Task 7: 编译验证 [已完成 ✅]
- [x] 7.1 编译 Game Server，确认无错误
- [x] 7.2 检查 DLL 依赖
