#pragma once
#include "CUNYAIModule.h"
#include "PlayerModelManager.h"
#include "FAP\FAP\include\FAP.hpp" // could add to include path but this is more explicit.

using namespace BWAPI;

class TechManager {
    static std::map<UpgradeType, int> upgrade_cycle_; // persistent valuation of buildable upgrades. Should build most valuable one every opportunity.
    static bool tech_avail_;
public:
    //Checks if an upgrade can be built, and passes additional boolean criteria.  If all critera are passed, then it performs the upgrade. Requires extra critera.
    static bool Check_N_Upgrade(const UpgradeType & ups, const Unit & unit, const bool & extra_critera);
    // Checks if a research can be built, and passes additional boolean critera, if all criteria are passed, then it performs the research. 
    static bool Check_N_Research(const TechType & tech, const Unit & unit, const bool & extra_critera);
    static void Print_Upgrade_FAP_Cycle(const int & screen_x, const int & screen_y);
    static bool updateTech_Avail();
    static void updateOptimalTech();
    static bool checkTechAvail();
    static bool checkBuildingReady(const UpgradeType up);
    static bool checkUpgradeFull(const UpgradeType up);
    static bool checkUpgradeUseable(const UpgradeType up);
    bool Tech_BeginBuildFAP(Unit building, Unit_Inventory &ui, const Map_Inventory &inv);
    static void clearSimulationHistory(); //Clears the MA history. Should run every time a relevant comabat unit is made/destroyed to prevent the MA from having weight in dissimilar situations.
    static int returnTechRank(const UpgradeType & ut);
};