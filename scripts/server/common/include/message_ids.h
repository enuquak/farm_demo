// 自动生成，请勿手动修改
// 生成时间：2026-06-02 23:00:43
// 源文件：shared/message_ids.json
#pragma once

#include <cstdint>

namespace farm {

inline constexpr uint32_t MSG_ID_HEARTBEAT                        = 1;  // 心跳消息
inline constexpr uint32_t MSG_ID_HEARTBEAT_RESP                   = 2;  // 心跳响应
inline constexpr uint32_t MSG_ID_LOGIN_REQ                        = 3;  // 登录请求
inline constexpr uint32_t MSG_ID_LOGIN_RESP                       = 4;  // 登录响应
inline constexpr uint32_t MSG_ID_QUERY_ROLES_REQ                  = 1001;  // 查询角色请求
inline constexpr uint32_t MSG_ID_QUERY_ROLES_RESP                 = 1002;  // 查询角色响应
inline constexpr uint32_t MSG_ID_CREATE_ROLE_REQ                  = 1003;  // 创建角色请求
inline constexpr uint32_t MSG_ID_CREATE_ROLE_RESP                 = 1004;  // 创建角色响应
inline constexpr uint32_t MSG_ID_ENTER_GAME_REQ                   = 1005;  // 进入游戏请求
inline constexpr uint32_t MSG_ID_ENTER_GAME_RESP                  = 1006;  // 进入游戏响应
inline constexpr uint32_t MSG_ID_MAP_DATA_NOTIFY                  = 2001;  // 地图数据通知
inline constexpr uint32_t MSG_ID_POSITION_UPDATE                  = 2101;  // 位置更新
inline constexpr uint32_t MSG_ID_POSITION_CORRECT                 = 2102;  // 位置纠正
inline constexpr uint32_t MSG_ID_SCENE_CHANGE_REQ                 = 2201;  // 场景切换请求
inline constexpr uint32_t MSG_ID_SCENE_CHANGE_RESP                = 2202;  // 场景切换响应
inline constexpr uint32_t MSG_ID_CLOCK_SYNC                       = 2301;  // 时钟同步
inline constexpr uint32_t MSG_ID_FORCE_SLEEP_NOTIFY               = 2302;  // 强制睡觉通知
inline constexpr uint32_t MSG_ID_FORCE_SLEEP_READY                = 2303;  // 强制睡觉就绪
inline constexpr uint32_t MSG_ID_ITEM_USE_REQ                     = 3001;  // 物品使用请求
inline constexpr uint32_t MSG_ID_ITEM_USE_RESP                    = 3002;  // 物品使用响应
inline constexpr uint32_t MSG_ID_FARM_STATE_SYNC                  = 3003;  // 农场状态同步
inline constexpr uint32_t MSG_ID_DROP_ITEM_SYNC                   = 3004;  // 掉落物同步
inline constexpr uint32_t MSG_ID_INVENTORY_SYNC                   = 3100;  // 背包数据同步
inline constexpr uint32_t MSG_ID_ACTIVE_SLOT_CHANGE               = 3101;  // 快捷栏选中格切换
inline constexpr uint32_t MSG_ID_GIFT_REQ                         = 3201;  // NPC送礼请求
inline constexpr uint32_t MSG_ID_GIFT_RESP                        = 3202;  // NPC送礼响应
inline constexpr uint32_t MSG_ID_AFFECTION_SYNC                   = 3203;  // 好感度同步
inline constexpr uint32_t MSG_ID_DIALOG_START_NOTIFY              = 3204;  // NPC对话开始通知
inline constexpr uint32_t MSG_ID_QUEST_ACCEPT_REQ                 = 4001;  // 任务接受请求
inline constexpr uint32_t MSG_ID_QUEST_ACCEPT_RESP                = 4002;  // 任务接受响应
inline constexpr uint32_t MSG_ID_QUEST_SUBMIT_REQ                 = 4003;  // 任务提交请求
inline constexpr uint32_t MSG_ID_QUEST_SUBMIT_RESP                = 4004;  // 任务提交响应
inline constexpr uint32_t MSG_ID_QUEST_ABANDON_REQ                = 4005;  // 任务放弃请求
inline constexpr uint32_t MSG_ID_QUEST_ABANDON_RESP               = 4006;  // 任务放弃响应
inline constexpr uint32_t MSG_ID_QUEST_SYNC_NOTIFY                = 4007;  // 任务同步通知
inline constexpr uint32_t MSG_ID_QUEST_PROGRESS_NOTIFY            = 4008;  // 任务进度通知

}  // namespace farm
