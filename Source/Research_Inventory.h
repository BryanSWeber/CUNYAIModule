#pragma once
#include <BWAPI.h>
#include "CUNYAIModule.h"

using namespace std;
using namespace BWAPI;

struct Research_Inventory {
    Research_Inventory() {};//need a constructor method.

    map<UpgradeType, unsigned int> upgrades_;
    map<TechType, bool> tech_;
    map<UnitType, bool> buildings_;
    unsigned int tech_stock_ = 0;
    unsigned int upgrade_stock_ = 0;
    unsigned int research_stock_ = 0;
    unsigned int building_stock_ = 0;
    void updateUpgradeTypes(const Player & player);
    void updateUpgradeStock();
    void updateTechTypes(const Player & player);
    void updateTechStock();
    void updateResearchBuildings();
    void updateBuildingStock();
    void updateResearchStock();
};