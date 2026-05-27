# CropSystem Implementation Tasks

## 1. Create crop_system.h [已完成 ✅]
- Define `CropData` struct (planted_at, grow_time)
- Define `CropSystem` class with growing_tiles map
- Methods: register_crop, update, simulate_elapsed, serialize, deserialize, clear

## 2. Create crop_system.cpp [已完成 ✅]
- Implement register_crop with TILLED ground validation
- Implement update to check maturity and transition CROP_GROWING -> CROP_READY
- Implement simulate_elapsed for scene freeze/resume catch-up
- Implement serialize to `{"tiles": {"x,y": {"planted_at": ..., "grow_time": 60}}}` format
- Implement deserialize with old save compatibility (empty crops if no data)
- Implement cleanup of invalid tiles (where object is no longer CROP_GROWING)

## 3. Modify item_interaction_handler.h/cpp [已完成 ✅]
- Remove local CropData struct and crops_ map
- Add CropSystem pointer dependency
- Delegate crop registration to CropSystem in execute_effect
- Delegate crop update to CropSystem in update()
- Call CropSystem cleanup after harvest_crop

## 4. Modify game_server.h/cpp [已完成 ✅]
- Add CropSystem member
- Initialize CropSystem in constructor
- Wire up scene freeze/resume support

## 5. Update CMakeLists.txt [已完成 ✅]
- Add crop_system.cpp to SOURCES

## 6. Compile and verify [已完成 ✅]
- Build with cmake --build
- Build succeeded: game_server.exe (1175552 bytes)
