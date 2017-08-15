#pragma once

#include <BWAPI.h>
#include "Source\MeatAIModule.h"
#include "Source\InventoryManager.h"
#include "Source\Unit_Inventory.h"


using namespace std;

// Creates a Inventory Object
Inventory::Inventory() {};
Inventory::Inventory( const Unit_Inventory &ui ) {

    updateLn_Army_Stock( ui );
    updateLn_Tech_Stock( ui );
    updateLn_Worker_Stock();
    updateVision_Count();

    updateLn_Supply_Remain( ui );
    updateLn_Supply_Total();

    updateLn_Gas_Total();
    updateLn_Min_Total();

    updateGas_Workers();
    updateMin_Workers();

    updateMin_Possessed();
    updateHatcheries( ui );

    updateReserveSystem();

    if ( resource_positions_.size() == 0 ) {
        updateMineralPos();
        updateBuildablePos();
        updateBaseLoc();
        int buildable_ct = 0;
        for ( vector<int>::size_type i = 0; i != buildable_positions_.size(); ++i ) {
            for ( vector<int>::size_type j = 0; j != buildable_positions_[i].size(); ++j ) {
                buildable_ct += buildable_positions_[i][j];
            }
        }
        Broodwar->sendText( "There are %d resources on the map, %d buildable positions.", resource_positions_.size(), buildable_ct );
    }

    if ( smoothed_barriers_.size() == 0 ) {
        updateSmoothPos();
        int unwalkable_ct = 0;
        for ( vector<int>::size_type i = 0; i != smoothed_barriers_.size(); ++i ) {
            for ( vector<int>::size_type j = 0; j != smoothed_barriers_[i].size(); ++j ) {
                unwalkable_ct += smoothed_barriers_[i][j];
            }
        }
        Broodwar->sendText( "There are %d roughly tiles, %d unwalkable ones.", smoothed_barriers_.size(), unwalkable_ct );
    }
};

// Defines the (safe) log of our army stock.
void Inventory::updateLn_Army_Stock(const Unit_Inventory &ui) {

    double total = ui.stock_total_;
    for ( auto & u : ui.unit_inventory_ ) {
        if ( u.second.type_ == UnitTypes::Zerg_Drone ) {
            total -= u.second.current_stock_value_;
        }
    }

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

    double cost = sqrt( pow( UnitTypes::Zerg_Drone.mineralPrice(), 2 ) + pow( 1.25 * UnitTypes::Zerg_Drone.gasPrice(), 2 ) + pow( 25 * UnitTypes::Zerg_Drone.supplyRequired(), 2 ) );

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
void Inventory::updateLn_Supply_Remain( const Unit_Inventory &ui ) {

    double total = 0;
    for ( int i = 37; i != 48; i++ )
    { // iterating through all units.  (including buildings).
        UnitType u_current = (UnitType)i;
        total += MeatAIModule::Stock_Supply( u_current, ui );
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
        return ln_supply_remain_ / ln_supply_total_ ;
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
void Inventory::updateMin_Possessed() {

    int min_fields = 0;
    Unitset resource = Broodwar->getMinerals(); // get any mineral field that exists on the map.
    if ( !resource.empty() ) { // check if the minerals exist
        for ( auto r = resource.begin(); r != resource.end() && !resource.empty(); ++r ) { //for each mineral
            if ( (*r) && (*r)->exists() ) {
                Unitset mybases = Broodwar->getUnitsInRadius( (*r)->getPosition(), 250, Filter::IsResourceDepot && Filter::IsOwned ); // is there a mining base near there?
                if ( !mybases.empty() ) { // check if there is a base nearby
                    min_fields++; // if there is a base near it, then this mineral counts.
                } // closure if base is nearby
            } // closure for existance check.
        } // closure: for each mineral
    } // closure, minerals are visible on map.

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

// Updates the number of hatcheries (and decendent buildings).
void Inventory::updateHatcheries( const Unit_Inventory &ui ) {
    hatches_ = MeatAIModule::Count_Units( UnitTypes::Zerg_Hatchery, ui ) +
               MeatAIModule::Count_Units( UnitTypes::Zerg_Lair, ui ) +
               MeatAIModule::Count_Units( UnitTypes::Zerg_Hive, ui );
}

// Updates the static locations of minerals and gas on the map. Should only be called on game start.
void Inventory::updateMineralPos() {

    Unitset min = Broodwar->getStaticMinerals();
    Unitset geysers = Broodwar->getStaticGeysers();

    for ( auto m = min.begin(); m != min.end(); ++m ) {
        resource_positions_.push_back( (*m)->getPosition() );
    }
    for ( auto g = geysers.begin(); g != geysers.end(); ++g ) {
        resource_positions_.push_back( (*g)->getPosition() );
    }
}

//In Tiles?
void Inventory::updateBuildablePos()
{
    //Buildable_positions_ = std::vector< std::vector<bool> >( BWAPI::Broodwar->mapWidth()/8, std::vector<bool>( BWAPI::Broodwar->mapHeight()/8, false ) );

    int map_x = Broodwar->mapWidth() ;
    int map_y = Broodwar->mapHeight() ;
    for ( int x = 0; x <= map_x; ++x ) {
        vector<bool> temp;
        for ( int y = 0; y <= map_y; ++y ) {
             temp.push_back( Broodwar->isBuildable( x, y ) );
        }
        buildable_positions_.push_back( temp );
    }
};

void Inventory::updateSmoothPos() {
    int map_x = Broodwar->mapWidth() * 4;
    int map_y = Broodwar->mapHeight() * 4 ; //tile positions are 32x32, walkable checks 8x8 minitiles. 
    int choke_score = 0;

    // first, define matrixes to recieve the walkable locations for every minitile.
    for ( int x = 0; x <= map_x; ++x ) {
        vector<int> temp;
        for ( int y = 0; y <= map_y; ++y ) {
            temp.push_back( !Broodwar->isWalkable( x , y ) ); 
        }
        smoothed_barriers_.push_back( temp );
    }

    for ( auto iter = 2; iter < 100; iter++ ) { // iteration 1 is already done by labling unwalkables.
        for ( auto tile_x = 1; tile_x <= map_x ; ++tile_x ) {
            for ( auto tile_y = 1; tile_y <= map_y ; ++tile_y ) { // Check all possible walkable locations.

                // Psudocode: if any two opposing points are unwalkable, while an alternative path through the center is walkable, it is a choke, the fewer cycles it takes to identify this, the tigher the choke.
                // If any 3 points adjacent are unwalkable it is probably just a bad place to walk, dead end, etc. Mark it as unwalkable.  Do not consider it unwalkable this cycle.
                // if any corner of it is inaccessable, it is a diagonal wall, mark it as unwalkable. Do not consider it unwalkable this cycle.
                // Repeat untill finished.

                if ( smoothed_barriers_[tile_x][tile_y] == 0 ) { // if it is walkable, consider it a canidate for a choke.

                    // Predefine grid we will search over.
                    bool local_tile_0_0 = (smoothed_barriers_[(tile_x - 1)][(tile_y - 1)] < iter && smoothed_barriers_[(tile_x - 1)][(tile_y - 1)] > 0) ;
                    bool local_tile_1_0 = (smoothed_barriers_[tile_x][(tile_y - 1)]       < iter && smoothed_barriers_[tile_x][(tile_y - 1)] > 0 )      ;
                    bool local_tile_2_0 = (smoothed_barriers_[(tile_x + 1)][(tile_y - 1)] < iter && smoothed_barriers_[(tile_x + 1)][(tile_y - 1)] > 0) ;

                    bool local_tile_0_1 = (smoothed_barriers_[(tile_x - 1)][tile_y] < iter  && smoothed_barriers_[(tile_x - 1)][tile_y] > 0) ;
                    bool local_tile_1_1 = (smoothed_barriers_[tile_x][tile_y]       < iter  && smoothed_barriers_[tile_x][tile_y] > 0)       ;
                    bool local_tile_2_1 = (smoothed_barriers_[(tile_x + 1)][tile_y] < iter  && smoothed_barriers_[(tile_x + 1)][tile_y]> 0)  ;

                    bool local_tile_0_2 = (smoothed_barriers_[(tile_x - 1)][(tile_y + 1)]  < iter  && smoothed_barriers_[(tile_x - 1)][(tile_y + 1)] > 0)  ;
                    bool local_tile_1_2 = (smoothed_barriers_[tile_x][(tile_y + 1)]        < iter  && smoothed_barriers_[tile_x][(tile_y + 1)] > 0)        ;
                    bool local_tile_2_2 = (smoothed_barriers_[(tile_x + 1)][(tile_y + 1)]  < iter  && smoothed_barriers_[(tile_x + 1)][(tile_y + 1)] > 0)  ;

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
                        (!local_tile_0_1 && !local_tile_2_1) ;

                    bool adjacent_tiles =
                        local_tile_0_0 && local_tile_0_1 && local_tile_0_2 || // left edge
                        local_tile_2_0 && local_tile_2_1 && local_tile_2_2 || // right edge
                        local_tile_0_0 && local_tile_1_0 && local_tile_2_0 || // bottom edge
                        local_tile_0_2 && local_tile_1_2 && local_tile_2_2 || // top edge
                        local_tile_0_1 && local_tile_1_0 && local_tile_0_0 || // lower left slice.
                        local_tile_0_1 && local_tile_1_2 && local_tile_0_2 || // upper left slice.
                        local_tile_1_2 && local_tile_2_1 && local_tile_2_2 || // upper right slice.
                        local_tile_1_0 && local_tile_2_1 && local_tile_2_0; // lower right slice.

                    if ( opposing_tiles && (open_path || adjacent_tiles) ) { // if it is adjacently blocked, it is not a choke, it's just a bad place to walk. Mark as unwalkable and continue.
                        smoothed_barriers_[tile_x][tile_y] = 99 - iter;
                    } 

//                  if ( !(opposing_tiles && open_path) && adjacent_tiles ) {

                    if ( !open_path && (opposing_tiles || adjacent_tiles) ) {
                        smoothed_barriers_[tile_x][tile_y] = iter;
                    }
                }
            }
        }
    }
}

void Inventory::updateBaseLoc() {

    int map_x = Broodwar->mapWidth();
    int map_y = Broodwar->mapHeight();
    int location_quality = 0;

    // first, define matrixes to recieve the base locations. 0 if unbuildable, 1 if buildable.
    for ( int x = 0; x <= map_x; ++x ) {
        vector<int> temp;
        for ( int y = 0; y <= map_y; ++y ) {
            temp.push_back( (int) Broodwar->isBuildable( x, y ) ); // explicit converion.
        }
        base_values_.push_back( temp );
    }


    for ( vector<int>::size_type p = 0; p != resource_positions_.size(); ++p ) { // search for closest resource group. They are our potential expos.

        TilePosition min_pos_t = { resource_positions_[p].x / 32, resource_positions_[p].y / 32 }; // remember that this gives the upper left corner of the mineral.

        for ( auto tile_x = min_pos_t.x - 10; tile_x != min_pos_t.x + 10; ++tile_x ) {
            for ( auto tile_y = min_pos_t.y - 10; tile_y != min_pos_t.y + 10; ++tile_y ) { // Check wide area of possible build locations.

                if ( tile_x >= 0 && tile_x <= map_x &&
                    tile_y >= 0 && tile_y <= map_y ) { // must be in bounds 

                    if ( (tile_x >= min_pos_t.x + 3 ||
                        tile_x <= min_pos_t.x - 3 ||
                        tile_y >= min_pos_t.y + 3 ||
                        tile_y <= min_pos_t.y - 3) &&
                        Broodwar->canBuildHere( { tile_x, tile_y }, UnitTypes::Zerg_Hatchery ) ) { // if it is 3 away from the resource

                        Position prosepective_location = { tile_x , tile_y }; // The build location is upper left tile of the building The origin tile is 0,0. 

                        int local_min = 0;

                        for ( vector<int>::size_type j = 0; j != resource_positions_.size(); ++j ) {

                            bool long_condition = resource_positions_[j].x / 32 >= prosepective_location.x - UnitTypes::Resource_Mineral_Field.tileWidth() - 3 &&
                                                  resource_positions_[j].y / 32 >= prosepective_location.y - UnitTypes::Resource_Mineral_Field.tileHeight() - 3 && 
                                                  resource_positions_[j].x / 32 <= prosepective_location.x + UnitTypes::Zerg_Hatchery.tileWidth() + 3 + 2 &&
                                                  resource_positions_[j].y / 32 <= prosepective_location.y + UnitTypes::Zerg_Hatchery.tileHeight() + 3 + 1;  // the +/-1 or 2 is a manual adjustment to get this to work. I don't quite get why they have to be this way, but it works best.
                            if ( long_condition ) {
                                ++local_min;
                            }
                        }

                        int walk = sqrt( pow( Broodwar->self()->getStartLocation().x - prosepective_location.x, 2 ) + pow( Broodwar->self()->getStartLocation().y - prosepective_location.y, 2 ) ); // team_center and center are both in pixels

                        location_quality = local_min * 50 /*- walk + map_x + map_y*/; // a BS metric.

                    }
                    else {
                        location_quality = 0; // redundant, defaults to 0 - but clear.
                    } // if it's invalid for some reason return 0.

                    base_values_[tile_x][tile_y] = location_quality;

                } // closure in bounds

            }
        }
    }
}

void Inventory::updateReserveSystem() {
    if ( Broodwar->getFrameCount() == 0 ) {
        min_reserve_= 0;
        gas_reserve_ = 0;
        building_timer_ = 0;
    }
    else {
        building_timer_ > 0 ? --building_timer_ : 0;
    }
}


void Inventory::updateNextExpo(const Unit_Inventory &e_inv, const Unit_Inventory &u_inv ) {

    Position center_self = Broodwar->self()->getUnits().getPosition();
    int location_qual_threshold = 0;
    acceptable_expo_ = false;

    for ( vector<int>::size_type x = 0; x != base_values_.size(); ++x ) {
        for ( vector<int>::size_type y = 0; y != base_values_[x].size(); ++y ) {
            if ( base_values_[x][y] > 1 ) { // only consider the decent locations please.

                int net_quality = base_values_[x][y] - 5 * sqrt( pow( static_cast<int>(x) - center_self.x / 32, 2 ) + pow( static_cast<int>(y) - center_self.y / 32, 2 ) ) + Broodwar->mapHeight() + Broodwar->mapWidth(); //value of location and distance from our center.  Plus some terms so it's positive, we like to look at positive numbers.

                bool enemy_in_inventory_near_expo = false; // Don't build on enemies!
                bool found_rdepot = false;

                Unit_Inventory e_loc = MeatAIModule::getUnitInventoryInRadius( e_inv, { static_cast<int>(x) * 32, static_cast<int>(y) * 32 }, 500 );
                if ( !e_loc.unit_inventory_.empty() ) {
                    enemy_in_inventory_near_expo = true;
                }

                Unit_Inventory rdepot = MeatAIModule::getUnitInventoryInRadius( u_inv, { static_cast<int>(x) * 32, static_cast<int>(y) * 32 }, 500 );
                for ( const auto &r : rdepot.unit_inventory_ ) {
                    if ( r.second.type_.isResourceDepot() ) {
                        found_rdepot = true;
                    }
                }

                bool condition = net_quality > location_qual_threshold && !enemy_in_inventory_near_expo && !found_rdepot;

                if ( condition ) {
                    next_expo_ = { static_cast<int>(x), static_cast<int>(y) };
                    acceptable_expo_ = true;
                    location_qual_threshold = net_quality;
                }
            }
        } // closure y
    } // closure x
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