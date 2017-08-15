#pragma once
#include "Source\MeatAIModule.h"
#include "Source\InventoryManager.h"
#include "Source\AssemblyManager.h"

using namespace BWAPI;
using namespace Filter;
using namespace std;

//Checks if a building can be built, and passes additional boolean criteria.  If all critera are passed, then it builds the building and announces this to the building gene manager. It may now allow morphing, eg, lair, hive and lurkers, but this has not yet been tested.  It now has an extensive creep colony script that prefers centralized locations.
void MeatAIModule::Check_N_Build( const UnitType &building, const Unit &unit, const Unit_Inventory &ui, const bool &extra_critera )
{
    if ( Broodwar->self()->minerals() >= building.mineralPrice() &&
        Broodwar->self()->gas() >= building.gasPrice() &&
        (buildorder.checkBuilding_Desired( building ) || (extra_critera && buildorder.checkEmptyBuildOrder() && !buildorder.active_builders_) ) ) {
        if ( unit->canBuild( building ) &&
            building != UnitTypes::Zerg_Creep_Colony )
        {
            TilePosition buildPosition = Broodwar->getBuildLocation( building, unit->getTilePosition(), 64, building == UnitTypes::Zerg_Creep_Colony );
            if ( unit->build( building, buildPosition ) ) {
                buildorder.setBuilding_Complete( building );
            }
        }
        else if ( unit->canBuild( building ) &&
            building == UnitTypes::Zerg_Creep_Colony ) { // creep colony loop specifically.
            
            Unitset base_core = unit->getUnitsInRadius( 1, IsBuilding && IsResourceDepot && IsCompleted ); // don't want undefined crash online 44.

            for ( const auto &u : ui.unit_inventory_ ) {
                if ( u.second.type_ == UnitTypes::Zerg_Hatchery ) {
                    base_core.insert( u.second.bwapi_unit_ );
                }
            }

            int middle_x = Broodwar->mapWidth() / 2 * 32;
            int middle_y = Broodwar->mapHeight() / 2 * 32;

            int central_base_x = 0;
            int central_base_y = 0;

            int furth_x_dist = 0;
            int furth_y_dist = 0;

            if ( !base_core.empty() ) {

                for ( auto base = base_core.begin(); base != base_core.end(); ++base ) {

                    int x_dist = pow( middle_x - (*base)->getPosition().x, 2 );
                    int y_dist = pow( middle_y - (*base)->getPosition().y, 2 );

                    int new_dist = sqrt( (double)x_dist + (double)y_dist );

                    int furth_x_dist = pow( middle_x - central_base_x, 2 );
                    int furth_y_dist = pow( middle_y - central_base_y, 2 );

                    int old_dist = sqrt( (double)furth_x_dist + (double)furth_y_dist );

                    if ( new_dist <= old_dist ) {
                        central_base_x = (*base)->getPosition().x;
                        central_base_y = (*base)->getPosition().y;
                    }
                }
            }
            double theta = atan2( middle_y - central_base_y, middle_x - central_base_x );
            int adj_dx = cos( theta ) * 4; // move n tiles closer to the center of the map.
            int adj_dy = sin( theta ) * 4;

            TilePosition buildPosition = Broodwar->getBuildLocation( building, { central_base_x / 32 + adj_dx , central_base_y / 32 + adj_dy }, 5 );
            if ( unit->build( building, buildPosition ) ) {
                buildorder.setBuilding_Complete( building );
            }
        }
        
        if ( unit->canMorph( building ) )
        {
            if ( unit->morph( building ) ) {
                buildorder.setBuilding_Complete( building );
            }
        }
    }
}

//Checks if an upgrade can be built, and passes additional boolean criteria.  If all critera are passed, then it performs the upgrade. Requires extra critera.
void MeatAIModule::Check_N_Upgrade( const UpgradeType &ups, const Unit &unit, const bool &extra_critera )
{
    if ( unit->canUpgrade( ups ) &&
        Broodwar->self()->minerals() >= ups.mineralPrice() &&
        Broodwar->self()->gas() >= ups.gasPrice() &&
        (buildorder.checkUpgrade_Desired( ups ) || ( extra_critera && buildorder.checkEmptyBuildOrder() ) ) ) {
        unit->upgrade( ups );
        buildorder.updateRemainingBuildOrder( ups );
        Broodwar->sendText( "Upgrading %s. Let's hope it finishes!", ups.c_str() );
    }
}

//Checks if a unit can be built from a larva, and passes additional boolean criteria.  If all critera are passed, then it performs the upgrade. Requires extra critera.
void MeatAIModule::Check_N_Grow( const UnitType &unittype, const Unit &larva, const bool &extra_critera )
{
    if ( larva->canMorph( unittype ) &&
        Broodwar->self()->minerals() >= unittype.mineralPrice() &&
        Broodwar->self()->gas() >= unittype.gasPrice() &&
        ( buildorder.checkBuilding_Desired( unittype ) || (extra_critera && (buildorder.checkEmptyBuildOrder() || unittype == UnitTypes::Zerg_Drone || unittype == UnitTypes::Zerg_Overlord) ) ) )
    {
        larva->morph( unittype );

        buildorder.setBuilding_Complete( unittype ); // Shouldn't be a problem if unit isn't in buildorder. Makes it negative, build order preference checks for >0.

        if ( unittype.isTwoUnitsInOneEgg() ) {
            buildorder.setBuilding_Complete( unittype ); // Shouldn't be a problem if unit isn't in buildorder. Makes it negative, build order preference checks for >0.
        } 
    }
}

//Creates a new unit. Reflects (poorly) upon enemy units in enemy_set. Incomplete.
void MeatAIModule::Reactive_Build( const Unit &larva, const Inventory &inv, const Unit_Inventory &fi, const Unit_Inventory &ei )
{

    //Tally up crucial details about enemy. Should be doing this onclass. Perhaps make an enemy summary class?

    //Supply blocked protection 
    Check_N_Grow( UnitTypes::Zerg_Overlord, larva, supply_starved );

    //Army build/replenish.  Cycle through military units available.
    if ( ei.stock_fliers_ > 0.15 * inventory.est_enemy_stock_ ) { // Mutas generally sucks against air unless properly massed and manuvered (which mine are not)
        Check_N_Grow( UnitTypes::Zerg_Scourge, larva, army_starved && Count_Units( UnitTypes::Zerg_Spire, fi ) > 0 && Count_Units( UnitTypes::Zerg_Scourge, fi ) < 5 ); // hard cap on scourges, they build 2 at a time. 
        Check_N_Grow( UnitTypes::Zerg_Mutalisk, larva, army_starved && Count_Units( UnitTypes::Zerg_Spire, fi ) > 0 );
        Check_N_Grow( UnitTypes::Zerg_Hydralisk, larva, army_starved && Count_Units( UnitTypes::Zerg_Hydralisk_Den, fi ) > 0 );
    } else if ( ei.stock_high_ground_ > 0.15 * inventory.est_enemy_stock_ ) { // if we have to go through a choke, this is all we want. Save for them.
        Check_N_Grow( UnitTypes::Zerg_Ultralisk, larva, army_starved && Count_Units( UnitTypes::Zerg_Ultralisk_Cavern, fi ) > 0 );
        Check_N_Grow( UnitTypes::Zerg_Mutalisk, larva, army_starved && Count_Units( UnitTypes::Zerg_Spire, fi ) > 0 );
    } else if ( ei.stock_cannot_shoot_up_ > 0.15 * inventory.est_enemy_stock_ ) {
        Check_N_Grow( UnitTypes::Zerg_Mutalisk, larva, army_starved && Count_Units( UnitTypes::Zerg_Spire, fi ) > 0 );
    } else if ( ei.stock_cannot_shoot_down_ > 0.75 * inventory.est_enemy_stock_ ) {
        Check_N_Grow( UnitTypes::Zerg_Hydralisk, larva, army_starved && Count_Units( UnitTypes::Zerg_Hydralisk_Den, fi ) > 0 );
    }

    Check_N_Grow( UnitTypes::Zerg_Ultralisk, larva, army_starved && Count_Units( UnitTypes::Zerg_Ultralisk_Cavern, fi ) > 0 ); // catchall ground units.
    Check_N_Grow( UnitTypes::Zerg_Mutalisk, larva, army_starved && Count_Units( UnitTypes::Zerg_Spire, fi ) > 0 );
    Check_N_Grow( UnitTypes::Zerg_Hydralisk, larva, army_starved && Count_Units( UnitTypes::Zerg_Hydralisk_Den, fi ) > 0 );
    Check_N_Grow( UnitTypes::Zerg_Zergling, larva, army_starved && Count_Units( UnitTypes::Zerg_Spawning_Pool, fi ) > 0 );

    //Econ Build/replenish loop. Will build workers if I have no spawning pool, or if there is a worker shortage.
    bool early_game = Count_Units( UnitTypes::Zerg_Spawning_Pool, fi ) - Broodwar->self()->incompleteUnitCount( UnitTypes::Zerg_Spawning_Pool ) == 0 && inv.min_workers_ + inv.gas_workers_ <= 9;
    bool drone_conditional = (econ_starved || early_game); // or it is early game and you have nothing to build. // if you're eco starved

    Check_N_Grow( larva->getType().getRace().getWorker(), larva, drone_conditional ); 
}

//Creates a new building with DRONE. Incomplete.
void MeatAIModule::Building_Begin( const Unit &drone, const Inventory &inv, const Unit_Inventory &e_inv ) {
    // will send it to do the LAST thing on this list that it can build.

    //Gas Buildings
    Check_N_Build( UnitTypes::Zerg_Extractor, drone, friendly_inventory, inv.gas_workers_ > 3 * Count_Units( UnitTypes::Zerg_Extractor, friendly_inventory ) || gas_starved );  // wait till you have a spawning pool to start gathering gas. If your gas is full (or nearly full) get another extractor.

    //Expo loop, whenever not army starved. 
    Expo( drone, !army_starved, inv);
    Check_N_Build( UnitTypes::Zerg_Hatchery, drone, friendly_inventory, Count_Units( UnitTypes::Zerg_Larva, friendly_inventory ) == 0  && Broodwar->self()->minerals() > 600 ); // only macrohatch if you are short on larvae and being a moron.

    //Basic Buildings
    Check_N_Build( UnitTypes::Zerg_Spawning_Pool, drone, friendly_inventory, !econ_starved &&
        Count_Units( UnitTypes::Zerg_Spawning_Pool, friendly_inventory ) == 0 );

    //Tech Buildings
    Check_N_Build( UnitTypes::Zerg_Evolution_Chamber, drone, friendly_inventory, tech_starved &&
        Count_Units( UnitTypes::Zerg_Evolution_Chamber, friendly_inventory ) < 2 && // This has resolved our issues with 4x evo chambers
        Count_Units( UnitTypes::Zerg_Spawning_Pool, friendly_inventory ) > 0 );

    Check_N_Build( UnitTypes::Zerg_Hydralisk_Den, drone, friendly_inventory, tech_starved &&
        Count_Units( UnitTypes::Zerg_Spawning_Pool, friendly_inventory ) > 0 &&
        Count_Units( UnitTypes::Zerg_Hydralisk_Den, friendly_inventory ) == 0 );

    Check_N_Build( UnitTypes::Zerg_Spire, drone, friendly_inventory, tech_starved &&
        Count_Units( UnitTypes::Zerg_Spire, friendly_inventory ) == 0 &&
        Count_Units( UnitTypes::Zerg_Lair, friendly_inventory ) >= 0 );

    Check_N_Build( UnitTypes::Zerg_Queens_Nest, drone, friendly_inventory, tech_starved &&
        Count_Units( UnitTypes::Zerg_Queens_Nest, friendly_inventory ) == 0 &&
        Count_Units( UnitTypes::Zerg_Lair, friendly_inventory ) >= 0 &&
        Count_Units( UnitTypes::Zerg_Spire, friendly_inventory ) >= 0 );  // Spires are expensive and it will probably skip them unless it is floating a lot of gas.

    Check_N_Build( UnitTypes::Zerg_Ultralisk_Cavern, drone, friendly_inventory, tech_starved &&
        Count_Units( UnitTypes::Zerg_Ultralisk_Cavern, friendly_inventory ) == 0 &&
        Count_Units( UnitTypes::Zerg_Hive, friendly_inventory ) >= 0 );

    //Combat Buildings
    Check_N_Build( UnitTypes::Zerg_Creep_Colony, drone, friendly_inventory, army_starved &&  // army starved.
        Count_Units( UnitTypes::Zerg_Creep_Colony, friendly_inventory ) == 0 && // no creep colonies waiting to upgrade
        ((Count_Units( UnitTypes::Zerg_Spawning_Pool, friendly_inventory ) > 0 && Count_Units( UnitTypes::Zerg_Spawning_Pool, friendly_inventory ) > Broodwar->self()->incompleteUnitCount( UnitTypes::Zerg_Spawning_Pool )) ||
        (Count_Units( UnitTypes::Zerg_Evolution_Chamber, friendly_inventory ) > 0 && Count_Units( UnitTypes::Zerg_Evolution_Chamber, friendly_inventory ) > Broodwar->self()->incompleteUnitCount( UnitTypes::Zerg_Evolution_Chamber ))) && // And there is a building complete that will allow either creep colony upgrade.
        (inv.hatches_ * (inv.hatches_ + 1 )) / 2 > Count_Units( UnitTypes::Zerg_Sunken_Colony, friendly_inventory ) ); // and you're not flooded with sunkens. Spores could be ok if you need AA.  as long as you have sum(hatches+hatches-1+hatches-2...)>sunkens.
        //hatches >= 2 ); // and don't build them if you're on one base.
};

void Building_Gene::updateBuildingTimer( const Unit_Inventory &ui ) {
    int longest_project = 0;

    active_builders_ = false;

    for ( auto & u : ui.unit_inventory_ ) {

        if ( u.second.bwapi_unit_->getRemainingBuildTime() > longest_project && u.second.bwapi_unit_->getType().isBuilding() ) {
            longest_project = u.second.bwapi_unit_->getRemainingBuildTime();
        }

        if ( (u.second.bwapi_unit_->getLastCommand().getType() == UnitCommandTypes::Build || u.second.bwapi_unit_->getLastCommand().getType() == UnitCommandTypes::Move) && u.second.bwapi_unit_->getType() == UnitTypes::Zerg_Drone ) {  // drones morph into buildings with the BUILD command not the MORPH command.  No drones ever get the MOVE command unless deliberately scouting, they are otherwise only sent the gather command.
            active_builders_ = true;
        }

    }

    building_timer_ = longest_project;
}

void Building_Gene::updateRemainingBuildOrder( const Unit &u ) {
    if ( !building_gene_.empty() ) {
        if ( building_gene_.front().getUnit() == u->getType() ) {
            building_gene_.erase( building_gene_.begin() );
        }
    }
}

void Building_Gene::updateRemainingBuildOrder( const UpgradeType &ups ) {
    if ( !building_gene_.empty() ) {
        if ( building_gene_.front().getUpgrade() == ups ) {
            building_gene_.erase( building_gene_.begin() );
        }
    }
}

void Building_Gene::setBuilding_Complete( UnitType ut ) {
    if ( ut.isBuilding() && ut.whatBuilds().first == UnitTypes::Zerg_Drone ) {
        last_build_order = ut;
        Broodwar->sendText( "Building a %s", last_build_order.c_str() );
        active_builders_ = true;
    }
}

//void setBuilding_Destroyed( UnitType ut ) {
//    --building_gene_[find( building_gene_.begin(), building_gene_.end(), u->getBuildType() )];
//}

bool Building_Gene::checkBuilding_Desired( UnitType ut ) {
    // A building is not wanted at that moment if we have active builders or the timer is nonzero.
    if ( building_gene_.empty() ) {
        return false;
    }
    else {
        return building_gene_.front().getUnit() == ut && !active_builders_;
    }
}

bool Building_Gene::checkUpgrade_Desired( UpgradeType upgrade ) {
    // A building is not wanted at that moment if we have active builders or the timer is nonzero.
    return !building_gene_.empty() && building_gene_.front().getUpgrade() == upgrade && !active_builders_;
}

bool Building_Gene::checkEmptyBuildOrder() {
    return building_gene_.empty();
}
