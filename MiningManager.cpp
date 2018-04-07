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
                int dist_temp = inv.getDifferentialDistanceOutFromHome(friendly_inventory.getMeanBuildingLocation(), Position(p));

                bool safe_expo = checkSafeBuildLoc(Position(p), inventory, enemy_inventory, friendly_inventory, neutral_inventory);

                bool occupied_expo =getClosestStored( friendly_inventory, UnitTypes::Zerg_Hatchery, Position( p ), 500 ) ||
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

            if ( Broodwar->isExplored( inv.next_expo_ ) && unit->build( UnitTypes::Zerg_Hatchery, inv.next_expo_ ) && my_reservation.addReserveSystem(UnitTypes::Zerg_Hatchery, inv.next_expo_)) {
                Broodwar->sendText( "Expoing at ( %d , %d ).", inv.next_expo_.x, inv.next_expo_.y );
                return true;
            }
            if ( !Broodwar->isExplored( inv.next_expo_ ) && my_reservation.addReserveSystem(UnitTypes::Zerg_Hatchery, inv.next_expo_)) {
                unit->move( Position( inv.next_expo_ ) );
                Broodwar->sendText( "Unexplored Expo at ( %d , %d ). Moving there to check it out.", inv.next_expo_.x, inv.next_expo_.y );
                return true;
            }
            //}
        }
    } // closure affordablity.

    return false;
}


  //Sends a Worker to gather resources from UNITTYPE (mineral or refinery). letabot has code on this. "AssignEvenSplit(Unit* unit)"
void MeatAIModule::Worker_Gather(const Unit &unit, const UnitType mine, Unit_Inventory &ui) {

    bool already_assigned = false;
    bool building_unit = unit->getLastCommand().getType() == UnitCommandTypes::Morph || unit->getLastCommand().getType() == UnitCommandTypes::Build || unit->getLastCommand().getTargetPosition() == Position(inventory.next_expo_);
    Stored_Unit& miner = ui.unit_inventory_.find(unit)->second;
    Resource_Inventory available_fields;
    Resource_Inventory long_dist_fields;

    int low_drone = 0;
    bool mine_minerals = mine.isMineralField();
    bool mine_is_right_type = true;

    if ( mine_minerals ) {
        low_drone = 1;
    } else {
        low_drone = 2;
    }

    for (auto& r = neutral_inventory.resource_inventory_.begin(); r != neutral_inventory.resource_inventory_.end() && !neutral_inventory.resource_inventory_.empty(); r++) {

        if (mine_minerals) {
            mine_is_right_type = r->second.type_.isMineralField();
        }
        else {
            mine_is_right_type = r->second.type_.isRefinery();
        }

        if ( mine_is_right_type && r->second.pos_.isValid() && r->second.number_of_miners_ <= low_drone && r->second.occupied_natural_) {
            low_drone = r->second.number_of_miners_;
        }

    } // find drone minima.

    for (auto& r = neutral_inventory.resource_inventory_.begin(); r != neutral_inventory.resource_inventory_.end() && !neutral_inventory.resource_inventory_.empty(); r++) {

        if (mine_minerals) {
            mine_is_right_type = r->second.type_.isMineralField();
        }
        else {
            mine_is_right_type = r->second.type_.isRefinery();
        }

        if (mine_is_right_type && r->second.number_of_miners_ <= low_drone) {
            if (r->second.occupied_natural_ /*&& checkSafeBuildLoc(r->second.pos_, inventory, enemy_inventory, friendly_inventory, neutral_inventory)*/) {
                available_fields.addStored_Resource(r->second);
            }
            else {
                long_dist_fields.addStored_Resource(r->second);
            }
        }
    } //find closest mine meeting this criteria.

    if (!available_fields.resource_inventory_.empty()) {
        Stored_Resource* closest = getClosestGroundStored(available_fields, inventory, miner.pos_);
        if ( closest && closest->bwapi_unit_ && miner.bwapi_unit_->gather(closest->bwapi_unit_) /*&& checkSafeMineLoc(closest->pos_, ui, inventory)*/) {
            miner.startMine(*closest, neutral_inventory);
            if (building_unit) {
                my_reservation.removeReserveSystem(unit->getBuildType());
            }
        }
    } else if (!long_dist_fields.resource_inventory_.empty()) {
        Stored_Resource * closest = getClosestGroundStored(long_dist_fields, inventory, miner.pos_);

        if (closest && closest->bwapi_unit_ && miner.bwapi_unit_->gather(closest->bwapi_unit_)) {
            miner.startMine(*closest, neutral_inventory);

            if (building_unit) {
                my_reservation.removeReserveSystem(unit->getBuildType());
            }

        }
        else if (closest && miner.bwapi_unit_->move(closest->pos_) /*&& checkSafeMineLoc(closest->pos_, ui, inventory)*/ ) {
            miner.startMine(*closest, neutral_inventory);

            if (building_unit) {
                my_reservation.removeReserveSystem(unit->getBuildType());
            }
        }
    }


} // closure worker mine

void MeatAIModule::Worker_Clear( const Unit & unit, Unit_Inventory & ui )
{
    bool already_assigned = false;
    bool building_unit = unit->getLastCommand().getType() == UnitCommandTypes::Morph || unit->getLastCommand().getType() == UnitCommandTypes::Build || unit->getLastCommand().getTargetPosition() == Position(inventory.next_expo_);
    Stored_Unit& miner = ui.unit_inventory_.find(unit)->second;
    Resource_Inventory available_fields;

    for (auto& r = neutral_inventory.resource_inventory_.begin(); r != neutral_inventory.resource_inventory_.end() && !neutral_inventory.resource_inventory_.empty(); r++) {
        if ( r->second.current_stock_value_ <= 8 && r->second.number_of_miners_ < 1 && r->second.pos_.isValid() && r->second.type_.isMineralField() ) {
            available_fields.addStored_Resource(r->second);
        }
    } //find closest mine meeting this criteria.

    if (!available_fields.resource_inventory_.empty()) {
        Stored_Resource* closest = getClosestGroundStored(available_fields, inventory, miner.pos_);
        if ( closest && miner.bwapi_unit_->gather(closest->bwapi_unit_) ) {
            miner.startMine(*closest, neutral_inventory);

            if (building_unit) {
                my_reservation.removeReserveSystem(unit->getBuildType());
            }

        }
        else if (closest && miner.bwapi_unit_->move(closest->pos_) && checkSafeMineLoc(closest->pos_, ui, inventory) ) {
            miner.startMine(*closest, neutral_inventory); // I think the problem is here. Starting to mine a location without a proper bwapi unit.
            if (building_unit) {
                my_reservation.removeReserveSystem(unit->getBuildType());
            }
        }
    }
}

bool MeatAIModule::Nearby_Blocking_Minerals(const Unit & unit, Unit_Inventory & ui)
{

    bool already_assigned = false;
    Stored_Unit& miner = ui.unit_inventory_.find(unit)->second;
    Resource_Inventory available_fields;

    for (auto& r = neutral_inventory.resource_inventory_.begin(); r != neutral_inventory.resource_inventory_.end() && !neutral_inventory.resource_inventory_.empty(); r++) {
        if (r->second.current_stock_value_ <= 8 && !checkOccupiedArea(enemy_inventory, r->second.pos_, 250) && unit->getDistance(r->second.pos_) < 5000 ) {
            return true;
        }
    } //find closest mine meeting this criteria.

    return false;
}

  //Returns True if there is an out for gas. Does not consider all possible gas outlets.
bool MeatAIModule::Gas_Outlet() {
    bool outlet_avail = false;

    if ( MeatAIModule::Tech_Avail() && Count_Units( BWAPI::UnitTypes::Zerg_Spawning_Pool, inventory ) > 0 ) {
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

