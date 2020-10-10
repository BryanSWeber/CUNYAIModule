#pragma once
#include "CUNYAIModule.h"
#include "PlayerModelManager.h"
#include "FAP\FAP\include\FAP.hpp" // could add to include path but this is more explicit.

using namespace BWAPI;

class TechManager {
private:

    static std::map<UpgradeType, int> upgrade_cycle_; // persistent valuation of buildable upgrades. Should build most valuable one every opportunity.
    static std::map<TechType, int> tech_cycle_; // persistent valuation of buildable techs. Only used to determine gas requirements at the moment.
    static bool tech_avail_; // Set to true (by canMakeTechExpendituresUpdate) if there is a tech I can get.

public:
    static bool updateCanMakeTechExpenditures(); //Checks if a tech item can be purchased (usually has gas).
    static void updateOptimalTech(); //Runs a round of FAP sims and then updates the scores of all the tech in the upgrade cycle. Will consider tech if the upgrade is not full and in the upgrade cartridge. Calls chooseTech();
    static void weightOptimalTech(const bool & condition, const UpgradeType & up, const double & weight);
    static void evaluateWeightsFor(const UpgradeType &up); //Checks all weightUnitSims relevant for unit.
    static bool chooseTech(); //Chooses the next best upgrade to make. subroutine of updateOptimalTech().
    static bool tryToTech(Unit building, UnitInventory &ui, const MapInventory &inv); //Orders the bot to begin a tech (upgrade or research).  Not guaranteed to tech anything.

    static bool Check_N_Upgrade(const UpgradeType & ups, const Unit & unit, const bool & extra_critera);     //Checks if an upgrade can be built, and passes additional boolean criteria. Upgrade must be *reserved* first If all critera are passed, then it performs the upgrade. Requires extra critera.
    static bool Check_N_Research(const TechType & tech, const Unit & unit, const bool & extra_critera);     // Checks if a research can be built, and passes additional boolean critera, if all criteria are passed, then it performs the research. 

    static int getMaxGas(); // Returns the most expensive piece of Tech (in terms of gas) we've considered making.
    static int returnUpgradeRank(const UpgradeType & ut); //What position is this upgrade relative to others? 0 = best.

    static bool checkTechAvail(); // Returns the result of the earlier canMakeTechExpendituresUpdate();
    static bool checkBuildingReady(const UpgradeType up); // Check if a building is standing by to upgrade UP.
    static bool checkBuildingReady(const TechType tech); // Check if a building is standing by to tech the research TECH.
    static bool checkUpgradeFull(const UpgradeType up);   // Check if I can get another copy of an upgrade, ex. melee damage +1 to +2 or +3.
    static bool checkUpgradeUseable(const UpgradeType up);     // Check if I have a unit that could use this upgrade, ex melee damage with only hydras.
    static bool checkResourceSlack(); // checks if resources are slack for tech - floating minerals or larva are gone, or buildings can upgrade and are idle.

    static bool canUpgradeCUNY(const UpgradeType type, const bool checkAffordable = false, const Unit &builder = nullptr);  // a modification of the BWAPI canUpgrade. Has an option to -exclude- cost. Affordability is min, gas, and supply.
    static bool canResearchCUNY(TechType type, const bool checkAffordable = false, const Unit &builder = nullptr);  // a modification of the BWAPI canResearch. Has an option to -exclude- cost. Affordability is min, gas, and supply.
    static bool isInUpgradeCartridge(const UpgradeType & ut); // Returns true if this upgrade is part of the upgrade cartridge, false otherwise. 
    static bool isInResearchCartridge(const TechType & ut); // Returns true if this tech/research is part of the tech/research cartridge, false otherwise. 

    static void clearSimulationHistory(); //Clears the MA history. Should run regularly, ex: every time a relevant comabat unit is made/destroyed to prevent the MA from having weight in dissimilar situations.
    static void Print_Upgrade_FAP_Cycle(const int & screen_x, const int & screen_y); //Diagnostic function to print the assessed value of all the upgrades.

};   
