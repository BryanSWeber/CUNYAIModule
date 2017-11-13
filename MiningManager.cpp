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

        bool safe_worker = !getClosestThreatOrTargetStored( enemy_inventory, UnitTypes::Zerg_Drone, unit->getPosition(), 250 ) || getClosestThreatOrTargetStored( enemy_inventory, UnitTypes::Zerg_Drone, unit->getPosition(), 250 )->type_.isWorker();

        if ( safe_worker ) {
            for ( auto &p : inv.expo_positions_ ) {
                WalkPosition wp = WalkPosition( friendly_inventory.getMeanBuildingLocation() );
                WalkPosition expo_p = WalkPosition( p );
                int dist_temp = abs(inventory.map_veins_out_[(size_t) wp.x][(size_t)wp.y]-inventory.map_veins_out_[(size_t)expo_p.x][(size_t)expo_p.y]) ;
                bool safe_expo = !getClosestThreatOrTargetStored( enemy_inventory, UnitTypes::Zerg_Hatchery, Position( p ), 500 ) || getClosestThreatOrTargetStored( enemy_inventory, UnitTypes::Zerg_Hatchery, Position( p ), 500 )->type_.isWorker();
                if ( dist_temp < dist ) {
                    dist = dist_temp;
                    inv.setNextExpo( p );
                }
            }
        }
        else {
            unit->stop();
        }

        if ( inv.next_expo_ )
        {
            //clear all obstructions, if any.
            Unitset obstructions = Broodwar->getUnitsInRadius( Position(inv.next_expo_), 3 * 32 );
            obstructions.move( { Position( inv.next_expo_ ).x + (rand() % 200 - 100) * 4 * 32, Position( inv.next_expo_ ).y + (rand() % 200 - 100) * 4 * 32 } );
            obstructions.build( UnitTypes::Zerg_Hatchery, inv.next_expo_ );// I think some thing quirky is happening here. Sometimes gets caught in rebuilding loop.

            Unit_Inventory hatch_builders = getUnitInventoryInRadius( friendly_inventory, UnitTypes::Zerg_Drone, Position( inv.next_expo_ ), 99999 );
            Stored_Unit *best_drone = getClosestStored( hatch_builders, Position( inv.next_expo_ ), 99999 );

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

                if ( unit->build( UnitTypes::Zerg_Hatchery, inv.next_expo_ ) ) {
                    my_reservation.addReserveSystem( UnitTypes::Zerg_Hatchery , inv.next_expo_);
                    buildorder.setBuilding_Complete( UnitTypes::Zerg_Hatchery );
                    //Broodwar->sendText( "Expoing at ( %d , %d ).", inv.next_expo_.x, inv.next_expo_.y );
                    return true;
                }
                else if ( !Broodwar->isExplored( inv.next_expo_ ) ) {
                    unit->move( Position( inv.next_expo_ ) );
                    my_reservation.addReserveSystem( UnitTypes::Zerg_Hatchery , inv.next_expo_);
                    //Broodwar->sendText( "Unexplored Expo at ( %d , %d ). Moving there to check it out.", inv.next_expo_.x, inv.next_expo_.y );
                    return true;
                }
            //}
        }
    } // closure affordablity.

    return false;
}

//Sends a worker to mine minerals.
void MeatAIModule::Worker_Mine( const Unit &unit, Unit_Inventory &ui, const int low_drone ) {

    bool already_assigned = false;
    Stored_Unit& miner = ui.unit_inventory_.find( unit )->second;
    Resource_Inventory available_fields;
//letabot has code on this. "AssignEvenSplit(Unit* unit)"

    if ( unit->getLastCommand().getType() == UnitCommandTypes::Morph || unit->getLastCommand().getType() == UnitCommandTypes::Build || unit->getLastCommand().getTargetPosition() == Position( inventory.next_expo_ ) ) {
        my_reservation.removeReserveSystem( unit->getBuildType() );
    }

    for ( auto& r = neutral_inventory.resource_inventory_.begin(); r != neutral_inventory.resource_inventory_.end() && !neutral_inventory.resource_inventory_.empty(); r++ ) {
        if ( r->second.bwapi_unit_ && r->second.bwapi_unit_->exists() && r->second.number_of_miners_ <= low_drone && r->second.type_.isMineralField() && r->second.occupied_natural_ ) {
            available_fields.addStored_Resource( r->second );
        }
    } //find closest mine meeting this criteria.
    if ( !available_fields.resource_inventory_.empty() ) {
        Stored_Resource* closest = getClosestStored( available_fields, miner.pos_, 9999999 );
        if ( miner.bwapi_unit_->getLastCommand().getTarget() != closest->bwapi_unit_ && miner.bwapi_unit_->gather( closest->bwapi_unit_ ) ) {
            miner.startMine( *closest, neutral_inventory );
        }
    }

} // closure worker mine

  //Sends a Worker to gather Gas.
void MeatAIModule::Worker_Gas( const Unit &unit, Unit_Inventory &ui, const int low_drone ) {

    bool already_assigned = false;
    Stored_Unit& miner = ui.unit_inventory_.find( unit )->second;
    Resource_Inventory available_fields;

    if ( unit->getLastCommand().getType() == UnitCommandTypes::Morph || unit->getLastCommand().getType() == UnitCommandTypes::Build || unit->getLastCommand().getTargetPosition() == Position( inventory.next_expo_ ) ) {
        my_reservation.removeReserveSystem( unit->getBuildType() );
    }

    for ( auto& r = neutral_inventory.resource_inventory_.begin(); r != neutral_inventory.resource_inventory_.end() && !neutral_inventory.resource_inventory_.empty(); r++ ) {
        if ( r->second.bwapi_unit_ && r->second.bwapi_unit_->exists() && r->second.number_of_miners_ <= low_drone && r->second.type_.isRefinery() && r->second.occupied_natural_ ) {
            available_fields.addStored_Resource( r->second );
        }
    } //find closest mine meeting this criteria.
    if ( !available_fields.resource_inventory_.empty() ) {
        Stored_Resource* closest = getClosestStored( available_fields, miner.pos_, 9999999 );
        if ( miner.bwapi_unit_->getLastCommand().getTarget() != closest->bwapi_unit_ && miner.bwapi_unit_->gather( closest->bwapi_unit_ ) ) {
            miner.startMine( *closest, neutral_inventory );
        }
    }


} // closure worker mine

void MeatAIModule::Worker_Clear( const Unit & unit, Unit_Inventory & ui )
{
    if ( unit->getLastCommand().getType() == UnitCommandTypes::Morph || unit->getLastCommand().getType() == UnitCommandTypes::Build || unit->getLastCommand().getTargetPosition() == Position( inventory.next_expo_ ) ) {
        my_reservation.removeReserveSystem( unit->getBuildType() );
    }

    bool already_assigned = false;
    Stored_Unit& miner = ui.unit_inventory_.find( unit )->second;
    Resource_Inventory available_fields;

    for ( auto& r = neutral_inventory.resource_inventory_.begin(); r != neutral_inventory.resource_inventory_.end() && !neutral_inventory.resource_inventory_.empty(); r++ ) {
        if ( r->second.bwapi_unit_ && r->second.bwapi_unit_->exists() && r->second.number_of_miners_ == 0 && r->second.current_stock_value_ <= 8 ) {
            available_fields.addStored_Resource( r->second );
        }
    } //find closest mine meeting this criteria.

    if ( !available_fields.resource_inventory_.empty() ) {
        Stored_Resource* closest = getClosestStored( available_fields, miner.pos_, 9999999 );
        if ( miner.bwapi_unit_->getLastCommand().getTarget() != closest->bwapi_unit_ && miner.bwapi_unit_->gather( closest->bwapi_unit_ ) ) {
            miner.startMine( *closest, neutral_inventory );
        }
    }
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

