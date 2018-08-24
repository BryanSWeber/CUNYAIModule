#pragma once
#include <BWAPI.h>
#include "CUNYAIModule.h"

using namespace std;
using namespace BWAPI;

struct Research_Inventory {
    Research_Inventory() {};//need a constructor method.

    map<UpgradeType, int> upgrades_;
    map<TechType, bool> tech_;
    map<UnitType, int> buildings_;
    int tech_stock_ = 0;
    int upgrade_stock_ = 0;
    int research_stock_ = 0;
    int building_stock_ = 0;
    void updateUpgradeTypes(const Player & player);
    void updateUpgradeStock();
    void updateTechTypes(const Player & player);
    void updateTechStock();
    void updateResearchBuildings(const Unit_Inventory &ei); // This function can be trimmed up a lot.
    void updateBuildingStock();
    void updateResearch(const Player & player, const Unit_Inventory &ei);

};