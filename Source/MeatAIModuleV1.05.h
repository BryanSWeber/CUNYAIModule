#pragma once
#include <BWAPI.h>
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
  double alpha_tech; 
    bool tech_starved; // for tech levels.

    // diagnostics
    bool diagnostic_mode; 
   //These measure its clock.
    int short_delay;
    int med_delay;
    int long_delay;
    //Clock App.
    int* short_delay_ptr = &short_delay;
    int* med_delay_ptr = &med_delay;
    int* long_delay_ptr = &long_delay;

    double ln_y; // for utility/worker
    double ln_Y; // for overall utility

  double gamma; // for supply levels.  Supply is an inhibition on growth rather than a resource to spend.  Cost of growth.
    bool supply_starved;
  double delta; // for gas levels. Gas is critical for spending but will be matched with supply.
    bool gas_starved;

  int t_build;

// Personally made functions:

  //Checks if a building can be built, and passes additional boolean criteria.  If all critera are passed, then it builds the building and delays the building timer 25 frames, or ~1 sec.
  void Check_N_Build(BWAPI::UnitType building, BWAPI::Unit unit , bool extra_critera);
  //Checks if an upgrade can be built, and passes additional boolean criteria.  If all critera are passed, then it performs the upgrade. Requires extra critera.
  void Check_N_Upgrade(BWAPI::UpgradeType ups, BWAPI::Unit unit, bool extra_critera);
  //Forces a unit to stutter in a brownian manner. Size of stutter is unit's (vision range * n ). Will attack if it sees something.
  void Brownian_Stutter(BWAPI::Unit unit , int n);
  // Prints unit's last error directly onto it.
  void PrintError_Unit(BWAPI::Unit unit );
  // An improvement on existing idle scripts. Checks if it is carrying, or otherwise busy. If it is stopped, it assumes it is not busy.
  bool isIdleEmpty(BWAPI::Unit unit );
  // evaluates the value of a stock of buildings, in terms of total cost (min+gas). Assumes building is zerg and therefore, a drone was spent on it.
  int Stock_Buildings(BWAPI::UnitType building);
  // evaluates the value of a stock of upgrades, in terms of total cost (min+gas).
  int Stock_Ups(BWAPI::UpgradeType ups);
  // evaluates the value of a stock of units, in terms of total cost (min+gas). Doesn't consider the counterfactual larva.
  int Stock_Units(BWAPI::UnitType unit);
  // evaluates the value of a stock of unit, in terms of supply added.
  int Stock_Supply(BWAPI::UnitType unit);
  // Checks if it is a fighting unit. UAB actually missed some logic in here, this is very different.
  bool IsFightingUnit( BWAPI::Unit unit );
  // Tells the unit to fight. If it can attack both air and ground.
  void MeatAIModule::Combat_Logic( BWAPI::Unit unit, BWAPI::Color color );

  //UAB-Based Commands:
  // Counts the tally of a particular unit. Includes those in production. Modification of UAB.
  int Count_Units(BWAPI::UnitType type );

  // Draws a line if diagnostic mode is TRUE.
  void MeatAIModule::Diagnostic_Line( BWAPI::Position s_pos, BWAPI::Position f_pos, BWAPI::Color col, bool diag_mode );
};
