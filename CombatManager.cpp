#pragma once

#include "Source\CUNYAIModule.h"
#include "Source\Unit_Inventory.h"
#include "CombatManager.h"
#include <bwem.h>

bool CombatManager::addAntiAir(const Unit & u)
{
    anti_air_squad_.addStored_Unit(u);
    anti_air_squad_.updateUnitInventorySummary();
}

bool CombatManager::addAntiGround(const Unit & u)
{
    anti_ground_squad_.addStored_Unit(u);
    anti_ground_squad_.updateUnitInventorySummary();
}

bool CombatManager::addUniversal(const Unit & u)
{
    universal_squad_.addStored_Unit(u);
    universal_squad_.updateUnitInventorySummary();
}

bool CombatManager::addLiablitity(const Unit & u)
{
    return false;
}

bool CombatManager::addScout(const Unit & u)
{
    return false;
}

