#pragma once
#include "CUNYAIModule.h"
#include "PlayerModelManager.h"
#include "FAP\FAP\include\FAP.hpp" // could add to include path but this is more explicit.

using namespace BWAPI;

class TechManager {
public:
    static std::map<UpgradeType, int> upgrade_cycle; // persistent valuation of buildable upgrades. Should build most valuable one every opportunity.

    static void updateTech_Avail();
    static bool tech_avail_;
    bool Tech_BeginBuildFAP(Unit building, Unit_Inventory &ui, const Map_Inventory &inv);
};