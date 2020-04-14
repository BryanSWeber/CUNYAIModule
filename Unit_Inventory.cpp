#pragma once

#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\Unit_Inventory.h"
#include "Source\Map_Inventory.h"
#include "Source\Reservation_Manager.h"
#include "Source/Diagnostics.h"
#include "Source\FAP\FAP\include\FAP.hpp" // could add to include path but this is more explicit.
#include <random> // C++ base random is low quality.
#include <fstream>
#include <filesystem>

//Unit_Inventory functions.

std::default_random_engine Unit_Inventory::generator_;  //Will be used to obtain a seed for the random number engine


//Creates an instance of the unit inventory class.
Unit_Inventory::Unit_Inventory() {}

Unit_Inventory::Unit_Inventory(const Unitset &unit_set) {

    for (const auto & u : unit_set) {
        unit_map_.insert({ u, StoredUnit(u) });
    }

    updateUnitInventorySummary(); //this call is a CPU sink.
}

void Unit_Inventory::updateUnitInventory(const Unitset &unit_set) {
    for (const auto & u : unit_set) {
        StoredUnit* found_unit = getStoredUnit(u);
        if (found_unit) {
            found_unit->updateStoredUnit(u); // explicitly does not change locked mineral.
        }
        else {
            unit_map_.insert({ u, StoredUnit(u) });
        }
    }
    updateUnitInventorySummary(); //this call is a CPU sink.
}

void Unit_Inventory::updateUnitsControlledBy(const Player &player)
{
    for (auto &e : unit_map_) {
        if (e.second.bwapi_unit_ && e.second.bwapi_unit_->exists()) { // If the unit is visible now, update its position.
            e.second.updateStoredUnit(e.second.bwapi_unit_);
        }
        else {
            e.second.time_since_last_seen_++;
            if (Broodwar->isVisible(TilePosition(e.second.pos_))) {  // if you can see the tile it SHOULD be at Burned down buildings will pose a problem in future.

                bool present = false;

                Unitset enemies_tile = Broodwar->getUnitsOnTile(TilePosition(e.second.pos_));  // Confirm it is present.  Main use: Addons convert to neutral if their main base disappears, extractors.
                for (auto &et : enemies_tile) {
                    present = et->getID() == e.second.unit_ID_ /*|| (*et)->isCloaked() || (*et)->isBurrowed()*/;
                    if (present && et->getPlayer() == player) {
                        e.second.updateStoredUnit(et);
                        break;
                    }
                }
                if (!present) { // If the last known position is visible, and the unit is not there, then they have an unknown position.  Note a variety of calls to e->first cause crashes here. Let us make a linear projection of their position 24 frames (1sec) into the future.
                    Position potential_running_spot = Position(e.second.pos_.x + e.second.velocity_x_, e.second.pos_.y + e.second.velocity_y_);
                    if (!potential_running_spot.isValid() || Broodwar->isVisible(TilePosition(potential_running_spot))) {
                        e.second.valid_pos_ = false;
                    }
                    else if (potential_running_spot.isValid() && !Broodwar->isVisible(TilePosition(potential_running_spot)) && (e.second.type_.isFlyer() || Broodwar->isWalkable(WalkPosition(potential_running_spot)))) {
                        e.second.pos_ = potential_running_spot;
                        e.second.valid_pos_ = true;
                    }
                }
            }
        }

        if (e.second.time_since_last_dmg_ > 6) e.second.circumference_remaining_ = e.second.circumference_; //Every 6 frames, give it back its circumfrance. This may occasionally lead to the unit being considered surrounded/unsurrounded incorrectly.  Tracking every single target and updating is not yet implemented but could be eventually.

        if ((e.second.type_ == UnitTypes::Resource_Vespene_Geyser) || e.second.type_ == UnitTypes::Unknown) { // Destroyed refineries revert to geyers, requiring the manual catch. Unknowns should be removed as well.
            e.second.valid_pos_ = false;
        }

    }

}

void Unit_Inventory::purgeBrokenUnits()
{
    for (auto &e = this->unit_map_.begin(); e != this->unit_map_.end() && !this->unit_map_.empty(); ) {
        if ((e->second.type_ == UnitTypes::Resource_Vespene_Geyser) || // Destroyed refineries revert to geyers, requiring the manual catc.
            e->second.type_ == UnitTypes::None) { // sometimes they have a "none" in inventory. This isn't very reasonable, either.
            e = this->unit_map_.erase(e); // get rid of these. Don't iterate if this occurs or we will (at best) end the loop with an invalid iterator.
        }
        else {
            ++e;
        }
    }
}

void Unit_Inventory::purgeUnseenUnits()
{
    for (auto &f = this->unit_map_.begin(); f != this->unit_map_.end() && !this->unit_map_.empty(); ) {
        if (!f->second.bwapi_unit_ || !f->second.bwapi_unit_->exists()) { // sometimes they have a "none" in inventory. This isn't very reasonable, either.
            f = this->unit_map_.erase(f); // get rid of these. Don't iterate if this occurs or we will (at best) end the loop with an invalid iterator.
        }
        else {
            ++f;
        }
    }
}

//void Unit_Inventory::purgeAllPhases()
//{
//    for (auto &u = this->unit_inventory_.begin(); u != this->unit_inventory_.end() && !this->unit_inventory_.empty(); ) {
//        u->second.phase_ = "None";
//    }
//}

// Decrements all resources worker was attached to, clears all reservations associated with that worker. Stops Unit.
void Unit_Inventory::purgeWorkerRelationsStop(const Unit &unit)
{
    UnitCommand command = unit->getLastCommand();
    auto found_object = this->unit_map_.find(unit);
    if (found_object != this->unit_map_.end()) {
        StoredUnit& miner = found_object->second;

        miner.stopMine();
        if (unit->getOrderTargetPosition() != Positions::Origin) {
            if (command.getType() == UnitCommandTypes::Morph || command.getType() == UnitCommandTypes::Build) {
                CUNYAIModule::my_reservation.removeReserveSystem(TilePosition(unit->getOrderTargetPosition()), unit->getBuildType(), true);
            }
            if (command.getTargetTilePosition() == CUNYAIModule::current_map_inventory.next_expo_) {
                CUNYAIModule::my_reservation.removeReserveSystem(CUNYAIModule::current_map_inventory.next_expo_, UnitTypes::Zerg_Hatchery, true);
            }
        }
        unit->stop();
        miner.time_of_last_purge_ = Broodwar->getFrameCount();
        miner.phase_ = StoredUnit::None;
        miner.updateStoredUnit(unit);
    }
    else {
        Diagnostics::DiagnosticText("Failed to purge worker in inventory.");
    }
}

// Decrements all resources worker was attached to, clears all reservations associated with that worker. Stops Unit.
void Unit_Inventory::purgeWorkerRelationsNoStop(const Unit &unit)
{
    UnitCommand command = unit->getLastCommand();
    auto found_object = this->unit_map_.find(unit);
    if (found_object != this->unit_map_.end()) {
        StoredUnit& miner = found_object->second;

        miner.stopMine();
        if (unit->getOrderTargetPosition() != Positions::Origin) {
            if (command.getType() == UnitCommandTypes::Morph || command.getType() == UnitCommandTypes::Build) {
                CUNYAIModule::my_reservation.removeReserveSystem(TilePosition(unit->getOrderTargetPosition()), unit->getBuildType(), true);
            }
            if (command.getTargetTilePosition() == CUNYAIModule::current_map_inventory.next_expo_) {
                CUNYAIModule::my_reservation.removeReserveSystem(CUNYAIModule::current_map_inventory.next_expo_, UnitTypes::Zerg_Hatchery, true);
            }
        }
        miner.time_of_last_purge_ = Broodwar->getFrameCount();
        miner.phase_ = StoredUnit::None;
        miner.updateStoredUnit(unit);
    }
    else {
        Diagnostics::DiagnosticText("Failed to purge worker in inventory.");
    }
}

// Decrements all resources worker was attached to, clears all reservations associated with that worker. Stops Unit.
void Unit_Inventory::purgeWorkerRelationsOnly(const Unit &unit, Resource_Inventory &ri, Map_Inventory &inv, Reservation &res)
{
    UnitCommand command = unit->getLastCommand();
    auto found_object = this->unit_map_.find(unit);
    if (found_object != this->unit_map_.end()) {
        StoredUnit& miner = found_object->second;

        miner.stopMine();
        if (unit->getOrderTargetPosition() != Positions::Origin) {
            if (command.getType() == UnitCommandTypes::Morph || command.getType() == UnitCommandTypes::Build) {
                res.removeReserveSystem(TilePosition(unit->getOrderTargetPosition()), unit->getBuildType(), true);
            }
            if (command.getTargetTilePosition() == inv.next_expo_) {
                res.removeReserveSystem(inv.next_expo_, UnitTypes::Zerg_Hatchery, true);
            }
        }
        miner.time_of_last_purge_ = Broodwar->getFrameCount();
        miner.updateStoredUnit(unit);
    }
    else {
        Diagnostics::DiagnosticText("Failed to purge worker in inventory.");
    }
}

void Unit_Inventory::drawAllWorkerTasks() const
{
    for (auto u : unit_map_) {
        if (u.second.type_ == UnitTypes::Zerg_Drone) {
            if (u.second.locked_mine_ && !u.second.isAssignedResource() && !u.second.isAssignedClearing()) {
                Diagnostics::drawLine(u.second.pos_, u.second.locked_mine_->getPosition(), CUNYAIModule::current_map_inventory.screen_position_, Colors::White);
            }
            else if (u.second.isAssignedMining()) {
                Diagnostics::drawLine(u.second.pos_, u.second.locked_mine_->getPosition(), CUNYAIModule::current_map_inventory.screen_position_, Colors::Green);
            }
            else if (u.second.isAssignedGas()) {
                Diagnostics::drawLine(u.second.pos_, u.second.locked_mine_->getPosition(), CUNYAIModule::current_map_inventory.screen_position_, Colors::Brown);
            }
            else if (u.second.isAssignedClearing()) {
                Diagnostics::drawLine(u.second.pos_, u.second.locked_mine_->getPosition(), CUNYAIModule::current_map_inventory.screen_position_, Colors::Blue);
            }

            if (u.second.isAssignedBuilding()) {
                Diagnostics::drawDot(u.second.pos_, CUNYAIModule::current_map_inventory.screen_position_, Colors::Purple);
            }
        }
    }
}

// Blue if invalid position (lost and can't find), red if valid.
void Unit_Inventory::drawAllLocations() const
{
    for (auto e = unit_map_.begin(); e != unit_map_.end() && !unit_map_.empty(); e++) {
        if (e->second.valid_pos_) {
            Diagnostics::drawCircle(e->second.pos_, CUNYAIModule::current_map_inventory.screen_position_, (e->second.type_.dimensionUp() + e->second.type_.dimensionLeft()) / 2, Colors::Red); // Plot their last known position.
        }
        else if (!e->second.valid_pos_) {
            Diagnostics::drawCircle(e->second.pos_, CUNYAIModule::current_map_inventory.screen_position_, (e->second.type_.dimensionUp() + e->second.type_.dimensionLeft()) / 2, Colors::Blue); // Plot their last known position.
        }

    }

}

//Marks as red if it's on a minitile that ground units should not be at.
void Unit_Inventory::drawAllMisplacedGroundUnits() const
{
    if constexpr (DIAGNOSTIC_MODE) {
        for (auto e = unit_map_.begin(); e != unit_map_.end() && !unit_map_.empty(); e++) {
            if (CUNYAIModule::isOnScreen(e->second.pos_, CUNYAIModule::current_map_inventory.screen_position_) && !e->second.type_.isBuilding()) {
                if (CUNYAIModule::current_map_inventory.unwalkable_barriers_with_buildings_[WalkPosition(e->second.pos_).x][WalkPosition(e->second.pos_).y] == 1) {
                    Broodwar->drawCircleMap(e->second.pos_, (e->second.type_.dimensionUp() + e->second.type_.dimensionLeft()) / 2, Colors::Red, true); // Mark as RED if not in a walkable spot.
                }
                else if (CUNYAIModule::current_map_inventory.unwalkable_barriers_with_buildings_[WalkPosition(e->second.pos_).x][WalkPosition(e->second.pos_).y] == 0) {
                    Broodwar->drawCircleMap(e->second.pos_, (e->second.type_.dimensionUp() + e->second.type_.dimensionLeft()) / 2, Colors::Blue, true); // Mark as RED if not in a walkable spot.
                }
            }
        }
    }
}

// Updates the count of units.
bool Unit_Inventory::addStoredUnit(const Unit &unit) {
    return unit_map_.insert({ unit, StoredUnit(unit) }).second;
};

bool Unit_Inventory::addStoredUnit(const StoredUnit &stored_unit) {
    return unit_map_.insert({ stored_unit.bwapi_unit_ , stored_unit }).second;
};

Position Unit_Inventory::positionBuildFap(bool friendly) {
    std::uniform_int_distribution<int> small_map(half_map_ * friendly, half_map_ + half_map_ * friendly);     // default values for output.
    int rand_x = small_map(generator_);
    int rand_y = small_map(generator_);
    return Position(rand_x, rand_y);
}

Position Unit_Inventory::positionMCFAP(const StoredUnit & su) {
    std::uniform_int_distribution<int> small_noise(static_cast<int>(-CUNYAIModule::getProperSpeed(su.type_)) * 4, static_cast<int>(CUNYAIModule::getProperSpeed(su.type_)) * 4);     // default values for output.
    int rand_x = small_noise(generator_);
    int rand_y = small_noise(generator_);
    return Position(rand_x, rand_y) + su.pos_;
}

void StoredUnit::updateStoredUnit(const Unit &unit) {

    valid_pos_ = true;
    pos_ = unit->getPosition();
    build_type_ = unit->getBuildType();
    if (unit->getShields() + unit->getHitPoints() < shields_ + health_) time_since_last_dmg_ = 0;
    else time_since_last_dmg_++;

    shields_ = unit->getShields();
    health_ = unit->getHitPoints();
    current_hp_ = shields_ + health_;
    velocity_x_ = static_cast<int>(round(unit->getVelocityX()));
    velocity_y_ = static_cast<int>(round(unit->getVelocityY()));
    order_ = unit->getOrder();
    command_ = unit->getLastCommand();
    areaID_ = BWEM::Map::Instance().GetNearestArea(unit->getTilePosition())->Id();
    time_since_last_command_ = Broodwar->getFrameCount() - unit->getLastCommandFrame();
    angle_ = unit->getAngle();

    if (unit->isVisible()) time_since_last_seen_ = 0;
    else time_since_last_seen_++;

    //Needed for FAP.
    is_flying_ = unit->isFlying();
    elevation_ = BWAPI::Broodwar->getGroundHeight(TilePosition(pos_));
    cd_remaining_ = unit->getAirWeaponCooldown();
    stimmed_ = unit->isStimmed();
    burrowed_ = unit->isBurrowed();
    cloaked_ = unit->isCloaked();
    detected_ = unit->isDetected(); // detected doesn't work for personal units, only enemy units.
    if (type_ != unit->getType()) {
        type_ = unit->getType();
        StoredUnit shell = StoredUnit(type_);
        if (unit->getPlayer()->isEnemy(Broodwar->self())) {
            StoredUnit shell = StoredUnit(type_, (bool)CUNYAIModule::enemy_player_model.researches_.upgrades_.at(UpgradeTypes::Carrier_Capacity), (bool)CUNYAIModule::enemy_player_model.researches_.upgrades_.at(UpgradeTypes::Reaver_Capacity));
        }
        stock_value_ = shell.stock_value_; // longer but prevents retyping.
        circumference_ = shell.circumference_;
        circumference_remaining_ = shell.circumference_;
        future_fap_value_ = shell.stock_value_; //Updated in updateFAPvalue(), this is simply a natural placeholder.
        current_stock_value_ = static_cast<int>(stock_value_ * current_hp_ / static_cast<double>(type_.maxHitPoints() + type_.maxShields()));
        count_of_consecutive_predicted_deaths_ = 0;
    }
    else {
        //bool unit_fighting = type_.canAttack() && phase_ == StoredUnit::Attacking"; //&& !(burrowed_ && type_ == UnitTypes::Zerg_Lurker && time_since_last_dmg_ > 24); // detected doesn't work for personal units, only enemy units.
        bool unit_escaped = (burrowed_ || cloaked_) && (!detected_ || time_since_last_dmg_ > FAP_SIM_DURATION); // can't still be getting shot if we're setting its assesment to 0.
        circumference_remaining_ = circumference_;
        current_stock_value_ = static_cast<int>(stock_value_ * current_hp_ / static_cast<double>(type_.maxHitPoints() + type_.maxShields()));
        if (future_fap_value_ > 0 || unit_escaped) count_of_consecutive_predicted_deaths_ = 0;
        else count_of_consecutive_predicted_deaths_++;


    }
    if ((phase_ == StoredUnit::Upgrading || phase_ == StoredUnit::Researching || (phase_ == StoredUnit::Building && unit->isCompleted() && type_.isBuilding())) && unit->isIdle()) phase_ = StoredUnit::None; // adjust units that are no longer upgrading.

}

//Removes units that have died
void Unit_Inventory::removeStoredUnit(Unit e_unit) {
    unit_map_.erase(e_unit);
};


Position Unit_Inventory::getMeanLocation() const {
    int x_sum = 0;
    int y_sum = 0;
    int count = 0;
    Position out = Position(0, 0);
    for (const auto &u : this->unit_map_) {
        if (u.second.valid_pos_) {
            x_sum += u.second.pos_.x;
            y_sum += u.second.pos_.y;
            count++;
        }
    }
    if (count > 0) {
        out = Position(x_sum / count, y_sum / count);
    }
    return out;
}

Position Unit_Inventory::getMeanBuildingLocation() const {
    int x_sum = 0;
    int y_sum = 0;
    int count = 0;
    for (const auto &u : this->unit_map_) {
        if ((((u.second.type_.isBuilding() || u.second.type_.isAddon()) && !u.second.type_.isSpecialBuilding()) || u.second.bwapi_unit_->isMorphing()) && u.second.valid_pos_) {
            x_sum += u.second.pos_.x;
            y_sum += u.second.pos_.y;
            count++;
        }
    }
    if (count > 0) {
        Position out = { x_sum / count, y_sum / count };
        return out;
    }
    else {
        return Position(0, 0); // you're dead at this point, fyi.
    }
}

Position Unit_Inventory::getMeanAirLocation() const {
    int x_sum = 0;
    int y_sum = 0;
    int count = 0;
    for (const auto &u : this->unit_map_) {
        if (u.second.is_flying_ && u.second.valid_pos_) {
            x_sum += u.second.pos_.x;
            y_sum += u.second.pos_.y;
            count++;
        }
    }
    if (count > 0) {
        Position out = { x_sum / count, y_sum / count };
        return out;
    }
    else {
        return Positions::Origin; // you're dead at this point, fyi.
    }
}
// In progress
Position Unit_Inventory::getStrongestLocation() const {
    int x_sum = 0;
    int y_sum = 0;
    int count = 0;
    int standard_value = StoredUnit(UnitTypes::Zerg_Drone, false, false).stock_value_;
    for (const auto &u : this->unit_map_) {
        if (CUNYAIModule::isFightingUnit(u.second) && u.second.valid_pos_) {
            int remaining_stock = u.second.current_stock_value_;
            while (remaining_stock > 0) {
                x_sum += u.second.pos_.x;
                y_sum += u.second.pos_.y;
                count++;
                remaining_stock -= standard_value;
            }
        }
    }
    if (count > 0) {
        Position out = { x_sum / count, y_sum / count };
        return out;
    }
    else {
        return Positions::Origin; // you're dead at this point, fyi.
    }
}


Position Unit_Inventory::getMeanCombatLocation() const {
    int x_sum = 0;
    int y_sum = 0;
    int count = 0;
    for (const auto &u : this->unit_map_) {
        if (u.second.type_.canAttack() && u.second.valid_pos_) {
            x_sum += u.second.pos_.x;
            y_sum += u.second.pos_.y;
            count++;
        }
    }
    if (count > 0) {
        Position out = { x_sum / count, y_sum / count };
        return out;
    }
    else {
        return Position(0, 0);  // you might be dead at this point, fyi.
    }

}

//for the army that can actually move.
Position Unit_Inventory::getMeanArmyLocation() const {
    int x_sum = 0;
    int y_sum = 0;
    int count = 0;
    for (const auto &u : this->unit_map_) {
        if (u.second.type_.canAttack() && u.second.valid_pos_ && u.second.type_.canMove() && !u.second.type_.isWorker()) {
            x_sum += u.second.pos_.x;
            y_sum += u.second.pos_.y;
            count++;
        }
    }
    if (count > 0) {
        Position out = { x_sum / count, y_sum / count };
        return out;
    }
    else {
        return Positions::Origin;  // you might be dead at this point, fyi.
    }
}

//for the army that can actually move. Removed for usage of Broodwar->getclosest(), a very slow function.
//Position Unit_Inventory::getClosestMeanArmyLocation() const {
//    Position mean_pos = getMeanArmyLocation();
//    if( mean_pos && mean_pos != Position(0,0) && mean_pos.isValid()){
//       Unit nearest_neighbor = Broodwar->getClosestUnit(mean_pos, !IsFlyer && IsOwned, 500);
//        if (nearest_neighbor && nearest_neighbor->getPosition() ) {
//            Position out = Broodwar->getClosestUnit(mean_pos, !IsFlyer && IsOwned, 500)->getPosition();
//            return out;
//        }
//        else {
//            return Positions::Origin;  // you might be dead at this point, fyi.
//        }
//    }
//    else {
//        return Positions::Origin;  // you might be dead at this point, fyi.
//    }
//}

Unit_Inventory Unit_Inventory::getInventoryAtArea(const int areaID) const {
    Unit_Inventory return_inventory;
    for (const auto &u : this->unit_map_) {
        if (u.second.areaID_ == areaID) { return_inventory.addStoredUnit(u.second); }
    }
    return return_inventory;
}

Unit_Inventory Unit_Inventory::getCombatInventoryAtArea(const int areaID) const {
    Unit_Inventory return_inventory;
    for (const auto &u : this->unit_map_) {
        if (u.second.areaID_ == areaID && CUNYAIModule::isFightingUnit(u.second)) { return_inventory.addStoredUnit(u.second); }
    }
    return return_inventory;
}

Unit_Inventory Unit_Inventory::getBuildingInventoryAtArea(const int areaID) const {
    Unit_Inventory return_inventory;
    for (const auto &u : this->unit_map_) {
        if (u.second.areaID_ == areaID && u.second.type_.isBuilding()) { return_inventory.addStoredUnit(u.second); }
    }
    return return_inventory;
}

Unit_Inventory operator+(const Unit_Inventory& lhs, const Unit_Inventory& rhs)
{
    Unit_Inventory total = lhs;
    total.unit_map_.insert(rhs.unit_map_.begin(), rhs.unit_map_.end());
    total.updateUnitInventorySummary();
    return total;
}

Unit_Inventory operator-(const Unit_Inventory& lhs, const Unit_Inventory& rhs)
{
    Unit_Inventory total;
    total.unit_map_.insert(lhs.unit_map_.begin(), lhs.unit_map_.end());

    for (map<Unit, StoredUnit>::const_iterator& it = rhs.unit_map_.begin(); it != rhs.unit_map_.end();) {
        if (total.unit_map_.find(it->first) != total.unit_map_.end()) {
            total.unit_map_.erase(it->first);
        }
        else {
            it++;
        }
    }
    total.updateUnitInventorySummary();
    return total;
}

void Unit_Inventory::updateUnitInventorySummary() {
    //Tally up crucial details about enemy. Should be doing this onclass. Perhaps make an enemy summary class?
    //Set default values to 0;

    stock_fliers_ = 0;
    stock_ground_units_ = 0;
    stock_both_up_and_down_ = 0;
    stock_shoots_up_ = 0;
    stock_shoots_down_ = 0;
    stock_high_ground_ = 0;
    stock_fighting_total_ = 0;
    stock_ground_fodder_ = 0;
    stock_air_fodder_ = 0;
    stock_total_ = 0;
    stock_full_health_ = 0;
    stock_psion_ = 0;
    total_supply_ = 0;
    max_range_air_ = 0;
    max_range_ground_ = 0;
    max_range_ = 0;
    max_cooldown_ = 0;
    max_speed_ = 0;
    worker_count_ = 0;
    volume_ = 0;
    detector_count_ = 0;
    cloaker_count_ = 0;
    flyer_count_ = 0;
    ground_count_ = 0;
    ground_melee_count_ = 0;
    ground_range_count_ = 0;
    building_count_ = 0;
    resource_depot_count_ = 0;
    future_fap_stock_ = 0;
    is_shooting_ = 0;
    island_count_ = 0;

    count_of_each_phase_ = { { StoredUnit::Phase::None, 0 } ,
     { StoredUnit::Phase::Attacking, 0 },
     { StoredUnit::Phase::Retreating, 0 },
     { StoredUnit::Phase::Prebuilding, 0 },
     { StoredUnit::Phase::PathingOut, 0 },
     { StoredUnit::Phase::PathingHome, 0 },
     { StoredUnit::Phase::Surrounding, 0 },
     { StoredUnit::Phase::NoRetreat, 0 },
     { StoredUnit::Phase::MiningMin, 0 },
     { StoredUnit::Phase::MiningGas, 0 },
     { StoredUnit::Phase::Returning, 0 },
     { StoredUnit::Phase::DistanceMining, 0 },
     { StoredUnit::Phase::Clearing, 0 },
     { StoredUnit::Phase::Upgrading, 0 },
     { StoredUnit::Phase::Researching, 0 },
     { StoredUnit::Phase::Morphing, 0 },
     { StoredUnit::Phase::Building, 0 },
     { StoredUnit::Phase::Detecting, 0 } };

    vector<UnitType> already_seen_types;

    for (auto const & u_iter : unit_map_) { // should only search through unit types not per unit.

        future_fap_stock_ += u_iter.second.future_fap_value_;
        is_shooting_ += u_iter.first->isAttacking();
        total_supply_ += u_iter.second.type_.supplyRequired();
        building_count_ += u_iter.second.type_.isBuilding();
        count_of_each_phase_.at(u_iter.second.phase_)++;
        stock_high_ground_ += u_iter.second.elevation_ == 2 || u_iter.second.elevation_ == 4;
        island_count_ += u_iter.second.is_on_island_;

        if (find(already_seen_types.begin(), already_seen_types.end(), u_iter.second.type_) == already_seen_types.end()) { // if you haven't already checked this unit type.

            bool flying_unit = u_iter.second.type_.isFlyer();
            int unit_value_for_all_of_type = CUNYAIModule::Stock_Units(u_iter.second.type_, *this);
            int count_of_unit_type = CUNYAIModule::countUnits(u_iter.second.type_, *this);
            if (CUNYAIModule::isFightingUnit(u_iter.second)) {
                bool up_gun = u_iter.second.type_.airWeapon() != WeaponTypes::None || u_iter.second.type_ == UnitTypes::Terran_Bunker || u_iter.second.type_ == UnitTypes::Protoss_Carrier;
                bool down_gun = u_iter.second.type_.groundWeapon() != WeaponTypes::None || u_iter.second.type_ == UnitTypes::Terran_Bunker || u_iter.second.type_ == UnitTypes::Protoss_Reaver || u_iter.second.type_ == UnitTypes::Zerg_Lurker || u_iter.second.type_ == UnitTypes::Protoss_Carrier;
                bool cloaker = u_iter.second.type_.isCloakable() || u_iter.second.type_ == UnitTypes::Zerg_Lurker || u_iter.second.type_.hasPermanentCloak();
                int range_temp = (bool)(u_iter.second.bwapi_unit_) * CUNYAIModule::getExactRange(u_iter.second.type_, u_iter.second.bwapi_unit_->getPlayer()) + !(bool)(u_iter.second.bwapi_unit_) * CUNYAIModule::getExactRange(u_iter.second.type_, Broodwar->enemy());

                stock_fliers_ += flying_unit * unit_value_for_all_of_type; // add the value of that type of unit to the flier stock.
                flyer_count_ += flying_unit * count_of_unit_type;

                cloaker_count_ = flying_unit * count_of_unit_type;
                stock_ground_units_ += !flying_unit * unit_value_for_all_of_type;
                ground_count_ += !flying_unit * count_of_unit_type;
                ground_melee_count_ += (u_iter.second.type_.groundWeapon().maxRange() <= 32) * !flying_unit * count_of_unit_type;
                ground_range_count_ += (u_iter.second.type_.groundWeapon().maxRange() > 32) * !flying_unit * count_of_unit_type;
                stock_shoots_up_ += up_gun * unit_value_for_all_of_type;
                stock_shoots_down_ += down_gun * unit_value_for_all_of_type;
                stock_both_up_and_down_ += (up_gun && down_gun) * unit_value_for_all_of_type;
                cloaker_count_ += cloaker * count_of_unit_type;
                stock_psion_ += (u_iter.second.type_.maxEnergy() > 0 * unit_value_for_all_of_type);

                max_cooldown_ = max(max(u_iter.second.type_.groundWeapon().damageCooldown(), u_iter.second.type_.airWeapon().damageCooldown()), max_cooldown_);
                max_range_ = (range_temp > max_range_) ? range_temp : max_range_;
                max_range_air_ = (range_temp > max_range_air_ && up_gun) ? range_temp : max_range_air_;
                max_range_ground_ = (range_temp > max_range_ground_ && down_gun) ? range_temp : max_range_ground_; //slightly faster if-else conditions.
                max_speed_ = max(static_cast<int>(CUNYAIModule::getProperSpeed(u_iter.second.type_)), max_speed_);
            }
            else {
                resource_depot_count_ += u_iter.second.type_.isResourceDepot() * count_of_unit_type;
                stock_air_fodder_ += flying_unit * unit_value_for_all_of_type; // add the value of that type of unit to the flier stock.
                stock_ground_fodder_ += !flying_unit * unit_value_for_all_of_type;

            }

            detector_count_ += u_iter.second.type_.isDetector() * count_of_unit_type;
            stock_full_health_ += u_iter.second.stock_value_ * count_of_unit_type;
            volume_ += !flying_unit * u_iter.second.type_.height()*u_iter.second.type_.width() * count_of_unit_type;
            //Region r = Broodwar->getRegionAt( u_iter.second.pos_ );
            //         if ( r && u_iter.second.valid_pos_ && u_iter.second.type_ != UnitTypes::Buildings ) {
            //             if ( r->isHigherGround() || r->getDefensePriority() > 1 ) {
            //                 high_ground += u_iter.second.current_stock_value_;
            //             }
            //         }
            already_seen_types.push_back(u_iter.second.type_);
        }
    }

    stock_fighting_total_ = stock_ground_units_ + stock_fliers_;
    stock_total_ = stock_fighting_total_ + stock_ground_fodder_ + stock_air_fodder_;
    worker_count_ = CUNYAIModule::countUnits(UnitTypes::Zerg_Drone, *this) + CUNYAIModule::countUnits(UnitTypes::Protoss_Probe, *this) + CUNYAIModule::countUnits(UnitTypes::Terran_SCV, *this);

}

void Unit_Inventory::printUnitInventory(const Player &player, const string &bonus)
{
    //string start = "learned_plan.readDirectory + player->getName() + bonus + ".txt";
    //string finish = learned_plan.writeDirectory + player->getName() + bonus + ".txt";
    //if (filesystem::exists(start.c_str()))
    //    filesystem::copy(start.c_str(), finish.c_str(), filesystem::copy_options::update_existing); // Furthermore, rename will fail if there is already an existing file.


    ifstream input; // brings in info;
    input.open(CUNYAIModule::learned_plan.writeDirectory + player->getName() + bonus + ".txt", ios::in);   // for each row
    string line;
    int csv_length = 0;
    while (getline(input, line)) {
        ++csv_length;
    }
    input.close(); // I have read the entire file already, need to close it and begin again.  Lacks elegance, but works.

    if ( (csv_length > 500 && player != Broodwar->self()) || Broodwar->elapsedTime() > (60 * 24) )
        return;  // let's not flood the world

    if (csv_length < 1) {
        ofstream output; // Prints to brood war file while in the WRITE file.
        output.open(CUNYAIModule::learned_plan.writeDirectory + player->getName() + bonus + ".txt", ios_base::app);
        output << "GameSeed" << ",";
        output << "GameTime" << ",";
        for (auto i : UnitTypes::allUnitTypes()) {
            if (!i.isNeutral() && !i.isHero() && !i.isSpecialBuilding() && !i.isResourceContainer() && !i.isPowerup() && !i.isBeacon() && i.getRace() != Races::None) {
                output << i.c_str() << ",";
            }
        }
        output << endl;
        output.close();
    }


    ofstream output; // Prints to brood war file while in the WRITE file.
    output.open(CUNYAIModule::learned_plan.writeDirectory + player->getName() + bonus + ".txt", ios_base::app);
    output << Broodwar->getRandomSeed() << ",";
    output << Broodwar->elapsedTime() << ",";
    for (auto i : UnitTypes::allUnitTypes()) {
        if (!i.isNeutral() && !i.isHero() && !i.isSpecialBuilding() && !i.isResourceContainer() && !i.isPowerup() && !i.isBeacon() && i.getRace() != Races::None) {
            output << CUNYAIModule::countUnits(i, *this) << ",";
        }
    }
    output << endl;
    output.close();

    //if constexpr (MOVE_OUTPUT_BACK_TO_READ) {
    //    string start = learned_plan.writeDirectory + player->getName() + bonus + ".txt";
    //    string finish = learned_plan.readDirectory + player->getName() + bonus + ".txt";
    //    if (filesystem::exists(start.c_str()))
    //        filesystem::copy(start.c_str(), finish.c_str(), filesystem::copy_options::update_existing); // Furthermore, rename will fail if there is already an existing file.
    //}
}

void Unit_Inventory::stopMine(Unit u) {
    if (u->getType().isWorker()) {
        StoredUnit& miner = unit_map_.find(u)->second;
        miner.stopMine();
    }
}

//StoredUnit functions.
StoredUnit::StoredUnit() = default;

//returns a steryotypical unit only.
StoredUnit::StoredUnit(const UnitType &unittype, const bool &carrierUpgrade, const bool &reaverUpgrade) {
    valid_pos_ = false;
    type_ = unittype;
    build_type_ = UnitTypes::None;
    shields_ = unittype.maxShields();
    health_ = unittype.maxHitPoints();
    current_hp_ = shields_ + health_;
    locked_mine_ = nullptr;
    circumference_ = type_.height() * 2 + type_.width() * 2;
    circumference_remaining_ = circumference_;
    is_flying_ = unittype.isFlyer();
    elevation_ = 0; //inaccurate and will need to be fixed.
    cd_remaining_ = 0;
    stimmed_ = false;
    is_on_island_ = false;

    shoots_down_ = unittype.groundWeapon() != WeaponTypes::None;
    shoots_up_ = unittype.airWeapon() != WeaponTypes::None;

    //Get unit's status. Precalculated, precached.

    stock_value_ = getGrownWeight(type_, carrierUpgrade, reaverUpgrade);
    if(stock_value_ == 0 ){
        stock_value_ = getTraditionalWeight(type_, carrierUpgrade, reaverUpgrade);
    }

    current_stock_value_ = stock_value_; // Precalculated, precached.
    future_fap_value_ = stock_value_;
};

int StoredUnit::getTraditionalWeight(const UnitType unittype, const bool &carrierUpgrade, const bool &reaverUpgrade){
        int modified_supply_ = unittype.supplyRequired();
        int modified_min_cost_ = unittype.mineralPrice();
        int modified_gas_cost_ = unittype.gasPrice();
        int stock_value_ = 0;

        if ((unittype.getRace() == Races::Zerg && unittype.isBuilding()) || unittype == UnitTypes::Terran_Bunker) {
            modified_supply_ += 2;
            modified_min_cost_ += 50;
        }  // Zerg units cost a supply (2, technically since BW cuts it in half.) // Assume bunkers are loaded with 1 marine

        if (unittype == UnitTypes::Zerg_Sunken_Colony || unittype == UnitTypes::Zerg_Spore_Colony) {
            modified_min_cost_ += UnitTypes::Zerg_Creep_Colony.mineralPrice();
        }

        if (unittype == UnitTypes::Zerg_Lurker) {
            modified_min_cost_ += UnitTypes::Zerg_Hydralisk.mineralPrice();
            modified_gas_cost_ += UnitTypes::Zerg_Hydralisk.gasPrice();
            modified_supply_ += UnitTypes::Zerg_Hydralisk.supplyRequired();
        }

        if (unittype == UnitTypes::Zerg_Devourer || unittype == UnitTypes::Zerg_Guardian) {
            modified_min_cost_ += UnitTypes::Zerg_Mutalisk.mineralPrice();
            modified_gas_cost_ += UnitTypes::Zerg_Mutalisk.gasPrice();
            modified_supply_ += UnitTypes::Zerg_Mutalisk.supplyRequired();
        }

        if (unittype == UnitTypes::Protoss_Archon) {
            modified_min_cost_ += UnitTypes::Protoss_High_Templar.mineralPrice() * 2;
            modified_gas_cost_ += UnitTypes::Protoss_High_Templar.gasPrice() * 2;
            modified_supply_ += UnitTypes::Protoss_High_Templar.supplyRequired() * 2;
        }

        if (unittype == UnitTypes::Protoss_Dark_Archon) {
            modified_min_cost_ += UnitTypes::Protoss_Dark_Templar.mineralPrice() * 2;
            modified_gas_cost_ += UnitTypes::Protoss_Dark_Templar.gasPrice() * 2;
            modified_supply_ += UnitTypes::Protoss_Dark_Templar.supplyRequired() * 2;
        }

        if (unittype == UnitTypes::Protoss_Carrier) { //Assume carriers are loaded with 4 interceptors.
            modified_min_cost_ += UnitTypes::Protoss_Interceptor.mineralPrice() * (4 + 4 * carrierUpgrade);
            modified_gas_cost_ += UnitTypes::Protoss_Interceptor.gasPrice() * (4 + 4 * carrierUpgrade);
        }

        if (unittype == UnitTypes::Protoss_Reaver) { // Assume Reavers are loaded with 5 scarabs unless upgraded
            modified_min_cost_ += BWAPI::UnitTypes::Protoss_Scarab.mineralPrice() * (5 + 5 * reaverUpgrade);
            modified_gas_cost_ += BWAPI::UnitTypes::Protoss_Scarab.gasPrice() * (5 + 5 * reaverUpgrade);
        }

        if (unittype == UnitTypes::Protoss_Interceptor || unittype.isSpell()) {
            modified_min_cost_ = 0;
            modified_gas_cost_ = 0;
            modified_supply_ = 0;
        }

        modified_supply_ /= (1 + static_cast<int>(unittype.isTwoUnitsInOneEgg())); // Lings return 1 supply when they should only return 0.5

        stock_value_ = static_cast<int>(modified_min_cost_ + 1.25 * modified_gas_cost_ + 25 * modified_supply_);

        return stock_value_ /= (1 + static_cast<int>(unittype.isTwoUnitsInOneEgg())); // condensed /2 into one line to avoid if-branch prediction.
}

int StoredUnit::getGrownWeight(const UnitType unittype, const bool &carrierUpgrade, const bool &reaverUpgrade) {
    int stock_value_ = CUNYAIModule::learned_plan.resetScale(unittype);

    if (unittype == UnitTypes::Protoss_Carrier) { //Assume carriers are loaded with 4 interceptors.
        stock_value_ += CUNYAIModule::learned_plan.resetScale(UnitTypes::Protoss_Interceptor) * (4 + 4 * carrierUpgrade);
    }

    if (unittype == UnitTypes::Protoss_Reaver) { // Assume Reavers are loaded with 5 scarabs unless upgraded
        stock_value_ += CUNYAIModule::learned_plan.resetScale(UnitTypes::Protoss_Scarab) * (5 + 5 * reaverUpgrade);
    }

    return stock_value_; // all other features are static features
}


// We must be able to create StoredUnit objects as well.
StoredUnit::StoredUnit(const Unit &unit) {
    valid_pos_ = true;
    unit_ID_ = unit->getID();
    bwapi_unit_ = unit;
    pos_ = unit->getPosition();
    type_ = unit->getType();
    build_type_ = unit->getBuildType();
    shields_ = unit->getShields();
    health_ = unit->getHitPoints();
    current_hp_ = shields_ + health_;
    locked_mine_ = nullptr;
    velocity_x_ = static_cast<int>(round(unit->getVelocityX()));
    velocity_y_ = static_cast<int>(round(unit->getVelocityY()));
    order_ = unit->getOrder();
    command_ = unit->getLastCommand();
    time_since_last_command_ = Broodwar->getFrameCount() - unit->getLastCommandFrame();
    time_since_last_dmg_ = FAP_SIM_DURATION + 1;// New units have not taken damage yet.
    count_of_consecutive_predicted_deaths_ = 0;
    circumference_ = type_.height() * 2 + type_.width() * 2;
    circumference_remaining_ = circumference_;
    angle_ = unit->getAngle();

    shoots_down_ = type_.groundWeapon() != WeaponTypes::None;
    shoots_up_ = type_.airWeapon() != WeaponTypes::None;

    //Needed for FAP.
    is_flying_ = unit->isFlying();
    elevation_ = BWAPI::Broodwar->getGroundHeight(TilePosition(pos_));
    is_on_island_ = CUNYAIModule::current_map_inventory.isOnIsland(pos_) && !is_flying_;

    cd_remaining_ = unit->getAirWeaponCooldown();
    stimmed_ = unit->isStimmed();

    //Get unit's status. Precalculated, precached.
    StoredUnit shell = StoredUnit(type_);
    if (unit->getPlayer()->isEnemy(Broodwar->self())) {
        StoredUnit shell = StoredUnit(type_, (bool)CUNYAIModule::enemy_player_model.researches_.upgrades_.at(UpgradeTypes::Carrier_Capacity), (bool)CUNYAIModule::enemy_player_model.researches_.upgrades_.at(UpgradeTypes::Reaver_Capacity));
    }
    modified_min_cost_ = shell.modified_min_cost_;
    modified_gas_cost_ = shell.modified_gas_cost_;
    modified_supply_ = shell.modified_supply_;
    stock_value_ = shell.stock_value_; //prevents retyping.
    future_fap_value_ = shell.stock_value_;
    current_stock_value_ = static_cast<int>(stock_value_ * current_hp_ / static_cast<double>(type_.maxHitPoints() + type_.maxShields())); // Precalculated, precached.

}


//Increments the number of miners on a resource.
void StoredUnit::startMine(Stored_Resource &new_resource) {
    locked_mine_ = new_resource.bwapi_unit_;
    CUNYAIModule::land_inventory.resource_inventory_.find(locked_mine_)->second.number_of_miners_++;
}
//Increments the number of miners on a resource.
void StoredUnit::startMine(Unit &new_resource) {
    locked_mine_ = new_resource;
    CUNYAIModule::land_inventory.resource_inventory_.find(locked_mine_)->second.number_of_miners_++;
}

//Decrements the number of miners on a resource, if possible.
void StoredUnit::stopMine() {
    if (locked_mine_) {
        if (getMine()) {
            getMine()->number_of_miners_ = max(getMine()->number_of_miners_ - 1, 0);
        }
    }
    locked_mine_ = nullptr;
}

//Decrements the number of miners on a resource, if possible.
void stopMine(const Unit &unit) {
    auto found_worker = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit);
    if (found_worker != CUNYAIModule::friendly_player_model.units_.unit_map_.end()) {
        if (found_worker->second.locked_mine_) {
            if (found_worker->second.getMine()) {
                found_worker->second.getMine()->number_of_miners_ = max(found_worker->second.getMine()->number_of_miners_ - 1, 0);
            }
        }
        found_worker->second.locked_mine_ = nullptr;
    }
}

//finds mine- Will return true something even if the mine DNE.
Stored_Resource* StoredUnit::getMine() {
    Stored_Resource* tenative_resource = nullptr;
    if (CUNYAIModule::land_inventory.resource_inventory_.find(locked_mine_) != CUNYAIModule::land_inventory.resource_inventory_.end()) {
        tenative_resource = &CUNYAIModule::land_inventory.resource_inventory_.find(locked_mine_)->second;
    }
    return tenative_resource;
}

//finds mine- Will return true null if the mine DNE.
Stored_Resource* getMine(const Unit &resource) {
    Stored_Resource* tenative_resource = nullptr;
    if (CUNYAIModule::land_inventory.resource_inventory_.find(resource) != CUNYAIModule::land_inventory.resource_inventory_.end()) {
        tenative_resource = &CUNYAIModule::land_inventory.resource_inventory_.find(resource)->second;
    }
    return tenative_resource;
}


//checks if mine started with less than 8 resource
bool StoredUnit::isAssignedClearing() {
    if (locked_mine_) {
        if (Stored_Resource* mine_of_choice = this->getMine()) { // if it has an associated mine.
            return mine_of_choice->blocking_mineral_;
        }
    }
    return false;
}

//checks if mine started with less than 8 resource
bool StoredUnit::isAssignedLongDistanceMining() {
    if (locked_mine_) {
        if (Stored_Resource* mine_of_choice = this->getMine()) { // if it has an associated mine.
            return !mine_of_choice->blocking_mineral_ && !mine_of_choice->occupied_resource_;
        }
    }
    return false;
}

//checks if worker is assigned to a mine that started with more than 8 resources (it is a proper mine).
bool StoredUnit::isAssignedMining() {
    if (locked_mine_) {
        if (CUNYAIModule::land_inventory.resource_inventory_.find(locked_mine_) != CUNYAIModule::land_inventory.resource_inventory_.end()) {
            Stored_Resource* mine_of_choice = this->getMine();
            return !mine_of_choice->blocking_mineral_ && mine_of_choice->type_.isMineralField();
        }
    }
    return false;
}

bool StoredUnit::isAssignedGas() {
    if (locked_mine_) {
        if (CUNYAIModule::land_inventory.resource_inventory_.find(locked_mine_) != CUNYAIModule::land_inventory.resource_inventory_.end()) {
            Stored_Resource* mine_of_choice = this->getMine();
            return mine_of_choice->type_.isRefinery();
        }
    }
    return false;
}

bool StoredUnit::isAssignedResource() {

    return StoredUnit::isAssignedMining() || StoredUnit::isAssignedGas();

}

// Warning- depends on unit being updated.
bool StoredUnit::isAssignedBuilding() {
    this->updateStoredUnit(this->bwapi_unit_); // unit needs to be updated to confirm this.
    bool building_sent = (build_type_.isBuilding() || order_ == Orders::Move || order_ == Orders::ZergBuildingMorph || command_.getType() == UnitCommandTypes::Build || command_.getType() == UnitCommandTypes::Morph) && time_since_last_command_ < 30 * 24 && !isAssignedResource();

    return building_sent;
}

//if the miner is not doing any thing
bool StoredUnit::isNoLock() {
    this->updateStoredUnit(this->bwapi_unit_); // unit needs to be updated to confirm this.
    return  bwapi_unit_ && !bwapi_unit_->getOrderTarget();
}

//if the miner is not mining his target. Target must be visible.
bool StoredUnit::isBrokenLock() {
    this->updateStoredUnit(this->bwapi_unit_); // unit needs to be updated to confirm this.
    Stored_Resource* target_mine = this->getMine(); // target mine must be visible to be broken. Otherwise it is a long range lock.
    return !(isLongRangeLock() || isLocallyLocked()); // Or its order target is not the mine, then we have a broken lock.
}

bool StoredUnit::isLocallyLocked() {
    this->updateStoredUnit(this->bwapi_unit_); // unit needs to be updated to confirm this.
    return  locked_mine_ && bwapi_unit_->getOrderTarget() && bwapi_unit_->getOrderTarget() == locked_mine_; // Everything must be visible and properly assigned.
}

//prototypeing
bool StoredUnit::isLongRangeLock() {
    this->updateStoredUnit(this->bwapi_unit_); // unit needs to be updated to confirm this.
    Stored_Resource* target_mine = this->getMine();
    return bwapi_unit_ && target_mine && target_mine->pos_ && (!Broodwar->isVisible(TilePosition(target_mine->pos_)) /*|| (target_mine->bwapi_unit_ && target_mine->bwapi_unit_->isMorphing())*/);
}

auto StoredUnit::convertToFAP(const Research_Inventory &ri) {
    int armor_upgrades = ri.upgrades_.at(type_.armorUpgrade()) + 2 * (type_ == UnitTypes::Zerg_Ultralisk * ri.upgrades_.at(UpgradeTypes::Chitinous_Plating));

    int gun_upgrades = max(ri.upgrades_.at(type_.groundWeapon().upgradeType()), ri.upgrades_.at(type_.airWeapon().upgradeType()));
    int shield_upgrades = static_cast<int>(shields_ > 0) * ri.upgrades_.at(UpgradeTypes::Protoss_Plasma_Shields);

    bool speed_tech = // safer to hardcode this.
        (type_ == UnitTypes::Zerg_Zergling && ri.upgrades_.at(UpgradeTypes::Metabolic_Boost)) ||
        (type_ == UnitTypes::Zerg_Hydralisk && ri.upgrades_.at(UpgradeTypes::Muscular_Augments)) ||
        (type_ == UnitTypes::Zerg_Overlord && ri.upgrades_.at(UpgradeTypes::Pneumatized_Carapace)) ||
        (type_ == UnitTypes::Zerg_Ultralisk && ri.upgrades_.at(UpgradeTypes::Anabolic_Synthesis)) ||
        (type_ == UnitTypes::Protoss_Scout && ri.upgrades_.at(UpgradeTypes::Gravitic_Thrusters)) ||
        (type_ == UnitTypes::Protoss_Observer && ri.upgrades_.at(UpgradeTypes::Gravitic_Boosters)) ||
        (type_ == UnitTypes::Protoss_Zealot && ri.upgrades_.at(UpgradeTypes::Leg_Enhancements)) ||
        (type_ == UnitTypes::Terran_Vulture && ri.upgrades_.at(UpgradeTypes::Ion_Thrusters));

    bool range_upgrade = // safer to hardcode this.
        (type_ == UnitTypes::Zerg_Hydralisk && ri.upgrades_.at(UpgradeTypes::Grooved_Spines)) ||
        (type_ == UnitTypes::Protoss_Dragoon && ri.upgrades_.at(UpgradeTypes::Singularity_Charge)) ||
        (type_ == UnitTypes::Terran_Marine && ri.upgrades_.at(UpgradeTypes::U_238_Shells)) ||
        (type_ == UnitTypes::Terran_Goliath && ri.upgrades_.at(UpgradeTypes::Charon_Boosters)) ||
        (type_ == UnitTypes::Terran_Barracks && ri.upgrades_.at(UpgradeTypes::U_238_Shells));

    bool attack_speed_upgrade =  // safer to hardcode this.
        (type_ == UnitTypes::Zerg_Zergling && ri.upgrades_.at(UpgradeTypes::Adrenal_Glands));

    int units_inside_object = 2 + (type_ == UnitTypes::Protoss_Carrier) * (2 + 4 * ri.upgrades_.at(UpgradeTypes::Carrier_Capacity)); // 2 if bunker, 4 if carrier, 8 if "carrier capacity" is present.

    return FAP::makeUnit<StoredUnit*>()
        .setData(this)
        .setUnitType(type_)
        .setPosition(pos_)
        .setHealth(health_)
        .setShields(shields_)
        .setFlying(is_flying_)
        .setElevation(elevation_)
        .setAttackerCount(units_inside_object)
        .setArmorUpgrades(armor_upgrades)
        .setAttackUpgrades(gun_upgrades)
        .setShieldUpgrades(shield_upgrades)
        .setSpeedUpgrade(speed_tech)
        .setAttackSpeedUpgrade(attack_speed_upgrade)
        .setAttackCooldownRemaining(cd_remaining_)
        .setStimmed(stimmed_)
        .setRangeUpgrade(range_upgrade)
        ;
}

auto StoredUnit::convertToFAPPosition(const Position &chosen_pos, const Research_Inventory &ri, const UpgradeType &upgrade, const TechType &tech) {

    int armor_upgrades = ri.upgrades_.at(type_.armorUpgrade()) +
        2 * (type_ == UnitTypes::Zerg_Ultralisk * ri.upgrades_.at(UpgradeTypes::Chitinous_Plating)) +
        (type_.armorUpgrade() == upgrade);

    int gun_upgrades = max(ri.upgrades_.at(type_.groundWeapon().upgradeType()) + type_.groundWeapon().upgradeType() == upgrade, ri.upgrades_.at(type_.airWeapon().upgradeType()) + type_.airWeapon().upgradeType() == upgrade);

    int shield_upgrades = static_cast<int>(shields_ > 0) * (ri.upgrades_.at(UpgradeTypes::Protoss_Plasma_Shields) + UpgradeTypes::Protoss_Plasma_Shields == upgrade); // No tests here.

    bool speed_tech = // safer to hardcode this.
        (type_ == UnitTypes::Zerg_Zergling && (ri.upgrades_.at(UpgradeTypes::Metabolic_Boost) || upgrade == UpgradeTypes::Metabolic_Boost)) ||
        (type_ == UnitTypes::Zerg_Hydralisk && (ri.upgrades_.at(UpgradeTypes::Muscular_Augments) || upgrade == UpgradeTypes::Muscular_Augments)) ||
        (type_ == UnitTypes::Zerg_Overlord && (ri.upgrades_.at(UpgradeTypes::Pneumatized_Carapace) || upgrade == UpgradeTypes::Pneumatized_Carapace)) ||
        (type_ == UnitTypes::Zerg_Ultralisk && (ri.upgrades_.at(UpgradeTypes::Anabolic_Synthesis) || upgrade == UpgradeTypes::Anabolic_Synthesis)) ||
        (type_ == UnitTypes::Protoss_Scout && (ri.upgrades_.at(UpgradeTypes::Gravitic_Thrusters) || upgrade == UpgradeTypes::Gravitic_Thrusters)) ||
        (type_ == UnitTypes::Protoss_Observer && (ri.upgrades_.at(UpgradeTypes::Gravitic_Boosters) || upgrade == UpgradeTypes::Gravitic_Boosters)) ||
        (type_ == UnitTypes::Protoss_Zealot && (ri.upgrades_.at(UpgradeTypes::Leg_Enhancements) || upgrade == UpgradeTypes::Leg_Enhancements)) ||
        (type_ == UnitTypes::Terran_Vulture && (ri.upgrades_.at(UpgradeTypes::Ion_Thrusters) || upgrade == UpgradeTypes::Ion_Thrusters));

    bool range_upgrade = // safer to hardcode this.
        (type_ == UnitTypes::Zerg_Hydralisk && (ri.upgrades_.at(UpgradeTypes::Grooved_Spines) || upgrade == UpgradeTypes::Grooved_Spines)) ||
        (type_ == UnitTypes::Protoss_Dragoon && (ri.upgrades_.at(UpgradeTypes::Singularity_Charge) || upgrade == UpgradeTypes::Singularity_Charge)) ||
        (type_ == UnitTypes::Terran_Marine && (ri.upgrades_.at(UpgradeTypes::U_238_Shells) || upgrade == UpgradeTypes::U_238_Shells)) ||
        (type_ == UnitTypes::Terran_Goliath && (ri.upgrades_.at(UpgradeTypes::Charon_Boosters) || upgrade == UpgradeTypes::Charon_Boosters)) ||
        (type_ == UnitTypes::Terran_Barracks && (ri.upgrades_.at(UpgradeTypes::U_238_Shells) || upgrade == UpgradeTypes::U_238_Shells));

    bool attack_speed_upgrade =  // safer to hardcode this.
        (type_ == UnitTypes::Zerg_Zergling && (ri.upgrades_.at(UpgradeTypes::Adrenal_Glands) || upgrade == UpgradeTypes::Adrenal_Glands));

    int units_inside_object = 2 + (type_ == UnitTypes::Protoss_Carrier) * (2 + 4 * ri.upgrades_.at(UpgradeTypes::Carrier_Capacity)); // 2 if bunker, 4 if carrier, 8 if "carrier capacity" is present. // Needs to extend for every race. Needs to include an indicator for self.

    return FAP::makeUnit<StoredUnit*>()
        .setData(this)
        .setUnitType(type_)
        .setPosition(chosen_pos)
        .setHealth(health_)
        .setShields(shields_)
        .setFlying(is_flying_)
        .setElevation(elevation_)
        .setAttackerCount(units_inside_object)
        .setArmorUpgrades(armor_upgrades)
        .setAttackUpgrades(gun_upgrades)
        .setShieldUpgrades(shield_upgrades)
        .setSpeedUpgrade(speed_tech)
        .setAttackSpeedUpgrade(attack_speed_upgrade)
        .setAttackCooldownRemaining(cd_remaining_)
        .setStimmed(stimmed_)
        .setRangeUpgrade(range_upgrade)
        ;
}

auto StoredUnit::convertToFAPDisabled(const Position &chosen_pos, const Research_Inventory &ri) {

    int armor_upgrades = ri.upgrades_.at(type_.armorUpgrade()) +
        2 * (type_ == UnitTypes::Zerg_Ultralisk * ri.upgrades_.at(UpgradeTypes::Chitinous_Plating));

    int gun_upgrades = max(ri.upgrades_.at(type_.groundWeapon().upgradeType()), ri.upgrades_.at(type_.airWeapon().upgradeType()));
    int shield_upgrades = static_cast<int>(shields_ > 0) * ri.upgrades_.at(UpgradeTypes::Protoss_Plasma_Shields);

    bool speed_tech = // safer to hardcode this.
        (type_ == UnitTypes::Zerg_Zergling && ri.upgrades_.at(UpgradeTypes::Metabolic_Boost)) ||
        (type_ == UnitTypes::Zerg_Hydralisk && ri.upgrades_.at(UpgradeTypes::Muscular_Augments)) ||
        (type_ == UnitTypes::Zerg_Overlord && ri.upgrades_.at(UpgradeTypes::Pneumatized_Carapace)) ||
        (type_ == UnitTypes::Zerg_Ultralisk && ri.upgrades_.at(UpgradeTypes::Anabolic_Synthesis)) ||
        (type_ == UnitTypes::Protoss_Scout && ri.upgrades_.at(UpgradeTypes::Gravitic_Thrusters)) ||
        (type_ == UnitTypes::Protoss_Observer && ri.upgrades_.at(UpgradeTypes::Gravitic_Boosters)) ||
        (type_ == UnitTypes::Protoss_Zealot && ri.upgrades_.at(UpgradeTypes::Leg_Enhancements)) ||
        (type_ == UnitTypes::Terran_Vulture && ri.upgrades_.at(UpgradeTypes::Ion_Thrusters));

    bool range_upgrade = // safer to hardcode this.
        (type_ == UnitTypes::Zerg_Hydralisk && ri.upgrades_.at(UpgradeTypes::Grooved_Spines)) ||
        (type_ == UnitTypes::Protoss_Dragoon && ri.upgrades_.at(UpgradeTypes::Singularity_Charge)) ||
        (type_ == UnitTypes::Terran_Marine && ri.upgrades_.at(UpgradeTypes::U_238_Shells)) ||
        (type_ == UnitTypes::Terran_Goliath && ri.upgrades_.at(UpgradeTypes::Charon_Boosters)) ||
        (type_ == UnitTypes::Terran_Barracks && ri.upgrades_.at(UpgradeTypes::U_238_Shells));

    bool attack_speed_upgrade =  // safer to hardcode this.
        (type_ == UnitTypes::Zerg_Zergling && ri.upgrades_.at(UpgradeTypes::Adrenal_Glands));

    int units_inside_object = 2 + (type_ == UnitTypes::Protoss_Carrier) * (2 + 4 * ri.upgrades_.at(UpgradeTypes::Carrier_Capacity)); // 2 if bunker, 4 if carrier, 8 if "carrier capacity" is present.

    return FAP::makeUnit<StoredUnit*>()
        .setData(this)
        .setOnlyUnitType(type_)
        //Vegetable characteristics below...
        .setSpeed(0)
        .setAirCooldown(99999)
        .setGroundCooldown(99999)
        .setAirDamage(0)
        .setGroundDamage(0)
        .setGroundMaxRange(0)
        .setAirMaxRange(0)
        .setGroundMinRange(0)
        .setAirMinRange(0)
        .setGroundDamageType(0)
        .setAirDamageType(0)
        .setArmor(type_.armor())
        .setMaxHealth(type_.maxHitPoints())
        .setMaxShields(type_.maxShields())
        .setOrganic(type_.isOrganic())
        .setUnitSize(type_.size())
        //normal characteristics below..
        .setPosition(chosen_pos)
        .setHealth(health_)
        .setShields(shields_)
        .setFlying(is_flying_)
        .setElevation(elevation_)
        .setAttackerCount(units_inside_object)
        .setArmorUpgrades(armor_upgrades)
        .setAttackUpgrades(gun_upgrades)
        .setShieldUpgrades(shield_upgrades)
        .setSpeedUpgrade(speed_tech)
        .setAttackSpeedUpgrade(attack_speed_upgrade)
        .setAttackCooldownRemaining(cd_remaining_)
        .setStimmed(stimmed_)
        .setRangeUpgrade(range_upgrade)
        ;
}

// Unit now can attack up and only attacks up. For the anti-air anti-ground test to have absolute equality.
auto StoredUnit::convertToFAPAnitAir(const Position &chosen_pos, const Research_Inventory &ri) {

    int armor_upgrades = ri.upgrades_.at(type_.armorUpgrade()) +
        2 * (type_ == UnitTypes::Zerg_Ultralisk * ri.upgrades_.at(UpgradeTypes::Chitinous_Plating));

    int gun_upgrades = max(ri.upgrades_.at(type_.groundWeapon().upgradeType()), ri.upgrades_.at(type_.airWeapon().upgradeType()));
    int shield_upgrades = static_cast<int>(shields_ > 0) * ri.upgrades_.at(UpgradeTypes::Protoss_Plasma_Shields);

    bool speed_tech = // safer to hardcode this.
        (type_ == UnitTypes::Zerg_Zergling && ri.upgrades_.at(UpgradeTypes::Metabolic_Boost)) ||
        (type_ == UnitTypes::Zerg_Hydralisk && ri.upgrades_.at(UpgradeTypes::Muscular_Augments)) ||
        (type_ == UnitTypes::Zerg_Overlord && ri.upgrades_.at(UpgradeTypes::Pneumatized_Carapace)) ||
        (type_ == UnitTypes::Zerg_Ultralisk && ri.upgrades_.at(UpgradeTypes::Anabolic_Synthesis)) ||
        (type_ == UnitTypes::Protoss_Scout && ri.upgrades_.at(UpgradeTypes::Gravitic_Thrusters)) ||
        (type_ == UnitTypes::Protoss_Observer && ri.upgrades_.at(UpgradeTypes::Gravitic_Boosters)) ||
        (type_ == UnitTypes::Protoss_Zealot && ri.upgrades_.at(UpgradeTypes::Leg_Enhancements)) ||
        (type_ == UnitTypes::Terran_Vulture && ri.upgrades_.at(UpgradeTypes::Ion_Thrusters));

    bool range_upgrade = // safer to hardcode this.
        (type_ == UnitTypes::Zerg_Hydralisk && ri.upgrades_.at(UpgradeTypes::Grooved_Spines)) ||
        (type_ == UnitTypes::Protoss_Dragoon && ri.upgrades_.at(UpgradeTypes::Singularity_Charge)) ||
        (type_ == UnitTypes::Terran_Marine && ri.upgrades_.at(UpgradeTypes::U_238_Shells)) ||
        (type_ == UnitTypes::Terran_Goliath && ri.upgrades_.at(UpgradeTypes::Charon_Boosters)) ||
        (type_ == UnitTypes::Terran_Barracks && ri.upgrades_.at(UpgradeTypes::U_238_Shells));

    bool attack_speed_upgrade =  // safer to hardcode this.
        (type_ == UnitTypes::Zerg_Zergling && ri.upgrades_.at(UpgradeTypes::Adrenal_Glands));

    int units_inside_object = 2 + (type_ == UnitTypes::Protoss_Carrier) * (2 + 4 * ri.upgrades_.at(UpgradeTypes::Carrier_Capacity)); // 2 if bunker, 4 if carrier, 8 if "carrier capacity" is present.

    return FAP::makeUnit<StoredUnit*>()
        .setData(this)
        .setOnlyUnitType(type_)
        //Vegetable characteristics below...
        .setSpeed(static_cast<float>(type_.topSpeed()))
        .setAirCooldown(type_.groundWeapon().damageCooldown())
        .setGroundCooldown(99999)
        .setAirDamage(type_.groundWeapon().damageAmount())
        .setGroundDamage(0)
        .setGroundMaxRange(0)
        .setAirMaxRange(type_.groundWeapon().maxRange())
        .setGroundMinRange(0)
        .setAirMinRange(type_.groundWeapon().minRange())
        .setGroundDamageType(0)
        .setAirDamageType(type_.groundWeapon().damageType())
        .setArmor(type_.armor())
        .setMaxHealth(type_.maxHitPoints())
        .setMaxShields(type_.maxShields())
        .setOrganic(type_.isOrganic())
        .setUnitSize(type_.size())
        //normal characteristics below..
        .setPosition(chosen_pos)
        .setHealth(health_)
        .setShields(shields_)
        .setFlying(is_flying_)
        .setElevation(elevation_)
        .setAttackerCount(units_inside_object)
        .setArmorUpgrades(armor_upgrades)
        .setAttackUpgrades(gun_upgrades)
        .setShieldUpgrades(shield_upgrades)
        .setSpeedUpgrade(speed_tech)
        .setAttackSpeedUpgrade(attack_speed_upgrade)
        .setAttackCooldownRemaining(cd_remaining_)
        .setStimmed(stimmed_)
        .setRangeUpgrade(range_upgrade)
        ;
}

auto StoredUnit::convertToFAPflying(const Position & chosen_pos, const Research_Inventory &ri) {
    int armor_upgrades = ri.upgrades_.at(type_.armorUpgrade()) + 2 * (type_ == UnitTypes::Zerg_Ultralisk * ri.upgrades_.at(UpgradeTypes::Chitinous_Plating));

    int gun_upgrades = max(ri.upgrades_.at(type_.groundWeapon().upgradeType()), ri.upgrades_.at(type_.airWeapon().upgradeType()));
    int shield_upgrades = static_cast<int>(shields_ > 0) * ri.upgrades_.at(UpgradeTypes::Protoss_Plasma_Shields);

    bool speed_tech = // safer to hardcode this.
        (type_ == UnitTypes::Zerg_Zergling && ri.upgrades_.at(UpgradeTypes::Metabolic_Boost)) ||
        (type_ == UnitTypes::Zerg_Hydralisk && ri.upgrades_.at(UpgradeTypes::Muscular_Augments)) ||
        (type_ == UnitTypes::Zerg_Overlord && ri.upgrades_.at(UpgradeTypes::Pneumatized_Carapace)) ||
        (type_ == UnitTypes::Zerg_Ultralisk && ri.upgrades_.at(UpgradeTypes::Anabolic_Synthesis)) ||
        (type_ == UnitTypes::Protoss_Scout && ri.upgrades_.at(UpgradeTypes::Gravitic_Thrusters)) ||
        (type_ == UnitTypes::Protoss_Observer && ri.upgrades_.at(UpgradeTypes::Gravitic_Boosters)) ||
        (type_ == UnitTypes::Protoss_Zealot && ri.upgrades_.at(UpgradeTypes::Leg_Enhancements)) ||
        (type_ == UnitTypes::Terran_Vulture && ri.upgrades_.at(UpgradeTypes::Ion_Thrusters));

    bool range_upgrade = // safer to hardcode this.
        (type_ == UnitTypes::Zerg_Hydralisk && ri.upgrades_.at(UpgradeTypes::Grooved_Spines)) ||
        (type_ == UnitTypes::Protoss_Dragoon && ri.upgrades_.at(UpgradeTypes::Singularity_Charge)) ||
        (type_ == UnitTypes::Terran_Marine && ri.upgrades_.at(UpgradeTypes::U_238_Shells)) ||
        (type_ == UnitTypes::Terran_Goliath && ri.upgrades_.at(UpgradeTypes::Charon_Boosters)) ||
        (type_ == UnitTypes::Terran_Barracks && ri.upgrades_.at(UpgradeTypes::U_238_Shells));

    bool attack_speed_upgrade =  // safer to hardcode this.
        (type_ == UnitTypes::Zerg_Zergling && ri.upgrades_.at(UpgradeTypes::Adrenal_Glands));

    int units_inside_object = 2 + (type_ == UnitTypes::Protoss_Carrier) * (2 + 4 * ri.upgrades_.at(UpgradeTypes::Carrier_Capacity)); // 2 if bunker, 4 if carrier, 8 if "carrier capacity" is present.

    return FAP::makeUnit<StoredUnit*>()
        .setData(this)
        .setUnitType(type_)
        .setPosition(chosen_pos)
        .setHealth(health_)
        .setShields(shields_)
        .setFlying(true)
        .setElevation(elevation_)
        .setAttackerCount(units_inside_object)
        .setArmorUpgrades(armor_upgrades)
        .setAttackUpgrades(gun_upgrades)
        .setShieldUpgrades(shield_upgrades)
        .setSpeedUpgrade(speed_tech)
        .setAttackSpeedUpgrade(attack_speed_upgrade)
        .setAttackCooldownRemaining(cd_remaining_)
        .setStimmed(stimmed_)
        .setRangeUpgrade(range_upgrade)
        ;
}

void StoredUnit::updateFAPvalue(FAP::FAPUnit<StoredUnit*> &fap_unit)
{

    double proportion_health = (fap_unit.health + fap_unit.shields) / static_cast<double>(fap_unit.maxHealth + fap_unit.maxShields);
    fap_unit.data->future_fap_value_ = static_cast<int>(fap_unit.data->stock_value_ * proportion_health);

    fap_unit.data->updated_fap_this_frame_ = true;
}

void StoredUnit::updateFAPvalueDead()
{
    future_fap_value_ = 0;
    updated_fap_this_frame_ = true;
}

bool StoredUnit::unitDeadInFuture(const StoredUnit &unit, const int &number_of_frames_voted_death) {
    return unit.count_of_consecutive_predicted_deaths_ >= number_of_frames_voted_death;
}


void Unit_Inventory::addToFAPatPos(FAP::FastAPproximation<StoredUnit*> &fap_object, const Position pos, const bool friendly, const Research_Inventory &ri) {
    for (auto &u : unit_map_) {
        if (friendly) fap_object.addIfCombatUnitPlayer1(u.second.convertToFAPPosition(pos, ri));
        else fap_object.addIfCombatUnitPlayer2(u.second.convertToFAPPosition(pos, ri));
    }
}

void Unit_Inventory::addDisabledToFAPatPos(FAP::FastAPproximation<StoredUnit*> &fap_object, const Position pos, const bool friendly, const Research_Inventory &ri) {
    for (auto &u : unit_map_) {
        if (friendly) fap_object.addIfCombatUnitPlayer1(u.second.convertToFAPDisabled(pos, ri));
        else fap_object.addIfCombatUnitPlayer2(u.second.convertToFAPDisabled(pos, ri));
    }
}

void Unit_Inventory::addAntiAirToFAPatPos(FAP::FastAPproximation<StoredUnit*> &fap_object, const Position pos, const bool friendly, const Research_Inventory &ri) {
    for (auto &u : unit_map_) {
        if (friendly) fap_object.addIfCombatUnitPlayer1(u.second.convertToFAPAnitAir(pos, ri));
        else fap_object.addIfCombatUnitPlayer2(u.second.convertToFAPAnitAir(pos, ri));
    }
}

void Unit_Inventory::addFlyingToFAPatPos(FAP::FastAPproximation<StoredUnit*> &fap_object, const Position pos, const bool friendly, const Research_Inventory &ri) {
    for (auto &u : unit_map_) {
        if (friendly) fap_object.addIfCombatUnitPlayer1(u.second.convertToFAPflying(pos, ri));
        else fap_object.addIfCombatUnitPlayer2(u.second.convertToFAPflying(pos, ri));
    }
}

//adds all nonretreating units to the sim. Retreating units are not simmed, eg, they are assumed dead.
void Unit_Inventory::addToMCFAP(FAP::FastAPproximation<StoredUnit*> &fap_object, const bool friendly, const Research_Inventory &ri) {
    for (auto &u : unit_map_) {
        Position pos = positionMCFAP(u.second);
        if (friendly) fap_object.addIfCombatUnitPlayer1(u.second.convertToFAPPosition(pos, ri));
        else fap_object.addIfCombatUnitPlayer2(u.second.convertToFAPPosition(pos, ri));
    }
}

// we no longer build sim against their buildings.
void Unit_Inventory::addToBuildFAP(FAP::FastAPproximation<StoredUnit*> &fap_object, const bool friendly, const Research_Inventory &ri, const UpgradeType &upgrade) {
    for (auto &u : unit_map_) {
        Position pos = positionBuildFap(friendly);
        if (friendly && !u.second.type_.isBuilding()) fap_object.addIfCombatUnitPlayer1(u.second.convertToFAPPosition(pos, ri, upgrade));
        else if (!u.second.type_.isBuilding()) fap_object.addIfCombatUnitPlayer2(u.second.convertToFAPPosition(pos, ri)); // they don't get the benifits of my upgrade tests.
    }

    // These units are sometimes NAN.
    //if (friendly) {
    //    fap_object.addUnitPlayer1(StoredUnit(Broodwar->self()->getRace().getResourceDepot()).convertToFAPDisabled(Position{ 240,240 }, ri));
    //    for (auto i = 0; i <= 5; i++) {
    //        fap_object.addUnitPlayer1(StoredUnit(Broodwar->self()->getRace().getSupplyProvider()).convertToFAPDisabled(Position{ 240,240 }, ri));
    //    }
    //    for (auto i = 0; i <= 5; i++) {
    //        fap_object.addUnitPlayer1(StoredUnit(UnitTypes::Zerg_Overlord).convertToFAPDisabled(Position{ 240,240 }, ri));
    //    }
    //}
    //else {
    //    fap_object.addUnitPlayer2(StoredUnit(UnitTypes::Protoss_Nexus).convertToFAPDisabled(Position{ 0, 0 }, ri));
    //    for (auto i = 0; i <= 5; i++) {
    //        fap_object.addUnitPlayer2(StoredUnit(UnitTypes::Terran_Supply_Depot).convertToFAPDisabled(Position{ 0, 0 }, ri));
    //    }
    //    for (auto i = 0; i <= 5; i++) {
    //        fap_object.addUnitPlayer2(StoredUnit(UnitTypes::Zerg_Overlord).convertToFAPDisabled(Position{ 240,240 }, ri));
    //    }
    //}
}

//This call seems very inelgant. Check if it can be made better.
void Unit_Inventory::pullFromFAP(vector<FAP::FAPUnit<StoredUnit*>> &fap_vector)
{
    for (auto &u : unit_map_) {
        u.second.updated_fap_this_frame_ = false;
    }

    for (auto &fu : fap_vector) {
        if (fu.data) {
            StoredUnit::updateFAPvalue(fu);
        }
    }

    for (auto &u : unit_map_) {
        if (!u.second.updated_fap_this_frame_) { u.second.updateFAPvalueDead(); }
    }

}

StoredUnit* Unit_Inventory::getStoredUnit(const Unit & unit)
{
    auto& find_result = unit_map_.find(unit);
    if (find_result != unit_map_.end()) return &find_result->second;
    else return nullptr;
}

StoredUnit Unit_Inventory::getStoredUnitValue(const Unit & unit) const
{
    auto& find_result = unit_map_.find(unit);
    if (find_result != unit_map_.end()) return find_result->second;
    else return nullptr;
}


