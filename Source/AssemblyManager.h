#pragma once
/*
    Manages both the assembly of units and (below) the fixed/immutable build order at the start of the game. 
*/

#include "CUNYAIModule.h"
#include "MapInventory.h"
#include "UnitInventory.h"
#include "ReservationManager.h"

#include "FAP\FAP\include\FAP.hpp" // could add to include path but this is more explicit.
#include "BWEB\BWEB.h"
#include <bwem.h>
#include "PlayerModelManager.h"

using namespace BWAPI;
using namespace Filter;
using namespace std;

//There should only be one assembly manager, so it can be declared static fairly safely.
class AssemblyManager {
private:
    static std::map<UnitType, int> assembly_cycle_; // persistent valuation of buildable combat units. Should build most valuable one every opportunity.
    static std::map<UnitType, int> core_buildings_; // persistent set of intended buildings.

    static UnitInventory larva_bank_; // collection of larva interested in morphing units.
    static UnitInventory hydra_bank_; // colleciton of hydras that may morph into lurkers.
    static UnitInventory muta_bank_; // collection of mutas that may morph into mutas.
    static UnitInventory builder_bank_; // collection of drones that could build.
    static UnitInventory creep_colony_bank_; // collection of creep colonies that could morph into sunkens/spores.
    static UnitInventory production_facility_bank_; // Set of hatchery decendants that could be used to create units.

    static bool subgoal_econ_; // If econ is preferred to army. Useful for slackness conditions.
    static bool subgoal_army_; // If army is preferred to econ. Useful for slackness conditions.

    int last_frame_of_larva_morph_command = 0;
    int last_frame_of_hydra_morph_command = 0;
    int last_frame_of_muta_morph_command = 0;
    int last_frame_of_creep_command = 0;

    RemainderTracker remainder_;

public:
    static bool Check_N_Build(const UnitType & building, const Unit & unit, const bool & extra_critera, const TilePosition &tp = TilePositions::Origin);  // Checks if a building can be built, and passes additional boolean criteria.  If all critera are passed, then it puts the worker into the pre-build phase with intent to build the building. Trys to build near tileposition TP or the unit if blank.
    static bool Check_N_Grow(const UnitType & unittype, const Unit & larva, const bool & extra_critera);  // Check and grow a unit using larva.

    //Unit assembly functions.
    bool assignAssemblyRole(); // Assigns units to appropriate bank and builds them when needed.
    static bool buildCombatUnit(const Unit & morph_canidate);  // Builds a Combat Unit. Which one? Assigned by morphOptimalCombatUnit.
    static bool morphOptimalCombatUnit(const Unit & morph_canidate, map<UnitType, int> combat_types); // Take this morph canidate and morph into the best combat unit you can find in the combat types map. We will restrict this map based on a collection of heuristics.
    static UnitType refineOptimalUnit(const map<UnitType, int> combat_types, const ResearchInventory & ri); // returns an optimal unit type from a comparison set.
    void updateOptimalCombatUnit(); // evaluates the optimal unit types from assembly_cycle_. Should be a LARGE comparison set, run this regularly but no more than once a frame to use moving averages instead of calculating each time a unit is made (high variance).
    static int returnUnitRank(const UnitType &ut);  //Simply returns the rank of a unit type in the buildfap sim. Higher rank = better!
    static bool checkBestUnit(const UnitType & ut); // returns true if preferred unit.
    static void weightUnitSim(const bool & condition, const UnitType &unit, const double &weight); //Increases the weight of the unit by weight% (w can be negative to penalize), when conditions are met.
    static void evaluateWeightsFor(const UnitType &unit); //Checks all weightUnitSims relevant for unit.

    static map<int, TilePosition> addClosestWall(const UnitType &building, const TilePosition &tp); // Return a map containing viable tile positions and their distance to tp.
    static map<int, TilePosition> addClosestBlockWithSizeOrLarger(const UnitType &building, const TilePosition &tp); // Return a map containing viable tile positions and their distance to tp.  Will add LARGE tiles as a backup because we have so many under current BWEB and sometimes the medium/small blocks do not appear properly.
    static map<int, TilePosition> addClosestStation(const UnitType &building, const TilePosition &tp);  // Return a map containing viable tile positions and their distance to tp.


    //Building assembly functions
    static bool buildBuilding(const Unit & drone);     // Builds the next building you can afford. Area of constant improvement.
    static bool buildAtNearestPlacement(const UnitType &building, map<int, TilePosition> &placements, const Unit u, const bool extra_critera, const int max_travel_distance_contributing = 500); // build at the nearest position in that map<int, TilePosition> using unit u. Confirms extra criteria is met. Will not consider more than (max_travel_distance_contributing) pixels of travel time into cost.
    static bool Expo(const Unit &unit, const bool &extra_critera, MapInventory &inv); // Build an expo at a internally chosen position, granting extra critera is true. Asks to be fed a map inventory so it knows what positions are safe.
    static void clearBuildingObstuctions(const UnitType & ut, const TilePosition & tile, const Unit & exception_unit); // Moves all units except for the Stored exeption_unit elsewhere.
    static bool isPlaceableCUNY(const UnitType &building, const TilePosition &tile);    // Checks if a tile position is buildable for a unit of type building and clear of immobile obstructions. Note this will NOT check visiblity.
    static bool isOccupiedBuildLocation(const UnitType & type, const TilePosition & location);     // Checks if a build position is occupied.
    static bool isFullyVisibleBuildLocation(const UnitType & type, const TilePosition & location);     // Checks if I can see every tile in a build location. 
    static void updatePotentialBuilders(); // Updates all units that might build something at this time.
    static bool creepColonyInArea(const Position & pos); // Assigns prestored units to the assembly task. Also builds emergency creep colonies.
    static void clearSimulationHistory(); // This should be ran when a unit is made/discovered so comparisons are fair!
    static void planDefensiveWalls(); // Creates a Z-sim city at the natural.
    static bool buildStaticDefence(const Unit & morph_canidate, const bool & force_spore, const bool & force_sunken); // take a creep colony and morph it into another type of static defence. 

    //Below are checks to see if an item can be built, if there is sufficent excess resource of a particular time around, and related checks.
    static bool canMakeCUNY(const UnitType &ut, const bool can_afford = false, const Unit &builder = nullptr); // a modification of the BWAPI canMake. Has an option to -exclude- cost, allowing for preperatory movement and positioning of builders. Affordability is min, gas, and supply.
    static bool checkSlackLarvae(); // Checks if there is slack larva (eg 2 floating).
    static bool checkSlackMinerals(); // Checks if there is slack minerals.
    static bool checkSlackGas(); // Checks if there is slack gas.
    static bool checkExcessSupply(); // Checks if there is slack supply. Follows custom heuristic, might not be triggered since GA overrides this to some extent.
    static bool checkSufficientSlack(const UnitType &ut = UnitTypes::Zerg_Zergling); // Checks if there is sufficient slack to build an XYZ. Defaults to zergling, the minimum unit. 

    static int getMaxGas(); // Returns the maximum gas cost of all currently builable units.
    static int getMaxSupply(); // Returns the maximum supply of all supply costs that I can make.

    static bool testActiveAirProblem(const ResearchInventory & ri, const bool & test_for_self_weakness);  // returns true if weak against air. Tests explosive damage.
    static bool testPotentialAirVunerability(const ResearchInventory & ri, const bool & test_for_self_weakness); //Returns true if (players) units would do more damage if they flew. Player is self (if true) or to the enemy (if false). 

    static void Print_Assembly_FAP_Cycle(const int & screen_x, const int & screen_y);     // print the assembly cycle we're thinking about.

};


class BuildOrderObject {
private:

    UnitType unit_in_queue_;
    UpgradeType upgrade_in_queue_;
    TechType research_in_queue_;

public:
    //bool operator==( const BuildOrderObject &rhs );
    //bool operator!=( const BuildOrderObject &rhs );

    BuildOrderObject(UnitType unit) {
        unit_in_queue_ = unit;
        upgrade_in_queue_ = UpgradeTypes::None;
        research_in_queue_ = TechTypes::None;
    };

    BuildOrderObject(UpgradeType up) {
        unit_in_queue_ = UnitTypes::None;
        upgrade_in_queue_ = up;
        research_in_queue_ = TechTypes::None;
    };

    BuildOrderObject(TechType tech) {
        unit_in_queue_ = UnitTypes::None;
        upgrade_in_queue_ = UpgradeTypes::None;
        research_in_queue_ = tech;
    };

    UnitType BuildOrderObject::getUnit() {
        return unit_in_queue_;
    };

    UpgradeType BuildOrderObject::getUpgrade() {
        return upgrade_in_queue_;
    };

    TechType BuildOrderObject::getResearch() {
        return research_in_queue_;
    };
};

struct BuildingGene {
    BuildingGene();
    BuildingGene(string s);

    vector<BuildOrderObject> building_gene_;  // how many of each of these do we want?
    string initial_building_gene_;

    map<UnitType, int> goal_units;

    bool ever_clear_ = false;
    UnitType last_build_order;

    int cumulative_gas_;
    int cumulative_minerals_;

    void getInitialBuildOrder(string s);
    void clearRemainingBuildOrder(const bool diagnostic); // empties the build order.
    void updateRemainingBuildOrder(const UpgradeType &ups); // drops item from list as complete.
    void updateRemainingBuildOrder(const TechType & research);// drops item from list as complete.
    void updateRemainingBuildOrder(const UnitType &ut); // drops item from list as complete.
    void announceBuildingAttempt(UnitType ut);  // do we have a guy going to build it?
    bool checkBuildingNextInBO(UnitType ut);
    int  countTimesInBO(UnitType ut);
    bool checkUpgradeNextInBO(UpgradeType upgrade);
    bool checkResearch_Desired(TechType upgrade);
    bool isEmptyBuildOrder();

    void addBuildOrderElement(const UpgradeType &ups); // adds an element to the list.
    void addBuildOrderElement(const TechType & research);// adds an element to the list.
    void addBuildOrderElement(const UnitType &ut); // adds an element to the list.

    void retryBuildOrderElement(const UnitType & ut); // Adds the element to the front of the list again.

    void retryBuildOrderElement(const UpgradeType & up);

    void getCumulativeResources();
    //bool checkExistsInBuild( UnitType unit );
};

