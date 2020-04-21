#pragma once

#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\MapInventory.h"
#include "Source\Unit_Inventory.h"
#include "Source/Diagnostics.h"
#include "Source\Resource_Inventory.h"
#include "Source/BaseManager.h"
#include <algorithm>
#include <fstream>
#include <ostream>
#include <set>
#include <tuple>

using namespace std;

// Creates a Inventory Object
MapInventory::MapInventory() {};
MapInventory::MapInventory(const Unit_Inventory &ui, const Resource_Inventory &ri) {

    updateVision_Count();

    updateLn_Supply_Remain();
    updateLn_Supply_Total();

    updateHatcheries();

    //Fields:
    vector< vector<int> > pf_threat_;
    vector< vector<int> > pf_attract_;
    vector< vector<int> > pf_aa_;
    vector< vector<int> > pf_explore_;

    //if ( smoothed_barriers_.size() == 0 ) {

    //    updateSmoothPos();
    //    int unwalkable_ct = 0;
    //    for ( vector<int>::size_type i = 0; i != smoothed_barriers_.size(); ++i ) {
    //        for ( vector<int>::size_type j = 0; j != smoothed_barriers_[i].size(); ++j ) {
    //            unwalkable_ct += smoothed_barriers_[i][j];
    //        }
    //    }
    //    Broodwar->sendText( "There are %d tiles, and %d smoothed out tiles.", smoothed_barriers_.size(), unwalkable_ct );
    //}

    if (unwalkable_barriers_.size() == 0) {
        updateUnwalkable();
    }

    if (ri.resource_inventory_.size() == 0) {
        updateBuildablePos();
        int buildable_ct = 0;
        for (vector<int>::size_type i = 0; i != buildable_positions_.size(); ++i) {
            for (vector<int>::size_type j = 0; j != buildable_positions_[i].size(); ++j) {
                buildable_ct += buildable_positions_[i][j];
            }
        }
        Diagnostics::DiagnosticText("There are %d resources on the map, %d canidate expo positions.", ri.resource_inventory_.size(), buildable_ct);
    }

    if (map_veins_.size() == 0) {
        updateMapVeins();
        int vein_ct = 0;
        for (vector<int>::size_type i = 0; i != map_veins_.size(); ++i) {
            for (vector<int>::size_type j = 0; j != map_veins_[i].size(); ++j) {
                if (map_veins_[i][j] > 10) {
                    ++vein_ct;
                }
            }
        }
        Diagnostics::DiagnosticText("There are %d roughly tiles, %d veins.", map_veins_.size(), vein_ct);
    }
}

//Marks Data for each area if it is "ground safe"
void MapInventory::updateGroundDangerousAreas()
{
    for (auto area : BWEM::Map::Instance().Areas()) {
        area.SetData(CUNYAIModule::checkDangerousArea(UnitTypes::Zerg_Drone, area.Id() ));
    }
}
;



// Updates the (safe) log of our supply stock. Looks specifically at our morphing units as "available".
void MapInventory::updateLn_Supply_Remain() {

    int total = Broodwar->self()->supplyTotal() - Broodwar->self()->supplyUsed();

    if (total <= 0) {
        total = 1;
    } // no log 0 under my watch!.

    ln_supply_remain_ = log(total);
};

// Updates the (safe) log of our consumed supply total.
void MapInventory::updateLn_Supply_Total() {

    double total = Broodwar->self()->supplyTotal();
    if (total <= 0) {
        total = 1;
    } // no log 0 under my watch!.

    ln_supply_total_ = log(total);
};


// Updates the (safe) log of our gas total. Returns very high int instead of infinity.
double MapInventory::getGasRatio() {
    // Normally:
    if (Broodwar->self()->minerals() > 0 || Broodwar->self()->gas() > 0) {
        return static_cast<double>(Broodwar->self()->gas()) / static_cast<double>(Broodwar->self()->minerals() + Broodwar->self()->gas());
    }
    else {
        return 99999;
    } // in the alternative case, you have nothing - you're mineral starved, you need minerals, not gas. Define as ~~infty, not 0.
};

// Updates the (safe) log of our supply total. Returns very high int instead of infinity.
double MapInventory::getLn_Supply_Ratio() {
    // Normally:
    if (ln_supply_total_ > 0) {
        return ln_supply_remain_ / ln_supply_total_;
    }
    else {
        return 0;
    } // in the alternative case, you have nothing - you're supply starved. Probably dead, too. Just in case- Define as ~~infty, not 0.
};

// Updates the count of our vision total, in tiles
void MapInventory::updateVision_Count() {
    int map_x = BWAPI::Broodwar->mapWidth();
    int map_y = BWAPI::Broodwar->mapHeight();

    int map_area = map_x * map_y; // map area in tiles.
    int total_tiles = 0;
    for (int tile_x = 1; tile_x <= map_x; tile_x++) { // there is no tile (0,0)
        for (int tile_y = 1; tile_y <= map_y; tile_y++) {
            if (BWAPI::Broodwar->isVisible(tile_x, tile_y)) {
                total_tiles += 1;
            }
        }
    } // this search must be very exhaustive to do every frame. But C++ does it without any problems.

    if (total_tiles == 0) {
        total_tiles = 1;
    } // catch some odd case where you are dead anyway. Rather not crash.
    vision_tile_count_ = total_tiles;
}

void MapInventory::updateScreen_Position()
{
    screen_position_ = Broodwar->getScreenPosition();
}

// Updates the number of hatcheries (and decendent buildings).
void MapInventory::updateHatcheries() {
    hatches_ = CUNYAIModule::countUnits(UnitTypes::Zerg_Hatchery) +
        CUNYAIModule::countUnits(UnitTypes::Zerg_Lair) +
        CUNYAIModule::countUnits(UnitTypes::Zerg_Hive);
}


//In Tiles?
void MapInventory::updateBuildablePos()
{
    int map_x = Broodwar->mapWidth();
    int map_y = Broodwar->mapHeight();
    buildable_positions_.reserve(map_x);
    for (int x = 0; x <= map_x; ++x) {
        vector<bool> temp;
        temp.reserve(map_y);
        for (int y = 0; y <= map_y; ++y) {
            temp.push_back(Broodwar->isBuildable(x, y));
        }
        buildable_positions_.push_back(temp);
    }
};

void MapInventory::updateUnwalkable() {
    int map_x = Broodwar->mapWidth() * 4;
    int map_y = Broodwar->mapHeight() * 4; //tile positions are 32x32, walkable checks 8x8 minitiles.

    unwalkable_barriers_.reserve(map_x);
    // first, define matrixes to recieve the walkable locations for every minitile.
    for (int x = 0; x <= map_x; ++x) {
        vector<int> temp;
        temp.reserve(map_y);
        for (int y = 0; y <= map_y; ++y) {
            temp.push_back(!Broodwar->isWalkable(x, y));
        }
        unwalkable_barriers_.push_back(temp);
    }

    unwalkable_barriers_with_buildings_ = unwalkable_barriers_; // preparing for the dependencies.
}

void MapInventory::updateSmoothPos() {
    int map_x = Broodwar->mapWidth() * 4;
    int map_y = Broodwar->mapHeight() * 4; //tile positions are 32x32, walkable checks 8x8 minitiles.
    int choke_score = 0;
    bool changed_a_value_last_cycle;

    // first, define matrixes to recieve the walkable locations for every minitile.
    smoothed_barriers_ = unwalkable_barriers_;

    for (auto iter = 2; iter < 16; iter++) { // iteration 1 is already done by labling unwalkables. Smoothout any dangerous tiles.
        changed_a_value_last_cycle = false;
        for (int minitile_x = 1; minitile_x <= map_x; ++minitile_x) {
            for (int minitile_y = 1; minitile_y <= map_y; ++minitile_y) { // Check all possible walkable locations.

                 // Psudocode: if any two opposing points are unwalkable, or the corners are blocked off, while an alternative path through the center is walkable, it can be smoothed out, the fewer cycles it takes to identify this, the rougher the surface.
                 // Repeat untill finished.

                if (smoothed_barriers_[minitile_x][minitile_y] == 0) { // if it is walkable, consider it a canidate for a choke.
                    // Predefine grid we will search over.
                    bool local_grid[3][3]; // WAY BETTER!

                    local_grid[0][0] = (smoothed_barriers_[(minitile_x - 1)][(minitile_y - 1)] < iter && smoothed_barriers_[(minitile_x - 1)][(minitile_y - 1)] > 0);
                    local_grid[0][1] = (smoothed_barriers_[(minitile_x - 1)][minitile_y] < iter && smoothed_barriers_[(minitile_x - 1)][minitile_y] > 0);
                    local_grid[0][2] = (smoothed_barriers_[(minitile_x - 1)][(minitile_y + 1)] < iter && smoothed_barriers_[(minitile_x - 1)][(minitile_y + 1)] > 0);

                    local_grid[1][0] = (smoothed_barriers_[minitile_x][(minitile_y - 1)] < iter && smoothed_barriers_[minitile_x][(minitile_y - 1)] > 0);
                    local_grid[1][1] = (smoothed_barriers_[minitile_x][minitile_y] < iter && smoothed_barriers_[minitile_x][minitile_y] > 0);
                    local_grid[1][2] = (smoothed_barriers_[minitile_x][(minitile_y + 1)] < iter && smoothed_barriers_[minitile_x][(minitile_y + 1)] > 0);

                    local_grid[2][0] = (smoothed_barriers_[(minitile_x + 1)][(minitile_y - 1)] < iter && smoothed_barriers_[(minitile_x + 1)][(minitile_y - 1)] > 0);
                    local_grid[2][1] = (smoothed_barriers_[(minitile_x + 1)][minitile_y] < iter && smoothed_barriers_[(minitile_x + 1)][minitile_y] > 0);
                    local_grid[2][2] = (smoothed_barriers_[(minitile_x + 1)][(minitile_y + 1)] < iter && smoothed_barriers_[(minitile_x + 1)][(minitile_y + 1)] > 0);

                    // if it is surrounded, it is probably a choke, with weight inversely proportional to the number of cycles we have taken this on.
                    bool opposing_tiles =
                        (local_grid[0][0] && (local_grid[2][2] || local_grid[2][1] || local_grid[1][2])) ||
                        (local_grid[1][0] && (local_grid[1][2] || local_grid[0][2] || local_grid[2][2])) ||
                        (local_grid[2][0] && (local_grid[0][2] || local_grid[0][1] || local_grid[1][2])) ||
                        (local_grid[0][1] && (local_grid[2][1] || local_grid[2][0] || local_grid[2][2])) ||
                        (local_grid[0][2] && (local_grid[1][0] || local_grid[2][0] || local_grid[2][1]));
                    //(local_grid[1][2] && (local_grid[0][0] || local_grid[1][0] || local_grid[2][0])) || //
                    //(local_grid[2][1] && (local_grid[0][0] || local_grid[0][1] || local_grid[0][2])) || //
                    //(local_grid[2][2] && (local_grid[0][0] || local_grid[0][1] || local_grid[1][0])) ; // several of these checks are redundant!

                    bool open_path =
                        (!local_grid[0][0] && !local_grid[2][2]) ||
                        (!local_grid[1][0] && !local_grid[1][2]) ||
                        (!local_grid[2][0] && !local_grid[0][2]) ||
                        (!local_grid[0][1] && !local_grid[2][1]); // this is symmetrical, so we only have to do half.


                    changed_a_value_last_cycle = opposing_tiles || changed_a_value_last_cycle;
                    smoothed_barriers_[minitile_x][minitile_y] = opposing_tiles * (iter + open_path * (99 - 2 * iter));
                }
            }
        }
        if (changed_a_value_last_cycle == false) {
            return; // if we did nothing last cycle, we don't need to punish ourselves.
        }
    }
}

void MapInventory::updateMapVeins() {
    int map_x = Broodwar->mapWidth() * 4;
    int map_y = Broodwar->mapHeight() * 4; //tile positions are 32x32, walkable checks 8x8 minitiles.
    bool changed_a_value_last_cycle = false;

    // first, define matrixes to recieve the walkable locations for every minitile.
    map_veins_.clear();
    map_veins_ = unwalkable_barriers_with_buildings_;


    list<WalkPosition> needs_filling;
    for (int minitile_x = 0; minitile_x < map_x; ++minitile_x) {
        for (int minitile_y = 0; minitile_y < map_y; ++minitile_y) { // Check all possible walkable locations.
            if (map_veins_[minitile_x][minitile_y] == 0) {
                needs_filling.push_back({ minitile_x, minitile_y });// if it is walkable, consider it a canidate for a choke.
                                                                    // Predefine list we will search over.
            }
        }
    }

    vector<int> flattened_map_veins;
    for (int minitile_x = 0; minitile_x < map_x; ++minitile_x) {
        for (int minitile_y = 0; minitile_y < map_y; ++minitile_y) { // Check all possible walkable locations. Must cross over the WHOLE matrix. No sloppy bits.
            flattened_map_veins.push_back(map_veins_[minitile_x][minitile_y]);
        }
    }

    //x = k / map_y;
    //y = k- i*m or k % map_y;
    //k = x * map_y + y;

    for (int iter = 2; iter < 300; iter++) { // iteration 1 is already done by labling smoothed away.
        changed_a_value_last_cycle = false;
        for (list<WalkPosition>::iterator position_to_investigate = needs_filling.begin(); position_to_investigate != needs_filling.end();) { // not last element !
               // Psudocode: Mark every point touching a wall as n. Then, mark all minitiles touching those points as n+1.
               // Repeat untill finished.

            bool local_grid = false; // further faster since I no longer care about actually generating the veins.
            int minitile_x = position_to_investigate->x;
            int minitile_y = position_to_investigate->y;

            bool safety_check = minitile_x > 0 && minitile_y > 0 && minitile_x + 1 < map_x && minitile_y + 1 < map_y;
            local_grid = safety_check && (flattened_map_veins[(minitile_x - 1) * map_y + (minitile_y - 1)] == iter - 1 ||
                flattened_map_veins[(minitile_x - 1) * map_y + minitile_y] == iter - 1 ||
                flattened_map_veins[(minitile_x - 1) * map_y + (minitile_y + 1)] == iter - 1 ||
                flattened_map_veins[minitile_x       * map_y + (minitile_y - 1)] == iter - 1 ||
                flattened_map_veins[minitile_x       * map_y + (minitile_y + 1)] == iter - 1 ||
                flattened_map_veins[(minitile_x + 1) * map_y + (minitile_y - 1)] == iter - 1 ||
                flattened_map_veins[(minitile_x + 1) * map_y + minitile_y] == iter - 1 ||
                flattened_map_veins[(minitile_x + 1) * map_y + (minitile_y + 1)] == iter - 1);

            int new_value = local_grid * iter;
            changed_a_value_last_cycle = local_grid || changed_a_value_last_cycle;
            map_veins_[minitile_x][minitile_y] = new_value;
            flattened_map_veins[minitile_x * map_y + minitile_y] = new_value;  //should just unpack this at the end.

            if (local_grid) position_to_investigate = needs_filling.erase(position_to_investigate);
            else ++position_to_investigate;
            //if ( local_grid ) {
            //    std::swap(*position_to_investigate, needs_filling.back()); // note back  - last element vs end - iterator past last element!
            //    needs_filling.pop_back(); //std::erase preserves order and vectors are contiguous. Erase is then an O(n^2) operator.
            //}
            //else {
            //    ++position_to_investigate;
            //}
        }

        if (changed_a_value_last_cycle == false) {
            return; // if we did nothing last cycle, we don't need to punish ourselves.
        }

    }

}

//int MapInventory::getMapValue(const Position & pos, const vector<vector<int>>& map)
//{
//    WalkPosition startloc = WalkPosition(pos);
//    return map[startloc.x][startloc.y];
//}

int MapInventory::getFieldValue(const Position & pos, const vector<vector<int>>& field)
{
    TilePosition startloc = TilePosition(pos);
    return field[startloc.x][startloc.y];
}

//void MapInventory::updateMapVeinsOut(const Position &newCenter, Position &oldCenter, vector<vector<int>> &map, const bool &print) { //in progress.
//
//    int map_x = Broodwar->mapWidth() * 4;
//    int map_y = Broodwar->mapHeight() * 4; //tile positions are 32x32, walkable checks 8x8 minitiles.
//
//    if (WalkPosition(oldCenter) == WalkPosition(newCenter)) return;
//
//    oldCenter = newCenter; // Must update old center manually.
//    WalkPosition startloc = WalkPosition(newCenter);
//    std::stringstream ss;
//    ss << WalkPosition(newCenter);
//    string base = ss.str();
//
//    ifstream newVeins(CUNYAIModule::learned_plan.writeDirectory + Broodwar->mapFileName() + "Veins" + base + ".txt", ios_base::in);
//    if (!newVeins)         //The file does not exist, we need to write it.
//    {
//
//        map = unwalkable_barriers_;
//
//        int minitile_x, minitile_y, distance_right_x, distance_below_y;
//        minitile_x = startloc.x;
//        minitile_y = startloc.y;
//        distance_right_x = max(map_x - minitile_x, map_x);
//        distance_below_y = max(map_y - minitile_y, map_y);
//        int t = std::max(map_x + distance_right_x + distance_below_y, map_y + distance_right_x + distance_below_y);
//        //int maxI = t*t; // total number of spiral steps we have to make.
//        int total_squares_filled = 2; // If you start at 1 you will be implicitly marking certain squares as unwalkable. 1 is the short code for unwalkable.
//
//        vector <WalkPosition> fire_fill_queue;
//        vector <WalkPosition> fire_fill_queue_holder;
//
//        //begin with a flood fill.
//        map[minitile_x][minitile_y] = total_squares_filled;
//        fire_fill_queue.push_back({ minitile_x, minitile_y });
//
//        int minitile_x_temp = minitile_x;
//        int minitile_y_temp = minitile_y;
//        bool filled_a_square = false;
//
//        // let us fill the map- counting outward from our destination.
//        while (!fire_fill_queue.empty() || !fire_fill_queue_holder.empty()) {
//
//            filled_a_square = false;
//
//            while (!fire_fill_queue.empty()) { // this portion is now a flood fill, iteratively filling from its interior. Seems to be very close to fastest reasonable implementation. May be able to remove diagonals without issue.
//
//                minitile_x_temp = fire_fill_queue.begin()->x;
//                minitile_y_temp = fire_fill_queue.begin()->y;
//                fire_fill_queue.erase(fire_fill_queue.begin());
//
//                // north
//                if (minitile_y_temp + 1 < map_y && map[minitile_x_temp][minitile_y_temp + 1] == 0) {
//                    filled_a_square = true;
//                    map[minitile_x_temp][minitile_y_temp + 1] = total_squares_filled;
//                    fire_fill_queue_holder.push_back({ minitile_x_temp , minitile_y_temp + 1 });
//                }
//                // north east
//                if (minitile_y_temp + 1 < map_y && minitile_x_temp + 1 < map_x && map[minitile_x_temp + 1][minitile_y_temp + 1] == 0) {
//                    filled_a_square = true;
//                    map[minitile_x_temp + 1][minitile_y_temp + 1] = total_squares_filled;
//                    fire_fill_queue_holder.push_back({ minitile_x_temp + 1, minitile_y_temp + 1 });
//                }
//                // north west
//                if (minitile_y_temp + 1 < map_y && 0 < minitile_x_temp - 1 && map[minitile_x_temp - 1][minitile_y_temp + 1] == 0) {
//                    filled_a_square = true;
//                    map[minitile_x_temp - 1][minitile_y_temp + 1] = total_squares_filled;
//                    fire_fill_queue_holder.push_back({ minitile_x_temp - 1, minitile_y_temp + 1 });
//                }
//                //south
//                if (0 < minitile_y_temp - 1 && map[minitile_x_temp][minitile_y_temp - 1] == 0) {
//                    filled_a_square = true;
//                    map[minitile_x_temp][minitile_y_temp - 1] = total_squares_filled;
//                    fire_fill_queue_holder.push_back({ minitile_x_temp, minitile_y_temp - 1 });
//                }
//                //south east
//                if (0 < minitile_y_temp - 1 && minitile_x_temp + 1 < map_x && map[minitile_x_temp + 1][minitile_y_temp - 1] == 0) {
//                    filled_a_square = true;
//                    map[minitile_x_temp + 1][minitile_y_temp - 1] = total_squares_filled;
//                    fire_fill_queue_holder.push_back({ minitile_x_temp + 1, minitile_y_temp - 1 });
//                }
//                //south west
//                if (0 < minitile_y_temp - 1 && 0 < minitile_x_temp - 1 && map[minitile_x_temp - 1][minitile_y_temp - 1] == 0) {
//                    filled_a_square = true;
//                    map[minitile_x_temp - 1][minitile_y_temp - 1] = total_squares_filled;
//                    fire_fill_queue_holder.push_back({ minitile_x_temp - 1, minitile_y_temp - 1 });
//                }
//                // east
//                if (minitile_x_temp + 1 < map_x && map[minitile_x_temp + 1][minitile_y_temp] == 0) {
//                    filled_a_square = true;
//                    map[minitile_x_temp + 1][minitile_y_temp] = total_squares_filled;
//                    fire_fill_queue_holder.push_back({ minitile_x_temp + 1, minitile_y_temp });
//                }
//                //west
//                if (0 < minitile_x_temp - 1 && map[minitile_x_temp - 1][minitile_y_temp] == 0) {
//                    filled_a_square = true;
//                    map[minitile_x_temp - 1][minitile_y_temp] = total_squares_filled;
//                    fire_fill_queue_holder.push_back({ minitile_x_temp - 1, minitile_y_temp });
//                }
//            }
//            total_squares_filled += filled_a_square;
//            fire_fill_queue.clear();
//            fire_fill_queue.swap(fire_fill_queue_holder);
//            fire_fill_queue_holder.clear();
//        }
//
//        if (print) writeMap(map, WalkPosition(newCenter));
//    }
//    else
//    {
//        readMap(map, WalkPosition(newCenter));
//    }
//    newVeins.close();
//}

int MapInventory::getDistanceBetween(const Position A, const Position B) const
{
    int plength = 0;
    auto cpp = BWEM::Map::Instance().GetPath(A, B, &plength);
    if (A.isValid() && B.isValid() && plength > -1) {
        return plength;
    }

    return 9999999;
}


int MapInventory::getRadialDistanceOutFromEnemy(const Position A) const
{
    return getDistanceBetween(A, enemy_base_ground_);
}


bool MapInventory::checkViableGroundPath(const Position A, const Position B) const
{
    int plength = 0;
    auto cpp = BWEM::Map::Instance().GetPath(A, B, &plength);

    if (plength >= 0 && A.isValid() && B.isValid()) {
        return true;
    }
    return false;
}

bool MapInventory::isOnIsland(const Position A) const
{
    return checkViableGroundPath(A, safe_base_);
}

int MapInventory::getRadialDistanceOutFromHome(const Position A) const
{
    return getDistanceBetween(A, safe_base_);
}

//void MapInventory::updateLiveMapVeins( const Unit &building, const Unit_Inventory &ui, const Unit_Inventory &ei, const Resource_Inventory &ri ) { // in progress.
//    int map_x = Broodwar->mapWidth() * 4;
//    int map_y = Broodwar->mapHeight() * 4; //tile positions are 32x32, walkable checks 8x8 minitiles
//    int start_iter = 2;
//    // Predefine grid we will search over.
//    bool local_grid[3][3]; // WAY BETTER!
//
//    //modified areas stopping at bounds. bounds are 1 inside edge of map.
//    WalkPosition max_lower_right = WalkPosition( Position( building->getPosition().x + 2 * building->getType().tileWidth() * 32, building->getPosition().y + 2 * building->getType().tileHeight() * 32 ) );
//    WalkPosition max_upper_left =  WalkPosition( Position( building->getPosition().x -     building->getType().tileWidth() * 32, building->getPosition().y -     building->getType().tileHeight() * 32 ) );
//
//    WalkPosition lower_right_modified = WalkPosition( max_lower_right.x < map_x ? max_lower_right.x : map_x - 1, max_lower_right.y < map_y ? max_lower_right.y : map_y - 1 );
//    WalkPosition upper_left_modified = WalkPosition( max_upper_left.x > 0 ? max_upper_left.x : 1, max_upper_left.y > 0 ? max_upper_left.y : 1 );
//
//    // clear tiles that may have been altered.
//    for ( auto minitile_x = upper_left_modified.x; minitile_x <= lower_right_modified.x; ++minitile_x ) {
//        for ( auto minitile_y = upper_left_modified.y; minitile_y <= lower_right_modified.y; ++minitile_y ) { // Check all possible walkable locations.
//            if ( unwalkable_barriers_[minitile_x][minitile_y] == 0 ) {
//
//                if ( start_iter < map_veins_[minitile_x][minitile_y] - building->getType().tileWidth() * 4  ) {
//                     start_iter = map_veins_[minitile_x][minitile_y] - building->getType().tileWidth() * 4;
//                }
//
//                Position pos = Position( WalkPosition( minitile_x, minitile_y ) );
//                if ( CUNYAIModule::checkBuildingOccupiedArea( ui, pos ) || CUNYAIModule::checkBuildingOccupiedArea( ei, pos ) || CUNYAIModule::checkResourceOccupiedArea(ri,pos) ) {
//                    map_veins_[minitile_x][minitile_y] = 1;
//                }
//                else /*if ( CUNYAIModule::checkUnitOccupiesArea( building, pos, area_modified ) )*/ {
//                    map_veins_[minitile_x][minitile_y] = 0; // if it is nearby nuke it to 0 for recasting.
//                }
//            }
//        }
//    }
//
//    for ( auto iter = start_iter; iter < 175; iter++ ) { // iteration 1 is already done by labling smoothed barriers. Less loops are needed because most of the map is already plotted.
//       // bool changed_a_value_last_cycle = false;
//        for ( auto minitile_x = upper_left_modified.x; minitile_x <= lower_right_modified.x; ++minitile_x ) {
//            for ( auto minitile_y = upper_left_modified.y; minitile_y <= lower_right_modified.y; ++minitile_y ) { // Check all possible walkable locations.
//
//                                             //Psudocode: if any two opposing points are smoothed away, while an alternative path through the center is walkable, it is a choke, the fewer cycles it takes to identify this, the tigher the choke.
//                                             //    If any 3 points adjacent are smoothed away it is probably just a bad place to walk, dead end, etc.Mark it as smoothed away.Do not consider it smoothed away this cycle.
//                                             //    if any corner of it is inaccessable, it is a diagonal wall, mark it as smoothed away.Do not consider it smoothed away this cycle.
//                                             //        Repeat until finished.
//
//                Position pos = Position( WalkPosition( minitile_x, minitile_y ) );
//
//                if ( map_veins_[minitile_x][minitile_y] == 0 ) { // if it is walkable, consider it a canidate for a choke.
//
//                    local_grid[0][0] = (map_veins_[(minitile_x - 1)][(minitile_y - 1)] < iter && map_veins_[(minitile_x - 1)][(minitile_y - 1)] > 0 ); // if the edge is a barrier or has been counted.
//                    local_grid[1][0] = (map_veins_[minitile_x]      [(minitile_y - 1)] < iter && map_veins_[minitile_x]      [(minitile_y - 1)] > 0 );
//                    local_grid[2][0] = (map_veins_[(minitile_x + 1)][(minitile_y - 1)] < iter && map_veins_[(minitile_x + 1)][(minitile_y - 1)] > 0 );
//
//                    local_grid[0][1] = (map_veins_[(minitile_x - 1)][minitile_y] < iter && map_veins_[(minitile_x - 1)][minitile_y] > 0 );
//                    local_grid[1][1] = (map_veins_[minitile_x]      [minitile_y] < iter && map_veins_[minitile_x]      [minitile_y] > 0 );
//                    local_grid[2][1] = (map_veins_[(minitile_x + 1)][minitile_y] < iter && map_veins_[(minitile_x + 1)][minitile_y] > 0 );
//
//                    local_grid[0][2] = (map_veins_[(minitile_x - 1)][(minitile_y + 1)] < iter && map_veins_[(minitile_x - 1)][(minitile_y + 1)] > 0 );
//                    local_grid[1][2] = (map_veins_[minitile_x]      [(minitile_y + 1)] < iter && map_veins_[minitile_x]      [(minitile_y + 1)] > 0 );
//                    local_grid[2][2] = (map_veins_[(minitile_x + 1)][(minitile_y + 1)] < iter && map_veins_[(minitile_x + 1)][(minitile_y + 1)] > 0 );
//
//                    // if it is surrounded, it is probably a choke, with weight inversely proportional to the number of cycles we have taken this on.
//                    bool opposing_tiles =
//                        (local_grid[0][0] && (local_grid[2][2] || local_grid[2][1] || local_grid[1][2])) ||
//                        (local_grid[1][0] && (local_grid[1][2] || local_grid[0][2] || local_grid[2][2])) ||
//                        (local_grid[2][0] && (local_grid[0][2] || local_grid[0][1] || local_grid[1][2])) ||
//                        (local_grid[0][1] && (local_grid[2][1] || local_grid[2][0] || local_grid[2][2])) ||
//                        (local_grid[0][2] && (local_grid[2][0] || local_grid[1][0] || local_grid[2][1])) ||
//                        (local_grid[1][2] && (local_grid[1][0] || local_grid[0][0] || local_grid[2][0])) || //
//                        (local_grid[2][1] && (local_grid[0][1] || local_grid[0][0] || local_grid[0][2])) || //
//                        (local_grid[2][2] && (local_grid[0][0] || local_grid[0][1] || local_grid[1][0])); // all of them, just in case. //11 is not opposed by anythign?!
//
//                    bool open_path =
//                        (!local_grid[0][0] && !local_grid[2][2]) ||
//                        (!local_grid[1][0] && !local_grid[1][2]) ||
//                        (!local_grid[2][0] && !local_grid[0][2]) ||
//                        (!local_grid[0][1] && !local_grid[2][1]); // this is symmetrical, so we only have to do half.
//
//                    bool adjacent_tiles =
//                        local_grid[0][0] && local_grid[0][1] && local_grid[0][2] || // left edge
//                        local_grid[2][0] && local_grid[2][1] && local_grid[2][2] || // right edge
//                        local_grid[0][0] && local_grid[1][0] && local_grid[2][0] || // bottom edge
//                        local_grid[0][2] && local_grid[1][2] && local_grid[2][2] || // top edge
//                        local_grid[0][1] && local_grid[1][0] && local_grid[0][0] || // lower left slice.
//                        local_grid[0][1] && local_grid[1][2] && local_grid[0][2] || // upper left slice.
//                        local_grid[1][2] && local_grid[2][1] && local_grid[2][2] || // upper right slice.
//                        local_grid[1][0] && local_grid[2][1] && local_grid[2][0]; // lower right slice.
//
//                    if ( open_path && opposing_tiles ) {  //mark chokes when found.
//                        map_veins_[minitile_x][minitile_y] = 299 - iter;
//                       // changed_a_value_last_cycle = true;
//                    }
//                    else if ( (!open_path && opposing_tiles) || adjacent_tiles ) { //if it is closing off in any other way than "formal choke"- it's just a bad place to walk. Mark as unwalkable and continue. Will seal map.
//                        map_veins_[minitile_x][minitile_y] = iter;
//                       // changed_a_value_last_cycle = true;
//                    }
//                }
//            }
//        }
//
//        //if (changed_a_value_last_cycle == false) {
//        //    return; // if we did nothing last cycle, we don't need to punish ourselves.
//        //}
//    }
//}


// This function causes several items to break. In particular, building locations will end up being inside the unwalkable area!
void MapInventory::updateUnwalkableWithBuildings() {
    int map_x = Broodwar->mapWidth() * 4;
    int map_y = Broodwar->mapHeight() * 4; //tile positions are 32x32, walkable checks 8x8 minitiles.

    unwalkable_barriers_with_buildings_ = unwalkable_barriers_;

    //mark all occupied areas.  IAAUW

    for (auto & u : CUNYAIModule::friendly_player_model.units_.unit_map_) {
        if (u.second.type_.isBuilding()) {

            // mark the building's current position.
            int max_x = u.second.pos_.x + u.second.type_.dimensionLeft();
            int min_x = u.second.pos_.x - u.second.type_.dimensionRight();
            int max_y = u.second.pos_.y + u.second.type_.dimensionUp();
            int min_y = u.second.pos_.y - u.second.type_.dimensionDown();

            WalkPosition max_upper_left = WalkPosition(Position(min_x, min_y));
            WalkPosition max_lower_right = WalkPosition(Position(max_x, max_y));

            //respect map bounds please.
            WalkPosition lower_right_modified = WalkPosition(max_lower_right.x < map_x ? max_lower_right.x : map_x - 1, max_lower_right.y < map_y ? max_lower_right.y : map_y - 1);
            WalkPosition upper_left_modified = WalkPosition(max_upper_left.x > 0 ? max_upper_left.x : 1, max_upper_left.y > 0 ? max_upper_left.y : 1);

            for (auto minitile_x = upper_left_modified.x; minitile_x <= lower_right_modified.x; ++minitile_x) {
                for (auto minitile_y = upper_left_modified.y; minitile_y <= lower_right_modified.y; ++minitile_y) { // Check all possible walkable locations.
                    unwalkable_barriers_with_buildings_[minitile_x][minitile_y] = 1;
                }
            }
        }
    }

    for (auto & e : CUNYAIModule::enemy_player_model.units_.unit_map_) {
        if (e.second.type_.isBuilding()) {

            // mark the building's current position.
            int max_x = e.second.pos_.x + e.second.type_.dimensionLeft();
            int min_x = e.second.pos_.x - e.second.type_.dimensionRight();
            int max_y = e.second.pos_.y + e.second.type_.dimensionUp();
            int min_y = e.second.pos_.y - e.second.type_.dimensionDown();

            WalkPosition max_upper_left = WalkPosition(Position(min_x, min_y));
            WalkPosition max_lower_right = WalkPosition(Position(max_x, max_y));

            //respect map bounds please.
            WalkPosition lower_right_modified = WalkPosition(max_lower_right.x < map_x ? max_lower_right.x : map_x - 1, max_lower_right.y < map_y ? max_lower_right.y : map_y - 1);
            WalkPosition upper_left_modified = WalkPosition(max_upper_left.x > 0 ? max_upper_left.x : 1, max_upper_left.y > 0 ? max_upper_left.y : 1);

            for (auto minitile_x = upper_left_modified.x; minitile_x <= lower_right_modified.x; ++minitile_x) {
                for (auto minitile_y = upper_left_modified.y; minitile_y <= lower_right_modified.y; ++minitile_y) { // Check all possible walkable locations.
                    unwalkable_barriers_with_buildings_[minitile_x][minitile_y] = 1;
                }
            }
        }
    }

    for (auto & n : CUNYAIModule::neutral_player_model.units_.unit_map_) {
        if (n.second.type_.isBuilding()) {

            // mark the building's current position.
            int max_x = n.second.pos_.x + n.second.type_.dimensionLeft();
            int min_x = n.second.pos_.x - n.second.type_.dimensionRight();
            int max_y = n.second.pos_.y + n.second.type_.dimensionUp();
            int min_y = n.second.pos_.y - n.second.type_.dimensionDown();

            WalkPosition max_upper_left = WalkPosition(Position(min_x, min_y));
            WalkPosition max_lower_right = WalkPosition(Position(max_x, max_y));

            //respect map bounds please.
            WalkPosition lower_right_modified = WalkPosition(max_lower_right.x < map_x ? max_lower_right.x : map_x - 1, max_lower_right.y < map_y ? max_lower_right.y : map_y - 1);
            WalkPosition upper_left_modified = WalkPosition(max_upper_left.x > 0 ? max_upper_left.x : 1, max_upper_left.y > 0 ? max_upper_left.y : 1);

            for (auto minitile_x = upper_left_modified.x; minitile_x <= lower_right_modified.x; ++minitile_x) {
                for (auto minitile_y = upper_left_modified.y; minitile_y <= lower_right_modified.y; ++minitile_y) { // Check all possible walkable locations.
                    unwalkable_barriers_with_buildings_[minitile_x][minitile_y] = 1;
                }
            }
        }
    }

    for (auto & u : CUNYAIModule::land_inventory.resource_inventory_) {
        // mark the building's current position.
        int max_x = u.second.pos_.x + u.second.type_.dimensionLeft();
        int min_x = u.second.pos_.x - u.second.type_.dimensionRight();
        int max_y = u.second.pos_.y + u.second.type_.dimensionUp();
        int min_y = u.second.pos_.y - u.second.type_.dimensionDown();

        WalkPosition max_upper_left = WalkPosition(Position(min_x, min_y));
        WalkPosition max_lower_right = WalkPosition(Position(max_x, max_y));

        //respect map bounds please.
        WalkPosition lower_right_modified = WalkPosition(max_lower_right.x < map_x ? max_lower_right.x : map_x - 1, max_lower_right.y < map_y ? max_lower_right.y : map_y - 1);
        WalkPosition upper_left_modified = WalkPosition(max_upper_left.x > 0 ? max_upper_left.x : 1, max_upper_left.y > 0 ? max_upper_left.y : 1);

        for (auto minitile_x = upper_left_modified.x; minitile_x <= lower_right_modified.x; ++minitile_x) {
            for (auto minitile_y = upper_left_modified.y; minitile_y <= lower_right_modified.y; ++minitile_y) { // Check all possible walkable locations.
                unwalkable_barriers_with_buildings_[minitile_x][minitile_y] = 1;
            }
        }

    }

}

//void MapInventory::updateLiveMapVeins(const Unit_Inventory &ui, const Unit_Inventory &ei, const Resource_Inventory &ri) { // in progress.
//
//    int map_x = Broodwar->mapWidth() * 4;
//    int map_y = Broodwar->mapHeight() * 4; //tile positions are 32x32, walkable checks 8x8 minitiles.
//    int test_bool = 0;
//                                           // Predefine grid we will search over.
//    bool local_grid[3][3]; // WAY BETTER!
//    map_veins_ = unwalkable_barriers_;
//
//    for (auto iter = 2; iter < 175; iter++) { // iteration 1 is already done by labling smoothed away.
//        bool changed_a_value_last_cycle = false;
//        for (auto minitile_x = 1; minitile_x <= map_x; ++minitile_x) {
//            for (auto minitile_y = 1; minitile_y <= map_y; ++minitile_y) { // Check all possible walkable locations.
//
//                                                                           // Psudocode: if any two opposing points are smoothed away, while an alternative path through the center is walkable, it is a choke, the fewer cycles it takes to identify this, the tigher the choke.
//                                                                           // If any 3 points adjacent are smoothed away it is probably just a bad place to walk, dead end, etc. Mark it as smoothed away.  Do not consider it smoothed away this cycle.
//                                                                           // if any corner of it is inaccessable, it is a diagonal wall, mark it as smoothed away. Do not consider it smoothed away this cycle.
//                                                                           // Repeat untill finished.
//
//                if (map_veins_[minitile_x][minitile_y] == 0) { // if it is walkable, consider it a canidate for a choke.
//
//                    changed_a_value_last_cycle = true;
//
//                    local_grid[0][0] = (map_veins_[(minitile_x - 1)][(minitile_y - 1)] < iter && map_veins_[(minitile_x - 1)][(minitile_y - 1)] > 0);
//                    local_grid[1][0] = (map_veins_[minitile_x][(minitile_y - 1)] < iter && map_veins_[minitile_x][(minitile_y - 1)] > 0);
//                    local_grid[2][0] = (map_veins_[(minitile_x + 1)][(minitile_y - 1)] < iter && map_veins_[(minitile_x + 1)][(minitile_y - 1)] > 0);
//
//                    local_grid[0][1] = (map_veins_[(minitile_x - 1)][minitile_y] < iter  && map_veins_[(minitile_x - 1)][minitile_y] > 0);
//                    local_grid[1][1] = (map_veins_[minitile_x][minitile_y] < iter  && map_veins_[minitile_x][minitile_y] > 0);
//                    local_grid[2][1] = (map_veins_[(minitile_x + 1)][minitile_y] < iter  && map_veins_[(minitile_x + 1)][minitile_y] > 0);
//
//                    local_grid[0][2] = (map_veins_[(minitile_x - 1)][(minitile_y + 1)] < iter  && map_veins_[(minitile_x - 1)][(minitile_y + 1)] > 0);
//                    local_grid[1][2] = (map_veins_[minitile_x][(minitile_y + 1)] < iter  && map_veins_[minitile_x][(minitile_y + 1)] > 0);
//                    local_grid[2][2] = (map_veins_[(minitile_x + 1)][(minitile_y + 1)] < iter  && map_veins_[(minitile_x + 1)][(minitile_y + 1)] > 0);
//
//                    // if it is surrounded, it is probably a choke, with weight inversely proportional to the number of cycles we have taken this on.
//                    bool opposing_tiles =
//                        (local_grid[0][0] && (local_grid[2][2] || local_grid[2][1] || local_grid[1][2])) ||
//                        (local_grid[1][0] && (local_grid[1][2] || local_grid[0][2] || local_grid[2][2])) ||
//                        (local_grid[2][0] && (local_grid[0][2] || local_grid[0][1] || local_grid[1][2])) ||
//                        (local_grid[0][1] && (local_grid[2][1] || local_grid[2][0] || local_grid[2][2])) ||
//                        (local_grid[0][2] && (local_grid[1][0] || local_grid[2][0] || local_grid[2][1])) ||
//                        (local_grid[1][2] && (local_grid[0][0] || local_grid[1][0] || local_grid[2][0])) || //
//                        (local_grid[2][1] && (local_grid[0][0] || local_grid[0][1] || local_grid[0][2])) || //
//                        (local_grid[2][2] && (local_grid[0][0] || local_grid[0][1] || local_grid[1][0])); // all of them, just in case. //11 is not opposed by anythign?!
//
//                    bool open_path =
//                        (!local_grid[0][0] && !local_grid[2][2]) ||
//                        (!local_grid[1][0] && !local_grid[1][2]) ||
//                        (!local_grid[2][0] && !local_grid[0][2]) ||
//                        (!local_grid[0][1] && !local_grid[2][1]); // this is symmetrical, so we only have to do half.
//
//                    bool adjacent_tiles =
//                        local_grid[0][0] && local_grid[0][1] && local_grid[0][2] || // left edge
//                        local_grid[2][0] && local_grid[2][1] && local_grid[2][2] || // right edge
//                        local_grid[0][0] && local_grid[1][0] && local_grid[2][0] || // bottom edge
//                        local_grid[0][2] && local_grid[1][2] && local_grid[2][2] || // top edge
//                        local_grid[0][1] && local_grid[1][0] && local_grid[0][0] || // lower left slice.
//                        local_grid[0][1] && local_grid[1][2] && local_grid[0][2] || // upper left slice.
//                        local_grid[1][2] && local_grid[2][1] && local_grid[2][2] || // upper right slice.
//                        local_grid[1][0] && local_grid[2][1] && local_grid[2][0]; // lower right slice.
//
//                    if (open_path && opposing_tiles) {  //mark chokes when found.
//                        map_veins_[minitile_x][minitile_y] = 299 - iter;
//                    }
//                    else if ((!open_path && opposing_tiles) || adjacent_tiles) { //if it is closing off in any other way than "formal choke"- it's just a bad place to walk. Mark as smoothed away and continue. Will seal map.
//                        map_veins_[minitile_x][minitile_y] = iter;
//                    }
//                }
//            }
//        }
//
//        if (changed_a_value_last_cycle == false) {
//            return; // if we did nothing last cycle, we don't need to punish ourselves.
//        }
//    }
//}

//void MapInventory::updateMapChokes() { // in progress. Idea : A choke is if the maximum variation of ground distances in a 5x5 tile square is LESS than some threshold. It is a plane if it is GREATER than some threshold.
//    int map_x = Broodwar->mapWidth() * 4;
//    int map_y = Broodwar->mapHeight() * 4; //tile positions are 32x32, walkable checks 8x8 minitiles.
//    WalkPosition map_dim = WalkPosition(TilePosition({ Broodwar->mapWidth(), Broodwar->mapHeight() }));
//    int current_region_number = 1;
//    int count_of_adjacent_importent_points = 0;
//    int temp_x = 0;
//    int temp_y = 0;
//    int local_tiles[9];
//
//    // first, define matrixes to recieve the walkable locations for every minitile.
//    //map_chokes_.reserve(map_x);
//    //for (int x = 0; x <= map_x; ++x) {
//    //    vector<int> temp;
//    //    temp.reserve(map_y);
//    //    for (int y = 0; y <= map_y; ++y) {
//    //        temp.push_back(unwalkable_barriers_[x][y] > 0);  //Was that location smoothed out?
//    //    }
//    //    map_chokes_.push_back(temp);
//    //}
//
//    map_chokes_ = unwalkable_barriers_;
//
//    for (int temp_x = 1; temp_x < map_x; temp_x++) {
//          for (int temp_y = 1; temp_y < map_y; temp_y++) {
//            if (smoothed_barriers_[temp_x][temp_y] == 0 ) { // if it is walkable, consider it a canidate for a choke. Need a buffer around 0,0 and the edge
//                int observed_difference = 0;
//
//                local_tiles[0] = map_out_from_home_[(temp_x - 1)][(temp_y - 1)];
//                local_tiles[1] = map_out_from_home_[temp_x]      [(temp_y - 1)];
//                local_tiles[2] = map_out_from_home_[(temp_x + 1)][(temp_y - 1)];
//
//                local_tiles[3] = map_out_from_home_[(temp_x - 1)][temp_y];
//                local_tiles[4] = map_out_from_home_[temp_x]      [temp_y]; // middle value, local_tiles[4], index starts at 0.
//                local_tiles[5] = map_out_from_home_[(temp_x + 1)][temp_y];
//
//                local_tiles[6] = map_out_from_home_[(temp_x - 1)][(temp_y + 1)];
//                local_tiles[7] = map_out_from_home_[temp_x]      [(temp_y + 1)];
//                local_tiles[8] = map_out_from_home_[(temp_x + 1)][(temp_y + 1)];
//
//                for (auto iterated_value : local_tiles) {
//                    if ( abs(iterated_value - local_tiles[4]) > observed_difference && iterated_value > 1) {
//                        observed_difference = abs(iterated_value - local_tiles[4]);
//                    }
//
//                }
//
//                if (observed_difference < local_tiles[4]) { // something.
//                    map_chokes_[temp_x][temp_y] = observed_difference;
//                }
//            }
//        }
//    }
//
//    //vector <WalkPosition> fire_fill_queue;
//
//    //fire_fill_queue.push_back(WalkPosition(this->home_base_));
//
//    //int minitile_x_temp = WalkPosition(this->home_base_).x;
//    //int minitile_y_temp = WalkPosition(this->home_base_).y;
//
//    //while (current_ceiling > 0) {
//    //    while (!fire_fill_queue.empty()) { // this portion is a fire fill.
//
//    //        minitile_x_temp = fire_fill_queue.begin()->x;
//    //        minitile_y_temp = fire_fill_queue.begin()->y;
//    //        fire_fill_queue.erase(fire_fill_queue.begin());
//
//    //        map_chokes_[minitile_x_temp][minitile_y_temp] = current_region_number;
//
//    //        // north
//    //        if (count_of_adjacent_importent_points < 5 && minitile_y_temp + 1 < map_y && map_veins_[minitile_x_temp][minitile_y_temp++] > current_ceiling) {
//    //            count_of_adjacent_importent_points++;
//    //            fire_fill_queue.push_back({ minitile_x_temp , minitile_y_temp++});
//    //        }
//    //        // north east
//    //        if (count_of_adjacent_importent_points < 5 && minitile_x_temp + 1 < map_x && minitile_y_temp + 1 < map_y && map_veins_[minitile_x_temp++][minitile_y_temp++] > current_ceiling) {
//    //            count_of_adjacent_importent_points++;
//    //            fire_fill_queue.push_back({ minitile_x_temp++ , minitile_y_temp++ });
//    //        }
//    //        // north west
//    //        if (count_of_adjacent_importent_points < 5 && 0 < minitile_x_temp - 1 && minitile_y_temp + 1 < map_y && map_veins_[minitile_x_temp--][minitile_y_temp++] > current_ceiling) {
//    //            count_of_adjacent_importent_points++;
//    //            fire_fill_queue.push_back({ minitile_x_temp-- , minitile_y_temp++ });
//
//    //        }
//    //        //south east
//    //        if (count_of_adjacent_importent_points < 5 && minitile_x_temp + 1 < map_x && 0 < minitile_y_temp - 1 && map_veins_[minitile_x_temp++][minitile_y_temp--] > current_ceiling) {
//    //            count_of_adjacent_importent_points++;
//    //            fire_fill_queue.push_back({ minitile_x_temp++ , minitile_y_temp-- });
//    //        }
//    //        //south west
//    //        if (count_of_adjacent_importent_points < 5 && 0 < minitile_x_temp - 1 && 0 < minitile_y_temp - 1 && map_veins_[minitile_x_temp--][minitile_y_temp++] > current_ceiling) {
//    //            count_of_adjacent_importent_points++;
//    //            fire_fill_queue.push_back({ minitile_x_temp-- , minitile_y_temp++ });
//
//    //        }
//    //        // east
//    //        if (count_of_adjacent_importent_points < 5 && minitile_x_temp + 1 < map_x && map_veins_[minitile_x_temp++][minitile_y_temp] > current_ceiling) {
//    //            count_of_adjacent_importent_points++;
//    //            fire_fill_queue.push_back({ minitile_x_temp++ , minitile_y_temp });
//    //        }
//    //        //west
//    //        if (count_of_adjacent_importent_points < 5 && 0 < minitile_x_temp - 1 && map_veins_[minitile_x_temp--][minitile_y_temp] > 0) {
//    //            count_of_adjacent_importent_points++;
//    //            fire_fill_queue.push_back({ minitile_x_temp--, minitile_y_temp });
//    //        }
//
//    //        // Make a final decision about the point.
//    //        if (count_of_adjacent_importent_points >= 5 && map_veins_[minitile_x_temp][minitile_y_temp] > 0 ) {
//    //            fire_fill_queue.push_back({ minitile_x_temp , minitile_y_temp });
//    //            count_of_adjacent_importent_points = 0;
//    //        }
//    //        else if( map_veins_[minitile_x_temp][minitile_y_temp] > 0 ){
//    //            fire_fill_queue.push_back({ minitile_x_temp , minitile_y_temp });
//    //            count_of_adjacent_importent_points = 0;
//    //        }
//    //    }
//    //    current_ceiling--;
//    //    current_region_number++;
//    //}
//}
//

//Position MapInventory::getBaseWithMostCausalties(const bool &friendly, const bool &fodder) const
//{
//    Position weakest_base = Positions::Origin;
//    int current_best_damage = 0; // damage must be bigger than 0 or else it's not really a base.
//    int sample_damage = 0;
//    int sample_ground_fodder = 0;
//
//    for (auto expo : expo_positions_complete_) {
//        Unit_Inventory ei_loc = CUNYAIModule::getUnitInventoryInNeighborhood(CUNYAIModule::enemy_player_model.units_, Position(expo));
//        Unit_Inventory ui_loc = CUNYAIModule::getUnitInventoryInNeighborhood(CUNYAIModule::friendly_player_model.units_, Position(expo));
//        Unit_Inventory ui_mini = CUNYAIModule::getUnitInventoryInArea(CUNYAIModule::friendly_player_model.units_, Position(expo));
//        Unit_Inventory ei_mini = CUNYAIModule::getUnitInventoryInArea(CUNYAIModule::enemy_player_model.units_, Position(expo));
//
//        ei_loc.updateUnitInventorySummary();
//        ei_mini.updateUnitInventorySummary();
//        ui_loc.updateUnitInventorySummary();
//        ui_mini.updateUnitInventorySummary();
//
//        if (friendly) {
//            sample_damage = CUNYAIModule::getFAPDamageForecast(ui_loc, ei_loc, fodder);
//            sample_ground_fodder = ui_mini.stock_ground_fodder_;
//        }
//        else {
//            sample_damage = CUNYAIModule::getFAPDamageForecast(ei_loc, ui_loc, fodder);
//            sample_ground_fodder = ei_mini.stock_ground_fodder_;
//        }
//
//
//        if (sample_damage > current_best_damage && sample_ground_fodder > 0) { // let's go to the place that is having the most damage done to it!
//            current_best_damage = sample_damage;
//            weakest_base = Position(expo);
//        }
//    }
//
//    return weakest_base;
//}

Position MapInventory::getBaseWithMostSurvivors(const bool &friendly, const bool &fodder) const
{
    Position strongest_base = Positions::Origin;
    int current_best_surviving = 0; // surviving units must be bigger than 0 or else it's not really a base.
    int sample_surviving = 0;
    int sample_ground_fodder = 0;
    for (auto b : CUNYAIModule::basemanager.getBases()) {
        Unit_Inventory ei_loc = CUNYAIModule::getUnitInventoryInNeighborhood(CUNYAIModule::enemy_player_model.units_, b.first);
        Unit_Inventory ui_loc = CUNYAIModule::getUnitInventoryInNeighborhood(CUNYAIModule::friendly_player_model.units_, b.first);
        Unit_Inventory ui_mini = CUNYAIModule::getUnitInventoryInArea(CUNYAIModule::friendly_player_model.units_, b.first);
        Unit_Inventory ei_mini = CUNYAIModule::getUnitInventoryInArea(CUNYAIModule::enemy_player_model.units_, b.first);

        ei_loc.updateUnitInventorySummary();
        ei_mini.updateUnitInventorySummary();
        ui_loc.updateUnitInventorySummary();
        ui_mini.updateUnitInventorySummary();

        if (friendly) {
            sample_surviving = CUNYAIModule::getFAPSurvivalForecast(ui_loc, ei_loc, FAP_SIM_DURATION, fodder);
            sample_ground_fodder = ui_mini.stock_ground_fodder_;
        }
        else {
            sample_surviving = CUNYAIModule::getFAPSurvivalForecast(ei_loc, ui_loc, FAP_SIM_DURATION, fodder);
            sample_ground_fodder = ei_mini.stock_ground_fodder_;
        }

        if (sample_surviving > current_best_surviving && sample_ground_fodder > 0) { // let's go to the place that is dishing out the most defensive damage.
            current_best_surviving = sample_surviving;
            strongest_base = Position(b.first);
        }
    }

    return strongest_base;
}

Position MapInventory::getBasePositionNearest(Position &p) {
    int shortest_path = INT_MAX;
    Position closest_base = Positions::Origin;
    for (auto b : CUNYAIModule::basemanager.getBases()) {
        if (getDistanceBetween(b.first, p) < shortest_path){
            shortest_path = getDistanceBetween(b.first, p);
            closest_base = b.first;
        }
    }
    return closest_base;
}

vector<TilePosition> MapInventory::getExpoTilePositions() {
    std::vector<TilePosition> expo_positions;
    for (auto & area : BWEM::Map::Instance().Areas()) {
        for (auto & base : area.Bases()) {
            expo_positions.push_back(base.Location());
        }
    }
    return expo_positions;
}


bool MapInventory::checkExploredAllStartPositions() {
    for (auto loc : Broodwar->getStartLocations()) {
        if (!Broodwar->isExplored(loc))
            return false;
    }
    return true;
}

void MapInventory::mainCurrentMap() {

    if (Broodwar->getFrameCount() % 24 == 0)
        updateGroundDangerousAreas(); // every second or so update ground frames.


    // Need to update map objects for every building!
    bool unit_calculation_frame = Broodwar->getFrameCount() % Broodwar->getLatencyFrames() != 0;

    if (unit_calculation_frame) return;

    StoredUnit* currently_visible_enemy = CUNYAIModule::getClosestIndicatorOfArmy(CUNYAIModule::enemy_player_model.units_, front_line_base_); // Get the closest ground unit with priority.
    discovered_enemy_this_frame = (enemy_found == false && currently_visible_enemy);
    enemy_found = enemy_found || currently_visible_enemy;

    assignArmyDestinations();
    assignAirDestinations();
    assignScoutDestinations();

    //Update Front Line Base
        //otherwise go to your weakest base.
    Position suspected_friendly_base = Positions::Origin;

    if (enemy_base_ground_ != Positions::Origin) {
        suspected_friendly_base = getBasePositionNearest(enemy_base_ground_);
    }

    if (suspected_friendly_base.isValid() && suspected_friendly_base != front_line_base_ && suspected_friendly_base != Positions::Origin) {
        front_line_base_ = suspected_friendly_base + Position(UnitTypes::Zerg_Hatchery.dimensionLeft(), UnitTypes::Zerg_Hatchery.dimensionUp());
    }

    // Update Safe Base
        //otherwise go to your safest base - the one with least deaths near it and most units.
    Position suspected_safe_base = Positions::Origin;

    suspected_safe_base = getBaseWithMostSurvivors(true, false);

    if (suspected_safe_base.isValid() && suspected_safe_base != safe_base_ && suspected_safe_base != Positions::Origin) {
        safe_base_ = suspected_safe_base + Position(UnitTypes::Zerg_Hatchery.dimensionLeft(), UnitTypes::Zerg_Hatchery.dimensionUp());
    }
    else {
        safe_base_ = front_line_base_;
    }

}

void MapInventory::writeMap(const vector< vector<int> > &mapin, const WalkPosition &center)
{
    std::stringstream ss;
    ss << center;
    string base = ss.str();

    //Flatten map before writing it.
    vector<int> holding_vector;
    for (int i = 0; i < Broodwar->mapWidth() * 4; i++)
        for (int j = 0; j < Broodwar->mapHeight() * 4; j++)
            holding_vector.push_back(mapin[i][j]);

    std::ostringstream merged_holding_vector;
    // Convert all but the last element to avoid a trailing ","
    std::copy(holding_vector.begin(), holding_vector.end() - 1,
        std::ostream_iterator<int>(merged_holding_vector, ","));
    // Now add the last element with no delimiter
    merged_holding_vector << holding_vector.back();

    ifstream newMap(CUNYAIModule::learned_plan.writeDirectory + Broodwar->mapFileName() + "Veins" + base + ".txt", ios_base::in);
    if (!newMap)
    {
        ofstream map;
        map.open(CUNYAIModule::learned_plan.writeDirectory + Broodwar->mapFileName() + "Veins" + base + ".txt", ios_base::app);
        //faster write of whole vector.
        map << merged_holding_vector.str() << endl;

        map.close();
    }
    newMap.close();
}

void MapInventory::readMap(vector< vector<int> > &mapin, const WalkPosition &center)

{
    std::stringstream ss;
    ss << center;
    string number;
    string base = ss.str();
    mapin.clear();

    ifstream newMap(CUNYAIModule::learned_plan.writeDirectory + Broodwar->mapFileName() + "Veins" + base + ".txt", ios_base::in);
    if (newMap)
    {
        for (int i = 0; i < Broodwar->mapWidth() * 4; i++)
        {
            mapin.push_back(std::vector<int>());
            for (int j = 0; j < Broodwar->mapHeight() * 4; j++) {
                getline(newMap, number, ',');
                mapin[i].push_back(stoi(number));
            }
        }
    }
    newMap.close();
}


vector<int> MapInventory::getRadialDistances(const Unit_Inventory & ui, const bool combat_units)
{
    vector<int> return_vector;

    for (auto u : ui.unit_map_) {
        if (u.second.type_.canAttack() && u.second.phase_ != StoredUnit::Phase::Retreating || !combat_units) {
            return_vector.push_back(this->getRadialDistanceOutFromHome(u.second.pos_));
        }
    }

    if (!return_vector.empty()) return return_vector;
    else return return_vector = { 0 };
}

vector< vector<int> > MapInventory::completeField(vector< vector<int> > pf, const int &reduction) {

    int tile_map_x = Broodwar->mapWidth();
    int tile_map_y = Broodwar->mapHeight(); //tile positions are 32x32, walkable checks 8x8 minitiles.
    int max_value = 0;
    vector<TilePosition> needs_filling;
    vector<int> flattened_potential_fields;
    for (int tile_x = 0; tile_x < tile_map_x; ++tile_x) {
        for (int tile_y = 0; tile_y < tile_map_y; ++tile_y) { // Check all possible walkable locations. Must cross over the WHOLE matrix. No sloppy bits.
            flattened_potential_fields.push_back(pf[tile_x][tile_y]);
            needs_filling.push_back(TilePosition({ tile_x, tile_y }));// if it is walkable, consider it a canidate for a choke.
            max_value = max(pf[tile_x][tile_y], max_value);
        }
    }

    bool changed_a_value_last_cycle = true;

    for (int iter = 0; iter < std::min({ tile_map_x, tile_map_y, max_value / reduction }); iter++) { // Do less iterations if we can get away with it.
        changed_a_value_last_cycle = false;
        for (auto position_to_investigate : needs_filling) { // not last element !
                                                                                                                                              // Psudocode: Mark every point touching value as value-reduction. Then, mark all minitiles touching those points as n+1.
                                                                                                                                              // Repeat untill finished.
            int local_grid = 0; // further faster since I no longer care about actually generating the veins.
            int tile_x = position_to_investigate.x;
            int tile_y = position_to_investigate.y;
            int home_value = flattened_potential_fields[tile_x * tile_map_x + tile_y];
            bool safety_check = tile_x > 0 && tile_y > 0 && tile_x + 1 < tile_map_x && tile_y + 1 < tile_map_x;

            if (safety_check) local_grid = std::max({
                flattened_potential_fields[(tile_x - 1) * tile_map_x + (tile_y - 1)],
                flattened_potential_fields[(tile_x - 1) * tile_map_x + tile_y],
                flattened_potential_fields[(tile_x - 1) * tile_map_x + (tile_y + 1)],
                flattened_potential_fields[tile_x       * tile_map_x + (tile_y - 1)],
                flattened_potential_fields[tile_x       * tile_map_x + (tile_y + 1)],
                flattened_potential_fields[(tile_x + 1) * tile_map_x + (tile_y - 1)],
                flattened_potential_fields[(tile_x + 1) * tile_map_x + tile_y],
                flattened_potential_fields[(tile_x + 1) * tile_map_x + (tile_y + 1)]
                }) - reduction;

            changed_a_value_last_cycle = local_grid > home_value || changed_a_value_last_cycle;
            flattened_potential_fields[tile_x * tile_map_x + tile_y] = std::max(home_value, local_grid);  //this leaves only local maximum densest units. It's very discontinuous and not a great approximation of even mildy spread forces.

        }

        if (changed_a_value_last_cycle == false) break; // if we did nothing last cycle, we don't need to punish ourselves.
    }

    //Unflatten
    for (int tile_x = 0; tile_x < tile_map_x; ++tile_x) {
        for (int tile_y = 0; tile_y < tile_map_y; ++tile_y) { // Check all possible walkable locations. Must cross over the WHOLE matrix. No sloppy bits.
            pf[tile_x][tile_y] = flattened_potential_fields[tile_x * tile_map_x + tile_y];
        }
    }

    return pf;
}


// IN PROGRESS  
void MapInventory::createAirThreatField(Player_Model &enemy_player) {

    int tile_map_x = Broodwar->mapWidth();
    int tile_map_y = Broodwar->mapHeight(); //tile positions are 32x32, walkable checks 8x8 minitiles.

    vector<vector<int>> pf_clear(tile_map_x, std::vector<int>(tile_map_y, 0));

    //set all the nonzero elements to their relevant values.
    for (auto unit : enemy_player.units_.unit_map_) { //Highest range dominates. We're just checking if they hit, not how HARD they hit.
        int air_range_ = CUNYAIModule::convertPixelDistanceToTileDistance(CUNYAIModule::getExactRange(unit.second.type_, enemy_player.bwapi_player_)) * unit.second.shoots_up_;
        pf_clear[TilePosition(unit.second.pos_).x][TilePosition(unit.second.pos_).y] = max(air_range_, pf_clear[TilePosition(unit.second.pos_).x][TilePosition(unit.second.pos_).y]);
    }
    // Fill the whole thing so each tile nearby is one less than the previous. All nonzero tiles are under threat.
    pf_air_threat_ = completeField(pf_clear, 1);

}

void MapInventory::createDetectField(Player_Model &enemy_player) {

    int tile_map_x = Broodwar->mapWidth();
    int tile_map_y = Broodwar->mapHeight(); //tile positions are 32x32, walkable checks 8x8 minitiles.

    vector<vector<int>> pf_clear(tile_map_x, std::vector<int>(tile_map_y, 0));

    //set all the nonzero elements to their relevant values.
    for (auto unit : enemy_player.units_.unit_map_) { //Highest range dominates. We're just checking if they detect, not how HARD they detect.
        int detection_range = unit.second.type_.isDetector() * CUNYAIModule::convertPixelDistanceToTileDistance(unit.second.type_.sightRange());
        pf_clear[TilePosition(unit.second.pos_).x][TilePosition(unit.second.pos_).y] = max(detection_range, pf_clear[TilePosition(unit.second.pos_).x][TilePosition(unit.second.pos_).y]);
    }
    // Fill the whole thing so each tile nearby is one less than the previous. All nonzero tiles are under threat.
    pf_detect_threat_ = completeField(pf_clear, 1);

}

void MapInventory::createGroundThreatField(Player_Model &enemy_player) {

    int tile_map_x = Broodwar->mapWidth();
    int tile_map_y = Broodwar->mapHeight(); //tile positions are 32x32, walkable checks 8x8 minitiles.

    vector<vector<int>> pf_clear(tile_map_x, std::vector<int>(tile_map_y, 0));

    //set all the nonzero elements to their relevant values.
    for (auto unit : enemy_player.units_.unit_map_) { //Highest range dominates. We're just checking if they detect, not how HARD they detect.
        int ground_range = CUNYAIModule::convertPixelDistanceToTileDistance(CUNYAIModule::getExactRange(unit.second.type_, enemy_player.bwapi_player_)) * unit.second.shoots_down_;
        pf_clear[TilePosition(unit.second.pos_).x][TilePosition(unit.second.pos_).y] = max(ground_range, pf_clear[TilePosition(unit.second.pos_).x][TilePosition(unit.second.pos_).y]);
    }
    // Fill the whole thing so each tile nearby is one less than the previous. All nonzero tiles are under threat.
    pf_ground_threat_ = completeField(pf_clear, 1);

}

void MapInventory::DiagnosticField(vector< vector<int> > &pf) {
    if (DIAGNOSTIC_MODE) {
        for (vector<int>::size_type i = 0; i < pf.size(); ++i) {
            for (vector<int>::size_type j = 0; j < pf[i].size(); ++j) {
                if (pf[i][j] > 0) {
                    if (CUNYAIModule::isOnScreen(Position(TilePosition{ static_cast<int>(i), static_cast<int>(j) }), CUNYAIModule::currentMapInventory.screen_position_)) {
                        Broodwar->drawTextMap(Position(TilePosition{ static_cast<int>(i), static_cast<int>(j) }), "%d", pf[i][j]);
                    }
                }
            }
        } // Pretty to look at!
    }
}

void MapInventory::DiagnosticTile() {
    if (DIAGNOSTIC_MODE) {
        int tile_map_x = Broodwar->mapWidth();
        int tile_map_y = Broodwar->mapHeight(); //tile positions are 32x32, walkable checks 8x8 minitiles.
        for (auto i = 0; i < tile_map_x; ++i) {
            for (auto j = 0; j < tile_map_y; ++j) {
                if (CUNYAIModule::isOnScreen(Position(TilePosition{ static_cast<int>(i), static_cast<int>(j) }), CUNYAIModule::currentMapInventory.screen_position_)) {
                    Broodwar->drawTextMap(Position(TilePosition{ static_cast<int>(i), static_cast<int>(j) }), "%d, %d", TilePosition{ static_cast<int>(i), static_cast<int>(j) }.x, TilePosition{ static_cast<int>(i), static_cast<int>(j) }.y);
                }
            }
        }
        // Pretty to look at!
    }
}

Position MapInventory::getEarlyGameScoutPosition() {
    // need to consider we could send 2 scouts to same position if it is unscouted. So filter by unexplore and unscouted and if nothing, then just try unexplored.

    vector<Position> viable_options;
    for (auto i: Broodwar->getStartLocations()) {
        if ( !Broodwar->isExplored(i) && !isScoutingPosition(Position(i)) ) { // if they don't exist yet use the starting location proceedure we've established earlier.
            viable_options.push_back(Position(i));
        }
    }

    if (getFurthestInVector(viable_options) != Positions::Origin) {
            return getFurthestInVector(viable_options);
    }

    viable_options.clear();
    for (auto i : Broodwar->getStartLocations()) {
        if (!Broodwar->isExplored(i)) { // if they don't exist yet use the starting location proceedure we've established earlier.
            viable_options.push_back(Position(i));
        }
    }

    if (getFurthestInVector(viable_options) != Positions::Origin) {
        return getFurthestInVector(viable_options);
    }
    else {
        Diagnostics::DiagnosticText("Oof, no unexplored start position left to march on?");
        return Positions::Origin;
    }
}

Position MapInventory::getEarlyGameArmyPosition() {
    vector<Position> viable_options;
    for (auto i : Broodwar->getStartLocations()) {
        if (!Broodwar->isExplored(i)) { // if they don't exist yet use the starting location proceedure we've established earlier.
            viable_options.push_back(Position(i));
        }
    }

    if (getClosestInVector(viable_options) != Positions::Origin) {
        return getClosestInVector(viable_options);
    }
    else {
        Diagnostics::DiagnosticText("Oof, no unexplored start position left to march on?");
        return Positions::Origin;
    }
}

Position MapInventory::getEarlyGameAirPosition() {
    vector<Position> viable_options;
    for (auto i : Broodwar->getStartLocations()) {
        if (!Broodwar->isExplored(i)) { // if they don't exist yet use the starting location proceedure we've established earlier.
            viable_options.push_back(Position(i));
        }
    }

    if (getClosestInVector(viable_options) != Positions::Origin) {
        return getClosestInVector(viable_options);
    }
    else {
        Diagnostics::DiagnosticText("Oof, no unexplored start position left to march on?");
        return Positions::Origin;
    }
}

Position MapInventory::getDistanceWeightedPosition(const Position & target_pos) {
    //From Dolphin Bot 2018 (with paraphrasing):
    double total_distance = 0;
    double sum_log_p = 0;

    vector<tuple<double, Position>> scout_expo_vector;
    vector<Position> chokesOrMineralPositions;
    for (const auto& r : CUNYAIModule::land_inventory.resource_inventory_) {
        chokesOrMineralPositions.push_back(r.second.pos_);
    }
    for (const auto& a : BWEM::Map::Instance().Areas()) {
        for (const auto& c : a.ChokePoints()) {
            chokesOrMineralPositions.push_back(Position(c->Center()));
        }
        chokesOrMineralPositions.push_back(Position(a.Top()));
    }

    // Create a map <log(distance), Position> of all base locations on map
    for (const auto& p : chokesOrMineralPositions) {
        int distance = getDistanceBetween(p, target_pos);
        if (distance >= 0 && !Broodwar->isVisible(TilePosition(p))) {
            total_distance += distance;
            scout_expo_vector.push_back({ distance, p });
        }
    }

    sort(scout_expo_vector.begin(), scout_expo_vector.end()); //sorts by first element of tuple in acending order.

    for (auto itr = scout_expo_vector.begin(); itr != scout_expo_vector.end(); ++itr) {
        sum_log_p += distanceTransformation(get<0>(*itr)); // sums to one, actually.
    }

    // Assign scout locations
    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<double> dis(0, 1);    // default values for output.
    int attempts = 0;
    while (attempts < 100) {
        for (auto itr = scout_expo_vector.begin(); itr != scout_expo_vector.end(); ++itr) {
            Position potential_scout_target = get<1>(*itr);
            double weighted_p_of_selection = distanceTransformation(get<0>(*itr)) / static_cast<double>(sum_log_p);

            if (dis(gen) < weighted_p_of_selection) {
                return potential_scout_target;
            }
        }
        attempts++;
    }
    Diagnostics::DiagnosticText("Oof, no scouting position?");
    return Positions::Origin;
}

//bool MapInventory::isScoutingOrMarchingOnPosition(const Position &pos, const bool &explored_sufficient, const bool &check_marching) {
//
//    int times_overexploring = 0;
//    int times_marching_against = 0; // we reject positions that are being scouted if 
//    int unexplored_starts = 0; // we reject positions that are being marched on if there is no other start position to scout.
//    
//    //If you are scouting, 
//    for(auto s : Broodwar->getStartLocations()) {
//        if (!Broodwar->isExplored(TilePosition(s)))
//            unexplored_starts++;
//    }
//    int number_of_excess_scouts = max(nScouts + 1 - unexplored_starts, 0);
//    if (Broodwar->getStartLocations().end() != find(Broodwar->getStartLocations().begin(), Broodwar->getStartLocations().end(), TilePosition(enemy_base_ground_)))
//        times_marching_against++;
//    for (auto s : scouting_bases_) {
//        if (Broodwar->getStartLocations().end() != find(Broodwar->getStartLocations().begin(), Broodwar->getStartLocations().end(), TilePosition(s)))
//            times_overexploring++;
//    }
//
//    bool explored = Broodwar->isExplored(TilePosition(pos));
//    bool visible = Broodwar->isVisible(TilePosition(pos));
//    return (isMarchingPosition(pos) && unexplored_starts == 0 && check_marching) || (isScoutingPosition(pos) && (times_overexploring + times_marching_against >= number_of_excess_scouts)) || visible || (explored && explored_sufficient);
//}

void MapInventory::assignArmyDestinations() {
    StoredUnit* currently_visible_enemy = CUNYAIModule::getClosestIndicatorOfArmy(CUNYAIModule::enemy_player_model.units_, front_line_base_); // Get the closest ground unit with priority.

    if (enemy_found) {
        if (currently_visible_enemy) {
            assignLateArmyMovement(currently_visible_enemy->pos_);
        }
        else {
            assignLateArmyMovement(Positions::Origin);
        }
    }
    else {
        //assign army to closest position. The army should move as little as possible.
        if (Broodwar->isExplored(TilePosition(enemy_base_ground_)) || enemy_base_ground_ == Positions::Origin)
            enemy_base_ground_ = getEarlyGameArmyPosition();
    }
}

void MapInventory::assignScoutDestinations() {
    StoredUnit* currently_visible_enemy = CUNYAIModule::getClosestIndicatorOfArmy(CUNYAIModule::enemy_player_model.units_, front_line_base_); // Get the closest ground unit with priority.

    if (enemy_found) {
        if (currently_visible_enemy) {
            assignLateScoutMovement(currently_visible_enemy->pos_);
        }
        else {
            assignLateScoutMovement(Positions::Origin);
        }

    }
    else {
        // create scouting position if bases are empty.
        if (scouting_bases_.empty()) {
            for (int i = 0; i < nScouts; i++) {
                scouting_bases_.push_back(Positions::Origin);
            }
        }
        //assign scouts to furthest position.
        for (auto& p : scouting_bases_) {
            if (Broodwar->isExplored(TilePosition(p)) || p == Positions::Origin)
                p = getEarlyGameScoutPosition();
        }
        //If they overlap with army, that's OK because we are minimizing the maximum time it takes to get the army to the enemy base in this pattern.
    }
}

void MapInventory::assignAirDestinations() {
    StoredUnit* currently_visible_air = CUNYAIModule::getClosestAirStoredWithPriority(CUNYAIModule::enemy_player_model.units_, CUNYAIModule::friendly_player_model.units_.getMeanBuildingLocation()); // Get the flyer closest to our base.

    if (enemy_found) {
        if (currently_visible_air) {
            assignLateAirMovement(currently_visible_air->pos_);
        }
        else {
            assignLateAirMovement(Positions::Origin);
        }
    }
    else {
        //assign air army to closest position. The air army should move as little as possible.
        if (Broodwar->isExplored(TilePosition(enemy_base_air_)) || enemy_base_air_ == Positions::Origin)
            enemy_base_air_ = getEarlyGameAirPosition();
    }
}

bool MapInventory::isScoutingPosition(const Position &pos) {
    return scouting_bases_.end() != find(scouting_bases_.begin(), scouting_bases_.end(), pos);
}

bool MapInventory::isMarchingPosition(const Position &pos) {
    return static_cast<int>(BWEM::Map::Instance().GetNearestArea(TilePosition(pos))->Id()) == static_cast<int>(BWEM::Map::Instance().GetNearestArea(TilePosition(enemy_base_ground_))->Id());
}

Position MapInventory::getClosestInVector(vector<Position> &posVector){
    Position pos_holder = Positions::Origin;
    int dist_holder = INT_MAX;
    for (auto p : posVector) {
        if (dist_holder > getRadialDistanceOutFromHome(p)) {
            dist_holder = getRadialDistanceOutFromHome(p);
            pos_holder = p;
        }
    }
    return pos_holder;
}

Position MapInventory::getFurthestInVector(vector<Position> &posVector) {
    Position pos_holder = Positions::Origin;
    int dist_holder = INT_MIN;
    for (auto p : posVector) {
        if (dist_holder < getRadialDistanceOutFromHome(p)) {
            dist_holder = getRadialDistanceOutFromHome(p);
            pos_holder = p;
        }
    }
    return pos_holder;
}

bool MapInventory::isStartPosition(const Position &p) {
    for (auto s : Broodwar->getStartLocations()) {
        if (TilePosition(p) == TilePosition(s))
            return true;
    }
    return false;
}

double MapInventory::distanceTransformation(const int distanceFromTarget) {
        return distanceFromTarget ==  0 ? 0.30 : 100.0/static_cast<double>(distanceFromTarget);
}

void MapInventory::assignLateArmyMovement(const Position closest_enemy){
    if (closest_enemy != Positions::Origin && closest_enemy.isValid()) { // let's go to the closest enemy if we've seen 'em!
        enemy_base_ground_ = closest_enemy;
    }
    else if (Broodwar->isVisible(TilePosition(enemy_base_ground_)) || enemy_base_ground_ == Positions::Origin) { //Let's hunt near the last visible enemy otherwise.
        enemy_base_ground_ = getDistanceWeightedPosition(enemy_base_ground_);
    }
}

void MapInventory::assignLateAirMovement(const Position closest_enemy) {
    if (closest_enemy != Positions::Origin && closest_enemy.isValid()) { // let's go to the closest enemy if we've seen 'em!
        enemy_base_air_ = closest_enemy;
    }
    else if (Broodwar->isVisible(TilePosition(enemy_base_air_)) || enemy_base_air_ == Positions::Origin) { //Let's hunt near the last visible enemy otherwise.
        enemy_base_air_ = getDistanceWeightedPosition(enemy_base_air_);
    }
}

void MapInventory::assignLateScoutMovement(const Position closest_enemy) {
    if (closest_enemy != Positions::Origin && closest_enemy.isValid()) { // let's go to hunt near the closest enemy if we've seen 'em!
        for (auto& p : scouting_bases_) {
            if (Broodwar->isVisible(TilePosition(p)) || discovered_enemy_this_frame)
                p = getDistanceWeightedPosition(closest_enemy);
        }
    }
    else { //Let's hunt near the last visible enemy otherwise.
        for (auto& p : scouting_bases_) {
            if (Broodwar->isVisible(TilePosition(p)))
                p = getDistanceWeightedPosition(enemy_base_ground_);
        }
    }
}

bool MapInventory::isTileDetected(const Position & p)
{
    return  CUNYAIModule::currentMapInventory.pf_detect_threat_[TilePosition(p).x][TilePosition(p).y] > 0;
}

bool MapInventory::isTileAirThreatened(const Position & p)
{
    return CUNYAIModule::currentMapInventory.pf_air_threat_[TilePosition(p).x][TilePosition(p).y] > 0;
}

bool MapInventory::isTileGroundThreatened(const Position & p)
{
    return CUNYAIModule::currentMapInventory.pf_ground_threat_[TilePosition(p).x][TilePosition(p).y] > 0;
}
