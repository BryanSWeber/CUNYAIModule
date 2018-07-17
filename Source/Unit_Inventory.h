#pragma once

#include <BWAPI.h>
#include "CUNYAIModule.h"
#include "Resource_Inventory.h"
#include "InventoryManager.h"
#include "Reservation_Manager.h"
#include "FAP\include\FAP.hpp"

using namespace std;
using namespace BWAPI;

// Two dependent structures for this inventory manager, a container of enemy_units and enemy units itself. Intend to add more funtionality to Enemy_Inventory, such as upgrades, etc.  May revisit when I learn about parentage, but ought to function for now.
class Inventory;
class Reservation;

struct Stored_Unit {

    //Creates a steryotyped ideal of the unit. For comparisons.
    Stored_Unit( const UnitType &unittype );
    // Creates an enemy unit object, an abbreviated version of the original.
    Stored_Unit( const Unit &unit );
    Stored_Unit();
    auto convertToFAP();

    void updateFAPvalue(FAP::FAPUnit fap_unit); //updates a single unit's fap forecast when given the fap unit.

    void updateStoredUnit(const Unit &unit);

    // Critical information not otherwise stored.
    UnitType type_;
    UnitType build_type_;
    Position pos_; // in pixels
	Unit locked_mine_;

    // Unit Orders
    Order order_;
    UnitCommand command_;
    int time_since_last_command_; // note command != orders.
    int time_of_last_purge_; //test

    //Unit Movement Information;
    Position attract_;
    Position seperation_;
    Position retreat_;
    Position cohesion_;
    unsigned int health_;
    unsigned int shields_;
    unsigned int is_flying_;
    unsigned int elevation_;
    unsigned int cd_remaining_;
    bool stimmed_;

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

    // evaluates the value of a stock of specific unit, in terms of pythagorian distance of min & gas & supply. Doesn't consider the counterfactual larva. Is set to considers the unit's condition. BWAPI measures supply in half units. 
    int current_stock_value_; // Precalculated, precached.
    int stock_value_; // Precalculated, precached.
    int future_fap_value_; // only taken from fap.
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
    int max_range_;
    int max_cooldown_;
	int worker_count_;
	int volume_;
    int detector_count_;
    int cloaker_count_;
    int resource_depot_count_;
    int future_fap_stock_;

	std::map <Unit, Stored_Unit> unit_inventory_;

    // Updates the count of units.
    void addStored_Unit( const Unit &unit );
    void addStored_Unit( const Stored_Unit &stored_unit );

    //Removes units
    void removeStored_Unit( Unit unit );

    //Updates summary of inventory, stored here.
    void updateUnitInventorySummary();
	void updateUnitInventory(const Unitset &unit_set);
    void updateUnitsControlledByOthers();
    void purgeBrokenUnits();
    void purgeUnseenUnits(); //drops all unseen units. Useful to make sure you don't have dead units in your own inventory.
    void purgeWorkerRelations(const Unit &unit, Resource_Inventory &ri, Inventory &inv, Reservation &res);
    void purgeWorkerRelationsNoStop(const Unit & unit, Resource_Inventory & ri, Inventory & inv, Reservation & res);
    void drawAllVelocities(const Inventory &inv) const; // sometimes causes a lag-out or a crash. Unclear why.
    void drawAllHitPoints(const Inventory & inv) const;
    void drawAllSpamGuards(const Inventory & inv) const;
    void drawAllWorkerTasks(const Inventory & inv, Resource_Inventory &ri) const;
    void drawAllLocations(const Inventory &inv) const;

    void addToFriendlyFAP(); // adds entire inventory to the friendly side of the FAP army.
    void addToEnemyFAP(); // adds entire inventory to the friendly side of the FAP army.
    void copyFAPResult(FAP::FAPUnit & fap, Stored_Unit & unit);
    void pullFromFAP(vector<FAP::FAPUnit> FAPPunits); // updates UI with FAP forecasts. Throws exceptions if something is misaligned.

    Position getMeanLocation() const;
    Position getMeanBuildingLocation() const;
    Position getStrongestLocation() const; //in progress
    Position getMeanCombatLocation() const;
    Position getMeanArmyLocation() const;
    //Position getClosestMeanArmyLocation() const;

    void stopMine(Unit u, Resource_Inventory & ri);
    friend Unit_Inventory operator + (const Unit_Inventory & lhs, const Unit_Inventory& rhs);
    friend Unit_Inventory operator - (const Unit_Inventory & lhs, const Unit_Inventory& rhs);
};


