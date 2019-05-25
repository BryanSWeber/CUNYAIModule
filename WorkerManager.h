#pragma once
// Remember not to use "Broodwar" in any global class constructor!

# include "Source\CUNYAIModule.h"

class WorkerManager {
private:


public:
    // Checks all Mines of type for undersaturation and assigns Gather. Goes to any undersaturated location, preference for local mine. Returns true on success.
    bool assignGather(const Unit & unit, const UnitType mine);
    // Clears blocking minerals (as determined by BWEB). Check if area is threatened before clearing.  Returns true on success.
    bool assignClear(const Unit & unit);
    // attaches the miner to the nearest mine in the inventory, and updates the stored_unit. Returns true on success.
    bool attachToNearestMine(Resource_Inventory & ri, Map_Inventory & inv, Stored_Unit & miner, bool distance);
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
    //Workers at their end build location should build there!  May return false if there is an object at the end destination that prohibits building.
    bool workerPrebuild(const Unit & unit);
    // Workers doing nothing should begin a resource task.
    bool workersCollect(const Unit & unit);
    // Workers should clear if conditions are met;  Ought to be reprogrammed to run a periodic cycle to check for clearing. Then grab nearest worker to patch and assign. This policy may lead to inefficent mining.
    bool workersClear(const Unit &unit);
    // Workers should return cargo and forget about their original mine.
    bool workersReturn(const Unit &unit);
};

