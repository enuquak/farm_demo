// 自动生成，请勿手动修改
// 生成时间：2026-06-02 23:06:42
// 源文件：tables/items.xlsx
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace farm {

struct ItemDef {
    int32_t item_id;
    std::string name;
    std::string type;
    int32_t max_stack;
};

inline const std::unordered_map<int32_t, ItemDef> ITEM_DEFS = {
    {1, {1, "木材", "RESOURCE", 99}},
    {2, {2, "石头", "RESOURCE", 99}},
    {3, {3, "斧头", "TOOL", 1}},
    {4, {4, "锄头", "TOOL", 1}},
    {5, {5, "种子", "SEED", 99}},
    {6, {6, "面包", "FOOD", 20}},
    {7, {7, "作物", "RESOURCE", 99}},
};

}  // namespace farm
