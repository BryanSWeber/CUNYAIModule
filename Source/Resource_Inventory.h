#pragma once

#include <BWAPI.h>
#include "CUNYAIModule.h"
#include "Unit_Inventory.h"
#include "Map_Inventory.h"

using namespace std;
using namespace BWAPI;

struct Unit_Inventory; //forward declaration permits use of Unit_Inventory class within resource_inventory.
struct Map_Inventory;

struct Stored_Resource{

    //Creator methods
    Stored_Resource();
    Stored_Resource(Unit unit);

    int current_stock_value_;
    int max_stock_value_;
    int number_of_miners_;

    bool occupied_natural_;
    bool full_resource_;
    bool valid_pos_;

    Position local_natural_;
    Unit bwapi_unit_;
    UnitType type_;
    Position pos_;

};

struct Resource_Inventory {

    //Creates an instance of the Resource inventory class.
    Resource_Inventory(); // for blank construction.
    Resource_Inventory(const Unitset &unit_set);

    //what about their upgrades?
    //Other details?

    std::map <Unit, Stored_Resource> resource_inventory_;

    // Updates the count of resource units.
    void addStored_Resource(Unit unit);
    void addStored_Resource(Stored_Resource stored_resource);

    //Removes Resource
    void removeStored_Resource(Unit unit);

    //Updates summary of inventory, stored here. Needs to potentially inject enemy extractors into the enemy inventory, ei.

    Position getMeanLocation() const;
    Position getMeanBuildingLocation() const;
    Position getMeanCombatLocation() const;

    void updateResourceInventory( Unit_Inventory & ui, Unit_Inventory & ei, Map_Inventory &inv); // updates values of units in mine.
    void countViableMines(); //counts number of viable gas mines and local mineral patches.
    void drawMineralRemaining(const Map_Inventory &inv) const;

    friend Resource_Inventory operator + (const Resource_Inventory & lhs, const Resource_Inventory& rhs);
    friend Resource_Inventory operator - (const Resource_Inventory & lhs, const Resource_Inventory & rhs);

    //for all patches, long distance or otherwise.
    int total_miners_;
    int total_gas_;

    // For only local patches.
    int local_mineral_patches_;
    int local_refineries_;
    int local_miners_;
    int local_gas_collectors_;

    void updateMiners();
    void updateGasCollectors();
};
