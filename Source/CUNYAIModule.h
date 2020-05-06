#pragma once

#include <BWAPI.h> //4.2.0 BWAPI
#include "MapInventory.h"
#include "UnitInventory.h"
#include "Resource_Inventory.h"
#include "Research_Inventory.h"
#include "AssemblyManager.h"
#include "Reservation_Manager.h"
#include "PlayerModelManager.h"
#include "FAP\FAP\include\FAP.hpp"
#include "LearningManager.h"
#include "TechManager.h"
#include "WorkerManager.h"
#include "CombatManager.h"
#include "BaseManager.h"
#include <bwem.h>
#include "BWEB\BWEB.h"
//#include "BrawlSim\BrawlSimLib\include\BrawlSim.hpp"
#include <chrono> // for in-game frame clock.

#define LARVA_BUILD_TIME 342

constexpr bool RESIGN_MODE = false; // must be off for proper game close in SC-docker
constexpr bool ANALYSIS_MODE = false; // Printing game logs, game status every few frames, etc.
constexpr bool DIAGNOSTIC_MODE = false; //Visualizations, printing records, etc. Should seperate these.
constexpr bool MOVE_OUTPUT_BACK_TO_READ = true; // should be FALSE for sc-docker, TRUE for chaoslauncher at home & Training against base ai.
constexpr bool TIT_FOR_TAT_ENGAGED = true; // permits in game-tit-for-tat responses.  Consider disabling this for TEST_MODE.
constexpr int FAP_SIM_DURATION = 24 * 5; // set FAP sim durations.
constexpr bool RIP_REPLAY = false; // Copy replay information.
constexpr bool PRINT_WD = false; // print a file to the current working directory.
constexpr bool DISABLE_ATTACKING = false; // never attack - for exploring movement and reatreating.

constexpr bool GENETIC_HISTORY = true; // use hand-crafted genetic history.
constexpr bool PY_RF_LEARNING = false; // use the random forest to filter unwanted parameters.
constexpr bool RANDOM_PLAN = false; // Turn off learning and always use a random set of starting conditions.  
constexpr bool TEST_MODE = false; // Locks in a build order and defined paramaters. Consider disabling TIT_FOR_TAT otherwise you will adapt towards your opponent and not get exactly the desired utility function.
constexpr bool PY_UNIT_WEIGHTING = false; // under development.
constexpr bool UNIT_WEIGHTING = true; // under development.

//Cheats:  Like, literal single player cheats.
constexpr bool MAP_REVEAL = false; // Just types in black sheep wall for local testing.
constexpr bool NEVER_DIE = false; // just types in power overwhelming for local testing.
constexpr bool INF_MONEY = false; // just types in show me the money for local testing.
constexpr bool INSTANT_WIN = false; // just types in there is no cow level for local testing.


// Remember not to use "Broodwar" in any global class constructor!

class CUNYAIModule : public BWAPI::AIModule
{
public:
    // Virtual functions for callbacks, leave these as they are.
    virtual void onStart();
    virtual void onEnd(bool isWinner);
    virtual void onFrame();
    virtual void onSendText(std::string text);
    virtual void onReceiveText(BWAPI::Player player, std::string text);
    virtual void onPlayerLeft(BWAPI::Player player);
    virtual void onNukeDetect(BWAPI::Position target);
    virtual void onUnitDiscover(BWAPI::Unit unit);
    virtual void onUnitEvade(BWAPI::Unit unit);
    virtual void onUnitShow(BWAPI::Unit unit);
    virtual void onUnitHide(BWAPI::Unit unit);
    virtual void onUnitCreate(BWAPI::Unit unit);
    virtual void onUnitDestroy(BWAPI::Unit unit);
    virtual void onUnitMorph(BWAPI::Unit unit);
    virtual void onUnitRenegade(BWAPI::Unit unit);
    virtual void onSaveGame(std::string gameName);
    virtual void onUnitComplete(BWAPI::Unit unit);
    // Everything below this line is safe to modify.


  // Status of AI
    static double supply_ratio; // for supply levels.  Supply is an inhibition on growth rather than a resource to spend.  Cost of growth.
    static bool supply_starved;
    static double gas_proportion; // for gas levels. Gas is critical for spending but will be matched with supply.
    static bool gas_starved;
    double win_rate; //fairly straighforward.

    static bool army_starved;
    static bool econ_starved;
    static bool tech_starved;
    static bool larva_starved;
    static bool larva_flooded;
    static double adaptation_rate; //Adaptation rate to opponent.
    static double alpha_army_original;
    static double alpha_tech_original;
    static double alpha_econ_original;

    //Game should begin some universally declared inventories.
    static PlayerModel friendly_player_model;
    static PlayerModel enemy_player_model;
    static PlayerModel neutral_player_model;
    static CombatManager combat_manager;
    static Resource_Inventory land_inventory; // resources.
    static MapInventory currentMapInventory;  // macro variables, not every unit I have.
    static FAP::FastAPproximation<StoredUnit*> MCfap; // integrating FAP into combat with a produrbation.
    static TechManager techmanager;
    static AssemblyManager assemblymanager;
    static BuildingGene buildorder; //
    static Reservation my_reservation;
    static LearningManager learned_plan;
    static WorkerManager workermanager;
    static BaseManager basemanager;

    //These measure its clock.
    int short_delay;
    int med_delay;
    int long_delay;

    char delay_string[50];
    char map_string[50];
    char playermodel_string[50];
    char larva_string[50];
    char worker_string[50];
    char scouting_string[50];
    char combat_string[50];
    char detection_string[50];
    char upgrade_string[50];
    char creep_colony_string[50];

    Race starting_enemy_race;

    int t_build;
    int last_frame_of_larva_morph_command = 0;
    int last_frame_of_hydra_morph_command = 0;
    int last_frame_of_muta_morph_command = 0;

    // Assembly Functions

    static bool checkInCartridge(const UnitType & ut);
    static bool checkInCartridge(const UpgradeType & ut);
    static bool checkInCartridge(const TechType & ut);
    static bool checkOpenToBuild(const UnitType & ut, const bool & extra_criteria); // In Cartridge and either next in BO or BO is clear.
    static bool checkWillingAndAble(const Unit & unit, const UnitType & ut, const bool & extra_criteria, const int & travel_distance = 0); // checks if UT is willing and able to be built next by unit. Used in many assembly functions. Checks affordability based on travel distance.  Requires it's part of BO or extra critera is met.
    static bool checkWillingAndAble(const UnitType & ut, const bool & extra_criteria, const int & travel_distance = 0);  // checks if UT is willing and able to be built in general by player. Used in many assembly functions. Checks affordability based on travel distance.  Requires it's part of BO or extra critera is met.
    static bool checkWillingAndAble(const UpgradeType & ut, const bool & extra_criteria);  // checks if UT is willing and able to be built. Used in many assembly functions.  Requires it's part of BO or extra critera is met.
    static bool checkWillingAndAble(const Unit &unit, const UpgradeType &up, const bool &extra_criteria); // checks if UP is willing and able to be built. Used in many assembly functions.  Requires it's part of BO or extra critera is met.
    static bool checkWilling(const UnitType & ut, const bool & extra_criteria); // checks if player is willing ito build the unit, in general. Does not care about ability to build it.

    static bool checkFeasibleRequirement(const Unit & unit, const UnitType & ut);     // checks if ut is required and can be built by unit at this time.
    static bool checkFeasibleRequirement(const Unit & unit, const UpgradeType & up);     // checks if up is required and can be built by unit at this time.
    static bool checkFeasibleRequirement(const UpgradeType & up);   //checks if up is required and can be built.


    //Forces selected unit (drone, hopefully!) to expo:
    static bool Expo(const Unit &unit, const bool &extra_critera, MapInventory &inv);

    // Utility Functions
        // Prints unit's last error directly onto it.
    void PrintError_Unit(const Unit &unit);
    // Identifies those moments where a worker is gathering $$$ and its unusual subsets.
    bool isActiveWorker(const Unit &unit);
    // An improvement on existing idle scripts. Checks if it is carrying, or otherwise busy. If it is stopped, it assumes it is not busy.
    bool isIdleEmpty(const Unit &unit);
    // When should we reset the lock?
    bool isInLine(const Unit &unit);
    // evaluates the value of a stock of buildings, in terms of total cost (min+gas). Assumes building is zerg and therefore, a drone was spent on it.
    static bool isFightingUnit(const Unit &unit);
    static bool isFightingUnit(const StoredUnit & unit);
    static bool isFightingUnit(const UnitType & unittype);

    // evaluates if it was order to fight recently.
    static bool isRecentCombatant(const StoredUnit &su);
    static void writePlayerModel(const PlayerModel &player, const string label);   //writes aribtrary player model to file.


    //static bool Futile_Fight( Unit unit, Unit enemy );

    // Outlines the case where you can attack their type (air/ground/cloaked)
    static bool Can_Fight(UnitType u_type, UnitType e_type);
    static bool Can_Fight(Unit unit, Unit enemy);
    static bool Can_Fight(Unit unit, StoredUnit enemy);
    static bool Can_Fight(StoredUnit unit, StoredUnit enemy);
    static bool Can_Fight(StoredUnit unit, Unit enemy);
    static bool canContributeToFight(const UnitType & ut, const UnitInventory enemy);
    static bool isInDanger(const UnitType & ut, const UnitInventory enemy);

    // Returns top speed of unit with upgrades.
    static double getProperSpeed(const Unit u);
    static double getProperSpeed(const UnitType & type, const Player owner = Broodwar->self());
    //range in pixels, including upgrades
    static int getExactRange(const Unit u);
    static int getExactRange(const UnitType u_type, const Player owner = Broodwar->self());
    //Shortcut returns 32 if unit is melee.
    static int getFunctionalRange(const UnitType u_type, const Player owner);
    static int getFunctionalRange(const Unit u);

    //Returns about how far a unit can move+shoot in a FAP sim duration.
    static int getChargableDistance(const Unit &u);

    //checks if there is a clear path to target. in minitiles. May now choose the map directly, and threshold will break as FALSE for values greater than or equal to. More flexible than previous versions.
    static bool isClearRayTrace(const Position &initialp, const Position &finalp, const vector<vector<int>> &target_map, const int &threshold);
    // Same but only checks the map itself.
    //static bool isMapClearRayTrace( const Position & initialp, const Position & finalp, const MapInventory & inv );
    //counts the number of minitiles in a smooth path to target that are less than that value. May now choose the map directly, and threshold will break as FALSE for values greater than or equal to. More flexible than previous versions.
    static int getClearRayTraceSquares(const Position &initialp, const Position &finalp, const vector<vector<int>> &target_map, const int &threshold);
    //gets the nearest choke by simple counting along in the direction of the final unit.
    static Position getNearestChoke(const Position & initial, const Position &final, const MapInventory & inv);

    //Strips the RACE_ from the front of the unit type string.
    static const char * noRaceName(const char *name);
    //Converts a unit inventory into a unit set directly. Checks range. Careful about visiblity.
    Unitset getUnit_Set(const UnitInventory & ui, const Position & origin, const int & dist);
    // Directly looks up unit in Unitinventory. Returns null if it is not found.
    static StoredUnit * getStoredUnit(const UnitInventory & ui, const Unit & u);
    //Gets pointer to closest unit to origin in appropriate inventory. Checks range. Careful about visiblity.
    static StoredUnit* getClosestStored(UnitInventory & ui, const Position & origin, const int & dist);
    static StoredUnit* getClosestStored(UnitInventory &ui, const UnitType &u_type, const Position &origin, const int &dist);
    static StoredUnit * getClosestGroundStored(UnitInventory & ui, const Position & origin);
    static StoredUnit * getClosestAirStored(UnitInventory & ui, const Position & origin);
    static StoredUnit * getClosestAirStoredWithPriority(UnitInventory & ui, const Position & origin);
    static StoredUnit * getClosestStoredBuilding(UnitInventory & ui, const Position & origin, const int & dist);
    static StoredUnit * getClosestStoredAvailable(UnitInventory & ui, const UnitType & u_type, const Position & origin, const int & dist);
    static Stored_Resource* getClosestStored(Resource_Inventory &ri, const Position &origin, const int & dist);
    static Stored_Resource* getClosestStored(Resource_Inventory & ri, const UnitType & r_type, const Position & origin, const int & dist);
    static StoredUnit * getClosestStored(const Unit unit, UnitInventory & ui, const UnitType & u_type, const int & dist);
    static Stored_Resource * getClosestGroundStored(Resource_Inventory & ri, const Position & origin);
    static Stored_Resource * getClosestGroundStored(Resource_Inventory & ri, const UnitType type, const Position & origin);
    //static Position getClosestExpo(const MapInventory &inv, const UnitInventory &ui, const Position &origin, const int &dist = 999999);


    //Gets pointer to closest attackable unit to point in UnitInventory. Checks range. Careful about visiblity.
    static StoredUnit * getClosestAttackableStored(UnitInventory & ui, const Unit unit, const int & dist);
    //Gets pointer to closest threat or target to unit in UnitInventory. Checks range. Careful about visiblity.
    static StoredUnit * getClosestThreatOrTargetStored(UnitInventory & ui, const UnitType & u_type, const Position & origin, const int & dist);
    static StoredUnit * getClosestThreatOrTargetStored(UnitInventory & ui, const Unit & unit, const int & dist = 999999);
    static StoredUnit * getClosestThreatOrTargetExcluding(UnitInventory & ui, const UnitType ut, const Unit & unit, const int & dist);
    static StoredUnit * getClosestThreatOrTargetWithPriority(UnitInventory & ui, const Unit & unit, const int & dist);
    static StoredUnit * getClosestThreatWithPriority(UnitInventory & ui, const Unit & unit, const int & dist); // gets the closest threat that is considered worth attacking (no interceptors, for example).
    static StoredUnit * getClosestTargettWithPriority(UnitInventory & ui, const Unit & unit, const int & dist); // gets the closest target that is considered worth attacking (no interceptors, for example).
    static StoredUnit * getClosestGroundWithPriority(UnitInventory & ui, const Position & pos, const int & dist = 999999);
    static StoredUnit * getClosestIndicatorOfArmy(UnitInventory & ui, const Position & pos, const int & dist = 999999);
    static bool hasPriority(StoredUnit e);
    static StoredUnit * getClosestThreatStored(UnitInventory & ui, const Unit & unit, const int & dist);
    static StoredUnit * getMostAdvancedThreatOrTargetStored(UnitInventory & ui, const Unit & unit, const int & dist = 999999);


    //Searches an enemy inventory for units of a type within a range. Returns enemy inventory meeting that critera. Returns pointers even if the unit is lost, but the pointers are empty.
    static UnitInventory getUnitInventoryInRadius(const UnitInventory &ui, const Position &origin, const int &dist);
    static UnitInventory getThreateningUnitInventoryInRadius(const UnitInventory & ui, const Position & origin, const int & dist, const bool & air_attack);
    static UnitInventory getUnitsOutOfReach(const UnitInventory & ui, const Unit & target);
    static UnitInventory getUnitInventoryInArea(const UnitInventory & ui, const Position & origin);
    static UnitInventory getUnitInventoryInArea(const UnitInventory & ui, const int AreaID);
    static UnitInventory getUnitInventoryInNeighborhood(const UnitInventory & ui, const Position & origin);
    static UnitInventory getUnitInventoryInArea(const UnitInventory & ui, const UnitType ut, const Position & origin);

    static Resource_Inventory CUNYAIModule::getResourceInventoryInArea(const Resource_Inventory &ri, const Position &origin);
    //Overload. Searches for units of a specific type.
    static UnitInventory getUnitInventoryInRadius(const UnitInventory &ui, const UnitType u_type, const Position &origin, const int &dist);
    static Resource_Inventory getResourceInventoryInRadius(const Resource_Inventory & ri, const Position & origin, const int & dist);
    //Searches an inventory for units of within a range. Returns TRUE if the area is occupied.
    static bool checkOccupiedArea(const UnitInventory &ui, const Position &origin);
    static bool checkOccupiedNeighborhood(const UnitInventory & ui, const Position & origin);
    static bool checkOccupiedArea(const UnitInventory & ui, const UnitType type, const Position & origin);
    //Searches if a particular unit is within a range of the position. Returns TRUE if the area is occupied. Checks retangles for performance reasons rather than radius.
    static bool checkUnitOccupiesArea(const Unit &unit, const Position &origin, const int & dist);


    // Utility functions that need to be accessed by any number of classes, ie. static declarations.
        // Counts the tally of a particular int a specific unit set. Includes those in production.
    static int countUnits(const UnitType &type, const Unitset &unit_set);
    // Counts the tally of a particular unit type. Includes those in production, those in inventory (passed by value).
    static int countUnits(const UnitType &type, const UnitInventory &ei);
    static bool containsUnit(const UnitType & type, const UnitInventory & ui);
    // Counts all units of a type or successors of that type.
    static int countSuccessorUnits(const UnitType & type, const UnitInventory & ui = CUNYAIModule::friendly_player_model.units_);
    // Counts the tally of a particular unit type in a reservation queue.
    static int const countUnits(const UnitType &type, const Reservation &res);
    // Counts the tally of all created units in my personal inventory of that type
    static int countUnits(const UnitType &type, bool reservations_included = false);
    // Counts the tally of a particular unit type performing X. Includes those in production, those in inventory (passed by value).
    static int countUnitsDoing(const UnitType &type, const UnitCommandType &u_command_type, const Unitset &unit_set);
    static int countUnitsDoing(const UnitType & type, const UnitCommandType & u_command_type, const UnitInventory & ui);
    static int countUnitsInProgress(const UnitType & type, const UnitInventory & ui);
    static int countUnitsInProgress(const UnitType & type);
    // Counts units that can perform a given upgrade, uses BWAPI's unit sets so it is somewhat slow but requires details I don't otherwise store.
    static int countUnitsAvailableToPerform(const UpgradeType & upType);
    // Counts units that can perform a given research (tech), uses BWAPI's unit sets so it is somewhat slow but requires details I don't otherwise store.
    static int countUnitsAvailableToPerform(const TechType & techType);
    // Evaluates the total stock of a type of unit in the inventory.
    static int Stock_Units(const UnitType & unit_type, const UnitInventory & ui);
    // evaluates the value of a stock of combat units, for all unit types in a unit inventory.
    static int Stock_Combat_Units(const UnitInventory &ui);

    // Evaluates the value of a stock of buildings, in terms of total cost (min+gas)
    static int Stock_Buildings(const UnitType &building, const UnitInventory &ei);
    // evaluates the value of a stock of upgrades, in terms of total cost (min+gas).
    static int Stock_Ups(const UpgradeType &ups);
    // evaluates stock of tech (eg. lurker_aspect);
    static int Stock_Tech(const TechType & tech);


    //Evaluates stock of allied units in set that can shoot up.
    static int Stock_Units_ShootUp(const UnitInventory &ui);
    // Evaluates stock of allied units in set that can shoot down.
    static int Stock_Units_ShootDown(const UnitInventory &ui);
    // evaluates the value of a stock of unit, in terms of supply added.
    static int Stock_Supply(const UnitType &unit);
    // returns both useful stocks if both groups were to have a fight;
    //static vector<int> getUsefulStocks(const UnitInventory &friend_loc, const UnitInventory &enemy_loc);
    // returns the stock of opponants I can actually fight in their local area.
    static int getTargetableStocks(const Unit & u, const UnitInventory & enemy_loc);
    // returns the stock of units that might actually threaten U in region.
    static int getThreateningStocks(const Unit & u, const UnitInventory & enemy_loc);

    // Checks if a particular pixel position will be onscreen. Used to save drawing time on offscreen artwork.
    static bool isOnScreen(const Position &pos, const Position &screen_pos);
    //Returns TRUE if a unit is safe to send an order to. False if the unit has been ordered about recently.
    static bool spamGuard(const Unit & unit, int cd_frames_chosen = 99);
    // Returns the actual center of a unit.
    static Position getUnit_Center(Unit unit);
    // checks if it is safe to build, uses heuristic critera.
    static bool checkSafeBuildLoc(const Position pos);;
    // Checks if it is safe to mine, uses heuristic critera.
    bool checkSafeMineLoc(const Position pos, const UnitInventory &ui, const MapInventory &inv);

    static double bindBetween(double x, double lower_bound, double upper_bound);
    // Gets total value of FAP structure using StoredUnits. If friendly player option is chose, it uses P1, the standard for friendly player.
    static int getFAPScore(FAP::FastAPproximation<StoredUnit*>& fap, bool friendly_player);
    static bool checkMiniFAPForecast(UnitInventory & ui, UnitInventory & ei, const bool equality_is_win);
    // Tells if we will be dealing more damage than we recieve, proportionally or total.
    static bool checkSuperiorFAPForecast(const UnitInventory & ui, const UnitInventory & ei, const bool equality_is_win = false);
    // Tells the size of the losses after a fight. The fodder setting also includes the results of destroying the units that cannot defend themselves, such as a nexus.
    static int getFAPDamageForecast(const UnitInventory & ui, const UnitInventory & ei, const bool fodder = true);
    // Tells the size of the surviving forces after a fight. The fodder setting also includes the results of surviving units that cannot defend themselves, such as a nexus.
    static int getFAPSurvivalForecast(const UnitInventory & ui, const UnitInventory & ei, const int duration, const bool fodder = true);
    // Mostly a check if the unit can be touched. Includes spamguard, much of this is a holdover from the Examplebot.
    static bool checkUnitTouchable(const Unit & u);
    static void DiagnosticTrack(const Unit & u);
    static void DiagnosticTrack(const Position & p);
    static bool updateUnitPhase(const Unit & u, const StoredUnit::Phase phase); // finds the unit in friendly unit inventory and updates its phase. Function updates that the unit has been touched.
    static bool updateUnitBuildIntent(const Unit & u, const UnitType & intended_build_type, const TilePosition & intended_build_tile); // finds the unit in friendly unit inventory and updates its phase to prebuild , its intended build type to Type, and its intended build tile to the listed tileposition. Function updates that the unit has been touched.
    // Checks if an area (by position) is dangerous for a unit to be in. If it is dangerous, returns TRUE.
    static bool checkDangerousArea(const UnitType ut, const Position pos);
    static bool checkDangerousArea(const UnitType ut, const int AreaID);

    // Removes ( ) and " " from string.
    static string safeString(string input);

    static int convertTileDistanceToPixelDistance(int numberOfTiles);
    static int convertPixelDistanceToTileDistance(int numberOfPixels);

// Vision Functions
    // returns number of visible tiles.
    int Vision_Count();

    //Suprisingly missing functions:
    template< typename ContainerT, typename PredicateT >
    void erase_if(ContainerT& items, const PredicateT& predicate) {
        for (auto it = items.begin(); it != items.end(); ) {
            if (predicate(*it)) it = items.erase(it);
            else ++it;
        }
    };

    template<typename Iter, typename RandomGenerator>
    static Iter select_randomly(Iter start, Iter end, RandomGenerator& g) {
        std::uniform_int_distribution<> dis(0, std::distance(start, end) - 1);
        std::advance(start, dis(g));
        return start;
    }

    template<typename Iter>
    static Iter select_randomly(Iter start, Iter end) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        return select_randomly(start, end, gen);
    }

    friend Position operator+(const Position &A, const Position &B)
    {
        return Position(A.x + B.x, A.y + B.y);
    }
    friend Position operator-(const Position &A, const Position &B)
    {
        return Position(A.x - B.x, A.y - B.y);
    }


};
