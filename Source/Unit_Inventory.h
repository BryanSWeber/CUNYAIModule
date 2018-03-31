#pragma once

#include <BWAPI.h>
#include "MeatAIModule.h"
#include "Resource_Inventory.h"
#include "InventoryManager.h"
#include "Reservation_Manager.h"
#include "Fight_MovementManager.h"

using namespace std;
using namespace BWAPI;

// Two dependent structures for this inventory manager, a container of enemy_units and enemy units itself. Intend to add more funtionality to Enemy_Inventory, such as upgrades, etc.  May revisit when I learn about parentage, but ought to function for now.
class Inventory;
class Reservation;

struct Stored_Unit {

    //Creates a steryotyped ideal of the unit. For comparisons.
    Stored_Unit( const UnitType &unittype );
    // Creates an enemy unit object, an abbreviated version of the original.
    Stored_Unit( Unit unit );
    Stored_Unit();
	void updateStoredUnit(const Unit &unit);

    // Critical information not otherwise stored.
    UnitType type_;
    UnitType build_type_;
    Position pos_; // in pixels
	Unit locked_mine_;
    //Boids unit_movement_;

	void startMine(Stored_Resource &new_resource, Resource_Inventory &ri);
	void stopMine(Resource_Inventory &ri);
    Stored_Resource * getMine(Resource_Inventory & ri);
    bool isClearing(Resource_Inventory &ri);  // If the unit is clearing a spot.
	//void addMine(Stored_Resource mine);

    int current_hp_;
    bool valid_pos_; // good suggestion by Hannes Brandenburg. Know to alter unit data when we see that they are not present.
    int unit_ID_;

    // evaluates the value of a stock of specific unit, in terms of pythagorian distance of min & gas & supply. Doesn't consider the counterfactual larva. Is set to considers the unit's condition. BWAPI measures supply in half units. 
    int current_stock_value_; // Precalculated, precached.
    int stock_value_; // Precalculated, precached.
    bool hasTarget_;

    int velocity_x_;
    int velocity_y_;

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
    int stock_shoots_up_;
    int stock_shoots_down_;
    int stock_high_ground_;
    int stock_total_;
    int max_range_;
    int max_cooldown_;
	int worker_count_;
	int volume_;
    int detector_count_;
    int cloaker_count_;

	std::map <Unit, Stored_Unit> unit_inventory_;

    // Updates the count of  units.
    void addStored_Unit( Unit unit );
    void addStored_Unit( Stored_Unit stored_unit );

    //Removes units
    void removeStored_Unit( Unit unit );

    //Updates summary of inventory, stored here.
    void updateUnitInventorySummary();
	void updateUnitInventory(const Unitset &unit_set);
    void purgeBrokenUnits();
    void purgeUnseenUnits(); //drops all unseen units. Useful to make sure you don't have dead units in your own inventory.
    void purgeWorkerRelations(const Unit &unit, Resource_Inventory &ri, Inventory &inv, Reservation &res);
    void purgeWorkerMineRelations(const Unit &unit, Resource_Inventory &ri);
    void purgeWorkerBuildRelations(const Unit &unit, Inventory &inv, Reservation &res);

    Position getMeanLocation() const;
    Position getMeanBuildingLocation() const;
    Position getMeanCombatLocation() const;
    Position getMeanArmyLocation() const;
    Position getClosestMeanArmyLocation() const;


    void stopMine(Unit u, Resource_Inventory & ri);
    friend Unit_Inventory operator + (const Unit_Inventory & lhs, const Unit_Inventory& rhs);
    friend Unit_Inventory operator - (const Unit_Inventory & lhs, const Unit_Inventory& rhs);

};


