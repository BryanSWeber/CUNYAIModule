#pragma once

// Contains Build Objects - tracks things that are important in openings and basic plans.
// Core Build Orders are hardcoded starting parameters for build objects, and the build objects are mutable afterwards as the bot adapts to the situation.

#include <BWAPI.h>
#include "CUNYAIModule.h"
#include <string>

enum BuildParameterNames {
    ArmyAlpha,
    TechAlpha,
    EconAlpha,
    AdaptationRate,
    GasProportion,
    SupplyRatio
};

enum BuildEnums {
    Lurker,
    PoolFive,
    PoolSeven,
    OneBaseMuta,
    TwoBaseMuta,
    //FourHatchCarapace,
    FourHatchBeforePool
};

class BuildOrderElement {
private:

    UnitType unit_in_queue_;
    UpgradeType upgrade_in_queue_;
    TechType research_in_queue_;

public:

    BuildOrderElement(UnitType unit) {
        unit_in_queue_ = unit;
        upgrade_in_queue_ = UpgradeTypes::None;
        research_in_queue_ = TechTypes::None;
    };

    BuildOrderElement(UpgradeType up) {
        unit_in_queue_ = UnitTypes::None;
        upgrade_in_queue_ = up;
        research_in_queue_ = TechTypes::None;
    };

    BuildOrderElement(TechType tech) {
        unit_in_queue_ = UnitTypes::None;
        upgrade_in_queue_ = UpgradeTypes::None;
        research_in_queue_ = tech;
    };

    UnitType BuildOrderElement::getUnit() const {
        return unit_in_queue_;
    };

    UpgradeType BuildOrderElement::getUpgrade() const {
        return upgrade_in_queue_;
    };

    TechType BuildOrderElement::getResearch() const {
        return research_in_queue_;
    };

};

class BuildOrderSetup {
private:
    vector<BuildOrderElement> queueBuild_;  // What do we need to make?
    double parameterValues_[6]; //See Enum BuildParameters for order.
    BuildEnums buildName_;
public:
    BuildOrderSetup();
    BuildOrderSetup(vector<BuildOrderElement> q, double parameterArray[6], BuildEnums n); //creationMethod
    vector<BuildOrderElement> getSetupQueue();
    double getCoreParameters(int i);
    BuildEnums getBuildEnum();
    int getParameterCount(); //How Many Parameters are there?
};

class Build {
private:

    int cumulative_gas_;
    int cumulative_minerals_;
    vector<BuildOrderElement> queueBuild_;  // What do we need to make?
    double parameterValues_[6]; //See Enum BuildParameters for order.
    BuildEnums buildName_;
    void updateCumulativeResources(); // Run after setting or modifying build order. Run every time after every command.
    int getParameterCount(); //How many parameters are there in this build? Will be useful if we ever choose to add a new parameter.

public:

    void initializeBuildOrder(BuildOrderSetup b); // Starts the build to a predefined setup.
    void clearRemainingBuildOrder(const bool diagnostic); // empties the build order.

    void updateRemainingBuildOrder(const UpgradeType & ups); // drops item from list as complete.
    void updateRemainingBuildOrder(const TechType & research);// drops item from list as complete.
    void updateRemainingBuildOrder(const UnitType & ut); // drops item from list as complete.

    void addBuildOrderElement(const UpgradeType & ups); // adds an element to the end of the queue.
    void addBuildOrderElement(const TechType & research);// adds an element to the end of the queue.
    void addBuildOrderElement(const UnitType & ut); // adds an element to the end of the queue.

    void retryBuildOrderElement(const UnitType & ut); // Adds the element to the front of the queue, presumably to retry something that went wrong.
    void retryBuildOrderElement(const UpgradeType & up); // Adds the element to the front of the queue, presumably to retry something that went wrong.

    void announceBuildingAttempt(const UnitType ut) const;  // Announce that a building has been attempted.
    int countTimesInBuildQueue(const UnitType ut) const;  //Counts the number of this unit in the BO. Needed to evaluate extractor-related items.

    bool isEmptyBuildOrder() const; // Returns true if empty.

    double getParameter(const BuildParameterNames b) const; //Getting specific parameters by name.
    BuildEnums getBuildEnum() const; //Gets the name of the build.
    BuildOrderElement getNext() const; //Returns next element, otherwise returns unittype none.
    vector<BuildOrderElement> getQueue() const; //returns the entire queue.

    int getRemainingGas() const;
    int getRemainingMinerals() const;
    int getNextGasCost() const;
    int getNextMinCost() const;

    bool checkIfNextInBuild(const UpgradeType upgrade) const; //Convenience overload for common call.
    bool checkIfNextInBuild(const TechType upgrade) const; //Convenience overload for common call.
    bool checkIfNextInBuild(const UnitType ut) const; //Convenience for overload common call.
};


//vector<string> build_order_list = {
//    //"drone drone drone drone drone overlord pool drone creep drone drone", // The blind sunken. For the bots that just won't take no for an answer.
//    "5pool drone drone ling ling ling overlord ling ling ling ling ling ling ling ling", // 5 pool with some commitment.
//    "7pool drone overlord drone ling ling ling ling ling", // 7 pool, omits drone scout, https://liquipedia.net/starcraft/7_Pool_(vs._Terran)#Ling_Production
//    "9pool drone extract overlord drone ling ling ling speed", // 9 pool, omits plans after ling speed, since responses vary: https://liquipedia.net/starcraft/9_Pool_(vs._Zerg)
//    "9pool drone extract overlord drone ling ling ling lair drone overlord drone hydra_den hydra hydra hydra hydra ling ling ling ling lurker_tech", //1 h lurker, tenative.
//    //"drone drone drone drone drone pool drone extract overlord drone ling ling ling hydra_den drone drone drone drone", //zerg_9pool to hydra one base.
//    "12pool drone extract hatch ling ling ling speed", // 12-pool and speed.
//    "12hatch pool drone drone", // 12 hatch-pool
//    //"drone drone drone drone drone overlord drone drone drone hatch pool extract drone drone drone drone ling ling ling overlord lair drone drone drone speed drone drone drone overlord hydra_den drone drone drone drone lurker_tech creep drone creep drone sunken sunken drone drone drone drone drone overlord overlord hydra hydra hydra hydra ling ling lurker lurker lurker lurker ling ling", // 2h lurker
//    "12pool extract drone hatch drone drone ling ling lair drone drone drone speed hydra_den ling ling lurker_tech overlord hydra hydra hydra hydra extract lurker lurker lurker lurker", // 2h lurker https://liquipedia.net/starcraft/2_Hatch_Lurker_(vs._Terran)
//    //"drone drone drone drone drone overlord drone drone drone hatch pool drone drone drone ling ling ling drone creep drone sunken creep drone sunken creep drone sunken creep drone sunken",  // 2 h turtle, tenative. Dies because the first hatch does not have creep by it when it is time to build.
//    //"drone drone drone drone drone overlord drone drone drone hatch pool drone drone drone drone drone drone drone creep drone sunken creep drone sunken creep drone sunken creep drone sunken evo drone creep spore", // Sunken Testing build. Superpassive.
//    //"drone drone drone drone drone overlord drone drone drone pool creep drone sunken creep drone sunken creep drone sunken creep drone sunken evo drone creep spore", // Sunken Testing build. Superpassive.
//    "12hatch pool extract drone drone drone ling drone drone lair overlord drone drone speed drone drone drone drone drone drone drone drone spire drone extract drone creep drone creep drone sunken sunken overlord overlord overlord muta muta muta muta muta muta muta muta muta muta muta muta", // 2h - TwoBaseMuta. Extra overlord is for safety.
//    "12hatch pool drone extract drone drone drone drone drone drone hydra_den drone overlord drone drone drone grooved_spines hydra hydra hydra hydra hydra hydra hydra overlord hydra hydra hydra hydra hydra hatch extract", //zerg_2hatchhydra - range added an overlord.
//    "12hatch pool drone extract drone drone drone drone drone drone hydra_den drone overlord drone drone drone muscular_augments hydra hydra hydra hydra hydra hydra hydra overlord hydra hydra hydra hydra hydra hatch extract", //zerg_2hatchhydra - speed. added an overlord
//    "12hatch drone drone drone hatch drone drone drone hatch overlord pool", //supermacro cheese
//}; //https://liquipedia.net/starcraft/4_Hatch_before_Gas_(vs._Protoss) //https://liquipedia.net/starcraft/3_Hatch_Hydralisk_(vs._Protoss) //https://liquipedia.net/starcraft/3_Hatch_Muta_(vs._Terran)
