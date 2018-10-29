#pragma once
#include <BWAPI.h>
#include "CUNYAIModule.h"
#include "Research_Inventory.h"
#include "Unit_Inventory.h"
#include "CobbDouglas.h"

using namespace std;
using namespace BWAPI;

struct Player_Model {
    Player_Model() {}; // need a constructor method.

    Player bwapi_player_; // this is a pointer, explicitly.
    double estimated_workers_ = 0;
    double estimated_cumulative_worth_ = 0;
    double estimated_net_worth_ = 0;

    Unit_Inventory units_;
    Unit_Inventory casualties_;
    Research_Inventory researches_;
    CobbDouglas spending_model_;
    //Other player-based factoids that may be useful should eventually go here- fastest time to air, popular build items, etc.

    bool u_relatively_weak_against_air_; 
    bool e_relatively_weak_against_air_;

    void updateOtherOnFrame(const Player &other_player);
    void updateSelfOnFrame(const Player_Model &target_player);
    void evaluateWorkerCount();
    void evaluateCurrentWorth();
    // under development. Currently bugged but of interest.

    //stored to avoid extensive counting.  
    void updateUnit_Counts();

    vector< UnitType > unit_type_;
    vector< int > unit_count_;
    vector< int > unit_incomplete_;
    vector< int > radial_distances_from_enemy_ground_ = { 0 };
    int closest_radial_distance_enemy_ground_ = INT_MAX;

    //unit cartridges, Combat unit cartridge is all mobile noneconomic units we may consider building (excludes static defense).
    map<UnitType, int> combat_unit_cartridge_;
    map<UnitType, int> building_cartridge_;
    map<UpgradeType, int> upgrade_cartridge_;
    map<TechType, int> tech_cartridge_;

};

