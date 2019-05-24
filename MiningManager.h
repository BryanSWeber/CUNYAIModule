#pragma once
// Remember not to use "Broodwar" in any global class constructor!

# include "Source\CUNYAIModule.h"

class MiningManager {
public:
    // Checks all Mines of type for undersaturation. Goes to any undersaturated location, preference for local mine.
    void workerGather(const Unit & unit, const UnitType mine);
    // Clears blocking minerals (as determined by BWEB). Check if area is threatened before clearing.
    void workerClear(const Unit & unit);
    // attaches the miner to the nearest mine in the inventory, and updates the stored_unit.
    void attachToNearestMine(Resource_Inventory &ri, Map_Inventory &inv, Stored_Unit &miner);
    // attaches the miner to the particular mine and updates the stored unit.
    void attachToParticularMine(Stored_Resource &mine, Resource_Inventory &ri, Stored_Unit &miner);
    // Overload: attaches the miner to the particular mine and updates the stored unit.
    void attachToParticularMine(Unit & mine, Resource_Inventory & ri, Stored_Unit & miner);
    // Returns true if there is a blocking mineral nearby.
    bool checkBlockingMinerals(const Unit & unit, Unit_Inventory & ui);
    // returns true if a blocking mineral exists.
    bool checkGasOutlet();
    // Do worker things:
    bool workerWork(const Unit &u);
    // Checks if unit is carrying minerals.
    bool isEmptyWorker(const Unit & unit); 
};

