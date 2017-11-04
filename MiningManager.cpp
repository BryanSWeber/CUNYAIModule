#pragma once
#include "Source\MeatAIModule.h"

using namespace BWAPI;
using namespace Filter;
using namespace std;

//Builds an expansion. No recognition of past build sites. Needs a drone=unit, some extra boolian logic that you might need, and your inventory, containing resource locations.
bool MeatAIModule::Expo( const Unit &unit, const bool &extra_critera, Inventory &inv ) {
    if ( Broodwar->self()->minerals() >= 300 &&
        (buildorder.checkBuilding_Desired( UnitTypes::Zerg_Hatchery ) || (extra_critera && buildorder.checkEmptyBuildOrder() && !buildorder.active_builders_)) ) {

        int dist = 99999999;

        bool safe_worker = !getClosestThreatOrTargetStored( enemy_inventory, UnitTypes::Zerg_Drone, unit->getPosition(), 250 ) || getClosestThreatOrTargetStored( enemy_inventory, UnitTypes::Zerg_Drone, unit->getPosition(), 250 )->type_.isWorker();

        if ( safe_worker ) {
            for ( auto &p : inv.expo_positions_ ) {
                int dist_temp = unit->getPosition().getDistance( Position( p ) );
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
            //Unitset obstructions = Broodwar->getUnitsInRadius( Position(inv.next_expo_), 3 * 32 );
            //obstructions.move( { Position( inv.next_expo_ ).x + (rand() % 200 - 100) * 4 * 32, Position( inv.next_expo_ ).y + (rand() % 200 - 100) * 4 * 32 } );
            //obstructions.build( UnitTypes::Zerg_Hatchery, inv.next_expo_ );// I think some thing quirky is happening here. Sometimes gets caught in rebuilding loop.

            Unit_Inventory hatch_builders = getUnitInventoryInRadius( friendly_inventory, UnitTypes::Zerg_Drone, Position( inv.next_expo_ ), 99999 );
            Stored_Unit *best_drone = getClosestStored( hatch_builders, Position( inv.next_expo_ ), 99999 );

            if ( best_drone && best_drone->bwapi_unit_ ) {
                if ( best_drone->bwapi_unit_->build( UnitTypes::Zerg_Hatchery, inv.next_expo_ ) ) {
                    buildorder.setBuilding_Complete( UnitTypes::Zerg_Hatchery );
                    Broodwar->sendText( "Expoing at ( %d , %d ).", inv.next_expo_.x, inv.next_expo_.y );
                    return true;
                }
                else if ( !Broodwar->isExplored( inv.next_expo_ ) ) {
                    best_drone->bwapi_unit_->move( Position( inv.next_expo_ ) );
                    buildorder.active_builders_ = true;
                    Broodwar->sendText( "Unexplored Expo at ( %d , %d ). Moving there to check it out.", inv.next_expo_.x, inv.next_expo_.y );
                    return true;
                }
            }
            else {
                if ( unit->build( UnitTypes::Zerg_Hatchery, inv.next_expo_ ) ) {
                    buildorder.setBuilding_Complete( UnitTypes::Zerg_Hatchery );
                    Broodwar->sendText( "Expoing at ( %d , %d ).", inv.next_expo_.x, inv.next_expo_.y );
                    return true;
                }
                else if ( !Broodwar->isExplored( inv.next_expo_ ) ) {
                    unit->move( Position( inv.next_expo_ ) );
                    buildorder.active_builders_ = true;
                    Broodwar->sendText( "Unexplored Expo at ( %d , %d ). Moving there to check it out.", inv.next_expo_.x, inv.next_expo_.y );
                    return true;
                }
            }
        }
    } // closure affordablity.

    return false;
}

//Sends a worker to mine minerals.
void MeatAIModule::Worker_Mine( const Unit &unit, Unit_Inventory &ui ) {

    bool already_assigned = false;
    Stored_Unit& miner = ui.unit_inventory_.find( unit )->second;
    Resource_Inventory available_fields;
    int miner_count = 0;
    int low_drone = 1; //letabot has code on this. "AssignEvenSplit(Unit* unit)"

    for ( auto& r = neutral_inventory.resource_inventory_.begin(); r != neutral_inventory.resource_inventory_.end() && !neutral_inventory.resource_inventory_.empty(); r++ ) {
        miner_count += r->second.number_of_miners_;
        if ( r->second.bwapi_unit_ && r->second.bwapi_unit_->exists() && r->second.number_of_miners_< low_drone && r->second.type_.isMineralField() && r->second.occupied_natural_ ) {
            low_drone = r->second.number_of_miners_;
        }
    } // find drone minima.

    for ( auto& r = neutral_inventory.resource_inventory_.begin(); r != neutral_inventory.resource_inventory_.end() && !neutral_inventory.resource_inventory_.empty(); r++ ) {
        if ( r->second.bwapi_unit_ && r->second.bwapi_unit_->exists() && r->second.number_of_miners_ == low_drone && r->second.type_.isMineralField() && r->second.occupied_natural_ ) {
            available_fields.addStored_Resource( r->second );
        }
    } //find closest mine meeting this criteria.

    if ( !available_fields.resource_inventory_.empty() ) { // if there are fields to mine
        Stored_Unit* nearest_building = getClosestStored( friendly_inventory, UnitTypes::Zerg_Hatchery, miner.pos_, 9999999 );
        if ( !nearest_building ) {
            Stored_Unit* nearest_building = getClosestStored( friendly_inventory, UnitTypes::Zerg_Lair, miner.pos_, 9999999 );
        }
        if ( !nearest_building ) {
            Stored_Unit* nearest_building = getClosestStored( friendly_inventory, UnitTypes::Zerg_Hive, miner.pos_, 9999999 );
        }

        if ( nearest_building ) {
            Stored_Resource* closest = getClosestStored( available_fields, nearest_building->pos_, 9999999 );
            if ( miner.bwapi_unit_->getLastCommand().getTarget() != closest->bwapi_unit_ && miner.bwapi_unit_->gather( closest->bwapi_unit_ ) ) {
                miner.startMine( *closest, neutral_inventory );
            }
        }
    } // send drone to closest base's closest mineral field meeting that critera.
    else { // if there are no suitible bases:
           // mine those empty patches, perhaps?
    }

    miner_count_ += miner_count;

    //  Unit local_base = unit->getClosestUnit( IsResourceDepot && IsOwned && IsCompleted );

    //  if ( local_base && local_base->exists() ) {

    //Resource_Inventory local_sat = getResourceInventoryInRadius(neutral_inventory, local_base->getPosition(), 400);
    //	if (isOnScreen(local_base->getPosition())){
    //		Broodwar->drawCircleMap(local_base->getPosition(), 300, Colors::Green);
    //	}
    //	bool fully_saturated = true;
    //	for (auto r = local_sat.resource_inventory_.begin(); r != local_sat.resource_inventory_.end(); r++){
    //		if (!r->second.full_resource_){
    //			fully_saturated = false;
    //			break;
    //		}
    //	}

    //          int local_dist = unit->getDistance( local_base );

    //          bool acceptable_local = local_dist < 500 && !fully_saturated; // if local conditions are fine (and you are working), continue, no commands.

    //          if ( !acceptable_local && rand() % 100 + 1 < 25 ) { // if local conditions are bad, we will consider tranfering 25% of these workers elsewhere.
    //              Unitset bases = unit->getUnitsInRadius( 999999, IsResourceDepot && IsOwned );

    //              int nearest_dist = 9999999;
    //              for ( auto base = bases.begin(); base != bases.end() && !bases.empty(); ++base ) {
    //                  if ( (*base)->exists() ) {
    //                      int dist = unit->getDistance( *base );

    //                      Unitset base_sat = (*base)->getUnitsInRadius( 500, IsWorker && (IsGatheringMinerals || IsCarryingMinerals) );
    //                      Unitset base_min = (*base)->getUnitsInRadius( 500, IsMineralField );
    //                      bool acceptable_foreign = !base_min.empty() && (int)base_sat.size() < (int)base_min.size(); // there must be a severe desparity to switch, and please only switch to ones clearly different than your local mine, 250 p.

    //                      if ( dist > 750 && dist < nearest_dist && acceptable_foreign) {
    //                          nearest_dist = dist; // transfer to the nearest undersaturated base.
    //					//Resource_Inventory local_sat = getResourceInventoryInRadius(neutral_inventory, (*base)->getPosition(), 400);
    //					//for (auto r = local_sat.resource_inventory_.begin(); r != local_sat.resource_inventory_.end() && !local_sat.resource_inventory_.empty(); r++){
    //					//	if (r->second.bwapi_unit_ && r->second.bwapi_unit_->exists() && !r->second.full_resource_){
    //					//		unit->gather(r->second.bwapi_unit_); // if you are idle, get to work.
    //					//		Broodwar->sendText("Trying To Mine long distance");
    //					//		Unit old_mine = friendly_inventory.unit_inventory_.at(unit).locked_mine_;
    //					//		neutral_inventory.resource_inventory_.at(r->second.bwapi_unit_).addMiner(unit);// that mine is now busy.
    //					//		friendly_inventory.unit_inventory_.at(unit).addMine(r->second.bwapi_unit_); // that miner is now locked to r
    //					//		break;
    //					//	}
    //					//}
    //                      } // closure for base being a canidate for transfer.
    //                  } // closure for base existance.
    //              } // iterate through all possible bases
    //          } // closure for worker transfer
    //      } // closure safety check for existance of local base.
} // closure worker mine

  //Sends a Worker to gather Gas.
void MeatAIModule::Worker_Gas( const Unit &unit, Unit_Inventory &ui ) {

    bool already_assigned = false;
    Stored_Unit& miner = ui.unit_inventory_.find( unit )->second;
    Resource_Inventory available_fields;
    int miner_count = 0;
    int low_drone = 2; //letabot has code on this. "AssignEvenSplit(Unit* unit)"

    for ( auto& r = neutral_inventory.resource_inventory_.begin(); r != neutral_inventory.resource_inventory_.end() && !neutral_inventory.resource_inventory_.empty(); r++ ) {
        miner_count += r->second.number_of_miners_;
        if ( r->second.bwapi_unit_ && r->second.bwapi_unit_->exists() && r->second.number_of_miners_< low_drone && r->second.type_.isRefinery() && r->second.occupied_natural_ ) {
            low_drone = r->second.number_of_miners_;
        }
    } // find drone minima.

    for ( auto& r = neutral_inventory.resource_inventory_.begin(); r != neutral_inventory.resource_inventory_.end() && !neutral_inventory.resource_inventory_.empty(); r++ ) {
        if ( r->second.bwapi_unit_ && r->second.bwapi_unit_->exists() && r->second.number_of_miners_ == low_drone && r->second.type_.isRefinery() && r->second.occupied_natural_ ) {
            available_fields.addStored_Resource( r->second );
        }
    } //find closest mine meeting this criteria.

    if ( !available_fields.resource_inventory_.empty() ) { // if there are fields to mine
        Stored_Unit* nearest_building = getClosestStored( friendly_inventory, UnitTypes::Zerg_Hatchery, miner.pos_, 9999999 );
        if ( !nearest_building ) {
            Stored_Unit* nearest_building = getClosestStored( friendly_inventory, UnitTypes::Zerg_Lair, miner.pos_, 9999999 );
        }
        if ( !nearest_building ) {
            Stored_Unit* nearest_building = getClosestStored( friendly_inventory, UnitTypes::Zerg_Hive, miner.pos_, 9999999 );
        }

        if ( nearest_building ) {
            Stored_Resource* closest = getClosestStored( available_fields, nearest_building->pos_, 9999999 );
            if ( miner.bwapi_unit_->getLastCommand().getTarget() != closest->bwapi_unit_ && miner.bwapi_unit_->gather( closest->bwapi_unit_ ) ) {
                miner.startMine( *closest, neutral_inventory );
            }
        }
    } // send drone to closest base's closest mineral field meeting that critera.
    miner_count_ += miner_count;
} // closure worker mine

  //Returns True if there is an out for gas. Does not consider all possible gas outlets.
bool MeatAIModule::Gas_Outlet() {
    bool outlet_avail = false;

    if ( MeatAIModule::Tech_Avail()/* && tech_starved*/ && Count_Units( BWAPI::UnitTypes::Zerg_Spawning_Pool, friendly_inventory ) > 0 ) {
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

