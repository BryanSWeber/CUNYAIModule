#pragma once
#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\ResearchInventory.h"
#include "Source\PlayerModelManager.h"
#include "Source\UnitInventory.h"
#include <set>
#include "Source\Diagnostics.h"

using namespace std;
using namespace BWAPI;

void ResearchInventory::updateUpgradeTypes(const Player &player) {
    for (int i = 0; i < 63; i++)//Max number of possible upgrade types
    {
        int observed_level = player->getUpgradeLevel((UpgradeType)i);
        int new_level = max(observed_level, upgrades_[(UpgradeType)i]);
        upgrades_[(UpgradeType)i] = new_level;
    }
}

void ResearchInventory::updateTechTypes(const Player &player) {
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

void ResearchInventory::updateResearchBuildings(const Player & player) {

    std::set<UnitType> unit_types;
    std::set<UnitType> temp_unit_types;

    PlayerModel player_model_to_compare;
    if (player == Broodwar->self())
        player_model_to_compare = CUNYAIModule::friendly_player_model;
    else
        player_model_to_compare = CUNYAIModule::enemy_player_model;


    for (auto &i : player_model_to_compare.units_.unit_map_) {// for every unit type they have or have imputed.
        temp_unit_types.insert(i.second.type_);
    }
    for (auto &i : player_model_to_compare.unseen_units_) {// for every unit type I have imputed.
        temp_unit_types.insert(i.first);
    }

    unit_types.insert(temp_unit_types.begin(), temp_unit_types.end());

    unit_types = inferUnits(unit_types);

    for (auto u : unit_types) {
        if ( isTechBuilding(u) )
            tech_buildings_[u] = max(CUNYAIModule::countUnits(u, player_model_to_compare.units_), 1); // If a required building is present. If it has been destroyed then we have to rely on the visible count of them, though.
    }
    for (auto i : upgrades_) {
        if (i.second && isTechBuilding(i.first.whatsRequired(i.second)))
            tech_buildings_[i.first.whatsRequired(i.second)] = max(tech_buildings_[i.first.whatsRequired(i.second)], 1); // requirements might be "none".
    }
    for (auto i : tech_) {
        if (i.second && isTechBuilding(i.first.whatResearches()))
            tech_buildings_[i.first.whatResearches()] = max(tech_buildings_[i.first.whatResearches()],1); // requirements might be "none".
    }

    for (auto &i : tech_buildings_) {// for every unit type they have.
        int value = max(CUNYAIModule::countUnits(i.first, player_model_to_compare.units_), i.second);
        if(value > 0)
          i.second = value; // we update the count of them that we hve seen so far.
        //if (CUNYAIModule::Count_Units(i.first, player_model_to_compare.units_) < i.second && player != Broodwar->self()) PlayerModel::imputeUnits(StoredUnit(i.first));
    }

}

void ResearchInventory::updateUpgradeStock() {
    int temp_upgrade_stock = 0;
    for (auto i : upgrades_)//Max number of possible upgrade types
    {
        int number_of_times_factor_triggers = (i.second * (i.second + 1)) / 2 - i.second; // 0, 1, 2 maximum...
        int value = i.first.mineralPrice() + static_cast<int>(i.first.gasPrice() * 1.25);
        temp_upgrade_stock += i.second * value + number_of_times_factor_triggers * static_cast<int>(i.first.mineralPriceFactor() + i.first.gasPriceFactor() * 1.25);
    }
    upgrade_stock_ = temp_upgrade_stock;
}

void ResearchInventory::updateTechStock() {
    int temp_tech_stock = 0;
    for (auto i : tech_)//Max number of possible upgrade types
    {
        int value = static_cast<int>(i.first.mineralPrice() + i.first.gasPrice() * 1.25);
        temp_tech_stock += static_cast<int>(i.second) * value;
    }
    tech_stock_ = temp_tech_stock;
}

void ResearchInventory::updateBuildingStock(const Player & player) {
    PlayerModel player_model_to_compare;
    if (player == Broodwar->self())
        player_model_to_compare = CUNYAIModule::friendly_player_model;
    else
        player_model_to_compare = CUNYAIModule::enemy_player_model;

    int temp_building_stock = 0;
    for (auto i : player_model_to_compare.researches_.tech_buildings_)//Max number of possible upgrade types
    {
        int value = StoredUnit(i.first).stock_value_;
        temp_building_stock += i.second * value; // include value of drone if race is zerg.
    }

    if (player == Broodwar->self()) {
        for (auto i : CUNYAIModule::my_reservation.getReservedBuildings())
            if(isTechBuilding(i.second))
                temp_building_stock += i.second * StoredUnit(i.second).stock_value_; // include value of drone if race is zerg.
    }

    building_stock_ = temp_building_stock;
}


void ResearchInventory::updateResearch(const Player & player)
{
    updateUpgradeTypes(player);
    updateTechTypes(player);
    updateResearchBuildings(player);
    updateUpgradeStock();
    updateTechStock();
    updateBuildingStock(player);

    research_stock_ = tech_stock_ + upgrade_stock_ + building_stock_;

    if (Broodwar->getFrameCount() % (60 * 24) == 0) {
        Diagnostics::DiagnosticWrite("What do we think is happening for researches?");
        Diagnostics::DiagnosticWrite("This is the research units of an %s:", player->isEnemy(Broodwar->self()) ? "ENEMY" : "NOT ENEMY");
        for (auto ut : tech_buildings_)
            Diagnostics::DiagnosticWrite("They have %d of %s", ut.second, ut.first.c_str());
    }
}

int ResearchInventory::countResearchBuildings(const UnitType & ut)
{
    auto matching_unseen_units = tech_buildings_.find(ut);
    if (matching_unseen_units != tech_buildings_.end())
        return tech_buildings_.at(ut); // reduce the count of this unite by one.
    return 0;
}

bool ResearchInventory::isTechBuilding(const UnitType &u) {
    return (u.isBuilding() || u.isAddon()) && (!u.upgradesWhat().empty() || !u.researchesWhat().empty()) && !u.isResourceDepot();
}

map<UpgradeType, int> ResearchInventory::getUpgrades() const
{
    return upgrades_;
}

map<TechType, bool> ResearchInventory::getTech() const
{
    return tech_;
}

map<UnitType, int> ResearchInventory::getTechBuildings() const
{
    return tech_buildings_;
}

int ResearchInventory::getUpLevel(const UpgradeType & up) const
{
    if (upgrades_.find(up) != upgrades_.end())
        return upgrades_.at(up);
    else 
        return 0;
}

bool ResearchInventory::hasTech(const TechType & tech)
{
    if (tech_.find(tech) != tech_.end())
        return tech_[tech];
    else
        return false;
}

std::set<UnitType> inferUnits(const std::set<UnitType>& unitsIn){
    std::set<UnitType> temp_unit_types;
    std::set<UnitType> unitsOut;

    int n = 0;
    while (n < 5) {
        temp_unit_types.clear();
        for (auto u : unitsIn) { // for any unit that is needed in the construction of the units above.
            for (auto i : u.requiredUnits()) {
                temp_unit_types.insert(i.first);
            }
        }
        unitsOut.insert(temp_unit_types.begin(), temp_unit_types.end()); // this could be repeated with some clever stop condition. Or just crudely repeated a few times.
        n++;
    }
    return unitsOut;
};

int inferEarliestPossible(const UnitType & ut) {
    std::set<UnitType> temp_unit_types;
    std::set<UnitType> unitsOut;

    int fastest_possible_pool = 24*(2 * 60 + 14); //just a guess, 2:14.
    int build_time = fastest_possible_pool - UnitTypes::Zerg_Spawning_Pool.buildTime();

    int n = 0;
    while (n < 5) {
        temp_unit_types.clear();
        for ( auto u : inferUnits({ ut }) ) { // for any unit that is needed in the construction of the ut above.
            for (auto i : u.requiredUnits()) {
                if(!i.first.isResourceDepot() && !i.first.isWorker())
                    temp_unit_types.insert(i.first);
            }
        }
        unitsOut.insert(temp_unit_types.begin(), temp_unit_types.end()); // this could be repeated with some clever stop condition. Or just crudely repeated a few times.
        n++;
    }

    if (ut.getRace() != Races::Zerg)
        unitsOut.insert(ut.getRace().getSupplyProvider());

    for (auto & u : unitsOut) {
        build_time += u.buildTime();
    }




    Diagnostics::DiagnosticWrite("The earliest possible %s is %d frames, by my guess.", ut.c_str(), build_time);
    for(auto u : unitsOut)
        Diagnostics::DiagnosticWrite("I'd have to build a %s, takes %d frames.", u.c_str(), u.buildTime());
    Diagnostics::DiagnosticWrite("And you can't really build anything before %d frames.", fastest_possible_pool - UnitTypes::Zerg_Spawning_Pool.buildTime());

    return build_time;
};

