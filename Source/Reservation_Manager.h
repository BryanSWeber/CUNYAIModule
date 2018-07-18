#pragma once

#include <BWAPI.h>
#include "CUNYAIModule.h"
#include "InventoryManager.h"
#include "Unit_Inventory.h"
#include "Resource_Inventory.h"

struct Reservation {

    Reservation::Reservation();

    int min_reserve_;
    int gas_reserve_;
    int building_timer_;
    int last_builder_sent_;
    map<UnitType, TilePosition> reservation_map_;

    // Updates mineral, gas, and time reserves for a particular unit. 
    bool Reservation::addReserveSystem( UnitType type , TilePosition tile);
    void Reservation::removeReserveSystem( UnitType type );

    // Decrements the clock. Simple but works.
    void Reservation::decrementReserveTimer();

    int Reservation::getExcessMineral();
    int Reservation::getExcessGas();

    bool Reservation::checkExcessIsGreaterThan(const UnitType & type) const;
    bool Reservation::checkExcessIsGreaterThan(const TechType & type) const;

    bool Reservation::checkAffordablePurchase( const UnitType type );
    int Reservation::countTimesWeCanAffortPurchase(const UnitType type);
    bool Reservation::checkAffordablePurchase( const TechType type );
    bool Reservation::checkAffordablePurchase( const UpgradeType type );

    void Reservation::confirmOngoingReservations( const Unit_Inventory &ui);
};