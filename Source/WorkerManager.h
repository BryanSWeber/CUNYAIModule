#pragma once
// Remember not to use "Broodwar" in any global class constructor!

# include "CUNYAIModule.h"

class WorkerManager {
private:

public:
    int gas_workers_; // sometimes this count may be off by one when units are in the geyser.
    int min_workers_;
    int workers_clearing_;
    int workers_distance_mining_;
    bool excess_gas_capacity_;

    // Checks all Mines of type for undersaturation and assigns Gather. Goes to any undersaturated location, preference for local mine. Returns true on success.
    bool assignGather(const Unit & unit, const UnitType mine);
    // Clears blocking minerals (as determined by BWEB). Check if area is threatened before clearing.  Returns true on success.
    bool assignClear(const Unit & unit);
    // attaches the miner to the nearest mine in the inventory, and updates the stored_unit. Returns true on success.
    bool attachToNearestMine(Resource_Inventory & ri, Stored_Unit & miner);
    // attaches the miner to the particular mine and updates the stored unit.
    void attachToParticularMine(Stored_Resource &mine, Resource_Inventory &ri, Stored_Unit &miner);
    // Overload: attaches the miner to the particular mine and updates the stored unit.
    void attachToParticularMine(Unit & mine, Resource_Inventory & ri, Stored_Unit & miner);
    // Returns true if there is a blocking mineral nearby.
    bool checkBlockingMinerals(const Unit & unit, Unit_Inventory & ui);
    bool checkGasDump();
    //Returns True if there is an out for gas. Does not consider all possible gas outlets.
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
    // Updates the count of our gas workers.
    void WorkerManager::updateGas_Workers();
    // Updates the count of our min workers.
    void WorkerManager::updateMin_Workers();
    // Updates the count of clearing workers
    void WorkerManager::updateWorkersClearing();
    // Updates the count of mining workers
    void WorkerManager::updateWorkersLongDistanceMining();
    // Updates the if gas has excess capacity. NOTE: Does not check if we WANT that capacity atm.
    void WorkerManager::updateExcessCapacity();
};

