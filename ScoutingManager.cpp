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

    overlord_scout_(nullptr),
    zergling_scout_(nullptr),
    expo_zergling_scout_(nullptr),
    last_overlord_scout_(nullptr),
    last_zergling_scout_(nullptr),
    last_expo_scout_(nullptr),

    scout_start_positions_(NULL),
    scout_expo_positions_(NULL)
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
            for (auto pos : inv.start_positions_)
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
            if (scout_expo_positions_.empty()) {

                int total_distance = 0;

                // Doesn't feel optimal, likely better off using probabilities but was stuck on the implementation
                // For each possible base, get the distance from enemy and add to total distance
                for (auto pos : inv.expo_positions_complete_) {
                    int base_distance = inv.getRadialDistanceOutFromEnemy(Position(pos));
                    total_distance += base_distance;
                    scout_expo_distances_.push_back(base_distance);
                }

                int mean_base_distance = total_distance / size(scout_expo_distances_);

                // For each base distance, if that distance is smaller than the avg then that base is a more likely enemy base
                for (auto dis : scout_expo_distances_) {
                    if (dis < mean_base_distance) {
                        // Reassign the base distances to actual expo positions
                        for (auto pos : inv.expo_positions_complete_) {
                            if (dis == inv.getRadialDistanceOutFromEnemy(Position(pos))) {
                                scout_expo_positions_.push_back(Position(pos));
                                break; //If we found the match, we are finished
                            }
                        }
                    }
                }
            }

            else {
                scout_spot = scout_expo_positions_[0];
                scout_expo_positions_.erase(scout_expo_positions_.begin());  // Scout goes to unique location each time
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
        // If a suicidal zergling scout doesn't exists and it's been 30s since death
        if (!exists_zergling_scout_ && last_zergling_scout_sent_ < t_game - Broodwar->getLatencyFrames() - 30 * 24)
            return true;
    }

    // Make sure we have an overlord
    if (u_type == UnitTypes::Zerg_Overlord) {
        // Initial overlord scout
        if (t_game < 3)
            return true;
        // If an overlord scout doesn't exist and it's been 60s since death
        if (!exists_overlord_scout_ && last_overlord_scout_sent_ < t_game - Broodwar->getLatencyFrames() - 60 * 24)
            return true;
    }

    // Don't need a scout
    return false;
}

void ScoutingManager::updateScouts() {
// Check if scouts have died

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

    // Clear overlords
    if (u_type == UnitTypes::Zerg_Overlord) {
        last_overlord_scout_ = unit; // Keep track of the cleared scout if still exists, not used for anything yet
        overlord_scout_ = nullptr;
        exists_overlord_scout_ = false;
        last_overlord_scout_sent_ = Broodwar->getFrameCount(); // Store timer of dead scout
        //Broodwar->sendText("Overlord scout cleared");
    }

    // Clear zerglings
    if (u_type == UnitTypes::Zerg_Zergling) {
        // if suicide scout, clearing lets them engage in combat logic
        if (zergling_scout_ == unit) {
            last_zergling_scout_ = unit;   // Keep track of the cleared scout if still exists, not used for anything yet
            zergling_scout_ = nullptr;
            exists_zergling_scout_ = false;
            last_zergling_scout_sent_ = Broodwar->getFrameCount(); // Store timer of dead scout
            //Broodwar->sendText("Ling scout cleared");
        }
        // if expo scout
        if (expo_zergling_scout_ == unit) {
            last_expo_scout_ = unit;   // Keep track of the cleared scout if still exists, not used for anything yet
            expo_zergling_scout_ = nullptr;
            exists_expo_zergling_scout_ = false;
            //Broodwar->sendText("Expo ling scout cleared");
        }
    }
}

bool ScoutingManager::isScoutingUnit(const Unit &unit) const {
// Check if a particular unit is a designated scout
    if (overlord_scout_ == unit || zergling_scout_ == unit || expo_zergling_scout_ == unit) 
        return true;

    return false;
}

void ScoutingManager::sendScout(const Unit &unit, const Position &scout_spot) const{
// Move the scout to the assigned scouting location
    unit->move(scout_spot);
}
