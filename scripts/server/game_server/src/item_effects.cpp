#include "item_effects.h"

namespace farm {

// ===========================================
// Item definitions
// ===========================================

const std::unordered_map<int32_t, ItemEffects::ItemDef>& ItemEffects::get_item_defs() {
    static const std::unordered_map<int32_t, ItemDef> defs = {
        {1, {"wood",      TYPE_RESOURCE, 99}},
        {2, {"stone",     TYPE_RESOURCE, 99}},
        {3, {"axe",       TYPE_TOOL,      1}},
        {4, {"hoe",       TYPE_TOOL,      1}},
        {5, {"seed",      TYPE_SEED,     99}},
        {6, {"bread",     TYPE_FOOD,     20}},
        {7, {"crop",      TYPE_RESOURCE, 99}},
    };
    return defs;
}

const char* ItemEffects::get_item_name(int32_t item_id) {
    auto& defs = get_item_defs();
    auto it = defs.find(item_id);
    if (it != defs.end()) {
        return it->second.name;
    }
    return "unknown";
}

const char* ItemEffects::get_item_type(int32_t item_id) {
    auto& defs = get_item_defs();
    auto it = defs.find(item_id);
    if (it != defs.end()) {
        return it->second.type;
    }
    return "unknown";
}

int32_t ItemEffects::get_max_stack(int32_t item_id) {
    auto& defs = get_item_defs();
    auto it = defs.find(item_id);
    if (it != defs.end()) {
        return it->second.max_stack;
    }
    return 99;
}

// ===========================================
// Item effects configuration
// ===========================================

const std::unordered_map<int32_t, ItemEffects::EffectMap>& ItemEffects::get_effects() {
    static const std::unordered_map<int32_t, EffectMap> effects = {
        // Axe (id=3): break stone
        {3, {
            {"obj:STONE", ItemEffect{
                /*remove_object=*/true,
                /*consume_self=*/false,
                /*set_ground=*/"",
                /*place_object=*/"",
                /*energy_cost=*/4,
                /*energy_restore=*/0,
                /*interact_range=*/1,
                /*drops=*/{{2, 1, 3}}  // 1~3 stone
            }},
        }},
        // Hoe (id=4): till grass/dirt
        {4, {
            {"gnd:GRASS", ItemEffect{
                false, false, "TILLED", "", 2, 0, 50, {}
            }},
            {"gnd:DIRT", ItemEffect{
                false, false, "TILLED", "", 2, 0, 50, {}
            }},
        }},
        // Seed (id=5): plant on tilled soil
        {5, {
            {"gnd:TILLED", ItemEffect{
                false, true, "", "CROP_GROWING", 1, 0, 1, {}
            }},
        }},
        // Bread (id=6): eat anywhere
        {6, {
            {"ANY", ItemEffect{
                false, true, "", "", 0, 15, -1, {}
            }},
        }},
    };
    return effects;
}

const ItemEffect* ItemEffects::get_effect(int32_t item_id,
                                            const std::string& obj_type,
                                            const std::string& ground_type) {
    auto& all_effects = get_effects();
    auto it = all_effects.find(item_id);
    if (it == all_effects.end()) {
        return nullptr;
    }
    const auto& effect_map = it->second;

    // Priority 1: match object type
    if (!obj_type.empty()) {
        std::string key = "obj:" + obj_type;
        auto eit = effect_map.find(key);
        if (eit != effect_map.end()) {
            return &eit->second;
        }
    }

    // Priority 2: match ground type
    if (!ground_type.empty()) {
        std::string key = "gnd:" + ground_type;
        auto eit = effect_map.find(key);
        if (eit != effect_map.end()) {
            return &eit->second;
        }
    }

    // Priority 3: fallback to ANY
    auto eit = effect_map.find("ANY");
    if (eit != effect_map.end()) {
        return &eit->second;
    }

    return nullptr;
}

// ===========================================
// Type string conversions
// ===========================================

const char* ItemEffects::ground_type_to_string(GroundType gt) {
    switch (gt) {
        case GroundType::GRASS:      return "GRASS";
        case GroundType::DIRT:       return "DIRT";
        case GroundType::WATER:      return "WATER";
        case GroundType::SAND:       return "SAND";
        case GroundType::TILLED:     return "TILLED";
        case GroundType::WALL:       return "WALL";
        case GroundType::WOOD_FLOOR: return "WOOD_FLOOR";
        default: return "UNKNOWN";
    }
}

const char* ItemEffects::object_type_to_string(ObjectType ot) {
    switch (ot) {
        case ObjectType::NONE:         return "";
        case ObjectType::STONE:        return "STONE";
        case ObjectType::CROP_GROWING: return "CROP_GROWING";
        case ObjectType::CROP_READY:   return "CROP_READY";
        case ObjectType::TREE:         return "TREE";
        case ObjectType::DOOR_IN:      return "DOOR_IN";
        case ObjectType::DOOR_OUT:     return "DOOR_OUT";
        case ObjectType::BED:          return "BED";
        case ObjectType::TV:           return "TV";
        case ObjectType::STOVE:        return "STOVE";
        default: return "UNKNOWN";
    }
}

int ItemEffects::string_to_ground_type(const std::string& name) {
    if (name == "GRASS")      return static_cast<int>(GroundType::GRASS);
    if (name == "DIRT")       return static_cast<int>(GroundType::DIRT);
    if (name == "WATER")      return static_cast<int>(GroundType::WATER);
    if (name == "SAND")       return static_cast<int>(GroundType::SAND);
    if (name == "TILLED")     return static_cast<int>(GroundType::TILLED);
    if (name == "WALL")       return static_cast<int>(GroundType::WALL);
    if (name == "WOOD_FLOOR") return static_cast<int>(GroundType::WOOD_FLOOR);
    return -1;
}

int ItemEffects::string_to_object_type(const std::string& name) {
    if (name == "STONE")        return static_cast<int>(ObjectType::STONE);
    if (name == "CROP_GROWING") return static_cast<int>(ObjectType::CROP_GROWING);
    if (name == "CROP_READY")   return static_cast<int>(ObjectType::CROP_READY);
    if (name == "TREE")         return static_cast<int>(ObjectType::TREE);
    if (name == "DOOR_IN")      return static_cast<int>(ObjectType::DOOR_IN);
    if (name == "DOOR_OUT")     return static_cast<int>(ObjectType::DOOR_OUT);
    if (name == "BED")          return static_cast<int>(ObjectType::BED);
    if (name == "TV")           return static_cast<int>(ObjectType::TV);
    if (name == "STOVE")        return static_cast<int>(ObjectType::STOVE);
    return -1;
}

}  // namespace farm
