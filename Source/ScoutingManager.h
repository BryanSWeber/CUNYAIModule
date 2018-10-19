#pragma once

#include <BWAPI.h>
#include "CUNYAIModule.h"

// Work in progress -- Missing some major features / bug fixes
struct ScoutingManager {
	int _last_overlord_scout_sent;
	int _last_zergling_scout_sent;
	bool _let_overlords_scout;
	bool _exists_overlord_scout;
	bool _exists_zergling_scout;
	Unit _overlord_scout;
	Unit _zergling_scout;
	Unit _last_overlord_scout;
	Unit _last_zergling_scout;
	
	// Initalizer
	ScoutingManager();

	// Checks if unit is our scouting unit
	Position getScoutTargets(const Unit &unit, Map_Inventory &inv, Unit_Inventory &ei);
	void updateScouts();
	bool needScout(const Unit &unit, const int &t_game);
	void setScout(const Unit &unit);
	void clearScout(const Unit &unit);
	bool isScoutingUnit(const Unit &unit);
	void sendScout(const Unit &unit, const Position &scout_spot);
};