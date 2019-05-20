#pragma once

#include "Source\CUNYAIModule.h"
#include "Source\Unit_Inventory.h"
#include "CombatManager.h"
#include <bwem.h>

//bool CombatManager::identifyTargets()
//{
//    // Need to update map objects for every building!
//    bool unit_calculation_frame = Broodwar->getFrameCount() % Broodwar->getLatencyFrames() != 0;
//    int frames_this_cycle = Broodwar->getFrameCount() % (24 * 4); // technically more.
//
//                                                                  // every frame this is incremented.
//    frames_since_enemy_base_ground_++;
//    frames_since_enemy_base_air_++;
//    frames_since_home_base++;
//    frames_since_map_veins++;
//    frames_since_safe_base++;
//    frames_since_unwalkable++;
//
//    //every 10 sec check if we're sitting at our destination.
//    //if (Broodwar->isVisible(TilePosition(enemy_base_ground_)) && Broodwar->getFrameCount() % (24 * 5) == 0) {
//    //    fram = true;
//    //}
//    if (unit_calculation_frame) return;
//
//    //If we need updating (from building destruction or any other source) - begin the cautious chain of potential updates.
//    if (frames_since_unwalkable > 24 * 30) {
//
//        getExpoPositions();
//        updateUnwalkableWithBuildings();
//        frames_since_unwalkable = 0;
//        return;
//    }
//
//    if (frames_since_map_veins > 24 * 30) { // impose a second wait here because we don't want to update this if we're discovering buildings rapidly.
//
//        updateMapVeins();
//        frames_since_map_veins = 0;
//        return;
//    }
//
//    if (frames_since_enemy_base_ground_ > 24 * 10) {
//
//        Stored_Unit* center_army = CUNYAIModule::getClosestGroundStored(ei, ui.getMeanLocation()); // If the mean location is over water, nothing will be updated. Current problem: Will not update if no combat forces!
//        Stored_Unit* center_base = CUNYAIModule::getClosestStoredBuilding(ei, ui.getMeanLocation(), 999999); // 
//
//        if (center_army && center_army->pos_ && center_army->pos_ != Positions::Origin) { // Sometimes buildings get invalid positions. Unclear why. Then we need to use a more traditioanl method.
//            updateMapVeinsOut(center_army->pos_, enemy_base_ground_, map_out_from_enemy_ground_, false); // don't print this one, it could be anywhere and to print all of them would end up filling up our hard drive.
//        }
//        else if (center_base && center_base->pos_ && center_base->pos_ != Positions::Origin) { // Sometimes buildings get invalid positions. Unclear why. Then we need to use a more traditioanl method.
//            updateMapVeinsOut(center_base->pos_, enemy_base_ground_, map_out_from_enemy_ground_, false); // don't print this one, it could be anywhere and to print all of them would end up filling up our hard drive.
//        }
//        else if (!start_positions_.empty() && start_positions_[0] && start_positions_[0] != Positions::Origin && !cleared_all_start_positions_) { // maybe it's an starting base we havent' seen yet?
//            int attempts = 0;
//            while (Broodwar->isVisible(TilePosition(enemy_base_ground_)) && attempts < static_cast<int>(start_positions_.size()) && Broodwar->isVisible(TilePosition(start_positions_[0]))) {
//                std::rotate(start_positions_.begin(), start_positions_.begin() + 1, start_positions_.end());
//                attempts++;
//            }
//            updateMapVeinsOut(start_positions_[0] + Position(UnitTypes::Zerg_Hatchery.dimensionLeft(), UnitTypes::Zerg_Hatchery.dimensionUp()), enemy_base_ground_, map_out_from_enemy_ground_);
//        }
//        else if (!expo_positions_complete_.empty()) { // maybe it's a expansion we havent' seen yet?
//            expo_positions_ = expo_positions_complete_;
//
//            int attempts = 0;
//            while (Broodwar->isVisible(TilePosition(enemy_base_ground_)) && attempts < static_cast<int>(expo_positions_.size()) && (start_positions_.empty() || Broodwar->isVisible(TilePosition(start_positions_[0])))) {
//                std::rotate(expo_positions_.begin(), expo_positions_.begin() + 1, expo_positions_.end());
//                attempts++;
//            }
//
//            if (attempts >= static_cast<int>(expo_positions_.size())) {
//                int random_index = rand() % static_cast<int>(expo_positions_.size() - 1); // random enough for our purposes.
//                updateMapVeinsOut(Position(expo_positions_[random_index]) + Position(UnitTypes::Zerg_Hatchery.dimensionLeft(), UnitTypes::Zerg_Hatchery.dimensionUp()), enemy_base_ground_, map_out_from_enemy_ground_);
//            }
//            else { // you can see everything but they have no enemy forces, then let's go smash randomly.
//                updateMapVeinsOut(Position(expo_positions_[0]) + Position(UnitTypes::Zerg_Hatchery.dimensionLeft(), UnitTypes::Zerg_Hatchery.dimensionUp()), enemy_base_ground_, map_out_from_enemy_ground_);
//            }
//        }
//        frames_since_enemy_base_ground_ = 0;
//        return;
//    }
//
//    if (frames_since_enemy_base_air_ > 24 * 5) {
//
//        Stored_Unit* center_flyer = CUNYAIModule::getClosestAirStored(ei, ui.getMeanAirLocation()); // If the mean location is over water, nothing will be updated. Current problem: Will not update if on
//
//        if (ei.getMeanBuildingLocation() != Positions::Origin && center_flyer && center_flyer->pos_) { // Sometimes buildings get invalid positions. Unclear why. Then we need to use a more traditioanl method.
//            updateMapVeinsOut(center_flyer->pos_, enemy_base_air_, map_out_from_enemy_air_, false);
//        }
//        else {
//            enemy_base_air_ = enemy_base_ground_;
//            map_out_from_enemy_air_ = map_out_from_enemy_ground_;
//        }
//        frames_since_enemy_base_air_ = 0;
//        return;
//
//    }
//
//    if (frames_since_home_base > 24 * 10) {
//
//        //otherwise go to your weakest base.
//        Position suspected_friendly_base = Positions::Origin;
//
//        if (ei.stock_fighting_total_ > 0) {
//            suspected_friendly_base = getMostValuedBase(ui);
//        }
//
//        if (suspected_friendly_base.isValid() && suspected_friendly_base != home_base_ && suspected_friendly_base != Positions::Origin) {
//            //updateMapVeinsOut(suspected_friendly_base + Position(UnitTypes::Zerg_Hatchery.dimensionLeft(), UnitTypes::Zerg_Hatchery.dimensionUp()), home_base_, map_out_from_home_);
//        }
//        frames_since_home_base = 0;
//        return;
//    }
//
//    if (frames_since_safe_base > 24 * 10) {
//
//        //otherwise go to your safest base - the one with least deaths near it and most units.
//        Position suspected_safe_base = Positions::Origin;
//
//        suspected_safe_base = getNonCombatBase(ui, di); // If the mean location is over water, nothing will be updated. Current problem: Will not update if on building. Which we are trying to make it that way.
//
//        if (suspected_safe_base.isValid() && suspected_safe_base != safe_base_ && suspected_safe_base != Positions::Origin) {
//            //updateMapVeinsOut(suspected_safe_base + Position(UnitTypes::Zerg_Hatchery.dimensionLeft(), UnitTypes::Zerg_Hatchery.dimensionUp()), safe_base_, map_out_from_safety_);
//        }
//        else {
//            safe_base_ = home_base_;
//            //map_out_from_safety_ = map_out_from_home_;
//        }
//
//        frames_since_safe_base = 0;
//        return;
//    }
//}

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

