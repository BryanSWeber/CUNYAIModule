#pragma once

#include <BWAPI.h>
#include "MeatAIModule.h"
#include "Unit_Inventory.h"

using namespace std;
using namespace BWAPI;

//struct Stored_Unit;

struct Stored_Resource{

	//Creator methods
	Stored_Resource();
	Stored_Resource(Unit unit);
	bool isBeingMinedBy(const Unit unit);

	int current_stock_value_;
	int number_of_miners_;

	bool occupied_natural_;
	bool full_resource_;
	bool valid_pos_;

	vector<Unit> miner_inventory_; //what miners are attached to this resource?
	void addMiner(Unit miner);
	void removeMiner(Unit miner);
	//void addMiner(Unit_Inventory::Stored_Unit miner);

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

	// Updates the count of enemy units.
	void addStored_Resource(Unit unit);
	void addStored_Resource(Stored_Resource stored_resource);

	//Removes Resource
	void removeStored_Resource(Unit unit);

	//Updates summary of inventory, stored here.
	void updateResourceInventorySummary();

	Position getMeanLocation() const;
	Position getMeanBuildingLocation() const;
	Position getMeanCombatLocation() const;
};
