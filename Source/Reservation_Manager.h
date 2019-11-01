#pragma once

#include <BWAPI.h>
#include "CUNYAIModule.h"
#include "Map_Inventory.h"
#include "Unit_Inventory.h"
#include "Resource_Inventory.h"

struct Reservation {

    Reservation::Reservation();

    int min_reserve_;
    int gas_reserve_;
    int building_timer_;
    int last_builder_sent_;
    map<TilePosition, UnitType> reservation_map_;

    // Updates mineral, gas, and time reserves for a particular unit. Will return FALSE if it is already present. This functionality is taken advantage of in some cases.
    bool Reservation::addReserveSystem(TilePosition tile, UnitType type);
    // Removes an item from the reserve system. Will return FALSE if it is not there.
    bool Reservation::removeReserveSystem(TilePosition tile, UnitType type, bool retry_this_building);
    // Checks if an item of type is in reserve system.
    bool checkTypeInReserveSystem(UnitType type);

    // Decrements the clock. Simple but works.
    void Reservation::decrementReserveTimer();

    int Reservation::getExcessMineral();
    int Reservation::getExcessGas();

    bool Reservation::checkExcessIsGreaterThan(const UnitType & type) const;
    bool Reservation::checkExcessIsGreaterThan(const TechType & type) const;

    //Checks if a purchase is affordable, imagines that we have a constant income stream X seconds in the future.
    bool Reservation::checkAffordablePurchase(const UnitType type, const int X = 3);
    int Reservation::countTimesWeCanAffordPurchase(const UnitType type);
    bool Reservation::checkAffordablePurchase(const TechType type);
    bool Reservation::checkAffordablePurchase(const UpgradeType type);

    void Reservation::confirmOngoingReservations();

};