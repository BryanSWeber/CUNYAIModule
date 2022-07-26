#pragma once

#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\UnitInventory.h"
#include "Source\MapInventory.h"
#include "Source\ReservationManager.h"
#include "Source/Diagnostics.h"
#include "Source\FAP\FAP\include\FAP.hpp" // could add to include path but this is more explicit.
#include <random> // C++ base random is low quality.
#include <fstream>
#include <filesystem>

//UnitInventory functions.



//Creates an instance of the unit inventory class.
UnitInventory::UnitInventory() {}

UnitInventory::UnitInventory(const Unitset &unit_set) {

    for (const auto & u : unit_set) {
        unit_map_.insert({ u, StoredUnit(u) });
    }

    updateUnitInventorySummary(); //this call is a CPU sink.
}

void UnitInventory::updateUnitInventory(const Unitset &unit_set) {
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

void UnitInventory::updateUnitsControlledBy(const Player &player)
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
                    e.second.valid_pos_ = false;
                }
            }
            else { //If you can't see their position, then... assume they continue on their merry way.
                Position potential_running_spot = Position(e.second.pos_.x + e.second.velocity_x_, e.second.pos_.y + e.second.velocity_y_);
                if( potential_running_spot.isValid() && e.second.type_.canMove() && (e.second.type_.isFlyer() || Broodwar->isWalkable(WalkPosition(potential_running_spot))) ) // if you haven't seen them, then assume they kept moving the same direction untill they disappeared.
                    e.second.pos_ = potential_running_spot; 
                if (e.second.time_since_last_seen_ > 30 * 24 && !e.second.type_.isBuilding()) { // if you haven't seen them for 30 seconds and they're not buildings, they're gone.
                    e.second.valid_pos_ = false;
                }
            }
        }

        if (e.second.time_since_last_dmg_ > 6) e.second.circumference_remaining_ = e.second.circumference_; //Every 6 frames, give it back its circumfrance. This may occasionally lead to the unit being considered surrounded/unsurrounded incorrectly.  Tracking every single target and updating is not yet implemented but could be eventually.

        if ((e.second.type_ == UnitTypes::Resource_Vespene_Geyser) || e.second.type_ == UnitTypes::Unknown) { // Destroyed refineries revert to geyers, requiring the manual catch. Unknowns should be removed as well.
            e.second.valid_pos_ = false;
        }

    }

}

void UnitInventory::purgeBrokenUnits()
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

void UnitInventory::purgeUnseenUnits()
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

//void UnitInventory::purgeAllPhases()
//{
//    for (auto &u = this->UnitInventory_.begin(); u != this->UnitInventory_.end() && !this->UnitInventory_.empty(); ) {
//        u->second.phase_ = "None";
//    }
//}

// Decrements all resources worker was attached to, clears all reservations associated with that worker. Stops Unit.
void UnitInventory::purgeWorkerRelationsStop(const Unit &unit)
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
        }
        unit->stop();
        miner.time_of_last_purge_ = Broodwar->getFrameCount();
        miner.phase_ = StoredUnit::None;
        miner.updateStoredUnit(unit);
    }
    else {
        Diagnostics::DiagnosticWrite("Failed to purge worker in inventory.");
    }
}

// Decrements all resources worker was attached to, clears all reservations associated with that worker. Stops Unit.
void UnitInventory::purgeWorkerRelationsNoStop(const Unit &unit)
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
        }
        miner.time_of_last_purge_ = Broodwar->getFrameCount();
        miner.phase_ = StoredUnit::None;
        miner.updateStoredUnit(unit);
    }
    else {
        Diagnostics::DiagnosticWrite("Failed to purge worker in inventory.");
    }
}

// Decrements all resources worker was attached to, clears all reservations associated with that worker. Stops Unit.
void UnitInventory::purgeWorkerRelationsOnly(const Unit &unit, ResourceInventory &ri, MapInventory &inv, Reservation &res)
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
        }
        miner.time_of_last_purge_ = Broodwar->getFrameCount();
        miner.updateStoredUnit(unit);
    }
    else {
        Diagnostics::DiagnosticWrite("Failed to purge worker in inventory.");
    }
}

void UnitInventory::drawAllWorkerTasks() const
{
    for (auto u : unit_map_) {
        if (u.second.type_ == UnitTypes::Zerg_Drone) {
            if (u.second.locked_mine_ && !u.second.isAssignedResource() && !u.second.isAssignedClearing()) {
                Diagnostics::drawLine(u.second.pos_, u.second.locked_mine_->getPosition(), CUNYAIModule::currentMapInventory.screen_position_, Colors::White);
            }
            else if (u.second.isAssignedMining()) {
                Diagnostics::drawLine(u.second.pos_, u.second.locked_mine_->getPosition(), CUNYAIModule::currentMapInventory.screen_position_, Colors::Green);
            }
            else if (u.second.isAssignedGas()) {
                Diagnostics::drawLine(u.second.pos_, u.second.locked_mine_->getPosition(), CUNYAIModule::currentMapInventory.screen_position_, Colors::Brown);
            }
            else if (u.second.isAssignedClearing()) {
                Diagnostics::drawLine(u.second.pos_, u.second.locked_mine_->getPosition(), CUNYAIModule::currentMapInventory.screen_position_, Colors::Blue);
            }

            if (u.second.isAssignedBuilding()) {
                Diagnostics::drawDot(u.second.pos_, CUNYAIModule::currentMapInventory.screen_position_, Colors::Purple);
            }
        }
    }
}

// Blue if invalid position (lost and can't find), red if valid.
void UnitInventory::drawAllLocations() const
{
    for (auto e = unit_map_.begin(); e != unit_map_.end() && !unit_map_.empty(); e++) {
        if (e->second.valid_pos_) {
            Diagnostics::drawCircle(e->second.pos_, CUNYAIModule::currentMapInventory.screen_position_, (e->second.type_.dimensionUp() + e->second.type_.dimensionLeft()) / 2, Colors::Red); // Plot their last known position.
        }
        else if (!e->second.valid_pos_) {
            Diagnostics::drawCircle(e->second.pos_, CUNYAIModule::currentMapInventory.screen_position_, (e->second.type_.dimensionUp() + e->second.type_.dimensionLeft()) / 2, Colors::Blue); // Plot their last known position.
        }

    }

}

// Blue if invalid position (lost and can't find), red if valid.
void UnitInventory::drawAllLastSeens() const
{
    for (auto e = unit_map_.begin(); e != unit_map_.end() && !unit_map_.empty(); e++) {
        Diagnostics::writeMap(e->second.pos_,std::to_string(e->second.time_since_last_seen_));
    }
}

//Marks as red if it's on a minitile that ground units should not be at.
void UnitInventory::drawAllMisplacedGroundUnits() const
{
    if constexpr (DIAGNOSTIC_MODE) {
        for (auto e = unit_map_.begin(); e != unit_map_.end() && !unit_map_.empty(); e++) {
            if (CUNYAIModule::isOnScreen(e->second.pos_, CUNYAIModule::currentMapInventory.screen_position_) && !e->second.type_.isBuilding()) {
                if (CUNYAIModule::currentMapInventory.unwalkable_barriers_with_buildings_[WalkPosition(e->second.pos_).x][WalkPosition(e->second.pos_).y] == 1) {
                    Broodwar->drawCircleMap(e->second.pos_, (e->second.type_.dimensionUp() + e->second.type_.dimensionLeft()) / 2, Colors::Red, true); // Mark as RED if not in a walkable spot.
                }
                else if (CUNYAIModule::currentMapInventory.unwalkable_barriers_with_buildings_[WalkPosition(e->second.pos_).x][WalkPosition(e->second.pos_).y] == 0) {
                    Broodwar->drawCircleMap(e->second.pos_, (e->second.type_.dimensionUp() + e->second.type_.dimensionLeft()) / 2, Colors::Blue, true); // Mark as RED if not in a walkable spot.
                }
            }
        }
    }
}

// Updates the count of units.
bool UnitInventory::addStoredUnit(const Unit &unit) {
    return unit_map_.insert({ unit, StoredUnit(unit) }).second;
};

bool UnitInventory::addStoredUnit(const StoredUnit &stored_unit) {
    return unit_map_.insert({ stored_unit.bwapi_unit_ , stored_unit }).second;
};


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
            StoredUnit shell = StoredUnit(type_, (bool)CUNYAIModule::enemy_player_model.researches_.getUpLevel(UpgradeTypes::Carrier_Capacity), (bool)CUNYAIModule::enemy_player_model.researches_.getUpLevel(UpgradeTypes::Reaver_Capacity));
        }
        stock_value_ = shell.stock_value_; // longer but prevents retyping.
        circumference_ = shell.circumference_;
        circumference_remaining_ = shell.circumference_;
        future_fap_value_ = shell.stock_value_; //Updated in updateFAPvalue(), this is simply a natural placeholder.
        count_of_consecutive_predicted_deaths_ = 0;
    }
    else {
        bool unit_escaped = (burrowed_ || cloaked_) && (!detected_ || time_since_last_dmg_ < 6 ); // can't still be getting shot if we're setting its assesment to 0.
        circumference_remaining_ = circumference_;
        if (future_fap_value_ > 0 || unit_escaped) count_of_consecutive_predicted_deaths_ = 0;
        else count_of_consecutive_predicted_deaths_++;
    }

    current_stock_value_ = static_cast<int>(stock_value_ * current_hp_ / static_cast<double>(type_.maxHitPoints() + type_.maxShields()));

    if ((phase_ == StoredUnit::Upgrading || phase_ == StoredUnit::Researching || (phase_ == StoredUnit::Building && unit->isCompleted() && type_.isBuilding())) && unit->isIdle()) phase_ = StoredUnit::None; // adjust units that are no longer upgrading.

}

//Removes units that have died
void UnitInventory::removeStoredUnit(Unit e_unit) {
    unit_map_.erase(e_unit);
};


Position UnitInventory::getMeanLocation() const {
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

Position UnitInventory::getMeanBuildingLocation() const {
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

Position UnitInventory::getMeanAirLocation() const {
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
Position UnitInventory::getStrongestLocation() const {
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


Position UnitInventory::getMeanCombatLocation() const {
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
Position UnitInventory::getMeanArmyLocation() const {
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
//Position UnitInventory::getClosestMeanArmyLocation() const {
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

UnitInventory UnitInventory::getInventoryAtArea(const int areaID) const {
    UnitInventory return_inventory;
    for (const auto &u : this->unit_map_) {
        if (u.second.areaID_ == areaID) { return_inventory.addStoredUnit(u.second); }
    }
    return return_inventory;
}

UnitInventory UnitInventory::getCombatInventoryAtArea(const int areaID) const {
    UnitInventory return_inventory;
    for (const auto &u : this->unit_map_) {
        if (u.second.areaID_ == areaID && CUNYAIModule::isFightingUnit(u.second)) { return_inventory.addStoredUnit(u.second); }
    }
    return return_inventory;
}

UnitInventory UnitInventory::getBuildingInventoryAtArea(const int areaID) const {
    UnitInventory return_inventory;
    for (const auto &u : this->unit_map_) {
        if (u.second.areaID_ == areaID && u.second.type_.isBuilding()) { return_inventory.addStoredUnit(u.second); }
    }
    return return_inventory;
}

UnitInventory UnitInventory::getBuildingInventory() const {
    UnitInventory return_inventory;
    for (const auto &u : this->unit_map_) {
        if (u.second.type_.isBuilding()) { return_inventory.addStoredUnit(u.second); }
    }
    return return_inventory;
}


UnitInventory operator+(const UnitInventory& lhs, const UnitInventory& rhs)
{
    UnitInventory total = lhs;
    total.unit_map_.insert(rhs.unit_map_.begin(), rhs.unit_map_.end());
    total.updateUnitInventorySummary();
    return total;
}

UnitInventory operator-(const UnitInventory& lhs, const UnitInventory& rhs)
{
    UnitInventory total;
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

int UnitInventory::countRecentAdditions(int frames) {
    int count = 0;
    for (auto const & u_iter : unit_map_) { // should only search through unit types not per unit.
        if (Broodwar->getFrameCount() - u_iter.second.time_first_observed_ > frames)
            count++;
    }
    return count;
}


//Tally up crucial details about enemy. Should be doing this onclass. Perhaps make an enemy summary class?
void UnitInventory::updateUnitInventorySummary() {
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

void UnitInventory::printUnitInventory(const Player &player, const string &bonus)
{
    //string start = "learnedPlan.readDirectory + player->getName() + bonus + ".txt";
    //string finish = learnedPlan.writeDirectory + player->getName() + bonus + ".txt";
    //if (filesystem::exists(start.c_str()))
    //    filesystem::copy(start.c_str(), finish.c_str(), filesystem::copy_options::update_existing); // Furthermore, rename will fail if there is already an existing file.


    ifstream input; // brings in info;
    input.open(CUNYAIModule::learnedPlan.getWriteDir() + player->getName() + bonus + ".txt", ios::in);   // for each row
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
        output.open(CUNYAIModule::learnedPlan.getWriteDir() + player->getName() + bonus + ".txt", ios_base::app);
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
    output.open(CUNYAIModule::learnedPlan.getWriteDir() + player->getName() + bonus + ".txt", ios_base::app);
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
    //    string start = learnedPlan.writeDirectory + player->getName() + bonus + ".txt";
    //    string finish = learnedPlan.readDirectory + player->getName() + bonus + ".txt";
    //    if (filesystem::exists(start.c_str()))
    //        filesystem::copy(start.c_str(), finish.c_str(), filesystem::copy_options::update_existing); // Furthermore, rename will fail if there is already an existing file.
    //}
}

void UnitInventory::stopMine(Unit u) {
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
    stock_value_ = getTraditionalWeight(type_, carrierUpgrade, reaverUpgrade);
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
    is_on_island_ = CUNYAIModule::currentMapInventory.isOnIsland(pos_) && !is_flying_;

    cd_remaining_ = unit->getAirWeaponCooldown();
    stimmed_ = unit->isStimmed();

    //Get unit's status. Precalculated, precached.
    StoredUnit shell = StoredUnit(type_);
    if (unit->getPlayer()->isEnemy(Broodwar->self())) {
        StoredUnit shell = StoredUnit(type_, (bool)CUNYAIModule::enemy_player_model.researches_.getUpLevel(UpgradeTypes::Carrier_Capacity), (bool)CUNYAIModule::enemy_player_model.researches_.getUpLevel(UpgradeTypes::Reaver_Capacity));
    }
    modified_min_cost_ = shell.modified_min_cost_;
    modified_gas_cost_ = shell.modified_gas_cost_;
    modified_supply_ = shell.modified_supply_;
    stock_value_ = shell.stock_value_; //prevents retyping.
    future_fap_value_ = shell.stock_value_;
    current_stock_value_ = static_cast<int>(stock_value_ * current_hp_ / static_cast<double>(type_.maxHitPoints() + type_.maxShields())); // Precalculated, precached.

}

//Returns true if the unit is voted as dead in X of the future frames.
bool StoredUnit::unitDeadInFuture(const int &numberOfConsecutiveDeadSims) const {
    return count_of_consecutive_predicted_deaths_ >= numberOfConsecutiveDeadSims;
}

//Increments the number of miners on a resource.
void StoredUnit::startMine(Stored_Resource &new_resource) {
    locked_mine_ = new_resource.bwapi_unit_;
    CUNYAIModule::land_inventory.ResourceInventory_.find(locked_mine_)->second.number_of_miners_++;
}
//Increments the number of miners on a resource.
void StoredUnit::startMine(Unit &new_resource) {
    locked_mine_ = new_resource;
    CUNYAIModule::land_inventory.ResourceInventory_.find(locked_mine_)->second.number_of_miners_++;
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
    if (CUNYAIModule::land_inventory.ResourceInventory_.find(locked_mine_) != CUNYAIModule::land_inventory.ResourceInventory_.end()) {
        tenative_resource = &CUNYAIModule::land_inventory.ResourceInventory_.find(locked_mine_)->second;
    }
    return tenative_resource;
}

//finds mine- Will return true null if the mine DNE.
Stored_Resource* getMine(const Unit &resource) {
    Stored_Resource* tenative_resource = nullptr;
    if (CUNYAIModule::land_inventory.ResourceInventory_.find(resource) != CUNYAIModule::land_inventory.ResourceInventory_.end()) {
        tenative_resource = &CUNYAIModule::land_inventory.ResourceInventory_.find(resource)->second;
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
        if (CUNYAIModule::land_inventory.ResourceInventory_.find(locked_mine_) != CUNYAIModule::land_inventory.ResourceInventory_.end()) {
            Stored_Resource* mine_of_choice = this->getMine();
            return !mine_of_choice->blocking_mineral_ && mine_of_choice->type_.isMineralField();
        }
    }
    return false;
}

bool StoredUnit::isAssignedGas() {
    if (locked_mine_) {
        if (CUNYAIModule::land_inventory.ResourceInventory_.find(locked_mine_) != CUNYAIModule::land_inventory.ResourceInventory_.end()) {
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

void StoredUnit::updateFAPvalue(FAP::FAPUnit<StoredUnit*> fap_unit)
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

void UnitInventory::updatePredictedStatus(bool friendly)
{
    for (auto &u : unit_map_) {
        u.second.updated_fap_this_frame_ = false;
    }

    if (friendly) {
        for (auto fu : CUNYAIModule::mainCombatSim.getFriendlySim()) {
            if (fu.data) {
                StoredUnit::updateFAPvalue(fu);
            }
        }
    }
    else {
        for (auto fu : CUNYAIModule::mainCombatSim.getEnemySim()) {
            if (fu.data) {
                StoredUnit::updateFAPvalue(fu);
            }
        }
    }
}
    for (auto &u : unit_map_) {
        if (!u.second.updated_fap_this_frame_) { u.second.updateFAPvalueDead(); }
    }
    //vector<FAP::FAPUnit<StoredUnit*>> &fap_vector
}

StoredUnit* UnitInventory::getStoredUnit(const Unit & unit)
{
    auto& find_result = unit_map_.find(unit);
    if (find_result != unit_map_.end()) return &find_result->second;
    else return nullptr;
}

StoredUnit UnitInventory::getStoredUnitValue(const Unit & unit) const
{
    auto& find_result = unit_map_.find(unit);
    if (find_result != unit_map_.end()) return find_result->second;
    else return nullptr;
}


