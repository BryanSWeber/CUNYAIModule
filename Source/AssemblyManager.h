#pragma once
/*
    Manages the assembly of units, which is mitigated by the initialized build order. 
*/

#include "CUNYAIModule.h"
#include "MapInventory.h"
#include "UnitInventory.h"
#include "ReservationManager.h"
#include "Build.h"

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
    static std::map<UnitType, int> assemblyCycle_; // persistent valuation of buildable combat units. Should build most valuable one every opportunity.
    static std::map<UnitType, int> coreBuildings_; // persistent set of intended buildings.
    inline static std::map<UnitType, int> maxUnits_ = { { UnitTypes::Zerg_Ultralisk, 4 },
                                                        { UnitTypes::Zerg_Mutalisk, 75 },
                                                        { UnitTypes::Zerg_Scourge, 12 },
                                                        { UnitTypes::Zerg_Hydralisk, 36 },
                                                        { UnitTypes::Zerg_Zergling , 36 },
                                                        { UnitTypes::Zerg_Lurker, 8 } ,
                                                        { UnitTypes::Zerg_Guardian, 12 } ,
                                                        { UnitTypes::Zerg_Devourer, 12 },
                                                        { UnitTypes::Zerg_Drone , 65 },
                                                        { UnitTypes::Zerg_Hatchery , 25 },
                                                        { UnitTypes::Zerg_Overlord , 30 },
                                                        { UnitTypes::Zerg_Extractor, 15 },
                                                        { UnitTypes::Zerg_Spawning_Pool, 1 } ,
                                                        { UnitTypes::Zerg_Evolution_Chamber, 2 },
                                                        { UnitTypes::Zerg_Hydralisk_Den, 1 },
                                                        { UnitTypes::Zerg_Spire, 2 },
                                                        { UnitTypes::Zerg_Queens_Nest , 1 },
                                                        { UnitTypes::Zerg_Ultralisk_Cavern, 1 },
                                                        { UnitTypes::Zerg_Greater_Spire, 1 },
                                                        { UnitTypes::Zerg_Lair, 2 },
                                                        { UnitTypes::Zerg_Hive, 2 },
                                                        { UnitTypes::Zerg_Creep_Colony, 30 },
                                                        { UnitTypes::Zerg_Sunken_Colony, 30 },
                                                        { UnitTypes::Zerg_Spore_Colony, 30 } }; // persistent hard maximums for all combat units.

    static UnitInventory larvaBank_; // collection of larva interested in morphing units.
    static UnitInventory hydraBank_; // colleciton of hydras that may morph into lurkers.
    static UnitInventory mutaBank_; // collection of mutas that may morph into mutas.
    static UnitInventory builderBank_; // collection of drones that could build.
    static UnitInventory creepColonyBank_; // collection of creep colonies that could morph into sunkens/spores.
    static UnitInventory productionFacilityBank_; // Set of hatchery decendants that could be used to create units.
    TilePosition expo_spot_ = TilePositions::Origin; // TilePosition that we anticipate should be the next expo.

    static bool subgoalEcon_; // If econ is preferred to army. Useful for slackness conditions.
    static bool subgoalArmy_; // If army is preferred to econ. Useful for slackness conditions.

    int last_frame_of_larva_morph_command = 0;
    int last_frame_of_hydra_morph_command = 0;
    int last_frame_of_muta_morph_command = 0;
    int last_frame_of_creep_command = 0;

    RemainderTracker remainder_;

public:

    bool Check_N_Build(const UnitType & building, const Unit & unit, const bool & extra_critera, const TilePosition &tp = TilePositions::Origin);  // Checks if a building can be built, and passes additional boolean criteria.  If all critera are passed, then it puts the worker into the pre-build phase with intent to build the building. Trys to build near tileposition TP or the unit if blank.
    static bool Check_N_Grow(const UnitType & unittype, const Unit & larva, const bool & extra_critera);  // Check and grow a unit using larva.
    static bool checkNewUnitWithinMaximum(const UnitType &unit); // Returns TRUE if a new copy of this unit would not exeed our predetermined maximum. Returns FALSE if a new unit would exeed our maximums.

    //Unit assembly functions.
    bool assignAssemblyRole(); // Assigns units to appropriate bank and builds them when needed.
    void morphReservedUnits(); // Build units that have been reserved.
    static bool buildCombatUnit(const Unit & morph_canidate);  // Builds a Combat Unit. Which one? Assigned by morphOptimalCombatUnit.
    static bool reserveOptimalCombatUnit(const Unit & morph_canidate, map<UnitType, int> combat_types); // Take this morph canidate and morph into the best combat unit you can find in the combat types map. We will restrict this map based on a collection of heuristics.
    static UnitType refineOptimalUnit(const map<UnitType, int> combat_types, const ResearchInventory & ri); // returns an optimal unit type from a comparison set.
    void updateOptimalCombatUnit(); // evaluates the optimal unit types from assembly_cycle_. Should be a LARGE comparison set, run this regularly but no more than once a frame to use moving averages instead of calculating each time a unit is made (high variance).
    static int returnUnitRank(const UnitType &ut);  //Simply returns the rank of a unit type in the buildfap sim. Higher rank = better!
    static bool checkBestUnit(const UnitType & ut); // returns true if preferred unit.
    static void weightUnitSim(const bool & condition, const UnitType &unit, const int &weight); //Increases the weight of the unit in the sim by +weight (w can be negative to penalize), when conditions are met.
    static void applyWeightsFor(const UnitType &unit); //Checks all weightUnitSims relevant for unit.
    static void clearSimulationHistory(); // This should be ran when a unit is made/discovered so comparisons are fair!

    map<int, TilePosition> addClosestWall(const UnitType &building, const TilePosition &tp); // Return a map containing viable tile positions and their distance to tp.
    //static map<int, TilePosition> addClosestBlockWithSizeOrLarger(const UnitType &building, const TilePosition &tp); // Return a map containing viable tile positions and their distance to tp.  Will add LARGE tiles as a backup because we have so many under current BWEB and sometimes the medium/small blocks do not appear properly.
    map<int, TilePosition> addClosestBlockWithSizeOrLargerWithinWall(const UnitType & building, const TilePosition & tp); 
    map<int, TilePosition> addClosestStation(const UnitType &building, const TilePosition &tp);  // Return a map containing viable tile positions and their distance to tp.
    static void planDefensiveWalls(); // Creates a Z-sim city at the natural.

    //Building assembly functions
    TilePosition updateExpoPosition(); // Returns the TilePosition of the next Expo we want to make. Is determined without considering cost first, which should evade issues of sending drones on long trips so we can "afford" the expo upon arrival.
    bool buildBuilding(const Unit & drone);     // Builds the next building you can afford. Area of constant improvement. 
    bool buildAtNearestPlacement(const UnitType &building, map<int, TilePosition> placements, const Unit u, const bool extra_critera, const int max_travel_distance_contributing = 500); // build at the nearest position in that map<int, TilePosition> using unit u. Confirms extra criteria is met. Will not consider more than (max_travel_distance_contributing) pixels of travel time into cost. Now checks every worker to see if they are at the closest spot, now O(n^2) speed ... but hopefully better!
    bool Expo(const Unit &unit, const bool &extra_critera, MapInventory &inv); // Build an expo at a internally chosen position, granting extra critera is true. Asks to be fed a map inventory so it knows what positions are safe.
    static void clearBuildingObstuctions(const UnitType & ut, const TilePosition & tile, const Unit & exception_unit); // Moves all units except for the Stored exeption_unit elsewhere.
    static bool isPlaceableCUNY(const UnitType &building, const TilePosition &tile);    // Checks if a tile position is buildable for a unit of type building and clear of immobile obstructions. Note this will NOT check visiblity. Will return false if a building is at the tile.
    static bool isOccupiedBuildLocation(const UnitType & type, const TilePosition & location, bool checkEnemy);     // Checks if a build position is occupied by an immobile object or, optionally, an enemy unit of any sort.
    static bool isFullyVisibleBuildLocation(const UnitType & type, const TilePosition & location);     // Checks if I can see every tile in a build location. 
    static void updatePotentialBuilders(); // Updates all units that might build something at this time.
    static bool creepColonyInArea(const Position & pos); // Assigns prestored units to the assembly task. Also builds emergency creep colonies.
    static bool buildStaticDefence(const Unit & morph_canidate, const bool & force_spore, const bool & force_sunken); // take a creep colony and morph it into another type of static defence. 

    //Below are checks to see if an item can be built, if there is sufficent excess resource of a particular time around, and related checks.
    static bool canMakeCUNY(const UnitType &ut, const bool can_afford = false, const Unit &builder = nullptr); // a modification of the BWAPI canMake. Has an option to -exclude- cost, allowing for preperatory movement and positioning of builders. Affordability is min, gas, and supply.
    static bool checkSlackLarvae(); // Checks if there is slack larva (eg 2 floating). To determine if I am alarmingly floating minerals
    static bool checkSlackMinerals(); // Checks if there is slack minerals (50 unassigned). To determine if I am alarmingly floating minerals
    static bool checkSlackGas(); // Checks if there is slack gas (50 unassigned).To determine if I am alarmingly floating gas
    static bool checkSlackSupply(); // Checks if there is slack supply. Follows custom heuristic, might not be triggered since GA overrides this to some extent. To determine if I am alarmingly floating extra supply.

    static int getMaxGas(); // Returns the maximum gas cost of all currently builable units.
    static int getMaxSupply(); // Returns the maximum supply of all supply costs that I can make.
    int getMaxTravelDistance(); //Returns the most pixels a drone will travel to build an expo.
    int getBuildingRelatedGroundDistance(const Position & A, const Position & B); // Returns ground distance, tries JPS first then falls back to CPP.
    int getUnbuiltSpaceGroundDistance(const Position & A, const Position & B);
    TilePosition getExpoPosition(); // retreive stored expo position, if it's set to origin, then recalculate it (1x).

    void setMaxUnit(const UnitType &ut, const int max); //Sets the maximum number of units to MAX.

    bool testActiveAirDefenseBest(const bool testSelfForWeakness = true) const; //Returns true if my units would do more damage if they only shot up. Plugging in false will consider if the enemy is better off with anti-air defenses.
    bool testAirAttackBest(const bool testSelfForWeakness = true) const; //Returns true if my units would do more damage if they flew. Plugging in false will consider if the enemy is better off with air units.

    static void Print_Assembly_FAP_Cycle(const int & screen_x, const int & screen_y);     // print the assembly cycle we're thinking about.

};


