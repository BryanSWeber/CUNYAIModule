#pragma once
#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\Research_Inventory.h"
#include "Source\Unit_Inventory.h"
#include <set>

using namespace std;
using namespace BWAPI;

void Research_Inventory::updateUpgradeTypes(const Player &player) {
    for (int i = 0; i < 63; i++)//Max number of possible upgrade types
    {
        int observed_level = player->getUpgradeLevel((UpgradeType)i);
        int new_level = max(observed_level, upgrades_[(UpgradeType)i]);
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

void Research_Inventory::updateResearchBuildings(const Unit_Inventory &ei) {

    std::set<UnitType> unit_types;
    std::set<UnitType> temp_unit_types;

    for (auto &i:ei.unit_inventory_) {// for every unit type they have.
        temp_unit_types.insert(i.second.type_);
    }

    unit_types.insert(temp_unit_types.begin(), temp_unit_types.end());

    int n = 0;
    while (n < 4) {
        temp_unit_types.clear();
        for (auto u : unit_types) { // for any unit that is needed in the construction of the units above.
            for (auto i : u.requiredUnits()) {
                temp_unit_types.insert(i.first);
            }
        }
        unit_types.insert(temp_unit_types.begin(), temp_unit_types.end()); // this could be repeated with some clever stop condition. Or just crudely repeated a few times.
        n++;
    }

    for (auto u : unit_types) {
        if (u.isBuilding() && (!u.upgradesWhat().empty() || !u.researchesWhat().empty()) && u != UnitTypes::Zerg_Hatchery ) buildings_[u] = 1; // If a required 
    }

    for (auto i : upgrades_) {
        if (i.second > 0) buildings_[i.first.whatsRequired(i.second)] = i.first.whatsRequired(i.second) != UnitTypes::None; // requirements might be "none".
    }
    for (auto i : tech_) {
        if (i.second > 0) buildings_[i.first.whatResearches()] = i.first.whatResearches() != UnitTypes::None; // requirements might be "none".
    }

    for (auto &i : buildings_) {// for every unit type they have.
        i.second = max(CUNYAIModule::Count_Units(i.first, ei), i.second);
    }

}

void Research_Inventory::updateUpgradeStock() {
    int temp_upgrade_stock = 0;
    for (auto i : upgrades_)//Max number of possible upgrade types
    {
        int number_of_times_factor_triggers = (i.second * (i.second + 1)) / 2;
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
        int value = i.first.mineralPrice() + i.first.gasPrice() * 1.25 + (i.first.getRace() == Races::Zerg) * (UnitTypes::Zerg_Drone.mineralPrice() + 25 * UnitTypes::Zerg_Drone.supplyRequired());
        temp_building_stock +=  i.second * value; // include value of drone if race is zerg.
    }
    building_stock_ = temp_building_stock;
}


void Research_Inventory::updateResearch(const Player & player, const Unit_Inventory &ei)
{
    updateUpgradeTypes(player);
    updateTechTypes(player);
    updateResearchBuildings(ei);
    updateUpgradeStock();
    updateTechStock();
    updateBuildingStock();

    research_stock_ = tech_stock_ + upgrade_stock_ + building_stock_;
}


