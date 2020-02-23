/*
    Tracks tech, upgrades and buildings which permit the creation of more powerful units and upgrade. Does not count buildings which produce, however.
    Backs out the production that must be behind a given unit.
*/

#pragma once
#include <BWAPI.h>
#include "CUNYAIModule.h"
#include <set>

using namespace std;
using namespace BWAPI;

struct Research_Inventory {
    Research_Inventory() {};//need a constructor method.

    map<UpgradeType, int> upgrades_;
    map<TechType, bool> tech_;
    map<UnitType, double> tech_buildings_;
    int tech_stock_ = 0;
    int upgrade_stock_ = 0;
    int research_stock_ = 0;
    int building_stock_ = 0;
    void updateUpgradeTypes(const Player & player); // Updates the upgrades a player has.
    void updateUpgradeStock(); // Updates the stock of upgrades a player has, counted from updateUpgradeTypes.
    void updateTechTypes(const Player & player); // Updates the tech types a player has (primarily Researches like lurker tech).
    void updateTechStock(); // Updates the stock of tech a player has, counted from updateTechTypes.
    void updateResearchBuildings(const Player & player); // This function can be trimmed up a lot. Should add directly into imputed units.
    void updateBuildingStock(const Player & player); // Updates the tech value of the buildings a player has built or reserved spots+money for. Should be called after updateResearchBuildings.
    void updateResearch(const Player & player); // Updates the entire resource inventory for player. Will get confused if there are multiple enemies.
    int countResearchBuildings(const UnitType &ut); // Returns the count of buildings in the research building map.
    bool isTechBuilding(const UnitType & u); // Returns true if the building is a technical building.
};

std::set<UnitType> inferUnits(const std::set<UnitType>& unitsIn); // returns a set of the units that must exist in order to create the unitsIn set.

int inferEarliestPossible(const UnitType & ut); // returns an estimate of the earliest possible time for a particular unit.
