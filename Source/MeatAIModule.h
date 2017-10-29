#pragma once

#include <BWAPI.h> //4.2.0 BWAPI
#include "InventoryManager.h"
#include "Unit_Inventory.h"
#include "Resource_Inventory.h"
#include "Fight_MovementManager.h"
#include "AssemblyManager.h"
#include <chrono> // for in-game frame clock.

#define _ANALYSIS_MODE true
#define _COBB_DOUGLASS_REVEALED false
#define _RESIGN_MODE false
#define _AT_HOME_MODE false

//#define _RESIGN_MODE true
//#define _AT_HOME_MODE true
//#define _ANALYSIS_MODE true
//#define _COBB_DOUGLASS_REVEALED true

// Remember not to use "Broodwar" in any global class constructor!

class MeatAIModule : public BWAPI::AIModule
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

  double alpha_army; // for troop levels.
    bool army_starved; 
  double alpha_vis; // for vision levels.
    bool vision_starved;
  double alpha_econ; // for econ levels.
    bool econ_starved; 
  double alpha_tech; // for tech levels.
    bool tech_starved;
  double gamma; // for supply levels.  Supply is an inhibition on growth rather than a resource to spend.  Cost of growth.
    bool supply_starved;
  double delta; // for gas levels. Gas is critical for spending but will be matched with supply.
    bool gas_starved;
  double win_rate; //fairly straighforward.

  int miner_count_; // a temp variable
 //Game should begin some universally declared inventories.
    Unit_Inventory enemy_inventory; // enemy units.
    Unit_Inventory friendly_inventory; // friendly units.
	Resource_Inventory neutral_inventory; // neutral resources.

    Inventory inventory;  // macro variables, not every unit I have.
    Building_Gene buildorder; //

   //These measure its clock.
    int short_delay;
    int med_delay;
    int long_delay;

	char delay_string [50];
	char preamble_string [50];
	char larva_string [50];
	char worker_string [50];
	char scouting_string [50];
	char combat_string [50];
	char detection_string [50];
	char upgrade_string [50];
	char creep_colony_string [50];

  int t_build;

// Personally made functions:

  // Assembly Functions
      //Checks if a building can be built, and passes additional boolean criteria.  If all critera are passed, then it builds the building and delays the building timer 25 frames, or ~1 sec.
      bool Check_N_Build( const UnitType &building, const Unit &unit, const Unit_Inventory &ui, const bool &extra_critera );
      // Check and grow a unit using larva.
      void Check_N_Grow( const UnitType &unittype, const Unit &larva, const bool &extra_critera );
      //Checks if an upgrade can be built, and passes additional boolean criteria.  If all critera are passed, then it performs the upgrade. Requires extra critera.
      void Check_N_Upgrade( const UpgradeType &ups, const Unit &unit, const bool &extra_critera );  
      // Morphs units "Reactively". Incomplete.
      void Reactive_Build( const Unit &larva, const Inventory &inv, const Unit_Inventory &fi, const Unit_Inventory &ei );
      // Builds the next building you can afford.  Incomplete.
      bool Building_Begin( const Unit &drone, const Inventory &inv, const Unit_Inventory &u_inv );

  // Mining Functions
      //Forces selected unit (drone, hopefully!) to expo:
      bool Expo( const Unit &unit , const bool &extra_critera, const Inventory &inv);
      // Checks all bases for undersaturation. Goes to any undersaturated location, preference for local mine.
      void Worker_Mine( const Unit &unit , Unit_Inventory &ui);
      // Checks all refineries for undersaturation. Goes to any undersaturated location, preference for local mine.
	  void Worker_Gas(const Unit &unit, Unit_Inventory &ui);
      // Checks if there is a way to spend gas.
      bool Gas_Outlet();

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
	  bool IsFightingUnit(const Unit &unit);
	  // evaluates if it was order to fight recently.
	  bool isRecentCombatant(const Unit &unit);
      // Draws a line if diagnostic mode is TRUE.
      static void Diagnostic_Line( Position s_pos, Position f_pos, Color col );
      // Outlines the case where you cannot attack their type (air/ground/cloaked), while they can attack you.
      static bool Futile_Fight( Unit unit, Unit enemy );
      // Outlines the case where you can attack their type (air/ground/cloaked)
      static bool Can_Fight( Unit unit, Unit enemy );
      static bool Can_Fight( Unit unit, Stored_Unit enemy );
      static bool Can_Fight( Stored_Unit unit, Unit enemy );

      //checks if there is a smooth path to target. in minitiles
      static bool isClearRayTrace( const Position &initial, const Position &final, const Inventory &inv );
      //counts the number of tiles in a smooth path to target. in minitiles
      static int getClearRayTraceSquares( const Position & initial, const Position & final, const Inventory & inv );
      //gets the nearest choke by simple counting along in the direction of the final unit.
      static Position getNearestChoke( const Position & initial, const Position &final, const Inventory & inv );

      // Announces to player the name and type of all of their upgrades. Bland but practical. Counts those in progress.
      void Print_Upgrade_Inventory( const int &screen_x, const int &screen_y );
      // Announces to player the name and type of all known units in set.
      void Print_Unit_Inventory( const int &screen_x, const int &screen_y, const Unit_Inventory &ui );
      // Announces to player the name and type of all units remaining in the Buildorder. Bland but practical.
      void Print_Build_Order_Remaining( const int & screen_x, const int & screen_y, const Building_Gene & bo );

      //Strips the RACE_ from the front of the unit type string. 
      const char * noRaceName( const char *name );
      //Converts a unit inventory into a unit set directly. Checks range. Careful about visiblity.
      Unitset getUnit_Set( const Unit_Inventory & ui, const Position & origin, const int & dist );
      //Gets pointer to closest unit to origin in appropriate inventory. Checks range. Careful about visiblity.
      Stored_Unit* getClosestStored( Unit_Inventory & ui, const Position & origin, const int & dist );
	  Stored_Unit* getClosestStored(Unit_Inventory &ui, const UnitType &u_type, const Position &origin, const int &dist);
	  Stored_Resource* getClosestStored(Resource_Inventory &ri, const Position &origin, const int & dist);

      //Gets pointer to closest attackable unit to point in Unit_inventory. Checks range. Careful about visiblity.
      Stored_Unit* getClosestAttackableStored( Unit_Inventory &ui, const UnitType &u_type, const Position &origin, const int &dist );
      //Gets pointer to closest threat or target to point in Unit_inventory. Checks range. Careful about visiblity.
      Stored_Unit * getClosestThreatOrTargetStored( Unit_Inventory & ui, const UnitType & u_type, const Position & origin, const int & dist );

      //Searches an enemy inventory for units of a type within a range. Returns enemy inventory meeting that critera. Returns pointers even if the unit is lost, but the pointers are empty.
      static Unit_Inventory getUnitInventoryInRadius( const Unit_Inventory &ui, const Position &origin, const int &dist );
	  static Resource_Inventory MeatAIModule::getResourceInventoryInRadius(const Resource_Inventory &ri, const Position &origin, const int &dist);
	  //Overload. Searches for units of a specific type. 
	  static Unit_Inventory getUnitInventoryInRadius(const Unit_Inventory &ui, const UnitType u_type, const Position &origin, const int &dist);
      //Searches an inventory for units of within a range. Returns TRUE if the area is occupied.
      static bool checkOccupiedArea( const Unit_Inventory &ui, const Position &origin, const int &dist );
      //Searches an inventory for buildings. Returns TRUE if the area is occupied. Checks retangles for performance reasons rather than radius.
      static bool checkBuildingOccupiedArea( const Unit_Inventory & ui, const Position & origin);
      //Searches if a particular unit is within a range of the position. Returns TRUE if the area is occupied. Checks retangles for performance reasons rather than radius.
      static bool checkUnitOccupiesArea( const Unit &unit, const Position &origin, const int & dist );

  // Utility functions that need to be accessed by any number of classes, ie. static declarations.
      // Counts the tally of a particular int a specific unit set. Includes those in production.
      static int Count_Units( const UnitType &type, const Unitset &unit_set );
      // Counts the tally of a particular unit type. Includes those in production, those in inventory (passed by value).
      static int Count_Units( const UnitType &type, const Unit_Inventory &ei );
	  // Counts the tally of a particular unit type performing X. Includes those in production, those in inventory (passed by value).
	  static int Count_Units_Doing(const UnitType &type, const UnitCommandType &u_command_type, const Unitset &unit_set);
      // Evaluates the total stock of a type of unit in the inventory.
      static int Stock_Units( const UnitType & unit_type, const Unit_Inventory & ui );
      // evaluates the value of a stock of combat units, for all unit types in a unit inventory.
      static int Stock_Combat_Units( const Unit_Inventory &ui );

      // Evaluates the value of a stock of buildings, in terms of total cost (min+gas)
      static int Stock_Buildings( const UnitType &building, const Unit_Inventory &ei );
      // evaluates the value of a stock of upgrades, in terms of total cost (min+gas).
      static int Stock_Ups( const UpgradeType &ups );


      //Evaluates stock of allied units in set that can shoot up.
      static int Stock_Units_ShootUp( const Unit_Inventory &ui );
      // Evaluates stock of allied units in set that can shoot down.
      static int Stock_Units_ShootDown( const Unit_Inventory &ui );
      // evaluates the value of a stock of unit, in terms of supply added.
      static int Stock_Supply( const UnitType &unit, const Unit_Inventory &ui );

      // Checks if a particular pixel position will be onscreen. Used to save drawing time on offscreen artwork.
      static bool isOnScreen( const Position &pos );
	  // Returns the actual center of a unit.
	  static Position MeatAIModule::getUnit_Center(Unit unit);
  // Genetic History Functions
      //gathers win history. Imposes genetic learning algorithm, matched on race. 
      double Win_History(std::string file, int value);

  // Vision Functions
      // returns number of visible tiles.
      int Vision_Count();

  // Tech Functions
      // Returns true if there are any new technology improvements available at this time (new buildings, upgrades, researches, mutations).
      bool Tech_Avail();
      // Returns next upgrade to get. Also manages morph.
      void Tech_Begin(Unit building, const Unit_Inventory &ui);
};
