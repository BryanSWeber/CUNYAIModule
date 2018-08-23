#pragma once
#include <BWAPI.h>
#include "CUNYAIModule.h"
#include "Research_Inventory.h"
#include "Unit_Inventory.h"
#include "CobbDouglas.h"

using namespace std;
using namespace BWAPI;

class Player_Model {
    Unit_Inventory units_;
    Research_Inventory researches_;
    CobbDouglas  spending_model_;
    //Other player-based factoids that may be useful should eventually go here- fastest time to air, popular build items, etc.
};