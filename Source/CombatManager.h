#pragma once

#include "CUNYAIModule.h"
#include "Unit_Inventory.h"
#include <bwem.h>

class CombatManager {
private:
    //Squad only attacks up.
    Unit_Inventory anti_air_squad_;
    //Squad only attacks down.
    Unit_Inventory anti_ground_squad_;
    //Squad attacks either.
    Unit_Inventory universal_squad_;
    //Squad only gets hurt. Overlords, low energy psions...
    static Unit_Inventory liabilities_squad_;
    //Squad scouts
    static Unit_Inventory scout_squad_;
    //Squad detects
    Unit_Inventory detector_squad_;
    // Some conditions for advancing units in or back.
    static bool ready_to_fight;

public:
    //bool identifyTargets();
    //bool identifyWeaknesses();

    // Runs all movement script for nonworker units. Priortizes combat when needed and pathing otherwise.
    bool grandStrategyScript(const Unit & u);

    //Subroutines:
    // Runs basic combat script, and if there is no combat, it returns false.
    static bool combatScript(const Unit &u);
    // Runs a basic scouting script, primarily for overlords but valid for non-overlords.
    static bool scoutScript(const Unit &u);
    static bool liabilitiesScript(const Unit &u);
    // Runs a basic pathing script. Home if concerned, out if safe.
    static bool pathingScript(const Unit &u);

    //Grouping for inventories, mostly unused.
    //Adds to appropriate inventory. May move differently.
    bool addAntiAir(const Unit &u);
    //Adds to appropriate inventory. May move differently.
    bool addAntiGround(const Unit &u);
    //Adds to appropriate inventory. May move differently.
    bool addUniversal(const Unit &u);
    //Adds to appropriate inventory. May move differently.
    bool addLiablitity(const Unit &u);
    //Adds to appropriate inventory. May move differently.
    static bool addScout(const Unit &u);
    //Removes from appropriate inventory. May move differently.
    static void removeScout(const Unit & u);
    //Checks if a particular unit is stored in scout.
    static bool isScout(const Unit &u);
    //Checks if a particular unit is stored in liablities.
    static bool isLiability(const Unit & u);
    //Removes from appropriate inventory. May move differently.
    static void removeLiablity(const Unit & u);

    // Returns True if all enemy units are workers. 
    static bool isWorkerFight(const Unit_Inventory &friendly, const Unit_Inventory &enemy);
    // Returns True if all enemy units are Workers or Buildings
    static bool isPulledWorkersFight(const Unit_Inventory &friendly, const Unit_Inventory &enemy);

    // Updating advance/retreat conditions:
    void updateReadiness();
};