#pragma once
#include "Source\CUNYAIModule.h"
#include "WorkerManager.h"
#include "Source\Unit_Inventory.h"

using namespace BWAPI;
using namespace Filter;
using namespace std;

bool WorkerManager::isEmptyWorker(const Unit &unit) {
    bool laden_worker = unit->isCarryingGas() || unit->isCarryingMinerals();
    return !laden_worker;
}
bool WorkerManager::workerPrebuild(const Unit & unit)
{
    if (Broodwar->isExplored(CUNYAIModule::current_map_inventory.next_expo_) && unit->build(UnitTypes::Zerg_Hatchery, CUNYAIModule::current_map_inventory.next_expo_) && CUNYAIModule::my_reservation.addReserveSystem(CUNYAIModule::current_map_inventory.next_expo_, UnitTypes::Zerg_Hatchery)) {
        CUNYAIModule::DiagnosticText("Continuing to Expo at ( %d , %d ).", CUNYAIModule::current_map_inventory.next_expo_.x, CUNYAIModule::current_map_inventory.next_expo_.y);
        return CUNYAIModule::updateUnitPhase(unit, Stored_Unit::Phase::Building);
    }
    else if (!Broodwar->isExplored(CUNYAIModule::current_map_inventory.next_expo_) && CUNYAIModule::my_reservation.addReserveSystem(CUNYAIModule::current_map_inventory.next_expo_, UnitTypes::Zerg_Hatchery)) {
        unit->move(Position(CUNYAIModule::current_map_inventory.next_expo_));
        CUNYAIModule::DiagnosticText("Unexplored Expo at ( %d , %d ). Still moving there to check it out.", CUNYAIModule::current_map_inventory.next_expo_.x, CUNYAIModule::current_map_inventory.next_expo_.y);
        return CUNYAIModule::updateUnitPhase(unit, Stored_Unit::Phase::Prebuilding);
    }
    return false;
}

bool WorkerManager::workersCollect(const Unit & unit)
{
    Stored_Unit& miner = *CUNYAIModule::friendly_player_model.units_.getStoredUnit(unit); // we will want DETAILED information about this unit.

    if (excess_gas_capacity_ && CUNYAIModule::gas_starved) {
        if (assignGather(unit, UnitTypes::Zerg_Extractor)) {
            return true;
        }
        else {// assign a worker a mine (gas). Will return null if no viable refinery exists. Might be a bug source.
            return assignGather(unit, UnitTypes::Resource_Mineral_Field); //assign a worker (minerals)
        }
    } // Otherwise, we should put them on minerals.
    else { //if this is your first worker of the frame consider resetting him.
        return assignGather(unit, UnitTypes::Resource_Mineral_Field); // assign a worker a mine (gas). Will return null if no viable refinery exists. Might be a bug source.
    }
    return false;
}

bool WorkerManager::workersClear(const Unit & unit)
{
    Stored_Unit& miner = *CUNYAIModule::friendly_player_model.units_.getStoredUnit(unit); // we will want DETAILED information about this unit.

                                                                                          //Workers need to clear empty patches.
    bool time_to_start_clearing_a_path = CUNYAIModule::current_map_inventory.hatches_ >= 2 && checkBlockingMinerals(unit, CUNYAIModule::friendly_player_model.units_);
    if (time_to_start_clearing_a_path && CUNYAIModule::workermanager.workers_clearing_ == 0 && isEmptyWorker(unit)) {
        if (assignClear(unit)) {
            CUNYAIModule::workermanager.updateWorkersClearing();
            return true;
        }
    } // clear those empty mineral patches that block paths.
    return false;
}

bool WorkerManager::workersReturn(const Unit & unit)
{
    Stored_Unit& miner = *CUNYAIModule::friendly_player_model.units_.getStoredUnit(unit); // we will want DETAILED information about this unit.
    if (miner.bwapi_unit_->returnCargo()) {
        miner.phase_ = Stored_Unit::Returning;
        miner.updateStoredUnit(unit);
        return true;
    }
    return false;
}

//Sends a Worker to gather resources from UNITTYPE (mineral or refinery). letabot has code on this. "AssignEvenSplit(Unit* unit)"
bool WorkerManager::assignGather(const Unit &unit, const UnitType mine) {

    Stored_Unit& miner = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit)->second;
    //bool building_unit = unit->getLastCommand().getType() == UnitCommandTypes::Morph || unit->getLastCommand().getType() == UnitCommandTypes::Build || unit->getLastCommand().getTargetPosition() == Position(inventory.next_expo_);
    bool building_unit = miner.isAssignedBuilding();
    Resource_Inventory available_fields;
    Resource_Inventory long_dist_fields;
    Unit old_mineral_patch = nullptr;
    old_mineral_patch = miner.locked_mine_;
    bool assignment_complete = false;
    miner.stopMine();// will reassign later.

    int low_drone = 0;
    int max_drone = 3;
    bool mine_minerals = mine.isMineralField();
    bool found_low_occupied_mine = false;

    // mineral patches can handle up to 2 miners, gas/refineries can handle up to 3.
    if (mine_minerals) {
        low_drone = 1;
        max_drone = 2;
    }
    else {
        low_drone = 2; // note : Does not count worker IN extractor.
        max_drone = 3;
    }


    // scrape over every resource to determine the lowest number of miners.
    for (auto r : CUNYAIModule::land_inventory.resource_inventory_) {
        bool mine_is_right_type = false;
        if (mine_minerals) {
            mine_is_right_type = r.second.type_.isMineralField() && !r.second.blocking_mineral_; // Only gather from "Real" mineral patches with substantive value. Don't mine from obstacles.
        }
        else {
            mine_is_right_type = r.second.type_.isRefinery() && r.second.bwapi_unit_ && IsOwned(r.second.bwapi_unit_);
        }

        if (mine_is_right_type && r.second.pos_.isValid() && r.second.number_of_miners_ <= low_drone && r.second.number_of_miners_ < max_drone && r.second.occupied_resource_ && CUNYAIModule::current_map_inventory.checkViableGroundPath(r.second.pos_, miner.pos_)) { //occupied natural -> resource is close to a base
            low_drone = r.second.number_of_miners_;
            found_low_occupied_mine = true;
        }

    } // find drone minima.

      //    CUNYAIModule::DiagnosticText("LOW DRONE COUNT : %d", low_drone);
      //    CUNYAIModule::DiagnosticText("Mine Minerals : %d", mine_minerals);

    for (auto r : CUNYAIModule::land_inventory.resource_inventory_) {
        bool mine_is_right_type = false;
        if (mine_minerals) {
            mine_is_right_type = r.second.type_.isMineralField() && !r.second.blocking_mineral_; // Only gather from "Real" mineral patches with substantive value. Don't mine from obstacles.
        }
        else {
            mine_is_right_type = r.second.type_.isRefinery() && r.second.bwapi_unit_ && IsOwned(r.second.bwapi_unit_);
        }

        if (mine_is_right_type && r.second.number_of_miners_ <= low_drone && r.second.number_of_miners_ < max_drone &&  CUNYAIModule::current_map_inventory.checkViableGroundPath(r.second.pos_, miner.pos_)) {
            long_dist_fields.addStored_Resource(r.second); // if it doesn't have a closeby base, then it is a long distance field and not a priority.
            if (r.second.occupied_resource_ && found_low_occupied_mine) { //if it has a closeby base, we want to prioritize those resources first.
                available_fields.addStored_Resource(r.second);
            }
        }
    } //find closest mine meeting this criteria.

      // mine from the closest mine with a base nearby.
    if (!available_fields.resource_inventory_.empty()) {
        assignment_complete = attachToNearestMine(available_fields, CUNYAIModule::current_map_inventory, miner); // phase is already updated.
    }
    else if (!long_dist_fields.resource_inventory_.empty()) { // if there are no suitible mineral patches with bases nearby, long-distance mine.
        assignment_complete = attachToNearestMine(long_dist_fields, CUNYAIModule::current_map_inventory, miner); // phase is already updated.
    }

    if (!assignment_complete && old_mineral_patch) {
        miner.startMine(old_mineral_patch);
        miner.updateStoredUnit(unit);
    }
    return assignment_complete;
} // closure worker mine

  //Attaches MINER to nearest mine in RESOURCE INVENTORY. Performs proper incremenation in the overall land_inventory, requires access to overall inventory for maps.
bool WorkerManager::attachToNearestMine(Resource_Inventory &ri, Map_Inventory &inv, Stored_Unit &miner) {
    Stored_Resource* closest = CUNYAIModule::getClosestGroundStored(ri, miner.pos_);
    if (closest) {
        miner.startMine(*closest); // this must update the LAND INVENTORY proper. Otherwise it will update some temperary value, to "availabile Fields".
        if (miner.bwapi_unit_ && miner.isAssignedBuilding()) {
            CUNYAIModule::my_reservation.removeReserveSystem(TilePosition(miner.bwapi_unit_->getOrderTargetPosition()), miner.bwapi_unit_->getBuildType(), true);
        }
        miner.phase_ = closest->type_.isNeutral() ? Stored_Unit::MiningMin : Stored_Unit::MiningGas; // is it gas or mineral?
        miner.phase_ = !closest->occupied_resource_ ? Stored_Unit::DistanceMining : miner.phase_; // is it distance?
        miner.updateStoredUnit(miner.bwapi_unit_);
        return true;
    }
    return false;
}

void WorkerManager::attachToParticularMine(Stored_Resource &mine, Resource_Inventory &ri, Stored_Unit &miner) {
    miner.startMine(ri.resource_inventory_.find(mine.bwapi_unit_)->second); // go back to your old job.  // Let's not make mistakes by attaching it to "availabile Fields""
    if (miner.bwapi_unit_ && miner.isAssignedBuilding()) {
        CUNYAIModule::my_reservation.removeReserveSystem(TilePosition(miner.bwapi_unit_->getOrderTargetPosition()), miner.bwapi_unit_->getBuildType(), true);
    }
    miner.phase_ = mine.type_.isNeutral() ? Stored_Unit::MiningMin : Stored_Unit::MiningGas;
    miner.updateStoredUnit(miner.bwapi_unit_);
}

void WorkerManager::attachToParticularMine(Unit &mine, Resource_Inventory &ri, Stored_Unit &miner) {
    miner.startMine(ri.resource_inventory_.find(mine)->second); // Let's not make mistakes by attaching it to "availabile Fields""
    miner.phase_ = mine->getType().isNeutral() ? Stored_Unit::MiningMin : Stored_Unit::MiningGas;
    miner.updateStoredUnit(miner.bwapi_unit_);
}

bool WorkerManager::assignClear(const Unit & unit)
{
    bool already_assigned = false;
    Stored_Unit& miner = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit)->second;
    bool assignment_worked = false;
    bool building_unit = miner.isAssignedBuilding();
    Resource_Inventory available_fields;

    for (auto& r = CUNYAIModule::land_inventory.resource_inventory_.begin(); r != CUNYAIModule::land_inventory.resource_inventory_.end() && !CUNYAIModule::land_inventory.resource_inventory_.empty(); r++) {
        if (r->second.blocking_mineral_ && r->second.number_of_miners_ < 1 && r->second.pos_.isValid() && r->second.type_.isMineralField() && CUNYAIModule::current_map_inventory.checkViableGroundPath(r->second.pos_, miner.pos_) && CUNYAIModule::current_map_inventory.home_base_.getDistance(r->second.pos_) <  CUNYAIModule::current_map_inventory.my_portion_of_the_map_) {
            available_fields.addStored_Resource(r->second);
        }
    } //find closest mine meeting this criteria.

    if (!available_fields.resource_inventory_.empty()) {
        assignment_worked = attachToNearestMine(available_fields, CUNYAIModule::current_map_inventory, miner);
        if (assignment_worked) {
            miner.phase_ = Stored_Unit::Clearing; //oof we have to manually edit the command to clear, it's a rare case.
            miner.updateStoredUnit(miner.bwapi_unit_);
        }
    }
    return assignment_worked;
}

bool WorkerManager::checkBlockingMinerals(const Unit & unit, Unit_Inventory & ui)
{
    bool already_assigned = false;
    Stored_Unit& miner = ui.unit_map_.find(unit)->second;
    Resource_Inventory available_fields;

    for (auto& r = CUNYAIModule::land_inventory.resource_inventory_.begin(); r != CUNYAIModule::land_inventory.resource_inventory_.end() && !CUNYAIModule::land_inventory.resource_inventory_.empty(); r++) {
        if (r->second.blocking_mineral_ && r->second.number_of_miners_ < 1 && r->second.pos_.isValid() && r->second.type_.isMineralField() && !CUNYAIModule::checkOccupiedArea(CUNYAIModule::enemy_player_model.units_, r->second.pos_) && CUNYAIModule::current_map_inventory.checkViableGroundPath(r->second.pos_, miner.pos_)) {
            return true;
        }
    } //find closest mine meeting this criteria.

    return false;
}

//Returns True if there is an out for gas. Does not consider all possible gas outlets.
bool WorkerManager::checkGasOutlet() {
    bool outlet_avail = false;

    if (CUNYAIModule::techmanager.checkTechAvail() && CUNYAIModule::Count_Units(BWAPI::UnitTypes::Zerg_Spawning_Pool) > 0) {
        outlet_avail = true;
    }

    bool long_condition = Broodwar->self()->hasUnitTypeRequirement(UnitTypes::Zerg_Hydralisk) ||
        Broodwar->self()->hasUnitTypeRequirement(UnitTypes::Zerg_Mutalisk) ||
        Broodwar->self()->hasUnitTypeRequirement(UnitTypes::Zerg_Ultralisk);

    if (long_condition) {
        outlet_avail = true;
    } // turns off gas interest when larve are 0.

    if (!CUNYAIModule::buildorder.building_gene_.empty()) {
        UnitType unit = CUNYAIModule::buildorder.building_gene_.front().getUnit();
        TechType tech = CUNYAIModule::buildorder.building_gene_.front().getResearch();
        UpgradeType up = CUNYAIModule::buildorder.building_gene_.front().getUpgrade();

        // Don't write this way:
        if (unit != UnitTypes::None) if (unit.gasPrice() > 0) outlet_avail = true;
        if (tech != TechTypes::None) if (tech.gasPrice() > 0) outlet_avail = true;
        if (up != UpgradeTypes::None) if (up.gasPrice() > 0) outlet_avail = true;
    }


    return outlet_avail;
}

bool WorkerManager::workerWork(const Unit &u) {

    Stored_Unit& miner = *CUNYAIModule::friendly_player_model.units_.getStoredUnit(u); // we will want DETAILED information about this unit.
    int t_game = Broodwar->getFrameCount();

    bool too_much_gas = CUNYAIModule::current_map_inventory.getGasRatio() > CUNYAIModule::delta;
    bool no_recent_worker_alteration = miner.time_of_last_purge_ < t_game - 12 && miner.time_since_last_command_ > 12;

    // Identify old mineral task. If there's no new better job, put them back on this without disturbing them.
    bool was_gas = miner.isAssignedGas();
    bool was_mineral = miner.isAssignedMining();
    bool was_long_mine = miner.isLongRangeLock();
    Unit old_mineral_patch = nullptr;
    if ((was_mineral || was_gas) && !was_long_mine) {
        old_mineral_patch = miner.locked_mine_;
    }
    bool task_guard = false;

    bool build_check_this_frame_ = (t_game % 24 == 0);

    //if (CUNYAIModule::spamGuard(miner.bwapi_unit_)) { // careful about interactions between spam guards.
    //Do not disturb fighting workers or workers assigned to clear a position. Do not spam. Allow them to remain locked on their task.
    switch (miner.phase_)
    {
    case Stored_Unit::MiningGas:
        if (!isEmptyWorker(u) && u->isVisible()) { // If he's not in the refinery, auto return.
            task_guard = workersReturn(u);
        }
        else if (u->isVisible() && CUNYAIModule::spamGuard(u, 14) && miner.bwapi_unit_->gather(miner.locked_mine_)) { // If he's not in the refinery, reassign him back to work.
            miner.updateStoredUnit(u);
            task_guard = true;
        }
        break;
    case Stored_Unit::MiningMin: // does the same as...
    case Stored_Unit::Clearing: // does the same as...
        if (!isEmptyWorker(u)) { //auto return if needed.
            task_guard = workersReturn(u);
        }
        else if ((miner.isBrokenLock() && CUNYAIModule::spamGuard(u, 14)) || u->isIdle()) { //5 frame pause needed on gamestart or else the workers derp out. Can't go to 3.
            if (miner.bwapi_unit_->gather(miner.locked_mine_)) { // reassign him back to work.
                task_guard = true;
                miner.updateStoredUnit(u);
            }
        }
        break;
    case Stored_Unit::DistanceMining: // does the same as...
        if (!isEmptyWorker(u)) { //auto return if needed.
            task_guard = workersReturn(u); // mark worker as returning.
        }
        else if (!miner.getMine()->bwapi_unit_ && CUNYAIModule::spamGuard(u, 14)) { // Otherwise walk to that mineral.
            if (miner.bwapi_unit_->move(miner.getMine()->pos_)) { // reassign him back to work.
                miner.updateStoredUnit(u);
                task_guard = true;
            }
        }
        else if (miner.getMine()->bwapi_unit_ && miner.isBrokenLock() && (CUNYAIModule::spamGuard(u, 14) || u->isIdle())) { //If there is a mineral and we can see it, mine it.
            if (miner.bwapi_unit_->gather(miner.locked_mine_)) { // reassign him back to work.
                miner.updateStoredUnit(u);
                task_guard = true;
            }
        }
        break;
    case Stored_Unit::Prebuilding:
        task_guard = workerPrebuild(u); // may need to move from prebuild to "build".
        break;
    case Stored_Unit::Returning: // this command is very complex. Only consider reassigning if reassignment is NEEDED. Otherwise reassign to locked mine (every 14 frames) and move to the proper phase.
        if (isEmptyWorker(u)) {
            if (miner.locked_mine_) {
                if (miner.locked_mine_->getType().isMineralField() && excess_gas_capacity_ && CUNYAIModule::gas_starved) {
                    workersCollect(u);
                }
                else if (miner.locked_mine_->getType().isRefinery() && !CUNYAIModule::gas_starved) {
                    workersCollect(u);
                }
                else if ((miner.locked_mine_->getType().isMineralField() && !excess_gas_capacity_) || (miner.locked_mine_->getType().isRefinery() && CUNYAIModule::gas_starved)) {
                    task_guard = workersClear(u) || (!build_check_this_frame_ && CUNYAIModule::assemblymanager.buildBuilding(u));
                    if (!task_guard && CUNYAIModule::spamGuard(u, 14)) {
                        if (miner.locked_mine_->getType().isRefinery()) {
                            task_guard = CUNYAIModule::updateUnitPhase(u, Stored_Unit::Phase::MiningGas);
                        }
                        else if (miner.locked_mine_->getType().isMineralField()) {
                            task_guard = CUNYAIModule::updateUnitPhase(u, Stored_Unit::Phase::MiningMin);
                        }
                    }
                }
            }
        }
        break;
    case Stored_Unit::None:
        task_guard = workersClear(u) || (!build_check_this_frame_ && CUNYAIModule::assemblymanager.buildBuilding(u)) || workersCollect(u);
        break;
    default:
        task_guard = workersCollect(u);
        break;
    }

    if (task_guard) return true;
    else return false;
    //}
};

// Updates the count of our gas workers.
void WorkerManager::updateGas_Workers() {
    // Get worker tallies.
    int gas_workers = 0;

    Unitset myUnits = Broodwar->self()->getUnits(); // out of my units  Probably easier than searching the map for them.
    if (!myUnits.empty()) { // make sure this object is valid!
        for (auto u = myUnits.begin(); u != myUnits.end() && !myUnits.empty(); ++u)
        {
            if ((*u) && (*u)->exists()) {
                if ((*u)->getType().isWorker()) {
                    if ((*u)->isGatheringGas() || (*u)->isCarryingGas()) // implies exists and isCompleted
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
void WorkerManager::updateMin_Workers() {
    // Get worker tallies.
    int min_workers = 0;

    Unitset myUnits = Broodwar->self()->getUnits(); // out of my units  Probably easier than searching the map for them.
    if (!myUnits.empty()) { // make sure this object is valid!
        for (auto u = myUnits.begin(); u != myUnits.end() && !myUnits.empty(); ++u)
        {
            if ((*u) && (*u)->exists()) {
                if ((*u)->getType().isWorker()) {
                    if ((*u)->isGatheringMinerals() || (*u)->isCarryingMinerals()) // implies exists and isCompleted
                    {
                        ++min_workers;
                    }
                } // closure: Only investigate closely if they are drones.
            } // Closure: only investigate on existance of unit..
        } // closure: count all workers
    }

    min_workers_ = min_workers;
}

void WorkerManager::updateWorkersClearing()
{
    int clearing_workers_found = 0;

    if (!CUNYAIModule::friendly_player_model.units_.unit_map_.empty()) {
        for (auto & w = CUNYAIModule::friendly_player_model.units_.unit_map_.begin(); w != CUNYAIModule::friendly_player_model.units_.unit_map_.end() && !CUNYAIModule::friendly_player_model.units_.unit_map_.empty(); w++) {
            if (w->second.isAssignedClearing()) {
                clearing_workers_found++;
            }
        }
    }
    workers_clearing_ = clearing_workers_found;
}

void WorkerManager::updateWorkersLongDistanceMining()
{
    int long_distance_miners_found = 0;

    if (!CUNYAIModule::friendly_player_model.units_.unit_map_.empty()) {
        for (auto & w = CUNYAIModule::friendly_player_model.units_.unit_map_.begin(); w != CUNYAIModule::friendly_player_model.units_.unit_map_.end() && !CUNYAIModule::friendly_player_model.units_.unit_map_.empty(); w++) {
            if (w->second.isAssignedLongDistanceMining()) {
                long_distance_miners_found++;
            }
        }
    }
    workers_distance_mining_ = long_distance_miners_found;
}

void WorkerManager::updateExcessCapacity()
{
    //excess_gas_capacity_ = CUNYAIModule::land_inventory.local_gas_collectors_ <= CUNYAIModule::land_inventory.local_refineries_ * 2 && CUNYAIModule::land_inventory.local_refineries_ > 0 && CUNYAIModule::gas_starved;
    excess_gas_capacity_ = (gas_workers_ <= CUNYAIModule::land_inventory.getLocalRefineries() * 2) && CUNYAIModule::land_inventory.getLocalRefineries() > 0;

}
