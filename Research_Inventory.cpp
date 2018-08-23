#pragma once
#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\Research_Inventory.h"

using namespace std;
using namespace BWAPI;

void Research_Inventory::updateUpgradeTypes(const Player &player) {
    for (int i = 0; i < 63; i++)//Max number of possible upgrade types
    {
        int observed_level = player->getUpgradeLevel((UpgradeType)i);
        int new_level = max(observed_level, int(upgrades_[(UpgradeType)i]));
        upgrades_[(UpgradeType)i] = new_level;
    }
}

void Research_Inventory::updateTechTypes(const Player &player) {
    for (int i = 0; i < 47; i++) //Max number of possible tech types
    {
        int observed_level = player->hasResearched((TechType)i);
        int new_level = max(observed_level, int(tech_[(TechType)i]));
        tech_[(TechType)i] = new_level;
    }
}

void Research_Inventory::updateResearchBuildings() {
    for (auto i : upgrades_) {
        if (i.second > 0) buildings_[i.first.whatsRequired()] = true;
    }
    for (auto i : tech_) {
        if (i.second > 0) buildings_[i.first.whatResearches()] = true;
    }
}

void Research_Inventory::updateUpgradeStock() {
    int temp_upgrade_stock = 0;
    for (auto i : upgrades_)//Max number of possible upgrade types
    {
        int number_of_times_factor_triggers = (i.second*(i.second + 1)) / 2;
        temp_upgrade_stock += i.second * (i.first.mineralPrice() + i.first.gasPrice() * 1.25) + number_of_times_factor_triggers * (i.first.mineralPriceFactor() + i.first.gasPriceFactor() * 1.25);
    }
    upgrade_stock_ = temp_upgrade_stock;
}

void Research_Inventory::updateTechStock() {
    int temp_tech_stock = 0;
    for (auto i : tech_)//Max number of possible upgrade types
    {
        temp_tech_stock += i.second * (i.first.mineralPrice() + i.first.gasPrice() * 1.25);
    }
    tech_stock_ = temp_tech_stock;
}

void Research_Inventory::updateBuildingStock() {
    int temp_building_stock = 0;
    for (auto i : buildings_)//Max number of possible upgrade types
    {
        temp_building_stock += i.first.mineralPrice() + i.first.gasPrice() * 1.25 + i.first.getRace() == Races::Zerg * (UnitTypes::Zerg_Drone.mineralPrice() + 25 * UnitTypes::Zerg_Drone.supplyRequired() ); // include value of drone if race is zerg.
    }
    building_stock_ = temp_building_stock;
}

void Research_Inventory::updateResearchStock() {
    updateTechStock();
    updateUpgradeStock();
    updateBuildingStock();
    research_stock_ = tech_stock_ + upgrade_stock_ + building_stock_;
}