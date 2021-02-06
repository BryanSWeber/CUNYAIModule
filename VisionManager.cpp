#pragma once
// Remember not to use "Broodwar" in any global class constructor!
# include "Source\CUNYAIModule.h"
# include <BWAPI.h>

int CUNYAIModule::Vision_Count() {
    int map_x = BWAPI::Broodwar->mapWidth();
    int map_y = BWAPI::Broodwar->mapHeight();

    int map_area = map_x * map_y; // map area in tiles.
    int vision_tile_count = 1; // starting at 1 to avoid /0 issues. Should be profoundly rare and vision is usually in the thousands anyway.
    for (int tile_x = 1; tile_x <= map_x; tile_x++) { // there is no tile (0,0)
        for (int tile_y = 1; tile_y <= map_y; tile_y++) {
            if (BWAPI::Broodwar->isVisible(tile_x, tile_y)) {
                vision_tile_count += 1;
            }
        }
    } // this search must be very exhaustive to do every frame. But C++ does it without any problems.
    return vision_tile_count;
}

Position CUNYAIModule::getUnitCenter(StoredUnit unit)
{
    return Position(unit.pos_.x + unit.type_.dimensionLeft(), unit.pos_.y + unit.type_.dimensionUp());
}

Position CUNYAIModule::getUnitCenter(Stored_Resource Resource)
{
    return Position(Resource.pos_.x + Resource.type_.dimensionLeft(), Resource.pos_.y + Resource.type_.dimensionUp());
}

