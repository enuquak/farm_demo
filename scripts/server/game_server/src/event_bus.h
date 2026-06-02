#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace farm {

/**
 * @brief Event types emitted by the game systems.
 */
enum class EventType : uint32_t {
    ItemCollected,
    ItemUsed,
    ItemCrafted,
    CropPlanted,
    CropHarvested,
    NPCTalked,
    SceneVisited,
    LevelUp,
    GoldChanged,
    QuestAccepted,
    QuestCompleted,
    QuestFailed,
    RewardGranted,
    CustomConditionMet,
};

/**
 * @brief A game event carrying its type, the player who triggered it, and
 *        arbitrary JSON payload.
 */
struct GameEvent {
    EventType type;
    uint64_t player_id;
    nlohmann::json data;
};

using EventHandler   = std::function<void(const GameEvent&)>;
using EventHandlerId = uint64_t;

/**
 * @brief Central publish/subscribe bus for game events.
 *
 * The EventBus is a singleton. Systems subscribe to specific event types and
 * are notified synchronously when an event is emitted. Thread safety is NOT
 * required because the server runs a single-threaded event loop.
 */
class EventBus {
public:
    static EventBus& instance();

    // Non-copyable, non-movable
    EventBus(const EventBus&)            = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&)                 = delete;
    EventBus& operator=(EventBus&&)      = delete;

    /**
     * @brief Subscribe a handler to an event type.
     *
     * @param type    The event type to listen for.
     * @param handler Callback invoked when the event fires.
     * @return A unique handler ID that can be used to unsubscribe later.
     */
    EventHandlerId subscribe(EventType type, EventHandler handler);

    /**
     * @brief Unsubscribe a previously registered handler.
     *
     * @param type   The event type the handler was registered for.
     * @param id     The handler ID returned by subscribe().
     */
    void unsubscribe(EventType type, EventHandlerId id);

    /**
     * @brief Emit an event, invoking all registered handlers for its type.
     *
     * Exceptions thrown by individual handlers are caught and logged so that
     * one failing handler does not prevent others from running.
     *
     * @param type      The event type.
     * @param player_id The player who triggered the event.
     * @param data      Arbitrary JSON payload associated with the event.
     */
    void emit(EventType type, uint64_t player_id, nlohmann::json data = {});

private:
    EventBus() = default;

    static const char* event_type_name(EventType type);

    uint64_t next_id_ = 1;
    std::unordered_map<EventType, std::vector<std::pair<EventHandlerId, EventHandler>>> handlers_;
};

}  // namespace farm
