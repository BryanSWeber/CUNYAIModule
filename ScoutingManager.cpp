#pragma once

#include "Source\ScoutingManager.h"

ScoutingManager::ScoutingManager() {
	int _last_overlord_scout_sent(0);
	int _last_zergling_scout_sent(0);
	bool _exists_zergling_scout(false);
	bool _exists_overlord_scout(false);
	bool _let_overlords_scout(true);
	Unit _overlord_scout(nullptr);
	Unit _zergling_scout(nullptr);
	Unit _last_overlord_scout(nullptr);
	Unit _last_zergling_scout(nullptr);
}

// -- Work in progress -- Only scouting starting base locations currently
Position ScoutingManager::getScoutTargets(const Unit &unit, Map_Inventory &inv, Unit_Inventory &ei) {
// Scouting priorities
	Position scout_spot;
	if (!inv.start_positions_.empty() && find(inv.start_positions_.begin(), inv.start_positions_.end(), unit->getLastCommand().getTargetPosition()) == inv.start_positions_.end()) {
		scout_spot = inv.start_positions_[0];
	}
	return scout_spot;
}

bool ScoutingManager::needScout(const Unit &unit, const int &t_game) {
// When do we want to scout
	UnitType u_type = unit->getType();
	if (t_game < 5) return true;  // first 5 frames for a buffer. Doing (t_game == 0) sometimes skips sending initial overloard

	// need zergling scout whenever we have zerglings, a scout doesn't exist already, and one hasn't been killed semi-recently
	if ( u_type == UnitTypes::Zerg_Zergling && !_exists_zergling_scout && (_last_zergling_scout_sent < t_game - Broodwar->getLatencyFrames() - 30 * 24) ) return true;

	if (_let_overlords_scout && !_exists_overlord_scout && _last_overlord_scout_sent < t_game - Broodwar->getLatencyFrames() - 30 * 24) return true;
	return false;
}

void ScoutingManager::updateScouts() {
// Check if scouts have died
	if (_exists_zergling_scout && !_zergling_scout->exists()) {  //if we thought we had a scout but now we don't
		_zergling_scout = nullptr;
		_exists_zergling_scout = false;
		Broodwar->sendText("Zergling scout died");
	}
	if (_exists_overlord_scout && !_overlord_scout->exists()) {  //if we thought we had a scout but now we don't
		_overlord_scout = nullptr;
		_exists_overlord_scout = false;
		Broodwar->sendText("Overlord scout died");
	}
}

void ScoutingManager::setScout(const Unit &unit) {
// Store unit as a designated scout
	UnitType u_type = unit->getType();

	if (u_type == UnitTypes::Zerg_Overlord && _let_overlords_scout) {
		_overlord_scout = unit;
		_exists_overlord_scout = true;
		_last_overlord_scout_sent = Broodwar->getFrameCount(); // Store timer of dead scout
		Broodwar->sendText("Setting an overlord scout");
		return;
	}

	if (u_type == UnitTypes::Zerg_Zergling) {
		_zergling_scout = unit;
		_exists_zergling_scout = true;
		_last_zergling_scout_sent = Broodwar->getFrameCount(); // Store timer of dead scout
		Broodwar->sendText("Setting a zergling scout");
		return;
	}
}

void ScoutingManager::clearScout(const Unit &unit) {
// Clear the unit from scouting duty
	UnitType u_type = unit->getType();

	if (u_type == UnitTypes::Zerg_Overlord && isScoutingUnit(unit) && _overlord_scout->exists()) {
		_last_overlord_scout = unit; // Keep track of the cleared scout if still exists
		_overlord_scout = nullptr;
		_exists_overlord_scout = false;
		_last_overlord_scout_sent = Broodwar->getFrameCount(); // Store timer of dead scout
		Broodwar->sendText("Scout cleared");
	}

	if (u_type == UnitTypes::Zerg_Zergling && isScoutingUnit(unit) && _zergling_scout->exists()) {
		_last_zergling_scout = unit;   // Keep track of the cleared scout if still exists
		_zergling_scout = nullptr;
		_exists_zergling_scout = false;
		_last_zergling_scout_sent = Broodwar->getFrameCount(); // Store timer of dead scout
		Broodwar->sendText("Scout cleared");
	}
}

bool ScoutingManager::isScoutingUnit(const Unit &unit) {
// Check if a particular unit is a designated scout
	if (_overlord_scout == unit || _zergling_scout == unit) return true;

	return false;
}

// -- Work in progress --
void ScoutingManager::sendScout(const Unit &unit, const Position &scout_spot) {
// Move the scout to the assigned scouting location
	unit->move(scout_spot);
	Broodwar->sendText("Scout Sent");
}
