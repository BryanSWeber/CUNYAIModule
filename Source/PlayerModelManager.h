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
    //(while these only are relevant for CUNYBot, they are still passed to all players anyway by default on construction), Combat unit cartridge is all mobile noneconomic units we may consider building(excludes static defense).
    inline static std::map<UnitType, int> combat_unit_cartridge_ = { { UnitTypes::Zerg_Ultralisk, 0 } ,{ UnitTypes::Zerg_Mutalisk, 0 },{ UnitTypes::Zerg_Scourge, 0 },{ UnitTypes::Zerg_Hydralisk, 0 },{ UnitTypes::Zerg_Zergling , 0 },{ UnitTypes::Zerg_Lurker, 0 } ,{ UnitTypes::Zerg_Guardian, 0 } ,{ UnitTypes::Zerg_Devourer, 0 } };
    inline static std::map<UnitType, int> eco_unit_cartridge_ = { { UnitTypes::Zerg_Drone , 0 },{ UnitTypes::Zerg_Hatchery , 0 },{ UnitTypes::Zerg_Overlord , 0 },{ UnitTypes::Zerg_Extractor, 0 } };
    inline static std::map<UnitType, int> building_cartridge_ = { { UnitTypes::Zerg_Spawning_Pool, 0 } ,{ UnitTypes::Zerg_Evolution_Chamber, 0 },{ UnitTypes::Zerg_Hydralisk_Den, 0 },{ UnitTypes::Zerg_Spire, 0 },{ UnitTypes::Zerg_Queens_Nest , 0 },{ UnitTypes::Zerg_Ultralisk_Cavern, 0 } ,{ UnitTypes::Zerg_Greater_Spire, 0 },{ UnitTypes::Zerg_Hatchery, 0 } ,{ UnitTypes::Zerg_Lair, 0 },{ UnitTypes::Zerg_Hive, 0 },{ UnitTypes::Zerg_Creep_Colony, 0 },{ UnitTypes::Zerg_Sunken_Colony, 0 },{ UnitTypes::Zerg_Spore_Colony, 0 } };
    inline static std::map<UpgradeType, int> upgrade_cartridge_ = { { UpgradeTypes::Zerg_Carapace, 0 } ,{ UpgradeTypes::Zerg_Flyer_Carapace, 0 },{ UpgradeTypes::Zerg_Melee_Attacks, 0 },{ UpgradeTypes::Zerg_Missile_Attacks, 0 },{ UpgradeTypes::Zerg_Flyer_Attacks, 0 }/*,{ UpgradeTypes::Antennae, 0 }*/,{ UpgradeTypes::Pneumatized_Carapace, 0 },{ UpgradeTypes::Metabolic_Boost, 0 },{ UpgradeTypes::Adrenal_Glands, 0 },{ UpgradeTypes::Muscular_Augments, 0 },{ UpgradeTypes::Grooved_Spines, 0 },{ UpgradeTypes::Chitinous_Plating, 0 },{ UpgradeTypes::Anabolic_Synthesis, 0 } }; // removed antennae. It's simply a typically bad upgrade.
    inline static std::map<TechType, int> tech_cartridge_ = { { TechTypes::Lurker_Aspect, 0 } };

    // Averages for Opponent Modeling, outputting info at game end in history file.
    void updatePlayerAverageCD();
    double average_army_; // Cumulative.
    double average_econ_; // Cumulative.
    double average_tech_; // Cumulative.

    void Print_Average_CD(const int &screen_x, const int &screen_y); // Onscreen Diagnostic.

public:

    // Important variables.
    Player bwapi_player_;
    double estimated_workers_ = 0; // an active count of workers, both seen and unseen.
    double estimated_bases_ = 0;
    double estimated_cumulative_worth_ = 0; // prototyping.
    double estimated_net_worth_ = 0; // prototyping.
    double estimated_resources_per_frame_ = 0; // prot0typing.
    double estimated_unseen_tech_per_frame_ = 0; // prot0typing.
    double estimated_unseen_army_per_frame_ = 0; // prot0typing.
    double estimated_unseen_army_ = 0;
    double estimated_unseen_flyers_ = 0; // a subset of estimated army.
    double estimated_unseen_ground_ = 0; // a subset of estimated army.
    double estimated_unseen_tech_ = 0;
    double estimated_unseen_workers_ = 0; // an active count of unseen workers, since they are important among unseen units.

    //Classes borrowed from other sources
    Unit_Inventory units_;
    Unit_Inventory casualties_;
    map< UnitType, double> unseen_units_ = {}; //These units may never have been built, but they COULD have been built and the opposing player would not know. This is a unique map and might be seperated at a later time.
    Research_Inventory researches_;
    CobbDouglas spending_model_;

    // Inferred player states - eg, what needs to be done?
    bool u_have_active_air_problem_; // You're dying from air units.
    bool e_has_air_vunerability_; // Enemy could potentially be damaged by air units.

    // Counts for tallying what's in this inventory.
    vector< UnitType > unit_type_; // A vector of all the unit types, used in counting units.
    vector< int > unit_count_;
    vector< int > unit_incomplete_;
    vector< int > radial_distances_from_enemy_ground_ = { 0 };
    int closest_ground_combatant_ = INT_MAX;

    // Updating all those classes and values above
    void updateOtherOnFrame(const Player &other_player); //Throw an opponent in here and it will assume he produces from all known units (tech when possible, army when possible, econ always) and update his known units.
    void updateSelfOnFrame(); // Similar to update other, but perfect visiblity aids in most tasks, and inference about actions is unneeded.

    // Updating the map of unseen units. Might make this a seperate class.
    void imputeUnits(const Unit &unit); //Takes a single unit that has just been discovered and checks if it is in the unseen_unit_ map. We then remove it from the unseen unit map. If the unseen_unit_ becomes negative in net worth, we assume they must have had another producer of units hidden from us.
    void incrementUnseenUnits(const UnitType &ut); // adds a unit to the unseen unit collection.
    void decrementUnseenUnits(const UnitType &ut); // removes a unit from the unseen unit collection.
    double countUnseenUnits(const UnitType & ut); // safely returns the count of a unit from the unseen unit collection and 0 if it's not there.

    //void evaluatePotentialWorkerCount(); // Estimates how many workers they have, assuming continuous building with observed platforms.
    void evaluatePotentialUnitExpenditures(); // Estimates the value of troops that could be incoming this frame given their known production capacity. In progress, only a rough estimate.
    void evaluatePotentialTechExpenditures();  // Estimates the value of Tech that could be incoming this frame given their known production capacity. In progress, only a rough estimate.
    void evaluateCurrentWorth(); // under development. 

    void considerWorstUnseenProducts(const Unit_Inventory &ui); //Takes a map of imputed units and guesses the "worst" thing they could have produced. Applies exclusively to unit inventory's unit_map_
    void considerWorstUnseenProducts(const map<UnitType, double>& ui); //Takes a map of imputed units and guesses the "worst" thing they could have produced. Intended to apply to unseen_unit_ maps, but prepared to eventually go onto something else.
    void considerWorkerUnseenProducts(const Unit_Inventory &ui); //Takes a map of imputed units and assumes workers are produced at every facility, the "worst" thing they could have produced. Applies exclusively to unit inventory's unit_map_
    void considerWorkerUnseenProducts(const map<UnitType, double>& ui); //Takes a map of imputed units and assumes workers are produced at every facility, the "worst" thing they could have produced. Intended to apply to unseen_unit_ maps, but prepared to eventually go onto something else.

    UnitType getWorstProduct(const UnitType & ut); // gets the "worst" unit that might be made by a unit.
    vector<UnitType> findAlternativeProducts(const UnitType &ut); //Takes a unittype and returns a vector of the possible types of units that could have been made instead of that unittype.

    bool opponentHasRequirements(const UnitType &ut); // Can they make this unit type? Does not check if it is already done.
    bool opponentHasRequirements(const TechType & tech); // Can they make this tech type? Does not check if it is already done.
    bool opponentHasRequirements(const UpgradeType & up); // Can they make this upgrade? Does not check if it is already done.
    bool opponentCouldBeUpgrading(const UpgradeType & up); // Can they make this upgrade? Considers if the upgrade is finished, in which case they cannot remake it.
    bool opponentCouldBeTeching(const TechType & tech); // Can they make this tech type? Considers if the tech type is finished, in which case they cannot remake it.

    void updateUnit_Counts(); //stores the count of all unit types the player has to avoid extensive recounting.  

    void setLockedOpeningValues(const double alpha_army, const double alpha_econ, const double alpha_tech, const double gas_proportion, const double supply_ratio, const string build_order); //if we want locked values this is how we set them.

    double getCumArmy(); // getters for the private average alpha army stat.
    double getCumEco(); // getters for the private average alpha eco stat.
    double getCumTech(); // getters for the private average alpha tech stat.
};

