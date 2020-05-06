#pragma once
#include <BWAPI.h>
#include "Source\CUNYAIModule.h"


//This class will choose the appropriate unit to build (building or unit).
//It will then find an appropriate position for that building or unit.
//It will then order a unit to create that building.
class Strategy {

private:

    BuildingGene nextBuild_;

public:

    static BuildOrderObject determineNext(); //Take game state, determine next build order object 
    static bool Check_N_Build(const UnitType & building, const Unit & unit, const bool & extra_critera, const TilePosition &tp = TilePositions::Origin);  // Checks if a building can be built, and passes additional boolean criteria.  If all critera are passed, then it puts the worker into the pre-build phase with intent to build the building. Trys to build near tileposition TP or the unit if blank.
    static bool Check_N_Grow(const UnitType & unittype, const Unit & larva, const bool & extra_critera);  // Check and grow a unit using larva.

   
}

//Choose upgrades, or buildings.
//Should be able to skip ahead when appropriate to pre-move workers.