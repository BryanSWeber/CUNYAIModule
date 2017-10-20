#pragma once

#include <BWAPI.h>
#include "Source\MeatAIModule.h"
#include "Source\Resource_Inventory.h"

//Resource_Inventory functions.
//Creates an instance of the resource inventory class.

Resource_Inventory::Resource_Inventory(){
	// Updates the static locations of minerals and gas on the map. Should only be called on game start.
	Unitset min = Broodwar->getStaticMinerals();
	Unitset geysers = Broodwar->getStaticGeysers();

	for (auto m = min.begin(); m != min.end(); ++m) {
		if ((*m)->getInitialResources() > 8){
			this->addStored_Resource(*m);
		}
	}
	for (auto g = geysers.begin(); g != geysers.end(); ++g) {
		this->addStored_Resource(*g);
	}
}


Resource_Inventory::Resource_Inventory(const Unitset &unit_set) {

	for (const auto & u : unit_set) {
		resource_inventory_.insert({ u, Stored_Resource(u) });
	}

}

// Updates the count of enemy units.
void Resource_Inventory::addStored_Resource(Unit resource) {
	resource_inventory_.insert({ resource, Stored_Resource(resource) });
};

void Resource_Inventory::addStored_Resource(Stored_Resource stored_resource) {
	resource_inventory_.insert({ stored_resource.bwapi_unit_, stored_resource });
};


//Removes enemy units that have died
void Resource_Inventory::removeStored_Resource(Unit resource) {
	resource_inventory_.erase(resource);
};

Position Resource_Inventory::getMeanLocation() const {
	int x_sum = 0;
	int y_sum = 0;
	int count = 0;
	Position out = Position(0, 0);
	for (const auto &u : this->resource_inventory_) {
		x_sum += u.second.pos_.x;
		y_sum += u.second.pos_.y;
		count++;
	}
	if (count > 0) {
		out = Position(x_sum / count, y_sum / count);
	}
	return out;
}


void Resource_Inventory::updateResourceInventorySummary() {

}

//Stored_Resource functions.
Stored_Resource::Stored_Resource() = default;

// We must be able to create Stored_Resource objects as well.
Stored_Resource::Stored_Resource(Unit resource) {

	current_stock_value_ = resource->getResources();
	number_of_miners_ = 0;
	full_resource_ = false;
	occupied_natural_ = false;
	valid_pos_ = true;

	//local_natural_;
    bwapi_unit_ = resource;
	type_ = resource->getType();
	pos_ = resource->getPosition();
}

