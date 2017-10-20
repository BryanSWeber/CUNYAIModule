#pragma once

#include <BWAPI.h>
#include "MeatAIModule.h"

using namespace std;
using namespace BWAPI;

struct Stored_Resource{

	//Creator methods
	Stored_Resource(Unit unit);
	Stored_Resource();


	int current_stock_value_;
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
	Resource_Inventory();
	Resource_Inventory(const Unitset &unit_set);

	//what about their upgrades?
	//Other details?

	int stock_total_;
	int worker_count_;
	int volume_;

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
