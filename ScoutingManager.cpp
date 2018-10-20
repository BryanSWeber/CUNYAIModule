#pragma once

#include "Source\ScoutingManager.h"

ScoutingManager::ScoutingManager()
	: last_overlord_scout_sent_(0),  // Using member initalizer list instead
	  last_zergling_scout_sent_(0),
	  let_overlords_scout_(true),
	  exists_overlord_scout_(false),
	  exists_zergling_scout_(false),
	  overlord_scout_(nullptr),
	  zergling_scout_(nullptr),
	  last_overlord_scout_(nullptr),
	  last_zergling_scout_(nullptr)
{
}

// -- Work in progress -- Only scouting starting base locations currently
Position ScoutingManager::getScoutTargets(const Unit &unit, Map_Inventory &inv, Unit_Inventory &ei) {
// Scouting priorities
	Position scout_spot;
	//Position e_base_scout = inv.getMeanEnemyBuildingLocation(ei);
	//Broodwar->sendText("%s"), e_base_scout;
	if (!inv.start_positions_.empty() && find(inv.start_positions_.begin(), inv.start_positions_.end(), unit->getLastCommand().getTargetPosition()) == inv.start_positions_.end()) {
		scout_spot = inv.start_positions_[0];
	}
	return scout_spot;
}

bool ScoutingManager::needScout(const Unit &unit, const int &t_game) {
// When do we want to scout
	UnitType u_type = unit->getType();
	if (t_game < 5 && let_overlords_scout_) // first 5 frames for a buffer. Doing (t_game == 0) sometimes skips sending initial overloard
		return true;

	// need zergling scout whenever we have zerglings, a scout doesn't exist already, and one hasn't been killed semi-recently
	if ( u_type == UnitTypes::Zerg_Zergling && !exists_zergling_scout_ && (last_zergling_scout_sent_ < t_game - Broodwar->getLatencyFrames() - 30 * 24) ) 
		return true;

	if (let_overlords_scout_ && !exists_overlord_scout_ && last_overlord_scout_sent_ < t_game - Broodwar->getLatencyFrames() - 30 * 24) 
		return true;
	
	return false;
}

void ScoutingManager::updateScouts() {
// Check if scouts have died
	if (exists_zergling_scout_ && !zergling_scout_->exists()) {  //if we thought we had a scout but now we don't
		zergling_scout_ = nullptr;
		exists_zergling_scout_ = false;
		Broodwar->sendText("Zergling scout died");
	}
	if (exists_overlord_scout_ && !overlord_scout_->exists()) {  //if we thought we had a scout but now we don't
		overlord_scout_ = nullptr;
		exists_overlord_scout_ = false;
		Broodwar->sendText("Overlord scout died");
	}
}

void ScoutingManager::setScout(const Unit &unit) {
// Store unit as a designated scout
	UnitType u_type = unit->getType();

	if (u_type == UnitTypes::Zerg_Overlord) {
		overlord_scout_ = unit;
		exists_overlord_scout_ = true;
		last_overlord_scout_sent_ = Broodwar->getFrameCount(); // Store timer of dead scout
		Broodwar->sendText("Setting an overlord scout");
		return;
	}

	if (u_type == UnitTypes::Zerg_Zergling) {
		zergling_scout_ = unit;
		exists_zergling_scout_ = true;
		last_zergling_scout_sent_ = Broodwar->getFrameCount(); // Store timer of dead scout
		Broodwar->sendText("Setting a zergling scout");
		return;
	}
}

void ScoutingManager::clearScout(const Unit &unit) {
// Clear the unit from scouting duty
	UnitType u_type = unit->getType();

	if (u_type == UnitTypes::Zerg_Overlord && isScoutingUnit(unit) && overlord_scout_->exists()) {
		last_overlord_scout_ = unit; // Keep track of the cleared scout if still exists
		overlord_scout_ = nullptr;
		exists_overlord_scout_ = false;
		last_overlord_scout_sent_ = Broodwar->getFrameCount(); // Store timer of dead scout
		Broodwar->sendText("Scout cleared");
	}

	if (u_type == UnitTypes::Zerg_Zergling && isScoutingUnit(unit) && zergling_scout_->exists()) {
		last_zergling_scout_ = unit;   // Keep track of the cleared scout if still exists
		zergling_scout_ = nullptr;
		exists_zergling_scout_ = false;
		last_zergling_scout_sent_ = Broodwar->getFrameCount(); // Store timer of dead scout
		Broodwar->sendText("Scout cleared");
	}
}

bool ScoutingManager::isScoutingUnit(const Unit &unit) {
// Check if a particular unit is a designated scout
	if (overlord_scout_ == unit || zergling_scout_ == unit) 
		return true;

	return false;
}

// -- Work in progress --
void ScoutingManager::sendScout(const Unit &unit, const Position &scout_spot) {
// Move the scout to the assigned scouting location
	unit->move(scout_spot);
	Broodwar->sendText("Scout Sent");
}
