#pragma once
#include <BWAPI.h>
#include "CUNYAIModule.h"
#include "Research_Inventory.h"
#include "Unit_Inventory.h"
#include "CobbDouglas.h"

using namespace std;
using namespace BWAPI;

struct Player_Model {
    Player_Model() {
        //(while these only are relevant for CUNYBot, they are still  passed to all players anyway by default on construction), Combat unit cartridge is all mobile noneconomic units we may consider building(excludes static defense).
         combat_unit_cartridge_ = { { UnitTypes::Zerg_Ultralisk, INT_MIN } ,{ UnitTypes::Zerg_Mutalisk, INT_MIN },{ UnitTypes::Zerg_Scourge, INT_MIN },{ UnitTypes::Zerg_Hydralisk, INT_MIN },{ UnitTypes::Zerg_Zergling , INT_MIN },{ UnitTypes::Zerg_Lurker, INT_MIN } ,{ UnitTypes::Zerg_Guardian, INT_MIN } ,{ UnitTypes::Zerg_Devourer, INT_MIN } };
         eco_unit_cartridge_ = { { UnitTypes::Zerg_Drone , INT_MIN }, { UnitTypes::Zerg_Hatchery , INT_MIN },{ UnitTypes::Zerg_Overlord , INT_MIN }, {UnitTypes::Zerg_Extractor, INT_MIN} };
         building_cartridge_ = { { UnitTypes::Zerg_Spawning_Pool, INT_MIN } ,{ UnitTypes::Zerg_Evolution_Chamber, INT_MIN },{ UnitTypes::Zerg_Hydralisk_Den, INT_MIN },{ UnitTypes::Zerg_Spire, INT_MIN },{ UnitTypes::Zerg_Queens_Nest , INT_MIN },{ UnitTypes::Zerg_Ultralisk_Cavern, INT_MIN } ,{ UnitTypes::Zerg_Greater_Spire, INT_MIN },{ UnitTypes::Zerg_Hatchery, INT_MIN } ,{ UnitTypes::Zerg_Lair, INT_MIN },{ UnitTypes::Zerg_Hive, INT_MIN },{ UnitTypes::Zerg_Creep_Colony, INT_MIN },{ UnitTypes::Zerg_Sunken_Colony, INT_MIN }, { UnitTypes::Zerg_Spore_Colony, INT_MIN } };
         upgrade_cartridge_ = { { UpgradeTypes::Zerg_Carapace, INT_MIN } ,{ UpgradeTypes::Zerg_Flyer_Carapace, INT_MIN },{ UpgradeTypes::Zerg_Melee_Attacks, INT_MIN },{ UpgradeTypes::Zerg_Missile_Attacks, INT_MIN },{ UpgradeTypes::Zerg_Flyer_Attacks, INT_MIN },{ UpgradeTypes::Antennae, INT_MIN },{ UpgradeTypes::Pneumatized_Carapace, INT_MIN },{ UpgradeTypes::Metabolic_Boost, INT_MIN },{ UpgradeTypes::Adrenal_Glands, INT_MIN },{ UpgradeTypes::Muscular_Augments, INT_MIN },{ UpgradeTypes::Grooved_Spines, INT_MIN },{ UpgradeTypes::Chitinous_Plating, INT_MIN },{ UpgradeTypes::Anabolic_Synthesis, INT_MIN } };
         tech_cartridge_ = { { TechTypes::Lurker_Aspect, INT_MIN } };
    }; // need a constructor method.

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

    void setLockedOpeningValues();

    vector< UnitType > unit_type_;
    vector< int > unit_count_;
    vector< int > unit_incomplete_;
    vector< int > radial_distances_from_enemy_ground_ = { 0 };
    int closest_radial_distance_enemy_ground_ = INT_MAX;

    //unit cartridges 
    map<UnitType, int> combat_unit_cartridge_;
    map<UnitType, int> eco_unit_cartridge_;
    map<UnitType, int> building_cartridge_;
    map<UpgradeType, int> upgrade_cartridge_;
    map<TechType, int> tech_cartridge_;
};

