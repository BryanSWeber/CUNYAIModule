#pragma once

#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\MapInventory.h"
#include "Source\Unit_Inventory.h"
#include "Source\Resource_Inventory.h"
#include "Source\Reservation_Manager.h"
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

bool Reservation::checkTypeInReserveSystem(UnitType type) {
    for (auto reservation : reservation_map_) {
        if (reservation.second == type) return true;
    }
    return false;
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

bool Reservation::checkExcessIsGreaterThan(const UnitType &type) const {
    bool okay_on_gas = Broodwar->self()->gas() - gas_reserve_ > type.gasPrice() || type.gasPrice() == 0;
    bool okay_on_minerals = Broodwar->self()->minerals() > type.mineralPrice() || type.mineralPrice() == 0;
    return okay_on_gas && okay_on_minerals;
}

bool Reservation::checkExcessIsGreaterThan(const TechType &type) const {
    return Broodwar->self()->gas() - gas_reserve_ > type.gasPrice() && Broodwar->self()->minerals() > type.mineralPrice();
}

bool Reservation::checkAffordablePurchase(const UnitType type, const int distance) {
    bool affordable = Broodwar->self()->minerals() + 0.046 * static_cast<double>(CUNYAIModule::workermanager.min_workers_) * static_cast<double>(distance) / CUNYAIModule::getProperSpeed(UnitTypes::Zerg_Drone) - min_reserve_ >= type.mineralPrice() &&
                      Broodwar->self()->gas() + 0.069 * static_cast<double>(CUNYAIModule::workermanager.gas_workers_) * static_cast<double>(distance) / CUNYAIModule::getProperSpeed(UnitTypes::Zerg_Drone) - gas_reserve_ >= type.gasPrice();
    bool already_making_one = false;
    for (auto it = reservation_map_.begin(); it != reservation_map_.end(); it++) {
        if (it->second == type) {
            already_making_one = true;
            break;
        }
    }
    bool open_reservation = reservation_map_.empty() || !already_making_one;
    return affordable && open_reservation;
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

    if (!reservation_map_.empty() && last_builder_sent_ < Broodwar->getFrameCount() - 30 * 24) {
        Diagnostics::DiagnosticText("...We're stuck, aren't we? Have a friendly nudge.", "");
        reservation_map_.clear();
        min_reserve_ = 0;
        gas_reserve_ = 0;
    }
}