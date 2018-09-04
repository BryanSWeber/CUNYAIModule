#pragma once

#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\InventoryManager.h"
#include "Source\Unit_Inventory.h"
#include "Source\Resource_Inventory.h"
#include <algorithm>
#include <fstream>
#include <ostream>
#include <set>

using namespace std;

// Creates a Inventory Object
Inventory::Inventory() {};
Inventory::Inventory( const Unit_Inventory &ui, const Resource_Inventory &ri ) {

    updateUnit_Counts( ui );
    updateLn_Army_Stock( ui );
    updateLn_Tech_Stock( ui );
    updateLn_Worker_Stock();
    updateVision_Count();

    updateLn_Supply_Remain();
    updateLn_Supply_Total();

    updateLn_Gas_Total();
    updateLn_Min_Total();

    updateGas_Workers();
    updateMin_Workers();

    updateMin_Possessed( ri );
    updateHatcheries();

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
        updateBaseLoc( ri );
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

// Tallies up my units for rapid counting.
void Inventory::updateUnit_Counts(const Unit_Inventory &ui) {

    vector <UnitType> already_seen;
    vector <int> unit_count_temp;
    vector <int> unit_incomplete_temp;
    for (auto const & u_iter : ui.unit_inventory_) { // should only search through unit types not per unit.
        UnitType u_type = u_iter.second.type_;
        bool new_unit_type = find(already_seen.begin(), already_seen.end(), u_type) == already_seen.end();
        if ( new_unit_type ) {
            int found_units = CUNYAIModule::Count_Units(u_type, ui);
            int incomplete_units = CUNYAIModule::Count_Units_In_Progress(u_type, ui);
            already_seen.push_back(u_type);
            unit_count_temp.push_back(found_units);
            unit_incomplete_temp.push_back(incomplete_units);
        }
    }

    unit_type_ = already_seen;
    unit_count_ = unit_count_temp;
    unit_incomplete_ = unit_incomplete_temp;
}


// Defines the (safe) log of our army stock.
void Inventory::updateLn_Army_Stock( const Unit_Inventory &ui ) {

    double total = ui.stock_fighting_total_ /*- CUNYAIModule::Stock_Units(UnitTypes::Zerg_Drone, ui)*/;

    if ( total <= 0 ) {
        total = 1;
    }

    ln_army_stock_ = log( total );
};

// Updates the (safe) log of our tech stock.
void Inventory::updateLn_Tech_Stock( const Unit_Inventory &ui ) {

    double total = 0;

    for ( int i = 132; i != 143; i++ )
    { // iterating through all tech buildings. See enumeration of unittype for details.
        UnitType build_current = (UnitType)i;
        total += CUNYAIModule::Stock_Buildings( build_current, ui );
    }

    for ( int i = 0; i != 62; i++ )
    { // iterating through all upgrades.
        UpgradeType up_current = (UpgradeType)i;
        total += CUNYAIModule::Stock_Ups( up_current );
    }

    if ( total <= 0 ) {
        total = 1;
    } // no log 0 under my watch!.

    ln_tech_stock_ = log( total );
};

// Updates the (safe) log of our worker stock. calls both worker updates to be safe.
void Inventory::updateLn_Worker_Stock() {

    double total = 0;

    double cost = Stored_Unit(UnitTypes::Zerg_Drone).stock_value_;

    updateGas_Workers();
    updateMin_Workers();

    int workers = CUNYAIModule::Count_Units(UnitTypes::Zerg_Drone, *this); //gas_workers_ + min_workers_; // this is not needed if all workers are active.

    total = cost * workers;

    if ( total <= 0 ) {
        total = 1;
    } // no log 0 under my watch!.

    ln_worker_stock_ = log( total );
};

// Updates the (safe) log of our supply stock. Looks specifically at our morphing units as "available".
void Inventory::updateLn_Supply_Remain() {

    double total = 0;
    for ( int i = 37; i != 48; i++ )
    { // iterating through all units.  (including buildings).
        UnitType u_current = (UnitType)i;
        total += CUNYAIModule::Stock_Supply( u_current, *this );
    }

    total = total - Broodwar->self()->supplyUsed();

    if ( total <= 0 ) {
        total = 1;
    } // no log 0 under my watch!.

    ln_supply_remain_ = log( total );
};

// Updates the (safe) log of our consumed supply total.
void Inventory::updateLn_Supply_Total() {

    double total = Broodwar->self()->supplyTotal();
    if ( total <= 0 ) {
        total = 1;
    } // no log 0 under my watch!.

    ln_supply_total_ = log( total );
};

// Updates the (safe) log of our gas total.
void Inventory::updateLn_Gas_Total() {

    double total = Broodwar->self()->gas();
    if ( total <= 0 ) {
        total = 1;
    } // no log 0 under my watch!.

    ln_gas_total_ = log( total );
};

// Updates the (safe) log of our mineral total.
void Inventory::updateLn_Min_Total() {

    double total = Broodwar->self()->minerals();
    if ( total <= 0 ) {
        total = 1;
    } // no log 0 under my watch!.

    ln_min_total_ = log( total );
};

// Updates the (safe) log of our gas total. Returns very high int instead of infinity.
double Inventory::getLn_Gas_Ratio() {
    // Normally:
    if ( ln_min_total_ > 0 || ln_gas_total_ > 0 ) {
        return ln_gas_total_ / (ln_min_total_ + ln_gas_total_);
    }
    else {
        return 99999;
    } // in the alternative case, you have nothing - you're mineral starved, you need minerals, not gas. Define as ~~infty, not 0.
};

// Updates the (safe) log of our supply total. Returns very high int instead of infinity.
double Inventory::getLn_Supply_Ratio() {
    // Normally:
    if ( ln_supply_total_ > 0 ) {
        return ln_supply_remain_ / ln_supply_total_;
    }
    else {
        return 0;
    } // in the alternative case, you have nothing - you're supply starved. Probably dead, too. Just in case- Define as ~~infty, not 0.
};

// Updates the count of our gas workers.
void Inventory::updateGas_Workers() {
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
void Inventory::updateMin_Workers() {
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
void Inventory::updateMin_Possessed(const Resource_Inventory &ri) {

    int min_fields = 0;
    for (auto r = ri.resource_inventory_.begin(); r != ri.resource_inventory_.end() && !ri.resource_inventory_.empty(); ++r) { //for each mineral
        if (r->second.occupied_natural_ && r->second.type_.isMineralField() ) {
                min_fields++; // if there is a base near it, then this mineral counts.
        } // closure for existance check.
    } // closure: for each mineral

    min_fields_ = min_fields;
}

// Updates the count of our vision total, in tiles
void Inventory::updateVision_Count() {
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

void Inventory::updateScreen_Position()
{
    screen_position_ = Broodwar->getScreenPosition();
}

// Updates the number of hatcheries (and decendent buildings).
void Inventory::updateHatcheries() {
    hatches_ = CUNYAIModule::Count_Units( UnitTypes::Zerg_Hatchery, *this ) +
        CUNYAIModule::Count_Units( UnitTypes::Zerg_Lair, *this ) +
        CUNYAIModule::Count_Units( UnitTypes::Zerg_Hive, *this );
}


//In Tiles?
void Inventory::updateBuildablePos()
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

void Inventory::updateUnwalkable() {
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

void Inventory::updateSmoothPos() {
    int map_x = Broodwar->mapWidth() * 4;
    int map_y = Broodwar->mapHeight() * 4; //tile positions are 32x32, walkable checks 8x8 minitiles. 
    int choke_score = 0;
    bool changed_a_value_last_cycle;

    // first, define matrixes to recieve the walkable locations for every minitile.
    smoothed_barriers_ = unwalkable_barriers_;

    for (auto iter = 2; iter < 16; iter++) { // iteration 1 is already done by labling unwalkables. Smoothout any dangerous tiles. 
        changed_a_value_last_cycle = false;
        for (auto minitile_x = 1; minitile_x <= map_x; ++minitile_x) {
            for (auto minitile_y = 1; minitile_y <= map_y; ++minitile_y) { // Check all possible walkable locations.

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

void Inventory::updateMapVeins() {
    int map_x = Broodwar->mapWidth() * 4;
    int map_y = Broodwar->mapHeight() * 4; //tile positions are 32x32, walkable checks 8x8 minitiles. 
    bool changed_a_value_last_cycle = false;

    // first, define matrixes to recieve the walkable locations for every minitile.
    map_veins_.clear();
    map_veins_ = unwalkable_barriers_with_buildings_;

    vector<WalkPosition> needs_filling;
    for (auto minitile_x = 0; minitile_x < map_x; ++minitile_x) {
        for (auto minitile_y = 0; minitile_y < map_y; ++minitile_y) { // Check all possible walkable locations.
            if (map_veins_[minitile_x][minitile_y] == 0) {
                needs_filling.push_back({ minitile_x, minitile_y });// if it is walkable, consider it a canidate for a choke.
                                                                    // Predefine list we will search over.
            }
        }
    }

    vector<unsigned> flattened_map_veins;
    for (auto minitile_x = 0; minitile_x < map_x; ++minitile_x) {
        for (auto minitile_y = 0; minitile_y < map_y; ++minitile_y) { // Check all possible walkable locations. Must cross over the WHOLE matrix. No sloppy bits.
            flattened_map_veins.push_back( map_veins_[minitile_x][minitile_y] );
        }
    }

    //x = k / map_y;
    //y = k- i*m or k % map_y;
    //k = x * map_y + y;

    for (auto iter = 2; iter < 300; iter++) { // iteration 1 is already done by labling smoothed away.
        changed_a_value_last_cycle = false;
        for ( vector<WalkPosition>::iterator position_to_investigate = needs_filling.begin(); position_to_investigate != needs_filling.end();) { // not last element !
            // Psudocode: if any two opposing points are smoothed away, while an alternative path through the center is walkable, it is a choke, the fewer cycles it takes to identify this, the tigher the choke.
            // If any 3 points adjacent are smoothed away it is probably just a bad place to walk, dead end, etc. Mark it as smoothed away.  Do not consider it smoothed away this cycle.
            // if any corner of it is inaccessable, it is a diagonal wall, mark it as smoothed away. Do not consider it smoothed away this cycle.
            // Repeat untill finished.

            //bool local_grid[3][3]; // WAY BETTER!
            bool local_grid = false; // further faster since I no longer care about actually generating the veins.
            int minitile_x = position_to_investigate->x;
            int minitile_y = position_to_investigate->y;

            bool safety_check = minitile_x > 0 && minitile_y > 0 && minitile_x + 1 < map_x && minitile_y + 1 < map_y;
            local_grid = safety_check && flattened_map_veins[(minitile_x - 1) * map_y + (minitile_y - 1)] - 1 < iter - 1 ||
                safety_check && flattened_map_veins[(minitile_x - 1) * map_y + minitile_y] - 1 < iter - 1 ||
                safety_check && flattened_map_veins[(minitile_x - 1) * map_y + (minitile_y + 1)] - 1 < iter - 1 ||
                safety_check && flattened_map_veins[minitile_x       * map_y + (minitile_y - 1)] - 1 < iter - 1 ||
                safety_check && flattened_map_veins[minitile_x       * map_y + (minitile_y + 1)] - 1 < iter - 1 ||
                safety_check && flattened_map_veins[(minitile_x + 1) * map_y + (minitile_y - 1)] - 1 < iter - 1 ||
                safety_check && flattened_map_veins[(minitile_x + 1) * map_y + minitile_y] - 1 < iter - 1 || 
                safety_check && flattened_map_veins[(minitile_x + 1) * map_y + (minitile_y + 1)] - 1 < iter - 1;

            //local_grid[0][0] = safety_check && flattened_map_veins[(minitile_x - 1) * map_y + (minitile_y - 1)] - 1 < iter - 1; // Checks if number is between upper and lower. Depends on flattened map being unsigned. SO suggests: (unsigned)(number-lower) <= (upper-lower)
            //local_grid[0][1] = safety_check && flattened_map_veins[(minitile_x - 1) * map_y + minitile_y]       - 1 < iter - 1;
            //local_grid[0][2] = safety_check && flattened_map_veins[(minitile_x - 1) * map_y + (minitile_y + 1)] - 1 < iter - 1;

            //local_grid[1][0] = safety_check && flattened_map_veins[minitile_x       * map_y + (minitile_y - 1)] - 1 < iter - 1;
            ////local_grid[1][1] = safety_check && flattened_map_veins[minitile_x       * map_y + minitile_y]       < iter && flattened_map_veins[minitile_x       * map_y + minitile_y]       > 0;
            //local_grid[1][2] = safety_check && flattened_map_veins[minitile_x       * map_y + (minitile_y + 1)] - 1 < iter - 1;

            //local_grid[2][0] = safety_check && flattened_map_veins[(minitile_x + 1) * map_y + (minitile_y - 1)] - 1 < iter - 1;
            //local_grid[2][1] = safety_check && flattened_map_veins[(minitile_x + 1) * map_y +  minitile_y]      - 1 < iter - 1;
            //local_grid[2][2] = safety_check && flattened_map_veins[(minitile_x + 1) * map_y + (minitile_y + 1)] - 1 < iter - 1;

            //// if it is surrounded, it is probably a choke, with weight inversely proportional to the number of cycles we have taken this on.
            //bool opposing_tiles =
            //    (local_grid[0][0] && (local_grid[2][2] || local_grid[2][1] || local_grid[1][2])) ||
            //    (local_grid[1][0] && (local_grid[1][2] || local_grid[0][2] || local_grid[2][2])) ||
            //    (local_grid[2][0] && (local_grid[0][2] || local_grid[0][1] || local_grid[1][2])) ||
            //    (local_grid[0][1] && (local_grid[2][1] || local_grid[2][0] || local_grid[2][2])) ||
            //    (local_grid[0][2] && (local_grid[1][0] || local_grid[2][0] || local_grid[2][1]));
            ////(local_grid[1][2] && (local_grid[0][0] || local_grid[1][0] || local_grid[2][0])) || //
            ////(local_grid[2][1] && (local_grid[0][0] || local_grid[0][1] || local_grid[0][2])) || //
            ////(local_grid[2][2] && (local_grid[0][0] || local_grid[0][1] || local_grid[1][0])) ; // several of these checks are redundant!

            //bool open_path =
            //    (!local_grid[0][0] && !local_grid[2][2]) ||
            //    (!local_grid[1][0] && !local_grid[1][2]) ||
            //    (!local_grid[2][0] && !local_grid[0][2]) ||
            //    (!local_grid[0][1] && !local_grid[2][1]); // this is symmetrical, so we only have to do half.

            //bool adjacent_tiles =
            //    local_grid[0][0] && local_grid[0][1] && local_grid[0][2] || // left edge
            //    local_grid[2][0] && local_grid[2][1] && local_grid[2][2] || // right edge
            //    local_grid[0][0] && local_grid[1][0] && local_grid[2][0] || // bottom edge
            //    local_grid[0][2] && local_grid[1][2] && local_grid[2][2] || // top edge
            //    local_grid[0][1] && local_grid[1][0] && local_grid[0][0] || // lower left slice.
            //    local_grid[0][1] && local_grid[1][2] && local_grid[0][2] || // upper left slice.
            //    local_grid[1][2] && local_grid[2][1] && local_grid[2][2] || // upper right slice.
            //    local_grid[1][0] && local_grid[2][1] && local_grid[2][0]; // lower right slice.
            
            //changed_a_value_last_cycle = opposing_tiles || adjacent_tiles || changed_a_value_last_cycle;
            //int new_value = open_path * opposing_tiles * (299 - iter) + ((!open_path && opposing_tiles) || adjacent_tiles) * iter;
            //int new_value = (opposing_tiles || adjacent_tiles) * iter;
            int new_value = local_grid * iter;
            changed_a_value_last_cycle = local_grid || changed_a_value_last_cycle;
            map_veins_[minitile_x][minitile_y] = new_value;
            flattened_map_veins[minitile_x * map_y + minitile_y] = new_value;  //should just unpack this at the end.

            if ( local_grid ) {
                std::swap(*position_to_investigate, needs_filling.back()); // note back  - last element vs end - iterator past last element!
                needs_filling.pop_back(); //std::erase preserves order and vectors are contiguous. Erase is then an O(n^2) operator. 
            }
            else { 
                ++position_to_investigate; 
            }
        }
        
        if (changed_a_value_last_cycle == false) {
            return; // if we did nothing last cycle, we don't need to punish ourselves.
        } 

    }

}

int Inventory::getMapValue(const Position & pos, const vector<vector<int>>& map)
{
    WalkPosition startloc = WalkPosition(pos);
    return map[startloc.x][startloc.y];
}


void Inventory::updateMapVeinsOut(const Position &newCenter, Position &oldCenter, vector<vector<int>> &map, const bool &print) { //in progress.

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
        int total_squares_filled = 0;

        vector <WalkPosition> fire_fill_queue;
        vector <WalkPosition> fire_fill_queue_holder;

        //begin with a flood fill.
        total_squares_filled++;
        map[minitile_x][minitile_y] = total_squares_filled;
        fire_fill_queue.push_back({ minitile_x, minitile_y });

        int minitile_x_temp = minitile_x;
        int minitile_y_temp = minitile_y;
        bool filled_a_square = false;

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

int Inventory::getDifferentialDistanceOutFromEnemy(const Position A, const Position B ) const
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

int Inventory::getRadialDistanceOutFromEnemy( const Position A) const
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

int Inventory::getDifferentialDistanceOutFromHome( const Position A, const Position B ) const
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

int Inventory::getRadialDistanceOutOnMap(const Position A, const vector<vector<int>> &map) const
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

bool Inventory::checkViableGroundPath(const Position A, const Position B) const
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
int Inventory::getRadialDistanceOutFromHome( const Position A ) const
{
    if (map_out_from_enemy_ground_.size() > 0 && A.isValid()) {
        WalkPosition wp_a = WalkPosition(A);
        int A = map_out_from_home_[(size_t)wp_a.x][(size_t)wp_a.y];
        if (A > 1) {
            return map_out_from_home_[(size_t)wp_a.x][(size_t)wp_a.y];
        }
    }

    return 9999999;

}

//void Inventory::updateLiveMapVeins( const Unit &building, const Unit_Inventory &ui, const Unit_Inventory &ei, const Resource_Inventory &ri ) { // in progress.
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
void Inventory::updateUnwalkableWithBuildings(const Unit_Inventory &ui, const Unit_Inventory &ei, const Resource_Inventory &ri, const Unit_Inventory &ni) {
    int map_x = Broodwar->mapWidth() * 4;
    int map_y = Broodwar->mapHeight() * 4; //tile positions are 32x32, walkable checks 8x8 minitiles. 

    unwalkable_barriers_with_buildings_ = unwalkable_barriers_;

    //mark all occupied areas.  IAAUW

    for (auto & u : ui.unit_inventory_) {
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

    for (auto & e : ei.unit_inventory_) {
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

    for (auto & n : ni.unit_inventory_) {
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

    for (auto & u : ri.resource_inventory_) {
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

//void Inventory::updateLiveMapVeins(const Unit_Inventory &ui, const Unit_Inventory &ei, const Resource_Inventory &ri) { // in progress.
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

//void Inventory::updateMapChokes() { // in progress. Idea : A choke is if the maximum variation of ground distances in a 5x5 tile square is LESS than some threshold. It is a plane if it is GREATER than some threshold.
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



void Inventory::updateBaseLoc( const Resource_Inventory &ri ) {

    int map_x = Broodwar->mapWidth();
    int map_y = Broodwar->mapHeight();
    int location_quality = 0;
    int residual_sq = 0;
    int resources_stored = 0;
    int search_field = 8;

    // first, define matrixes to recieve the base locations. 0 if unbuildable, 1 if buildable.
    if ( base_values_.empty() ) {
        for ( int x = 0; x <= map_x; ++x ) {
            vector<int> temp;
            for ( int y = 0; y <= map_y; ++y ) {
                temp.push_back( (int)Broodwar->isBuildable( x, y ) ); // explicit converion.
            }
            base_values_.push_back( temp );
        }
    }

    for ( auto p = ri.resource_inventory_.begin(); p != ri.resource_inventory_.end(); ++p ) { // search for closest resource group. They are our potential expos.

        TilePosition min_pos_t = TilePosition( p->second.pos_ );

        //if (p->second.type_.isMineralField()){
        //	int centralized_resource_x = p->second.pos_.x + 0.5 * UnitTypes::Resource_Mineral_Field.width();
        //	int centralized_resource_y = p->second.pos_.y + 0.5 * UnitTypes::Resource_Mineral_Field.height();
        //	min_pos_t = TilePosition(Position(centralized_resource_x, centralized_resource_y));
        //}
        //else {
        //	int centralized_resource_x = p->second.pos_.x + 0.5 * UnitTypes::Resource_Vespene_Geyser.width();
        //	int centralized_resource_y = p->second.pos_.y + 0.5 * UnitTypes::Resource_Vespene_Geyser.height();
        //	min_pos_t = TilePosition(Position(centralized_resource_x, centralized_resource_y));
        //}

        for ( auto possible_base_tile_x = min_pos_t.x - 8; possible_base_tile_x != min_pos_t.x + 8; ++possible_base_tile_x ) {
            for ( auto possible_base_tile_y = min_pos_t.y - 8; possible_base_tile_y != min_pos_t.y + 8; ++possible_base_tile_y ) { // Check wide area of possible build locations around each mineral.

                if ( possible_base_tile_x >= 0 && possible_base_tile_x <= map_x &&
                    possible_base_tile_y >= 0 && possible_base_tile_y <= map_y ) { // must be in the map bounds 

                    TilePosition prosepective_location_upper_left = { possible_base_tile_x , possible_base_tile_y }; // The build location is upper left tile of the building. T
                    TilePosition prosepective_location_upper_right = { possible_base_tile_x + UnitTypes::Zerg_Hatchery.tileWidth() , possible_base_tile_y };
                    TilePosition prosepective_location_lower_left = { possible_base_tile_x , possible_base_tile_y + UnitTypes::Zerg_Hatchery.tileHeight() };
                    TilePosition prosepective_location_lower_right = { possible_base_tile_x + UnitTypes::Zerg_Hatchery.tileWidth() , possible_base_tile_y + UnitTypes::Zerg_Hatchery.tileHeight() };

                    if ( (p->second.bwapi_unit_->getDistance( Position( prosepective_location_upper_left ) ) <= 4 * 32 ||
                        p->second.bwapi_unit_->getDistance( Position( prosepective_location_upper_right ) ) <= 4 * 32 ||
                        p->second.bwapi_unit_->getDistance( Position( prosepective_location_lower_left ) ) <= 4 * 32 ||
                        p->second.bwapi_unit_->getDistance( Position( prosepective_location_lower_right ) ) <= 4 * 32) &&
                        Broodwar->canBuildHere( prosepective_location_upper_left, UnitTypes::Zerg_Hatchery, false ) &&
                        (CUNYAIModule::isClearRayTrace(Position(prosepective_location_upper_left), Position(min_pos_t), this->unwalkable_barriers_, 1) ||
                         CUNYAIModule::isClearRayTrace(Position(prosepective_location_upper_right), Position(min_pos_t), this->unwalkable_barriers_, 1) ||
                         CUNYAIModule::isClearRayTrace(Position(prosepective_location_lower_left), Position(min_pos_t), this->unwalkable_barriers_, 1) ||
                         CUNYAIModule::isClearRayTrace(Position(prosepective_location_lower_right), Position(min_pos_t), this->unwalkable_barriers_, 1) ) ) { // if it is 3 away from the resource, and has clear vision to the resource, eg not up a wall or something.

                        int local_min = 0;

                        for ( auto j = ri.resource_inventory_.begin(); j != ri.resource_inventory_.end(); ++j ) {

                            //TilePosition tile_resource_position;

                            //if (j->second.type_.isMineralField()){
                            //	int local_resource_x = j->second.pos_.x + 0.5 * UnitTypes::Resource_Mineral_Field.width();
                            //	int local_resource_y = j->second.pos_.y + 0.5 * UnitTypes::Resource_Mineral_Field.height();
                            //	tile_resource_position = TilePosition(Position(local_resource_x, local_resource_y));
                            //}
                            //else {
                            //	int local_resource_x = j->second.pos_.x + 0.5 * UnitTypes::Resource_Vespene_Geyser.width();
                            //	int local_resource_y = j->second.pos_.y + 0.5 * UnitTypes::Resource_Vespene_Geyser.height();
                            //	tile_resource_position = TilePosition(Position(local_resource_x, local_resource_y));
                            //}

                            int long_condition = min( j->second.bwapi_unit_->getDistance( Position( prosepective_location_lower_left ) ),
                                                 min( j->second.bwapi_unit_->getDistance( Position( prosepective_location_lower_right ) ),
                                                 min( j->second.bwapi_unit_->getDistance( Position( prosepective_location_upper_left ) ),
                                                j->second.bwapi_unit_->getDistance( Position( prosepective_location_upper_right ) ) ) ) );

                            if ( long_condition <= 5 * 32 ) {
                                //residual_sq += pow(Position( TilePosition(possible_base_tile_x, possible_base_tile_y) ).getDistance(Position(tile_resource_position)) / 32, 2); //in minitiles of distance
                                resources_stored += j->second.current_stock_value_ - long_condition;
                                ++local_min;
                            }

                        }

                        if ( local_min >= 5 ) {
                            location_quality = resources_stored;
                        }

                    }
                    else {
                        location_quality = 0; // redundant, defaults to 0 - but clear.
                    } // if it's invalid for some reason return 0.

                    base_values_[possible_base_tile_x][possible_base_tile_y] = location_quality;

                    residual_sq = 0; // clear so i don't over-aggregate
                    resources_stored = 0;

                } // closure in bounds

            }
        }
    }
}

void Inventory::updateWorkersClearing( Unit_Inventory & ui, Resource_Inventory & ri )
{
    int clearing_workers_found = 0;

    if (!ui.unit_inventory_.empty()) {
        for (auto & w = ui.unit_inventory_.begin(); w != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); w++) {
            if ( w->second.isAssignedClearing(ri) ) {
                clearing_workers_found ++;
            }
        }
    }
    workers_clearing_ = clearing_workers_found;
}

void Inventory::updateWorkersLongDistanceMining(Unit_Inventory & ui, Resource_Inventory & ri)
{
    int long_distance_miners_found = 0;

    if (!ui.unit_inventory_.empty()) {
        for (auto & w = ui.unit_inventory_.begin(); w != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); w++) {
            if (w->second.isAssignedLongDistanceMining(ri)) {
                long_distance_miners_found++;
            }
        }
    }
    workers_distance_mining_ = long_distance_miners_found ;
}

Position Inventory::getWeakestBase( const Unit_Inventory &ei) const
{
    Position weakest_base = Positions::Origin;
    int stock_current_best = 0;

    for (auto expo : expo_positions_complete_) {
        Unit_Inventory ei_loc = CUNYAIModule::getUnitInventoryInRadius(ei, Position(expo), my_portion_of_the_map_);
        Unit_Inventory ei_tiny = CUNYAIModule::getUnitInventoryInRadius(ei_loc, Position(expo), my_portion_of_the_map_ * Broodwar->getPlayers().size() / (double)expo_positions_complete_.size());
        ei_loc.updateUnitInventorySummary();
        ei_tiny.updateUnitInventorySummary();
        if (ei_loc.moving_average_fap_stock_ < stock_current_best && ei_loc.stock_ground_fodder_ > 0 && ei_tiny.stock_ground_fodder_ > 0) { // if they have fodder (buildings) and it is weaker, target that place!
            stock_current_best = ei_loc.moving_average_fap_stock_;
            weakest_base = Position(expo);
        }
    }

    return weakest_base;
}

Position Inventory::getStrongestBase(const Unit_Inventory &ei) const
{
    Position strongest_base = Positions::Origin;
    int stock_current_best = 0;

    for (auto expo : expo_positions_complete_) {
        Unit_Inventory ei_loc = CUNYAIModule::getUnitInventoryInRadius(ei, Position(expo), my_portion_of_the_map_);
        Unit_Inventory ei_tiny = CUNYAIModule::getUnitInventoryInRadius(ei_loc, Position(expo), my_portion_of_the_map_ * Broodwar->getPlayers().size() / (double)expo_positions_complete_.size());
        ei_loc.updateUnitInventorySummary();
        ei_tiny.updateUnitInventorySummary();

        if (ei_loc.stock_fighting_total_ > stock_current_best && ei_loc.stock_ground_fodder_ > 0 && ei_tiny.stock_ground_fodder_ > 0) { // if they have fodder (buildings) and it is weaker, target that place!
            stock_current_best = ei_loc.stock_fighting_total_;
            strongest_base = Position(expo);
        }
    }
    
    return strongest_base;
}

//Gets position of base nearby greatest imbalance of combat units.
Position Inventory::getAttackedBase(const Unit_Inventory & ei, const Unit_Inventory & ui) const
{
    //// Attempt 1.
    Position attacked_base = Positions::Origin;
    int largest_current_conflict = 0;
    int temp_worst_base = 0;

    for (auto expo : expo_positions_complete_) {
        Unit_Inventory ei_loc = CUNYAIModule::getUnitInventoryInRadius(ei, Position(expo), my_portion_of_the_map_);
        Unit_Inventory ui_loc = CUNYAIModule::getUnitInventoryInRadius(ui, Position(expo), my_portion_of_the_map_ * Broodwar->getPlayers().size() / (double)expo_positions_complete_.size());
        ei_loc.updateUnitInventorySummary();
        ui_loc.updateUnitInventorySummary();
        temp_worst_base = ei_loc.stock_fighting_total_ - ui_loc.stock_fighting_total_; //total future losses at base.
        if ( temp_worst_base > largest_current_conflict && ui_loc.stock_ground_fodder_ > 0 ) { // you've got to have a building in that area.
            largest_current_conflict = temp_worst_base;
            attacked_base = Position(expo);
        }
    }

    return attacked_base;

}

////Gets Position of base with the most attackers outside of it.
Position Inventory::getBaseWithMostAttackers(const Unit_Inventory & ei, const Unit_Inventory & ui) const
{
    Position attacked_base = Positions::Origin;
    int largest_current_conflict = 0;
    int temp_worst_base = 0;

    for (auto expo : expo_positions_complete_) {
        Unit_Inventory ei_loc = CUNYAIModule::getUnitInventoryInRadius(ei, Position(expo), my_portion_of_the_map_);
        Unit_Inventory ui_loc = CUNYAIModule::getUnitInventoryInRadius(ui, Position(expo), my_portion_of_the_map_ * Broodwar->getPlayers().size() / (double)expo_positions_complete_.size());
        ei_loc.updateUnitInventorySummary();
        ui_loc.updateUnitInventorySummary();
        temp_worst_base = ei_loc.is_shooting_ + 10 * ui_loc.is_shooting_; //total future losses at base.
        if (temp_worst_base > largest_current_conflict && ui_loc.stock_ground_fodder_ > 0) { // you've got to have a building in that area.
            largest_current_conflict = temp_worst_base;
            attacked_base = Position(expo);
        }
    }

    return attacked_base;
}

void Inventory::getExpoPositions() {

    expo_positions_.clear();

    int location_qual_threshold = -999999;
    //Region home = Broodwar->getRegionAt(Position(center_self));
    //Regionset neighbors;
    bool local_maximum = true;

    int map_x = Broodwar->mapWidth();
    int map_y = Broodwar->mapHeight();

    // if we haven't checked before, start from the beginning.
    if ( expo_positions_complete_.empty() ) {
        for (vector<int>::size_type x = 0; x != map_x; ++x) {
            for (vector<int>::size_type y = 0; y != map_y; ++y) {
                if (base_values_[x][y] > 1) { // only consider the decent locations please.

                    local_maximum = true;

                    //TilePosition canidate_spot = TilePosition(x + 2, y + 1); // from the true center of the object.
                    //int walk = Position( canidate_spot ).getDistance( Position( center_self ) ) / 32;
                    //int net_quality = base_values_[x][y]; //value of location and distance from our center.  Plus some terms so it's positive, we like to look at positive numbers.

                    for (int i = -12; i <= 12; i++) {
                        for (int j = -12; j <= 12; j++) {
                            bool safety_check = TilePosition(x + i, y + j).isValid() ; //valid tile position
                            if (safety_check && base_values_[x][y] < base_values_[x + i][y + j]) {
                                local_maximum = false;
                                break;
                            }
                        }
                    }

                    if (local_maximum) {
                        expo_positions_.push_back({ static_cast<int>(x), static_cast<int>(y) });
                    }
                }
            } // closure y
        } // closure x
    }
    else { // only check potentially relevant locations.
        for (TilePosition canidate_spot : expo_positions_complete_) {
            local_maximum = true;

            int x = canidate_spot.x;
            int y = canidate_spot.y;
            
            for (int i = -12; i <= 12; i++) {
                for (int j = -12; j <= 12; j++) {
                    bool safety_check = TilePosition(x + i, y + j).isValid(); //valid tile position
                    if (safety_check && base_values_[x][y] < base_values_[x + i][y + j]) {
                        local_maximum = false;
                        break;
                    }
                }
            }

            if (local_maximum) {
                expo_positions_.push_back(canidate_spot);
            }
        }
    
    }


    //From SO, quick conversion into set.
    set<TilePosition> s;
    unsigned size = expo_positions_.size();
    for (unsigned i = 0; i < size; ++i) s.insert(expo_positions_[i]);
    expo_positions_complete_.assign(s.begin(), s.end());
}

void Inventory::getStartPositions() {
    for ( auto loc : Broodwar->getStartLocations() ) {
        start_positions_.push_back( Position( loc ) );
    }
}

void Inventory::updateStartPositions(const Unit_Inventory &ei) {
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

void Inventory::updateBasePositions(Unit_Inventory &ui, Unit_Inventory &ei, const Resource_Inventory &ri, const Unit_Inventory &ni) {

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
        updateUnwalkableWithBuildings(ui, ei, ri, ni);
        frames_since_unwalkable = 0;
        return;
    }
    
    if (frames_since_map_veins > 24 * 30) { // impose a second wait here because we don't want to update this if we're discovering buildings rapidly.

        updateMapVeins();
        frames_since_map_veins = 0;
        return;
    }
    
    if (frames_since_enemy_base_ground_ > 24 * 10) {
        checked_all_expo_positions_ = false;

        Stored_Unit* center_building = CUNYAIModule::getClosestGroundStored(ei, ui.getMeanLocation(), *this); // If the mean location is over water, nothing will be updated. Current problem: Will not update if on 

        if (ei.getMeanBuildingLocation() != Positions::Origin && center_building && center_building->pos_ && center_building->pos_ != Positions::Origin) { // Sometimes buildings get invalid positions. Unclear why. Then we need to use a more traditioanl method. 
            updateMapVeinsOut( center_building->pos_, enemy_base_ground_, map_out_from_enemy_ground_, false);
        }
        else if (!start_positions_.empty() && start_positions_[0] && start_positions_[0] !=  Positions::Origin && !cleared_all_start_positions_) { // maybe it's a base we havent' seen yet?
            int attempts = 0;
            while (Broodwar->isVisible(TilePosition(enemy_base_ground_)) && attempts < start_positions_.size()) {
                std::rotate(start_positions_.begin(), start_positions_.begin() + 1, start_positions_.end());
                attempts++;
            }
            updateMapVeinsOut( start_positions_[0], enemy_base_ground_, map_out_from_enemy_ground_);
        }
        else if (!expo_positions_complete_.empty()) { // maybe it's a base we havent' seen yet?
            expo_positions_ = expo_positions_complete_;
            int random_index = rand() % expo_positions_.size(); // random enough for our purposes.
            checked_all_expo_positions_ = true;
            expo_positions_ = expo_positions_complete_;
            while (Broodwar->isVisible(TilePosition(enemy_base_ground_)) && !expo_positions_.empty() && Broodwar->isVisible(expo_positions_[random_index])) { // Check any expo we're not looking at.
                random_index = rand() % expo_positions_.size();
                if (Broodwar->isVisible(expo_positions_[random_index])) {
                    expo_positions_.erase(expo_positions_.begin() + random_index);
                }
            }

            if (!expo_positions_.empty()) {
                updateMapVeinsOut(Position(expo_positions_[random_index]), enemy_base_ground_, map_out_from_enemy_ground_);
            }
        }
        frames_since_enemy_base_ground_ = 0;
        return;
    }
    
    if (frames_since_enemy_base_air_ > 24 * 5) {
    
        Stored_Unit* center_flyer = CUNYAIModule::getClosestAirStored(ei, ui.getMeanAirLocation(), *this); // If the mean location is over water, nothing will be updated. Current problem: Will not update if on 

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
            suspected_friendly_base = getAttackedBase(ei, ui); // If the mean location is over water, nothing will be updated. Current problem: Will not update if on building. Which we are trying to make it that way.
        }
        else {
            suspected_friendly_base = getStrongestBase(ui);
        }

        if (suspected_friendly_base.isValid() && suspected_friendly_base != home_base_ && suspected_friendly_base !=  Positions::Origin) {
            updateMapVeinsOut(suspected_friendly_base, home_base_, map_out_from_home_);
        }
        frames_since_home_base = 0;
        return;
    }

    if (frames_since_safe_base > 24 * 10) {

        //otherwise go to your weakest base.
        Position suspected_safe_base = Positions::Origin;

        suspected_safe_base = getStrongestBase(ui); // If the mean location is over water, nothing will be updated. Current problem: Will not update if on building. Which we are trying to make it that way.

        if (suspected_safe_base.isValid() && suspected_safe_base != safe_base_ && suspected_safe_base !=  Positions::Origin) {
            updateMapVeinsOut(suspected_safe_base, safe_base_, map_out_from_safety_);
        }
        else {
            safe_base_ = home_base_;
            map_out_from_safety_ = map_out_from_home_;
        }

        frames_since_safe_base = 0;
        return;
    }
}


void Inventory::setNextExpo( const TilePosition tp ) {
    next_expo_ = tp;
}

void Inventory::drawExpoPositions() const
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

void Inventory::drawBasePositions() const
{
    if constexpr (DRAWING_MODE) {
        Broodwar->drawCircleMap(enemy_base_ground_, 25, Colors::Red, true);

        Broodwar->drawCircleMap(enemy_base_air_, 5, Colors::Orange, true);
        Broodwar->drawCircleMap(enemy_base_air_, 30, Colors::Orange, false);

        Broodwar->drawCircleMap(home_base_, 25, Colors::Green, true);

        Broodwar->drawCircleMap(safe_base_, 5, Colors::Blue, true);
        Broodwar->drawCircleMap(safe_base_, 30, Colors::Blue, false);
    }
}

void Inventory::writeMap(const vector< vector<int> > &mapin, const WalkPosition &center)
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

    int number;
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

void Inventory::readMap( vector< vector<int> > &mapin, const WalkPosition &center)

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


vector<int> Inventory::getRadialDistances(const Unit_Inventory & ui, const vector<vector<int>>& map)
{
    vector<int> return_vector;

    if (!map.empty()) {
        for (auto u : ui.unit_inventory_) {
            return_vector.push_back(map[WalkPosition(u.second.pos_).x][WalkPosition(u.second.pos_).y]);
        }
        return return_vector;

    }

    return return_vector = { 0 };
}
