#pragma once

#include "Source\CUNYAIModule.h"
#include "Source\Map_Inventory.h"
#include "Source\AssemblyManager.h"
#include "Source\Unit_Inventory.h"
#include "Source\MobilityManager.h"
#include "Source/Diagnostics.h"
#include "Source\FAP\FAP\include\FAP.hpp" // could add to include path but this is more explicit.
#include "Source\PlayerModelManager.h" // needed for cartidges.
#include "Source\BWEB\BWEB.h"
#include <bwem.h>
#include <iterator>
#include <numeric>
#include <fstream>

class Base {
public:
    Base(const Unit &u);
    bool gas_taken_;
    bool gas_tolerable_;
    bool air_weak_;
    bool ground_weak_;
    int spore_count_;
    int sunken_count_;
    int creep_count_;
    int gas_gatherers_;
    int mineral_gatherers_;
    Unit unit_;
};

class BaseManager {
private:
    void updateBases();
public:
    map<Position, Base> baseMap_;
};
