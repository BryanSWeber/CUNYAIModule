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
        Unit_Inventory scout_squad_;
        //Squad detects
        Unit_Inventory detector_squad_;

    public:
        bool identifyTargets();
        bool identifyWeaknesses();
        bool addAntiAir(const Unit &u);
        bool addAntiGround(const Unit &u);
        bool addUniversal(const Unit &u);
        bool addLiablitity(const Unit &u);
        bool addScout(const Unit &u);
};