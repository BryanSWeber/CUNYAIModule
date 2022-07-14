#pragma once
// Class engages in several types of combat simulation. Serves as a wrapper between FAP and my existing inventories.
// The first player should always be the "friendly" one. The second player will always be the hostile one.
#include "Source/FAP/FAP/include/FAP.hpp"
#include "BWAPI.h"
#include "Source/CUNYAIModule.h"
#include "Source/UnitInventory.h"
#include <random> // C++ base random is low quality.

class CombatSimulator {

private:
    std::default_random_engine generator_;  //Will be used to obtain a seed for the random number engine
    static const int miniMap_ = 60; // SC Screen size is 680 X 240

    FAP::FastAPproximation<StoredUnit*> internalFAP_; //Simulates the fight with some random perturbance.
    int friendly_fap_score_;
    int enemy_fap_score_;
    double unitWeight(FAP::FAPUnit<StoredUnit*> FAPunit); //Grabs the value of a unit from the FAP object.

    auto createFAPVersion(const StoredUnit u, const ResearchInventory & ri); // Returns a FAP stored Unit that has measured all details from the research inventory.
    auto createModifiedFAPVersion(const StoredUnit u, const ResearchInventory &ri, const Position & chosen_pos = Positions::Origin, const UpgradeType &upgrade = UpgradeTypes::None, const TechType &tech = TechTypes::None); // creates a FAP object for the stored unit with new contents.

    Position positionMiniFAP(const bool friendly);
    Position positionMCFAP(const StoredUnit su);

public:
    void addExtraUnitToSimulation(StoredUnit u, bool friendly = true); // Squeeze in this extra unit into the sim as a counterfactual. Adjust if friendly or not.
    void addPlayersToMiniSimulation(const UpgradeType &upgrade = UpgradeTypes::None, const TechType &tech = TechTypes::None); // Add both players into the simulation but give the friendly player some counterfactual technologies.
    void addPlayersToSimulation(); //Add both friendly and enemy players to simulation as is.

    const auto getFriendlySim(); //Get the friendly sim - for looking only. Make sure you have Ran the sim before using.
    const auto getEnemySim(); //Get the enemy sim - for looking only. Make sure you have RAN the sim before using.

    int getFriendlyScore(); //Returns the first player, the friendly player's score.
    int getEnemyScore(); //Returns the second player, the enemy player's score. Duration is an optional number of frames.
    void runSimulation(int duration = FAP_SIM_DURATION); //runs simulation updates scores.

    bool unitDeadInFuture(const StoredUnit &unit, const int & number_of_frames_voted_death) const; // returns true if the unit has a MA forcast that implies it will be alive in X frames.
};