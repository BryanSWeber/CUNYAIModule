#pragma once
// Remember not to use "Broodwar" in any global class constructor!

# include "CUNYAIModule.h"

class WorkerManager {

private:
    int gas_workers_; // how many have the explicit order gathering gas or have gas in their mouths? sometimes this count may be off by one when units are in the geyser.(?)
    int min_workers_; // how many have the explicit order gather minerals or have minerals in their mouths?
    int workers_clearing_; // How many workers are removing obstructing minerals?
    int workers_distance_mining_; //How many workers are mining at bases we don't yet control?
    int workers_overstacked_; // How many workers are piled on a resource (more than 2x?)
    bool excess_gas_capacity_; // Do we have slots for workers in an extractor?

    // Need to run these every frame.
    void updateGas_Workers();     // Updates the count of our gas workers.
    void updateMin_Workers();     // Updates the count of our min workers. 
    void updateWorkersLongDistanceMining();     // Updates the count of mining workers
    void updateWorkersOverstacked();     //Updates the count of overstacked workers.
    void updateExcessCapacity();    // Updates the if gas has excess capacity. NOTE: Does not check if we WANT that capacity atm.

public:

    bool workerWork(const Unit &u);     // Do worker things. MAIN LOOP.
    bool assignGather(const Unit & unit, const UnitType mine, const int max_dist);     // Checks all Mines of type for undersaturation and assigns Gather. Goes to any undersaturated location, preference for local mine. Returns true on success.
    bool assignClear(const Unit & unit);     // Clears blocking minerals (as determined by BWEB). Check if area is threatened before clearing.  Returns true on success.
    bool attachToNearestMine(ResourceInventory & ri, StoredUnit & miner);     // attaches the miner to the nearest mine in the inventory, and updates the stored_unit. Returns true on success.
    void attachToParticularMine(Stored_Resource &mine, ResourceInventory &ri, StoredUnit &miner);     // attaches the miner to the particular mine and updates the stored unit.
    void attachToParticularMine(Unit & mine, ResourceInventory & ri, StoredUnit & miner);     // Overload: attaches the miner to the particular mine and updates the stored unit.
    bool checkBlockingMinerals(const Unit & unit, UnitInventory & ui) const;     // Returns true if there is a blocking mineral nearby.
    bool checkGasDump() const;     //Returns True if there is an endless dumping place for gas. Does not consider all possible gas outlets.
    bool checkGasOutlet() const;     //Returns True if there is an out for gas.
    bool isEmptyWorker(const Unit & unit) const;     // Checks if unit is carrying minerals.
    bool workerPrebuild(const Unit & unit);     //Workers at their end build location should build there!  May return false if there is an object at the end destination that prohibits building.
    bool workersCollect(const Unit & unit, const int max_dist);     // Workers doing nothing should begin a resource task. Max_dist represents the longest a worker should be transfered.
    bool workersClear(const Unit &unit);     // Workers should clear if conditions are met;  Ought to be reprogrammed to run a periodic cycle to check for clearing. Then grab nearest worker to patch and assign. This policy may lead to inefficent mining.
    bool workersReturn(const Unit &unit);     // Workers should return cargo and forget about their original mine.

    void updateWorkersClearing();     // Updates the count of clearing workers

    //Getters
    int getGasWorkers() const; // how many have the explicit order gathering gas or have gas in their mouths?
    int getMinWorkers() const; // how many have the explicit order gather minerals or have minerals in their mouths?
    bool checkExcessGasCapacity() const; //returns if we have excess gas capacity.
    int getDistanceWorkers() const; //returns the number of distance miners.
    int getOverstackedWorkers() const; //returns the number of distance miners.

    //onFrame
    void onFrame(); //Performs the 1x per frame update of workers.
};

