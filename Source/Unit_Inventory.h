#pragma once

#include <BWAPI.h>
#include "MeatAIModule.h"

using namespace std;
using namespace BWAPI;

// Two dependent structures for this inventory manager, a container of enemy_units and enemy units itself. Intend to add more funtionality to Enemy_Inventory, such as upgrades, etc.  May revisit when I learn about parentage, but ought to function for now.

struct Stored_Unit {

    // Creates an enemy unit object, an abbreviated version of the original.
    Stored_Unit( Unit unit );
    Stored_Unit();

    // Critical information not otherwise stored.
    UnitType type_;
    UnitType build_type_;
    Position pos_; // in pixels
    int current_hp_;
    bool valid_pos_; // good suggestion by Hannes Brandenburg. Know to alter unit data when we see that they are not present.
    int unit_ID_;

    // evaluates the value of a stock of specific unit, in terms of pythagorian distance of min & gas & supply. Doesn't consider the counterfactual larva. Is set to considers the unit's condition. BWAPI measures supply in half units. 
    int current_stock_value_; // Precalculated, precached.
    int stock_value_; // Precalculated, precached.

    Unit bwapi_unit_;

};

struct Unit_Inventory {

    //Creates an instance of the Unit inventory class.
    Unit_Inventory();
    Unit_Inventory(const Unitset &unit_set );

    //what about their upgrades?
    //Other details?

    int stock_fliers_;
    int stock_ground_units_;
    int stock_shoots_up_;
    int stock_shoots_down_;
    int stock_high_ground_;
    int stock_total_;
    int max_range_;
	int worker_count_;
	int volume_;

    std::map <Unit, Stored_Unit> unit_inventory_;

    // Updates the count of enemy units.
    void addStored_Unit( Unit unit );
    void addStored_Unit( Stored_Unit stored_unit );

    //Removes enemy units
    void removeStored_Unit( Unit unit );

    // Updates enemy units
    void updateStored_Unit( Unit unit );

    //Updates summary of enemy inventory, stored here.
    void updateUnitInventorySummary();

    Position getMeanLocation() const;
    Position getMeanBuildingLocation() const;
    Position getMeanCombatLocation() const;
};


