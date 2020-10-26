#pragma once

#include <BWAPI.h>
#include "CUNYAIModule.h"
#include "MapInventory.h"
#include "UnitInventory.h"
#include "Resource_Inventory.h"

class Reservation {

private:

    int minReserve_; //Minerals needed to build everything in the reservation manager.
    int gasReserve_; //Gas needed to build everything in the reservation manager.
    int supplyReserve_; //Supply needed to build everything in the reservation manager.
    int larvaReserve_; //Larva needed to build everything in the reservation manager.
    int lastBuilderSent_; //Time last builder was sent. Used to clear reservations if things seem problematic (30+ seconds of being stuck).
    map<TilePosition, UnitType> reservationBuildingMap_; //Map containing position of building and intended type.
    vector<UpgradeType> reservedUpgrades_; //Intended upgrades.
    map<Unit, UnitType> reservationUnits_; // map containing unit and number of intended units. Care that the key could become null if the unit is killed.

public:
    Reservation::Reservation(); //Creator method! Don't declare as private!

    bool Reservation::addReserveSystem(TilePosition tile, UnitType type);  // Updates mineral, gas, and time reserves for a particular unit. Will return FALSE if it is already present. This functionality is taken advantage of in some cases.
    void Reservation::addReserveSystem(UpgradeType up); //Updates reserves for an upgrade. Does not return anything if it is already present, since a vector can have duplicates.
    bool Reservation::addReserveSystem(Unit u, UnitType type);  // Updates mineral, gas, and time reserves for a particular unit. Will return FALSE if it is already present. This functionality is taken advantage of in some cases.

    bool Reservation::removeReserveSystem(TilePosition tile, UnitType type, bool retry_this_building); // Removes an item from the reserve system. Will return FALSE if it is not there.
    bool Reservation::removeReserveSystem(UpgradeType up, bool retry_this_upgrade);   // Removes an item from the reserve system. Will return FALSE if it is not there.
    bool Reservation::removeReserveSystem(UnitType type, bool retry_this_unit); // Removes an item from the reserve system. Will return FALSE if it is not there.

    bool isBuildingInReserveSystem(const UnitType & type);  // Checks if an item of type is in reserve system.
    bool isInReserveSystem(const UpgradeType & up); // Checks if an item of type is in reserve system.
    bool isUnitInReserveSystem(const UnitType & up); // Checks if an item of type is in reserve system.

    int countInReserveSystem(const UnitType & type); // Counts the number of units of this type in the reserve system.

    int Reservation::getExcessMineral(); //gets minerals we have above the resevation amount.
    int Reservation::getExcessGas(); //gets gas we have above the reservation amount.
    int Reservation::getExcessSupply(); //gets supply we have above the reservation amount.
    int Reservation::getExcessLarva(); //gets larva we have above the reservation amount.
    bool Reservation::requiresOvertappedResource(const UnitType &ut); //Checks if a unit requires a resource that is overtapped.

    map<TilePosition, UnitType> Reservation::getReservedBuildings() const; //Building getter.
    vector<UpgradeType> Reservation::getReservedUpgrades() const; // Upgrade getter.
    map<Unit, UnitType> Reservation::getReservedUnits() const; // Unit getter.

    bool Reservation::checkAffordablePurchase(const UnitType type, const int X = 0);     //Checks if a purchase is affordable, imagines that we have a constant income stream X seconds in the future.  Does not yet account for forecasted larva.
    bool Reservation::checkAffordablePurchase(const TechType type);     //Checks if a purchase is affordable.
    bool Reservation::checkAffordablePurchase(const UpgradeType type);   //Checks if a purchase is affordable.

    void Reservation::confirmOngoingReservations(); //Makes sure there are no idle workers trying to build in impossible situations or reservations without workers.

};

class RemainderTracker {
private:
    int supplyRemaining_ = 0;
    int gasRemaining_ = 0;
    int minRemaining_ = 0;
    int larvaeRemaining_ = 0;
public:
    RemainderTracker();
    int getWaveSize(UnitType u);
    void getReservationCapacity();
};