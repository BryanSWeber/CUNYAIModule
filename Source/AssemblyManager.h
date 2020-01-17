#pragma once
#include "CUNYAIModule.h"
#include "Map_Inventory.h"
#include "Unit_Inventory.h"
#include "FAP\FAP\include\FAP.hpp" // could add to include path but this is more explicit.
#include "BWEB\BWEB.h"
#include <bwem.h>
#include "PlayerModelManager.h"

using namespace BWAPI;
using namespace Filter;
using namespace std;

class AssemblyManager {
private:
    static std::map<UnitType, int> assembly_cycle_; // persistent valuation of buildable combat units. Should build most valuable one every opportunity.
    static std::map<UnitType, int> core_buildings_; // persistent set of intended buildings.
    static std::map<UnitType, int> specialization_buildings_; // persistent set of intended buildings.

    static Unit_Inventory larva_bank_;
    static Unit_Inventory hydra_bank_;
    static Unit_Inventory muta_bank_;
    static Unit_Inventory builder_bank_;
    static Unit_Inventory creep_colony_bank_;
    static Unit_Inventory production_facility_bank_;
    static bool have_idle_evos_;
    static bool have_idle_spires_;
    static bool resources_are_slack_; //Do we have floating $$ or larva?
    static bool subgoal_econ_; // How should we prioritize dumping floating resources?
    static bool subgoal_army_; // How should we prioritize dumping floating resources?
    static int last_frame_of_larva_morph_command;
    static int last_frame_of_hydra_morph_command;
    static int last_frame_of_muta_morph_command;
    static int last_frame_of_creep_command;

public:
    static bool testActiveAirProblem(const Research_Inventory & ri, const bool & test_for_self_weakness);  // returns spore colony if weak against air. Tests explosive damage.
    static bool testPotentialAirVunerability(const Research_Inventory & ri, const bool & test_for_self_weakness);
    static UnitType returnOptimalUnit(const map<UnitType, int> combat_types, const Research_Inventory & ri); // returns an optimal unit type from a comparison set.
    static int returnUnitRank(const UnitType &ut);
    static void updateOptimalCombatUnit(); // evaluates the optimal unit types from assembly_cycle_. Should be a LARGE comparison set, run this regularly but no more than once a frame to use moving averages instead of calculating each time a unit is made (high variance).
    static bool buildStaticDefence(const Unit & morph_canidate, const bool & force_spore, const bool & force_sunken);
    static bool buildOptimalCombatUnit(const Unit & morph_canidate, map<UnitType, int> combat_types);
    //Checks if a building can be built, and passes additional boolean criteria.  If all critera are passed, then it puts the worker into the pre-build phase with intent to build the building.
    static bool Check_N_Build(const UnitType & building, const Unit & unit, const bool & extra_critera);
    // Check and grow a unit using larva.
    static bool Check_N_Grow(const UnitType & unittype, const Unit & larva, const bool & extra_critera);
    static bool Expo(const Unit &unit, const bool &extra_critera, Map_Inventory &inv);
    // Builds the next building you can afford. Area of constant improvement.
    static bool buildBuilding(const Unit & drone);
    // Moves all units except for the Stored exeption_unit elsewhere.
    static void clearBuildingObstuctions(const UnitType & ut, const TilePosition & tile, const Unit & exception_unit);
    // Checks if a tile position is buildable for a unit of type building and clear of immobile obstructions. Note this will NOT check visiblity.
    static bool isPlaceableCUNY(const UnitType &building, const TilePosition &tile);
    // Checks if a build position is occupied.
    static bool isOccupiedBuildLocation(const UnitType & type, const TilePosition & location);
    // Checks if I can see every tile in a build location. 
    static bool isFullyVisibleBuildLocation(const UnitType & type, const TilePosition & location);
    // returns a combat unit of usefulness. Determined by a series of FAP simulations stored in assembly_cycle_.
    static bool Reactive_BuildFAP(const Unit & morph_canidate);
    // print the assembly cycle we're thinking about.
    static void Print_Assembly_FAP_Cycle(const int & screen_x, const int & screen_y);
    // Updates all units that might build something at this time.
    static void updatePotentialBuilders();
    static bool creepColonyInArea(const Position & pos);
    // Assigns prestored units to the assembly task. Also builds emergency creep colonies.
    static bool assignUnitAssembly();
    static void clearSimulationHistory(); // This should be ran when a unit is made/discovered so comparisons are fair!
    static void getDefensiveWalls();
    // a modification of the BWAPI canMake. Has an option to -exclude- cost, allowing for preperatory movement and positioning of builders. Affordability is min, gas, and supply.
    static bool canMakeCUNY(const UnitType &ut, const bool can_afford = false, const Unit &builder = nullptr);

    //Returns the maximum gas cost of all currently builable units.
    static int getMaxGas();
};


class Build_Order_Object {
private:

    UnitType _unit_in_queue;
    UpgradeType _upgrade_in_queue;
    TechType _research_in_queue;

public:
    //bool operator==( const Build_Order_Object &rhs );
    //bool operator!=( const Build_Order_Object &rhs );

    Build_Order_Object(UnitType unit) {
        _unit_in_queue = unit;
        _upgrade_in_queue = UpgradeTypes::None;
        _research_in_queue = TechTypes::None;
    };

    Build_Order_Object(UpgradeType up) {
        _unit_in_queue = UnitTypes::None;
        _upgrade_in_queue = up;
        _research_in_queue = TechTypes::None;
    };

    Build_Order_Object(TechType tech) {
        _unit_in_queue = UnitTypes::None;
        _upgrade_in_queue = UpgradeTypes::None;
        _research_in_queue = tech;
    };

    UnitType Build_Order_Object::getUnit() {
        return _unit_in_queue;
    };

    UpgradeType Build_Order_Object::getUpgrade() {
        return _upgrade_in_queue;
    };

    TechType Build_Order_Object::getResearch() {
        return _research_in_queue;
    };
};

struct Building_Gene {
    Building_Gene();
    Building_Gene(string s);

    vector<Build_Order_Object> building_gene_;  // how many of each of these do we want?
    string initial_building_gene_;

    map<UnitType, int> goal_units;

    bool ever_clear_ = false;
    UnitType last_build_order;

    int cumulative_gas_;
    int cumulative_minerals_;

    void getInitialBuildOrder(string s);
    void clearRemainingBuildOrder(const bool diagnostic); // empties the build order.
    void updateRemainingBuildOrder(const Unit &u); // drops item from list as complete.
    void updateRemainingBuildOrder(const UpgradeType &ups); // drops item from list as complete.
    void updateRemainingBuildOrder(const TechType & research);// drops item from list as complete.
    void updateRemainingBuildOrder(const UnitType &ut); // drops item from list as complete.
    void announceBuildingAttempt(UnitType ut);  // do we have a guy going to build it?
    bool checkBuilding_Desired(UnitType ut);
    bool checkUpgrade_Desired(UpgradeType upgrade);
    bool checkResearch_Desired(TechType upgrade);
    bool isEmptyBuildOrder();

    void addBuildOrderElement(const UpgradeType &ups); // adds an element to the list.
    void addBuildOrderElement(const TechType & research);// adds an element to the list.
    void addBuildOrderElement(const UnitType &ut); // adds an element to the list.

    void retryBuildOrderElement(const UnitType & ut); // Adds the element to the front of the list again.

    void getCumulativeResources();
    //bool checkExistsInBuild( UnitType unit );
};

