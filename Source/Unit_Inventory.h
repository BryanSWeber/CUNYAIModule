#pragma once

#include <BWAPI.h>
#include "CUNYAIModule.h"
#include "Resource_Inventory.h"
#include "Map_Inventory.h"
#include "Reservation_Manager.h"
#include "Research_Inventory.h"
#include "FAP\FAP\include\FAP.hpp"
#include <random> // C++ base random is low quality.

using namespace std;
using namespace BWAPI;

// Two dependent structures for this inventory manager, a container of enemy_units and enemy units itself. Intend to add more funtionality to Enemy_Inventory, such as upgrades, etc.  May revisit when I learn about parentage, but ought to function for now.
struct Map_Inventory;
struct Reservation;

struct Stored_Unit {

    //Creates a steryotyped ideal of the unit. For comparisons.
    Stored_Unit( const UnitType &unittype );
    // Creates an enemy unit object, an abbreviated version of the original.
    Stored_Unit( const Unit &unit );
    Stored_Unit();
    auto convertToFAP(const Research_Inventory &ri); // puts stored unit into the fap type.
    auto convertToFAPPosition(const Position & chosen_pos, const Research_Inventory &ri, const UpgradeType &upgrade = UpgradeTypes::None, const TechType &tech = TechTypes::None); // puts the stored unit into the fap type... at a specific position
    auto convertToFAPDisabled(const Position & chosen_pos, const Research_Inventory & ri); // puts the unit in as an immobile unit.
    auto convertToFAPAnitAir(const Position & chosen_pos, const Research_Inventory & ri); // puts the unit in as an anti-air only tool.

    static void updateFAPvalue(FAP::FAPUnit<Stored_Unit*> &fap_unit); //updates a single unit's fap forecast when given the fap unit.
    void updateFAPvalueDead(); //Updates the unit in the case of it not surviving the FAP simulation.

    static bool unitDeadInFuture(const Stored_Unit &unit, const int & number_of_frames_voted_death); // returns true if the unit has a MA forcast that implies it will be alive in X frames.

    void updateStoredUnit(const Unit &unit);

    // Critical information not otherwise stored.
    UnitType type_; // the type of the unit.
    UnitType build_type_; // the type the worker is about to build.
    Position pos_; // in pixels
    Unit locked_mine_;

    // Unit Orders
    Order order_;
    UnitCommand command_;
    int time_since_last_command_; // note command != orders.
    int time_of_last_purge_; //Mostly for workers

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
    bool detected_; // this bool only works for enemy units not our own.
    bool updated_fap_this_frame_;
    int areaID_;
    
    string phase_ = "None";

    //Needed commands for workers.
    void startMine(Stored_Resource &new_resource, Resource_Inventory &ri);
    void stopMine(Resource_Inventory &ri);
    Stored_Resource * getMine(Resource_Inventory & ri);
    bool isAssignedClearing( Resource_Inventory &ri);  // If the unit is clearing a spot.
    bool isAssignedLongDistanceMining(Resource_Inventory & ri);
    bool isAssignedMining(Resource_Inventory & ri); // If the unit is assigned to mine a spot.
    bool isAssignedGas(Resource_Inventory & ri); // If the unit is assigned to mine gas.
    bool isAssignedResource(Resource_Inventory  &ri);
    bool isAssignedBuilding(Resource_Inventory  &ri); // If the unit is assigned to build something.
    bool isBrokenLock(Resource_Inventory & ri); // If the unit has been distracted somehow.
    bool isLocallyLocked(Resource_Inventory & ri); // If the unit is properly attached.
    bool isNoLock(); // If the unit has no target. May be broken.
    bool isLongRangeLock(Resource_Inventory & ri); // if the unit cannot see its target.
    bool isMovingLock(Resource_Inventory & ri); // if the unit is moving towards its target not gathering.

    int current_hp_;
    bool valid_pos_; // good suggestion by Hannes Brandenburg. Know to alter unit data when we see that they are not present.
    int unit_ID_;

    // evaluates the value of a stock of specific unit, in terms of min & gas & supply. Doesn't consider the counterfactual larva. Is set to considers the unit's condition. BWAPI measures supply in half units. 
    int modified_supply_;
    int modified_min_cost_;
    int modified_gas_cost_;
    int current_stock_value_; // Precalculated, precached.
    int stock_value_; // Precalculated, precached.
    int future_fap_value_; // only taken from fap.
    int ma_future_fap_value_; // A moving average of FAP values.
    int count_of_expected_deaths_; // the number of sims forcasting the unit's death.
    bool hasTarget_;

    int velocity_x_;
    int velocity_y_;
    int circumference_;
    int circumference_remaining_;

    Unit bwapi_unit_;

};

struct Unit_Inventory {

    //Creates an instance of the Unit inventory class.
    Unit_Inventory();
    Unit_Inventory( const Unitset &unit_set );

    //what about their upgrades?
    //Other details?

    static const int half_map_ = 120; // SC Screen size is 680 X 240
    static std::default_random_engine generator_;  //Will be used to obtain a seed for the random number engine

    int stock_fliers_;
    int stock_ground_units_;
    int stock_both_up_and_down_;
    int stock_shoots_up_;
    int stock_shoots_down_;
    int stock_high_ground_;
    int stock_fighting_total_;
    int stock_ground_fodder_;
    int stock_air_fodder_;
    int stock_total_;
    int stock_full_health_;
    int max_range_;
    int max_cooldown_;
    int worker_count_;
    int volume_;
    int detector_count_;
    int cloaker_count_;
    int flyer_count_;
    int resource_depot_count_;
    int future_fap_stock_;
    int moving_average_fap_stock_;
    int is_shooting_;
    int is_attacking_;
    int is_retreating_;
    std::map <Unit, Stored_Unit> unit_map_;

    // Updates the count of units.
    void addStored_Unit( const Unit &unit );
    void addStored_Unit( const Stored_Unit &stored_unit );

    //Removes units
    void removeStored_Unit( Unit unit );

    //Updates summary of inventory, stored here.
    void updateUnitInventorySummary();
    void updateUnitInventory(const Unitset &unit_set);
    void updateUnitsControlledBy(const Player & Player);
    void purgeBrokenUnits();
    void purgeUnseenUnits(); //drops all unseen units. Useful to make sure you don't have dead units in your own inventory.
    void purgeAllPhases();
    void purgeWorkerRelationsStop(const Unit &unit, Resource_Inventory &ri, Map_Inventory &inv, Reservation &res);
    void purgeWorkerRelationsNoStop(const Unit & unit, Resource_Inventory & ri, Map_Inventory & inv, Reservation & res);
    void purgeWorkerRelationsOnly(const Unit & unit, Resource_Inventory & ri, Map_Inventory & inv, Reservation & res);
    void drawAllVelocities(const Map_Inventory &inv) const; // sometimes causes a lag-out or a crash. Unclear why.
    void drawAllHitPoints(const Map_Inventory & inv) const;
    void drawAllMAFAPaverages(const Map_Inventory & inv) const;
    void drawAllSpamGuards(const Map_Inventory & inv) const;
    void drawAllWorkerTasks(const Map_Inventory & inv, Resource_Inventory &ri) const;
    void drawAllLocations(const Map_Inventory &inv) const;
    void drawAllMisplacedGroundUnits(const Map_Inventory & inv) const;
    Unit_Inventory getInventoryAtArea(const int areaID) const;
    Unit_Inventory getCombatInventoryAtArea(const int areaID) const;
    Unit_Inventory getBuildingInventoryAtArea(const int areaID) const;



    // Several ways to add to FAP models. At specific locations, immobilized, at a random position around their original position, to buildFAP's small combat scenario.
    void addToFAPatPos(FAP::FastAPproximation<Stored_Unit*>& fap_object, const Position pos, const bool friendly, const Research_Inventory &ri); // adds to buildFAP
    void addDisabledToFAPatPos(FAP::FastAPproximation<Stored_Unit*>& fap_object, const Position pos, const bool friendly, const Research_Inventory & ri);
    void addAntiAirToFAPatPos(FAP::FastAPproximation<Stored_Unit*>& fap_object, const Position pos, const bool friendly, const Research_Inventory & ri);
    void addToMCFAP(FAP::FastAPproximation<Stored_Unit*>& fap_object, const bool friendly, const Research_Inventory & ri); // adds to MC fap.
    void addToBuildFAP(FAP::FastAPproximation<Stored_Unit*>& fap_object, const bool friendly, const Research_Inventory & ri, const UpgradeType &upgrade = UpgradeTypes::None);// adds to the building combat simulator, friendly side.


    void pullFromFAP(vector<FAP::FAPUnit<Stored_Unit*>> &FAPunits); // updates UI with FAP forecasts. Throws exceptions if something is misaligned.

    // Pass pointers
    Stored_Unit* getStoredUnit(const Unit &unit);
    // Passing values for const-safety.
    Stored_Unit getStoredUnitValue(const Unit & unit) const;

    Position getMeanLocation() const;
    Position getMeanBuildingLocation() const;
    Position getMeanAirLocation() const;
    Position getStrongestLocation() const; //in progress
    Position getMeanCombatLocation() const;
    Position getMeanArmyLocation() const;
    static Position positionMCFAP(const Stored_Unit & su);
    static Position positionBuildFap(const bool friendly);
    //Position getClosestMeanArmyLocation() const;

    void stopMine(Unit u, Resource_Inventory & ri);
    friend Unit_Inventory operator + (const Unit_Inventory & lhs, const Unit_Inventory& rhs);
    friend Unit_Inventory operator - (const Unit_Inventory & lhs, const Unit_Inventory& rhs);
    Unit_Inventory(Unit_Inventory const &) = default;

};




