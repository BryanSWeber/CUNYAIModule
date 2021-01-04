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
    minReserve_ = 0;
    gasReserve_ = 0;
    supplyReserve_ = 0;
    larvaReserve_ = 0;
    lastBuilderSent_ = 0;
}

bool Reservation::addReserveSystem(TilePosition pos, UnitType type) {
    bool safe = reservationBuildingMap_.insert({ pos, type }).second;
    if (safe) {
        Diagnostics::DiagnosticWrite("Reserving a %s.", type.c_str());
        minReserve_ += type.mineralPrice();
        gasReserve_ += type.gasPrice();
        supplyReserve_ += type.supplyRequired();
        lastBuilderSent_ = Broodwar->getFrameCount();
        CUNYAIModule::buildorder.updateRemainingBuildOrder(type);
    }

    return safe;
}

void Reservation::addReserveSystem(UpgradeType up)
{
    reservedUpgrades_.push_back(up);
    Diagnostics::DiagnosticWrite("Reserving a %s.", up.c_str());
    int level = Broodwar->self()->getUpgradeLevel(up);
    minReserve_ += up.mineralPrice(level);
    gasReserve_ += up.gasPrice(level);
    CUNYAIModule::buildorder.updateRemainingBuildOrder(up);
}

bool Reservation::addReserveSystem(Unit originUnit, UnitType outputUnit) {
    bool inserted = reservationUnits_.insert({ originUnit, outputUnit }).second;
    if (inserted) {
        Diagnostics::DiagnosticWrite("Reserving a %s.", outputUnit.c_str());
        minReserve_ += outputUnit.mineralPrice();
        gasReserve_ += outputUnit.gasPrice();
        supplyReserve_ += outputUnit.supplyRequired();
        larvaReserve_ += originUnit->getType() == UnitTypes::Zerg_Larva;
        CUNYAIModule::buildorder.updateRemainingBuildOrder(outputUnit);
    }

    return inserted;
}

bool Reservation::removeReserveSystem(TilePosition pos, UnitType type, bool retry_this_building = false) {
    map<TilePosition, UnitType>::iterator it = reservationBuildingMap_.find(pos);
    if (it != reservationBuildingMap_.end() && !reservationBuildingMap_.empty()) {
        if (!CUNYAIModule::buildorder.isEmptyBuildOrder() && retry_this_building) CUNYAIModule::buildorder.retryBuildOrderElement(type);
        reservationBuildingMap_.erase(pos);
        return true;
    }
    return false;
}

bool Reservation::removeReserveSystem(UpgradeType up, bool retry_this_upgrade) {
    auto it = find(reservedUpgrades_.begin(), reservedUpgrades_.end(), up);
    if (it != reservedUpgrades_.end() && !reservedUpgrades_.empty()) {
        if (!CUNYAIModule::buildorder.isEmptyBuildOrder() && retry_this_upgrade) CUNYAIModule::buildorder.retryBuildOrderElement(up);
        reservedUpgrades_.erase(it);
        return true;
    }
    return false;
}

bool Reservation::removeReserveSystem(UnitType type, bool retry_this_unit = false) {
    for (auto i = reservationUnits_.begin(); i != reservationUnits_.end(); i++) {
        if (i->second == type) {
            if (!CUNYAIModule::buildorder.isEmptyBuildOrder() && retry_this_unit) CUNYAIModule::buildorder.retryBuildOrderElement(type);
            reservationUnits_.erase(i);
            return true;
        }
    }
    return false;
}

bool Reservation::isBuildingInReserveSystem(const UnitType & type) {
    for (auto reservation : reservationBuildingMap_) {
        if (reservation.second == type) return true;
    }
    return false;
}

bool Reservation::isInReserveSystem(const UpgradeType & up) {
    return find(reservedUpgrades_.begin(), reservedUpgrades_.end(), up) != reservedUpgrades_.end();
}

bool Reservation::isUnitInReserveSystem(const UnitType & type)
{
    for (auto reservation : reservationUnits_) {
        if (reservation.second == type) return true;
    }
    return false;
}

int Reservation::countInReserveSystem(const UnitType & type) {
    int count = 0;
    for (auto reservation : reservationBuildingMap_) {
        if (reservation.second == type) count += 1 + type.isTwoUnitsInOneEgg();
    }
    for (auto reservation : reservationUnits_) {
        if (reservation.second == type) count += 1 + type.isTwoUnitsInOneEgg();
    }
    return count;
}


int Reservation::getExcessMineral() {
    return max(Broodwar->self()->minerals() - minReserve_, 0);
}

int Reservation::getExcessGas() {
    return max(Broodwar->self()->gas() - gasReserve_, 0);
}

int Reservation::getExcessSupply()
{
    return max(Broodwar->self()->supplyTotal() - Broodwar->self()->supplyUsed(), 0);
}

int Reservation::getExcessLarva()
{
    return max(CUNYAIModule::countUnits(UnitTypes::Zerg_Larva) - larvaReserve_, 0);
}

bool Reservation::canReserveWithExcessResource(const UnitType & ut)
{
    if (ut.mineralPrice() > getExcessMineral() && minReserve_ > 0) return false;
    if (ut.gasPrice() > getExcessGas() && (gasReserve_ > 0 || CUNYAIModule::countUnits(ut.getRace().getRefinery()) == 0)) return false;
    if (ut.supplyRequired() > getExcessSupply() && supplyReserve_ > 0) return false;
    if (ut.whatBuilds().first == UnitTypes::Zerg_Larva && getExcessLarva() == 0 && larvaReserve_ > 0) return false;
    //if (ut.whatBuilds().first == UnitTypes::Zerg_Hydralisk && CUNYAIModule::countUnits(UnitTypes::Zerg_Hydralisk) == 0) return true;
    //if (ut.whatBuilds().first == UnitTypes::Zerg_Mutalisk && CUNYAIModule::countUnits(UnitTypes::Zerg_Mutalisk) == 0) return true;
    return true;
}

bool Reservation::canReserveWithExcessResource(const TechType & ut)
{
    if (ut.mineralPrice() > getExcessMineral() && minReserve_ > 0 ) return false;
    if (ut.gasPrice() > getExcessGas() && (gasReserve_ > 0 || CUNYAIModule::countUnits(ut.getRace().getRefinery()) == 0)) return false;
    return true;
}

bool Reservation::canReserveWithExcessResource(const UpgradeType & ut)
{
    if (ut.mineralPrice() > getExcessMineral() && minReserve_ > 0) return false;
    if (ut.gasPrice() > getExcessGas() && (gasReserve_ > 0 || CUNYAIModule::countUnits(ut.getRace().getRefinery()) == 0)) return false;
    return true;
}

//bool Reservation::canBuildWithExcessResource(const UnitType & ut)
//{
//    if (ut.mineralPrice() > getExcessMineral() ) return false;
//    if (ut.gasPrice() > getExcessGas() ) return false;
//    if (ut.supplyRequired() > getExcessSupply() ) return false;
//    if (ut.whatBuilds().first == UnitTypes::Zerg_Larva && getExcessLarva() == 0) return false;
//    //if (ut.whatBuilds().first == UnitTypes::Zerg_Hydralisk && CUNYAIModule::countUnits(UnitTypes::Zerg_Hydralisk) == 0) return true;
//    //if (ut.whatBuilds().first == UnitTypes::Zerg_Mutalisk && CUNYAIModule::countUnits(UnitTypes::Zerg_Mutalisk) == 0) return true;
//    return true;
//}
//
//bool Reservation::canBuildWithExcessResource(const TechType & ut)
//{
//    if (ut.mineralPrice() > getExcessMineral()) return false;
//    if (ut.gasPrice() > getExcessGas()) return false;
//    return true;
//}
//
//bool Reservation::canBuildWithExcessResource(const UpgradeType & ut)
//{
//    if (ut.mineralPrice() > getExcessMineral()) return false;
//    if (ut.gasPrice() > getExcessGas()) return false;
//    return true;
//}

map<TilePosition, UnitType> Reservation::getReservedBuildings() const
{
    return reservationBuildingMap_;
}

vector<UpgradeType> Reservation::getReservedUpgrades() const
{
    return reservedUpgrades_;
}

map<Unit,UnitType> Reservation::getReservedUnits() const
{
    return reservationUnits_;
}

bool Reservation::checkAffordablePurchase(const UnitType type, const int distance) {
    double bonus_frames = 48.0; //we need extra frames to make SURE we arrive early. 3 seconds?
    double extra_min = 0.046 * static_cast<double>(CUNYAIModule::workermanager.getMinWorkers()) * (static_cast<double>(distance) / (CUNYAIModule::getProperSpeed(UnitTypes::Zerg_Drone) * 0.5) + bonus_frames);
    double extra_gas = 0.069 * static_cast<double>(CUNYAIModule::workermanager.getGasWorkers()) * (static_cast<double>(distance) / (CUNYAIModule::getProperSpeed(UnitTypes::Zerg_Drone) * 0.5) + bonus_frames); // top speed overestimates drone movement heavily.

    bool min_affordable = ( static_cast<double>(Broodwar->self()->minerals()) + extra_min - static_cast<double>(minReserve_) >= type.mineralPrice() ) || type.mineralPrice() == 0;
    bool gas_affordable = ( static_cast<double>(Broodwar->self()->gas()) + extra_gas - static_cast<double>(gasReserve_) >= type.gasPrice() ) || type.gasPrice() == 0;
    bool supply_affordable = (static_cast<double>(Broodwar->self()->supplyTotal() - Broodwar->self()->supplyUsed()) - static_cast<double>(supplyReserve_) >= type.supplyRequired()) || type.supplyRequired() == 0;
    bool larva_affordable = type.whatBuilds().first != UnitTypes::Zerg_Larva || larvaReserve_ > 1;

    bool already_making_one = false;
    for (auto it = reservationBuildingMap_.begin(); it != reservationBuildingMap_.end(); it++) {
        if (it->second == type) {
            already_making_one = true;
            break;
        }
    }
    for (auto it = reservationUnits_.begin(); it != reservationUnits_.end(); it++) {
        if (it->second == type) {
            already_making_one = true;
            break;
        }
    }

    bool open_building = type.isBuilding() && (reservationBuildingMap_.empty() || !already_making_one);
    bool open_unitType = !type.isBuilding() && (reservationUnits_.empty() || !already_making_one);

    return min_affordable && gas_affordable && (open_building || open_unitType);
}

bool Reservation::checkAffordablePurchase(const TechType type) {
    return Broodwar->self()->minerals() - minReserve_ >= type.mineralPrice() && Broodwar->self()->gas() - gasReserve_ >= type.gasPrice();
}

bool Reservation::checkAffordablePurchase(const UpgradeType type) {
    return Broodwar->self()->minerals() - minReserve_ >= type.mineralPrice() && Broodwar->self()->gas() - gasReserve_ >= type.gasPrice();
}

void Reservation::confirmOngoingReservations() {

    //Remove unwanted elements.
    for (auto res_it = reservationBuildingMap_.begin(); res_it != reservationBuildingMap_.end() && !reservationBuildingMap_.empty(); ) {
        bool keep = false;

        for (auto unit_it = CUNYAIModule::friendly_player_model.units_.unit_map_.begin(); unit_it != CUNYAIModule::friendly_player_model.units_.unit_map_.end() && !CUNYAIModule::friendly_player_model.units_.unit_map_.empty(); unit_it++) {
            StoredUnit& miner = *CUNYAIModule::friendly_player_model.units_.getStoredUnit(unit_it->first); // we will want DETAILED information about this unit.
            if (miner.intended_build_type_ == res_it->second && miner.intended_build_tile_ == res_it->first && CUNYAIModule::assemblymanager.canMakeCUNY(res_it->second, false) ) //If the miner is there and we can still make the object (ignoring costs).
                keep = true;

        } // check if we have a unit building it.

        if (keep) {
            ++res_it;
        }
        else {
            Diagnostics::DiagnosticWrite("No worker is building the reserved %s. Freeing up the funds.", res_it->second.c_str());
            reservationBuildingMap_.erase(res_it++);  // contains an erase.
        }
    }

    for (auto res_it = reservationUnits_.begin(); res_it != reservationUnits_.end() && !reservationUnits_.empty(); ) {
        bool keep = false;

        for (auto u : Broodwar->self()->getUnits()) {
            if (u == res_it->first && u->getType() == res_it->second.whatBuilds().first)
                keep = true;
        } // check if we have the intended builder and a unit building it.

        if (keep) {
            ++res_it;
        }
        else {
            Diagnostics::DiagnosticWrite("The intended creator of %s does not exist. Freeing up the funds.", res_it->second.c_str()); //Don't broadcast the removal of "None".
            reservationUnits_.erase(res_it++);
        }
    }
    
    //Set default values and update.
    minReserve_ = 0;
    gasReserve_ = 0;
    supplyReserve_ = 0;
    larvaReserve_ = 0;

    for (auto res_it = reservationBuildingMap_.begin(); res_it != reservationBuildingMap_.end() && !reservationBuildingMap_.empty(); res_it++) {
        minReserve_ += res_it->second.mineralPrice();
        gasReserve_ += res_it->second.gasPrice();
    }

    for (auto res_it = reservedUpgrades_.begin(); res_it != reservedUpgrades_.end() && !reservedUpgrades_.empty(); res_it++) {
        minReserve_ += res_it->mineralPrice();
        gasReserve_ += res_it->gasPrice();
    }

    for (auto res_it = reservationUnits_.begin(); res_it != reservationUnits_.end() && !reservationUnits_.empty(); res_it++) {
        minReserve_ += res_it->second.mineralPrice();
        gasReserve_ += res_it->second.gasPrice();
        supplyReserve_ += res_it->second.supplyRequired();
        larvaReserve_ += res_it->first->getType() == UnitTypes::Zerg_Larva;
    }

    //if (!reservationBuildingMap_.empty() && lastBuilderSent_ < Broodwar->getFrameCount() - 30 * 24) {
    //    Diagnostics::DiagnosticWrite("...We're stuck, aren't we? Have a friendly nudge.", "");
    //    reservationBuildingMap_.clear();
    //    reservationUnits_.clear();
    //    reservedUpgrades_.clear();
    //    minReserve_ = 0;
    //    gasReserve_ = 0;
    //    supplyReserve_ = 0;
    //    larvaReserve_ = 0;
    //}
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
    int supplyRemaining_ = CUNYAIModule::my_reservation.getExcessSupply();
    int larvaeRemaining_ = CUNYAIModule::countUnits(UnitTypes::Zerg_Larva);
}
