#pragma once
#include "CUNYAIModule.h"
#include "PlayerModelManager.h"
#include "FAP\FAP\include\FAP.hpp" // could add to include path but this is more explicit.

using namespace BWAPI;

class TechManager {

public:
    std::map<UpgradeType, int> upgrade_cycle;

    static bool Tech_Avail();
    bool Tech_Begin(Unit building, Unit_Inventory &ui, const Map_Inventory &inv);
    bool Tech_BeginBuildFAP(Unit building, Unit_Inventory &ui, const Map_Inventory &inv);
};