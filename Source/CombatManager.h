#pragma once

#include "CUNYAIModule.h"
#include "UnitInventory.h"
#include <bwem.h>

class CombatManager {
private:
    UnitInventory anti_air_squad_;     //Squad only attacks up.
    UnitInventory anti_ground_squad_;     //Squad only attacks down.
    UnitInventory universal_squad_;     //Squad attacks either.
    static UnitInventory liabilities_squad_;     //Squad only gets hurt. Overlords, low energy psions...
    static UnitInventory scout_squad_;     //Squad scouts
    UnitInventory detector_squad_;     //Squad detects
    static bool ready_to_fight;     // Some conditions for advancing units in or back.

public:
    bool grandStrategyScript(const Unit & u);     // Runs all movement script for nonworker units. Priortizes combat when needed and pathing otherwise.

    static bool combatScript(const Unit &u);   // Runs basic combat script, and if there is no combat, it returns false.
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

    static bool isWorkerFight(const UnitInventory &friendly, const UnitInventory &enemy);      // Returns True if all enemy units are workers. 
    //static bool isPulledWorkersFight(const UnitInventory &friendly, const UnitInventory &enemy);      // Returns True if all enemy units are Workers or Buildings

    void updateReadiness();  // Updating advance/retreat conditions:
};