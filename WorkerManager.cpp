#pragma once
#include "Source\CUNYAIModule.h"
#include "Source\WorkerManager.h"
#include "Source\UnitInventory.h"
#include "Source\MobilityManager.h"
#include "Source\AssemblyManager.h"
#include "Source/Diagnostics.h"

using namespace BWAPI;
using namespace Filter;
using namespace std;

bool WorkerManager::isEmptyWorker(const Unit &unit) {
    bool laden_worker = unit->isCarryingGas() || unit->isCarryingMinerals();
    return !laden_worker;
}
bool WorkerManager::workerPrebuild(const Unit & unit)
{
    bool has_path = false;
    StoredUnit& miner = *CUNYAIModule::friendly_player_model.units_.getStoredUnit(unit); // we will want DETAILED information about this unit.
    AssemblyManager::clearBuildingObstuctions(miner.intended_build_type_, miner.intended_build_tile_, unit);

    if (CUNYAIModule::my_reservation.addReserveSystem(miner.intended_build_tile_, miner.intended_build_type_)) // get it in the build system if it is not already there.
        Diagnostics::DiagnosticText("We seem to be overzealous with keeping our reserve system clean, sir!");

    BWEB::Path newPath;
    newPath.createUnitPath(miner.pos_, Position(miner.intended_build_tile_));
    has_path = newPath.isReachable() && !newPath.getTiles().empty();

    //If no JPS path, try CPP paths:
    int plength = 0;
    if (!has_path) {
        auto cpp = BWEM::Map::Instance().GetPath(miner.pos_, Position(miner.intended_build_tile_), &plength);
        has_path = has_path || plength > 0;
    }

    //if we can build it with an offical build order, and it is in the reserve system, do so now.
    if (AssemblyManager::isFullyVisibleBuildLocation(miner.intended_build_type_, miner.intended_build_tile_) && unit->build(miner.intended_build_type_, miner.intended_build_tile_)) {
        Diagnostics::DiagnosticText("Continuing to Build at ( %d , %d ).", miner.intended_build_tile_.x, miner.intended_build_tile_.y);
        return CUNYAIModule::updateUnitPhase(unit, StoredUnit::Building);
    }
    // if it is not capable of an official build order right now, but it is in the reserve system, send it to the end destination.
    else if(has_path && AssemblyManager::isPlaceableCUNY(miner.intended_build_type_, miner.intended_build_tile_)) {
        Mobility(unit).moveTo(unit->getPosition(), Position(miner.intended_build_tile_) + Position(16,16), StoredUnit::Phase::Prebuilding);
        Diagnostics::DiagnosticText("Unexplored Location at ( %d , %d ). Still moving there to check it out.", miner.intended_build_tile_.x, miner.intended_build_tile_.y);
        return CUNYAIModule::updateUnitBuildIntent(unit, miner.intended_build_type_, miner.intended_build_tile_);
    }
    else {
        CUNYAIModule::my_reservation.removeReserveSystem(miner.intended_build_tile_, miner.intended_build_type_, true);
        CUNYAIModule::updateUnitPhase(unit, StoredUnit::None);
    }

    return false;
}

bool WorkerManager::workersCollect(const Unit & unit, int max_dist = 500)
{
    StoredUnit& miner = *CUNYAIModule::friendly_player_model.units_.getStoredUnit(unit); // we will want DETAILED information about this unit.

    if (excess_gas_capacity_ && CUNYAIModule::gas_starved) {
        if (assignGather(unit, UnitTypes::Zerg_Extractor, max_dist)) {
            return true;
        }
        else {// assign a worker a mine (gas). Will return null if no viable refinery exists. Might be a bug source.
            return assignGather(unit, UnitTypes::Resource_Mineral_Field, max_dist); //assign a worker (minerals)
        }
    } // Otherwise, we should put them on minerals.
    else { //if this is your first worker of the frame consider resetting him.
        return assignGather(unit, UnitTypes::Resource_Mineral_Field, max_dist); // assign a worker a mine (gas). Will return null if no viable refinery exists. Might be a bug source.
    }

    return false;
}

bool WorkerManager::workersClear(const Unit & unit)
{
    StoredUnit& miner = *CUNYAIModule::friendly_player_model.units_.getStoredUnit(unit); // we will want DETAILED information about this unit.

     //Workers need to clear empty patches.
    bool time_to_start_clearing_a_path = CUNYAIModule::basemanager.getBaseCount() >= 2 && checkBlockingMinerals(unit, CUNYAIModule::friendly_player_model.units_);
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
    StoredUnit& miner = *CUNYAIModule::friendly_player_model.units_.getStoredUnit(unit); // we will want DETAILED information about this unit.
    if (miner.bwapi_unit_->returnCargo()) {
        miner.phase_ = StoredUnit::Returning;
        miner.updateStoredUnit(unit);
        return true;
    }
    return false;
}

//Sends a Worker to gather resources from UNITTYPE (mineral or refinery). letabot has code on this. "AssignEvenSplit(Unit* unit)"
bool WorkerManager::assignGather(const Unit &unit, const UnitType mine, const int max_dist = 500) {

    StoredUnit& miner = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit)->second;

    bool building_unit = miner.isAssignedBuilding();
    Resource_Inventory safe_fields;
    Resource_Inventory escape_fields;
    Resource_Inventory desperation_fields;

    Resource_Inventory local_fields;
    Resource_Inventory long_dist_fields;
    Resource_Inventory overmining_fields;

    Unit old_mineral_patch = nullptr;
    old_mineral_patch = miner.locked_mine_;
    bool assignment_complete = false;
    miner.stopMine();// will reassign later.
    Mobility drone_pathing_options = Mobility(unit);
    int max_drone = 3;
    int safe_resource_min = INT_MAX;
    int escape_resource_min = INT_MAX;

    bool mine_minerals = mine.isMineralField();
    bool found_low_occupied_mine = false;

    // mineral patches can handle up to 2 miners, gas/refineries can handle up to 3.
    if (mine_minerals)
        max_drone = 2;
    else
        max_drone = 3;

    bool there_exists_a_safe_mine = false;
    bool there_exists_an_escape_mine = false;
    // scrape over every resource to determine the lowest number of miners, and identify if there are any safe mines for this worker.
    for (auto r : CUNYAIModule::land_inventory.resource_inventory_) {
        bool mine_is_right_type = false;
        bool potential_escape = drone_pathing_options.checkSafeEscapePath(r.second.pos_); 
        bool safe = drone_pathing_options.checkSafeGroundPath(unit->getPosition()) && potential_escape; // If there's no escape, it is not safe.

        bool mine_is_unoccupied_by_enemy = CUNYAIModule::enemy_player_model.units_.getBuildingInventoryAtArea(r.second.areaID_).unit_map_.empty();
        bool mine_is_occupied = CUNYAIModule::basemanager.getClosestBaseGround(r.second.pos_).r_loc_.resource_inventory_.count(r.first) > 0;

        bool path_exists = CUNYAIModule::currentMapInventory.checkViableGroundPath(r.second.pos_, miner.pos_);

        if (mine_minerals) {
            int plength = 0;
            bool unit_sent = false;
            auto cpp = BWEM::Map::Instance().GetPath(miner.pos_, r.second.pos_, &plength);
            mine_is_right_type = r.second.type_.isMineralField() && !r.second.blocking_mineral_ && mine_is_unoccupied_by_enemy && path_exists && r.second.pos_.isValid() && (plength < max_dist || !old_mineral_patch); // Only gather from "Real" mineral patches with substantive value. Don't mine from obstacles. Don't hike so far.
        }
        else { // gas can never be more than 3x, ever.
            int plength = 0;
            bool unit_sent = false;
            auto cpp = BWEM::Map::Instance().GetPath(miner.pos_, r.second.pos_, &plength);
            mine_is_right_type = r.second.type_.isRefinery() && r.second.bwapi_unit_ && IsOwned(r.second.bwapi_unit_) && mine_is_unoccupied_by_enemy && path_exists && r.second.pos_.isValid() && r.second.number_of_miners_ < max_drone && (plength < max_dist || !old_mineral_patch);// never hike to a gas location.
        }

        if (mine_is_right_type) {
            if (safe && mine_is_occupied) {
                there_exists_a_safe_mine = safe || there_exists_a_safe_mine;
                safe_fields.addStored_Resource(r.second);
                safe_resource_min = max(min(safe_resource_min, r.second.number_of_miners_), 0);
            }
            if (potential_escape && mine_is_occupied) {
                there_exists_an_escape_mine = potential_escape || there_exists_an_escape_mine;
                escape_fields.addStored_Resource(r.second);
                escape_resource_min = max(min(escape_resource_min, r.second.number_of_miners_), 0);
            }
            desperation_fields.addStored_Resource(r.second);
        }

    } // find drone minima, and mark if there are ANY safe mines for this worker, regardless of type.

    //We have to go over each type otherwise we may accidentially enter a mode without qualifiers.
    if (there_exists_a_safe_mine) {
        for (auto r : safe_fields.resource_inventory_) {
            if (r.second.number_of_miners_ < max_drone && r.second.number_of_miners_ == safe_resource_min) {
                long_dist_fields.addStored_Resource(r.second); // if it doesn't have a closeby base, then it is a long distance field and not a priority.
                if (r.second.occupied_resource_)
                    local_fields.addStored_Resource(r.second);
            }
        }
    }
    
    if (there_exists_an_escape_mine) {
        for (auto r : escape_fields.resource_inventory_) {
            if (r.second.number_of_miners_ < max_drone && r.second.number_of_miners_ == escape_resource_min) {
                long_dist_fields.addStored_Resource(r.second); // if it doesn't have a closeby base, then it is a long distance field and not a priority.
                if (r.second.occupied_resource_)
                    local_fields.addStored_Resource(r.second);
            }
        }
    }

    for (auto r : desperation_fields.resource_inventory_) {
        if (r.second.occupied_resource_) // if the area has workers and is not occupied, then it is now a desperate choice. Just go anywhere occupied.
            if (r.second.number_of_miners_ >= 0)
                overmining_fields.addStored_Resource(r.second); //
    }

    // mine from the closest mine with a base nearby.
    if (!local_fields.resource_inventory_.empty())
        assignment_complete = attachToNearestMine(local_fields, miner); // phase is updated here.
    else if (!long_dist_fields.resource_inventory_.empty()) // if there are no suitible mineral patches with bases nearby, long-distance mine.
        assignment_complete = attachToNearestMine(long_dist_fields, miner); // phase is updated here.
    else if (!overmining_fields.resource_inventory_.empty()) { // if you are still in trouble just... mine *something*.
        assignment_complete = attachToNearestMine(overmining_fields, miner); // phase is updated here.
        Diagnostics::DiagnosticText("I cannot find a safe place to mine, I'm going to overstack somewhere safe.");
    }

    //Diagnostics::DiagnosticText("local:%d,distance:%d,overmining:%d", local_fields.resource_inventory_.size(), long_dist_fields.resource_inventory_.size(), overmining_fields.resource_inventory_.size());

    if (!assignment_complete && old_mineral_patch) {
        miner.startMine(old_mineral_patch);
        old_mineral_patch->getType().isRefinery() ? CUNYAIModule::updateUnitPhase(unit, StoredUnit::MiningGas) : CUNYAIModule::updateUnitPhase(unit, StoredUnit::MiningMin);
    }
    return assignment_complete;
} // closure worker mine

  //Attaches MINER to nearest mine in RESOURCE INVENTORY. Performs proper incremenation in the overall land_inventory, requires access to overall inventory for maps.
bool WorkerManager::attachToNearestMine(Resource_Inventory & ri, StoredUnit & miner) {
    Stored_Resource* closest = CUNYAIModule::getClosestGroundStored(ri, miner.pos_);
    if (closest) {
        miner.startMine(*closest); // this must update the LAND INVENTORY proper. Otherwise it will update some temperary value, to "availabile Fields".
        if (miner.bwapi_unit_ && miner.isAssignedBuilding()) {
            CUNYAIModule::my_reservation.removeReserveSystem(TilePosition(miner.bwapi_unit_->getOrderTargetPosition()), miner.bwapi_unit_->getBuildType(), true);
        }
        miner.phase_ = closest->type_.isNeutral() ? StoredUnit::MiningMin : StoredUnit::MiningGas; // is it gas or mineral?
        miner.phase_ = !closest->occupied_resource_ ? StoredUnit::DistanceMining : miner.phase_; // is it distance?
        miner.updateStoredUnit(miner.bwapi_unit_);
        return true;
    }
    return false;
}

void WorkerManager::attachToParticularMine(Stored_Resource &mine, Resource_Inventory &ri, StoredUnit &miner) {
    miner.startMine(ri.resource_inventory_.find(mine.bwapi_unit_)->second); // go back to your old job.  // Let's not make mistakes by attaching it to "availabile Fields""
    if (miner.bwapi_unit_ && miner.isAssignedBuilding()) {
        CUNYAIModule::my_reservation.removeReserveSystem(TilePosition(miner.bwapi_unit_->getOrderTargetPosition()), miner.bwapi_unit_->getBuildType(), true);
    }
    miner.phase_ = mine.type_.isNeutral() ? StoredUnit::MiningMin : StoredUnit::MiningGas;
    miner.updateStoredUnit(miner.bwapi_unit_);
}

void WorkerManager::attachToParticularMine(Unit &mine, Resource_Inventory &ri, StoredUnit &miner) {
    miner.startMine(ri.resource_inventory_.find(mine)->second); // Let's not make mistakes by attaching it to "availabile Fields""
    miner.phase_ = mine->getType().isNeutral() ? StoredUnit::MiningMin : StoredUnit::MiningGas;
    miner.updateStoredUnit(miner.bwapi_unit_);
}

bool WorkerManager::assignClear(const Unit & unit)
{
    StoredUnit& miner = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit)->second;
    Resource_Inventory available_fields;
    Unit old_mineral_patch = nullptr;
    old_mineral_patch = miner.locked_mine_;
    miner.stopMine();

    for (auto& r = CUNYAIModule::land_inventory.resource_inventory_.begin(); r != CUNYAIModule::land_inventory.resource_inventory_.end() && !CUNYAIModule::land_inventory.resource_inventory_.empty(); r++) {
        if (r->second.blocking_mineral_ && r->second.number_of_miners_ < 1 && r->second.pos_.isValid() && r->second.type_.isMineralField() && CUNYAIModule::currentMapInventory.checkViableGroundPath(r->second.pos_, miner.pos_) && CUNYAIModule::currentMapInventory.front_line_base_.getDistance(r->second.pos_) < CUNYAIModule::currentMapInventory.my_portion_of_the_map_) {
            available_fields.addStored_Resource(r->second);
        }
    } //find closest mine meeting this criteria.

    if (!available_fields.resource_inventory_.empty() && attachToNearestMine(available_fields, miner))
        return CUNYAIModule::updateUnitPhase(miner.bwapi_unit_, StoredUnit::Clearing); //oof we have to manually edit the command to clear, it's a rare case.
    else if (old_mineral_patch) {
        miner.startMine(old_mineral_patch);
        old_mineral_patch->getType().isRefinery() ? CUNYAIModule::updateUnitPhase(unit, StoredUnit::MiningGas) : CUNYAIModule::updateUnitPhase(unit, StoredUnit::MiningMin);
    }

    return false;
}

bool WorkerManager::checkBlockingMinerals(const Unit & unit, UnitInventory & ui)
{
    bool already_assigned = false;
    StoredUnit& miner = ui.unit_map_.find(unit)->second;
    Resource_Inventory available_fields;

    for (auto& r = CUNYAIModule::land_inventory.resource_inventory_.begin(); r != CUNYAIModule::land_inventory.resource_inventory_.end() && !CUNYAIModule::land_inventory.resource_inventory_.empty(); r++) {
        if (r->second.blocking_mineral_ && r->second.number_of_miners_ < 1 && r->second.pos_.isValid() && r->second.type_.isMineralField() && !CUNYAIModule::checkOccupiedNeighborhood(CUNYAIModule::enemy_player_model.units_, r->second.pos_) && CUNYAIModule::currentMapInventory.checkViableGroundPath(r->second.pos_, miner.pos_)) {
            return true;
        }
    } //find closest mine meeting this criteria.

    return false;
}

bool WorkerManager::checkGasDump() {
    return AssemblyManager::canMakeCUNY(UnitTypes::Zerg_Hydralisk, false) ||
        AssemblyManager::canMakeCUNY(UnitTypes::Zerg_Mutalisk, false) ||
        AssemblyManager::canMakeCUNY(UnitTypes::Zerg_Ultralisk, false);
}

//Returns True if there is an out for gas. Does not consider all possible gas outlets.
bool WorkerManager::checkGasOutlet() {
    if (CUNYAIModule::techmanager.checkTechAvail() && max({ CUNYAIModule::assemblymanager.getMaxGas(), CUNYAIModule::techmanager.getMaxGas() }) > CUNYAIModule::my_reservation.getExcessGas()) return true;
    if (checkGasDump()) return true;
    if (CUNYAIModule::countUnitsInProgress(UnitTypes::Zerg_Lair) > 0) true; //Tier 2 means gas is always possible. 
    if(CUNYAIModule::buildorder.cumulative_gas_ > 0) return true;

    return false;
}

bool WorkerManager::workerWork(const Unit &u) {

    StoredUnit& miner = *CUNYAIModule::friendly_player_model.units_.getStoredUnit(u); // we will want DETAILED information about this unit.
    int t_game = Broodwar->getFrameCount();


    // Identify old mineral task. If there's no new better job, put them back on this without disturbing them.
    bool was_gas = miner.isAssignedGas();
    bool was_mineral = miner.isAssignedMining();
    bool was_long_mine = miner.isLongRangeLock();
    Unit old_mineral_patch = nullptr;
    if ((was_mineral || was_gas) && !was_long_mine) {
        old_mineral_patch = miner.locked_mine_;
    }
    bool task_guard = false;

    //if (CUNYAIModule::spamGuard(miner.bwapi_unit_)) { // careful about interactions between spam guards.
    //Do not disturb fighting workers or workers assigned to clear a position. Do not spam. Allow them to remain locked on their task.
    switch (miner.phase_)
    {
    case StoredUnit::MiningGas:
        if (!isEmptyWorker(u) && u->isVisible()) { // If he's not in the refinery, auto return.
            task_guard = workersReturn(u);
        }
        else if (u->isVisible() && CUNYAIModule::spamGuard(u, 14) && miner.bwapi_unit_->gather(miner.locked_mine_)) { // If he's not in the refinery, reassign him back to work.
            miner.updateStoredUnit(u);
            task_guard = true;
        }
        break;
    case StoredUnit::MiningMin:
        if (!isEmptyWorker(u)) { //auto return if needed.
            task_guard = workersReturn(u);
        }
        else if ((miner.isBrokenLock() && CUNYAIModule::spamGuard(u, 14)) || u->isIdle()) { //5 frame pause needed on gamestart or else the workers derp out. Can't go to 3.
            if (miner.bwapi_unit_->gather(miner.locked_mine_)) { // reassign him back to work.
                miner.updateStoredUnit(u);
                task_guard = true;
            }
        }
        break;
    case StoredUnit::Clearing: // does the same thing as...
    case StoredUnit::DistanceMining:
        if (!isEmptyWorker(u)) { //auto return if needed.
            miner.stopMine();
            task_guard = workersReturn(u);
        }
        else if (miner.getMine() && miner.getMine()->bwapi_unit_ && miner.getMine()->bwapi_unit_->isVisible() && ((miner.isBrokenLock() && CUNYAIModule::spamGuard(u, 14)) || u->isIdle())) { //If there is a mineral and we can see it, mine it.
            if (miner.bwapi_unit_->gather(miner.locked_mine_)) { // reassign him back to work.
                miner.updateStoredUnit(u);
                task_guard = true;
            }
        }
        else if (miner.getMine() && (!miner.getMine()->bwapi_unit_ || !miner.getMine()->bwapi_unit_->isVisible()) && (CUNYAIModule::spamGuard(u, 14) || u->isIdle())) { // Otherwise walk to that mineral.
            if (miner.bwapi_unit_->move(miner.getMine()->pos_)) { // reassign him back to work.
                miner.updateStoredUnit(u);
                task_guard = true;
            }
        }
        break;
    case StoredUnit::Prebuilding:
        //Diagnostics::DiagnosticTrack(u);
        if (CUNYAIModule::spamGuard(u, 14) /*&& u->isIdle()*/) {
            task_guard = workerPrebuild(u); // may need to move from prebuild to "build".
        }
        break;
    case StoredUnit::Returning: // this command is very complex. Only consider reassigning if reassignment is NEEDED. Otherwise reassign to locked mine (every 14 frames) and move to the proper phase.
        if (isEmptyWorker(u)) {
            if (CUNYAIModule::assemblymanager.buildBuilding(u)) { // try to build, if succesful we break otherwise mine more.
                break;
            }
            else if (miner.locked_mine_) {
                if (miner.locked_mine_->getType().isMineralField() && excess_gas_capacity_ && CUNYAIModule::gas_starved) {
                    task_guard = workersCollect(u, 500);
                }
                else if (miner.locked_mine_->getType().isRefinery() && !CUNYAIModule::gas_starved) {
                    task_guard = workersCollect(u, 500);
                }
                else if ((miner.locked_mine_->getType().isMineralField() && !excess_gas_capacity_) || (miner.locked_mine_->getType().isRefinery() && CUNYAIModule::gas_starved)) { // if they're doing the proper thing, consider reassigning them.
                    if (workersClear(u) || CUNYAIModule::assemblymanager.buildBuilding(u)) {
                        task_guard = true;
                    }
                    else {
                        if (miner.locked_mine_->getType().isRefinery()) {
                            task_guard = CUNYAIModule::updateUnitPhase(u, StoredUnit::Phase::MiningGas);
                        }
                        else if (miner.locked_mine_->getType().isMineralField()) {
                            task_guard = CUNYAIModule::updateUnitPhase(u, StoredUnit::Phase::MiningMin);
                        }
                    }
                }
            }
            if (!task_guard) task_guard = CUNYAIModule::updateUnitPhase(u, StoredUnit::Phase::None);
        }
        else if (CUNYAIModule::spamGuard(u, 14) && u->isIdle()) {
            if (!task_guard) task_guard = CUNYAIModule::updateUnitPhase(u, StoredUnit::Phase::None);
        }
        break;
    case StoredUnit::None:
        if (!isEmptyWorker(u)) { //auto return if needed.
            task_guard = workersReturn(u); // mark worker as returning.
        }
        else {
            task_guard = workersClear(u) || CUNYAIModule::assemblymanager.buildBuilding(u) || workersCollect(u, 500);
        }
        break;
    case StoredUnit::Building:
        if (CUNYAIModule::spamGuard(u, 14) && u->isIdle()) {
            if (AssemblyManager::isFullyVisibleBuildLocation(miner.intended_build_type_, miner.intended_build_tile_)) {
                if ((AssemblyManager::isPlaceableCUNY(miner.intended_build_type_, miner.intended_build_tile_) || miner.intended_build_type_.isRefinery())) {
                    Diagnostics::DiagnosticText("Continuing to Build at ( %d , %d ).", miner.intended_build_tile_.x, miner.intended_build_tile_.y);
                    //if (!u->build(miner.intended_build_type_, miner.intended_build_tile_))
                    //    Diagnostics::DiagnosticText("Can't seem to build at ( %d , %d ).", miner.intended_build_tile_.x, miner.intended_build_tile_.y);
                    return CUNYAIModule::updateUnitPhase(u, StoredUnit::Building);
                }
                else {
                    task_guard = CUNYAIModule::updateUnitPhase(u, StoredUnit::Phase::None);
                }
            }
            //if (CUNYAIModule::assemblymanager.buildBuilding(u))
            //    task_guard = true;
            //if (!task_guard) task_guard = CUNYAIModule::updateUnitPhase(u, StoredUnit::Phase::None);
        }
        break;
    case StoredUnit::Attacking:
    case StoredUnit::Retreating:
        CUNYAIModule::my_reservation.removeReserveSystem(miner.intended_build_tile_, miner.intended_build_type_, true); // workers ought to be free of obligations
        if (CUNYAIModule::spamGuard(u) /*&& u->isIdle()*/) { //If you don't stop them from fighting, you will easily over-pull workers, this is a disaster. So you must stop them, even if they are not-idle.
            auto enemy_loc = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::enemy_player_model.units_, u->getPosition(), 400);
            enemy_loc.updateUnitInventorySummary();
            if (!CUNYAIModule::isInDanger(u->getType(), enemy_loc)) {
                task_guard = workersCollect(u);
            }
        }
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
            if (w->second.phase_ == StoredUnit::DistanceMining) {
                long_distance_miners_found++;
            }
        }
    }
    workers_distance_mining_ = long_distance_miners_found;
}

void WorkerManager::updateWorkersOverstacked()
{
    int overstacked_workers = 0;
    for (auto & w = CUNYAIModule::land_inventory.resource_inventory_.begin(); w != CUNYAIModule::land_inventory.resource_inventory_.end() && !CUNYAIModule::land_inventory.resource_inventory_.empty(); w++) {
        if (w->second.type_.isMineralField() && w->second.number_of_miners_ - 2 > 0) {
            overstacked_workers += w->second.number_of_miners_ - 2;
        }
        if (w->second.type_.isRefinery() && w->second.number_of_miners_ - 3 > 0) {
            overstacked_workers += w->second.number_of_miners_ - 3;
        }
    }
    workers_overstacked_ = overstacked_workers;
}

void WorkerManager::updateExcessCapacity()
{
    //excess_gas_capacity_ = CUNYAIModule::land_inventory.local_gas_collectors_ <= CUNYAIModule::land_inventory.local_refineries_ * 2 && CUNYAIModule::land_inventory.local_refineries_ > 0 && CUNYAIModule::gas_starved;
    excess_gas_capacity_ = (gas_workers_ <= CUNYAIModule::land_inventory.getLocalRefineries() * 2) && CUNYAIModule::land_inventory.getLocalRefineries() > 0;

}

int WorkerManager::getGasWorkers()
{
    return gas_workers_;
}

int WorkerManager::getMinWorkers()
{
    return min_workers_;
}

bool WorkerManager::checkExcessGasCapacity()
{
    return excess_gas_capacity_;
}

int WorkerManager::getDistanceWorkers()
{
    return workers_distance_mining_;
}

int WorkerManager::getOverstackedWorkers()
{
    return workers_overstacked_;
}
