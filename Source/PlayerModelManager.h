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
    void evaluateCurrentWorth();// under development. Currently bugged but of interest.
	void detection(Player_Model, bool);
};

