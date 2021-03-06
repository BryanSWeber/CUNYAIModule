#pragma once

//Controls most ML processes, including Genetic algorithms, random forest.  Only point of interaction with python. Used for learning between games. Does not control any learning or adaptation within games.

#include <BWAPI.h>
#include "CUNYAIModule.h"
#include <string>

using namespace std;
using namespace BWAPI;

struct LearningManager {

    //string readDirectory = "..//..//read//"; // If you are launched by the shell exe and are inside CUNYAIBot folder, you start from your particular position and search down.
    //string writeDirectory = "..//..//write//";
    string readDirectory = "bwapi-data//read//"; // If you are launched by BWAPI you start from starcraft.exe
    string writeDirectory = "bwapi-data//write//";

    bool confirmLearningFilesPresent(); //Copy all important files to the write folder so they are present for access and modification during the game. 
    void initializeGeneticLearning(); //Use a handcrafted GA in order to determine the best opening. 
    //void initializeRFLearning(); // Use random forest (and python) to filter out unwanted opening parameters.
    void initializeTestStart(); // Use a specific opening (hardcoded) so there is no variation in bot behavior.
    void initializeRandomStart(); // Use a random opening to get a broad idea of the action space of the bot over several hundred games.
    //void initializeCMAESUnitWeighting(); // In development. Use python's CMAES algorithim to choose unit weights.
    void initializeGAUnitWeighting(); // In development. Use a simple c++ genetic algorithm to choose the unit weights.

    vector<string> build_order_list = {
    //"drone drone drone drone drone overlord pool drone creep drone drone", // The blind sunken. For the bots that just won't take no for an answer.
    "5pool drone drone ling ling ling overlord ling ling ling ling ling ling ling ling", // 5 pool with some commitment.
    "7pool drone overlord drone ling ling ling ling ling", // 7 pool, omits drone scout, https://liquipedia.net/starcraft/7_Pool_(vs._Terran)#Ling_Production
    "9pool drone extract overlord drone ling ling ling speed", // 9 pool, omits plans after ling speed, since responses vary: https://liquipedia.net/starcraft/9_Pool_(vs._Zerg)
    "9pool drone extract overlord drone ling ling ling lair drone overlord drone hydra_den hydra hydra hydra hydra ling ling ling ling lurker_tech", //1 h lurker, tenative.
    //"drone drone drone drone drone pool drone extract overlord drone ling ling ling hydra_den drone drone drone drone", //zerg_9pool to hydra one base.
    "12pool drone extract hatch ling ling ling speed", // 12-pool and speed.
    "12hatch pool drone drone", // 12 hatch-pool
    //"drone drone drone drone drone overlord drone drone drone hatch pool extract drone drone drone drone ling ling ling overlord lair drone drone drone speed drone drone drone overlord hydra_den drone drone drone drone lurker_tech creep drone creep drone sunken sunken drone drone drone drone drone overlord overlord hydra hydra hydra hydra ling ling lurker lurker lurker lurker ling ling", // 2h lurker
    "12pool extract drone hatch drone drone ling ling lair drone drone drone speed hydra_den ling ling lurker_tech overlord hydra hydra hydra hydra extract lurker lurker lurker lurker", // 2h lurker https://liquipedia.net/starcraft/2_Hatch_Lurker_(vs._Terran)
    //"drone drone drone drone drone overlord drone drone drone hatch pool drone drone drone ling ling ling drone creep drone sunken creep drone sunken creep drone sunken creep drone sunken",  // 2 h turtle, tenative. Dies because the first hatch does not have creep by it when it is time to build.
    //"drone drone drone drone drone overlord drone drone drone hatch pool drone drone drone drone drone drone drone creep drone sunken creep drone sunken creep drone sunken creep drone sunken evo drone creep spore", // Sunken Testing build. Superpassive.
    //"drone drone drone drone drone overlord drone drone drone pool creep drone sunken creep drone sunken creep drone sunken creep drone sunken evo drone creep spore", // Sunken Testing build. Superpassive.
    "12hatch pool extract drone drone drone ling drone drone lair overlord drone drone speed drone drone drone drone drone drone drone drone spire drone extract drone creep drone creep drone sunken sunken overlord overlord overlord muta muta muta muta muta muta muta muta muta muta muta muta", // 2h - Muta. Extra overlord is for safety.
    "12hatch pool drone extract drone drone drone drone drone drone hydra_den drone overlord drone drone drone grooved_spines hydra hydra hydra hydra hydra hydra hydra overlord hydra hydra hydra hydra hydra hatch extract", //zerg_2hatchhydra - range added an overlord.
    "12hatch pool drone extract drone drone drone drone drone drone hydra_den drone overlord drone drone drone muscular_augments hydra hydra hydra hydra hydra hydra hydra overlord hydra hydra hydra hydra hydra hatch extract", //zerg_2hatchhydra - speed. added an overlord  
    "12hatch drone drone drone hatch drone drone drone hatch overlord pool", //supermacro cheese
    };

    double gas_proportion_t0; // Gas ratio at time 0.
    double supply_ratio_t0; // Supply ratio at time 0.
    double a_army_t0; // Army alpha value at time 0.
    double a_vis_t0; // Now defunct. Value of visibility at time 0.
    double a_econ_t0; // Econ alpha value at time 0.
    double a_tech_t0; // Tech alpha value at time 0.
    double r_out_t0; // Parameter controlling how rapidly bot adapts to mimic the opponent.
    string build_order_t0; // Starting build order.
    int fliers_t0; // Flier count at time zero, it's a placeholder for the count of fliers.
    int detectors_t0; // Detector count at time 0, it's a placeholder for the count of detectors.

    map<UnitType,double> unit_weights; //Weights designed to approximate the value of each unit in FAP sims and production estimations. Will affect weight of macro parmeters indirectly.
    int max_value_; // The value of the most expensive unit in the model. Needed for normalizing and denormalizing.
    int resetScale(const UnitType ut); // takes the unit weights (which will be passed from) the UnitWeights file (bound between -1 and 1) and converts them back into the orginal scale (0 and the cost of the most expensive units).

    double getOutcomeScore(const bool isWinner, const int buildScore, const int killScore, const int razeScore, const int unitScore);

};

struct HistoryEntry {
    HistoryEntry();
    HistoryEntry(
        double gas_proportion_total,
        double supply_ratio_total,
        double a_army_total,
        double a_econ_total,
        double a_tech_total,
        double r_total,
        string race_total,
        bool win_total,
        int sdelay_total,
        int mdelay_total,
        int ldelay_total,
        string name_total,
        string map_name_total,
        double enemy_average_army,
        double enemy_average_econ,
        double enemy_average_tech,
        string opening,
        int score_building,
        int score_kills,
        int score_raze,
        int score_units,
        int detector_count,
        int flyers,
        int duration
    );

    double gas_proportion_total_;
    double supply_ratio_total_;
    double a_army_total_;
    double a_econ_total_;
    double a_tech_total_;
    double r_total_;
    string race_total_;
    bool win_total_;
    int sdelay_total_;
    int mdelay_total_;
    int ldelay_total_;
    string name_total_;
    string map_name_total_;
    double enemy_average_army_;
    double enemy_average_econ_;
    double enemy_average_tech_;
    string opening_;
    int score_building_;
    int score_kills_;
    int score_raze_;
    int score_units_;
    int detector_count_;
    int flyers_;
    int duration_;
};