#pragma once

#include "Source\CUNYAIModule.h"
#include "Source\Unit_Inventory.h"
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
        Unit_Inventory liabilities_squad_;
        //Squad scouts
        static Unit_Inventory scout_squad_;
        //Squad detects
        Unit_Inventory detector_squad_;

    public:
        //bool identifyTargets();
        //bool identifyWeaknesses();
        // Runs basic combat script, and if there is no combat, it defaults to the movement manager.
        static bool combatScript(const Unit &u);
        // Runs a basic scouting script, primarily for overlords but valid for non-overlords.
        static bool scoutScript(const Unit &u);
        //Adds to appropriate inventory. May move differently.
        bool addAntiAir(const Unit &u);
        //Adds to appropriate inventory. May move differently.
        bool addAntiGround(const Unit &u);
        //Adds to appropriate inventory. May move differently.
        bool addUniversal(const Unit &u);
        //Adds to appropriate inventory. May move differently.
        bool addLiablitity(const Unit &u);
        //Adds to appropriate inventory. May move differently.
        bool addScout(const Unit &u);
        //Removes from appropriate inventory. May move differently.
        void removeScout(const Unit & u);
        // Some conditions for advancing units in or back.
        static bool ready_to_fight;
        // Updating those conditions:
        void updateReadiness();
};