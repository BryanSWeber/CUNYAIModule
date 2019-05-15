#pragma once
#include "Source\CUNYAIModule.h"

using namespace BWAPI;
using namespace Filter;
using namespace std;

  //Sends a Worker to gather resources from UNITTYPE (mineral or refinery). letabot has code on this. "AssignEvenSplit(Unit* unit)"
void CUNYAIModule::Worker_Gather(const Unit &unit, const UnitType mine, Unit_Inventory &ui) {

    bool already_assigned = false;
    Stored_Unit& miner = ui.unit_map_.find(unit)->second;
    //bool building_unit = unit->getLastCommand().getType() == UnitCommandTypes::Morph || unit->getLastCommand().getType() == UnitCommandTypes::Build || unit->getLastCommand().getTargetPosition() == Position(inventory.next_expo_);
    bool building_unit = miner.isAssignedBuilding();
    Resource_Inventory available_fields;
    Resource_Inventory long_dist_fields;

    int low_drone = 0;
    int max_drone = 3;
    bool mine_minerals = mine.isMineralField();
    bool mine_is_right_type = true;
    bool found_low_occupied_mine = false;

    // mineral patches can handle up to 2 miners, gas/refineries can handle up to 3.
    if ( mine_minerals ) {
        low_drone = 1;
        max_drone = 2;
    } else {
        low_drone = 2; // note : Does not count worker IN extractor.
        max_drone = 3;
    }


    // scrape over every resource to determine the lowest number of miners.
    for (auto r : land_inventory.resource_inventory_) {

        if (mine_minerals) {
            mine_is_right_type = r.second.type_.isMineralField() && r.second.max_stock_value_ >= 8; // Only gather from "Real" mineral patches with substantive value. Don't mine from obstacles.
        }
        else {
            mine_is_right_type = r.second.type_.isRefinery() && r.second.bwapi_unit_ && IsOwned(r.second.bwapi_unit_);
        }

        if ( mine_is_right_type && r.second.pos_.isValid() && r.second.number_of_miners_ <= low_drone && r.second.number_of_miners_ < max_drone && r.second.occupied_natural_ && current_map_inventory.checkViableGroundPath(r.second.pos_, miner.pos_)) { //occupied natural -> resource is close to a base
            low_drone = r.second.number_of_miners_;
            found_low_occupied_mine = true;
        }

    } // find drone minima.

    //    CUNYAIModule::DiagnosticText("LOW DRONE COUNT : %d", low_drone);
    //    CUNYAIModule::DiagnosticText("Mine Minerals : %d", mine_minerals);


    for (auto r : land_inventory.resource_inventory_) {

        if (mine_minerals) {
            mine_is_right_type = r.second.type_.isMineralField() && r.second.max_stock_value_ >= 8; // Only gather from "Real" mineral patches with substantive value. Don't mine from obstacles.
        }
        else {
            mine_is_right_type = r.second.type_.isRefinery() && r.second.bwapi_unit_ && IsOwned(r.second.bwapi_unit_);
        }

        if (mine_is_right_type && r.second.number_of_miners_ <= low_drone && r.second.number_of_miners_ < max_drone && current_map_inventory.checkViableGroundPath(r.second.pos_, miner.pos_) ) {
            long_dist_fields.addStored_Resource(r.second); // if it doesn't have a closeby base, then it is a long distance field and not a priority.

            if (r.second.occupied_natural_ && found_low_occupied_mine) { //if it has a closeby base, we want to prioritize those resources first.
                available_fields.addStored_Resource(r.second);
            }
        }
    } //find closest mine meeting this criteria.

    // mine from the closest mine with a base nearby.
    if (!available_fields.resource_inventory_.empty()) {
        attachToNearestMine(available_fields, current_map_inventory, miner);
        miner.phase_ = Stored_Unit::Mining;
    } 
    else if (!long_dist_fields.resource_inventory_.empty()) { // if there are no suitible mineral patches with bases nearby, long-distance mine.
        attachToNearestMine(long_dist_fields, current_map_inventory, miner);
        miner.phase_ = Stored_Unit::DistanceMining;
    }
    
    miner.updateStoredUnit(unit);
} // closure worker mine

//Ataches MINER to nearest mine in RESOURCE INVENTORY. Performs proper incremenation in the overall land_inventory, requires access to overall inventory for maps.
void CUNYAIModule::attachToNearestMine(Resource_Inventory &ri, Map_Inventory &inv, Stored_Unit &miner) {
    Stored_Resource* closest = getClosestGroundStored(ri, miner.pos_);
    if (closest /*&& closest->bwapi_unit_ && miner.bwapi_unit_->gather(closest->bwapi_unit_) && checkSafeMineLoc(closest->pos_, ui, inventory)*/) {
        miner.startMine(*closest); // this must update the LAND INVENTORY proper. Otherwise it will update some temperary value, to "availabile Fields".
        if (miner.bwapi_unit_ && miner.isAssignedBuilding()) {
            my_reservation.removeReserveSystem(TilePosition(miner.bwapi_unit_->getOrderTargetPosition()), miner.bwapi_unit_->getBuildType(), true);
        }
        miner.phase_ = Stored_Unit::Mining;
    }
    miner.updateStoredUnit(miner.bwapi_unit_);
}

void CUNYAIModule::attachToParticularMine(Stored_Resource &mine, Resource_Inventory &ri, Stored_Unit &miner) {
    miner.startMine(ri.resource_inventory_.find(mine.bwapi_unit_)->second); // go back to your old job.  // Let's not make mistakes by attaching it to "availabile Fields""
    if (miner.bwapi_unit_ && miner.isAssignedBuilding()) {
        my_reservation.removeReserveSystem(TilePosition(miner.bwapi_unit_->getOrderTargetPosition()), miner.bwapi_unit_->getBuildType(), true);
    }
    miner.phase_ = Stored_Unit::Mining;
    miner.updateStoredUnit(miner.bwapi_unit_);
}

void CUNYAIModule::attachToParticularMine(Unit &mine, Resource_Inventory &ri, Stored_Unit &miner) {
    miner.startMine(ri.resource_inventory_.find(mine)->second); // Let's not make mistakes by attaching it to "availabile Fields""
    miner.phase_ = Stored_Unit::Mining;
    miner.updateStoredUnit(miner.bwapi_unit_);
}

void CUNYAIModule::Worker_Clear( const Unit & unit, Unit_Inventory & ui )
{
    bool already_assigned = false;
    Stored_Unit& miner = ui.unit_map_.find(unit)->second;
    //bool building_unit = unit->getLastCommand().getType() == UnitCommandTypes::Morph || unit->getLastCommand().getType() == UnitCommandTypes::Build || unit->getLastCommand().getTargetPosition() == Position(inventory.next_expo_);
    bool building_unit = miner.isAssignedBuilding();
    Resource_Inventory available_fields;

    for (auto& r = land_inventory.resource_inventory_.begin(); r != land_inventory.resource_inventory_.end() && !land_inventory.resource_inventory_.empty(); r++) {
        if ( r->second.max_stock_value_ <= 8 && r->second.number_of_miners_ < 1 && r->second.pos_.isValid() && r->second.type_.isMineralField() && current_map_inventory.checkViableGroundPath(r->second.pos_, miner.pos_) && current_map_inventory.home_base_.getDistance(r->second.pos_) < current_map_inventory.my_portion_of_the_map_ ) {
            available_fields.addStored_Resource(r->second);
        }
    } //find closest mine meeting this criteria.

    if (!available_fields.resource_inventory_.empty()) {
        attachToNearestMine(available_fields, current_map_inventory, miner);
        miner.phase_ = Stored_Unit::Clearing;
    }
    miner.updateStoredUnit(unit);
}

bool CUNYAIModule::Nearby_Blocking_Minerals(const Unit & unit, Unit_Inventory & ui)
{

    bool already_assigned = false;
    Stored_Unit& miner = ui.unit_map_.find(unit)->second;
    Resource_Inventory available_fields;

    for (auto& r = land_inventory.resource_inventory_.begin(); r != land_inventory.resource_inventory_.end() && !land_inventory.resource_inventory_.empty(); r++) {
        if (r->second.max_stock_value_ <= 8 && r->second.number_of_miners_ < 1 && r->second.pos_.isValid() && r->second.type_.isMineralField() && !checkOccupiedArea(enemy_player_model.units_, r->second.pos_) && current_map_inventory.checkViableGroundPath(r->second.pos_, miner.pos_)) {
            return true;
        }
    } //find closest mine meeting this criteria.

    return false;
}

  //Returns True if there is an out for gas. Does not consider all possible gas outlets.
bool CUNYAIModule::Gas_Outlet() {
    bool outlet_avail = false;

    if ( techmanager.checkTechAvail() && Count_Units( BWAPI::UnitTypes::Zerg_Spawning_Pool) > 0 ) {
        outlet_avail = true;
    }

    bool long_condition = Broodwar->self()->hasUnitTypeRequirement( UnitTypes::Zerg_Hydralisk ) ||
        Broodwar->self()->hasUnitTypeRequirement( UnitTypes::Zerg_Mutalisk ) ||
        Broodwar->self()->hasUnitTypeRequirement( UnitTypes::Zerg_Ultralisk );

    if ( long_condition ) {
        outlet_avail = true;
    } // turns off gas interest when larve are 0.

    if (!buildorder.building_gene_.empty()) {
        UnitType unit = buildorder.building_gene_.front().getUnit();
        TechType tech = buildorder.building_gene_.front().getResearch();
        UpgradeType up = buildorder.building_gene_.front().getUpgrade();

        // Don't write this way:
        if (unit != UnitTypes::None) if (unit.gasPrice() > 0) outlet_avail = true;
        if (tech != TechTypes::None) if (tech.gasPrice() > 0) outlet_avail = true;
        if (up != UpgradeTypes::None) if (up.gasPrice() > 0) outlet_avail = true;

    }


    return outlet_avail;
}

