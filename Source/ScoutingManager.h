#pragma once

#include <BWAPI.h>
#include "CUNYAIModule.h"

// Work in progress -- Missing some major features / bug fixes
struct ScoutingManager {
	int last_overlord_scout_sent_;
	int last_zergling_scout_sent_;
	bool let_overlords_scout_;
	bool exists_overlord_scout_;
	bool exists_zergling_scout_;
	Unit overlord_scout_;
	Unit zergling_scout_;
	Unit last_overlord_scout_;
	Unit last_zergling_scout_;
	
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