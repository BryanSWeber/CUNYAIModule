#pragma once
#include "Source\MeatAIModule.h"

using namespace BWAPI;
using namespace Filter;
using namespace std;

//Builds an expansion. No recognition of past build sites. Needs a drone=unit, some extra boolian logic that you might need, and your inventory, containing resource locations.
bool MeatAIModule::Expo( const Unit &unit, const bool &extra_critera, Inventory &inv ) {
    if ( my_reservation.checkAffordablePurchase( UnitTypes::Zerg_Hatchery ) && 
        (buildorder.checkBuilding_Desired( UnitTypes::Zerg_Hatchery ) || (extra_critera && buildorder.checkEmptyBuildOrder()) ) ) {

        int dist = 99999999;

        bool safe_worker = enemy_inventory.unit_inventory_.empty() ||
            getClosestThreatOrTargetStored( enemy_inventory, UnitTypes::Zerg_Drone, unit->getPosition(), 500 ) == nullptr ||
            getClosestThreatOrTargetStored( enemy_inventory, UnitTypes::Zerg_Drone, unit->getPosition(), 500 )->type_.isWorker();

        if ( safe_worker ) {
            for ( auto &p : inv.expo_positions_ ) {
                int dist_temp = inv.getDifferentialDistanceOutFromHome( friendly_inventory.getMeanBuildingLocation(), Position(p) );

                bool safe_expo = enemy_inventory.unit_inventory_.empty() ||
                    getClosestThreatOrTargetStored( enemy_inventory, UnitTypes::Zerg_Hatchery, Position( p ), 500 ) == nullptr ||
                    getClosestThreatOrTargetStored( enemy_inventory, UnitTypes::Zerg_Hatchery, Position( p ), 500 )->type_.isWorker();

                bool occupied_expo = getClosestStored( friendly_inventory, UnitTypes::Zerg_Hatchery, Position( p ), 500 ) ||
                    getClosestStored( friendly_inventory, UnitTypes::Zerg_Lair, Position( p ), 500 ) ||
                    getClosestStored( friendly_inventory, UnitTypes::Zerg_Hive, Position( p ), 500 );

                if ( dist_temp < dist && safe_expo && !occupied_expo) {
                    dist = dist_temp;
                    inv.setNextExpo( p );
                }
            }
        }
        else {
            return false;
        }

        if ( inv.next_expo_ )
        {
            //clear all obstructions, if any.
            Unit_Inventory obstructions = getUnitInventoryInRadius( friendly_inventory, Position( inv.next_expo_ ), 3 * 32 );
            for ( auto u = obstructions.unit_inventory_.begin(); u != obstructions.unit_inventory_.end() && !obstructions.unit_inventory_.empty(); u++ ) {
                if ( u->second.type_ != UnitTypes::Zerg_Drone && u->second.bwapi_unit_ ) {
                    u->second.bwapi_unit_->move( { Position( inv.next_expo_ ).x + (rand() % 200 - 100) * 4 * 32, Position( inv.next_expo_ ).y + (rand() % 200 - 100) * 4 * 32 } );
                }
            }

            //Unit_Inventory hatch_builders = getUnitInventoryInRadius( friendly_inventory, UnitTypes::Zerg_Drone, Position( inv.next_expo_ ), 99999 );
            //Stored_Unit *best_drone = getClosestStored( hatch_builders, Position( inv.next_expo_ ), 99999 );

            //if ( best_drone && best_drone->bwapi_unit_ ) {
            //    if ( best_drone->bwapi_unit_->build( UnitTypes::Zerg_Hatchery, inv.next_expo_ ) ) {
            //        buildorder.setBuilding_Complete( UnitTypes::Zerg_Hatchery );
            //        Broodwar->sendText( "Expoing at ( %d , %d ).", inv.next_expo_.x, inv.next_expo_.y );
            //        return true;
            //    }
            //    else if ( !Broodwar->isExplored( inv.next_expo_ ) ) {
            //        best_drone->bwapi_unit_->move( Position( inv.next_expo_ ) );
            //        buildorder.active_builders_ = true;
            //        Broodwar->sendText( "Unexplored Expo at ( %d , %d ). Moving there to check it out.", inv.next_expo_.x, inv.next_expo_.y );
            //        return true;
            //    }
            //}
            //else {      

            if ( unit->getLastCommand().getType() == UnitCommandTypes::Morph || unit->getLastCommand().getType() == UnitCommandTypes::Build || unit->getLastCommand().getTargetPosition() == Position( inventory.next_expo_ ) ) {
                my_reservation.removeReserveSystem( unit->getBuildType() );
            }

            if ( Broodwar->isExplored( inv.next_expo_ ) && unit->build( UnitTypes::Zerg_Hatchery, inv.next_expo_ ) ) {
                my_reservation.addReserveSystem( UnitTypes::Zerg_Hatchery, inv.next_expo_ );
                buildorder.setBuilding_Complete( UnitTypes::Zerg_Hatchery );
                Broodwar->sendText( "Expoing at ( %d , %d ).", inv.next_expo_.x, inv.next_expo_.y );
                return true;
            }
            if ( !Broodwar->isExplored( inv.next_expo_ ) ) {
                unit->move( Position( inv.next_expo_ ) );
                my_reservation.addReserveSystem( UnitTypes::Zerg_Hatchery, inv.next_expo_ );
                Broodwar->sendText( "Unexplored Expo at ( %d , %d ). Moving there to check it out.", inv.next_expo_.x, inv.next_expo_.y );
                return true;
            }
            //}
        }
    } // closure affordablity.

    return false;
}

//Sends a worker to mine minerals.
void MeatAIModule::Worker_Mine( const Unit &unit, Unit_Inventory &ui ) {

    bool already_assigned = false;
    Stored_Unit& miner = ui.unit_inventory_.find( unit )->second;
    Resource_Inventory available_fields;
//letabot has code on this. "AssignEvenSplit(Unit* unit)"

    int low_drone_min = 1;

    for (auto& r = neutral_inventory.resource_inventory_.begin(); r != neutral_inventory.resource_inventory_.end() && !neutral_inventory.resource_inventory_.empty(); r++) {
        if (r->second.bwapi_unit_ && r->second.bwapi_unit_->exists() && r->second.type_.isMineralField() && r->second.occupied_natural_) {
            miner_count_ += r->second.number_of_miners_;
        }
        if (r->second.bwapi_unit_ && r->second.bwapi_unit_->exists() && r->second.number_of_miners_< low_drone_min && r->second.type_.isMineralField() && r->second.occupied_natural_) {
            low_drone_min = r->second.number_of_miners_;
        }
    } // find drone minima.


    if ( unit->getLastCommand().getType() == UnitCommandTypes::Morph || unit->getLastCommand().getType() == UnitCommandTypes::Build || unit->getLastCommand().getTargetPosition() == Position( inventory.next_expo_ ) ) {
        my_reservation.removeReserveSystem( unit->getBuildType() );
    }

    for ( auto& r = neutral_inventory.resource_inventory_.begin(); r != neutral_inventory.resource_inventory_.end() && !neutral_inventory.resource_inventory_.empty(); r++ ) {
        if ( r->second.bwapi_unit_ && r->second.bwapi_unit_->exists() && r->second.number_of_miners_ <= low_drone_min && r->second.type_.isMineralField() && r->second.occupied_natural_ ) {
            available_fields.addStored_Resource( r->second );
        }
    } //find closest mine meeting this criteria.
    if (!available_fields.resource_inventory_.empty()) {
        Stored_Resource* closest = getClosestStored(available_fields, miner.pos_, 9999999);
        if (closest->bwapi_unit_->exists() && miner.bwapi_unit_->gather(closest->bwapi_unit_)) {
            miner.startMine(*closest, neutral_inventory);
        }
    }

} // closure worker mine

  //Sends a Worker to gather Gas.
void MeatAIModule::Worker_Gas( const Unit &unit, Unit_Inventory &ui ) {

    bool already_assigned = false;
    Stored_Unit& miner = ui.unit_inventory_.find( unit )->second;
    Resource_Inventory available_fields;

    int low_drone_gas = 2; //letabot has code on this. "AssignEvenSplit(Unit* unit)"

    for (auto& r = neutral_inventory.resource_inventory_.begin(); r != neutral_inventory.resource_inventory_.end() && !neutral_inventory.resource_inventory_.empty(); r++) {
        if (r->second.bwapi_unit_ && r->second.bwapi_unit_->exists() && r->second.type_.isRefinery() && r->second.occupied_natural_) {
            gas_count_ += r->second.number_of_miners_;
        }
        if (r->second.bwapi_unit_ && r->second.bwapi_unit_->exists() && r->second.number_of_miners_< low_drone_gas && r->second.type_.isRefinery() && r->second.occupied_natural_) {
            low_drone_gas = r->second.number_of_miners_;
        }
    } // find drone minima.

    if ( unit->getLastCommand().getType() == UnitCommandTypes::Morph || unit->getLastCommand().getType() == UnitCommandTypes::Build || unit->getLastCommand().getTargetPosition() == Position( inventory.next_expo_ ) ) {
        my_reservation.removeReserveSystem( unit->getBuildType() );
    }

    for ( auto& r = neutral_inventory.resource_inventory_.begin(); r != neutral_inventory.resource_inventory_.end() && !neutral_inventory.resource_inventory_.empty(); r++ ) {
        if ( r->second.bwapi_unit_ && r->second.bwapi_unit_->exists() && r->second.number_of_miners_ <= low_drone_gas && r->second.type_.isRefinery() && r->second.occupied_natural_ ) {
            available_fields.addStored_Resource( r->second );
        }
    } //find closest mine meeting this criteria.

    if ( !available_fields.resource_inventory_.empty() ) {
        Stored_Resource* closest = getClosestStored( available_fields, miner.pos_, 9999999 );
        if (closest->bwapi_unit_->exists() && miner.bwapi_unit_->gather(closest->bwapi_unit_)) {
            miner.startMine(*closest, neutral_inventory);
        }
    }


} // closure worker mine

void MeatAIModule::Worker_Clear( const Unit & unit, Unit_Inventory & ui )
{
    bool already_assigned = false;
    Stored_Unit& miner = ui.unit_inventory_.find(unit)->second;
    Resource_Inventory available_fields;

    if (unit->getLastCommand().getType() == UnitCommandTypes::Morph || unit->getLastCommand().getType() == UnitCommandTypes::Build || unit->getLastCommand().getTargetPosition() == Position(inventory.next_expo_)) {
        my_reservation.removeReserveSystem(unit->getBuildType());
    }

    for (auto& r = neutral_inventory.resource_inventory_.begin(); r != neutral_inventory.resource_inventory_.end() && !neutral_inventory.resource_inventory_.empty(); r++) {
        if (r->second.current_stock_value_ <= 8 && r->second.number_of_miners_ <= 1 && r->second.type_.isMineralField() ) {
            available_fields.addStored_Resource(r->second);
        }
    } //find closest mine meeting this criteria.
    if (!available_fields.resource_inventory_.empty()) {
        Stored_Resource* closest = getClosestStored(available_fields, miner.pos_, 9999999);
        if (closest->bwapi_unit_->exists() && miner.bwapi_unit_->gather(closest->bwapi_unit_)) {
            miner.startMine(*closest, neutral_inventory);
        }
        else {
            miner.bwapi_unit_->move(closest->pos_);
            miner.startMine(*closest, neutral_inventory);
        }
    }
}

bool MeatAIModule::Nearby_Blocking_Minerals(const Unit & unit, Unit_Inventory & ui)
{

    bool already_assigned = false;
    Stored_Unit& miner = ui.unit_inventory_.find(unit)->second;
    Resource_Inventory available_fields;

    for (auto& r = neutral_inventory.resource_inventory_.begin(); r != neutral_inventory.resource_inventory_.end() && !neutral_inventory.resource_inventory_.empty(); r++) {
        if (r->second.current_stock_value_ <= 8 && getClosestThreatOrTargetStored(enemy_inventory, UnitTypes::Zerg_Drone, r->second.pos_, 500) == nullptr && unit->getDistance(r->second.pos_) < 2000 ) {
            return true;
        }
    } //find closest mine meeting this criteria.

    return false;
}

  //Returns True if there is an out for gas. Does not consider all possible gas outlets.
bool MeatAIModule::Gas_Outlet() {
    bool outlet_avail = false;

    if ( MeatAIModule::Tech_Avail() && Count_Units( BWAPI::UnitTypes::Zerg_Spawning_Pool, friendly_inventory ) > 0 ) {
        outlet_avail = true;
    }


    bool long_condition = Broodwar->self()->hasUnitTypeRequirement( UnitTypes::Zerg_Hydralisk ) ||
        Broodwar->self()->hasUnitTypeRequirement( UnitTypes::Zerg_Mutalisk ) ||
        Broodwar->self()->hasUnitTypeRequirement( UnitTypes::Zerg_Ultralisk );
    if ( long_condition ) {
        outlet_avail = true;
    } // turns off gas interest when larve are 0.

      //bool long_condition = Count_Units(UnitTypes::Zerg_Hydralisk_Den, friendly_inventory) > 0 ||
      //		Count_Units( UnitTypes::Zerg_Spire, friendly_inventory ) > 0 ||
      //		Count_Units( UnitTypes::Zerg_Ultralisk_Cavern, friendly_inventory ) > 0;
      //if ( long_condition ) {
      //    outlet_avail = true;
      //} 

    return outlet_avail;
}

