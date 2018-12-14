#pragma once

#include "Source\ScoutingManager.h"

ScoutingManager::ScoutingManager()
    : last_overlord_scout_sent_(0),
    last_zergling_scout_sent_(0),

    initial_scouts_(true),
    let_overlords_scout_(true),
    exists_overlord_scout_(false),
    exists_zergling_scout_(false),
    exists_expo_zergling_scout_(false),
    found_enemy_base_(false),
    force_zergling_(false),

    overlord_scout_(nullptr),
    zergling_scout_(nullptr),
    expo_zergling_scout_(nullptr),
    last_overlord_scout_(nullptr),
    last_zergling_scout_(nullptr),
    last_expo_scout_(nullptr),

    scout_start_positions_(NULL)
{
}

Position ScoutingManager::getScoutTargets(const Unit &unit, Map_Inventory &inv, Unit_Inventory &ei) {
    // Scouting priorities
    Position scout_spot;
    Position e_base_scout = Positions::Origin;

    // Get the mean enemy base location if we have found the enemy base
    if (inv.getMeanEnemyBuildingLocation(ei) != Positions::Origin) {
        Stored_Unit* nearest_building = CUNYAIModule::getClosestGroundStored(ei, inv.getMeanEnemyBuildingLocation(ei), inv);
        if (nearest_building != nullptr) {
            e_base_scout = nearest_building->pos_;
            found_enemy_base_ = true;
        }
    }

    // If we haven't found any enemy buildings yet
    if (!found_enemy_base_) {

        // Build our scout start positions when empty, should only build once
        if (scout_start_positions_.empty()) {
            for (const auto& pos : inv.start_positions_)
                scout_start_positions_.push_back(pos);
        }

        scout_spot = scout_start_positions_[0];
        scout_start_positions_.erase(scout_start_positions_.begin());  // Scouts go to unique starting locations
        return scout_spot;
    }

    // Found an enemy building
    else {

        // Suicide zergling or an overlord scout
        if (zergling_scout_ == unit || overlord_scout_ == unit) {
            scout_spot = e_base_scout;   // Path into their base
            return scout_spot;
        }

        // Check for expos/hidden stuff scout
        else if (expo_zergling_scout_ == unit) {

            // Build our scout expo positions when empty, will rebuild when empties
            if (scout_expo_map_.empty()) {

                int total_distance = 0;

                // Create a map <int, Position> of all base locations on map
                for (const auto& pos : inv.expo_positions_complete_) {
                    int base_distance = inv.getRadialDistanceOutFromEnemy(Position(pos));
                    total_distance += base_distance;
                    scout_expo_map_[base_distance] = Position(pos);
                }

                int mean_base_distance = total_distance / (scout_expo_map_.size() - 1);

                // Remove unwanted scout locations
                for (auto itr = scout_expo_map_.begin(); itr != scout_expo_map_.end(); ++itr) {

                    if (scout_expo_map_.size() > 1) {

                        // Erase bases that our scout can't walk to
                        if (!Broodwar->isWalkable(WalkPosition(itr->second))) {
                            itr = scout_expo_map_.erase(itr);
                            continue;
                        }
                        // Erase bases that are already visible
                        if (Broodwar->isVisible(TilePosition(itr->second))) {
                            itr = scout_expo_map_.erase(itr);
                            continue;
                        }
                        // Erase bases that are less than the mean distance -- doesn't work as intended on all maps
                        if (itr->first > mean_base_distance) {
                            itr = scout_expo_map_.erase(itr);
                            continue;
                        }

                        // Erase starting bases, suicide ling is for that
                        for (auto start_pos : inv.start_positions_complete_) {
                            if (itr->second == start_pos) {
                                itr = scout_expo_map_.erase(itr);
                                break;
                            }
                        }
                    }
                    // Break loop if only 1 base left to scout
                    else
                        break;
                }
            }

            // Assign scout locations
            for (auto itr = scout_expo_map_.begin(); itr != scout_expo_map_.end(); ++itr) {
                scout_spot = itr->second;
                itr = scout_expo_map_.erase(itr); //Erase the used base location, each scout goes to unique location
                return scout_spot;
            }
        }
    }
    return scout_spot;
}

bool ScoutingManager::needScout(const Unit &unit, const int &t_game) const {
// When do we want to scout
    UnitType u_type = unit->getType();

    // Make sure we have a zergling
    if (u_type == UnitTypes::Zerg_Zergling) {
        // Always have an expo scout
        if (!exists_expo_zergling_scout_)
            return true;
        // If a suicidal zergling scout doesn't exists and it's been 30s since death/cleared
        if (!exists_zergling_scout_ && last_zergling_scout_sent_ < t_game - Broodwar->getLatencyFrames() - 30 * 24)
            return true;
    }

    // Make sure we have an overlord
    if (u_type == UnitTypes::Zerg_Overlord) {
        // Initial overlord scout
        if (t_game < 3)
            return true;
        // If an overlord scout doesn't exist and it's been 30s since death/cleared
        if (!exists_overlord_scout_ && last_overlord_scout_sent_ < t_game - Broodwar->getLatencyFrames() - 30 * 24)
            return true;
    }

    // Don't need a scout
    return false;
}

void ScoutingManager::updateScouts(const Player_Model& enemy_player_model, const Player_Model& friendly_player_model) {
// Check if scouts have died and update overlord scouting

    // If enemy has units that can shoot overlords, stop overlord scouting
    if ((enemy_player_model.enemy_race_ == Races::Terran || (enemy_player_model.units_.stock_shoots_up_ || enemy_player_model.units_.stock_both_up_and_down_)) && Broodwar->self()->getUpgradeLevel(UpgradeTypes::Pneumatized_Carapace) == 0)
        let_overlords_scout_ = false;
    // Turn overlord scouting back on if we have overlord speed upgrade
    if (Broodwar->self()->getUpgradeLevel(UpgradeTypes::Pneumatized_Carapace) == 1)
        let_overlords_scout_ = true;

    if (Broodwar->canMake(UnitTypes::Zerg_Zergling) && CUNYAIModule::Count_Units(UnitTypes::Zerg_Zergling) <= 1 && CUNYAIModule::Count_Units_In_Progress(UnitTypes::Zerg_Zergling, friendly_player_model.units_) == 0) {
        Broodwar->sendText("building zergling");
        force_zergling_ = true;
    }

    //if we thought we had a suicide zergling scout but now we don't
    if (zergling_scout_) {
        if (!zergling_scout_->exists()) {
            zergling_scout_ = nullptr;
            exists_zergling_scout_ = false;
            last_zergling_scout_sent_ = Broodwar->getFrameCount();
            Broodwar->sendText("Zergling scout died");
        }
    }

    //if we thought we had an expo zergling scout but now we don't
    if (expo_zergling_scout_) {
        if (!expo_zergling_scout_->exists()) {
            expo_zergling_scout_ = nullptr;
            exists_expo_zergling_scout_ = false;
            Broodwar->sendText("Expo ling scout died");
        }
    }

    //if we thought we had an overlord scout but now we don't
    if (overlord_scout_) {
        if (!overlord_scout_->exists()) { 
            overlord_scout_ = nullptr;
            exists_overlord_scout_ = false;
            last_overlord_scout_sent_ = Broodwar->getFrameCount();
            Broodwar->sendText("Overlord scout died");
        }
    }
}

void ScoutingManager::setScout(const Unit &unit, const int &ling_type) {
// Store unit as a designated scout
// ling_type = 1 is expo scout, ling_type = 2 is suicide base scout
    UnitType u_type = unit->getType();
    Stored_Unit& scout_unit = CUNYAIModule::friendly_player_model.units_.unit_inventory_.find(unit)->second;
    scout_unit.updateStoredUnit(unit);
    // Set overlord as a scout
    if (u_type == UnitTypes::Zerg_Overlord) {
        overlord_scout_ = unit;
        exists_overlord_scout_ = true;
        scout_unit.phase_ = "Scouting";
        Broodwar->sendText("Overlord scout sent");
        return;
    }

    // Set zergling as a scout
    if (u_type == UnitTypes::Zerg_Zergling) {
        // expo scout
        if (ling_type == 1) {
            expo_zergling_scout_ = unit;
            exists_expo_zergling_scout_ = true;
            scout_unit.phase_ = "Scouting";
            Broodwar->sendText("Expo ling scout sent");
            return;
        }
        // suicide scout
        if (ling_type == 2) {
            zergling_scout_ = unit;
            exists_zergling_scout_ = true;
            scout_unit.phase_ = "Scouting";
            Broodwar->sendText("Suicide ling scout sent");
            return;
        }
    }

}

void ScoutingManager::clearScout(const Unit &unit) {
// Clear the unit from scouting duty
    UnitType u_type = unit->getType();
    Stored_Unit& scout_unit = CUNYAIModule::friendly_player_model.units_.unit_inventory_.find(unit)->second;

    // Clear overlords
    if (u_type == UnitTypes::Zerg_Overlord) {
        last_overlord_scout_ = unit; // Keep track of the cleared scout if still exists, not used for anything yet
        overlord_scout_ = nullptr;
        exists_overlord_scout_ = false;
        last_overlord_scout_sent_ = Broodwar->getFrameCount(); // Store timer of dead scout
        scout_unit.phase_ = "None";
    }

    // Clear zerglings
    if (u_type == UnitTypes::Zerg_Zergling) {
        // if suicide scout, clearing lets them engage in combat logic
        if (zergling_scout_ == unit) {
            last_zergling_scout_ = unit;   // Keep track of the cleared scout if still exists, not used for anything yet
            zergling_scout_ = nullptr;
            exists_zergling_scout_ = false;
            last_zergling_scout_sent_ = Broodwar->getFrameCount(); // Store timer of dead scout
            scout_unit.phase_ = "None";
        }
        // if expo scout
        if (expo_zergling_scout_ == unit) {
            last_expo_scout_ = unit;   // Keep track of the cleared scout if still exists, not used for anything yet
            expo_zergling_scout_ = nullptr;
            exists_expo_zergling_scout_ = false;
            scout_unit.phase_ = "None";
        }
    }
}

bool ScoutingManager::isScoutingUnit(const Unit &unit) const {
// Check if a particular unit is a designated scout
    if (overlord_scout_ == unit || zergling_scout_ == unit || expo_zergling_scout_ == unit) 
        return true;

    return false;
}

void ScoutingManager::sendScout(const Unit &unit, const Position &scout_spot) const {
// Move the scout to the assigned scouting location
    unit->move(scout_spot);
}

void ScoutingManager::diagnosticLine(const Unit &unit, const Map_Inventory &inv) const {
    if (unit == zergling_scout_ || unit == overlord_scout_) {
        Position pos = unit->getPosition();
        Position scout_spot = unit->getTargetPosition();
        CUNYAIModule::Diagnostic_Line(pos, scout_spot, inv.screen_position_, Colors::Purple);
    }
    if (unit == expo_zergling_scout_) {
        Position pos = unit->getPosition();
        Position scout_spot = unit->getTargetPosition();
        CUNYAIModule::Diagnostic_Line(pos, scout_spot, inv.screen_position_, Colors::Yellow);
    }
}

