#pragma once

#include "CUNYAIModule.h"
#include "UnitInventory.h"
#include <bwem.h>
#include "FAP/FAP/include/FAP.hpp"
#include "CombatSimulator.h"

class CombatSimulator;

class CombatManager {
private:

    UnitInventory anti_air_squad_;     //Squad only attacks up.
    UnitInventory anti_ground_squad_;     //Squad only attacks down.
    UnitInventory universal_squad_;     //Squad attacks either.
    static UnitInventory liabilities_squad_;     //Squad only gets hurt. Overlords, low energy psions...
    static UnitInventory scout_squad_;     //Squad scouts
    UnitInventory detector_squad_;     //Squad detects
    static bool getMacroCombatReadiness();  // Evaluate advance/retreat conditions based on macro conditions.

public:
    bool grandStrategyScript(const Unit & u);     // Runs all movement script for nonworker units. Priortizes combat when needed and pathing otherwise.

    bool combatScript(const Unit &u);   // Runs basic combat script, and if there is no combat, it returns false.
    static bool scoutScript(const Unit &u);       // Runs a basic scouting script, primarily for overlords but valid for non-overlords.
    static int scoutCount(); //  How many scouts do we have?
    static int scoutPosition(const Unit & u); // is this the 1st scout or second, etc?  Will return an out of bounds value if it is not in there.
    static bool liabilitiesScript(const Unit &u); // Runs liability units towards static defence.
    static bool pathingScript(const Unit &u);      // Runs a basic pathing script. Home if concerned, out if safe.
    static bool checkNeedMoreWorkersToHold(const UnitInventory & friendly, const UnitInventory & enemy); // Returns true if we are using enough workers to defend some rush.

    //Grouping for inventories, mostly unused.
    bool addAntiAir(const Unit &u);      //Adds to appropriate inventory. May move differently.
    bool addAntiGround(const Unit &u);      //Adds to appropriate inventory. May move differently.
    bool addUniversal(const Unit &u);   //Adds to appropriate inventory. May move differently.
    bool addLiablitity(const Unit &u);   //Adds to appropriate inventory. May move differently.
    static bool addScout(const Unit &u);      //Adds to appropriate inventory. May move differently.
    static void removeScout(const Unit & u);     //Removes from appropriate inventory. May move differently.
    static bool isScout(const Unit &u);      //Checks if a particular unit is stored in scout.
    static bool isLiability(const Unit & u);      //Checks if a particular unit is stored in liablities.
    static void removeLiablity(const Unit & u);      //Removes from appropriate inventory. May move differently.
    static bool isCollectingForces(const UnitInventory &ui); //Checks if you're preparing to attack in a given UI. Tests the % of units pathing out.
    
    static bool isWorkerFight(const UnitInventory &friendly, const UnitInventory &enemy);      // Returns True if all enemy units are workers. 

    int getSearchRadius(const Unit &u); //Returns the number of pixels we should search for targets.

    void onFrame(); //Updates the combat sims so inferences can be drawn.
};
