#pragma once
#include "CUNYAIModule.h"
#include "PlayerModelManager.h"
#include "FAP\FAP\include\FAP.hpp" // could add to include path but this is more explicit.

using namespace BWAPI;

class TechManager {
    static std::map<UpgradeType, int> upgrade_cycle_; // persistent valuation of buildable upgrades. Should build most valuable one every opportunity.
    static std::map<TechType, int> tech_cycle_; // persistent valuation of buildable techs. Only used to determine gas requirements at the moment.
    static bool tech_avail_;
    static int max_gas_value_;
public:
    //Checks if an upgrade can be built, and passes additional boolean criteria.  If all critera are passed, then it performs the upgrade. Requires extra critera.
    static bool Check_N_Upgrade(const UpgradeType & ups, const Unit & unit, const bool & extra_critera);
    // Checks if a research can be built, and passes additional boolean critera, if all criteria are passed, then it performs the research. 
    static bool Check_N_Research(const TechType & tech, const Unit & unit, const bool & extra_critera);
    static void Print_Upgrade_FAP_Cycle(const int & screen_x, const int & screen_y);
    static bool updateTech_Avail();
    static void updateOptimalTech();
    static void updateMaxGas();
    // Return True if there's something I might want to make that is a tech building.
    static bool checkTechAvail();
    // Check if building is made and complete.
    static bool checkBuildingReady(const UpgradeType up);
    // Check if I can get another copy of an upgrade, ex. melee damage +1 to +2 or +3.
    static bool checkUpgradeFull(const UpgradeType up);
    // Overload
    static bool checkBuildingReady(const TechType tech);
    // Check if I have a unit that could use this upgrade, ex melee damage with only hydras.
    static bool checkUpgradeUseable(const UpgradeType up);
    static bool Tech_BeginBuildFAP(Unit building, Unit_Inventory &ui, const Map_Inventory &inv);
    static void clearSimulationHistory(); //Clears the MA history. Should run every time a relevant comabat unit is made/destroyed to prevent the MA from having weight in dissimilar situations.
    static int returnTechRank(const UpgradeType & ut);
    // Returns the most expensive piece of Tech we've considered making.
    static int getMaxGas();
    // a modification of the BWAPI canUpgrdae. Has an option to -exclude- cost. Affordability is min, gas, and supply.
    static bool canUpgradeCUNY(const UpgradeType type, const bool checkAffordable = false, const Unit &builder = nullptr);
    // a modification of the BWAPI canResearch (canTech in my parlance). Has an option to -exclude- cost. Affordability is min, gas, and supply.
    static bool canTech(TechType type, const bool checkAffordable = false, const Unit &builder = nullptr);
    // checks if resources are slack for tech - floating minerals or larva are gone.
    static bool checkResourceSlack();
};   
