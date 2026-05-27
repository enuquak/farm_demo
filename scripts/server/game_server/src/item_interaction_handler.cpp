#include "item_interaction_handler.h"
#include "player.h"
#include "game_server.h"
#include "msg_ids.h"
#include "log_macros.h"

#include <nlohmann/json.hpp>
#include <cmath>
#include <ctime>
#include <cstdlib>

namespace farm {

// ===========================================
// PlayerInventory serialization
// ===========================================

std::string PlayerInventory::serialize() const {
    nlohmann::json j;

    nlohmann::json slots_arr = nlohmann::json::array();
    for (const auto& slot : slots) {
        if (slot.count > 0) {
            nlohmann::json s;
            s["item_id"] = slot.item_id;
            s["count"] = slot.count;
            slots_arr.push_back(s);
        } else {
            slots_arr.push_back(nullptr);
        }
    }

    j["slots"] = slots_arr;
    j["active_slot"] = active_slot;

    return j.dump();
}

bool PlayerInventory::deserialize(const std::string& json_str) {
    if (json_str.empty() || json_str == "{}") {
        return false;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(json_str);

        if (!j.contains("slots")) {
            return false;
        }

        auto& slots_arr = j["slots"];
        for (size_t i = 0; i < slots_arr.size() && i < 30; i++) {
            if (slots_arr[i].is_null()) {
                slots[i] = {0, 0};
            } else {
                slots[i].item_id = slots_arr[i].value("item_id", 0);
                slots[i].count = slots_arr[i].value("count", 0);
            }
        }

        if (j.contains("active_slot")) {
            active_slot = j["active_slot"].get<int32_t>();
            if (active_slot < 0 || active_slot >= 10) {
                active_slot = 0;
            }
        }

        return true;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[ItemHandler]Failed to parse inventory: {}", e.what());
        return false;
    }
}

// ===========================================
// ItemInteractionHandler
// ===========================================

ItemInteractionHandler::ItemInteractionHandler(WorldState* world, DropItemManager* drops, CropSystem* crops)
    : world_(world)
    , drops_(drops)
    , crops_(crops)
{
}

ItemUseResult ItemInteractionHandler::handle_item_use(
    Player* player,
    int32_t target_x, int32_t target_y,
    const std::string& direction,
    int32_t active_slot,
    GameServer* game_server) {

    if (!player) {
        return ItemUseResult::INTERNAL_ERROR;
    }

    uint64_t player_id = player->player_id();

    // Load inventory
    PlayerInventory inv;
    if (!load_inventory(player, inv)) {
        SPDLOG_ERROR("[ItemHandler]Failed to load inventory for player={}", player_id);
        return ItemUseResult::INTERNAL_ERROR;
    }

    // Use the client's requested active slot (validated)
    if (active_slot >= 0 && active_slot < 10) {
        inv.active_slot = active_slot;
    }
    InventorySlot* active = inv.get_active_slot();

    // Get target tile info
    ObjectType obj_type = world_->get_object(target_x, target_y);
    GroundType ground_type = world_->get_ground(target_x, target_y);

    const char* obj_str = ItemEffects::object_type_to_string(obj_type);
    const char* gnd_str = ItemEffects::ground_type_to_string(ground_type);

    SPDLOG_INFO("[ItemHandler]Player={} use item at ({},{}), obj={} gnd={}, active_slot={} item_id={}",
                player_id, target_x, target_y, obj_str, gnd_str,
                inv.active_slot, active ? active->item_id : -1);

    // Check if active slot has an item
    if (!active || active->count <= 0) {
        // No active item - check for harvestable crop
        if (obj_type == ObjectType::CROP_READY) {
            // Harvest costs 1 energy
            static constexpr int32_t HARVEST_ENERGY_COST = 1;
            int32_t current_energy = player->get_energy();
            if (current_energy < HARVEST_ENERGY_COST) {
                SPDLOG_INFO("[ItemHandler]Player={} energy exhausted for harvest (have={}, need={})",
                            player_id, current_energy, HARVEST_ENERGY_COST);
                return ItemUseResult::ENERGY_EXHAUSTED;
            }

            if (harvest_crop(target_x, target_y, inv)) {
                // Deduct energy for harvest
                player->set_energy(current_energy - HARVEST_ENERGY_COST);
                SPDLOG_INFO("[ItemHandler]Player={} energy deducted {} for harvest (now={})",
                            player_id, HARVEST_ENERGY_COST, current_energy - HARVEST_ENERGY_COST);

                save_inventory(player, inv);
                SPDLOG_INFO("[ItemHandler]Player={} harvested crop at ({},{})",
                            player_id, target_x, target_y);
                return ItemUseResult::SUCCESS;
            }
        }
        SPDLOG_INFO("[ItemHandler]Player={} no active item, no crop to harvest", player_id);
        return ItemUseResult::NO_ACTIVE_ITEM;
    }

    int32_t item_id = active->item_id;

    // Look up effect
    const ItemEffect* effect = ItemEffects::get_effect(item_id, obj_str, gnd_str);
    if (!effect) {
        SPDLOG_INFO("[ItemHandler]Player={} no matching effect for item={} on obj={} gnd={}",
                    player_id, item_id, obj_str, gnd_str);
        return ItemUseResult::NO_MATCHING_EFFECT;
    }

    // Check interact range (Chebyshev distance)
    {
        float player_tile_x = player->get_pos_x() / TILE_SIZE;
        float player_tile_y = player->get_pos_y() / TILE_SIZE;
        int32_t dx = std::abs(static_cast<int32_t>(player_tile_x) - target_x);
        int32_t dy = std::abs(static_cast<int32_t>(player_tile_y) - target_y);
        int32_t chebyshev_dist = std::max(dx, dy);
        if (effect->interact_range >= 0 && chebyshev_dist > effect->interact_range) {
            SPDLOG_INFO("[ItemHandler]Player={} target ({},{}) out of range (dist={}, range={})",
                        player_id, target_x, target_y, chebyshev_dist, effect->interact_range);
            return ItemUseResult::INVALID_TARGET;
        }
    }

    // Check energy
    if (effect->energy_cost > 0) {
        int32_t current_energy = player->get_energy();
        if (current_energy < effect->energy_cost) {
            SPDLOG_INFO("[ItemHandler]Player={} energy exhausted (have={}, need={})",
                        player_id, current_energy, effect->energy_cost);
            return ItemUseResult::ENERGY_EXHAUSTED;
        }
    }

    // Execute the effect
    if (!execute_effect(*effect, target_x, target_y, inv, item_id)) {
        return ItemUseResult::INTERNAL_ERROR;
    }

    // Deduct energy
    if (effect->energy_cost > 0) {
        int32_t new_energy = player->get_energy() - effect->energy_cost;
        player->set_energy(new_energy);
        SPDLOG_INFO("[ItemHandler]Player={} energy deducted {} (now={})",
                    player_id, effect->energy_cost, new_energy);
    }

    // Restore energy (food), capped at max_energy (100)
    if (effect->energy_restore > 0) {
        static constexpr int32_t MAX_ENERGY = 100;
        int32_t new_energy = std::min(player->get_energy() + effect->energy_restore, MAX_ENERGY);
        player->set_energy(new_energy);
        SPDLOG_INFO("[ItemHandler]Player={} energy restored {} (now={})",
                    player_id, effect->energy_restore, new_energy);
    }

    // Save inventory
    save_inventory(player, inv);

    SPDLOG_INFO("[ItemHandler]Player={} item use successful, item_id={}", player_id, item_id);
    return ItemUseResult::SUCCESS;
}

bool ItemInteractionHandler::execute_effect(const ItemEffect& effect,
                                              int32_t target_x, int32_t target_y,
                                              PlayerInventory& inventory,
                                              int32_t active_item_id) {
    // Remove object
    if (effect.remove_object) {
        world_->set_object(target_x, target_y, ObjectType::NONE);
        SPDLOG_INFO("[ItemHandler]Removed object at ({},{})", target_x, target_y);
    }

    // Set ground
    if (!effect.set_ground.empty()) {
        int gt = ItemEffects::string_to_ground_type(effect.set_ground);
        if (gt >= 0) {
            world_->set_ground(target_x, target_y, static_cast<GroundType>(gt));
            SPDLOG_INFO("[ItemHandler]Set ground to {} at ({},{})", effect.set_ground, target_x, target_y);
        }
    }

    // Place object
    if (!effect.place_object.empty()) {
        int ot = ItemEffects::string_to_object_type(effect.place_object);
        if (ot >= 0) {
            world_->set_object(target_x, target_y, static_cast<ObjectType>(ot));
            SPDLOG_INFO("[ItemHandler]Placed {} at ({},{})", effect.place_object, target_x, target_y);

            // Register crop if placing CROP_GROWING
            if (effect.place_object == "CROP_GROWING") {
                if (crops_) {
                    crops_->register_crop(target_x, target_y, world_);
                }
            }
        }
    }

    // Spawn drops
    for (const auto& drop_def : effect.drops) {
        int32_t drop_count = drop_def.min_count;
        if (drop_def.max_count > drop_def.min_count) {
            drop_count = drop_def.min_count +
                (std::rand() % (drop_def.max_count - drop_def.min_count + 1));
        }

        // Convert tile coordinates to pixel coordinates (center of tile)
        float pixel_x = (target_x + 0.5f) * TILE_SIZE;
        float pixel_y = (target_y + 0.5f) * TILE_SIZE;

        auto drop_ids = drops_->spawn_drops(drop_def.item_id, drop_count, pixel_x, pixel_y);

        // Broadcast spawn to clients
        for (uint32_t did : drop_ids) {
            const DropItem* drop = drops_->get(did);
            if (drop) {
                broadcast_drop_sync(did, *drop, 0, nullptr);  // action=0 (spawn)
            }
        }
    }

    // Consume self
    if (effect.consume_self) {
        if (!inventory.remove_item(active_item_id, 1)) {
            SPDLOG_WARN("[ItemHandler]Failed to consume item_id={}", active_item_id);
        } else {
            SPDLOG_INFO("[ItemHandler]Consumed 1 of item_id={}", active_item_id);
        }
    }

    return true;
}

bool ItemInteractionHandler::harvest_crop(int32_t target_x, int32_t target_y,
                                            PlayerInventory& inventory) {
    // Remove the CROP_READY object
    world_->set_object(target_x, target_y, ObjectType::NONE);

    // Remove crop data from CropSystem if exists
    if (crops_) {
        crops_->remove_crop(target_x, target_y);
    }

    // Add 2~3 crops to inventory (item_id=7)
    int32_t crop_count = 2 + (std::rand() % 2);  // 2 or 3
    int32_t max_stack = ItemEffects::get_max_stack(7);
    int32_t leftover = inventory.add_item(7, crop_count, max_stack);

    if (leftover > 0) {
        SPDLOG_WARN("[ItemHandler]Inventory full, {} crops dropped", leftover);
        // TODO: spawn drop items for leftover crops
    }

    SPDLOG_INFO("[ItemHandler]Harvested {} crops at ({},{})", crop_count, target_x, target_y);
    return true;
}

bool ItemInteractionHandler::load_inventory(Player* player, PlayerInventory& inv) {
    const std::string& inv_json = player->get_inventory();
    if (!inv_json.empty() && inv_json != "{}") {
        return inv.deserialize(inv_json);
    }

    // Default inventory for new players
    inv.slots[0] = {3, 1};   // axe x1
    inv.slots[1] = {4, 1};   // hoe x1
    inv.slots[2] = {5, 5};   // seeds x5
    inv.slots[3] = {6, 3};   // bread x3
    return true;
}

void ItemInteractionHandler::save_inventory(Player* player, const PlayerInventory& inv) {
    std::string json = inv.serialize();
    player->set_inventory(json);
}

void ItemInteractionHandler::broadcast_drop_sync(uint32_t drop_id, const DropItem& drop,
                                                    int action, GameServer* game_server) {
    // This will be called from the game server context
    // For now, we just log - the actual broadcast is done in the handler registration
    SPDLOG_INFO("[ItemHandler]DropItemSync: drop_id={} item_id={} action={} pos=({:.1f},{:.1f})",
                drop_id, drop.item_id, action, drop.x, drop.y);
}

void ItemInteractionHandler::update(GameServer* game_server) {
    time_t now = std::time(nullptr);

    // Update crops via CropSystem
    if (crops_) {
        crops_->update(world_);
    }

    // Update drop items (remove expired)
    auto expired = drops_->update(now);
    for (uint32_t drop_id : expired) {
        SPDLOG_INFO("[ItemHandler]Drop expired: drop_id={}", drop_id);
        // The actual broadcast of removal would be done by the game server
    }
}

}  // namespace farm
