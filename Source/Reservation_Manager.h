#pragma once

#include <BWAPI.h>
#include "CUNYAIModule.h"
#include "MapInventory.h"
#include "UnitInventory.h"
#include "Resource_Inventory.h"

class Reservation {

private:

    int min_reserve_; //Minerals needed to build everything in the reservation manager.
    int gas_reserve_; //Gas needed to build everything in the reservation manager.
    int building_timer_; //Time (frames) needed to build everything in the reservation manager.
    int last_builder_sent_; //Time last builder was sent. Used to clear reservations if things seem problematic (30+ seconds of being stuck).
    map<TilePosition, UnitType> reservation_map_; //Map containing position of building and intended type.
    vector<UpgradeType> reserved_upgrades_; //Intended upgrades.

public:
    Reservation::Reservation(); //Creator method! Don't declare as private!

    bool Reservation::addReserveSystem(TilePosition tile, UnitType type);  // Updates mineral, gas, and time reserves for a particular unit. Will return FALSE if it is already present. This functionality is taken advantage of in some cases.
    void Reservation::addReserveSystem(UpgradeType up); //Updates reserves for an upgrade. Does not return anything if it is already present, since a vector can have duplicates.

    bool Reservation::removeReserveSystem(TilePosition tile, UnitType type, bool retry_this_building); // Removes an item from the reserve system. Will return FALSE if it is not there.
    bool Reservation::removeReserveSystem(UpgradeType up, bool retry_this_upgrade);     // Removes an item from the reserve system. Will return FALSE if it is not there.

    bool isInReserveSystem(const UnitType & type);  // Checks if an item of type is in reserve system.
    bool isInReserveSystem(const UpgradeType & up); // Checks if an item of type is in reserve system.

    int countInReserveSystem(const UnitType & type); // Counts the number of units of this type in the reserve system.


    void Reservation::decrementReserveTimer(); // Decrements the clock.

    int Reservation::getExcessMineral(); //gets minerals we have above the resevation amount.
    int Reservation::getExcessGas(); //gets gas we have above the reservation amount.

    map<TilePosition, UnitType> Reservation::getReservedUnits() const; //Unit getter.
    vector<UpgradeType> Reservation::getReservedUpgrades() const; // Upgrade getter.

    bool Reservation::checkExcessIsGreaterThan(const UnitType & type) const;
    bool Reservation::checkExcessIsGreaterThan(const TechType & type) const;


    bool Reservation::checkAffordablePurchase(const UnitType type, const int X = 3);     //Checks if a purchase is affordable, imagines that we have a constant income stream X seconds in the future.
    bool Reservation::checkAffordablePurchase(const TechType type);     //Checks if a purchase is affordable.
    bool Reservation::checkAffordablePurchase(const UpgradeType type);   //Checks if a purchase is affordable.

    int Reservation::countTimesWeCanAffordPurchase(const UnitType type); // returns N, number of times a purchase can be made.

    void Reservation::confirmOngoingReservations(); //Makes sure there are no idle workers trying to build in impossible situations or reservations without workers.

};