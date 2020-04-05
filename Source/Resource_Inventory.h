#pragma once

#include <BWAPI.h>
#include "CUNYAIModule.h"
#include "Unit_Inventory.h"
#include "Map_Inventory.h"
#include <bwem.h>

using namespace std;
using namespace BWAPI;

struct Unit_Inventory; //forward declaration permits use of Unit_Inventory class within resource_inventory.
struct Map_Inventory;

struct Stored_Resource {

    //Creator methods
    Stored_Resource();
    Stored_Resource(Unit unit);

    int current_stock_value_;
    int max_stock_value_;
    int number_of_miners_;
    int areaID_;

    bool occupied_resource_;
    bool full_resource_;
    bool valid_pos_;
    bool blocking_mineral_;

    Unit bwapi_unit_;
    UnitType type_;
    Position pos_;

};

class Resource_Inventory {
private:
    // For only occupied patches.
    int local_mineral_patches_ = 0;
    int local_refineries_ = 0;

    //Note: Now depreciated in favor of WorkerManager's tallies. Only use for LOCAL values.
    int local_miners_ = 0;
    int local_gas_collectors_ = 0;

public:
    //Creates an instance of the Resource inventory class.
    Resource_Inventory(); // for blank construction.
    Resource_Inventory(const Unitset &unit_set);

    std::map <Unit, Stored_Resource> resource_inventory_;

    void addStored_Resource(Unit unit);     // Updates the count of resource units.
    void addStored_Resource(Stored_Resource stored_resource);     // Updates the count of resource units.

    void removeStored_Resource(Unit unit);    //Removes Resource

    Position getMeanLocation() const; //Updates summary of inventory, stored here. Needs to potentially inject enemy extractors into the enemy inventory, ei.

    void updateResourceInventory(Unit_Inventory & ui, Unit_Inventory & ei, Map_Inventory &inv); // updates values of units in mine.
    void updateMines(); //counts number of viable gas mines and local mineral patches.
    void drawMineralRemaining() const;
    void drawUnreachablePatch(const Map_Inventory & inv) const;;
    friend Resource_Inventory operator + (const Resource_Inventory & lhs, const Resource_Inventory& rhs);
    friend Resource_Inventory operator - (const Resource_Inventory & lhs, const Resource_Inventory & rhs);

    // This command sometimes is inaccurate depending on the latancy. 
    int getLocalMiners();
    // This command sometimes is inaccurate depending on the latancy. 
    int getLocalGasCollectors();
    int getLocalMinPatches();
    int getLocalRefineries();

};
