#pragma once
#include "Source\CUNYAIModule.h"

using namespace BWAPI;
using namespace Filter;
using namespace std;

//Builds an expansion. No recognition of past build sites. Needs a drone=unit, some extra boolian logic that you might need, and your inventory, containing resource locations. Now Updates Friendly inventory when command is sent.
bool CUNYAIModule::Expo( const Unit &unit, const bool &extra_critera, Map_Inventory &inv ) {
    if ( my_reservation.checkAffordablePurchase( UnitTypes::Zerg_Hatchery ) && 
        (buildorder.checkBuilding_Desired( UnitTypes::Zerg_Hatchery ) || (extra_critera && buildorder.isEmptyBuildOrder()) ) ) {

        int dist = 99999999;
        inv.getExpoPositions(); // update the possible expo positions.
        inv.setNextExpo(TilePositions::Origin); // if we find no replacement position, we will know this null postion is never a good build canidate.

        bool safe_worker = enemy_player_model.units_.unit_inventory_.empty() ||
            getClosestThreatOrTargetStored( enemy_player_model.units_, UnitTypes::Zerg_Drone, unit->getPosition(), 500 ) == nullptr ||
            getClosestThreatOrTargetStored( enemy_player_model.units_, UnitTypes::Zerg_Drone, unit->getPosition(), 500 )->type_.isWorker();

        // Let's build at the safest close canidate position.
        if ( safe_worker ) {
            for ( auto &p : inv.expo_positions_ ) {
                int dist_temp = inv.getRadialDistanceOutFromHome(Position(p)) ;

                bool safe_expo = checkSafeBuildLoc(Position(p), inv, enemy_player_model.units_, friendly_player_model.units_, land_inventory);

                bool occupied_expo =getClosestStored( friendly_player_model.units_, UnitTypes::Zerg_Hatchery, Position( p ), 500 ) ||
                                    getClosestStored( friendly_player_model.units_, UnitTypes::Zerg_Lair, Position( p ), 500 ) ||
                                    getClosestStored( friendly_player_model.units_, UnitTypes::Zerg_Hive, Position( p ), 500 );
                bool expansion_is_home = inv.home_base_.getDistance(Position(p)) <= 32;
                if ( (dist_temp < dist || expansion_is_home) && safe_expo && !occupied_expo) {
                    dist = dist_temp;
                    inv.setNextExpo( p );
					CUNYAIModule::DiagnosticText("Found an expo at ( %d , %d )", inv.next_expo_.x, inv.next_expo_.y);
                }
            }
        }
        else {
            return false;  // If there's nothing, give up.
        }

        // If we found -something-
        if ( inv.next_expo_ && inv.next_expo_ != TilePositions::Origin ) {
            //clear all obstructions, if any.
            clearBuildingObstuctions(friendly_player_model.units_, inv, unit);

            if ( Broodwar->isExplored( inv.next_expo_ ) && unit->build( UnitTypes::Zerg_Hatchery, inv.next_expo_ ) && my_reservation.addReserveSystem(UnitTypes::Zerg_Hatchery, inv.next_expo_)) {
                CUNYAIModule::DiagnosticText( "Expoing at ( %d , %d ).", inv.next_expo_.x, inv.next_expo_.y );
                Stored_Unit& morphing_unit = friendly_player_model.units_.unit_inventory_.find(unit)->second;
                morphing_unit.updateStoredUnit(unit);
                return true;
            }
            else if ( !Broodwar->isExplored( inv.next_expo_ ) && my_reservation.addReserveSystem(UnitTypes::Zerg_Hatchery, inv.next_expo_)) {
                unit->move( Position( inv.next_expo_ ) );
                Stored_Unit& morphing_unit = friendly_player_model.units_.unit_inventory_.find(unit)->second;
                morphing_unit.updateStoredUnit(unit);
                CUNYAIModule::DiagnosticText( "Unexplored Expo at ( %d , %d ). Moving there to check it out.", inv.next_expo_.x, inv.next_expo_.y );
                return true;
            }
            //}
        }
    } // closure affordablity.

    return false;
}


  //Sends a Worker to gather resources from UNITTYPE (mineral or refinery). letabot has code on this. "AssignEvenSplit(Unit* unit)"
void CUNYAIModule::Worker_Gather(const Unit &unit, const UnitType mine, Unit_Inventory &ui) {

    bool already_assigned = false;
    Stored_Unit& miner = ui.unit_inventory_.find(unit)->second;
    //bool building_unit = unit->getLastCommand().getType() == UnitCommandTypes::Morph || unit->getLastCommand().getType() == UnitCommandTypes::Build || unit->getLastCommand().getTargetPosition() == Position(inventory.next_expo_);
    bool building_unit = miner.isAssignedBuilding(land_inventory);
    Resource_Inventory available_fields;
    Resource_Inventory long_dist_fields;

    int low_drone = 0;
    int max_drone = 3;
    bool mine_minerals = mine.isMineralField();
    bool mine_is_right_type = true;
    bool found_low_occupied_mine = false;

    // mineral patches can handle up to 2 miners, gas/refineries can handle up to 3.
    if ( mine_minerals ) {
        low_drone = 2;
        max_drone = 2;
    } else {
        low_drone = 2; // note : Does not count worker IN extractor.
        max_drone = 3;
    }


    // scrape over every resource to determine the lowest number of miners.
    for (auto& r = land_inventory.resource_inventory_.begin(); r != land_inventory.resource_inventory_.end() && !land_inventory.resource_inventory_.empty(); r++) {

        if (mine_minerals) {
            mine_is_right_type = r->second.type_.isMineralField() && r->second.max_stock_value_ >= 8; // Only gather from "Real" mineral patches with substantive value. Don't mine from obstacles.
        }
        else {
            mine_is_right_type = r->second.type_.isRefinery() && r->second.bwapi_unit_ && IsOwned(r->second.bwapi_unit_);
        }

        if ( mine_is_right_type && r->second.pos_.isValid() && r->second.number_of_miners_ < low_drone && r->second.number_of_miners_ < max_drone && r->second.occupied_natural_ && current_map_inventory.checkViableGroundPath(r->second.pos_, miner.pos_)) { //occupied natural -> resource is close to a base
            low_drone = r->second.number_of_miners_;
            found_low_occupied_mine = true;
        }

    } // find drone minima.

    //    CUNYAIModule::DiagnosticText("LOW DRONE COUNT : %d", low_drone);
    //    CUNYAIModule::DiagnosticText("Mine Minerals : %d", mine_minerals);


    for (auto& r = land_inventory.resource_inventory_.begin(); r != land_inventory.resource_inventory_.end() && !land_inventory.resource_inventory_.empty(); r++) {

        if (mine_minerals) {
            mine_is_right_type = r->second.type_.isMineralField() && r->second.max_stock_value_ >= 8; // Only gather from "Real" mineral patches with substantive value. Don't mine from obstacles.
        }
        else {
            mine_is_right_type = r->second.type_.isRefinery() && r->second.bwapi_unit_ && IsOwned(r->second.bwapi_unit_);
        }

        if (mine_is_right_type && r->second.number_of_miners_ <= low_drone && r->second.number_of_miners_ < max_drone && current_map_inventory.checkViableGroundPath(r->second.pos_, miner.pos_)) {
            if (r->second.occupied_natural_ && found_low_occupied_mine) { //if it has a closeby base, we want to prioritize those resources first.
                available_fields.addStored_Resource(r->second);
            }
            else {
                long_dist_fields.addStored_Resource(r->second); // if it doesn't have a closeby base, then it is a long distance field and not a priority.
            }
        }
    } //find closest mine meeting this criteria.

    // mine from the closest mine with a base nearby.
    if (!available_fields.resource_inventory_.empty()) {
        attachToNearestMine(available_fields, current_map_inventory, miner);
    } 
    
    if (!miner.isAssignedResource(available_fields) && !long_dist_fields.resource_inventory_.empty()) { // if there are no suitible mineral patches with bases nearby, long-distance mine.
        attachToNearestMine(long_dist_fields, current_map_inventory, miner);
    }

    miner.updateStoredUnit(unit);
} // closure worker mine

//Ataches MINER to nearest mine in RESOURCE INVENTORY. Performs proper incremenation in the overall land_inventory, requires access to overall inventory for maps.
void CUNYAIModule::attachToNearestMine(Resource_Inventory &ri, Map_Inventory &inv, Stored_Unit &miner) {
    Stored_Resource* closest = getClosestGroundStored(ri, current_map_inventory, miner.pos_);
    if (closest /*&& closest->bwapi_unit_ && miner.bwapi_unit_->gather(closest->bwapi_unit_) && checkSafeMineLoc(closest->pos_, ui, inventory)*/) {
        miner.startMine(*closest, land_inventory); // this must update the LAND INVENTORY proper. Otherwise it will update some temperary value, to "availabile Fields".
        if (miner.bwapi_unit_ && miner.isAssignedBuilding(ri)) {
            my_reservation.removeReserveSystem(miner.bwapi_unit_->getBuildType());
        }
    }
    miner.updateStoredUnit(miner.bwapi_unit_);
}

void CUNYAIModule::attachToParticularMine(Stored_Resource &mine, Resource_Inventory &ri, Stored_Unit &miner) {
    miner.startMine(ri.resource_inventory_.find(mine.bwapi_unit_)->second, land_inventory); // go back to your old job.  // Let's not make mistakes by attaching it to "availabile Fields""
    if (miner.bwapi_unit_ && miner.isAssignedBuilding(land_inventory)) {
        my_reservation.removeReserveSystem(miner.bwapi_unit_->getBuildType());
    }
    miner.updateStoredUnit(miner.bwapi_unit_);
}

void CUNYAIModule::attachToParticularMine(Unit &mine, Resource_Inventory &ri, Stored_Unit &miner) {
    miner.startMine(ri.resource_inventory_.find(mine)->second, land_inventory); // Let's not make mistakes by attaching it to "availabile Fields""
    miner.updateStoredUnit(miner.bwapi_unit_);
}

void CUNYAIModule::Worker_Clear( const Unit & unit, Unit_Inventory & ui )
{
    bool already_assigned = false;
    Stored_Unit& miner = ui.unit_inventory_.find(unit)->second;
    //bool building_unit = unit->getLastCommand().getType() == UnitCommandTypes::Morph || unit->getLastCommand().getType() == UnitCommandTypes::Build || unit->getLastCommand().getTargetPosition() == Position(inventory.next_expo_);
    bool building_unit = miner.isAssignedBuilding(land_inventory);
    Resource_Inventory available_fields;

    for (auto& r = land_inventory.resource_inventory_.begin(); r != land_inventory.resource_inventory_.end() && !land_inventory.resource_inventory_.empty(); r++) {
        if ( r->second.max_stock_value_ <= 8 && r->second.number_of_miners_ < 1 && r->second.pos_.isValid() && r->second.type_.isMineralField() && current_map_inventory.checkViableGroundPath(r->second.pos_, miner.pos_) && current_map_inventory.home_base_.getDistance(r->second.pos_) < current_map_inventory.my_portion_of_the_map_ ) {
            available_fields.addStored_Resource(r->second);
        }
    } //find closest mine meeting this criteria.

    if (!available_fields.resource_inventory_.empty()) {
        attachToNearestMine(available_fields, current_map_inventory, miner);
    }
    miner.updateStoredUnit(unit);
}

bool CUNYAIModule::Nearby_Blocking_Minerals(const Unit & unit, Unit_Inventory & ui)
{

    bool already_assigned = false;
    Stored_Unit& miner = ui.unit_inventory_.find(unit)->second;
    Resource_Inventory available_fields;

    for (auto& r = land_inventory.resource_inventory_.begin(); r != land_inventory.resource_inventory_.end() && !land_inventory.resource_inventory_.empty(); r++) {
        if (r->second.max_stock_value_ <= 8 && r->second.number_of_miners_ < 1 && r->second.pos_.isValid() && r->second.type_.isMineralField() && !checkOccupiedArea(enemy_player_model.units_, r->second.pos_, 250) && current_map_inventory.checkViableGroundPath(r->second.pos_, miner.pos_)) {
            return true;
        }
    } //find closest mine meeting this criteria.

    return false;
}

  //Returns True if there is an out for gas. Does not consider all possible gas outlets.
bool CUNYAIModule::Gas_Outlet() {
    bool outlet_avail = false;

    if ( CUNYAIModule::Tech_Avail() && Count_Units( BWAPI::UnitTypes::Zerg_Spawning_Pool) > 0 ) {
        outlet_avail = true;
    }

    bool long_condition = Broodwar->self()->hasUnitTypeRequirement( UnitTypes::Zerg_Hydralisk ) ||
        Broodwar->self()->hasUnitTypeRequirement( UnitTypes::Zerg_Mutalisk ) ||
        Broodwar->self()->hasUnitTypeRequirement( UnitTypes::Zerg_Ultralisk );
    if ( long_condition ) {
        outlet_avail = true;
    } // turns off gas interest when larve are 0.

      //bool long_condition = Count_Units(UnitTypes::Zerg_Hydralisk_Den, friendly_player_model.units_) > 0 ||
      //        Count_Units( UnitTypes::Zerg_Spire, friendly_player_model.units_ ) > 0 ||
      //        Count_Units( UnitTypes::Zerg_Ultralisk_Cavern, friendly_player_model.units_ ) > 0;
      //if ( long_condition ) {
      //    outlet_avail = true;
      //} 

    return outlet_avail;
}

