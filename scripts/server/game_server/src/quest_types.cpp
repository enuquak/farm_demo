#include "quest_types.h"

#include <stdexcept>
#include <string>

namespace farm {

// ---------------------------------------------------------------------------
// ObjectiveType conversions
// ---------------------------------------------------------------------------

const char* objective_type_to_string(ObjectiveType type) {
    switch (type) {
        case ObjectiveType::CollectItem:  return "CollectItem";
        case ObjectiveType::PlantCrop:    return "PlantCrop";
        case ObjectiveType::HarvestCrop:  return "HarvestCrop";
        case ObjectiveType::TalkToNPC:    return "TalkToNPC";
        case ObjectiveType::VisitScene:   return "VisitScene";
        case ObjectiveType::CraftItem:    return "CraftItem";
        case ObjectiveType::UseItem:      return "UseItem";
        case ObjectiveType::ReachLevel:   return "ReachLevel";
        case ObjectiveType::OwnGold:      return "OwnGold";
        case ObjectiveType::Custom:       return "Custom";
    }
    return "Custom";
}

ObjectiveType objective_type_from_string(const std::string& str) {
    if (str == "CollectItem")  return ObjectiveType::CollectItem;
    if (str == "PlantCrop")    return ObjectiveType::PlantCrop;
    if (str == "HarvestCrop")  return ObjectiveType::HarvestCrop;
    if (str == "TalkToNPC")    return ObjectiveType::TalkToNPC;
    if (str == "VisitScene")   return ObjectiveType::VisitScene;
    if (str == "CraftItem")    return ObjectiveType::CraftItem;
    if (str == "UseItem")      return ObjectiveType::UseItem;
    if (str == "ReachLevel")   return ObjectiveType::ReachLevel;
    if (str == "OwnGold")      return ObjectiveType::OwnGold;
    if (str == "Custom")       return ObjectiveType::Custom;

    throw std::invalid_argument("Unknown ObjectiveType: " + str);
}

// ---------------------------------------------------------------------------
// TriggerType conversions
// ---------------------------------------------------------------------------

TriggerType trigger_type_from_string(const std::string& str) {
    if (str == "NPC") return TriggerType::NPC;
    return TriggerType::Auto;
}

} // namespace farm
