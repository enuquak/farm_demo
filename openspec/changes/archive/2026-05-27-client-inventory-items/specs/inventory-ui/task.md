# inventory-ui Task List

## 1. Add Inventory Message IDs and Protobuf Messages [已完成 ✅]
- [x] Add MSG_ID_INVENTORY_SYNC and MSG_ID_ACTIVE_SLOT_CHANGE to msg_ids.py
- [x] Add InventorySlot and InventorySync messages to player.proto
- [x] Regenerate protobuf Python code

## 2. Create Item Icon Data and IconManager [已完成 ✅]
- [x] Define 8x8 pixel icon palette data for all 7 item types in constants.py
- [x] Create scripts/client/icon_manager.py with IconManager class
- [x] Implement palette-to-Surface generation (8x8 -> 32x32 scaling)
- [x] Implement icon caching (shared Surface instances)

## 3. Implement Hotbar UI (HotbarRenderer) [已完成 ✅]
- [x] Create scripts/client/ui/hotbar.py
- [x] Draw 10 slots at screen bottom center (TILE_SIZE x TILE_SIZE each)
- [x] Render item icons and quantity text in each slot
- [x] Render active slot highlight (white/yellow border)
- [x] Render empty slot borders

## 4. Implement Inventory Panel (InventoryPanel) [已完成 ✅]
- [x] Create scripts/client/ui/inventory_panel.py
- [x] Draw 10x3 grid layout centered on screen
- [x] Row 1 = hotbar (10 slots), rows 2-3 = extended (20 slots)
- [x] Render title "背包" at top
- [x] Render close button [x] at top-right corner
- [x] Click close button to close panel

## 5. Implement Number Key Switching [已完成 ✅]
- [x] In GameScene, handle hotbar_1 through hotbar_0 actions
- [x] Update activeSlot in Inventory on key press
- [x] Send ActiveSlotChange message to server

## 6. Implement E Key Toggle and UI Blocking [已完成 ✅]
- [x] Add open_inventory action handling in GameScene (E key)
- [x] Toggle inventory panel open/close state
- [x] Set InputManager ui_blocking when panel is open
- [x] Block WASD movement, space interaction, mouse click interaction when panel is open

## 7. Integrate InventorySync Message Handling [已完成 ✅]
- [x] Register MSG_ID_INVENTORY_SYNC handler in GameScene
- [x] Parse InventorySync and update local Inventory data
- [x] Refresh hotbar and panel display on data change
- [x] Only update changed slots for performance optimization

## 8. Integrate UI into GameScene Render Loop [已完成 ✅]
- [x] Initialize HotbarRenderer and InventoryPanel in GameScene
- [x] Render hotbar overlay after game world (always visible)
- [x] Render inventory panel overlay when open
- [x] Wire up Inventory data to UI renderers
