#pragma once
// Class engages in several types of combat simulation. Serves as a wrapper between FAP and my existing inventories.
// The first player should always be the "friendly" one. The second player will always be the hostile one.
#include "FAP/FAP/include/FAP.hpp"
#include "BWAPI.h"
#include "CUNYAIModule.h"
#include "UnitInventory.h"
#include <random> // C++ base random is low quality.

struct StoredUnit;
constexpr int FAP_SIM_DURATION = 24 * 5; // set FAP sim durations.

class CombatSimulator {

private:
    std::default_random_engine generator_;  //Will be used to obtain a seed for the random number engine
    static const int miniMap_ = 60; // SC Screen size is 680 X 240

    FAP::FastAPproximation<StoredUnit*> internalFAP_; //Simulates the fight with some random perturbance.
    int friendly_fap_score_;
    int enemy_fap_score_;
    double unitWeight(FAP::FAPUnit<StoredUnit*> FAPunit); //Grabs the value of a unit from the FAP object.

    auto createFAPVersion(StoredUnit &u, const ResearchInventory &ri); // Returns a FAP stored Unit that has measured all details from the research inventory.
    auto createModifiedFAPVersion(StoredUnit &u, const ResearchInventory &ri, const Position &chosen_pos = Positions::Origin, const UpgradeType &upgrade = UpgradeTypes::None, const TechType &tech = TechTypes::None); // creates a FAP object for the stored unit with new contents.

    Position createPositionMiniFAP(const bool friendly);
    Position createPositionMCFAP(const StoredUnit su);

public:
    void addExtraUnitToSimulation(StoredUnit u, bool friendly = true); // Squeeze in this extra unit into the sim as a counterfactual. Adjust if friendly or not.
    void addPlayersToMiniSimulation(const UpgradeType &upgrade = UpgradeTypes::None, const TechType &tech = TechTypes::None); // Add both players into the simulation but give the friendly player some counterfactual technologies.
    void addPlayersToSimulation(); //Add both friendly and enemy players to simulation as is.

    std::vector<FAP::FAPUnit<StoredUnit*>> getFriendlySim(); //Get the friendly sim. Make sure you have Ran the sim before using. Careful, as it will your sim.
    std::vector<FAP::FAPUnit<StoredUnit*>> getEnemySim(); //Get the enemy sim. Make sure you have RAN the sim before using.  Careful, as it will your sim.

    int getFriendlyScore() const; //Returns the first player, the friendly player's score.
    int getEnemyScore() const; //Returns the second player, the enemy player's score. Duration is an optional number of frames.
    int getScoreGap(bool friendly = true) const;
    void runSimulation(int duration = FAP_SIM_DURATION); //runs simulation updates scores.
};