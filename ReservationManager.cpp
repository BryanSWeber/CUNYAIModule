#pragma once

#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\MapInventory.h"
#include "Source\UnitInventory.h"
#include "Source\Resource_Inventory.h"
#include "Source\ReservationManager.h"
#include "Source/Diagnostics.h"
#include <algorithm>

using namespace std;
using namespace BWAPI;

Reservation::Reservation() {
    min_reserve_ = 0;
    gas_reserve_ = 0;
    building_timer_ = 0;
    last_builder_sent_ = 0;
}

bool Reservation::addReserveSystem(TilePosition pos, UnitType type) {
    bool safe = reservation_map_.insert({ pos, type }).second;
    if (safe) {
        min_reserve_ += type.mineralPrice();
        gas_reserve_ += type.gasPrice();
        building_timer_ = type.buildTime() > building_timer_ ? type.buildTime() : building_timer_;
        last_builder_sent_ = Broodwar->getFrameCount();
        CUNYAIModule::buildorder.updateRemainingBuildOrder(type);
    }

    return safe;
}

void Reservation::addReserveSystem(UpgradeType up)
{
    reserved_upgrades_.push_back(up);
    int level = Broodwar->self()->getUpgradeLevel(up);
    min_reserve_ += up.mineralPrice(level);
    gas_reserve_ += up.gasPrice(level);
    building_timer_ = up.upgradeTime(level) > building_timer_ ? up.upgradeTime(level) : building_timer_;
    CUNYAIModule::buildorder.updateRemainingBuildOrder(up);
}

bool Reservation::removeReserveSystem(TilePosition pos, UnitType type, bool retry_this_building = false) {
    map<TilePosition, UnitType>::iterator it = reservation_map_.find(pos);
    if (it != reservation_map_.end() && !reservation_map_.empty()) {
        if (!CUNYAIModule::buildorder.isEmptyBuildOrder() && retry_this_building) CUNYAIModule::buildorder.retryBuildOrderElement(type);
        if (it->second.mineralPrice()) min_reserve_ -= it->second.mineralPrice();
        if (it->second.gasPrice())gas_reserve_ -= it->second.gasPrice();
        return reservation_map_.erase(pos);
    }
    return false;
};

bool Reservation::removeReserveSystem(UpgradeType up, bool retry_this_upgrade) {
    auto it = find(reserved_upgrades_.begin(), reserved_upgrades_.end(), up);
    if (it != reserved_upgrades_.end() && !reserved_upgrades_.empty()) {
        if (!CUNYAIModule::buildorder.isEmptyBuildOrder() && retry_this_upgrade) CUNYAIModule::buildorder.retryBuildOrderElement(up);
        if (it->mineralPrice()) min_reserve_ -= it->mineralPrice();
        if (it->gasPrice()) gas_reserve_ -= it->gasPrice();
        reserved_upgrades_.erase(it);
        return true;
    }
    return false;
};

bool Reservation::isInReserveSystem(const UnitType & type) {
    for (auto reservation : reservation_map_) {
        if (reservation.second == type) return true;
    }
    return false;
};

bool Reservation::isInReserveSystem(const UpgradeType & up) {
    return find(reserved_upgrades_.begin(), reserved_upgrades_.end(), up) != reserved_upgrades_.end();
};

int Reservation::countInReserveSystem(const UnitType & type) {
    int count = 0;
    for (auto reservation : reservation_map_) {
        if (reservation.second == type) count++;
    }
    return count;
};

void Reservation::decrementReserveTimer() {
    if (Broodwar->getFrameCount() == 0) {
        building_timer_ = 0;
    }
    else {
        building_timer_ > 0 ? --building_timer_ : 0;
    }
}

int Reservation::getExcessMineral() {
    return max(Broodwar->self()->minerals() - min_reserve_, 0);
}

int Reservation::getExcessGas() {
    return max(Broodwar->self()->gas() - gas_reserve_, 0);
}

map<TilePosition, UnitType> Reservation::getReservedUnits() const
{
    return reservation_map_;
}

vector<UpgradeType> Reservation::getReservedUpgrades() const
{
    return reserved_upgrades_;
}

bool Reservation::checkExcessIsGreaterThan(const UnitType &type) const {
    bool okay_on_gas = Broodwar->self()->gas() - gas_reserve_ > type.gasPrice() || type.gasPrice() == 0;
    bool okay_on_minerals = Broodwar->self()->minerals() > type.mineralPrice() || type.mineralPrice() == 0;
    return okay_on_gas && okay_on_minerals;
}

bool Reservation::checkExcessIsGreaterThan(const TechType &type) const {
    return Broodwar->self()->gas() - gas_reserve_ > type.gasPrice() && Broodwar->self()->minerals() > type.mineralPrice();
}

bool Reservation::checkAffordablePurchase(const UnitType type, const int distance) {
    double bonus_frames = 48.0; //we need extra frames to make SURE we arrive early. 3 seconds?
    double extra_min = 0.046 * static_cast<double>(CUNYAIModule::workermanager.getMinWorkers()) * (static_cast<double>(distance) / (CUNYAIModule::getProperSpeed(UnitTypes::Zerg_Drone) * 0.5) + bonus_frames);
    double extra_gas = 0.069 * static_cast<double>(CUNYAIModule::workermanager.getGasWorkers()) * (static_cast<double>(distance) / (CUNYAIModule::getProperSpeed(UnitTypes::Zerg_Drone) * 0.5) + bonus_frames); // top speed overestimates drone movement heavily.

    bool min_affordable = ( static_cast<double>(Broodwar->self()->minerals()) + extra_min - static_cast<double>(min_reserve_) >= type.mineralPrice() ) || type.mineralPrice() == 0;
    bool gas_affordable = ( static_cast<double>(Broodwar->self()->gas()) + extra_gas - static_cast<double>(gas_reserve_) >= type.gasPrice() ) || type.gasPrice() == 0;
    
    bool already_making_one = false;
    for (auto it = reservation_map_.begin(); it != reservation_map_.end(); it++) {
        if (it->second == type) {
            already_making_one = true;
            break;
        }
    }
    bool open_reservation = reservation_map_.empty() || !already_making_one;
    return min_affordable && gas_affordable && open_reservation;
}

int Reservation::countTimesWeCanAffordPurchase(const UnitType type) {
    bool affordable = true;
    int i = 0;
    bool already_making_one = false;
    for (auto it = reservation_map_.begin(); it != reservation_map_.end(); it++) {
        if (it->second == type) {
            already_making_one = true;
            break;
        }
    }
    bool open_reservation = reservation_map_.empty() || !already_making_one;

    while (affordable) {
        affordable = Broodwar->self()->minerals() - i * type.mineralPrice() >= min_reserve_ && Broodwar->self()->gas() - i * type.gasPrice() >= gas_reserve_;
        if (affordable) i++;
    }
    return affordable && open_reservation;
}

bool Reservation::checkAffordablePurchase(const TechType type) {
    return Broodwar->self()->minerals() - min_reserve_ >= type.mineralPrice() && Broodwar->self()->gas() - gas_reserve_ >= type.gasPrice();
}

bool Reservation::checkAffordablePurchase(const UpgradeType type) {
    return Broodwar->self()->minerals() - min_reserve_ >= type.mineralPrice() && Broodwar->self()->gas() - gas_reserve_ >= type.gasPrice();
}

void Reservation::confirmOngoingReservations() {

    min_reserve_ = 0;
    gas_reserve_ = 0;

    for (auto res_it = reservation_map_.begin(); res_it != reservation_map_.end() && !reservation_map_.empty(); ) {
        bool keep = false;

        for (auto unit_it = CUNYAIModule::friendly_player_model.units_.unit_map_.begin(); unit_it != CUNYAIModule::friendly_player_model.units_.unit_map_.end() && !CUNYAIModule::friendly_player_model.units_.unit_map_.empty(); unit_it++) {
            StoredUnit& miner = *CUNYAIModule::friendly_player_model.units_.getStoredUnit(unit_it->first); // we will want DETAILED information about this unit.
            if (miner.intended_build_type_ == res_it->second && miner.intended_build_tile_ == res_it->first)
                keep = true;

        } // check if we have a unit building it.

        if (keep) {
            ++res_it;
        }
        else {
            Diagnostics::DiagnosticText("No worker is building the reserved %s. Freeing up the funds.", res_it->second.c_str());
            auto remove_me = res_it;
            res_it++;
            removeReserveSystem(remove_me->first, remove_me->second, true);  // contains an erase.
        }
    }

    for (auto res_it = reservation_map_.begin(); res_it != reservation_map_.end() && !reservation_map_.empty(); res_it++) {
        min_reserve_ += res_it->second.mineralPrice();
        gas_reserve_ += res_it->second.gasPrice();
    }

    for (auto res_it = reserved_upgrades_.begin(); res_it != reserved_upgrades_.end() && !reserved_upgrades_.empty(); res_it++) {
        min_reserve_ += res_it->mineralPrice();
        gas_reserve_ += res_it->gasPrice();
    }

    if (!reservation_map_.empty() && last_builder_sent_ < Broodwar->getFrameCount() - 30 * 24) {
        Diagnostics::DiagnosticText("...We're stuck, aren't we? Have a friendly nudge.", "");
        reservation_map_.clear();
        min_reserve_ = 0;
        gas_reserve_ = 0;
    }
}

RemainderTracker::RemainderTracker()
{
}

int RemainderTracker::getWaveSize(UnitType ut)
{
    if (!AssemblyManager::canMakeCUNY(ut, true))
        return 0;
    else {
        int nUnits = 0;
        while (gasRemaining_ > 0 && minRemaining_ > 0 && supplyRemaining_ > 0 && larvaeRemaining_ > 0) {
            gasRemaining_ -= ut.gasPrice();
            minRemaining_ -= ut.mineralPrice();
            supplyRemaining_--;
            supplyRemaining_ -= ut.supplyRequired();
            nUnits++;
        }
        return nUnits++;
    }
}

void RemainderTracker::getReservationCapacity()
{
    int gasRemaining_ = CUNYAIModule::my_reservation.getExcessGas();
    int minRemaining_ = CUNYAIModule::my_reservation.getExcessMineral();
    int supplyRemaining_ = Broodwar->self()->supplyTotal() - Broodwar->self()->supplyUsed();
    int larvaeRemaining_ = CUNYAIModule::countUnits(UnitTypes::Zerg_Larva);
}