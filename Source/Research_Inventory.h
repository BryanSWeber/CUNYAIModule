#pragma once
#include <BWAPI.h>
#include "CUNYAIModule.h"

using namespace std;
using namespace BWAPI;

struct Research_Inventory {
    Research_Inventory() {};//need a constructor method.

    map<UpgradeType, int> upgrades_;
    map<TechType, bool> tech_;
    map<UnitType, int> tech_buildings_;
    int tech_stock_ = 0;
    int upgrade_stock_ = 0;
    int research_stock_ = 0;
    int building_stock_ = 0;
    void updateUpgradeTypes(const Player & player);
    void updateUpgradeStock();
    void updateTechTypes(const Player & player);
    void updateTechStock();
    void updateResearchBuildings(const Player & player); // This function can be trimmed up a lot. Should add directly into imputed units.
    void updateBuildingStock();
    void updateResearch(const Player & player);
    int countResearchBuildings(const UnitType &ut); // Returns the count of buildings in the research building map.
};