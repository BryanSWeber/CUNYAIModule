#pragma once

#include <BWAPI.h>
#include "CUNYAIModule.h"
#include "ResourceInventory.h"
#include "MapInventory.h"
#include "ReservationManager.h"
#include "ResearchInventory.h"
#include "FAP\FAP\include\FAP.hpp"
#include "CombatSimulator.h"
#include <random> // C++ base random is low quality.
#include <vector>

using namespace std;
using namespace BWAPI;

//Forward declarations of relevant classes.
class MapInventory;
class Reservation;
class CombatSimulator;

struct StoredUnit {

    //Creates a steryotyped ideal of the unit. For comparisons.
    StoredUnit(const UnitType & unittype, const bool &carrierUpgrade = false, const bool &reaverUpgrade = false);

    static int getTraditionalWeight(const UnitType unittype, const bool &carrierUpgrade = false, const bool &reaverUpgrade = false);
    //static int getGrownWeight(const UnitType unittype, const bool & carrierUpgrade, const bool & reaverUpgrade);

    // Creates an enemy unit object, an abbreviated version of the original.
    StoredUnit(const Unit &unit);
    StoredUnit();

    void updateFAPvalue(FAP::FAPUnit<StoredUnit*> fap_unit); //updates a single unit's fap forecast when given the fap unit.
    void updateFAPvalueDead(); //Updates the unit in the case of it not surviving the FAP simulation.
    void updateStoredUnit(const Unit &unit);

    // Critical information not otherwise stored.
    UnitType type_; // the type of the unit.
    UnitType build_type_; // the type the worker is about to build. (found in bwapi, has build order already sent.)
    UnitType intended_build_type_; // the type the builder plans to build. (Not found in bwapi, build order has yet to be sent.
    TilePosition intended_build_tile_; // the tileposition at which the builder plans to build. (Not found in bwapi, build order has yet to be sent.
    Position pos_; // in pixels
    Unit locked_mine_;

    // Unit Orders
    Order order_;
    UnitCommand command_;
    int time_since_last_command_ = 0; // note command != orders.
    int time_of_last_purge_ = 0; //Mostly for workers
    int time_since_last_seen_ = 0; //Enemy production estimates depend on this.
    int time_first_observed_ = Broodwar->getFrameCount();

    //Unit Movement Information;
    Position attract_;
    Position seperation_;
    Position retreat_;
    Position cohesion_;
    int health_;
    int shields_;
    bool is_flying_;
    bool shoots_up_;
    bool shoots_down_;
    int elevation_;
    int cd_remaining_;
    bool stimmed_;
    bool burrowed_;
    bool cloaked_;
    bool detected_; // this bool only works for enemy units not our own.
    bool updated_fap_this_frame_;
    int areaID_;
    int time_since_last_dmg_;
    bool is_on_island_;
    double angle_;

    enum Phase
    {
        None = 0,
        Attacking = 1,
        Retreating = 2,
        Prebuilding = 3,
        PathingOut = 4,
        PathingHome = 5,
        Surrounding = 6,
        NoRetreat = 7,
        MiningMin = 8,
        MiningGas = 9,
        Returning = 10,
        DistanceMining = 11,
        Clearing = 12,
        Upgrading = 13,
        Researching = 14,
        Morphing = 15,
        Building = 16,
        Detecting = 17
    };
    Phase phase_ = Phase::None;
    StoredUnit(Phase p) : phase_(p) {}
    operator Phase () const { return phase_; }

    bool unitDeadInFuture(const int & numberOfConsecutiveDeadSims = 4) const; // returns true if the unit has a MA forcast that implies it will be alive in X frames.

    //Needed commands for workers.
    void startMine(Stored_Resource &new_resource);
    void startMine(Unit & new_resource);
    void stopMine();
    Stored_Resource * getMine();
    bool isAssignedClearing();  // If the unit is clearing a spot.
    bool isAssignedLongDistanceMining(); //If the unit is mining at a distance.
    bool isAssignedMining(); // If the unit is assigned to mine a spot.
    bool isAssignedGas(); // If the unit is assigned to mine gas.
    bool isAssignedResource();
    bool isAssignedBuilding(); // If the unit is assigned to build something.
    bool isBrokenLock(); // If the unit has been distracted somehow.
    bool isLocallyLocked(); // If the unit is properly attached.
    bool isNoLock(); // If the unit has no target. May be broken.
    bool isLongRangeLock(); // if the unit cannot see its target.

    int current_hp_;
    bool valid_pos_; // good suggestion by Hannes Brandenburg. Know to alter unit data when we see that they are not present.
    int unit_ID_;

    // evaluates the value of a stock of specific unit, in terms of min & gas & supply. Doesn't consider the counterfactual larva. Is set to considers the unit's condition. BWAPI measures supply in half units. 
    int modified_supply_;
    int modified_min_cost_;
    int modified_gas_cost_;
    int current_stock_value_; // Precalculated, precached.
    int stock_value_; // Precalculated, precached.
    int future_fap_value_; // Current FAP prediction.
    int count_of_consecutive_predicted_deaths_; // the number of sims forcasting the unit's death.
    bool hasTarget_;

    int velocity_x_;
    int velocity_y_;
    int circumference_;
    int circumference_remaining_;

    Unit bwapi_unit_;
//private:
//    //prevent automatic conversion for any other built-in types such as bool, int, etc
//    template<typename T>
//    operator T () const;
};

struct UnitInventory {

    //Creates an instance of the Unit inventory class.
    UnitInventory();
    UnitInventory(const Unitset &unit_set);

    //what about their upgrades?
    //Other details?


    int stock_fliers_ = 0;
    int stock_ground_units_ = 0;
    int stock_both_up_and_down_ = 0;
    int stock_shoots_up_ = 0;
    int stock_shoots_down_ = 0;
    int stock_high_ground_ = 0;
    int stock_fighting_total_ = 0;
    int stock_ground_fodder_ = 0;
    int stock_air_fodder_ = 0;
    int stock_total_ = 0;
    int stock_full_health_ = 0;
    int stock_psion_ = 0;
    int total_supply_ = 0;
    int max_range_ = 0;
    int max_range_air_ = 0;
    int max_range_ground_ = 0;
    int max_cooldown_ = 0;
    int max_speed_ = 0;
    int worker_count_ = 0;
    int volume_ = 0;
    int detector_count_ = 0;
    int cloaker_count_ = 0;
    int flyer_count_ = 0;
    int ground_count_ = 0;
    int ground_melee_count_ = 0;
    int ground_range_count_ = 0;
    int building_count_ = 0;
    int resource_depot_count_ = 0;
    int future_fap_stock_ = 0;
    int is_shooting_ = 0;
    int island_count_ = 0;

    map<StoredUnit::Phase, int > count_of_each_phase_ = { { StoredUnit::Phase::None, 0 } ,
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

    std::map <Unit, StoredUnit> unit_map_;

    // Updates the count of units.
    bool addStoredUnit(const Unit &unit);
    bool addStoredUnit(const StoredUnit &StoredUnit);

    //Removes units
    void removeStoredUnit(Unit unit);


    //Updates summary of inventory, stored here.
    int countRecentAdditions(int frames);

    //Must run after the creation of most (but not all) unit inventories, fills in all important details.
    void updateUnitInventorySummary();
    void updateUnitInventory(const Unitset &unit_set);
    void updateUnitsControlledBy(const Player & Player);
    void purgeBrokenUnits();
    void purgeUnseenUnits(); //drops all unseen units. Useful to make sure you don't have dead units in your own inventory.
    void purgeWorkerRelationsStop(const Unit & unit);
    void purgeWorkerRelationsNoStop(const Unit & unit);
    void purgeWorkerRelationsOnly(const Unit & unit, ResourceInventory & ri, MapInventory & inv, Reservation & res);
    void drawAllWorkerTasks() const;
    void drawAllLocations() const;
    void drawAllLastSeens() const;
    void drawAllMisplacedGroundUnits() const;
    UnitInventory getInventoryAtArea(const int areaID) const;
    UnitInventory getCombatInventoryAtArea(const int areaID) const;
    UnitInventory getBuildingInventoryAtArea(const int areaID) const;
    UnitInventory getBuildingInventory() const;


    void updatePredictedStatus(bool friendly = true); // updates UI with FAP forecasts. Throws exceptions if something is misaligned.

    //Gets a pointer to a stored unit when provided the unit pointer.
    StoredUnit* getStoredUnit(const Unit & u);
    // Passing a copy of the stored unit for safety and const-compatibility.
    StoredUnit getStoredUnitValue(const Unit & unit) const;

    Position getMeanLocation() const;
    Position getMeanBuildingLocation() const;
    Position getMeanAirLocation() const;
    Position getStrongestLocation() const; //in progress
    Position getMeanCombatLocation() const;
    Position getMeanArmyLocation() const;
    static Position positionMCFAP(const StoredUnit & su);
    static Position positionBuildFap(const bool friendly);
    //Position getClosestMeanArmyLocation() const;

    void printUnitInventory(const Player &player, const string &bonus = "");

    void stopMine(Unit u);
    friend UnitInventory operator + (const UnitInventory & lhs, const UnitInventory& rhs);
    friend UnitInventory operator - (const UnitInventory & lhs, const UnitInventory& rhs);
    UnitInventory(UnitInventory const &) = default;

};


void stopMine(const Unit &resource);
Stored_Resource* getMine(const Unit &resource);

