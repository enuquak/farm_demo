# Inventory Implementation Tasks

## Tasks

### 1. Create Inventory class with slot array and activeSlot
- [x] 1.1 Create `scripts/client/inventory.py` with Inventory class [已完成 ✅]
- [x] 1.2 Implement 30-slot array (10 hotbar + 20 extended) with None default [已完成 ✅]
- [x] 1.3 Implement activeSlot pointer (0-9) with range validation [已完成 ✅]

### 2. Implement core inventory operations
- [x] 2.1 Implement `add_item(item_id, count)` with stacking and overflow logic [已完成 ✅]
- [x] 2.2 Implement `remove_item(item_id, count)` with cross-slot removal [已完成 ✅]
- [x] 2.3 Implement `get_count(item_id)` to query total item count [已完成 ✅]

### 3. Implement activeSlot management
- [x] 3.1 Implement `set_active_slot(slot_index)` with range validation (0-9) [已完成 ✅]
- [x] 3.2 Implement `get_active_item()` to get currently held item [已完成 ✅]

### 4. Implement serialization/deserialization
- [x] 4.1 Implement `serialize()` to convert inventory to dict [已完成 ✅]
- [x] 4.2 Implement `deserialize(data)` class method to restore from dict [已完成 ✅]
- [x] 4.3 Handle legacy save compatibility (missing inventory field) [已完成 ✅]

### 5. Implement initial items for new players
- [x] 5.1 Add default items: slot[0]=axe x1, slot[1]=hoe x1, slot[2]=seeds x5, slot[3]=bread x3 [已完成 ✅]

### 6. Integration with item_registry
- [x] 6.1 Use item_registry.get_item_def() for max_stack values [已完成 ✅]
- [x] 6.2 Validate item_id exists in registry before adding [已完成 ✅]

### 7. Write unit tests
- [x] 7.1 Create `scripts/client/test_inventory.py` [已完成 ✅]
- [x] 7.2 Test add_item with stacking and overflow [已完成 ✅]
- [x] 7.3 Test remove_item with cross-slot removal [已完成 ✅]
- [x] 7.4 Test get_count queries [已完成 ✅]
- [x] 7.5 Test activeSlot management [已完成 ✅]
- [x] 7.6 Test serialization/deserialization [已完成 ✅]
- [x] 7.7 Test initial items for new players [已完成 ✅]
