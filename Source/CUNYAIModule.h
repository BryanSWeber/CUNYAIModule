#pragma once

#include <BWAPI.h> //4.2.0 BWAPI
#include "Map_Inventory.h"
#include "Unit_Inventory.h"
#include "Resource_Inventory.h"
#include "Research_Inventory.h"
#include "AssemblyManager.h"
#include "Reservation_Manager.h"
#include "PlayerModelManager.h"
#include "FAP\FAP\include\FAP.hpp"
#include "GeneticHistoryManager.h"
#include "TechManager.h"
#include "WorkerManager.h"
#include "CombatManager.h"

#include <bwem.h>
#include "BWEB\BWEB.h"
//#include "BrawlSim\BrawlSimLib\include\BrawlSim.hpp"
#include <chrono> // for in-game frame clock.

constexpr bool RESIGN_MODE = false; // must be off for proper game close in SC-docker
constexpr bool ANALYSIS_MODE = false; // Printing game logs, game status every few frames, etc.
constexpr bool DRAWING_MODE = true; //Visualizations, printing records, etc.Should seperate these.
constexpr bool MOVE_OUTPUT_BACK_TO_READ = false; // should be FALSE for sc-docker, TRUE for chaoslauncher at home & Training against base ai.
constexpr bool SSCAIT_OR_DOCKER = true; // should be TRUE for SC-docker, TRUE for SSCAIT.
constexpr bool LEARNING_MODE = true; //if we are exploring new positions or simply keeping existing ones.  Should almost always be on. If off, prevents both mutation and interbreeding of parents, they will only clone themselves.
constexpr bool TIT_FOR_TAT_ENGAGED = true; // permits in game-tit-for-tat responses.  Consider disabling this for TEST_MODE.
constexpr bool TEST_MODE = true; // Locks in a build order and defined paramaters. Consider disabling TIT_FOR_TAT.
constexpr int FAP_SIM_DURATION = 24*5; // set FAP sim durations.
constexpr bool RANDOM_PLAN = false; // Turn off learning and always use a random set of starting conditions.
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
  static double adaptation_rate; //Adaptation rate to opponent.
  static double alpha_army_original;
  static double alpha_tech_original;
  static double alpha_econ_original;

 //Game should begin some universally declared inventories.
    static Player_Model friendly_player_model;
    static Player_Model enemy_player_model;
    static Player_Model neutral_player_model;
    static CombatManager combat_manager;
    static Resource_Inventory land_inventory; // resources.
    static Map_Inventory current_map_inventory;  // macro variables, not every unit I have.
    static FAP::FastAPproximation<Stored_Unit*> MCfap; // integrating FAP into combat with a produrbation.
    static TechManager techmanager;
    static AssemblyManager assemblymanager;
    static Building_Gene buildorder; //
    static Reservation my_reservation;
    static GeneticHistory gene_history;
    static WorkerManager workermanager;

    //These measure its clock.
    int short_delay;
    int med_delay;
    int long_delay;

    char delay_string [50];
    char map_string[50];
    char playermodel_string[50];
    char larva_string [50];
    char worker_string [50];
    char scouting_string [50];
    char combat_string [50];
    char detection_string [50];
    char upgrade_string [50];
    char creep_colony_string [50];

    Race starting_enemy_race;

  int t_build;
  int last_frame_of_larva_morph_command = 0;
  int last_frame_of_hydra_morph_command = 0;
  int last_frame_of_muta_morph_command = 0;

  // Assembly Functions

      static bool checkInCartridge( const UnitType & ut);
      static bool checkInCartridge( const UpgradeType & ut);
      static bool checkInCartridge( const TechType & ut);
      // checks if ut is willing and able to be built next by unit. Used in many assembly functions.
      static bool checkDesirable(const Unit &unit, const UnitType &ut, const bool &extra_criteria);
      static bool checkDesirable(const UpgradeType & ut, const bool & extra_criteria);
      static bool checkDesirable(const Unit &unit, const UpgradeType &up, const bool &extra_criteria);
      static bool checkDesirable(const UnitType & ut, const bool & extra_criteria);
      // checks if ut is required and can be built by unit at this time.
      static bool checkFeasibleRequirement(const Unit & unit, const UnitType & ut);
      // checks if up is required and can be built by unit at this time.
      static bool checkFeasibleRequirement(const Unit & unit, const UpgradeType & up);

      //Forces selected unit (drone, hopefully!) to expo:
      static bool Expo( const Unit &unit , const bool &extra_critera, Map_Inventory &inv);

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
      static bool isFightingUnit(const Stored_Unit & unit);
      static bool isFightingUnit(const UnitType & unittype);

      // evaluates if it was order to fight recently.
      static bool isRecentCombatant(const Stored_Unit &su);
      // Draws a line if diagnostic mode is TRUE.
      static void Diagnostic_Line(const Position &s_pos, const Position &f_pos, const Position &screen_pos, Color col );
      static void Diagnostic_Tiles(const Position & screen_pos, Color col);
      static void Diagnostic_Watch_Position(TilePosition & tp);
      static void Diagnostic_Destination(const Unit_Inventory & ui, const Position & screen_pos, Color col);
      static void Diagnostic_Dot(const Position & s_pos, const Position & screen_pos, Color col);
      static void DiagnosticHitPoints(const Stored_Unit unit, const Position & screen_pos);
      static void DiagnosticFAP(const Stored_Unit unit, const Position & screen_pos);
      static void DiagnosticDeath(const Stored_Unit unit, const Position & screen_pos);
      static void DiagnosticLastDamage(const Stored_Unit unit, const Position & screen_pos);
      static void DiagnosticMineralsRemaining(const Stored_Resource unit, const Position & screen_pos);
      static void DiagnosticSpamGuard(const Stored_Unit unit, const Position & screen_pos);
      static void DiagnosticLastOrder(const Stored_Unit unit, const Position & screen_pos);
      static void DiagnosticPhase(const Stored_Unit unit, const Position & screen_pos);
      static void DiagnosticReservations(const Reservation reservations, const Position & screen_pos);
      static void writePlayerModel(const Player_Model &player, const string label);   //writes aribtrary player model to file.

      //Sends a diagnostic text message, accepts another argument..
      template<typename ...Ts>
      static void DiagnosticText(char const *fmt, Ts && ... vals) {
          if constexpr (DRAWING_MODE) {
              Broodwar->sendText(fmt, std::forward<Ts>(vals) ...);
          }
      }
      // Defunct: Outlines the case where you cannot attack their type (air/ground/cloaked), while they can attack you.
      //static bool Futile_Fight( Unit unit, Unit enemy );

      // Outlines the case where you can attack their type (air/ground/cloaked)
      static bool Can_Fight( Unit unit, Unit enemy );
      static bool Can_Fight( Unit unit, Stored_Unit enemy );
      static bool Can_Fight( Stored_Unit unit, Stored_Unit enemy);
      static bool Can_Fight( Stored_Unit unit, Unit enemy );
      static bool canContributeToFight(const UnitType & ut, const Unit_Inventory enemy);
      static bool isInDanger(const UnitType & ut, const Unit_Inventory enemy);
      // Can_Fight_Type does NOT check cloaked status.
      static bool Can_Fight_Type( UnitType unittype, UnitType enemytype);

      // Returns top speed of unit with upgrades.
      static double getProperSpeed( const Unit u );
      static double getProperSpeed(const UnitType & type, const Player owner = Broodwar->self() );
      //range in pixels, including upgrades
      static int getProperRange(const Unit u);
      static int getProperRange(const UnitType u_type, const Player owner = Broodwar->self() );
      static int getChargableDistance(const Unit &u);

      //checks if there is a clear path to target. in minitiles. May now choose the map directly, and threshold will break as FALSE for values greater than or equal to. More flexible than previous versions.
      static bool isClearRayTrace(const Position &initialp, const Position &finalp, const vector<vector<int>> &target_map, const int &threshold);
      // Same but only checks the map itself.
      //static bool isMapClearRayTrace( const Position & initialp, const Position & finalp, const Map_Inventory & inv );
      //counts the number of minitiles in a smooth path to target that are less than that value. May now choose the map directly, and threshold will break as FALSE for values greater than or equal to. More flexible than previous versions.
      static int getClearRayTraceSquares(const Position &initialp, const Position &finalp, const vector<vector<int>> &target_map, const int &threshold);
      //gets the nearest choke by simple counting along in the direction of the final unit.
      static Position getNearestChoke( const Position & initial, const Position &final, const Map_Inventory & inv );

      // Announces to player the name and type of all of their upgrades. Bland but practical. Counts those in progress.
      void Print_Upgrade_Inventory( const int &screen_x, const int &screen_y );
      // Announces to player the name and type of all known units in set.
      void Print_Unit_Inventory( const int &screen_x, const int &screen_y, const Unit_Inventory &ui );
      void Print_Test_Case(const int & screen_x, const int & screen_y);
      void Print_Cached_Inventory(const int & screen_x, const int & screen_y);
      void Print_Research_Inventory(const int & screen_x, const int & screen_y, const Research_Inventory & ri);
      // Announces to player the name and type of all units remaining in the Buildorder. Bland but practical.
      void Print_Build_Order_Remaining( const int & screen_x, const int & screen_y, const Building_Gene & bo );
      // Announces to player the name and type of all units remaining in the reservation system. Bland but practical.
      void Print_Reservations( const int &screen_x, const int &screen_y, const Reservation &res );

      //Strips the RACE_ from the front of the unit type string.
      static const char * noRaceName( const char *name );
      //Converts a unit inventory into a unit set directly. Checks range. Careful about visiblity.
      Unitset getUnit_Set( const Unit_Inventory & ui, const Position & origin, const int & dist );
      //Gets pointer to closest unit to origin in appropriate inventory. Checks range. Careful about visiblity.
      static Stored_Unit* getClosestStored( Unit_Inventory & ui, const Position & origin, const int & dist );
      static Stored_Unit* getClosestStored(Unit_Inventory &ui, const UnitType &u_type, const Position &origin, const int &dist);
      static Stored_Unit * getClosestGroundStored(Unit_Inventory & ui, const Position & origin);
      static Stored_Unit * getClosestAirStored(Unit_Inventory & ui, const Position & origin);
      static Stored_Unit * getClosestStoredBuilding(Unit_Inventory & ui, const Position & origin, const int & dist);
      static Stored_Resource* getClosestStored(Resource_Inventory &ri, const Position &origin, const int & dist);
      static Stored_Resource* getClosestStored(Resource_Inventory & ri, const UnitType & r_type, const Position & origin, const int & dist);
      static Stored_Resource * getClosestGroundStored(Resource_Inventory & ri, const Position & origin);
      static Stored_Resource * getClosestGroundStored(Resource_Inventory & ri, const UnitType type, const Position & origin);
      //static Position getClosestExpo(const Map_Inventory &inv, const Unit_Inventory &ui, const Position &origin, const int &dist = 999999);


      //Gets pointer to closest attackable unit to point in Unit_inventory. Checks range. Careful about visiblity.
      static Stored_Unit * getClosestAttackableStored(Unit_Inventory & ui, const Unit unit, const int & dist);
      //Gets pointer to closest threat or target to unit in Unit_inventory. Checks range. Careful about visiblity.
      static Stored_Unit * getClosestThreatOrTargetStored( Unit_Inventory & ui, const UnitType & u_type, const Position & origin, const int & dist );
      static Stored_Unit * getClosestThreatOrTargetStored( Unit_Inventory & ui, const Unit & unit, const int & dist = 999999);
      static Stored_Unit * getClosestThreatOrTargetExcluding(Unit_Inventory & ui, const UnitType ut, const Unit & unit, const int & dist);
      static Stored_Unit * getClosestThreatStored(Unit_Inventory & ui, const Unit & unit, const int & dist);
      static Stored_Unit * getMostAdvancedThreatOrTargetStored( Unit_Inventory & ui, const Unit & unit, const int & dist = 999999);


      //Searches an enemy inventory for units of a type within a range. Returns enemy inventory meeting that critera. Returns pointers even if the unit is lost, but the pointers are empty.
      static Unit_Inventory getUnitInventoryInRadius( const Unit_Inventory &ui, const Position &origin, const int &dist );
      static Unit_Inventory getThreateningUnitInventoryInRadius(const Unit_Inventory & ui, const Position & origin, const int & dist, const bool & air_attack);
      static Unit_Inventory getUnitsOutOfReach(const Unit_Inventory & ui, const Unit & target);
      static Unit_Inventory getUnitInventoryInArea(const Unit_Inventory & ui, const Position & origin);
      static Unit_Inventory getUnitInventoryInNeighborhood(const Unit_Inventory & ui, const Position & origin);
      static Unit_Inventory getUnitInventoryInArea(const Unit_Inventory & ui, const UnitType ut, const Position & origin);

      static Resource_Inventory CUNYAIModule::getResourceInventoryInArea(const Resource_Inventory &ri, const Position &origin);
      //Overload. Searches for units of a specific type.
      static Unit_Inventory getUnitInventoryInRadius(const Unit_Inventory &ui, const UnitType u_type, const Position &origin, const int &dist);
      //Searches an inventory for units of within a range. Returns TRUE if the area is occupied.
      static bool checkOccupiedArea( const Unit_Inventory &ui, const Position &origin );
      static bool checkOccupiedNeighborhood(const Unit_Inventory & ui, const Position & origin);
      static bool checkOccupiedArea( const Unit_Inventory & ui, const UnitType type, const Position & origin);
      //Searches if a particular unit is within a range of the position. Returns TRUE if the area is occupied. Checks retangles for performance reasons rather than radius.
      static bool checkUnitOccupiesArea( const Unit &unit, const Position &origin, const int & dist );


  // Utility functions that need to be accessed by any number of classes, ie. static declarations.
      // Counts the tally of a particular int a specific unit set. Includes those in production.
      static int Count_Units( const UnitType &type, const Unitset &unit_set );
      // Counts the tally of a particular unit type. Includes those in production, those in inventory (passed by value).
      static int Count_Units( const UnitType &type, const Unit_Inventory &ei );
      static bool Contains_Unit(const UnitType & type, const Unit_Inventory & ui);
      static int Count_SuccessorUnits(const UnitType & type, const Unit_Inventory & ui);
      // Counts the tally of a particular unit type in a reservation queue.
      static int Count_Units( const UnitType &type, const Reservation &res );
      // Counts the tally of all created units in my personal inventory of that type.
      static int Count_Units(const UnitType & type);
      // Counts the tally of a particular unit type performing X. Includes those in production, those in inventory (passed by value).
      static int Count_Units_Doing(const UnitType &type, const UnitCommandType &u_command_type, const Unitset &unit_set);
      static int Count_Units_Doing(const UnitType & type, const UnitCommandType & u_command_type, const Unit_Inventory & ui);
      static int Count_Units_In_Progress(const UnitType & type, const Unit_Inventory & ui);
      static int Count_Units_In_Progress(const UnitType & type);
      // Evaluates the total stock of a type of unit in the inventory.
      static int Stock_Units( const UnitType & unit_type, const Unit_Inventory & ui );
      // evaluates the value of a stock of combat units, for all unit types in a unit inventory.
      static int Stock_Combat_Units( const Unit_Inventory &ui );

      // Evaluates the value of a stock of buildings, in terms of total cost (min+gas)
      static int Stock_Buildings( const UnitType &building, const Unit_Inventory &ei );
      // evaluates the value of a stock of upgrades, in terms of total cost (min+gas).
      static int Stock_Ups( const UpgradeType &ups );
      // evaluates stock of tech (eg. lurker_aspect);
      static int Stock_Tech(const TechType & tech);


      //Evaluates stock of allied units in set that can shoot up.
      static int Stock_Units_ShootUp( const Unit_Inventory &ui );
      // Evaluates stock of allied units in set that can shoot down.
      static int Stock_Units_ShootDown( const Unit_Inventory &ui );
      // evaluates the value of a stock of unit, in terms of supply added.
      static int Stock_Supply( const UnitType &unit );
      // returns both useful stocks if both groups were to have a fight;
      //static vector<int> getUsefulStocks(const Unit_Inventory &friend_loc, const Unit_Inventory &enemy_loc);
      // returns the stock of opponants I can actually fight in their local area.
      static int getTargetableStocks(const Unit & u, const Unit_Inventory & enemy_loc);
      // returns the stock of units that might actually threaten U in region.
      static int getThreateningStocks(const Unit & u, const Unit_Inventory & enemy_loc);

      // Checks if a particular pixel position will be onscreen. Used to save drawing time on offscreen artwork.
      static bool isOnScreen( const Position &pos , const Position &screen_pos);
      //Returns TRUE if a unit is safe to send an order to. False if the unit has been ordered about recently.
      static bool spamGuard(const Unit & unit, int cd_frames_chosen = 99);
      // Returns the actual center of a unit.
      static Position getUnit_Center(Unit unit);
      // checks if it is safe to build, uses heuristic critera.
      static bool checkSafeBuildLoc(const Position pos);;
      // Checks if it is safe to mine, uses heuristic critera.
      bool checkSafeMineLoc(const Position pos, const Unit_Inventory &ui, const Map_Inventory &inv);

      static double bindBetween(double x, double lower_bound, double upper_bound);
      // Gets total value of FAP structure using Stored_Units. If friendly player option is chose, it uses P1, the standard for friendly player.
      static int getFAPScore(FAP::FastAPproximation<Stored_Unit*>& fap, bool friendly_player);
      static bool checkMiniFAPForecast(Unit_Inventory & ui, Unit_Inventory & ei);
      // Tells if we will be dealing more damage than we recieve, proportionally or total.
      static bool checkSuperiorFAPForecast(const Unit_Inventory & ui, const Unit_Inventory & ei);
      // Tells the size of the losses after a fight. The fodder setting also includes the results of destroying the units that cannot defend themselves, such as a nexus.
      static int getFAPDamageForecast(const Unit_Inventory & ui, const Unit_Inventory & ei, const bool fodder = true);
      // Tells the size of the surviving forces after a fight. The fodder setting also includes the results of surviving units that cannot defend themselves, such as a nexus.
      static int getFAPSurvivalForecast(const Unit_Inventory & ui, const Unit_Inventory & ei, const bool fodder = true);
      // Mostly a check if the unit can be touched. Includes spamguard, much of this is a holdover from the Examplebot.
      static bool checkUnitTouchable(const Unit & u);
      static void DiagnosticTrack(const Unit & u);
      static void DiagnosticTrack(const Position & p);
      static bool updateUnitPhase(const Unit & u, const Stored_Unit::Phase phase); // finds the unit in friendly unit inventory and updates its phase. Function updates that the unit has been touched.
      static bool updateUnitBuildIntent(const Unit & u, const UnitType & intended_build_type, const TilePosition & intended_build_tile); // finds the unit in friendly unit inventory and updates its phase to prebuild , its intended build type to Type, and its intended build tile to the listed tileposition. Function updates that the unit has been touched.


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

     friend Position operator+(const Position &A, const Position &B)
     {
         return Position(A.x + B.x, A.y + B.y);
     }
     friend Position operator-(const Position &A, const Position &B)
     {
         return Position(A.x - B.x, A.y - B.y);
     }
};
