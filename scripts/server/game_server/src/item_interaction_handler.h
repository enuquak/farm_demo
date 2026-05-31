#pragma once

#include "item_effects.h"
#include "world_state.h"
#include "drop_item_manager.h"
#include "crop_system.h"

#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <ctime>
#include <memory>

namespace farm {

class Player;
class GameServer;

// Tile size in pixels (matching client TILE_SIZE)
static constexpr int TILE_SIZE = 32;

// Response codes for ItemUseResp
enum class ItemUseResult : int32_t {
    SUCCESS = 0,
    NO_ACTIVE_ITEM = 1,         // No item in active slot (and no crop to harvest)
    NO_MATCHING_EFFECT = 2,     // Item has no effect on this target
    ENERGY_EXHAUSTED = 3,       // Not enough energy
    INVALID_TARGET = 4,         // Target out of range or invalid
    INTERNAL_ERROR = 5,         // Server error
};

// Inventory slot representation (matching client Slot)
struct InventorySlot {
    int32_t item_id = 0;
    int32_t count = 0;
};

// Player inventory state (parsed from JSON)
struct PlayerInventory {
    std::vector<InventorySlot> slots;  // 30 slots
    int32_t active_slot = 0;           // 0-9

    PlayerInventory() : slots(30) {}

    // Create default inventory for new players
    static PlayerInventory create_default() {
        PlayerInventory inv;
        inv.slots[0] = {3, 1};   // axe x1
        inv.slots[1] = {4, 1};   // hoe x1
        inv.slots[2] = {5, 5};   // seeds x5
        inv.slots[3] = {6, 3};   // bread x3
        return inv;
    }

    std::optional<InventorySlot*> get_active_slot() {
        if (active_slot >= 0 && active_slot < 30) {
            return &slots[active_slot];
        }
        return std::nullopt;
    }

    std::optional<const InventorySlot*> get_active_slot() const {
        if (active_slot >= 0 && active_slot < 30) {
            return &slots[active_slot];
        }
        return std::nullopt;
    }

    int32_t get_count(int32_t item_id) const {
        int32_t total = 0;
        for (const auto& slot : slots) {
            if (slot.item_id == item_id && slot.count > 0) {
                total += slot.count;
            }
        }
        return total;
    }

    bool remove_item(int32_t item_id, int32_t count) {
        if (get_count(item_id) < count) return false;

        int32_t remaining = count;
        for (auto& slot : slots) {
            if (remaining <= 0) break;
            if (slot.item_id == item_id && slot.count > 0) {
                if (slot.count <= remaining) {
                    remaining -= slot.count;
                    slot.item_id = 0;
                    slot.count = 0;
                } else {
                    slot.count -= remaining;
                    remaining = 0;
                }
            }
        }
        return true;
    }

    int32_t add_item(int32_t item_id, int32_t count, int32_t max_stack) {
        int32_t remaining = count;

        // Phase 1: stack into existing slots
        for (auto& slot : slots) {
            if (remaining <= 0) break;
            if (slot.item_id == item_id && slot.count > 0 && slot.count < max_stack) {
                int32_t can_add = std::min(remaining, max_stack - slot.count);
                slot.count += can_add;
                remaining -= can_add;
            }
        }

        // Phase 2: fill empty slots
        for (auto& slot : slots) {
            if (remaining <= 0) break;
            if (slot.count == 0) {
                int32_t can_add = std::min(remaining, max_stack);
                slot.item_id = item_id;
                slot.count = can_add;
                remaining -= can_add;
            }
        }

        return remaining;  // 0 = all added
    }

    // Serialize to JSON string
    std::string serialize() const;

    // Deserialize from JSON string
    bool deserialize(const std::string& json_str);
};

/**
 * @brief Handles item interaction logic.
 *
 * Validates ItemUseReq, executes effects, manages inventory changes,
 * and coordinates with WorldState and DropItemManager.
 */
class ItemInteractionHandler {
public:
    // Default constructor: owns its own WorldState, DropItemManager, CropSystem
    ItemInteractionHandler();
    // External-dependency constructor: uses provided pointers (caller owns the objects)
    ItemInteractionHandler(WorldState* world, DropItemManager* drops, CropSystem* crops);

    /**
     * @brief Process an item use request.
     *
     * @param player The player using the item
     * @param target_x Target tile X
     * @param target_y Target tile Y
     * @param direction Player facing direction
     * @param active_slot Client-reported active slot index
     * @param game_server Game server for sending responses/broadcasts
     * @return Result code
     */
    ItemUseResult handle_item_use(Player* player,
                                   int32_t target_x, int32_t target_y,
                                   const std::string& direction,
                                   int32_t active_slot,
                                   GameServer* game_server);

    /**
     * @brief Update crops and drop items (called periodically).
     */
    void update(GameServer* game_server);

    /**
     * @brief Get the world state.
     */
    WorldState* world() { return world_; }

    /**
     * @brief Get the drop item manager.
     */
    DropItemManager* drops() { return drops_; }

    /**
     * @brief Get the crop system.
     */
    CropSystem* crops() { return crops_; }

private:
    // Execute effect on the world
    bool execute_effect(const ItemEffect& effect,
                        int32_t target_x, int32_t target_y,
                        PlayerInventory& inventory,
                        int32_t active_item_id);

    // Harvest a mature crop
    bool harvest_crop(int32_t target_x, int32_t target_y,
                      PlayerInventory& inventory);

    // Parse inventory from player data
    bool load_inventory(Player* player, PlayerInventory& inv);

    // Save inventory back to player data
    void save_inventory(Player* player, const PlayerInventory& inv);

    // Broadcast DropItemSync to nearby players
    void broadcast_drop_sync(uint32_t drop_id, const DropItem& drop,
                              int action, GameServer* game_server);

    // Owned instances (used when default-constructed)
    std::unique_ptr<WorldState> owned_world_;
    std::unique_ptr<DropItemManager> owned_drops_;
    std::unique_ptr<CropSystem> owned_crops_;

    // Working pointers (may point to owned or external instances)
    WorldState* world_;
    DropItemManager* drops_;
    CropSystem* crops_;
    std::mt19937 rng_{std::random_device{}()};
};

}  // namespace farm
