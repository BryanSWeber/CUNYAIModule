#pragma once

#include <BWAPI.h>
#include "Source\MeatAIModule.h"
#include "Source\InventoryManager.h"
#include "Source\Unit_Inventory.h"
#include "Source\Resource_Inventory.h"


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

    if ( smoothed_barriers_.size() == 0 ) {

        updateSmoothPos();
        int unwalkable_ct = 0;
        for ( vector<int>::size_type i = 0; i != smoothed_barriers_.size(); ++i ) {
            for ( vector<int>::size_type j = 0; j != smoothed_barriers_[i].size(); ++j ) {
                unwalkable_ct += smoothed_barriers_[i][j];
            }
        }
        Broodwar->sendText( "There are %d tiles, and %d smoothed out tiles.", smoothed_barriers_.size(), unwalkable_ct );
    }

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
        Broodwar->sendText( "There are %d resources on the map, %d canidate expo positions.", ri.resource_inventory_.size(), buildable_ct );
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
        Broodwar->sendText( "There are %d roughly tiles, %d veins.", map_veins_.size(), vein_ct );
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
            int found_units = MeatAIModule::Count_Units(u_type, ui);
            int incomplete_units = MeatAIModule::Count_Units_In_Progress(u_type, ui);
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

    double total = ui.stock_total_ - MeatAIModule::Stock_Units(UnitTypes::Zerg_Drone, ui);

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
        total += MeatAIModule::Stock_Buildings( build_current, ui );
    }

    for ( int i = 0; i != 62; i++ )
    { // iterating through all upgrades.
        UpgradeType up_current = (UpgradeType)i;
        total += MeatAIModule::Stock_Ups( up_current );
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

    int workers = gas_workers_ + min_workers_;

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
        total += MeatAIModule::Stock_Supply( u_current, *this );
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
        return 99999;
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
    hatches_ = MeatAIModule::Count_Units( UnitTypes::Zerg_Hatchery, *this ) +
        MeatAIModule::Count_Units( UnitTypes::Zerg_Lair, *this ) +
        MeatAIModule::Count_Units( UnitTypes::Zerg_Hive, *this );
}


//In Tiles?
void Inventory::updateBuildablePos()
{
    //Buildable_positions_ = std::vector< std::vector<bool> >( BWAPI::Broodwar->mapWidth()/8, std::vector<bool>( BWAPI::Broodwar->mapHeight()/8, false ) );

    int map_x = Broodwar->mapWidth();
    int map_y = Broodwar->mapHeight();
    for ( int x = 0; x <= map_x; ++x ) {
        vector<bool> temp;
        for ( int y = 0; y <= map_y; ++y ) {
            temp.push_back( Broodwar->isBuildable( x, y ) );
        }
        buildable_positions_.push_back( temp );
    }
};

void Inventory::updateUnwalkable() {
    int map_x = Broodwar->mapWidth() * 4;
    int map_y = Broodwar->mapHeight() * 4; //tile positions are 32x32, walkable checks 8x8 minitiles. 
    int choke_score = 0;

    // first, define matrixes to recieve the walkable locations for every minitile.
    for ( int x = 0; x <= map_x; ++x ) {
        vector<int> temp;
        for ( int y = 0; y <= map_y; ++y ) {
            temp.push_back( !Broodwar->isWalkable( x, y ) );
        }
        unwalkable_barriers_.push_back( temp );
    }

}

void Inventory::updateSmoothPos() {
    int map_x = Broodwar->mapWidth() * 4;
    int map_y = Broodwar->mapHeight() * 4; //tile positions are 32x32, walkable checks 8x8 minitiles. 
    int choke_score = 0;

    // first, define matrixes to recieve the walkable locations for every minitile.
    smoothed_barriers_ = unwalkable_barriers_;

    for ( auto iter = 2; iter < 100; iter++ ) { // iteration 1 is already done by labling unwalkables.
        for ( auto minitile_x = 1; minitile_x <= map_x; ++minitile_x ) {
            for ( auto minitile_y = 1; minitile_y <= map_y; ++minitile_y ) { // Check all possible walkable locations.

                                                               // Psudocode: if any two opposing points are unwalkable, or the corners are blocked off, while an alternative path through the center is walkable, it can be smoothed out, the fewer cycles it takes to identify this, the rougher the surface.
                                                                             // Repeat untill finished.

                if ( smoothed_barriers_[minitile_x][minitile_y] == 0 ) { // if it is walkable, consider it a canidate for a choke.

                                                                         // Predefine grid we will search over.
                    bool local_tile_0_0 = (smoothed_barriers_[(minitile_x - 1)][(minitile_y - 1)] < iter && smoothed_barriers_[(minitile_x - 1)][(minitile_y - 1)] > 0);
                    bool local_tile_1_0 = (smoothed_barriers_[minitile_x][(minitile_y - 1)]       < iter && smoothed_barriers_[minitile_x][(minitile_y - 1)] > 0);
                    bool local_tile_2_0 = (smoothed_barriers_[(minitile_x + 1)][(minitile_y - 1)] < iter && smoothed_barriers_[(minitile_x + 1)][(minitile_y - 1)] > 0);

                    bool local_tile_0_1 = (smoothed_barriers_[(minitile_x - 1)][minitile_y] < iter  && smoothed_barriers_[(minitile_x - 1)][minitile_y] > 0);
                    bool local_tile_1_1 = (smoothed_barriers_[minitile_x][minitile_y]       < iter  && smoothed_barriers_[minitile_x][minitile_y] > 0);
                    bool local_tile_2_1 = (smoothed_barriers_[(minitile_x + 1)][minitile_y] < iter  && smoothed_barriers_[(minitile_x + 1)][minitile_y]> 0);

                    bool local_tile_0_2 = (smoothed_barriers_[(minitile_x - 1)][(minitile_y + 1)]  < iter  && smoothed_barriers_[(minitile_x - 1)][(minitile_y + 1)] > 0);
                    bool local_tile_1_2 = (smoothed_barriers_[minitile_x][(minitile_y + 1)]        < iter  && smoothed_barriers_[minitile_x][(minitile_y + 1)] > 0);
                    bool local_tile_2_2 = (smoothed_barriers_[(minitile_x + 1)][(minitile_y + 1)]  < iter  && smoothed_barriers_[(minitile_x + 1)][(minitile_y + 1)] > 0);

                    // if it is surrounded, it is probably a choke, with weight inversely proportional to the number of cycles we have taken this on.
                    bool opposing_tiles =
                        (local_tile_0_0 && (local_tile_2_2 || local_tile_2_1 || local_tile_1_2)) ||
                        (local_tile_1_0 && (local_tile_1_2 || local_tile_0_2 || local_tile_2_2)) ||
                        (local_tile_2_0 && (local_tile_0_2 || local_tile_0_1 || local_tile_1_2)) ||
                        (local_tile_0_1 && (local_tile_2_1 || local_tile_2_0 || local_tile_2_2));

                    bool open_path =
                        (!local_tile_0_0 && (!local_tile_2_2 || !local_tile_2_1 || !local_tile_1_2)) ||
                        (!local_tile_1_0 && (!local_tile_1_2 || !local_tile_0_2 || !local_tile_2_2)) ||
                        (!local_tile_2_0 && (!local_tile_0_2 || !local_tile_0_1 || !local_tile_1_2)) ||
                        (!local_tile_0_1 && (!local_tile_2_1 || !local_tile_2_0 || !local_tile_2_2));

                    if ( open_path && (opposing_tiles) ) { // if it is closing off, but still has open space, mark as special and continue.  Will prevent algorithm from sealing map.
                        smoothed_barriers_[minitile_x][minitile_y] = 99 - iter;
                    }

                    if ( !open_path && (opposing_tiles) ) { // if it is closed off or blocked, then seal it up and continue. 
                        smoothed_barriers_[minitile_x][minitile_y] = iter;
                    }

                }
            }
        }
    }
}

void Inventory::updateMapVeins() {
    int map_x = Broodwar->mapWidth() * 4;
    int map_y = Broodwar->mapHeight() * 4; //tile positions are 32x32, walkable checks 8x8 minitiles. 

    // first, define matrixes to recieve the map_vein locations for every minitile.
    map_veins_ = unwalkable_barriers_;

    for ( auto iter = 2; iter < 300; iter++ ) { // iteration 1 is already done by labling unwalkables.
        for ( auto minitile_x = 1; minitile_x <= map_x; ++minitile_x ) {
            for ( auto minitile_y = 1; minitile_y <= map_y; ++minitile_y ) { // Check all possible walkable locations.

                                                                             // Psudocode: if any two opposing points are unwalkable, while an alternative path through the center is walkable, it is a choke, the fewer cycles it takes to identify this, the tigher the choke.
                                                                             // If any 3 points adjacent are unwalkable it is probably just a bad place to walk, dead end, etc. Mark it as unwalkable.  Do not consider it unwalkable this cycle.
                                                                             // if any corner of it is inaccessable, it is a diagonal wall, mark it as unwalkable. Do not consider it unwalkable this cycle.
                                                                             // Repeat untill finished.

                if ( map_veins_[minitile_x][minitile_y] == 0 ) { // if it is walkable, consider it a canidate for a choke.

                                                                 // Predefine grid we will search over.
                    bool local_tile_0_0 = (map_veins_[(minitile_x - 1)][(minitile_y - 1)] < iter && map_veins_[(minitile_x - 1)][(minitile_y - 1)] > 0);
                    bool local_tile_1_0 = (map_veins_[minitile_x][(minitile_y - 1)]       < iter && map_veins_[minitile_x][(minitile_y - 1)] > 0);
                    bool local_tile_2_0 = (map_veins_[(minitile_x + 1)][(minitile_y - 1)] < iter && map_veins_[(minitile_x + 1)][(minitile_y - 1)] > 0);

                    bool local_tile_0_1 = (map_veins_[(minitile_x - 1)][minitile_y] < iter  && map_veins_[(minitile_x - 1)][minitile_y] > 0);
                    bool local_tile_1_1 = (map_veins_[minitile_x][minitile_y]       < iter  && map_veins_[minitile_x][minitile_y] > 0);
                    bool local_tile_2_1 = (map_veins_[(minitile_x + 1)][minitile_y] < iter  && map_veins_[(minitile_x + 1)][minitile_y]> 0);

                    bool local_tile_0_2 = (map_veins_[(minitile_x - 1)][(minitile_y + 1)]  < iter  && map_veins_[(minitile_x - 1)][(minitile_y + 1)] > 0);
                    bool local_tile_1_2 = (map_veins_[minitile_x][(minitile_y + 1)]        < iter  && map_veins_[minitile_x][(minitile_y + 1)] > 0);
                    bool local_tile_2_2 = (map_veins_[(minitile_x + 1)][(minitile_y + 1)]  < iter  && map_veins_[(minitile_x + 1)][(minitile_y + 1)] > 0);

                    // if it is surrounded, it is probably a choke, with weight inversely proportional to the number of cycles we have taken this on.
                    bool opposing_tiles =
                        (local_tile_0_0 && (local_tile_2_2 || local_tile_2_1 || local_tile_1_2)) ||
                        (local_tile_1_0 && (local_tile_1_2 || local_tile_0_2 || local_tile_2_2)) ||
                        (local_tile_2_0 && (local_tile_0_2 || local_tile_0_1 || local_tile_1_2)) ||
                        (local_tile_0_1 && (local_tile_2_1 || local_tile_2_0 || local_tile_2_2));

                    bool open_path =
                        (!local_tile_0_0 && !local_tile_2_2) ||
                        (!local_tile_1_0 && !local_tile_1_2) ||
                        (!local_tile_2_0 && !local_tile_0_2) ||
                        (!local_tile_0_1 && !local_tile_2_1);

                    bool adjacent_tiles =
                        local_tile_0_0 && local_tile_0_1 && local_tile_0_2 || // left edge
                        local_tile_2_0 && local_tile_2_1 && local_tile_2_2 || // right edge
                        local_tile_0_0 && local_tile_1_0 && local_tile_2_0 || // bottom edge
                        local_tile_0_2 && local_tile_1_2 && local_tile_2_2 || // top edge
                        local_tile_0_1 && local_tile_1_0 && local_tile_0_0 || // lower left slice.
                        local_tile_0_1 && local_tile_1_2 && local_tile_0_2 || // upper left slice.
                        local_tile_1_2 && local_tile_2_1 && local_tile_2_2 || // upper right slice.
                        local_tile_1_0 && local_tile_2_1 && local_tile_2_0; // lower right slice.

                    if ( open_path && opposing_tiles ) {  //mark chokes when found.
                        map_veins_[minitile_x][minitile_y] = 299 - iter;
                    }
                    else if ( (!open_path && opposing_tiles) || adjacent_tiles ) { //if it is closing off in any other way than "formal choke"- it's just a bad place to walk. Mark as unwalkable and continue. Will seal map.
                        map_veins_[minitile_x][minitile_y] = iter;
                    }
                }
            }
        }
    }
}

void Inventory::updateMapVeinsOutFromMain(const Position center) { //in progress.

    int map_x = Broodwar->mapWidth() * 4;
    int map_y = Broodwar->mapHeight() * 4; //tile positions are 32x32, walkable checks 8x8 minitiles. 
    WalkPosition startloc = WalkPosition( center );

    if (!map_veins_out_from_main_.empty() && unwalkable_barriers_[startloc.x][startloc.y] != 0 ) {
        return;
    }
    else {
        map_veins_out_from_main_.clear();
    }
    // first, define matrixes to recieve the walkable locations for every minitile.
    map_veins_out_from_main_ = unwalkable_barriers_;

    int minitile_x, minitile_y, distance_right_x, distance_below_y;
    minitile_x = startloc.x;
    minitile_y = startloc.y;
    distance_right_x = max( map_x - minitile_x, map_x );
    distance_below_y = max( map_y - minitile_y, map_y );
    int t = std::max( map_x + distance_right_x + distance_below_y, map_y + distance_right_x + distance_below_y );
    //int maxI = t*t; // total number of spiral steps we have to make.
    int total_squares_filled = 0;
    //int steps_until_next_turn = 1;
    //int steps_since_last_turn = 0;
    //bool turn_trigger = false;
    //int turns_at_this_count = 0;
    //int number_of_turns = 0;
    //int direction = 1;

    vector <WalkPosition> fire_fill_queue;

    //begin with a fire fill.
        total_squares_filled++;
        map_veins_out_from_main_[minitile_x][minitile_y] = total_squares_filled;
        fire_fill_queue.push_back( { minitile_x, minitile_y } );

        int minitile_x_temp = minitile_x;
        int minitile_y_temp = minitile_y;

        while ( !fire_fill_queue.empty() ) { // this portion is a fire fill.

            minitile_x_temp = fire_fill_queue.begin()->x;
            minitile_y_temp = fire_fill_queue.begin()->y;
            fire_fill_queue.erase( fire_fill_queue.begin() );

            // north
            if ( minitile_y_temp + 1 < map_y && map_veins_[minitile_x_temp][minitile_y_temp + 1] > 0 && map_veins_out_from_main_[minitile_x_temp][minitile_y_temp + 1] == 0 ) {
                total_squares_filled++;
                map_veins_out_from_main_[minitile_x_temp][minitile_y_temp + 1] = total_squares_filled;
                fire_fill_queue.push_back( { minitile_x_temp , minitile_y_temp + 1 } );
            }
            // north east
            if ( minitile_x_temp + 1 < map_x && minitile_y_temp + 1 < map_y && map_veins_[minitile_x_temp + 1][minitile_y_temp + 1] > 0 && map_veins_out_from_main_[minitile_x_temp + 1][minitile_y_temp + 1] == 0 ) {
                total_squares_filled++;
                map_veins_out_from_main_[minitile_x_temp + 1][minitile_y_temp + 1] = total_squares_filled;
                fire_fill_queue.push_back( { minitile_x_temp + 1, minitile_y_temp + 1 } );
            }
            // north west
            if ( 0 < minitile_x_temp - 1 && minitile_y_temp + 1 < map_y && map_veins_[minitile_x_temp - 1][minitile_y_temp + 1] > 0 && map_veins_out_from_main_[minitile_x_temp - 1][minitile_y_temp + 1] == 0 ) {
                total_squares_filled++;
                map_veins_out_from_main_[minitile_x_temp - 1][minitile_y_temp + 1] = total_squares_filled;
                fire_fill_queue.push_back( { minitile_x_temp - 1, minitile_y_temp + 1 } );
            }
            //south east
            if ( minitile_x_temp + 1 < map_x && 0 < minitile_y_temp - 1 && map_veins_[minitile_x_temp + 1][minitile_y_temp - 1] > 0 && map_veins_out_from_main_[minitile_x_temp + 1][minitile_y_temp - 1] == 0 ) {
                total_squares_filled++;
                map_veins_out_from_main_[minitile_x_temp + 1][minitile_y_temp - 1] = total_squares_filled;
                fire_fill_queue.push_back( { minitile_x_temp + 1, minitile_y_temp - 1 } );
            }
            //south west
            if ( 0 < minitile_x_temp - 1 && 0 < minitile_y_temp - 1 && map_veins_[minitile_x_temp - 1][minitile_y_temp - 1] > 0 && map_veins_out_from_main_[minitile_x_temp - 1][minitile_y_temp - 1] == 0 ) {
                total_squares_filled++;
                map_veins_out_from_main_[minitile_x_temp - 1][minitile_y_temp - 1] = total_squares_filled;
                fire_fill_queue.push_back( { minitile_x_temp - 1, minitile_y_temp - 1 } );
            }
            // east
            if ( minitile_x_temp + 1 < map_x && map_veins_[minitile_x_temp + 1][minitile_y_temp] > 0 && map_veins_out_from_main_[minitile_x_temp + 1][minitile_y_temp] == 0 ) {
                total_squares_filled++;
                map_veins_out_from_main_[minitile_x_temp + 1][minitile_y_temp] = total_squares_filled;
                fire_fill_queue.push_back( { minitile_x_temp + 1, minitile_y_temp } );
            }
            //west
            if ( 0 < minitile_x_temp - 1 && map_veins_[minitile_x_temp - 1][minitile_y_temp] > 0 && map_veins_out_from_main_[minitile_x_temp - 1][minitile_y_temp] == 0 ) {
                total_squares_filled++;
                map_veins_out_from_main_[minitile_x_temp - 1][minitile_y_temp] = total_squares_filled;
                fire_fill_queue.push_back( { minitile_x_temp - 1, minitile_y_temp } );
            }
        }
        //for ( int i = 0; i < maxI; ) {
        //    if ( (0 < minitile_x) && (minitile_x < map_x) && (0 < minitile_y) && (minitile_y < map_y) ) { // if you are on the map, continue.

        //            // Working flood fill below:
        //        if ( map_veins_out_from_main_[minitile_x][minitile_y] == 0 /*&& map_veins_[minitile_x][minitile_y] > 175*/ ) { // if it is walkable, consider it a canidate for a choke.
        //            total_squares_filled++;
        //            map_veins_out_from_main_[minitile_x][minitile_y] = total_squares_filled;
        //            bool dead_end = false;
        //            int minitile_x_temp = minitile_x;
        //            int minitile_y_temp = minitile_y;
        //            int total_squares_filled_temp = total_squares_filled;
        //            while ( !dead_end ) { // this portion is a flood fill.
        //                // north
        //                if ( minitile_y_temp + 1 < map_y && map_veins_[minitile_x_temp][minitile_y_temp + 1] > 1 && map_veins_out_from_main_[minitile_x_temp][minitile_y_temp + 1] == 0 ) {
        //                    total_squares_filled_temp++;
        //                    minitile_y_temp++;
        //                    map_veins_out_from_main_[minitile_x_temp][minitile_y_temp] = total_squares_filled_temp;
        //                    continue;
        //                }
        //                // north east
        //                if ( minitile_y_temp + 1 < map_y && minitile_x_temp + 1 < map_x && map_veins_[minitile_x_temp + 1][minitile_y_temp + 1] > 1 && map_veins_out_from_main_[minitile_x_temp + 1][minitile_y_temp + 1] == 0 ) {
        //                    total_squares_filled_temp++;
        //                    minitile_x_temp++;
        //                    minitile_y_temp++;
        //                    map_veins_out_from_main_[minitile_x_temp][minitile_y_temp] = total_squares_filled_temp;
        //                    continue;
        //                }
        //                // north west
        //                if ( minitile_y_temp + 1 < map_y && 0 < minitile_x_temp - 1 && map_veins_[minitile_x_temp - 1][minitile_y_temp + 1] > 1 && map_veins_out_from_main_[minitile_x_temp - 1][minitile_y_temp + 1] == 0 ) {
        //                    total_squares_filled_temp++;
        //                    minitile_x_temp--;
        //                    minitile_y_temp++;
        //                    map_veins_out_from_main_[minitile_x_temp][minitile_y_temp] = total_squares_filled_temp;
        //                    continue;
        //                }
        //                //south east
        //                if ( 0 < minitile_y_temp - 1 && minitile_x_temp + 1 < map_x && map_veins_[minitile_x_temp + 1][minitile_y_temp - 1] > 1 && map_veins_out_from_main_[minitile_x_temp + 1][minitile_y_temp - 1] == 0 ) {
        //                    total_squares_filled_temp++;
        //                    minitile_x_temp++;
        //                    minitile_y_temp--;
        //                    map_veins_out_from_main_[minitile_x_temp][minitile_y_temp] = total_squares_filled_temp;
        //                    continue;
        //                }
        //                //south west
        //                if ( 0 < minitile_y_temp - 1 && 0 < minitile_x_temp - 1 && map_veins_[minitile_x_temp - 1][minitile_y_temp - 1] > 1 && map_veins_out_from_main_[minitile_x_temp - 1][minitile_y_temp - 1] == 0 ) {
        //                    total_squares_filled_temp++;
        //                    minitile_y_temp--;
        //                    minitile_x_temp--;
        //                    map_veins_out_from_main_[minitile_x_temp][minitile_y_temp] = total_squares_filled_temp;
        //                    continue;
        //                }
        //                // east
        //                if ( minitile_x_temp + 1 < map_x && map_veins_[minitile_x_temp + 1][minitile_y_temp] > 1 && map_veins_out_from_main_[minitile_x_temp + 1][minitile_y_temp] == 0 ) {
        //                    total_squares_filled_temp++;
        //                    minitile_x_temp++;
        //                    map_veins_out_from_main_[minitile_x_temp][minitile_y_temp] = total_squares_filled_temp;
        //                    continue;
        //                }
        //                //west
        //                if ( 0 < minitile_x_temp - 1 && map_veins_[minitile_x_temp - 1][minitile_y_temp] > 1 && map_veins_out_from_main_[minitile_x_temp - 1][minitile_y_temp] == 0 ) {
        //                    total_squares_filled_temp++;
        //                    minitile_x_temp--;
        //                    map_veins_out_from_main_[minitile_x_temp][minitile_y_temp] = total_squares_filled_temp;
        //                    continue;
        //                }
        //                dead_end = true;
        //            }
        //        }
        //    }

        //    if ( steps_since_last_turn == steps_until_next_turn ) { // this lower portion is a spiral fill, beginning at the start node of the fire fill.
        //        turn_trigger = true;
        //    }

        //    if ( turn_trigger ) {

        //        switch ( direction )
        //        {
        //        case 1:
        //            direction = 4;
        //            break;
        //        case 2:
        //            direction = 1;
        //            break;
        //        case 3:
        //            direction = 2;
        //            break;
        //        case 4:
        //            direction = 3;
        //            break;
        //        }

        //        if ( turns_at_this_count == 2 ) {
        //            steps_until_next_turn++;
        //            turns_at_this_count = 0;
        //        }
        //        else {
        //            turns_at_this_count++;
        //        }
        //        steps_since_last_turn = 0;
        //        number_of_turns++;
        //        turn_trigger = false;
        //    }

        //    switch ( direction )
        //    {
        //    case 1:
        //        minitile_y--;
        //        break;
        //    case 2:
        //        minitile_x++;
        //        break;
        //    case 3:
        //        minitile_y++;
        //        break;
        //    case 4:
        //        minitile_x--;
        //        break;
        //    }

        //    steps_since_last_turn++;
        //    i++;
        //}
}

void Inventory::updateMapVeinsOutFromFoe( const Position center ) { //in progress.

    int map_x = Broodwar->mapWidth() * 4;
    int map_y = Broodwar->mapHeight() * 4; //tile positions are 32x32, walkable checks 8x8 minitiles. 
    WalkPosition startloc = WalkPosition( center );

    if (!map_veins_out_from_enemy_.empty() && unwalkable_barriers_[startloc.x][startloc.y] != 0) {
        return;
    }
    else {
        map_veins_out_from_enemy_.clear();
    }

    // first, define matrixes to recieve the walkable locations for every minitile.
    map_veins_out_from_enemy_ = unwalkable_barriers_;

    int minitile_x, minitile_y, distance_right_x, distance_below_y;
    minitile_x = startloc.x;
    minitile_y = startloc.y;
    distance_right_x = max( map_x - minitile_x, map_x );
    distance_below_y = max( map_y - minitile_y, map_y );
    int t = std::max( map_x + distance_right_x + distance_below_y, map_y + distance_right_x + distance_below_y );
    int maxI = t*t; // total number of spiral steps we have to make.
    int total_squares_filled = 0;
    int steps_until_next_turn = 1;
    int steps_since_last_turn = 0;
    bool turn_trigger = false;
    int turns_at_this_count = 0;
    int number_of_turns = 0;
    int direction = 1;

    vector <WalkPosition> fire_fill_queue;

    //begin with a fire fill.
    total_squares_filled++;
    map_veins_out_from_enemy_[minitile_x][minitile_y] = total_squares_filled;
    fire_fill_queue.push_back( { minitile_x, minitile_y } );

    int minitile_x_temp = minitile_x;
    int minitile_y_temp = minitile_y;

    while ( !fire_fill_queue.empty() ) { // this portion is a fire fill.

        minitile_x_temp = fire_fill_queue.begin()->x;
        minitile_y_temp = fire_fill_queue.begin()->y;
        fire_fill_queue.erase( fire_fill_queue.begin() );

        // north
        if ( minitile_y_temp + 1 < map_y && map_veins_[minitile_x_temp][minitile_y_temp + 1] > 0 && map_veins_out_from_enemy_[minitile_x_temp][minitile_y_temp + 1] == 0 ) {
            total_squares_filled++;
            map_veins_out_from_enemy_[minitile_x_temp][minitile_y_temp + 1] = total_squares_filled;
            fire_fill_queue.push_back( { minitile_x_temp , minitile_y_temp + 1 } );
        }
        // north east
        if ( minitile_x_temp + 1 < map_x && minitile_y_temp + 1 < map_y && map_veins_[minitile_x_temp + 1][minitile_y_temp + 1] > 0 && map_veins_out_from_enemy_[minitile_x_temp + 1][minitile_y_temp + 1] == 0 ) {
            total_squares_filled++;
            map_veins_out_from_enemy_[minitile_x_temp + 1][minitile_y_temp + 1] = total_squares_filled;
            fire_fill_queue.push_back( { minitile_x_temp + 1, minitile_y_temp + 1 } );
        }
        // north west
        if ( 0 < minitile_x_temp - 1 && minitile_y_temp + 1 < map_y && map_veins_[minitile_x_temp - 1][minitile_y_temp + 1] > 0 && map_veins_out_from_enemy_[minitile_x_temp - 1][minitile_y_temp + 1] == 0 ) {
            total_squares_filled++;
            map_veins_out_from_enemy_[minitile_x_temp - 1][minitile_y_temp + 1] = total_squares_filled;
            fire_fill_queue.push_back( { minitile_x_temp - 1, minitile_y_temp + 1 } );
        }
        //south east
        if ( minitile_x_temp + 1 < map_x && 0 < minitile_y_temp - 1 && map_veins_[minitile_x_temp + 1][minitile_y_temp - 1] > 0 && map_veins_out_from_enemy_[minitile_x_temp + 1][minitile_y_temp - 1] == 0 ) {
            total_squares_filled++;
            map_veins_out_from_enemy_[minitile_x_temp + 1][minitile_y_temp - 1] = total_squares_filled;
            fire_fill_queue.push_back( { minitile_x_temp + 1, minitile_y_temp - 1 } );
        }
        //south west
        if ( 0 < minitile_x_temp - 1 && 0 < minitile_y_temp - 1 && map_veins_[minitile_x_temp - 1][minitile_y_temp - 1] > 0 && map_veins_out_from_enemy_[minitile_x_temp - 1][minitile_y_temp - 1] == 0 ) {
            total_squares_filled++;
            map_veins_out_from_enemy_[minitile_x_temp - 1][minitile_y_temp - 1] = total_squares_filled;
            fire_fill_queue.push_back( { minitile_x_temp - 1, minitile_y_temp - 1 } );
        }
        // east
        if ( minitile_x_temp + 1 < map_x && map_veins_[minitile_x_temp + 1][minitile_y_temp] > 0 && map_veins_out_from_enemy_[minitile_x_temp + 1][minitile_y_temp] == 0 ) {
            total_squares_filled++;
            map_veins_out_from_enemy_[minitile_x_temp + 1][minitile_y_temp] = total_squares_filled;
            fire_fill_queue.push_back( { minitile_x_temp + 1, minitile_y_temp } );
        }
        //west
        if ( 0 < minitile_x_temp - 1 && map_veins_[minitile_x_temp - 1][minitile_y_temp] > 0 && map_veins_out_from_enemy_[minitile_x_temp - 1][minitile_y_temp] == 0 ) {
            total_squares_filled++;
            map_veins_out_from_enemy_[minitile_x_temp - 1][minitile_y_temp] = total_squares_filled;
            fire_fill_queue.push_back( { minitile_x_temp - 1, minitile_y_temp } );
        }
    }
}

int Inventory::getDifferentialDistanceOutFromEnemy(const Position A, const Position B ) const
{
    if (map_veins_out_from_enemy_.size() > 0 && A.isValid() && B.isValid()) {
        WalkPosition wp_a = WalkPosition(A);
        WalkPosition wp_b = WalkPosition(B);
        int A = map_veins_out_from_enemy_[(size_t)wp_a.x][(size_t)wp_a.y];
        int B = map_veins_out_from_enemy_[(size_t)wp_b.x][(size_t)wp_b.y];
        if (A > 0 && B > 0) {
            return abs(A - B);
        }
    }

     return 9999999;
}

int Inventory::getRadialDistanceOutFromEnemy( const Position A) const
{
    if ( map_veins_out_from_enemy_.size() > 0 && A.isValid()) {
        WalkPosition wp_a = WalkPosition( A );
        int A = map_veins_out_from_enemy_[(size_t)wp_a.x][(size_t)wp_a.y];
        if (A > 0) {
            return map_veins_out_from_enemy_[(size_t)wp_a.x][(size_t)wp_a.y];
        }
    }

      return 9999999;

}

int Inventory::getDifferentialDistanceOutFromHome( const Position A, const Position B ) const
{
    if ( map_veins_out_from_main_.size() > 0 && A.isValid() && B.isValid() ) {
        WalkPosition wp_a = WalkPosition(A);
        WalkPosition wp_b = WalkPosition(B);
        int A = map_veins_out_from_main_[(size_t)wp_a.x][(size_t)wp_a.y];
        int B = map_veins_out_from_main_[(size_t)wp_b.x][(size_t)wp_b.y];
        if (A > 0 && B > 0) {
            return abs(A - B);
        }
    }

    return 9999999;
}
int Inventory::getRadialDistanceOutFromHome( const Position A ) const
{
    if (map_veins_out_from_enemy_.size() > 0 && A.isValid()) {
        WalkPosition wp_a = WalkPosition(A);
        int A = map_veins_out_from_main_[(size_t)wp_a.x][(size_t)wp_a.y];
        if (A > 0) {
            return map_veins_out_from_main_[(size_t)wp_a.x][(size_t)wp_a.y];
        }
    }

    return 9999999;

}

void Inventory::updateLiveMapVeins( const Unit &building, const Unit_Inventory &ui, const Unit_Inventory &ei, const Resource_Inventory &ri ) { // in progress.
    int map_x = Broodwar->mapWidth() * 4;
    int map_y = Broodwar->mapHeight() * 4; //tile positions are 32x32, walkable checks 8x8 minitiles
    int area_modified = 32 * 8;


    //modified areas stopping at bounds. bounds are 1 inside edge of map.
    WalkPosition max_lower_right = WalkPosition( Position( building->getPosition().x + area_modified, building->getPosition().y + area_modified ) );
    WalkPosition max_upper_left = WalkPosition( Position( building->getPosition().x - area_modified, building->getPosition().y - area_modified ) );

    WalkPosition lower_right_modified = WalkPosition( max_lower_right.x < map_x ? max_lower_right.x : map_x - 1, max_lower_right.y < map_y ? max_lower_right.y : map_y - 1 );
    WalkPosition upper_left_modified = WalkPosition( max_upper_left.x > 0 ? max_upper_left.x : 1, max_upper_left.y > 0 ? max_upper_left.y : 1 );

    // clear tiles that may have been altered.
    for ( auto minitile_x = upper_left_modified.x; minitile_x <= lower_right_modified.x; ++minitile_x ) {
        for ( auto minitile_y = upper_left_modified.y; minitile_y <= lower_right_modified.y; ++minitile_y ) { // Check all possible walkable locations.
            if ( smoothed_barriers_[minitile_x][minitile_y] == 0 ) {
                Position pos = Position( WalkPosition( minitile_x, minitile_y ) );
                if ( MeatAIModule::checkBuildingOccupiedArea( ui, pos ) || MeatAIModule::checkBuildingOccupiedArea( ei, pos ) || MeatAIModule::checkResourceOccupiedArea(ri,pos) ) {
                    map_veins_[minitile_x][minitile_y] = 1;
                }
                else /*if ( MeatAIModule::checkUnitOccupiesArea( building, pos, area_modified ) )*/ {
                    map_veins_[minitile_x][minitile_y] = 0; // if it is nearby nuke it to 0 for recasting.
                }
            }
        }
    }

    for ( auto iter = 2; iter < 175; iter++ ) { // iteration 1 is already done by labling unwalkables. Less loops are needed because most of the map is already plotted.
        for ( auto minitile_x = upper_left_modified.x; minitile_x <= lower_right_modified.x; ++minitile_x ) {
            for ( auto minitile_y = upper_left_modified.y; minitile_y <= lower_right_modified.y; ++minitile_y ) { // Check all possible walkable locations.

                                             //Psudocode: if any two opposing points are unwalkable, while an alternative path through the center is walkable, it is a choke, the fewer cycles it takes to identify this, the tigher the choke.
                                             //    If any 3 points adjacent are unwalkable it is probably just a bad place to walk, dead end, etc.Mark it as unwalkable.Do not consider it unwalkable this cycle.
                                           //    if any corner of it is inaccessable, it is a diagonal wall, mark it as unwalkable.Do not consider it unwalkable this cycle.
                                             //        Repeat until finished.

                Position pos = Position( WalkPosition( minitile_x, minitile_y ) );

                if ( map_veins_[minitile_x][minitile_y] == 0 ) { // if it is walkable, consider it a canidate for a choke.

                                                                 // Predefine grid we will search over.
                    bool local_tile_0_0 = (map_veins_[(minitile_x - 1)][(minitile_y - 1)] < iter && map_veins_[(minitile_x - 1)][(minitile_y - 1)] > 0);
                    bool local_tile_1_0 = (map_veins_[minitile_x][(minitile_y - 1)] < iter && map_veins_[minitile_x][(minitile_y - 1)] > 0);
                    bool local_tile_2_0 = (map_veins_[(minitile_x + 1)][(minitile_y - 1)] < iter && map_veins_[(minitile_x + 1)][(minitile_y - 1)] > 0);

                    bool local_tile_0_1 = (map_veins_[(minitile_x - 1)][minitile_y] < iter  && map_veins_[(minitile_x - 1)][minitile_y] > 0);
                    bool local_tile_1_1 = (map_veins_[minitile_x][minitile_y] < iter  && map_veins_[minitile_x][minitile_y] > 0);
                    bool local_tile_2_1 = (map_veins_[(minitile_x + 1)][minitile_y] < iter  && map_veins_[(minitile_x + 1)][minitile_y]> 0);

                    bool local_tile_0_2 = (map_veins_[(minitile_x - 1)][(minitile_y + 1)] < iter  && map_veins_[(minitile_x - 1)][(minitile_y + 1)] > 0);
                    bool local_tile_1_2 = (map_veins_[minitile_x][(minitile_y + 1)] < iter  && map_veins_[minitile_x][(minitile_y + 1)] > 0);
                    bool local_tile_2_2 = (map_veins_[(minitile_x + 1)][(minitile_y + 1)] < iter  && map_veins_[(minitile_x + 1)][(minitile_y + 1)] > 0);

                    // if it is surrounded, it is probably a choke, with weight inversely proportional to the number of cycles we have taken this on.
                    bool opposing_tiles =
                        (local_tile_0_0 && (local_tile_2_2 || local_tile_2_1 || local_tile_1_2)) ||
                        (local_tile_1_0 && (local_tile_1_2 || local_tile_0_2 || local_tile_2_2)) ||
                        (local_tile_2_0 && (local_tile_0_2 || local_tile_0_1 || local_tile_1_2)) ||
                        (local_tile_0_1 && (local_tile_2_1 || local_tile_2_0 || local_tile_2_2));

                    bool open_path =
                        (!local_tile_0_0 && !local_tile_2_2) ||
                        (!local_tile_1_0 && !local_tile_1_2) ||
                        (!local_tile_2_0 && !local_tile_0_2) ||
                        (!local_tile_0_1 && !local_tile_2_1);

                    bool adjacent_tiles =
                        local_tile_0_0 && local_tile_0_1 && local_tile_0_2 || // left edge
                        local_tile_2_0 && local_tile_2_1 && local_tile_2_2 || // right edge
                        local_tile_0_0 && local_tile_1_0 && local_tile_2_0 || // bottom edge
                        local_tile_0_2 && local_tile_1_2 && local_tile_2_2 || // top edge
                        local_tile_0_1 && local_tile_1_0 && local_tile_0_0 || // lower left slice.
                        local_tile_0_1 && local_tile_1_2 && local_tile_0_2 || // upper left slice.
                        local_tile_1_2 && local_tile_2_1 && local_tile_2_2 || // upper right slice.
                        local_tile_1_0 && local_tile_2_1 && local_tile_2_0; // lower right slice.

                    if ( open_path && opposing_tiles ) {  //mark chokes when found.
                        map_veins_[minitile_x][minitile_y] = 299 - iter;
                    }
                    else if ( (!open_path && opposing_tiles) || adjacent_tiles ) { //if it is closing off in any other way than "formal choke"- it's just a bad place to walk. Mark as unwalkable and continue. Will seal map.
                        map_veins_[minitile_x][minitile_y] = iter;
                    }

                }
            }
        }
    }
}

void Inventory::updateMapChokes() {
    int map_x = Broodwar->mapWidth() * 4;
    int map_y = Broodwar->mapHeight() * 4; //tile positions are 32x32, walkable checks 8x8 minitiles. 
    WalkPosition map_dim = WalkPosition( TilePosition( { Broodwar->mapWidth(), Broodwar->mapHeight() } ) );

    // first, define matrixes to recieve the smoothed locations for every minitile.
    for ( int x = 0; x <= map_x; ++x ) {
        vector<int> temp;
        for ( int y = 0; y <= map_y; ++y ) {
            temp.push_back( 0 );
        }
        map_chokes_.push_back( temp );
    }

    for ( auto minitile_x = 1; minitile_x <= map_x; ++minitile_x ) {
        for ( auto minitile_y = 1; minitile_y <= map_y; ++minitile_y ) { // Check all possible walkable locations.

            int max_observed = map_veins_[minitile_x][minitile_y];
            int counter = 0;

            if ( smoothed_barriers_[minitile_x][minitile_y] == 0 ) {
                for ( int x = -1; x <= 1; ++x ) {
                    for ( int y = -1; y <= 1; ++y ) {
                        int testing_x = minitile_x + x;
                        int testing_y = minitile_y + y;
                        if ( !(x == 0 && y == 0) &&
                            testing_x < map_dim.x &&
                            testing_y < map_dim.y &&
                            testing_x > 0 &&
                            testing_y > 0 ) { // check for being within reference space.

                            if ( map_veins_[testing_x][testing_y] <= max_observed ) {
                                counter++;
                                if ( counter == 8 ) {
                                    map_chokes_[minitile_x][minitile_y] = 300 - map_veins_[minitile_x][minitile_y];
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}




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
                        (MeatAIModule::isMapClearRayTrace(Position(prosepective_location_upper_left), Position(min_pos_t), *this) ||
                         MeatAIModule::isMapClearRayTrace(Position(prosepective_location_upper_right), Position(min_pos_t), *this) ||
                         MeatAIModule::isMapClearRayTrace(Position(prosepective_location_lower_left), Position(min_pos_t), *this) ||
                         MeatAIModule::isMapClearRayTrace(Position(prosepective_location_lower_right), Position(min_pos_t), *this) ) ) { // if it is 3 away from the resource, and has clear vision to the resource.

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
    bool clearing_workers_found = false;

    if (!ui.unit_inventory_.empty()) {
        for (auto & w = ui.unit_inventory_.begin(); w != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); w++) {
            if ( w->second.isClearing(ri) ) {
                clearing_workers_found = true;
                break;
            }
        }
    }
    workers_are_clearing_ = clearing_workers_found;
}

void Inventory::getExpoPositions() {

    expo_positions_.clear();

    int location_qual_threshold = -999999;
    //Region home = Broodwar->getRegionAt(Position(center_self));
    //Regionset neighbors;
    bool local_maximum = true;


    for ( vector<int>::size_type x = 0; x != base_values_.size(); ++x ) {
        for ( vector<int>::size_type y = 0; y != base_values_[x].size(); ++y ) {
            if ( base_values_[x][y] > 1 ) { // only consider the decent locations please.

                local_maximum = true;

                TilePosition canidate_spot = TilePosition( x + 2, y + 1 ); // from the true center of the object.
                //int walk = Position( canidate_spot ).getDistance( Position( center_self ) ) / 32;
                //int net_quality = base_values_[x][y]; //value of location and distance from our center.  Plus some terms so it's positive, we like to look at positive numbers.

                for ( int i = -12; i <= 12; i++ ) {
                    for ( int j = -12; j <= 12; j++ ) {
                        bool safety_check = x + i < base_values_.size() && x - i > 0 && y + j < base_values_[x + i].size() && y - j > 0;
                        if ( safety_check && base_values_[x][y] < base_values_[x + i][y + j] ) {
                            local_maximum = false;
                            break;
                        }
                    }
                }

                if ( local_maximum ) {
                    expo_positions_.push_back( { static_cast<int>(x), static_cast<int>(y) } );
                }

            }

        } // closure y
    } // closure x
}

void Inventory::getStartPositions() {
    for ( auto loc : Broodwar->getStartLocations() ) {
        start_positions_.push_back( Position( loc ) );
    }
}

void Inventory::updateStartPositions() {
    for ( auto visible_base = start_positions_.begin(); visible_base != start_positions_.end() && !start_positions_.empty();) {
        if ( Broodwar->isExplored( TilePosition( *visible_base ) ) || Broodwar->self()->getStartLocation() == TilePosition(*visible_base) ) {
            visible_base = start_positions_.erase( visible_base );
            if ( *visible_base == start_positions_[0] ) {
                updateMapVeinsOutFromFoe(start_positions_[0]);
            }

        }
        else {
            ++visible_base;
        }
    }

    if ( start_positions_.empty() ) {
        cleared_all_start_positions_ = true;
    }
}

void Inventory::setNextExpo( const TilePosition tp ) {
    next_expo_ = tp;
}

//Zerg_Zergling, 37
//Zerg_Hydralisk, 38
//Zerg_Ultralisk, 39
//Zerg_Broodling, 40
//Zerg_Drone, 41
//Zerg_Overlord, 42
//Zerg_Mutalisk, 43
//Zerg_Guardian, 44
//Zerg_Queen, 45
//Zerg_Defiler, 46
//Zerg_Scourge, 47
