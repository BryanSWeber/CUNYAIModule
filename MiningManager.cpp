#pragma once
#include "Source\CUNYAIModule.h"
#include "MiningManager.h"

using namespace BWAPI;
using namespace Filter;
using namespace std;

bool MiningManager::isEmptyWorker(const Unit &unit) {
    bool laden_worker = unit->isCarryingGas() || unit->isCarryingMinerals();
    return !laden_worker;
}
  //Sends a Worker to gather resources from UNITTYPE (mineral or refinery). letabot has code on this. "AssignEvenSplit(Unit* unit)"
void MiningManager::workerGather(const Unit &unit, const UnitType mine) {

    bool already_assigned = false;
    Stored_Unit& miner = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit)->second;
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
    for (auto r : CUNYAIModule::land_inventory.resource_inventory_) {

        if (mine_minerals) {
            mine_is_right_type = r.second.type_.isMineralField() && !r.second.blocking_mineral_; // Only gather from "Real" mineral patches with substantive value. Don't mine from obstacles.
        }
        else {
            mine_is_right_type = r.second.type_.isRefinery() && r.second.bwapi_unit_ && IsOwned(r.second.bwapi_unit_);
        }

        if ( mine_is_right_type && r.second.pos_.isValid() && r.second.number_of_miners_ <= low_drone && r.second.number_of_miners_ < max_drone && r.second.occupied_resource_ &&  CUNYAIModule::current_map_inventory.checkViableGroundPath(r.second.pos_, miner.pos_)) { //occupied natural -> resource is close to a base
            low_drone = r.second.number_of_miners_;
            found_low_occupied_mine = true;
        }

    } // find drone minima.

    //    CUNYAIModule::DiagnosticText("LOW DRONE COUNT : %d", low_drone);
    //    CUNYAIModule::DiagnosticText("Mine Minerals : %d", mine_minerals);

    for (auto r : CUNYAIModule::land_inventory.resource_inventory_) {

        if (mine_minerals) {
            mine_is_right_type = r.second.type_.isMineralField() && !r.second.blocking_mineral_; // Only gather from "Real" mineral patches with substantive value. Don't mine from obstacles.
        }
        else {
            mine_is_right_type = r.second.type_.isRefinery() && r.second.bwapi_unit_ && IsOwned(r.second.bwapi_unit_);
        }

        if (mine_is_right_type && r.second.number_of_miners_ <= low_drone && r.second.number_of_miners_ < max_drone &&  CUNYAIModule::current_map_inventory.checkViableGroundPath(r.second.pos_, miner.pos_) ) {
            long_dist_fields.addStored_Resource(r.second); // if it doesn't have a closeby base, then it is a long distance field and not a priority.

            if (r.second.occupied_resource_ && found_low_occupied_mine) { //if it has a closeby base, we want to prioritize those resources first.
                available_fields.addStored_Resource(r.second);
            }
        }
    } //find closest mine meeting this criteria.

    // mine from the closest mine with a base nearby.
    if (!available_fields.resource_inventory_.empty()) {
        attachToNearestMine(available_fields, CUNYAIModule::current_map_inventory, miner); // phase is already updated.
    }
    else if (!long_dist_fields.resource_inventory_.empty()) { // if there are no suitible mineral patches with bases nearby, long-distance mine.
        attachToNearestMine(long_dist_fields, CUNYAIModule::current_map_inventory, miner); // phase is already updated.
    }

} // closure worker mine

//Attaches MINER to nearest mine in RESOURCE INVENTORY. Performs proper incremenation in the overall land_inventory, requires access to overall inventory for maps.
void MiningManager::attachToNearestMine(Resource_Inventory &ri, Map_Inventory &inv, Stored_Unit &miner) {
    Stored_Resource* closest = CUNYAIModule::getClosestGroundStored(ri, miner.pos_);
    if (closest /*&& closest->bwapi_unit_ && miner.bwapi_unit_->gather(closest->bwapi_unit_) && checkSafeMineLoc(closest->pos_, ui, inventory)*/) {
        miner.startMine(*closest); // this must update the LAND INVENTORY proper. Otherwise it will update some temperary value, to "availabile Fields".
        if (miner.bwapi_unit_ && miner.isAssignedBuilding()) {
            CUNYAIModule::my_reservation.removeReserveSystem(TilePosition(miner.bwapi_unit_->getOrderTargetPosition()), miner.bwapi_unit_->getBuildType(), true);
        }
        miner.phase_ = closest->type_.isNeutral() ? Stored_Unit::MiningMin : Stored_Unit::MiningGas;
    }
    miner.updateStoredUnit(miner.bwapi_unit_);
}

void MiningManager::attachToParticularMine(Stored_Resource &mine, Resource_Inventory &ri, Stored_Unit &miner) {
    miner.startMine(ri.resource_inventory_.find(mine.bwapi_unit_)->second); // go back to your old job.  // Let's not make mistakes by attaching it to "availabile Fields""
    if (miner.bwapi_unit_ && miner.isAssignedBuilding()) {
        CUNYAIModule::my_reservation.removeReserveSystem(TilePosition(miner.bwapi_unit_->getOrderTargetPosition()), miner.bwapi_unit_->getBuildType(), true);
    }
    miner.phase_ = mine.type_.isNeutral() ? Stored_Unit::MiningMin : Stored_Unit::MiningGas;
    miner.updateStoredUnit(miner.bwapi_unit_);
}

void MiningManager::attachToParticularMine(Unit &mine, Resource_Inventory &ri, Stored_Unit &miner) {
    miner.startMine(ri.resource_inventory_.find(mine)->second); // Let's not make mistakes by attaching it to "availabile Fields""
    miner.phase_ = mine->getType().isNeutral() ? Stored_Unit::MiningMin : Stored_Unit::MiningGas;
    miner.updateStoredUnit(miner.bwapi_unit_);
}

void MiningManager::workerClear( const Unit & unit )
{
    bool already_assigned = false;
    Stored_Unit& miner = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit)->second;
    //bool building_unit = unit->getLastCommand().getType() == UnitCommandTypes::Morph || unit->getLastCommand().getType() == UnitCommandTypes::Build || unit->getLastCommand().getTargetPosition() == Position(inventory.next_expo_);
    bool building_unit = miner.isAssignedBuilding();
    Resource_Inventory available_fields;

    for (auto& r = CUNYAIModule::land_inventory.resource_inventory_.begin(); r != CUNYAIModule::land_inventory.resource_inventory_.end() && !CUNYAIModule::land_inventory.resource_inventory_.empty(); r++) {
        if (r->second.blocking_mineral_ && r->second.number_of_miners_ < 1 && r->second.pos_.isValid() && r->second.type_.isMineralField() && CUNYAIModule::current_map_inventory.checkViableGroundPath(r->second.pos_, miner.pos_) && CUNYAIModule::current_map_inventory.home_base_.getDistance(r->second.pos_) <  CUNYAIModule::current_map_inventory.my_portion_of_the_map_ ) {
            available_fields.addStored_Resource(r->second);
        }
    } //find closest mine meeting this criteria.

    if (!available_fields.resource_inventory_.empty()) {
        attachToNearestMine(available_fields, CUNYAIModule::current_map_inventory, miner);
        miner.phase_ = Stored_Unit::Clearing;
    }
    miner.updateStoredUnit(unit);
}

bool MiningManager::checkBlockingMinerals(const Unit & unit, Unit_Inventory & ui)
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
bool MiningManager::checkGasOutlet() {
    bool outlet_avail = false;

    if (CUNYAIModule::techmanager.checkTechAvail() && CUNYAIModule::Count_Units( BWAPI::UnitTypes::Zerg_Spawning_Pool) > 0 ) {
        outlet_avail = true;
    }

    bool long_condition = Broodwar->self()->hasUnitTypeRequirement( UnitTypes::Zerg_Hydralisk ) ||
        Broodwar->self()->hasUnitTypeRequirement( UnitTypes::Zerg_Mutalisk ) ||
        Broodwar->self()->hasUnitTypeRequirement( UnitTypes::Zerg_Ultralisk );

    if ( long_condition ) {
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

bool MiningManager::workerWork(const Unit &u) {

        Stored_Unit& miner = *CUNYAIModule::friendly_player_model.units_.getStoredUnit(u); // we will want DETAILED information about this unit.
        int t_game = Broodwar->getFrameCount();

        bool too_much_gas = CUNYAIModule::current_map_inventory.getGasRatio() > CUNYAIModule::delta;
        bool no_recent_worker_alteration = miner.time_of_last_purge_ < t_game - 12 && miner.time_since_last_command_ > 12 && !CUNYAIModule::isRecentCombatant(miner);

        // Identify old mineral task. If there's no new better job, put them back on this without disturbing them.
        bool was_gas = miner.isAssignedGas();
        bool was_mineral = miner.isAssignedMining();
        bool was_long_mine = miner.isLongRangeLock();
        bool build_check_this_frame = false;
        Unit old_mineral_patch = nullptr;
        if ((was_mineral || was_gas) && !was_long_mine) {
            old_mineral_patch = miner.locked_mine_;
        }

        //Workers at their end build location should build there!
        if (miner.phase_ == Stored_Unit::Prebuilding && t_game % 14 == 0) {
            if (Broodwar->isExplored(CUNYAIModule::current_map_inventory.next_expo_) && u->build(UnitTypes::Zerg_Hatchery, CUNYAIModule::current_map_inventory.next_expo_) && CUNYAIModule::my_reservation.addReserveSystem(CUNYAIModule::current_map_inventory.next_expo_, UnitTypes::Zerg_Hatchery)) {
                CUNYAIModule::DiagnosticText("Continuing to Expo at ( %d , %d ).", CUNYAIModule::current_map_inventory.next_expo_.x, CUNYAIModule::current_map_inventory.next_expo_.y);
                return CUNYAIModule::updateUnitPhase(u, Stored_Unit::Phase::Prebuilding);
            }
            else if (!Broodwar->isExplored(CUNYAIModule::current_map_inventory.next_expo_) && CUNYAIModule::my_reservation.addReserveSystem(CUNYAIModule::current_map_inventory.next_expo_, UnitTypes::Zerg_Hatchery)) {
                u->move(Position(CUNYAIModule::current_map_inventory.next_expo_));
                CUNYAIModule::DiagnosticText("Unexplored Expo at ( %d , %d ). Still moving there to check it out.", CUNYAIModule::current_map_inventory.next_expo_.x, CUNYAIModule::current_map_inventory.next_expo_.y);
                return CUNYAIModule::updateUnitPhase(u, Stored_Unit::Phase::Prebuilding);
            }
        }

        if (!CUNYAIModule::isRecentCombatant(miner) && !miner.isAssignedClearing() && !miner.isAssignedBuilding() && CUNYAIModule::spamGuard(miner.bwapi_unit_)) {
            //Do not disturb fighting workers or workers assigned to clear a position. Do not spam. Allow them to remain locked on their task.

            // Each mineral-related subtask does the following:
            // Checks if it is doing a task of lower priority.
            // It clears the worker.
            // It tries to assign the worker to the new task.
            // If it is successfully assigned, continue. On the next frame you will be caught by "Maintain the locks" step.
            // If it is not successfully assigned, return to old task.

            //BUILD-RELATED TASKS:
            if (isEmptyWorker(u) && miner.isAssignedResource() && !miner.isAssignedGas() && !miner.isAssignedBuilding() && CUNYAIModule::my_reservation.last_builder_sent_ < t_game - Broodwar->getLatencyFrames() - 3 * 24 && !build_check_this_frame) { //only get those that are in line or gathering minerals, but not carrying them or harvesting gas. This always irked me.
                build_check_this_frame = true;
                CUNYAIModule::friendly_player_model.units_.purgeWorkerRelationsNoStop(u); //Must be disabled or else under some conditions, we "stun" a worker every frame. Usually the exact same one, essentially killing it.
                CUNYAIModule::assemblymanager.buildBuilding(u); // something's funny here. I would like to put it in the next line conditional but it seems to cause a crash when no major buildings are left to build.
                if (miner.isAssignedBuilding()) { //Don't purge the building relations here - we just established them!
                    miner.stopMine();
                    return true;
                }
                else if (old_mineral_patch) {
                    attachToParticularMine(old_mineral_patch, CUNYAIModule::land_inventory, miner); // go back to your old job. Updated unit.
                }
            } // Close Build loop

              //MINERAL-RELATED TASKS
              //Workers need to clear empty patches.
            bool time_to_start_clearing_a_path = CUNYAIModule::current_map_inventory.hatches_ >= 2 && checkBlockingMinerals(u, CUNYAIModule::friendly_player_model.units_);
            if (time_to_start_clearing_a_path && CUNYAIModule::current_map_inventory.workers_clearing_ == 0 && isEmptyWorker(u)) {
                CUNYAIModule::friendly_player_model.units_.purgeWorkerRelationsNoStop(u);
                workerClear(u);
                if (miner.isAssignedClearing()) {
                    CUNYAIModule::current_map_inventory.updateWorkersClearing();
                    return true;
                }
                else if (old_mineral_patch) {
                    attachToParticularMine(old_mineral_patch, CUNYAIModule::land_inventory, miner); // go back to your old job. Updated unit.
                }
            } // clear those empty mineral patches that block paths.

            //Shall we assign them to gas?
            CUNYAIModule::land_inventory.updateMines();
            bool could_use_another_gas = CUNYAIModule::land_inventory.local_gas_collectors_ * 2 <= CUNYAIModule::land_inventory.local_refineries_ && CUNYAIModule::land_inventory.local_refineries_ > 0 && CUNYAIModule::gas_starved;
            bool worker_bad_gas = (CUNYAIModule::gas_starved && miner.isAssignedMining() && could_use_another_gas);
            bool worker_bad_mine = ((!CUNYAIModule::gas_starved || too_much_gas) && miner.isAssignedGas());
            bool unassigned_worker = !miner.isAssignedResource() && !miner.isAssignedBuilding() && !miner.isLongRangeLock() && !miner.isAssignedClearing();
            // If we need gas, get gas!
            if (unassigned_worker || (worker_bad_gas && CUNYAIModule::current_map_inventory.last_gas_check_ < t_game - 3 * 24) || worker_bad_mine && CUNYAIModule::current_map_inventory.last_gas_check_ < t_game - 5 * 24) { //if this is your first worker of the frame consider resetting him.
                CUNYAIModule::friendly_player_model.units_.purgeWorkerRelationsNoStop(u);
                CUNYAIModule::current_map_inventory.last_gas_check_ = t_game;

                if (could_use_another_gas) {
                    workerGather(u, UnitTypes::Zerg_Extractor); // assign a worker a mine (gas). Will return null if no viable refinery exists. Might be a bug source.
                    if (miner.isAssignedGas()) {
                    }
                    else { // default to gathering minerals.
                        CUNYAIModule::friendly_player_model.units_.purgeWorkerRelationsNoStop(u);
                        workerGather(u, UnitTypes::Resource_Mineral_Field); //assign a worker (minerals)
                        if (miner.isAssignedMining()) {
                        }
                        else {
                            //Broodwar->sendText("Whoopsie, a fall-through!");
                        }
                    }
                }                //Otherwise, we should put them on minerals.
                else { //if this is your first worker of the frame consider resetting him.
                    workerGather(u, UnitTypes::Resource_Mineral_Field); // assign a worker a mine (gas). Will return null if no viable refinery exists. Might be a bug source.
                }
            }
        }

        // return minerals manually if you have them.
        if (!isEmptyWorker(u) && u->isIdle() && no_recent_worker_alteration) {
            miner.bwapi_unit_->returnCargo();
            return true;
        }

        // If idle get a job.
        if (u->isIdle() && no_recent_worker_alteration) {
            CUNYAIModule::friendly_player_model.units_.purgeWorkerRelationsNoStop(u);
            workerGather(u, UnitTypes::Resource_Mineral_Field);
            if (!miner.isAssignedMining()) {
                workerGather(u, UnitTypes::Resource_Vespene_Geyser);
            }
            miner.updateStoredUnit(u);
            return true;
        }

        // let's leave units in full-mine alone. Miners will be automatically assigned a "return cargo task" by BW upon collecting a mineral from the mine.
        if (miner.isAssignedResource() && !isEmptyWorker(u) && !u->isIdle()) {
            return true;
        }

        // Maintain the locks by assigning the worker to their intended mine!
        bool worker_has_lockable_task = miner.isAssignedClearing() || miner.isAssignedResource();

        if (worker_has_lockable_task && !isEmptyWorker(u) && !u->isIdle()) {
            return true;
        }

        if (worker_has_lockable_task && ((miner.isBrokenLock() && miner.time_since_last_command_ > 12) || (u->isIdle() && no_recent_worker_alteration))) { //5 frame pause needed on gamestart or else the workers derp out. Can't go to 3.
            if (!miner.bwapi_unit_->gather(miner.locked_mine_)) { // reassign him back to work.
                CUNYAIModule::friendly_player_model.units_.purgeWorkerRelationsNoStop(u); //If he can't get back to work something's wrong with you and we're resetting you.
            }
            miner.updateStoredUnit(u);
            return true;
        }
        else if (worker_has_lockable_task && miner.isLongRangeLock()) {
            if (!miner.bwapi_unit_->move(miner.getMine()->pos_)) { // reassign him back to work.
                CUNYAIModule::friendly_player_model.units_.purgeWorkerRelationsNoStop(u); //If he can't get back to work something's wrong with you and we're resetting you.
            }
            miner.updateStoredUnit(u);
            return true;
        }
};