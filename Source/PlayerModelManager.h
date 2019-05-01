#pragma once
#include <BWAPI.h>
#include "CUNYAIModule.h"
#include "Research_Inventory.h"
#include "Unit_Inventory.h"
#include "CobbDouglas.h"

using namespace std;
using namespace BWAPI;

struct Player_Model {
private:

public:
    Player bwapi_player_; // this is a pointer, explicitly.
    double estimated_workers_ = 0;
    double estimated_bases_ = 0;
    double estimated_cumulative_worth_ = 0;
    double estimated_net_worth_ = 0;

    Unit_Inventory units_;
    Unit_Inventory casualties_;
    Research_Inventory researches_;
    CobbDouglas spending_model_;
    //Other player-based factoids that may be useful should eventually go here- fastest time to air, popular build items, etc.

    bool u_have_active_air_problem_; 
    bool e_has_air_vunerability_;

    void updateOtherOnFrame(const Player &other_player);
    void updateSelfOnFrame(const Player_Model &target_player);
    void evaluateWorkerCount();
    void evaluateCurrentWorth(); // under development. Currently bugged but of interest.

    //stored to avoid extensive counting.  
    void updateUnit_Counts();

    void setLockedOpeningValues();

    vector< UnitType > unit_type_;
    vector< int > unit_count_;
    vector< int > unit_incomplete_;
    vector< int > radial_distances_from_enemy_ground_ = { 0 };
    int closest_ground_combatant_ = INT_MAX;

// Averages for Opponent Modeling
    void updatePlayerAverageCD();
    double average_army_;
    double average_econ_;
    double average_tech_;
    void Print_Average_CD(const int &screen_x, const int &screen_y);

    //(while these only are relevant for CUNYBot, they are still  passed to all players anyway by default on construction), Combat unit cartridge is all mobile noneconomic units we may consider building(excludes static defense).
    inline static std::map<UnitType, int> combat_unit_cartridge_ = { { UnitTypes::Zerg_Ultralisk, 0 } ,{ UnitTypes::Zerg_Mutalisk, 0 },{ UnitTypes::Zerg_Scourge, 0 },{ UnitTypes::Zerg_Hydralisk, 0 },{ UnitTypes::Zerg_Zergling , 0 },{ UnitTypes::Zerg_Lurker, 0 } ,{ UnitTypes::Zerg_Guardian, 0 } ,{ UnitTypes::Zerg_Devourer, 0 } };
    inline static std::map<UnitType, int> eco_unit_cartridge_ = { { UnitTypes::Zerg_Drone , 0 },{ UnitTypes::Zerg_Hatchery , 0 },{ UnitTypes::Zerg_Overlord , 0 },{ UnitTypes::Zerg_Extractor, 0 } };
    inline static std::map<UnitType, int> building_cartridge_ = { { UnitTypes::Zerg_Spawning_Pool, 0 } ,{ UnitTypes::Zerg_Evolution_Chamber, 0 },{ UnitTypes::Zerg_Hydralisk_Den, 0 },{ UnitTypes::Zerg_Spire, 0 },{ UnitTypes::Zerg_Queens_Nest , 0 },{ UnitTypes::Zerg_Ultralisk_Cavern, 0 } ,{ UnitTypes::Zerg_Greater_Spire, 0 },{ UnitTypes::Zerg_Hatchery, 0 } ,{ UnitTypes::Zerg_Lair, 0 },{ UnitTypes::Zerg_Hive, 0 },{ UnitTypes::Zerg_Creep_Colony, 0 },{ UnitTypes::Zerg_Sunken_Colony, 0 },{ UnitTypes::Zerg_Spore_Colony, 0 } };
    inline static std::map<UpgradeType, int> upgrade_cartridge_ = { { UpgradeTypes::Zerg_Carapace, 0 } ,{ UpgradeTypes::Zerg_Flyer_Carapace, 0 },{ UpgradeTypes::Zerg_Melee_Attacks, 0 },{ UpgradeTypes::Zerg_Missile_Attacks, 0 },{ UpgradeTypes::Zerg_Flyer_Attacks, 0 }/*,{ UpgradeTypes::Antennae, 0 }*/,{ UpgradeTypes::Pneumatized_Carapace, 0 },{ UpgradeTypes::Metabolic_Boost, 0 },{ UpgradeTypes::Adrenal_Glands, 0 },{ UpgradeTypes::Muscular_Augments, 0 },{ UpgradeTypes::Grooved_Spines, 0 },{ UpgradeTypes::Chitinous_Plating, 0 },{ UpgradeTypes::Anabolic_Synthesis, 0 } }; // removed antennae. It's simply a typically bad upgrade.
    inline static std::map<TechType, int> tech_cartridge_ = { { TechTypes::Lurker_Aspect, 0 } };

};

