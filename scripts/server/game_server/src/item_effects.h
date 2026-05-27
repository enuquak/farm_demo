#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace farm {

// Ground type constants (matching client GroundType enum)
enum class GroundType : uint8_t {
    GRASS = 0,
    DIRT = 1,
    WATER = 2,
    SAND = 3,
    TILLED = 4,
};

// Object type constants (matching client ObjectType enum)
enum class ObjectType : uint8_t {
    NONE = 0,
    STONE = 1,
    CROP_GROWING = 2,
    CROP_READY = 3,
    TREE = 4,
    DOOR_IN = 10,
    DOOR_OUT = 11,
    BED = 12,
    TV = 13,
    STOVE = 14,
};

// Drop definition for an effect
struct DropDef {
    int32_t item_id;
    int32_t min_count;
    int32_t max_count;
};

// Item effect configuration
struct ItemEffect {
    bool remove_object = false;       // Remove object at target
    bool consume_self = false;        // Consume 1 of the used item
    std::string set_ground;           // Set ground type (e.g. "TILLED")
    std::string place_object;         // Place object (e.g. "CROP_GROWING")
    int32_t energy_cost = 0;          // Energy cost to use
    int32_t energy_restore = 0;       // Energy restored (food)
    int32_t interact_range = 1;       // Chebyshev distance (-1 = unlimited)
    std::vector<DropDef> drops;       // Items dropped on use
};

// Target key for effect lookup
struct TargetKey {
    std::string obj_type;   // e.g. "STONE" (empty if no object)
    std::string ground_type; // e.g. "GRASS"
};

/**
 * @brief Static item effects registry.
 *
 * C++ equivalent of scripts/server/item_registry.py.
 * Data-driven: new items only need configuration changes.
 */
class ItemEffects {
public:
    // Item type constants
    static constexpr const char* TYPE_RESOURCE = "RESOURCE";
    static constexpr const char* TYPE_TOOL = "TOOL";
    static constexpr const char* TYPE_SEED = "SEED";
    static constexpr const char* TYPE_FOOD = "FOOD";

    // Item name lookup (for logging)
    static const char* get_item_name(int32_t item_id);

    // Item type lookup
    static const char* get_item_type(int32_t item_id);

    // Max stack size lookup
    static int32_t get_max_stack(int32_t item_id);

    /**
     * @brief Look up the effect of using an item on a target.
     *
     * Lookup priority:
     *   1. obj:<obj_type>  - if obj_type is not empty
     *   2. gnd:<ground_type> - if ground_type is not empty
     *   3. ANY - fallback
     *
     * @param item_id The item being used
     * @param obj_type Object type at target (e.g. "STONE"), empty if none
     * @param ground_type Ground type at target (e.g. "GRASS")
     * @return Pointer to effect if found, nullptr otherwise
     */
    static const ItemEffect* get_effect(int32_t item_id,
                                         const std::string& obj_type,
                                         const std::string& ground_type);

    // Convert enum to string
    static const char* ground_type_to_string(GroundType gt);
    static const char* object_type_to_string(ObjectType ot);

    // Convert string to enum (returns -1 if not found)
    static int string_to_ground_type(const std::string& name);
    static int string_to_object_type(const std::string& name);

private:
    // Effect tables: item_id -> {target_key -> effect}
    // Target keys: "obj:TYPE", "gnd:TYPE", "ANY"
    using EffectMap = std::unordered_map<std::string, ItemEffect>;
    static const std::unordered_map<int32_t, EffectMap>& get_effects();

    // Item definitions
    struct ItemDef {
        const char* name;
        const char* type;
        int32_t max_stack;
    };
    static const std::unordered_map<int32_t, ItemDef>& get_item_defs();
};

}  // namespace farm
