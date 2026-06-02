#include "event_bus.h"
#include "log_macros.h"

#include <algorithm>

namespace farm {

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

EventBus& EventBus::instance() {
    static EventBus bus;
    return bus;
}

// ---------------------------------------------------------------------------
// subscribe / unsubscribe
// ---------------------------------------------------------------------------

EventHandlerId EventBus::subscribe(EventType type, EventHandler handler) {
    EventHandlerId id = next_id_++;
    handlers_[type].emplace_back(id, std::move(handler));
    SPDLOG_DEBUG("[EventBus] subscribed handler {} to event '{}'",
                 id, event_type_name(type));
    return id;
}

void EventBus::unsubscribe(EventType type, EventHandlerId id) {
    auto it = handlers_.find(type);
    if (it == handlers_.end()) {
        SPDLOG_WARN("[EventBus] unsubscribe: no handlers for event '{}'",
                     event_type_name(type));
        return;
    }

    auto& vec = it->second;
    auto  pos = std::remove_if(vec.begin(), vec.end(),
                               [id](const auto& pair) { return pair.first == id; });
    if (pos == vec.end()) {
        SPDLOG_WARN("[EventBus] unsubscribe: handler {} not found for event '{}'",
                     id, event_type_name(type));
        return;
    }
    vec.erase(pos, vec.end());
    SPDLOG_DEBUG("[EventBus] unsubscribed handler {} from event '{}'",
                 id, event_type_name(type));
}

// ---------------------------------------------------------------------------
// emit
// ---------------------------------------------------------------------------

void EventBus::emit(EventType type, uint64_t player_id, nlohmann::json data) {
    auto it = handlers_.find(type);
    if (it == handlers_.end()) {
        return;  // No listeners — nothing to do.
    }

    GameEvent event{type, player_id, std::move(data)};

    for (const auto& [id, handler] : it->second) {
        try {
            handler(event);
        } catch (const std::exception& ex) {
            SPDLOG_ERROR("[EventBus] handler {} for event '{}' threw exception: {}",
                         id, event_type_name(type), ex.what());
        } catch (...) {
            SPDLOG_ERROR("[EventBus] handler {} for event '{}' threw unknown exception",
                         id, event_type_name(type));
        }
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

const char* EventBus::event_type_name(EventType type) {
    switch (type) {
        case EventType::ItemCollected:     return "ItemCollected";
        case EventType::ItemUsed:          return "ItemUsed";
        case EventType::ItemCrafted:       return "ItemCrafted";
        case EventType::CropPlanted:       return "CropPlanted";
        case EventType::CropHarvested:     return "CropHarvested";
        case EventType::NPCTalked:         return "NPCTalked";
        case EventType::SceneVisited:      return "SceneVisited";
        case EventType::LevelUp:           return "LevelUp";
        case EventType::GoldChanged:       return "GoldChanged";
        case EventType::QuestAccepted:     return "QuestAccepted";
        case EventType::QuestCompleted:    return "QuestCompleted";
        case EventType::QuestFailed:       return "QuestFailed";
        case EventType::RewardGranted:     return "RewardGranted";
        case EventType::CustomConditionMet:return "CustomConditionMet";
    }
    return "Unknown";
}

}  // namespace farm
