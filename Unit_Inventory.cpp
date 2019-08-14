#pragma once

#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\Unit_Inventory.h"
#include "Source\Map_Inventory.h"
#include "Source\Reservation_Manager.h"
#include "Source\FAP\FAP\include\FAP.hpp" // could add to include path but this is more explicit.
#include <random> // C++ base random is low quality.
#include <fstream>

//Unit_Inventory functions.

std::default_random_engine Unit_Inventory::generator_;  //Will be used to obtain a seed for the random number engine


//Creates an instance of the unit inventory class.
Unit_Inventory::Unit_Inventory(){}

Unit_Inventory::Unit_Inventory( const Unitset &unit_set) {

    for (const auto & u : unit_set) {
        unit_map_.insert({ u, Stored_Unit(u) });
    }

    updateUnitInventorySummary(); //this call is a CPU sink.
}

void Unit_Inventory::updateUnitInventory(const Unitset &unit_set){
    for (const auto & u : unit_set) {
        Stored_Unit* found_unit = getStoredUnit(u);
        if (found_unit) {
            found_unit->updateStoredUnit(u); // explicitly does not change locked mineral.
        }
        else {
            unit_map_.insert({ u, Stored_Unit(u) });
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

        if(e.second.time_since_last_dmg_ > 6) e.second.circumference_remaining_ = e.second.circumference_; //Every 6 frames, give it back its circumfrance. This may occasionally lead to the unit being considered surrounded/unsurrounded incorrectly.  Tracking every single target and updating is not yet implemented but could be eventually.

        if ((e.second.type_ == UnitTypes::Resource_Vespene_Geyser) || e.second.type_ == UnitTypes::Unknown ) { // Destroyed refineries revert to geyers, requiring the manual catch. Unknowns should be removed as well.
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
        Stored_Unit& miner = found_object->second;

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
        miner.phase_ = Stored_Unit::None;
        miner.updateStoredUnit(unit);
    }
    else {
        CUNYAIModule::DiagnosticText("Failed to purge worker in inventory.");
    }
}

// Decrements all resources worker was attached to, clears all reservations associated with that worker. Stops Unit.
void Unit_Inventory::purgeWorkerRelationsNoStop(const Unit &unit)
{
    UnitCommand command = unit->getLastCommand();
    auto found_object = this->unit_map_.find(unit);
    if (found_object != this->unit_map_.end()) {
        Stored_Unit& miner = found_object->second;

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
        miner.phase_ = Stored_Unit::None;
        miner.updateStoredUnit(unit);
    }
    else {
        CUNYAIModule::DiagnosticText("Failed to purge worker in inventory.");
    }
}

// Decrements all resources worker was attached to, clears all reservations associated with that worker. Stops Unit.
void Unit_Inventory::purgeWorkerRelationsOnly(const Unit &unit, Resource_Inventory &ri, Map_Inventory &inv, Reservation &res)
{
    UnitCommand command = unit->getLastCommand();
    auto found_object = this->unit_map_.find(unit);
    if (found_object != this->unit_map_.end()) {
        Stored_Unit& miner = found_object->second;

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
        CUNYAIModule::DiagnosticText("Failed to purge worker in inventory.");
    }
}

void Unit_Inventory::drawAllVelocities() const
{
    for (auto u : unit_map_) {
        Position destination = Position(u.second.pos_.x + u.second.velocity_x_ * 24, u.second.pos_.y + u.second.velocity_y_ * 24);
        CUNYAIModule::Diagnostic_Line(u.second.pos_, destination, CUNYAIModule::current_map_inventory.screen_position_, Colors::Green);
    }
}

void Unit_Inventory::drawAllHitPoints() const
{
    for (auto u : unit_map_) {
        CUNYAIModule::DiagnosticHitPoints(u.second, CUNYAIModule::current_map_inventory.screen_position_);
    }

}
void Unit_Inventory::drawAllMAFAPaverages() const
{
    for (auto u : unit_map_) {
        CUNYAIModule::DiagnosticFAP(u.second, CUNYAIModule::current_map_inventory.screen_position_);
    }

}

void Unit_Inventory::drawAllFutureDeaths() const
{
    for (auto u : unit_map_) {
        CUNYAIModule::DiagnosticDeath(u.second, CUNYAIModule::current_map_inventory.screen_position_);
    }

}

void Unit_Inventory::drawAllLastDamage() const
{
    for (auto u : unit_map_) {
        CUNYAIModule::DiagnosticLastDamage(u.second, CUNYAIModule::current_map_inventory.screen_position_);
    }

}


void Unit_Inventory::drawAllSpamGuards() const
{
    for (auto u : unit_map_) {
        CUNYAIModule::DiagnosticSpamGuard(u.second, CUNYAIModule::current_map_inventory.screen_position_);
    }
}

void Unit_Inventory::drawAllWorkerTasks() const
{
    for (auto u : unit_map_) {
        if (u.second.type_ == UnitTypes::Zerg_Drone) {
            if (u.second.locked_mine_ && !u.second.isAssignedResource() && !u.second.isAssignedClearing()) {
                CUNYAIModule::Diagnostic_Line(u.second.pos_, u.second.locked_mine_->getPosition(), CUNYAIModule::current_map_inventory.screen_position_, Colors::White);
            }
            else if (u.second.isAssignedMining()) {
                CUNYAIModule::Diagnostic_Line(u.second.pos_, u.second.locked_mine_->getPosition(), CUNYAIModule::current_map_inventory.screen_position_, Colors::Green);
            }
            else if (u.second.isAssignedGas()) {
                CUNYAIModule::Diagnostic_Line(u.second.pos_, u.second.locked_mine_->getPosition(), CUNYAIModule::current_map_inventory.screen_position_, Colors::Brown);
            }
            else if (u.second.isAssignedClearing()) {
                CUNYAIModule::Diagnostic_Line(u.second.pos_, u.second.locked_mine_->getPosition(), CUNYAIModule::current_map_inventory.screen_position_, Colors::Blue);
            }

            if (u.second.isAssignedBuilding()) {
                CUNYAIModule::Diagnostic_Dot(u.second.pos_, CUNYAIModule::current_map_inventory.screen_position_, Colors::Purple);
            }
        }
    }
}

// Blue if invalid position (lost and can't find), red if valid.
void Unit_Inventory::drawAllLocations() const
{
    if constexpr (DRAWING_MODE) {
        for (auto e = unit_map_.begin(); e != unit_map_.end() && !unit_map_.empty(); e++) {
            if (CUNYAIModule::isOnScreen(e->second.pos_, CUNYAIModule::current_map_inventory.screen_position_)) {
                if (e->second.valid_pos_) {
                    Broodwar->drawCircleMap(e->second.pos_, (e->second.type_.dimensionUp() + e->second.type_.dimensionLeft()) / 2, Colors::Red); // Plot their last known position.
                }
                else if (!e->second.valid_pos_) {
                    Broodwar->drawCircleMap(e->second.pos_, (e->second.type_.dimensionUp() + e->second.type_.dimensionLeft()) / 2, Colors::Blue); // Plot their last known position.
                }
            }
        }
    }
}

//Marks as red if it's on a minitile that ground units should not be at.
void Unit_Inventory::drawAllMisplacedGroundUnits() const
{
    if constexpr (DRAWING_MODE) {
        for (auto e = unit_map_.begin(); e != unit_map_.end() && !unit_map_.empty(); e++) {
            if (CUNYAIModule::isOnScreen(e->second.pos_, CUNYAIModule::current_map_inventory.screen_position_) && !e->second.type_.isBuilding()) {
                if (CUNYAIModule::current_map_inventory.unwalkable_barriers_with_buildings_[WalkPosition(e->second.pos_).x][WalkPosition(e->second.pos_).y] == 1 ) {
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
void Unit_Inventory::addStored_Unit( const Unit &unit ) {
    unit_map_.insert( { unit, Stored_Unit( unit ) } );
};

void Unit_Inventory::addStored_Unit( const Stored_Unit &stored_unit ) {
    unit_map_.insert( { stored_unit.bwapi_unit_ , stored_unit } );
};

Position Unit_Inventory::positionBuildFap(bool friendly) {
    std::uniform_int_distribution<int> small_map(half_map_ * friendly, half_map_ + half_map_ * friendly);     // default values for output.
    int rand_x = small_map(generator_);
    int rand_y = small_map(generator_);
    return Position(rand_x, rand_y);
}

Position Unit_Inventory::positionMCFAP(const Stored_Unit & su) {
    std::uniform_int_distribution<int> small_noise(static_cast<int>(-CUNYAIModule::getProperSpeed(su.type_)) * 4, static_cast<int>(CUNYAIModule::getProperSpeed(su.type_)) * 4);     // default values for output.
    int rand_x = small_noise(generator_);
    int rand_y = small_noise(generator_);
    return Position(rand_x, rand_y) + su.pos_;
}

void Stored_Unit::updateStoredUnit(const Unit &unit){

    valid_pos_ = true;
    pos_ = unit->getPosition();
    build_type_ = unit->getBuildType();
    if (unit->getShields() + unit->getHitPoints() < shields_ + health_) time_since_last_dmg_ = 0;
    else time_since_last_dmg_ = time_since_last_dmg_++;

    shields_ = unit->getShields();
    health_ = unit->getHitPoints();
    current_hp_ = shields_ + health_;
    velocity_x_ = static_cast<int>(round(unit->getVelocityX()));
    velocity_y_ = static_cast<int>(round(unit->getVelocityY()));
    order_ = unit->getOrder();
    command_ = unit->getLastCommand();
    areaID_ = BWEM::Map::Instance().GetNearestArea(unit->getTilePosition())->Id();
    time_since_last_command_ = Broodwar->getFrameCount() - unit->getLastCommandFrame();

    if (unit->isVisible()) time_since_last_seen_ = 0;
    else time_since_last_seen_++;

    //Needed for FAP.
    is_flying_ = unit->isFlying();
    elevation_ = BWAPI::Broodwar->getGroundHeight(TilePosition(pos_));
    cd_remaining_ = unit->getAirWeaponCooldown();
    stimmed_ = unit->isStimmed();
    burrowed_ = unit->isBurrowed();
    detected_ = unit->isDetected(); // detected doesn't work for personal units, only enemy units.
    if (type_ != unit->getType()) {
        type_ = unit->getType();
        Stored_Unit shell = Stored_Unit(type_);
        stock_value_ = shell.stock_value_; // longer but prevents retyping.
        circumference_ = shell.circumference_;
        circumference_remaining_ = shell.circumference_;
        future_fap_value_ = shell.stock_value_; //Updated in updateFAPvalue(), this is simply a natural placeholder.
        current_stock_value_ = static_cast<int>(stock_value_ * current_hp_ / static_cast<double>(type_.maxHitPoints() + type_.maxShields()));
        ma_future_fap_value_ = shell.stock_value_;
        count_of_consecutive_predicted_deaths_ = 0;
    }
    else {
        //bool unit_fighting = type_.canAttack() && phase_ == Stored_Unit::Attacking"; //&& !(burrowed_ && type_ == UnitTypes::Zerg_Lurker && time_since_last_dmg_ > 24); // detected doesn't work for personal units, only enemy units.
        bool unit_escaped = burrowed_ && time_since_last_dmg_ > FAP_SIM_DURATION; // can't still be getting shot if we're setting its assesment to 0.
        bool overkilled = (count_of_consecutive_predicted_deaths_ > FAP_SIM_DURATION && time_since_last_dmg_ > FAP_SIM_DURATION) || !type_.canAttack(); // ad - hoc resetting idea.
        circumference_remaining_ = circumference_;
        current_stock_value_ = static_cast<int>(stock_value_ * current_hp_ / static_cast<double>(type_.maxHitPoints() + type_.maxShields()));
 
        //double weight = (MOVING_AVERAGE_DURATION - 1) / static_cast<double>(MOVING_AVERAGE_DURATION); // exponential moving average?
        //if(unit->getPlayer() == Broodwar->self()) ma_future_fap_value_ = retreating_undetected ? current_stock_value_ : static_cast<int>(weight * ma_future_fap_value_ + (1.0 - weight) * future_fap_value_); // exponential moving average?
        if (unit->getPlayer() == Broodwar->self()) {
            ma_future_fap_value_ = unit_escaped ? current_stock_value_ : static_cast<int>(((FAP_SIM_DURATION - 1) * ma_future_fap_value_ + future_fap_value_) / FAP_SIM_DURATION); // normal moving average.
            if (future_fap_value_ > 0 || unit_escaped ) count_of_consecutive_predicted_deaths_ = 0;
            else count_of_consecutive_predicted_deaths_++;
        }
        else {
            ma_future_fap_value_ = overkilled ? current_stock_value_ : static_cast<int>(((FAP_SIM_DURATION - 1) * ma_future_fap_value_ + future_fap_value_) / FAP_SIM_DURATION); // enemy units ought to be simply treated as their simulated value. Otherwise repeated exposure "drains" them and cannot restore them when they are "out of combat" and the MA_FAP sim gets out of touch with the game state.
            if (future_fap_value_ > 0 ) count_of_consecutive_predicted_deaths_ = 0;
            else count_of_consecutive_predicted_deaths_++;
        }
    }
    if ( (phase_ == Stored_Unit::Upgrading || phase_ == Stored_Unit::Researching || (phase_ == Stored_Unit::Building && unit->isCompleted() && type_.isBuilding()) ) && unit->isIdle()) phase_ = Stored_Unit::None; // adjust units that are no longer upgrading.

}

//Removes units that have died
void Unit_Inventory::removeStored_Unit( Unit e_unit ) {
    unit_map_.erase( e_unit );
};


 Position Unit_Inventory::getMeanLocation() const {
    int x_sum = 0;
    int y_sum = 0;
    int count = 0;
    Position out = Position(0,0);
    for ( const auto &u : this->unit_map_ ) {
        if (u.second.valid_pos_) {
            x_sum += u.second.pos_.x;
            y_sum += u.second.pos_.y;
            count++;
        }
    }
    if ( count > 0 ) {
        out = Position( x_sum / count, y_sum / count );
    }
    return out;
}

 Position Unit_Inventory::getMeanBuildingLocation() const {
     int x_sum = 0;
     int y_sum = 0;
     int count = 0;
     for ( const auto &u : this->unit_map_ ) {
         if ( ( ( (u.second.type_.isBuilding() || u.second.type_.isAddon()) && !u.second.type_.isSpecialBuilding()) || u.second.bwapi_unit_->isMorphing() ) && u.second.valid_pos_ ) {
             x_sum += u.second.pos_.x;
             y_sum += u.second.pos_.y;
             count++;
         }
     }
     if ( count > 0 ) {
         Position out = { x_sum / count, y_sum / count };
         return out;
     }
     else {
         return Position( 0, 0 ); // you're dead at this point, fyi.
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
     int standard_value = Stored_Unit(UnitTypes::Zerg_Drone).stock_value_;
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
     for ( const auto &u : this->unit_map_) {
         if ( u.second.type_.canAttack() && u.second.valid_pos_ ) {
             x_sum += u.second.pos_.x;
             y_sum += u.second.pos_.y;
             count++;
         }
     }
     if ( count > 0 ) {
         Position out = { x_sum / count, y_sum / count };
         return out;
     }
     else {
         return Position( 0, 0 );  // you might be dead at this point, fyi.
     }

 }

 //for the army that can actually move.
 Position Unit_Inventory::getMeanArmyLocation() const {
     int x_sum = 0;
     int y_sum = 0;
     int count = 0;
     for (const auto &u : this->unit_map_) {
         if (u.second.type_.canAttack() && u.second.valid_pos_ && u.second.type_.canMove() && !u.second.type_.isWorker() ) {
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

 Unit_Inventory Unit_Inventory::getInventoryAtArea(const int areaID ) const {
     Unit_Inventory return_inventory;
     for (const auto &u : this->unit_map_) {
         if (u.second.areaID_ == areaID) { return_inventory.addStored_Unit(u.second); }
     }
     return return_inventory;
 }

 Unit_Inventory Unit_Inventory::getCombatInventoryAtArea(const int areaID) const {
     Unit_Inventory return_inventory;
     for (const auto &u : this->unit_map_) {
         if (u.second.areaID_ == areaID && CUNYAIModule::isFightingUnit(u.second)) { return_inventory.addStored_Unit(u.second); }
     }
     return return_inventory;
 }

 Unit_Inventory Unit_Inventory::getBuildingInventoryAtArea(const int areaID) const {
     Unit_Inventory return_inventory;
     for (const auto &u : this->unit_map_) {
         if (u.second.areaID_ == areaID && u.second.type_.isBuilding()) { return_inventory.addStored_Unit(u.second); }
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

     for (map<Unit,Stored_Unit>::const_iterator& it = rhs.unit_map_.begin(); it != rhs.unit_map_.end();) {
         if (total.unit_map_.find(it->first) != total.unit_map_.end() ) {
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
     stock_fliers_ = stock_ground_units_ = stock_both_up_and_down_ = stock_shoots_up_ = stock_shoots_down_ = stock_high_ground_ = stock_fighting_total_ = stock_ground_fodder_ = stock_air_fodder_ = stock_total_ = total_supply_ = max_range_ = max_cooldown_ = worker_count_ = volume_ = detector_count_ = cloaker_count_ = flyer_count_ = resource_depot_count_ = future_fap_stock_ = moving_average_fap_stock_ = stock_full_health_ = is_shooting_ = is_attacking_ = is_retreating_ = 0;

     int fliers = 0;
     int flyer_count = 0;
     int ground_unit = 0;
     int shoots_up = 0;
     int shoots_down = 0;
     int shoots_both = 0;
     int high_ground = 0;
     int range = 0;
     int worker_count = 0;
     int volume = 0;
     int detector_count = 0;
     int cloaker_count = 0;
     int max_cooldown = 0;
     int ground_fodder = 0;
     int air_fodder = 0;
     int resource_depots = 0;
     int psion_stock = 0;
     int future_fap_stock = 0;
     int moving_average_fap_stock = 0;
     int stock_full_health = 0;
     int is_shooting = 0;
     int supply = 0;

     vector<UnitType> already_seen_types;

     for (auto const & u_iter : unit_map_) { // should only search through unit types not per unit.

         future_fap_stock += u_iter.second.future_fap_value_;
         moving_average_fap_stock += u_iter.second.ma_future_fap_value_;
         is_shooting += u_iter.second.cd_remaining_ > 0; 
         supply += u_iter.second.type_.supplyRequired();
         count_of_each_phase_.at(u_iter.second.phase_)++;


         if (find(already_seen_types.begin(), already_seen_types.end(), u_iter.second.type_) == already_seen_types.end()) { // if you haven't already checked this unit type.

             bool flying_unit = u_iter.second.type_.isFlyer();
             int unit_value_for_all_of_type = CUNYAIModule::Stock_Units(u_iter.second.type_, *this);
             int count_of_unit_type = CUNYAIModule::Count_Units(u_iter.second.type_, *this);

             if (CUNYAIModule::isFightingUnit(u_iter.second)) {

                 bool up_gun = u_iter.second.type_.airWeapon() != WeaponTypes::None || u_iter.second.type_ == UnitTypes::Terran_Bunker;
                 bool down_gun = u_iter.second.type_.groundWeapon() != WeaponTypes::None || u_iter.second.type_ == UnitTypes::Terran_Bunker;
                 bool cloaker = u_iter.second.type_.isCloakable() || u_iter.second.type_ == UnitTypes::Zerg_Lurker || u_iter.second.type_.hasPermanentCloak();
                 int range_temp = (bool)(u_iter.second.bwapi_unit_) * CUNYAIModule::getProperRange(u_iter.second.type_, u_iter.second.bwapi_unit_->getPlayer()) + !(bool)(u_iter.second.bwapi_unit_) * CUNYAIModule::getProperRange(u_iter.second.type_, Broodwar->enemy());

                 fliers += flying_unit * unit_value_for_all_of_type; // add the value of that type of unit to the flier stock.
                 flyer_count = flying_unit * count_of_unit_type;
                 ground_unit += !flying_unit * unit_value_for_all_of_type;
                 shoots_up += up_gun * unit_value_for_all_of_type;
                 shoots_down += down_gun * unit_value_for_all_of_type;
                 shoots_both += (up_gun && down_gun) * unit_value_for_all_of_type;
                 cloaker_count += cloaker * count_of_unit_type;
                 psion_stock += (u_iter.second.type_.maxEnergy() > 0 * unit_value_for_all_of_type);

                 max_cooldown = max(max(u_iter.second.type_.groundWeapon().damageCooldown(), u_iter.second.type_.airWeapon().damageCooldown()), max_cooldown);
                 range = (range_temp > range) * range_temp + !(range_temp > range) * range;

             }
             else {
                 resource_depots += u_iter.second.type_.isResourceDepot() * count_of_unit_type;
                 air_fodder += flying_unit * unit_value_for_all_of_type; // add the value of that type of unit to the flier stock.
                 ground_fodder += !flying_unit * unit_value_for_all_of_type;

             }

             detector_count += u_iter.second.type_.isDetector() * count_of_unit_type;
             stock_full_health += u_iter.second.stock_value_ * count_of_unit_type;
             volume += !flying_unit * u_iter.second.type_.height()*u_iter.second.type_.width() * count_of_unit_type;
             //Region r = Broodwar->getRegionAt( u_iter.second.pos_ );
             //         if ( r && u_iter.second.valid_pos_ && u_iter.second.type_ != UnitTypes::Buildings ) {
             //             if ( r->isHigherGround() || r->getDefensePriority() > 1 ) {
             //                 high_ground += u_iter.second.current_stock_value_;
             //             }
             //         }
             already_seen_types.push_back(u_iter.second.type_);
         }
     }


    worker_count = CUNYAIModule::Count_Units(UnitTypes::Zerg_Drone, *this) + CUNYAIModule::Count_Units(UnitTypes::Protoss_Probe, *this) + CUNYAIModule::Count_Units(UnitTypes::Terran_SCV, *this);

    stock_fliers_ = fliers;
    stock_ground_units_ = ground_unit;
    stock_both_up_and_down_ = shoots_both;
    stock_shoots_up_ = shoots_up;
    stock_shoots_down_ = shoots_down;
    stock_high_ground_= high_ground;
    stock_fighting_total_ = stock_ground_units_ + stock_fliers_;
    stock_ground_fodder_ = ground_fodder;
    stock_air_fodder_ = air_fodder;
    stock_total_ = stock_fighting_total_ + stock_ground_fodder_ + stock_air_fodder_;
    stock_psion_ = psion_stock;
    total_supply_ = supply;
    max_range_ = range;
    max_cooldown_ = max_cooldown;
    worker_count_ = worker_count;
    volume_ = volume;
    detector_count_ = detector_count;
    cloaker_count_ = cloaker_count;
    flyer_count_ = flyer_count;
    resource_depot_count_ = resource_depots;
    future_fap_stock_ = future_fap_stock;
    moving_average_fap_stock_ = moving_average_fap_stock;
    stock_full_health_ = stock_full_health;
    is_shooting_ = is_shooting;
}

void Unit_Inventory::stopMine(Unit u) {
    if (u->getType().isWorker()) {
        Stored_Unit& miner = unit_map_.find(u)->second;
        miner.stopMine();
    }
}

//Stored_Unit functions.
Stored_Unit::Stored_Unit() = default;

//returns a steryotypical unit only.
Stored_Unit::Stored_Unit(const UnitType &unittype) {
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

    shoots_down_ = unittype.groundWeapon() != WeaponTypes::None;
    shoots_up_ = unittype.airWeapon() != WeaponTypes::None;

    //Get unit's status. Precalculated, precached.
    modified_supply_ = unittype.supplyRequired();
    modified_min_cost_ = unittype.mineralPrice();
    modified_gas_cost_ = unittype.gasPrice();

    if ((unittype.getRace() == Races::Zerg && unittype.isBuilding()) || unittype == UnitTypes::Terran_Bunker) {
        modified_supply_ += 2;
        modified_min_cost_ += 50;
    }  // Zerg units cost a supply (2, technically since BW cuts it in half.) // Assume bunkers are loaded with 1 marine

    if (unittype == UnitTypes::Zerg_Sunken_Colony || unittype == UnitTypes::Zerg_Spore_Colony ) {
        modified_min_cost_ += UnitTypes::Zerg_Creep_Colony.mineralPrice();
    }

    if (unittype == UnitTypes::Zerg_Lurker) {
        modified_min_cost_ += UnitTypes::Zerg_Hydralisk.mineralPrice();
        modified_gas_cost_ += UnitTypes::Zerg_Hydralisk.gasPrice();
        modified_supply_ += UnitTypes::Zerg_Hydralisk.supplyRequired();
    }

    if (unittype == UnitTypes::Zerg_Devourer || unittype == UnitTypes::Zerg_Guardian ) {
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
        modified_min_cost_ += UnitTypes::Protoss_Interceptor.mineralPrice() * (4 + 4 * (bool)CUNYAIModule::enemy_player_model.researches_.upgrades_.at(UpgradeTypes::Carrier_Capacity));
        modified_gas_cost_ += UnitTypes::Protoss_Interceptor.gasPrice() * (4 + 4 * (bool)CUNYAIModule::enemy_player_model.researches_.upgrades_.at(UpgradeTypes::Carrier_Capacity));
    }

    if (unittype == UnitTypes::Protoss_Reaver) { // Assume Reavers are loaded with 5 scarabs unless upgraded
        modified_min_cost_ += BWAPI::UnitTypes::Protoss_Scarab.mineralPrice() * (5 + 5 * (bool)CUNYAIModule::enemy_player_model.researches_.upgrades_.at(UpgradeTypes::Reaver_Capacity));
        modified_gas_cost_ += BWAPI::UnitTypes::Protoss_Scarab.gasPrice() * (5 + 5 * (bool)CUNYAIModule::enemy_player_model.researches_.upgrades_.at(UpgradeTypes::Reaver_Capacity));
    }

    if (unittype == UnitTypes::Protoss_Interceptor || unittype.isSpell()) {
        modified_min_cost_ = 0;
        modified_gas_cost_ = 0;
        modified_supply_ = 0;
    }

    modified_supply_ /= (1 + static_cast<int>(unittype.isTwoUnitsInOneEgg())); // Lings return 1 supply when they should only return 0.5

    stock_value_ = static_cast<int>(modified_min_cost_ + 1.25 * modified_gas_cost_ + 25 * modified_supply_);

    stock_value_ /= (1 + static_cast<int>(unittype.isTwoUnitsInOneEgg())); // condensed /2 into one line to avoid if-branch prediction.

    current_stock_value_ = stock_value_; // Precalculated, precached.
    future_fap_value_ = stock_value_;
    ma_future_fap_value_ = stock_value_;
};

// We must be able to create Stored_Unit objects as well.
Stored_Unit::Stored_Unit( const Unit &unit ) {
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
    time_since_last_dmg_ = 0;
    count_of_consecutive_predicted_deaths_ = 0;
    circumference_ = type_.height() * 2 + type_.width() * 2;
    circumference_remaining_ = circumference_;

    shoots_down_ = type_.groundWeapon() != WeaponTypes::None;
    shoots_up_ = type_.airWeapon() != WeaponTypes::None;

    //Needed for FAP.
        is_flying_ = unit->isFlying();
        elevation_ = BWAPI::Broodwar->getGroundHeight(TilePosition(pos_));
        cd_remaining_ = unit->getAirWeaponCooldown();
        stimmed_ = unit->isStimmed();

    //Get unit's status. Precalculated, precached.
    Stored_Unit shell = Stored_Unit(type_);
        modified_min_cost_ = shell.modified_min_cost_;
        modified_gas_cost_ = shell.modified_gas_cost_;
        modified_supply_ = shell.modified_supply_;
        stock_value_ = shell.stock_value_; //prevents retyping.
        ma_future_fap_value_ = shell.stock_value_;
        future_fap_value_ = shell.stock_value_;

    current_stock_value_ = static_cast<int>(stock_value_ * current_hp_ / static_cast<double>( type_.maxHitPoints() + type_.maxShields() ) ); // Precalculated, precached.

}


//Increments the number of miners on a resource.
void Stored_Unit::startMine(Stored_Resource &new_resource){
    locked_mine_ = new_resource.bwapi_unit_;
    CUNYAIModule::land_inventory.resource_inventory_.find(locked_mine_)->second.number_of_miners_++;
}
//Increments the number of miners on a resource.
void Stored_Unit::startMine(Unit &new_resource) {
    locked_mine_ = new_resource;
    CUNYAIModule::land_inventory.resource_inventory_.find(locked_mine_)->second.number_of_miners_++;
}

//Decrements the number of miners on a resource, if possible.
void Stored_Unit::stopMine(){
    if (locked_mine_){
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
Stored_Resource* Stored_Unit::getMine() {
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
bool Stored_Unit::isAssignedClearing() {
    if ( locked_mine_ ) {
        if (Stored_Resource* mine_of_choice = this->getMine()) { // if it has an associated mine.
            return mine_of_choice->blocking_mineral_;
        }
    }
    return false;
}

//checks if mine started with less than 8 resource
bool Stored_Unit::isAssignedLongDistanceMining() {
    if (locked_mine_) {
        if (Stored_Resource* mine_of_choice = this->getMine()) { // if it has an associated mine.
            return !mine_of_choice->blocking_mineral_ && !mine_of_choice->occupied_resource_;
        }
    }
    return false;
}

//checks if worker is assigned to a mine that started with more than 8 resources (it is a proper mine).
bool Stored_Unit::isAssignedMining() {
    if (locked_mine_) {
        if (CUNYAIModule::land_inventory.resource_inventory_.find(locked_mine_) != CUNYAIModule::land_inventory.resource_inventory_.end()) {
            Stored_Resource* mine_of_choice = this->getMine();
            return !mine_of_choice->blocking_mineral_ && mine_of_choice->type_.isMineralField();
        }
    }
    return false;
}

bool Stored_Unit::isAssignedGas() {
    if (locked_mine_) {
        if (CUNYAIModule::land_inventory.resource_inventory_.find(locked_mine_) != CUNYAIModule::land_inventory.resource_inventory_.end()) {
            Stored_Resource* mine_of_choice = this->getMine();
            return mine_of_choice->type_.isRefinery();
        }
    }
    return false;
}

bool Stored_Unit::isAssignedResource() {

    return Stored_Unit::isAssignedMining() || Stored_Unit::isAssignedGas();

}

// Warning- depends on unit being updated.
bool Stored_Unit::isAssignedBuilding() {
    this->updateStoredUnit(this->bwapi_unit_); // unit needs to be updated to confirm this.
    bool building_sent = (build_type_.isBuilding() || order_ == Orders::Move || order_ == Orders::ZergBuildingMorph || command_.getType() == UnitCommandTypes::Build || command_.getType() == UnitCommandTypes::Morph) && time_since_last_command_ < 30 * 24 && !isAssignedResource();

    return building_sent;
}

//if the miner is not doing any thing
bool Stored_Unit::isNoLock(){
    this->updateStoredUnit(this->bwapi_unit_); // unit needs to be updated to confirm this.
    return  bwapi_unit_ && !bwapi_unit_->getOrderTarget();
}

//if the miner is not mining his target. Target must be visible.
bool Stored_Unit::isBrokenLock() {
    this->updateStoredUnit(this->bwapi_unit_); // unit needs to be updated to confirm this.
    Stored_Resource* target_mine = this->getMine(); // target mine must be visible to be broken. Otherwise it is a long range lock.
    return !(isLongRangeLock() || isLocallyLocked()); // Or its order target is not the mine, then we have a broken lock.
}

bool Stored_Unit::isLocallyLocked() {
    this->updateStoredUnit(this->bwapi_unit_); // unit needs to be updated to confirm this.
    return  locked_mine_ && bwapi_unit_->getOrderTarget() && bwapi_unit_->getOrderTarget() == locked_mine_; // Everything must be visible and properly assigned.
}

//prototypeing
bool Stored_Unit::isLongRangeLock() {
    this->updateStoredUnit(this->bwapi_unit_); // unit needs to be updated to confirm this.
    Stored_Resource* target_mine = this->getMine();
    return bwapi_unit_ && target_mine && target_mine->pos_ && (!Broodwar->isVisible(TilePosition(target_mine->pos_)) /*|| (target_mine->bwapi_unit_ && target_mine->bwapi_unit_->isMorphing())*/);
}

auto Stored_Unit::convertToFAP(const Research_Inventory &ri) {
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

    return FAP::makeUnit<Stored_Unit*>()
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

auto Stored_Unit::convertToFAPPosition(const Position &chosen_pos, const Research_Inventory &ri, const UpgradeType &upgrade, const TechType &tech) {

    int armor_upgrades = ri.upgrades_.at(type_.armorUpgrade()) +
        2 * (type_ == UnitTypes::Zerg_Ultralisk * ri.upgrades_.at(UpgradeTypes::Chitinous_Plating)) +
        (type_.armorUpgrade() == upgrade);

    int gun_upgrades = max(ri.upgrades_.at(type_.groundWeapon().upgradeType()) + type_.groundWeapon().upgradeType() == upgrade, ri.upgrades_.at(type_.airWeapon().upgradeType()) + type_.airWeapon().upgradeType() == upgrade) ;

    int shield_upgrades = static_cast<int>(shields_ > 0) * (ri.upgrades_.at(UpgradeTypes::Protoss_Plasma_Shields) + UpgradeTypes::Protoss_Plasma_Shields == upgrade); // No tests here.

    bool speed_tech = // safer to hardcode this.
        (type_ == UnitTypes::Zerg_Zergling      && (ri.upgrades_.at(UpgradeTypes::Metabolic_Boost)  || upgrade == UpgradeTypes::Metabolic_Boost)) ||
        (type_ == UnitTypes::Zerg_Hydralisk     && (ri.upgrades_.at(UpgradeTypes::Muscular_Augments) || upgrade == UpgradeTypes::Muscular_Augments)) ||
        (type_ == UnitTypes::Zerg_Overlord      && (ri.upgrades_.at(UpgradeTypes::Pneumatized_Carapace) || upgrade == UpgradeTypes::Pneumatized_Carapace)) ||
        (type_ == UnitTypes::Zerg_Ultralisk     && (ri.upgrades_.at(UpgradeTypes::Anabolic_Synthesis) || upgrade == UpgradeTypes::Anabolic_Synthesis)) ||
        (type_ == UnitTypes::Protoss_Scout      && (ri.upgrades_.at(UpgradeTypes::Gravitic_Thrusters) || upgrade == UpgradeTypes::Gravitic_Thrusters)) ||
        (type_ == UnitTypes::Protoss_Observer   && (ri.upgrades_.at(UpgradeTypes::Gravitic_Boosters) || upgrade == UpgradeTypes::Gravitic_Boosters)) ||
        (type_ == UnitTypes::Protoss_Zealot     && (ri.upgrades_.at(UpgradeTypes::Leg_Enhancements) || upgrade == UpgradeTypes::Leg_Enhancements)) ||
        (type_ == UnitTypes::Terran_Vulture     && (ri.upgrades_.at(UpgradeTypes::Ion_Thrusters)   || upgrade == UpgradeTypes::Ion_Thrusters));

    bool range_upgrade = // safer to hardcode this.
        (type_ == UnitTypes::Zerg_Hydralisk     && (ri.upgrades_.at(UpgradeTypes::Grooved_Spines) || upgrade == UpgradeTypes::Grooved_Spines)) ||
        (type_ == UnitTypes::Protoss_Dragoon    && (ri.upgrades_.at(UpgradeTypes::Singularity_Charge) || upgrade == UpgradeTypes::Singularity_Charge)) ||
        (type_ == UnitTypes::Terran_Marine      && (ri.upgrades_.at(UpgradeTypes::U_238_Shells) || upgrade == UpgradeTypes::U_238_Shells)) ||
        (type_ == UnitTypes::Terran_Goliath     && (ri.upgrades_.at(UpgradeTypes::Charon_Boosters) || upgrade == UpgradeTypes::Charon_Boosters)) ||
        (type_ == UnitTypes::Terran_Barracks    && (ri.upgrades_.at(UpgradeTypes::U_238_Shells) || upgrade == UpgradeTypes::U_238_Shells));

    bool attack_speed_upgrade =  // safer to hardcode this.
        (type_ == UnitTypes::Zerg_Zergling && (ri.upgrades_.at(UpgradeTypes::Adrenal_Glands) || upgrade == UpgradeTypes::Adrenal_Glands));

    int units_inside_object = 2 + (type_ == UnitTypes::Protoss_Carrier) * (2 + 4 * ri.upgrades_.at(UpgradeTypes::Carrier_Capacity)); // 2 if bunker, 4 if carrier, 8 if "carrier capacity" is present. // Needs to extend for every race. Needs to include an indicator for self.

    return FAP::makeUnit<Stored_Unit*>()
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

auto Stored_Unit::convertToFAPDisabled(const Position &chosen_pos, const Research_Inventory &ri) {

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

    return FAP::makeUnit<Stored_Unit*>()
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
auto Stored_Unit::convertToFAPAnitAir(const Position &chosen_pos, const Research_Inventory &ri) {

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

    return FAP::makeUnit<Stored_Unit*>()
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

auto Stored_Unit::convertToFAPflying(const Position & chosen_pos, const Research_Inventory &ri) {
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

    return FAP::makeUnit<Stored_Unit*>()
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

void Stored_Unit::updateFAPvalue(FAP::FAPUnit<Stored_Unit*> &fap_unit)
{

    double proportion_health = (fap_unit.health + fap_unit.shields) / static_cast<double>(fap_unit.maxHealth + fap_unit.maxShields);
    fap_unit.data->future_fap_value_ = static_cast<int>(fap_unit.data->stock_value_ * proportion_health);

    fap_unit.data->updated_fap_this_frame_ = true;
}

void Stored_Unit::updateFAPvalueDead()
{
    future_fap_value_ = 0;
    updated_fap_this_frame_ = true;
}

bool Stored_Unit::unitDeadInFuture(const Stored_Unit &unit, const int &number_of_frames_voted_death) {
    //return unit.ma_future_fap_value_ < (unit.current_stock_value_ * static_cast<double>(MOVING_AVERAGE_DURATION - number_of_frames_in_future) / static_cast<double>(MOVING_AVERAGE_DURATION)); 
    return unit.count_of_consecutive_predicted_deaths_ >= number_of_frames_voted_death;
}


void Unit_Inventory::addToFAPatPos(FAP::FastAPproximation<Stored_Unit*> &fap_object, const Position pos, const bool friendly, const Research_Inventory &ri) {
    for (auto &u : unit_map_) {
        if (friendly ) fap_object.addIfCombatUnitPlayer1(u.second.convertToFAPPosition(pos, ri));
        else fap_object.addIfCombatUnitPlayer2(u.second.convertToFAPPosition(pos, ri));
    }
}

void Unit_Inventory::addDisabledToFAPatPos(FAP::FastAPproximation<Stored_Unit*> &fap_object, const Position pos, const bool friendly, const Research_Inventory &ri) {
    for (auto &u : unit_map_) {
        if (friendly) fap_object.addIfCombatUnitPlayer1(u.second.convertToFAPDisabled(pos, ri));
        else fap_object.addIfCombatUnitPlayer2(u.second.convertToFAPDisabled(pos, ri));
    }
}

void Unit_Inventory::addAntiAirToFAPatPos(FAP::FastAPproximation<Stored_Unit*> &fap_object, const Position pos, const bool friendly, const Research_Inventory &ri) {
    for (auto &u : unit_map_) {
        if (friendly) fap_object.addIfCombatUnitPlayer1(u.second.convertToFAPAnitAir(pos, ri));
        else fap_object.addIfCombatUnitPlayer2(u.second.convertToFAPAnitAir(pos, ri));
    }
}

void Unit_Inventory::addFlyingToFAPatPos(FAP::FastAPproximation<Stored_Unit*> &fap_object, const Position pos, const bool friendly, const Research_Inventory &ri) {
    for (auto &u : unit_map_) {
        if (friendly) fap_object.addIfCombatUnitPlayer1(u.second.convertToFAPflying(pos, ri));
        else fap_object.addIfCombatUnitPlayer2(u.second.convertToFAPflying(pos, ri));
    }
}

//adds all nonretreating units to the sim. Retreating units are not simmed, eg, they are assumed dead.
void Unit_Inventory::addToMCFAP(FAP::FastAPproximation<Stored_Unit*> &fap_object, const bool friendly, const Research_Inventory &ri) {
    for (auto &u : unit_map_) {
        Position pos = positionMCFAP(u.second);
        if (friendly) fap_object.addIfCombatUnitPlayer1(u.second.convertToFAPPosition(pos, ri));
        else fap_object.addIfCombatUnitPlayer2(u.second.convertToFAPPosition(pos, ri));
    }
}

// we no longer build sim against their buildings.
void Unit_Inventory::addToBuildFAP( FAP::FastAPproximation<Stored_Unit*> &fap_object, const bool friendly, const Research_Inventory &ri, const UpgradeType &upgrade) {
    for (auto &u : unit_map_) {
        Position pos = positionBuildFap(friendly);
            if (friendly && !u.second.type_.isBuilding()) fap_object.addIfCombatUnitPlayer1(u.second.convertToFAPPosition(pos, ri, upgrade));
            else if (!u.second.type_.isBuilding()) fap_object.addIfCombatUnitPlayer2(u.second.convertToFAPPosition(pos, ri)); // they don't get the benifits of my upgrade tests.
    }

    // These units are sometimes NAN.
    //if (friendly) {
    //    fap_object.addUnitPlayer1(Stored_Unit(Broodwar->self()->getRace().getResourceDepot()).convertToFAPDisabled(Position{ 240,240 }, ri));
    //    for (auto i = 0; i <= 5; i++) {
    //        fap_object.addUnitPlayer1(Stored_Unit(Broodwar->self()->getRace().getSupplyProvider()).convertToFAPDisabled(Position{ 240,240 }, ri));
    //    }
    //    for (auto i = 0; i <= 5; i++) {
    //        fap_object.addUnitPlayer1(Stored_Unit(UnitTypes::Zerg_Overlord).convertToFAPDisabled(Position{ 240,240 }, ri));
    //    }
    //}
    //else {
    //    fap_object.addUnitPlayer2(Stored_Unit(UnitTypes::Protoss_Nexus).convertToFAPDisabled(Position{ 0, 0 }, ri));
    //    for (auto i = 0; i <= 5; i++) {
    //        fap_object.addUnitPlayer2(Stored_Unit(UnitTypes::Terran_Supply_Depot).convertToFAPDisabled(Position{ 0, 0 }, ri));
    //    }
    //    for (auto i = 0; i <= 5; i++) {
    //        fap_object.addUnitPlayer2(Stored_Unit(UnitTypes::Zerg_Overlord).convertToFAPDisabled(Position{ 240,240 }, ri));
    //    }
    //}
}

//This call seems very inelgant. Check if it can be made better.
void Unit_Inventory::pullFromFAP(vector<FAP::FAPUnit<Stored_Unit*>> &fap_vector)
{
    for (auto &u : unit_map_) {
        u.second.updated_fap_this_frame_ = false;
    }

    for (auto &fu : fap_vector) {
        if ( fu.data ) {
            Stored_Unit::updateFAPvalue(fu);
        }
    }

    for (auto &u : unit_map_) {
        if (!u.second.updated_fap_this_frame_) { u.second.updateFAPvalueDead(); }
    }

}

Stored_Unit* Unit_Inventory::getStoredUnit(const Unit & unit)
{
    auto& find_result = unit_map_.find(unit);
    if (find_result != unit_map_.end()) return &find_result->second;
    else return nullptr;
}

Stored_Unit Unit_Inventory::getStoredUnitValue(const Unit & unit) const
{
    auto& find_result = unit_map_.find(unit);
    if (find_result != unit_map_.end()) return find_result->second;
    else return nullptr;
}


