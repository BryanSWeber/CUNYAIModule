#pragma once

#include "Source\CUNYAIModule.h"
#include "Source\Unit_Inventory.h"
#include <bwem.h>

class CombatManager {
    private:
        Unit_Inventory anti_air_squad_;
        Unit_Inventory anti_ground_squad_;
        Unit_Inventory universal_squad_;
        Unit_Inventory liabilities_squad_;
        Unit_Inventory scout_squad_;
    public:
        bool identifyTargets();
        bool identifyWeaknesses();
        bool addAntiAir(const Unit &u);
        bool addAntiGround(const Unit &u);
        bool addUniversal(const Unit &u);
        bool addLiablitity(const Unit &u);
        bool addScout(const Unit &u);
};