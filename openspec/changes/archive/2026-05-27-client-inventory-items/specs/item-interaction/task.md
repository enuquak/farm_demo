# item-interaction 任务清单

## 1. Protobuf 消息定义
- [x] 1.1 在 player.proto 中添加 ItemUseReq、ItemUseResp、DropItemSync 消息
- [x] 1.2 重新生成 protobuf Python 代码
- [x] 1.3 在 msg_ids.h 和 msg_ids.py 中添加新的消息 ID

## 2. 服务端物品使用处理
- [x] 2.1 在 game_server 中注册 ItemUseReq handler
- [x] 2.2 实现验证逻辑（activeSlot 非空、效果匹配、能量充足）
- [x] 2.3 实现效果执行（remove_object、set_ground、place_object、consume_self、drops）
- [x] 2.4 实现收割成熟作物（空手 + CROP_READY）

## 3. 服务端掉落物系统
- [x] 3.1 实现 DropItem 实体管理（生成、生命周期 300s）
- [x] 3.2 实现自动拾取（距离 < 48px、背包空间检查）
- [x] 3.3 实现 DropItemSync 广播（spawn、remove、pickup）

## 4. 客户端物品使用
- [x] 4.1 修改 game_scene.py 发送 ItemUseReq（空格键 + 鼠标点击）
- [x] 4.2 处理 ItemUseResp 响应
- [x] 4.3 实现物品使用冷却（300ms）

## 5. 客户端掉落物渲染
- [x] 5.1 渲染 DropItem 实体（物品图标）
- [x] 5.2 实现浮动动画（sin 波动，幅度 2px）
- [x] 5.3 处理 DropItemSync 消息（spawn、remove）
