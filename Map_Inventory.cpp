#pragma once

#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\Map_Inventory.h"
#include "Source\Unit_Inventory.h"
#include "Source\Resource_Inventory.h"
#include <algorithm>
#include <fstream>
#include <ostream>
#include <set>

using namespace std;

// Creates a Inventory Object
Map_Inventory::Map_Inventory() {};
Map_Inventory::Map_Inventory( const Unit_Inventory &ui, const Resource_Inventory &ri ) {

    updateVision_Count();

    updateLn_Supply_Remain();
    updateLn_Supply_Total();

    updateGas_Workers();
    updateMin_Workers();

    updateMin_Possessed( ri );
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

    if ( unwalkable_barriers_.size() == 0 ) {
        updateUnwalkable();
    }

    if ( ri.resource_inventory_.size() == 0 ) {
        updateBuildablePos();
        int buildable_ct = 0;
        for ( vector<int>::size_type i = 0; i != buildable_positions_.size(); ++i ) {
            for ( vector<int>::size_type j = 0; j != buildable_positions_[i].size(); ++j ) {
                buildable_ct += buildable_positions_[i][j];
            }
        }
        CUNYAIModule::DiagnosticText( "There are %d resources on the map, %d canidate expo positions.", ri.resource_inventory_.size(), buildable_ct );
    }

    if ( map_veins_.size() == 0 ) {
        updateMapVeins();
        int vein_ct = 0;
        for ( vector<int>::size_type i = 0; i != map_veins_.size(); ++i ) {
            for ( vector<int>::size_type j = 0; j != map_veins_[i].size(); ++j ) {
                if ( map_veins_[i][j] > 10 ) {
                    ++vein_ct;
                }
            }
        }
        CUNYAIModule::DiagnosticText( "There are %d roughly tiles, %d veins.", map_veins_.size(), vein_ct );
    }

    if ( start_positions_.empty() && !cleared_all_start_positions_) {
        getStartPositions();
    }
};



// Updates the (safe) log of our supply stock. Looks specifically at our morphing units as "available".
void Map_Inventory::updateLn_Supply_Remain() {

    int total = Broodwar->self()->supplyTotal() - Broodwar->self()->supplyUsed();

    if ( total <= 0 ) {
        total = 1;
    } // no log 0 under my watch!.

    ln_supply_remain_ = log( total );
};

// Updates the (safe) log of our consumed supply total.
void Map_Inventory::updateLn_Supply_Total() {

    double total = Broodwar->self()->supplyTotal();
    if ( total <= 0 ) {
        total = 1;
    } // no log 0 under my watch!.

    ln_supply_total_ = log( total );
};


// Updates the (safe) log of our gas total. Returns very high int instead of infinity.
double Map_Inventory::getGasRatio() {
    // Normally:
    if ( Broodwar->self()->minerals() > 0 || Broodwar->self()->gas() > 0 ) {
        return static_cast<double>(Broodwar->self()->gas()) / static_cast<double>(Broodwar->self()->minerals() + Broodwar->self()->gas());
    }
    else {
        return 99999;
    } // in the alternative case, you have nothing - you're mineral starved, you need minerals, not gas. Define as ~~infty, not 0.
};

// Updates the (safe) log of our supply total. Returns very high int instead of infinity.
double Map_Inventory::getLn_Supply_Ratio() {
    // Normally:
    if ( ln_supply_total_ > 0 ) {
        return ln_supply_remain_ / ln_supply_total_;
    }
    else {
        return 0;
    } // in the alternative case, you have nothing - you're supply starved. Probably dead, too. Just in case- Define as ~~infty, not 0.
};

// Updates the count of our gas workers.
void Map_Inventory::updateGas_Workers() {
    // Get worker tallies.
    int gas_workers = 0;

    Unitset myUnits = Broodwar->self()->getUnits(); // out of my units  Probably easier than searching the map for them.
    if ( !myUnits.empty() ) { // make sure this object is valid!
        for ( auto u = myUnits.begin(); u != myUnits.end() && !myUnits.empty(); ++u )
        {
            if ( (*u) && (*u)->exists() ) {
                if ( (*u)->getType().isWorker() ) {
                    if ( (*u)->isGatheringGas() || (*u)->isCarryingGas() ) // implies exists and isCompleted
                    {
                        ++gas_workers;
                    }
                } // closure: Only investigate closely if they are drones.
            } // Closure: only investigate on existance of unit..
        } // closure: count all workers
    }

    gas_workers_ = gas_workers;
}

// Updates the count of our mineral workers.
void Map_Inventory::updateMin_Workers() {
    // Get worker tallies.
    int min_workers = 0;

    Unitset myUnits = Broodwar->self()->getUnits(); // out of my units  Probably easier than searching the map for them.
    if ( !myUnits.empty() ) { // make sure this object is valid!
        for ( auto u = myUnits.begin(); u != myUnits.end() && !myUnits.empty(); ++u )
        {
            if ( (*u) && (*u)->exists() ) {
                if ( (*u)->getType().isWorker() ) {
                    if ( (*u)->isGatheringMinerals() || (*u)->isCarryingMinerals() ) // implies exists and isCompleted
                    {
                        ++min_workers;
                    }
                } // closure: Only investigate closely if they are drones.
            } // Closure: only investigate on existance of unit..
        } // closure: count all workers
    }

    min_workers_ = min_workers;
}

// Updates the number of mineral fields we "possess".
void Map_Inventory::updateMin_Possessed(const Resource_Inventory &ri) {

    int min_fields = 0;
    for (auto r = ri.resource_inventory_.begin(); r != ri.resource_inventory_.end() && !ri.resource_inventory_.empty(); ++r) { //for each mineral
        if (r->second.occupied_natural_ && r->second.type_.isMineralField() ) {
                min_fields++; // if there is a base near it, then this mineral counts.
        } // closure for existance check.
    } // closure: for each mineral

    min_fields_ = min_fields;
}

// Updates the count of our vision total, in tiles
void Map_Inventory::updateVision_Count() {
    int map_x = BWAPI::Broodwar->mapWidth();
    int map_y = BWAPI::Broodwar->mapHeight();

    int map_area = map_x * map_y; // map area in tiles.
    int total_tiles = 0;
    for ( int tile_x = 1; tile_x <= map_x; tile_x++ ) { // there is no tile (0,0)
        for ( int tile_y = 1; tile_y <= map_y; tile_y++ ) {
            if ( BWAPI::Broodwar->isVisible( tile_x, tile_y ) ) {
                total_tiles += 1;
            }
        }
    } // this search must be very exhaustive to do every frame. But C++ does it without any problems.

    if ( total_tiles == 0 ) {
        total_tiles = 1;
    } // catch some odd case where you are dead anyway. Rather not crash.
    vision_tile_count_ = total_tiles;
}

void Map_Inventory::updateScreen_Position()
{
    screen_position_ = Broodwar->getScreenPosition();
}

// Updates the number of hatcheries (and decendent buildings).
void Map_Inventory::updateHatcheries() {
    hatches_ = CUNYAIModule::Count_Units( UnitTypes::Zerg_Hatchery) +
        CUNYAIModule::Count_Units( UnitTypes::Zerg_Lair) +
        CUNYAIModule::Count_Units( UnitTypes::Zerg_Hive);
}


//In Tiles?
void Map_Inventory::updateBuildablePos()
{
    int map_x = Broodwar->mapWidth();
    int map_y = Broodwar->mapHeight();
    buildable_positions_.reserve(map_x);
    for ( int x = 0; x <= map_x; ++x ) {
        vector<bool> temp;
        temp.reserve(map_y);
        for ( int y = 0; y <= map_y; ++y ) {
            temp.push_back( Broodwar->isBuildable( x, y ) );
        }
        buildable_positions_.push_back( temp );
    }
};

void Map_Inventory::updateUnwalkable() {
    int map_x = Broodwar->mapWidth() * 4;
    int map_y = Broodwar->mapHeight() * 4; //tile positions are 32x32, walkable checks 8x8 minitiles.

    unwalkable_barriers_.reserve(map_x);
    // first, define matrixes to recieve the walkable locations for every minitile.
    for ( int x = 0; x <= map_x; ++x ) {
        vector<int> temp;
        temp.reserve(map_y);
        for ( int y = 0; y <= map_y; ++y ) {
            temp.push_back( !Broodwar->isWalkable( x, y ) );
        }
        unwalkable_barriers_.push_back( temp );
    }

    unwalkable_barriers_with_buildings_ = unwalkable_barriers_; // preparing for the dependencies.
}

void Map_Inventory::updateSmoothPos() {
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
                    local_grid[0][1] = (smoothed_barriers_[(minitile_x - 1)][minitile_y]       < iter && smoothed_barriers_[(minitile_x - 1)][minitile_y] > 0);
                    local_grid[0][2] = (smoothed_barriers_[(minitile_x - 1)][(minitile_y + 1)] < iter && smoothed_barriers_[(minitile_x - 1)][(minitile_y + 1)] > 0);

                    local_grid[1][0] = (smoothed_barriers_[minitile_x][(minitile_y - 1)] < iter && smoothed_barriers_[minitile_x][(minitile_y - 1)] > 0);
                    local_grid[1][1] = (smoothed_barriers_[minitile_x][minitile_y]       < iter && smoothed_barriers_[minitile_x][minitile_y] > 0);
                    local_grid[1][2] = (smoothed_barriers_[minitile_x][(minitile_y + 1)] < iter && smoothed_barriers_[minitile_x][(minitile_y + 1)] > 0);

                    local_grid[2][0] = (smoothed_barriers_[(minitile_x + 1)][(minitile_y - 1)] < iter && smoothed_barriers_[(minitile_x + 1)][(minitile_y - 1)] > 0);
                    local_grid[2][1] = (smoothed_barriers_[(minitile_x + 1)][minitile_y]       < iter && smoothed_barriers_[(minitile_x + 1)][minitile_y] > 0);
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
                    smoothed_barriers_[minitile_x][minitile_y] = opposing_tiles * ( iter + open_path * (99 - 2 * iter) );
                }
            }
        }
        if (changed_a_value_last_cycle == false) {
            return; // if we did nothing last cycle, we don't need to punish ourselves.
        }
    }
}

void Map_Inventory::updateMapVeins() {
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
            flattened_map_veins.push_back( map_veins_[minitile_x][minitile_y] );
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

int Map_Inventory::getMapValue(const Position & pos, const vector<vector<int>>& map)
{
    WalkPosition startloc = WalkPosition(pos);
    return map[startloc.x][startloc.y];
}

int Map_Inventory::getFieldValue(const Position & pos, const vector<vector<int>>& field)
{
    TilePosition startloc = TilePosition(pos);
    return field[startloc.x][startloc.y];
}

void Map_Inventory::updateMapVeinsOut(const Position &newCenter, Position &oldCenter, vector<vector<int>> &map, const bool &print) { //in progress.

    int map_x = Broodwar->mapWidth() * 4;
    int map_y = Broodwar->mapHeight() * 4; //tile positions are 32x32, walkable checks 8x8 minitiles.

    if (WalkPosition(oldCenter) == WalkPosition(newCenter)) return;

    oldCenter = newCenter; // Must update old center manually.
    WalkPosition startloc = WalkPosition(newCenter);
    std::stringstream ss;
    ss << WalkPosition(newCenter);
    string base = ss.str();

    ifstream newVeins(".\\bwapi-data\\write\\" + Broodwar->mapFileName() + "Veins" + base + ".txt", ios_base::in);
    if (!newVeins)         //The file does not exist, we need to write it.
    {

        map = unwalkable_barriers_;

        int minitile_x, minitile_y, distance_right_x, distance_below_y;
        minitile_x = startloc.x;
        minitile_y = startloc.y;
        distance_right_x = max(map_x - minitile_x, map_x);
        distance_below_y = max(map_y - minitile_y, map_y);
        int t = std::max(map_x + distance_right_x + distance_below_y, map_y + distance_right_x + distance_below_y);
        //int maxI = t*t; // total number of spiral steps we have to make.
        int total_squares_filled = 2; // If you start at 1 you will be implicitly marking certain squares as unwalkable. 1 is the short code for unwalkable.

        vector <WalkPosition> fire_fill_queue;
        vector <WalkPosition> fire_fill_queue_holder;

        //begin with a flood fill.
        map[minitile_x][minitile_y] = total_squares_filled;
        fire_fill_queue.push_back({ minitile_x, minitile_y });

        int minitile_x_temp = minitile_x;
        int minitile_y_temp = minitile_y;
        bool filled_a_square = false;

        // let us fill the map- counting outward from our destination.
        while (!fire_fill_queue.empty() || !fire_fill_queue_holder.empty()) {

            filled_a_square = false;

            while (!fire_fill_queue.empty()) { // this portion is now a flood fill, iteratively filling from its interior. Seems to be very close to fastest reasonable implementation. May be able to remove diagonals without issue.

                minitile_x_temp = fire_fill_queue.begin()->x;
                minitile_y_temp = fire_fill_queue.begin()->y;
                fire_fill_queue.erase(fire_fill_queue.begin());

                // north
                if (minitile_y_temp + 1 < map_y && map[minitile_x_temp][minitile_y_temp + 1] == 0) {
                    filled_a_square = true;
                    map[minitile_x_temp][minitile_y_temp + 1] = total_squares_filled;
                    fire_fill_queue_holder.push_back({ minitile_x_temp , minitile_y_temp + 1 });
                }
                // north east
                if (minitile_y_temp + 1 < map_y && minitile_x_temp + 1 < map_x && map[minitile_x_temp + 1][minitile_y_temp + 1] == 0) {
                    filled_a_square = true;
                    map[minitile_x_temp + 1][minitile_y_temp + 1] = total_squares_filled;
                    fire_fill_queue_holder.push_back({ minitile_x_temp + 1, minitile_y_temp + 1 });
                }
                // north west
                if (minitile_y_temp + 1 < map_y && 0 < minitile_x_temp - 1 && map[minitile_x_temp - 1][minitile_y_temp + 1] == 0) {
                    filled_a_square = true;
                    map[minitile_x_temp - 1][minitile_y_temp + 1] = total_squares_filled;
                    fire_fill_queue_holder.push_back({ minitile_x_temp - 1, minitile_y_temp + 1 });
                }
                //south
                if (0 < minitile_y_temp - 1 && map[minitile_x_temp][minitile_y_temp - 1] == 0) {
                    filled_a_square = true;
                    map[minitile_x_temp][minitile_y_temp - 1] = total_squares_filled;
                    fire_fill_queue_holder.push_back({ minitile_x_temp, minitile_y_temp - 1 });
                }
                //south east
                if (0 < minitile_y_temp - 1 && minitile_x_temp + 1 < map_x && map[minitile_x_temp + 1][minitile_y_temp - 1] == 0) {
                    filled_a_square = true;
                    map[minitile_x_temp + 1][minitile_y_temp - 1] = total_squares_filled;
                    fire_fill_queue_holder.push_back({ minitile_x_temp + 1, minitile_y_temp - 1 });
                }
                //south west
                if (0 < minitile_y_temp - 1 && 0 < minitile_x_temp - 1 && map[minitile_x_temp - 1][minitile_y_temp - 1] == 0) {
                    filled_a_square = true;
                    map[minitile_x_temp - 1][minitile_y_temp - 1] = total_squares_filled;
                    fire_fill_queue_holder.push_back({ minitile_x_temp - 1, minitile_y_temp - 1 });
                }
                // east
                if (minitile_x_temp + 1 < map_x && map[minitile_x_temp + 1][minitile_y_temp] == 0) {
                    filled_a_square = true;
                    map[minitile_x_temp + 1][minitile_y_temp] = total_squares_filled;
                    fire_fill_queue_holder.push_back({ minitile_x_temp + 1, minitile_y_temp });
                }
                //west
                if (0 < minitile_x_temp - 1 && map[minitile_x_temp - 1][minitile_y_temp] == 0) {
                    filled_a_square = true;
                    map[minitile_x_temp - 1][minitile_y_temp] = total_squares_filled;
                    fire_fill_queue_holder.push_back({ minitile_x_temp - 1, minitile_y_temp });
                }
            }
            total_squares_filled += filled_a_square;
            fire_fill_queue.clear();
            fire_fill_queue.swap(fire_fill_queue_holder);
            fire_fill_queue_holder.clear();
        }

        if(print) writeMap(map, WalkPosition(newCenter));
    }
    else
    {
        readMap(map, WalkPosition(newCenter));
    }
    newVeins.close();
}

int Map_Inventory::getDifferentialDistanceOutFromEnemy(const Position A, const Position B ) const
{
    if (map_out_from_enemy_ground_.size() > 0 && A.isValid() && B.isValid()) {
        WalkPosition wp_a = WalkPosition(A);
        WalkPosition wp_b = WalkPosition(B);
        int A = map_out_from_enemy_ground_[(size_t)wp_a.x][(size_t)wp_a.y];
        int B = map_out_from_enemy_ground_[(size_t)wp_b.x][(size_t)wp_b.y];
        if (A > 1 && B > 1) {
            return abs(A - B);
        }
    }

     return 9999999;
}

int Map_Inventory::getRadialDistanceOutFromEnemy( const Position A) const
{
    if ( map_out_from_enemy_ground_.size() > 0 && A.isValid()) {
        WalkPosition wp_a = WalkPosition( A );
        int A = map_out_from_enemy_ground_[(size_t)wp_a.x][(size_t)wp_a.y];
        if (A > 1) {
            return map_out_from_enemy_ground_[(size_t)wp_a.x][(size_t)wp_a.y];
        }
    }

      return 9999999;

}

int Map_Inventory::getDifferentialDistanceOutFromHome( const Position A, const Position B ) const
{
    if ( map_out_from_home_.size() > 0 && A.isValid() && B.isValid() ) {
        WalkPosition wp_a = WalkPosition(A);
        WalkPosition wp_b = WalkPosition(B);
        int A = map_out_from_home_[(size_t)wp_a.x][(size_t)wp_a.y];
        int B = map_out_from_home_[(size_t)wp_b.x][(size_t)wp_b.y];
        if (A > 1 && B > 1) {
            return abs(A - B);
        }
    }

    return 9999999;
}

int Map_Inventory::getRadialDistanceOutOnMap(const Position A, const vector<vector<int>> &map) const
{
    if (map.size() > 0 && A.isValid()) {
        WalkPosition wp_a = WalkPosition(A);
        int A = map[(size_t)wp_a.x][(size_t)wp_a.y];
        if (A > 1) {
            return map[(size_t)wp_a.x][(size_t)wp_a.y];
        }
    }

    return 9999999;

}

bool Map_Inventory::checkViableGroundPath(const Position A, const Position B) const
{
    if (map_out_from_home_.size() > 0 && A.isValid() && B.isValid()) {
        WalkPosition wp_a = WalkPosition(A);
        WalkPosition wp_b = WalkPosition(B);
        int A = map_out_from_home_[(size_t)wp_a.x][(size_t)wp_a.y];
        int B = map_out_from_home_[(size_t)wp_b.x][(size_t)wp_b.y];
        if (A > 1 && B > 1) {
            return true;
        }
    }
return false;
}

int Map_Inventory::getRadialDistanceOutFromHome( const Position A ) const
{
    if (map_out_from_home_.size() > 0 && A.isValid()) {
        WalkPosition wp_a = WalkPosition(A);
        int A = map_out_from_home_[(size_t)wp_a.x][(size_t)wp_a.y];
        if (A > 1) {
            return map_out_from_home_[(size_t)wp_a.x][(size_t)wp_a.y];
        }
    }

    return 9999999;

}

//void Map_Inventory::updateLiveMapVeins( const Unit &building, const Unit_Inventory &ui, const Unit_Inventory &ei, const Resource_Inventory &ri ) { // in progress.
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
void Map_Inventory::updateUnwalkableWithBuildings() {
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
            WalkPosition upper_left_modified  = WalkPosition(max_upper_left.x > 0 ? max_upper_left.x : 1, max_upper_left.y > 0 ? max_upper_left.y : 1);

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

//void Map_Inventory::updateLiveMapVeins(const Unit_Inventory &ui, const Unit_Inventory &ei, const Resource_Inventory &ri) { // in progress.
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

//void Map_Inventory::updateMapChokes() { // in progress. Idea : A choke is if the maximum variation of ground distances in a 5x5 tile square is LESS than some threshold. It is a plane if it is GREATER than some threshold.
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



void Map_Inventory::updateWorkersClearing( )
{
    int clearing_workers_found = 0;

    if (!CUNYAIModule::friendly_player_model.units_.unit_map_.empty()) {
        for (auto & w = CUNYAIModule::friendly_player_model.units_.unit_map_.begin(); w != CUNYAIModule::friendly_player_model.units_.unit_map_.end() && !CUNYAIModule::friendly_player_model.units_.unit_map_.empty(); w++) {
            if ( w->second.isAssignedClearing() ) {
                clearing_workers_found ++;
            }
        }
    }
    workers_clearing_ = clearing_workers_found;
}

void Map_Inventory::updateWorkersLongDistanceMining()
{
    int long_distance_miners_found = 0;

    if (!CUNYAIModule::friendly_player_model.units_.unit_map_.empty()) {
        for (auto & w = CUNYAIModule::friendly_player_model.units_.unit_map_.begin(); w != CUNYAIModule::friendly_player_model.units_.unit_map_.end() && !CUNYAIModule::friendly_player_model.units_.unit_map_.empty(); w++) {
            if (w->second.isAssignedLongDistanceMining()) {
                long_distance_miners_found++;
            }
        }
    }
    workers_distance_mining_ = long_distance_miners_found ;
}

Position Map_Inventory::getWeakestBase( const Unit_Inventory &ei) const
{
    Position weakest_base = Positions::Origin;
    int stock_current_best = 0;

    for (auto expo : expo_positions_complete_) {
        Unit_Inventory ei_loc = CUNYAIModule::getUnitInventoryInNeighborhood(ei, Position(expo));
        Unit_Inventory ei_tiny = CUNYAIModule::getUnitInventoryInArea(ei_loc, Position(expo));
        ei_loc.updateUnitInventorySummary();
        ei_tiny.updateUnitInventorySummary();
        if (ei_loc.moving_average_fap_stock_ < stock_current_best && ei_loc.stock_ground_fodder_ > 0 && ei_tiny.stock_ground_fodder_ > 0) { // if they have fodder (buildings) and it is weaker, target that place!
            stock_current_best = ei_loc.moving_average_fap_stock_;
            weakest_base = Position(expo);
        }
    }

    return weakest_base;
}


Position Map_Inventory::getNonCombatBase(const Unit_Inventory & ui, const Unit_Inventory & di) const
{
    Position quiet_base = Positions::Origin;
    int temp_safest_base = INT_MIN;

    for (auto expo : expo_positions_complete_) {
        Unit_Inventory ui_loc = CUNYAIModule::getUnitInventoryInArea(ui, Position(expo));
        Unit_Inventory di_loc = CUNYAIModule::getUnitInventoryInNeighborhood(di, Position(expo));
        ui_loc.updateUnitInventorySummary();
        di_loc.updateUnitInventorySummary();

        if ( ui_loc.stock_fighting_total_ - di_loc.stock_total_ > temp_safest_base && ui_loc.resource_depot_count_ > 0 ) { // if they have fodder (buildings) and it is weaker, target that place!
            temp_safest_base = ui_loc.stock_fighting_total_ - di_loc.stock_total_;
            quiet_base = Position(expo);
        }
    }

    return quiet_base;
}


Position Map_Inventory::getMostValuedBase(const Unit_Inventory & ui) const
{
    Position valued_base = Positions::Origin;
    int temp_valued_base = INT_MIN;

    for (auto expo : expo_positions_complete_) {
        Unit_Inventory ui_loc = CUNYAIModule::getUnitInventoryInArea(ui, Position(expo));
        ui_loc.updateUnitInventorySummary();

        if (ui_loc.stock_ground_fodder_ + ui_loc.stock_air_fodder_ > temp_valued_base && ui_loc.resource_depot_count_ > 0) { // if they have fodder (buildings) and it is weaker, target that place!
            temp_valued_base = ui_loc.stock_ground_fodder_ + ui_loc.stock_air_fodder_;
            valued_base = Position(expo);
        }
    }

    return valued_base;
}


void Map_Inventory::getExpoPositions() {

    expo_positions_.clear();

    std::vector<TilePosition> expo_positions;
    for (auto & area : BWEM::Map::Instance().Areas()){
        for (auto & base : area.Bases()){
            expo_positions.push_back(base.Location());
        }
    }
    expo_positions_ = expo_positions;


    //From SO, quick conversion into set.
    set<TilePosition> s;
    int size = expo_positions_.size();
    for (int i = 0; i < size; ++i) s.insert(expo_positions_[i]);
    expo_positions_complete_.assign(s.begin(), s.end());
}

void Map_Inventory::getStartPositions() {
    for ( auto loc : Broodwar->getStartLocations() ) {
        start_positions_.push_back( Position( loc ) );
    }
}

void Map_Inventory::updateStartPositions(const Unit_Inventory &ei) {
    for ( auto visible_base = start_positions_.begin(); visible_base != start_positions_.end() && !start_positions_.empty();) {
        if ( Broodwar->isExplored( TilePosition( *visible_base ) ) || Broodwar->self()->getStartLocation() == TilePosition(*visible_base) ) {
            visible_base = start_positions_.erase( visible_base );
            //if ( *visible_base == start_positions_[0] ) {
            //    updateMapVeinsOutFromFoe(start_positions_[0]);
            //}
        }
        else {
            ++visible_base;
        }
    }

    if ( start_positions_.empty() ) {
        cleared_all_start_positions_ = true;
    }
    //else if (ei.getMeanBuildingLocation() == Position(0,0) && enemy_base_ground_ != start_positions_[0]){ // should start precaching the mean building location.
    //    updateMapVeinsOutFromFoe(start_positions_[0]);
    //}
}

void Map_Inventory::updateBasePositions(Unit_Inventory &ui, Unit_Inventory &ei, const Resource_Inventory &ri, const Unit_Inventory &ni, const Unit_Inventory &di) {

    // Need to update map objects for every building!
    bool unit_calculation_frame = Broodwar->getFrameCount() % Broodwar->getLatencyFrames() != 0;
    int frames_this_cycle = Broodwar->getFrameCount() % (24 * 4); // technically more.

                                                                  // every frame this is incremented.
    frames_since_enemy_base_ground_++;
    frames_since_enemy_base_air_++;
    frames_since_home_base++;
    frames_since_map_veins++;
    frames_since_safe_base++;
    frames_since_unwalkable++;

    //every 10 sec check if we're sitting at our destination.
    //if (Broodwar->isVisible(TilePosition(enemy_base_ground_)) && Broodwar->getFrameCount() % (24 * 5) == 0) {
    //    fram = true;
    //}
    if (unit_calculation_frame) return;

    //If we need updating (from building destruction or any other source) - begin the cautious chain of potential updates.
    if (frames_since_unwalkable > 24 * 30) {

        getExpoPositions();
        updateUnwalkableWithBuildings();
        frames_since_unwalkable = 0;
        return;
    }

    if (frames_since_map_veins > 24 * 30) { // impose a second wait here because we don't want to update this if we're discovering buildings rapidly.

        updateMapVeins();
        frames_since_map_veins = 0;
        return;
    }

    if (frames_since_enemy_base_ground_ > 24 * 10) {

        Stored_Unit* center_army = CUNYAIModule::getClosestGroundStored(ei, ui.getMeanLocation()); // If the mean location is over water, nothing will be updated. Current problem: Will not update if no combat forces!
        Stored_Unit* center_base = CUNYAIModule::getClosestStoredBuilding(ei, ui.getMeanLocation(),999999); // 

        if (center_army && center_army->pos_ && center_army->pos_ != Positions::Origin) { // Sometimes buildings get invalid positions. Unclear why. Then we need to use a more traditioanl method.
            updateMapVeinsOut( center_army->pos_, enemy_base_ground_, map_out_from_enemy_ground_, false); // don't print this one, it could be anywhere and to print all of them would end up filling up our hard drive.
        }
        else if (center_base && center_base->pos_ && center_base->pos_ != Positions::Origin) { // Sometimes buildings get invalid positions. Unclear why. Then we need to use a more traditioanl method.
            updateMapVeinsOut(center_base->pos_, enemy_base_ground_, map_out_from_enemy_ground_, false); // don't print this one, it could be anywhere and to print all of them would end up filling up our hard drive.
        }
        else if (!start_positions_.empty() && start_positions_[0] && start_positions_[0] !=  Positions::Origin && !cleared_all_start_positions_) { // maybe it's an starting base we havent' seen yet?
            int attempts = 0;
            while (Broodwar->isVisible(TilePosition(enemy_base_ground_)) && attempts < static_cast<int>(start_positions_.size()) && Broodwar->isVisible(TilePosition(start_positions_[0]))) {
                std::rotate(start_positions_.begin(), start_positions_.begin() + 1, start_positions_.end());
                attempts++;
            }
            updateMapVeinsOut( start_positions_[0] + Position(UnitTypes::Zerg_Hatchery.dimensionLeft(), UnitTypes::Zerg_Hatchery.dimensionUp()), enemy_base_ground_, map_out_from_enemy_ground_);
        }
        else if (!expo_positions_complete_.empty()) { // maybe it's a expansion we havent' seen yet?
            expo_positions_ = expo_positions_complete_;

            int attempts = 0;
            while (Broodwar->isVisible(TilePosition(enemy_base_ground_)) && attempts < static_cast<int>(expo_positions_.size()) && (start_positions_.empty() || Broodwar->isVisible(TilePosition(start_positions_[0])))) {
                std::rotate(expo_positions_.begin(), expo_positions_.begin() + 1, expo_positions_.end());
                attempts++;
            }

            if (attempts >= static_cast<int>(expo_positions_.size())) {
                int random_index = rand() % static_cast<int>(expo_positions_.size() - 1); // random enough for our purposes.
                updateMapVeinsOut(Position(expo_positions_[random_index]) + Position(UnitTypes::Zerg_Hatchery.dimensionLeft(), UnitTypes::Zerg_Hatchery.dimensionUp()) , enemy_base_ground_, map_out_from_enemy_ground_);
            }
            else { // you can see everything but they have no enemy forces, then let's go smash randomly.
                updateMapVeinsOut(Position(expo_positions_[0]) + Position(UnitTypes::Zerg_Hatchery.dimensionLeft(), UnitTypes::Zerg_Hatchery.dimensionUp()), enemy_base_ground_, map_out_from_enemy_ground_);
            }
        }
        frames_since_enemy_base_ground_ = 0;
        return;
    }

    if (frames_since_enemy_base_air_ > 24 * 5) {

        Stored_Unit* center_flyer = CUNYAIModule::getClosestAirStored(ei, ui.getMeanAirLocation()); // If the mean location is over water, nothing will be updated. Current problem: Will not update if on

        if (ei.getMeanBuildingLocation() !=  Positions::Origin && center_flyer && center_flyer->pos_) { // Sometimes buildings get invalid positions. Unclear why. Then we need to use a more traditioanl method.
            updateMapVeinsOut(center_flyer->pos_, enemy_base_air_, map_out_from_enemy_air_, false);
        }
        else {
            enemy_base_air_ = enemy_base_ground_;
            map_out_from_enemy_air_ = map_out_from_enemy_ground_;
        }
        frames_since_enemy_base_air_ = 0;
        return;

    }

    if (frames_since_home_base > 24 * 10) {

        //otherwise go to your weakest base.
        Position suspected_friendly_base = Positions::Origin;

        if (ei.stock_fighting_total_ > 0) {
            suspected_friendly_base = getMostValuedBase(ui);
        }

        if (suspected_friendly_base.isValid() && suspected_friendly_base != home_base_ && suspected_friendly_base !=  Positions::Origin) {
            updateMapVeinsOut(suspected_friendly_base + Position(UnitTypes::Zerg_Hatchery.dimensionLeft(), UnitTypes::Zerg_Hatchery.dimensionUp()), home_base_, map_out_from_home_);
        }
        frames_since_home_base = 0;
        return;
    }

    if (frames_since_safe_base > 24 * 10) {

        //otherwise go to your safest base - the one with least deaths near it and most units.
        Position suspected_safe_base = Positions::Origin;

        suspected_safe_base = getNonCombatBase(ui, di); // If the mean location is over water, nothing will be updated. Current problem: Will not update if on building. Which we are trying to make it that way.

        if (suspected_safe_base.isValid() && suspected_safe_base != safe_base_ && suspected_safe_base !=  Positions::Origin) {
            updateMapVeinsOut(suspected_safe_base + Position(UnitTypes::Zerg_Hatchery.dimensionLeft(), UnitTypes::Zerg_Hatchery.dimensionUp()), safe_base_, map_out_from_safety_);
        }
        else {
            safe_base_ = home_base_;
            map_out_from_safety_ = map_out_from_home_;
        }

        frames_since_safe_base = 0;
        return;
    }
}


void Map_Inventory::setNextExpo( const TilePosition tp ) {
    next_expo_ = tp;
}

void Map_Inventory::drawExpoPositions() const
{
    if constexpr (DRAWING_MODE) {
        for (auto &p : expo_positions_complete_) {
            Position lower_left = Position(p);
            if (CUNYAIModule::isOnScreen(lower_left, screen_position_)) {
                lower_left.x = lower_left.x + UnitTypes::Zerg_Hatchery.width() + 32;
                lower_left.y = lower_left.y + UnitTypes::Zerg_Hatchery.height() + 32;
                Broodwar->drawBoxMap(Position(p), lower_left, Colors::Green, false);
            }
        }

        Position lower_left = Position(next_expo_);
        if (CUNYAIModule::isOnScreen(lower_left, screen_position_)) {
            lower_left.x = lower_left.x + UnitTypes::Zerg_Hatchery.width() + 32;
            lower_left.y = lower_left.y + UnitTypes::Zerg_Hatchery.height() + 32;
            Broodwar->drawBoxMap(Position(next_expo_), lower_left, Colors::Red, false);
        }
    }
}

void Map_Inventory::drawBasePositions() const
{
    if constexpr (DRAWING_MODE) {
        Broodwar->drawCircleMap(enemy_base_ground_, 15, Colors::Red, true);

        Broodwar->drawCircleMap(enemy_base_air_, 5, Colors::Orange, true);
        Broodwar->drawCircleMap(enemy_base_air_, 20, Colors::Orange, false);

        Broodwar->drawCircleMap(home_base_, 15, Colors::Green, true);

        Broodwar->drawCircleMap(safe_base_, 5, Colors::Blue, true);
        Broodwar->drawCircleMap(safe_base_, 20, Colors::Blue, false);
    }
}

void Map_Inventory::writeMap(const vector< vector<int> > &mapin, const WalkPosition &center)
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
        std::ostream_iterator<int>(merged_holding_vector, "\n"));
    // Now add the last element with no delimiter
    merged_holding_vector << holding_vector.back();

    ifstream newMap(".\\bwapi-data\\write\\" + Broodwar->mapFileName() + "Veins" + base + ".txt", ios_base::in);
    if (!newMap)
    {
        ofstream map;
        map.open(".\\bwapi-data\\write\\" + Broodwar->mapFileName() + "Veins" + base + ".txt", ios_base::app);
        //faster write of whole vector.
        map << merged_holding_vector.str() << endl;

        map.close();
    }
    newMap.close();
}

void Map_Inventory::readMap( vector< vector<int> > &mapin, const WalkPosition &center)

{
    std::stringstream ss;
    ss << center;
    int number;
    string base = ss.str();
    mapin.clear();

    ifstream newMap(".\\bwapi-data\\write\\" + Broodwar->mapFileName() + "Veins" + base + ".txt", ios_base::in);
    if (newMap)
    {
        for (int i = 0; i < Broodwar->mapWidth() * 4; i++)
        {
            mapin.push_back(std::vector<int>());
            for (int j = 0; j < Broodwar->mapHeight() * 4; j++) {
                newMap >> number;
                mapin[i].push_back(number);
            }
        }
    }
    newMap.close();
}


vector<int> Map_Inventory::getRadialDistances(const Unit_Inventory & ui, const vector<vector<int>>& map, const bool combat_units)
{
    vector<int> return_vector;

    if (!map.empty() && !ui.unit_map_.empty()) {
        for (auto u : ui.unit_map_) {
            if (u.second.type_.canAttack() && u.second.phase_ != "Retreating" || !combat_units) {
                return_vector.push_back(map[WalkPosition(u.second.pos_).x][WalkPosition(u.second.pos_).y]);
            }
        }
    }
    if (!return_vector.empty()) return return_vector;
    else return return_vector = { 0 };
}

vector< vector<int> > Map_Inventory::completeField(vector< vector<int> > pf, const int &reduction) {

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

    for (int iter = 0; iter < std::min({ tile_map_x, tile_map_y, max_value % reduction }); iter++) { // Do less iterations if we can get away with it.
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
void Map_Inventory::createThreatField(Player_Model &enemy_player) {
    int tile_map_x = Broodwar->mapWidth();
    int tile_map_y = Broodwar->mapHeight(); //tile positions are 32x32, walkable checks 8x8 minitiles.

    vector<vector<int>> pf_clear(tile_map_x, std::vector<int>(tile_map_y, 0));

    for (auto unit : enemy_player.units_.unit_map_) {
        pf_clear[TilePosition(unit.second.pos_).x][TilePosition(unit.second.pos_).y] += unit.second.ma_future_fap_value_;
    }

    pf_threat_ = completeField(pf_clear, 10);
}

// IN PROGRESS  
void Map_Inventory::createAAField(Player_Model &enemy_player) {

    int tile_map_x = Broodwar->mapWidth();
    int tile_map_y = Broodwar->mapHeight(); //tile positions are 32x32, walkable checks 8x8 minitiles.

    vector<vector<int>> pf_clear(tile_map_x, std::vector<int>(tile_map_y, 0));

    for (auto unit : enemy_player.units_.unit_map_) {
        pf_clear[TilePosition(unit.second.pos_).x][TilePosition(unit.second.pos_).y] += unit.second.ma_future_fap_value_ * unit.second.shoots_up_;
    }

    pf_aa_ = completeField(pf_clear, 5);

}

void Map_Inventory::createExploreField() {

    int tile_map_x = Broodwar->mapWidth();
    int tile_map_y = Broodwar->mapHeight(); //tile positions are 32x32, walkable checks 8x8 minitiles.

    vector<vector<int>> pf_clear(tile_map_x, std::vector<int>(tile_map_y, 0));

    for (int tile_x = 0; tile_x < tile_map_x; ++tile_x) {
        for (int tile_y = 0; tile_y < tile_map_y; ++tile_y) { // Check all possible walkable locations. Must cross over the WHOLE matrix. No sloppy bits.
            pf_clear[tile_x][tile_y] += 3 * !Broodwar->isVisible(TilePosition(tile_x, tile_y)) + 6 * !Broodwar->isExplored(TilePosition(tile_x, tile_y));
        }
    }

    pf_explore_ = completeField(pf_clear, 1);

}

void Map_Inventory::createAttractField(Player_Model &enemy_player) {
    int tile_map_x = Broodwar->mapWidth();
    int tile_map_y = Broodwar->mapHeight(); //tile positions are 32x32, walkable checks 8x8 minitiles.

    vector<vector<int>> pf_clear(tile_map_x, std::vector<int>(tile_map_y, 0));


    for (auto unit : enemy_player.units_.unit_map_) {
        pf_clear[TilePosition(unit.second.pos_).x][TilePosition(unit.second.pos_).y] += unit.second.current_stock_value_ * !CUNYAIModule::IsFightingUnit(unit.second.type_);
    }

     pf_attract_ = completeField(pf_clear, 10);

}


void Map_Inventory::DiagnosticField(vector< vector<int> > &pf) {
    if (DRAWING_MODE) {
        for (vector<int>::size_type i = 0; i < pf.size(); ++i) {
            for (vector<int>::size_type j = 0; j < pf[i].size(); ++j) {
                if (pf[i][j] > 0) {
                    if (CUNYAIModule::isOnScreen(Position(TilePosition{ static_cast<int>(i), static_cast<int>(j) }), CUNYAIModule::current_map_inventory.screen_position_)) {
                        Broodwar->drawTextMap(Position(TilePosition{ static_cast<int>(i), static_cast<int>(j) }), "%d", pf[i][j]);
                    }
                }
            }
        } // Pretty to look at!
    }
}

void Map_Inventory::DiagnosticTile() {
    if (DRAWING_MODE) {
        int tile_map_x = Broodwar->mapWidth();
        int tile_map_y = Broodwar->mapHeight(); //tile positions are 32x32, walkable checks 8x8 minitiles.
        for (auto i = 0; i < tile_map_x; ++i) {
            for (auto j = 0; j < tile_map_y; ++j) {
                if (CUNYAIModule::isOnScreen(Position(TilePosition{ static_cast<int>(i), static_cast<int>(j) }), CUNYAIModule::current_map_inventory.screen_position_)) {
                    Broodwar->drawTextMap(Position(TilePosition{ static_cast<int>(i), static_cast<int>(j) }), "%d, %d", TilePosition{ static_cast<int>(i), static_cast<int>(j) }.x, TilePosition{ static_cast<int>(i), static_cast<int>(j) }.y);
                }
            }
        }
        // Pretty to look at!
    }
}