#pragma once
#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\Research_Inventory.h"

using namespace std;
using namespace BWAPI;

void Research_Inventory::updateUpgradeTypes(const Player &player) {
    for (int i = 0; i < 61; i++)//Max number of possible upgrade types
    {
        upgrades_.insert(pair<UpgradeType, unsigned int>((UpgradeType)i, player->getUpgradeLevel((UpgradeType)i)));
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

void Research_Inventory::updateTechTypes(const Player &player) {
    for (int i = 0; i < 45; i++) //Max number of possible tech types
    {
        tech_.insert(pair<TechType, bool>((TechType)i, player->hasResearched((TechType)i)));
    }
}

void Research_Inventory::updateTechStock() {
    int temp_tech_stock = 0;
    for (auto i : tech_)//Max number of possible upgrade types
    {
        temp_tech_stock += i.second * (i.first.mineralPrice() + i.first.gasPrice() * 1.25);
    }
    tech_stock_ = temp_tech_stock;
}

void Research_Inventory::updateResearchStock() {
    updateTechStock();
    updateResearchStock();
    research_stock_ = tech_stock_ + research_stock_;
}