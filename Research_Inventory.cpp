#pragma once
#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\Research_Inventory.h"
#include "Source\PlayerModelManager.h"
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
    //Revised list. Certain techs are completed at game start. (Ex, infestation, nuclear launch.) 
    //They are generally characterizable as abilities/spells that units have upon construction. This list excludes those researches.  
    //Including them causes the bot to believe it has upgrades finished at the start of the game, which can be misleading.
    vector<int> limited_array = { 1,2,3,5,7,8,9,10,11,13,15,16,17,19,20,21,22,24,25,27,30,31,32 };
    for (auto i : limited_array) //Max number of possible tech types
    {
        bool observed_level = player->hasResearched((TechType)i);
        bool new_level = observed_level || tech_[(TechType)i];
        tech_[(TechType)i] = new_level;
    }
}

void Research_Inventory::updateResearchBuildings(const Player & player) {

    std::set<UnitType> unit_types;
    std::set<UnitType> temp_unit_types;

    Player_Model player_model_to_compare;
    if (player == Broodwar->self())
        player_model_to_compare = CUNYAIModule::friendly_player_model;
    else
        player_model_to_compare = CUNYAIModule::enemy_player_model;


    for (auto &i : player_model_to_compare.units_.unit_map_) {// for every unit type they have or have imputed.
        temp_unit_types.insert(i.second.type_);
    }
    for (auto &i : player_model_to_compare.imputedUnits_.unit_map_) {// for every unit type I have imputed.
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
        if ((u.isBuilding() || u.isAddon()) && !CUNYAIModule::isFightingUnit(u) && u != UnitTypes::Zerg_Creep_Colony && u != UnitTypes::Protoss_Pylon && u != UnitTypes::Terran_Supply_Depot && u != UnitTypes::Protoss_Nexus && u != UnitTypes::Terran_Command_Center && u != UnitTypes::Zerg_Hatchery)
            tech_buildings_[u] = max(CUNYAIModule::countUnits(u, player_model_to_compare.units_) + CUNYAIModule::countUnits(u, player_model_to_compare.imputedUnits_), 1); // If a required building is present. If it has been destroyed then we have to rely on the visible count of them, though.
    }

    for (auto i : upgrades_) {
        if (i.second > 0)
            tech_buildings_[i.first.whatsRequired(i.second)] = (i.first.whatsRequired(i.second) != UnitTypes::None); // requirements might be "none".
    }
    for (auto i : tech_) {
        if (i.second)
            tech_buildings_[i.first.whatResearches()] = (i.first.whatResearches() != UnitTypes::None); // requirements might be "none".
    }

    for (auto &i : tech_buildings_) {// for every unit type they have.
        i.second = max(CUNYAIModule::countUnits(i.first, player_model_to_compare.units_), i.second); // we update the count of them that we hve seen so far.
        //if (CUNYAIModule::Count_Units(i.first, player_model_to_compare.units_) < i.second && player != Broodwar->self()) Player_Model::imputeUnits(Stored_Unit(i.first));
    }

}

void Research_Inventory::updateUpgradeStock() {
    int temp_upgrade_stock = 0;
    for (auto i : upgrades_)//Max number of possible upgrade types
    {
        int number_of_times_factor_triggers = (i.second * (i.second + 1)) / 2 - i.second; // 0, 1, 2 maximum...
        int value = i.first.mineralPrice() + static_cast<int>(i.first.gasPrice() * 1.25);
        temp_upgrade_stock += i.second * value + number_of_times_factor_triggers * static_cast<int>(i.first.mineralPriceFactor() + i.first.gasPriceFactor() * 1.25);
    }
    upgrade_stock_ = temp_upgrade_stock;
}

void Research_Inventory::updateTechStock() {
    int temp_tech_stock = 0;
    for (auto i : tech_)//Max number of possible upgrade types
    {
        int value = static_cast<int>(i.first.mineralPrice() + i.first.gasPrice() * 1.25);
        temp_tech_stock += static_cast<int>(i.second) * value;
    }
    tech_stock_ = temp_tech_stock;
}

void Research_Inventory::updateBuildingStock() {
    int temp_building_stock = 0;
    for (auto i : tech_buildings_)//Max number of possible upgrade types
    {
        int value = Stored_Unit(i.first).stock_value_;
        temp_building_stock += i.second * value; // include value of drone if race is zerg.
    }
    building_stock_ = temp_building_stock;
}


void Research_Inventory::updateResearch(const Player & player)
{
    updateUpgradeTypes(player);
    updateTechTypes(player);
    updateResearchBuildings(player);
    updateUpgradeStock();
    updateTechStock();
    updateBuildingStock();

    research_stock_ = tech_stock_ + upgrade_stock_ + building_stock_;
}


