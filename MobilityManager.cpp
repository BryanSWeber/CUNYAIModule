#pragma once

# include "Source\Diagnostics.h"
# include "Source\MobilityManager.h"
# include "Source/UnitInventory.h"
# include <random> // C++ base random is low quality.
# include <numeric>
# include <math.h>

//#define TOO_FAR_FROM_FRONT (CUNYAIModule::current_MapInventory.getRadialDistanceOutFromEnemy(pos_) > (CUNYAIModule::friendly_player_model.closest_ground_combatant_ + 3.0 * 0.125 * distance_metric_ )); //radial distance is in minitiles, distance is in pixels.
//#define DISTANCE_METRIC (2.760 * 24.0);

using namespace BWAPI;
using namespace Filter;
using namespace std;


bool Mobility::simplePathing(const Position &e_pos, const StoredUnit::Phase phase, const bool caution) {

    if (caution && CUNYAIModule::currentMapInventory.isInSurroundField(TilePosition(pos_)))
        return moveAndUpdate(getSurroundingPosition(pos_), phase); // If you are already in a surround field and need to move somewhere cautiously, you should stay in your surround field.
    if (caution && CUNYAIModule::currentMapInventory.isTileThreatened(TilePosition(e_pos)))
        return moveAndUpdate(getSurroundingPosition(e_pos), phase); // getting position one turn closer is for short linear movements - will stop mutas and lings from leaping over threatened areas.
    else
        return moveAndUpdate(e_pos, phase);

}

bool Mobility::BWEM_Movement(const bool &forward_movement) {
    // Units should head towards enemies when there is a large gap in our knowledge, OR when it's time to pick a fight.
    if (forward_movement) {
        if (CUNYAIModule::combat_manager.isScout(unit_)) {
            int scouts = CUNYAIModule::combat_manager.scoutPosition(unit_);
            return moveTo(pos_, CUNYAIModule::currentMapInventory.getScoutingBases().at(scouts), StoredUnit::Phase::PathingOut, true);
        }
        else if (u_type_.airWeapon() == WeaponTypes::None && u_type_.groundWeapon() != WeaponTypes::None) { // if you can't help air go ground.
            return moveTo(pos_, CUNYAIModule::currentMapInventory.getEnemyBaseGround(), StoredUnit::Phase::PathingOut, true);
        }
        else if (u_type_.airWeapon() != WeaponTypes::None && u_type_.groundWeapon() == WeaponTypes::None) { // if you can't help ground go air.
            return moveTo(pos_, CUNYAIModule::currentMapInventory.getEnemyBaseAir(), StoredUnit::Phase::PathingOut, true);
        }
        else if (u_type_.groundWeapon() != WeaponTypes::None && u_type_.airWeapon() != WeaponTypes::None) { // otherwise go to whichever type has an active problem..
            if (CUNYAIModule::friendly_player_model.u_have_active_air_problem_) {
                return moveTo(pos_, CUNYAIModule::currentMapInventory.getEnemyBaseAir(), StoredUnit::Phase::PathingOut, true);
            }
            else {
                return moveTo(pos_, CUNYAIModule::currentMapInventory.getEnemyBaseGround(), StoredUnit::Phase::PathingOut, true);
            }
        }
    }

    // Otherwise, return to home.
    return moveTo(pos_, CUNYAIModule::currentMapInventory.getFrontLineBase(), StoredUnit::Phase::PathingHome, true);

}

bool Mobility::surroundLogic()
{
    return moveTo(pos_, getSurroundingPosition(pos_), StoredUnit::Phase::Surrounding);
}

//Position Mobility::getVectorAwayFromNeighbors()
//{
//    UnitInventory u_loc = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, pos_, distance_metric_);
//
//    Position central_pos = Positions::Origin;
//    for (auto u : u_loc.unit_map_) {
//        central_pos += u.second.pos_ - pos_;
//    }
//
//    Position vector_away = Positions::Origin - (central_pos - pos_);
//    double theta = atan2(vector_away.y, vector_away.x); // we want to go away from them.
//    return Position(static_cast<int>(cos(theta) * 64), static_cast<int>(sin(theta) * 64)); // either {x,y}->{-y,x} or {x,y}->{y,-x} to rotate
//}

// This is basic combat logic for nonspellcasting units.
bool Mobility::Tactical_Logic(UnitInventory &ei, const UnitInventory &ui, const int &passed_distance, const Color &color = Colors::White)
{
    //vector<int> useful_stocks = CUNYAIModule::getUsefulStocks(ui, ei);
    Unit last_target = unit_->getLastCommand().getTarget();
    Position targets_pos;

    int widest_dim = max(u_type_.height(), u_type_.width());
    int priority = 0;

    //auto path = BWEM::Map::Instance().GetPath(pos_, e_unit.pos_); // maybe useful later.
    int helpful_u = ui.stock_fighting_total_;
    int helpful_e = ei.stock_fighting_total_;
    int max_dist_no_priority = INT_MAX;
    //int max_dist = passed_distance; // copy, to be modified later.
    bool weak_enemy_or_small_armies = (helpful_e < helpful_u || helpful_e < 500 || ei.worker_count_ == static_cast<int>(ei.unit_map_.size()));
    bool target_sentinel = false;
    bool target_sentinel_poor_target_atk = false;
    bool suicide_unit = stored_unit_->type_ == UnitTypes::Zerg_Scourge || stored_unit_->type_ == UnitTypes::Zerg_Infested_Terran;
    bool melee = CUNYAIModule::getFunctionalRange(unit_) == 32;
    double limit_units_diving = weak_enemy_or_small_armies ? (FAP_SIM_DURATION / 12) : (FAP_SIM_DURATION / 12) * log(helpful_e - helpful_u); // should be relatively stable if I reduce the duration.

    // Let us bin all potentially interesting units.
    UnitInventory DiveableTargets;
    UnitInventory ThreateningTargets;
    UnitInventory SecondOrderThreats;
    UnitInventory LowPriority;

    for (auto e = ei.unit_map_.begin(); e != ei.unit_map_.end() && !ei.unit_map_.empty(); ++e) {
        if (e->first && e->first->isDetected()) { // only target observable units.
            UnitType e_type = e->second.type_;
            //bool can_continue_to_surround = !melee || (melee && e->second.circumference_remaining_ > widest_dim * 0.75);
            if (!suicide_unit || (suicide_unit && e->second.stock_value_ >= stored_unit_->stock_value_ && CUNYAIModule::isFightingUnit(e->second.type_))) { // do not suicide into units less valuable than you are.
                bool critical_target = e_type.groundWeapon().innerSplashRadius() > 0 ||
                    (e_type.isSpellcaster() && !e_type.isBuilding()) ||
                    (e_type.isDetector() && ui.cloaker_count_ >= ei.detector_count_) ||
                    e_type == UnitTypes::Protoss_Carrier ||
                    (e->second.bwapi_unit_ && e->second.bwapi_unit_->exists() && e->second.bwapi_unit_->isRepairing()) ||
                    e_type == UnitTypes::Protoss_Reaver; // Prioritise these guys: Splash, crippled combat units

                if ((e_type.isWorker() || critical_target) && CUNYAIModule::canContributeToFight(e_type, ui)) {
                    DiveableTargets.addStoredUnit(e->second);
                }

                if (CUNYAIModule::checkCanFight(e_type, unit_)) {
                    ThreateningTargets.addStoredUnit(e->second);
                }

                if (CUNYAIModule::canContributeToFight(e_type, ui) || e_type.spaceProvided() > 0) {
                    SecondOrderThreats.addStoredUnit(e->second);
                }

                if (CUNYAIModule::hasPriority(e->second) || e_type == UnitTypes::Protoss_Interceptor) { // don't target larva, eggs, or trivia, but you must shoot interceptors eventually.
                    LowPriority.addStoredUnit(e->second);
                }
            }
        }
    }

    double dist_to_enemy = passed_distance;
    double target_surviablity = INT_MAX;
    Unit target = nullptr;

    ThreateningTargets.unit_map_.insert(DiveableTargets.unit_map_.begin(), DiveableTargets.unit_map_.end());
    SecondOrderThreats.unit_map_.insert(ThreateningTargets.unit_map_.begin(), ThreateningTargets.unit_map_.end());
    LowPriority.unit_map_.insert(SecondOrderThreats.unit_map_.begin(), SecondOrderThreats.unit_map_.end());

    DiveableTargets.updateUnitInventorySummary();
    ThreateningTargets.updateUnitInventorySummary();
    SecondOrderThreats.updateUnitInventorySummary();
    LowPriority.updateUnitInventorySummary();

    // Dive some modest distance if they're critical to kill.
    int temp_max_divable = static_cast<int>(max(static_cast<double>(CUNYAIModule::getChargableDistance(unit_)) / static_cast<double>(limit_units_diving), static_cast<double>(CUNYAIModule::getFunctionalRange(unit_))));
    if (!target) { // repeated calls should be functionalized.
        target = pickTarget(temp_max_divable, DiveableTargets);
    }

    // Shoot closest threat if they can shoot you or vis versa.
    temp_max_divable = max(ei.max_range_, CUNYAIModule::getFunctionalRange(unit_)) + CUNYAIModule::convertTileDistanceToPixelDistance(3);
    if (!target) { // repeated calls should be functionalized.
        target = pickTarget(temp_max_divable, ThreateningTargets);
    }

    // If they are threatening something, feel free to dive some distance to them, but not too far as to trigger another fight.
    temp_max_divable = max( ei.max_range_, CUNYAIModule::getFunctionalRange(unit_)) + CUNYAIModule::convertTileDistanceToPixelDistance(3);
    if (!target) { // repeated calls should be functionalized.
        target = pickTarget(temp_max_divable, SecondOrderThreats);
    }

    temp_max_divable = INT_MAX;
    if (!target) { // repeated calls should be functionalized.
        target = pickTarget(temp_max_divable, LowPriority);
    }

    if (target && !DISABLE_ATTACKING) {
        if (!prepareLurkerToAttack(target->getPosition())) {// adjust lurker if neccesary, otherwise attack.
            if (target->exists())
                unit_->attack(target);
            else
                unit_->attack(pos_ + getVectorFromUnitToEnemyDestination(target) + getVectorFromUnitToBeyondEnemy(target));
        }
        Diagnostics::drawLine(pos_, target->getPosition(), color);
        return CUNYAIModule::updateUnitPhase(unit_, StoredUnit::Phase::Attacking);
    }

    return false; // no target, we got a falsehood.
}

//Essentially, we would like to call the movement script BUT disable any attraction to the enemy since we are trying to only surround.
//void Mobility::Surrounding_Movement(const Unit & unit, const UnitInventory & ui, UnitInventory & ei, const MapInventory & inv){
//}

// Basic retreat logic
bool Mobility::Retreat_Logic() {

    //Position next_waypoint = getNextWaypoint(pos_, CUNYAIModule::currentMapInventory.getSafeBase());

    if (CUNYAIModule::currentMapInventory.isTileThreatened(TilePosition(pos_))) {
        return moveTo(pos_, getSaferPositionNear(pos_), StoredUnit::Phase::Retreating); //Let's just get out of threat!
    }
    else if (stored_unit_->shoots_down_ || stored_unit_->shoots_up_) {
        return moveTo(pos_, CUNYAIModule::currentMapInventory.getFrontLineBase(), StoredUnit::Phase::Retreating);
    }
    else if (CUNYAIModule::combat_manager.isScout(unit_)) {
        return moveTo(pos_, CUNYAIModule::currentMapInventory.getSafeBase(), StoredUnit::Phase::Retreating);
    }
    else {
        return moveTo(pos_, CUNYAIModule::currentMapInventory.getSafeBase(), StoredUnit::Phase::Retreating);
    }

}

bool Mobility::Scatter_Logic(const Position pos)
{
    Position problem_pos = Positions::Origin;

    if (pos == Positions::Origin) {
        if (unit_->isUnderStorm()) {
            double current_distance = 999999;
            for (auto s : Broodwar->getBullets()) {
                if (s->getType() == BulletTypes::Psionic_Storm  && s->getPosition().getDistance(pos_) < current_distance) {
                    problem_pos = s->getPosition();
                    current_distance = s->getPosition().getDistance(pos_);
                }
            }
        }
        if (unit_->isUnderDisruptionWeb()) {
            double current_distance = 999999;
            for (auto s : Broodwar->getAllUnits()) {
                if (s->getType() == UnitTypes::Spell_Disruption_Web  && s->getPosition().getDistance(pos_) < current_distance) {
                    problem_pos = s->getPosition();
                    current_distance = s->getPosition().getDistance(pos_);
                }
            }
        }

        if (unit_->isIrradiated()) {
            problem_pos = unit_->getClosestUnit()->getPosition();
        }
    }
    else {
        problem_pos = pos;
    }

    if (unit_->move(pos_ - getVectorApproachingPosition(problem_pos)))  // Move away from the problem position as soon as possible.
        return CUNYAIModule::updateUnitPhase(unit_, StoredUnit::Phase::Retreating);
    else
        return false;
}


Position Mobility::getSurroundingPosition(const Position p) {
    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<double> dis(0, 1);    // default values for output.

    TilePosition bestTile = TilePosition(p); //default action is to do nothing.
    TilePosition myTile = TilePosition(pos_);
    TilePosition centerOfSpiral = TilePosition(p);

    //Don't move if you're in the buffer around their threatRadus and you're the only one on your tile.
    // only about 1/n the time should you filter out, noting that each unit is . Otherwise all units will filter out. Scheduling is hard.
    bool slowExit = CUNYAIModule::currentMapInventory.getOccupationField(myTile) <= 2 ? false : dis(gen) < static_cast<double>(CUNYAIModule::currentMapInventory.getOccupationField(myTile) - 1.0) / static_cast<double>(CUNYAIModule::currentMapInventory.getOccupationField(myTile));
    if(CUNYAIModule::currentMapInventory.isInBufferField(myTile) && !slowExit)
        return getCenterOfTile(bestTile);

    SpiralOut spiral;
    int n = 15; // how far in one direction should we search for a tile?
    for (int i = 0; i <= pow(2 * n, 2); i++) {
        //Consider the tile of interest
        spiral.goNext();
        int x = centerOfSpiral.x + spiral.x;
        int y = centerOfSpiral.y + spiral.y;
        TilePosition target_tile = TilePosition(x, y);
        if (!target_tile.isValid()) {
            continue;
        }

        //If it's a better tile, switch to it. Exit upon finding a good one.
        if (CUNYAIModule::currentMapInventory.isInSurroundField(target_tile) && isMoreOpen(target_tile)) {
            bestTile = target_tile;
            // The first time this event occurs will be the closest tile, roughly. There may be some sub-tile differentiation.
            return getCenterOfTile(bestTile); // shift to surround, move to the center of the tile and not to the corners or something strange.
        }

    }

    // Fallback- sometimes the target position is surrounded by threatening forces for 15 tiles or more. Then we just have to get to the safest position?
    if (bestTile == TilePosition(p)) {
        return getSaferPositionNear(p);
    }

    return getCenterOfTile(bestTile);
}

Position Mobility::getPositionThatLetsUsAttack(const Position p, const double proportion)
{
    double theta = atan2(p.y - pos_.y, p.x - pos_.x); // get angle between two.
    double maxRange = max(stored_unit_->type_.groundWeapon().maxRange(), stored_unit_->type_.airWeapon().maxRange());
    Position closest_loc_to_permit_attacking = Position(static_cast<int>(cos(theta) * proportion * maxRange ), static_cast<int>(sin(theta) * proportion * maxRange)); // Get a vector at that angle of size a little shorter than our range.
    return getCenterOfTile(p - closest_loc_to_permit_attacking);
}


Position Mobility::getSaferPositionNear(const Position p) {

    TilePosition centerSpiral = TilePosition(p);
    TilePosition bestTile = TilePosition(p);
    double base_threat = std::ceil(CUNYAIModule::currentMapInventory.getTileThreat(bestTile) * 100.0) / 100.0; // round threat to nearest 2 decimal places, since you do not want to be attracted to machine rounding errors of 0.
    int baseDist = INT_MAX;

    SpiralOut spiral;
    int n = 15; // how far in one direction should we search for a tile?
    for (int i = 0; i <= pow(2 * n, 2); i++) {
        //Consider the tile of interest
        spiral.goNext();
        int x = centerSpiral.x + spiral.x;
        int y = centerSpiral.y + spiral.y;
        TilePosition target_tile = TilePosition(x, y);
        if (!target_tile.isValid()) {
            continue;
        }

        double new_threat = std::ceil(CUNYAIModule::currentMapInventory.getTileThreat(target_tile) * 100.0) / 100.0; // round threat to nearest 2 decimal places, since you do not want to be attracted to machine rounding errors of 0.


        //If it's a better tile, move there at the end of this.
        if (new_threat <= base_threat && isMoreOpen(target_tile)) {
            int newDist = target_tile.getDistance(TilePosition(p)); //Don't calculate distances you don't have to, but if they're equal and not better don't switch.

            if (new_threat == base_threat) {
                if (newDist < baseDist) {
                    bestTile = target_tile;
                    base_threat = new_threat;
                    baseDist = newDist;
                }
            }

            if (new_threat < base_threat) {
                bestTile = target_tile;
                base_threat = new_threat;
                baseDist = newDist;
            }
        }
    }

    return getCenterOfTile(bestTile); // The first time this event occurs will be the closest tile, roughly. There may be some sub-tile differentiation.
}



Position Mobility::getVectorAwayFromEdges() {

    // numerous tiles to check.
    WalkPosition main = WalkPosition(pos_);
    BWEM::MiniTile main_mini = BWEM::Map::Instance().GetMiniTile(main);
    WalkPosition alt = main;
    alt.x = main.x + 1;
    pair<BWEM::altitude_t, WalkPosition> up = {BWEM::Map::Instance().GetMiniTile(alt).Altitude(), alt};
    alt = main;
    alt.x = main.x - 1;
    pair<BWEM::altitude_t, WalkPosition> down = { BWEM::Map::Instance().GetMiniTile(alt).Altitude(), alt };
    alt = main;
    alt.y = main.y + 1;
    pair<BWEM::altitude_t, WalkPosition> left = { BWEM::Map::Instance().GetMiniTile(alt).Altitude(), alt };
    alt = main;
    alt.y = main.y - 1;
    pair<BWEM::altitude_t, WalkPosition> right = { BWEM::Map::Instance().GetMiniTile(alt).Altitude(), alt };

    vector<pair<BWEM::altitude_t, WalkPosition>> higher_ground;
    for (auto i : { up, down, left, right }) {
        if (i.first >= main_mini.Altitude())
            higher_ground.push_back(i);
    }

    if (higher_ground.empty()) {
        Diagnostics::DiagnosticWrite("No higher ground?");
        return Positions::Origin;
    }
    else {
        pair<BWEM::altitude_t, WalkPosition> targeted_pair = *CUNYAIModule::select_randomly(higher_ground.begin(), higher_ground.end());
        Position vector_to = Position(targeted_pair.second) - pos_;
        double theta = atan2(vector_to.y, vector_to.x);
        return Position(static_cast<int>(cos(theta) * distance_metric_), static_cast<int>(sin(theta) * distance_metric_)); // either {x,y}->{-y,x} or {x,y}->{y,-x} to rotate
    }
}

Position Mobility::getVectorApproachingPosition(const Position & p) {
    Position vector_to = p - pos_;
    double theta = atan2(vector_to.y, vector_to.x);
    int shorter_order = min(p.getDistance(pos_), distance_metric_);
    Position approach_vector = Position(static_cast<int>(cos(theta) * shorter_order), static_cast<int>(sin(theta) * shorter_order)); // either {x,y}->{-y,x} or {x,y}->{y,-x} to rotate

    return approach_vector; // only one direction for now.
}


bool Mobility::checkSafeEscapePath(const Position &finish) {

    int plength = 0;
    bool unit_sent = false;
    auto cpp = BWEM::Map::Instance().GetPath(pos_, finish, &plength);
    bool threat_found = true;
    if (!cpp.empty()) { // if there's an actual path to follow...
        for (auto choke_point : cpp) {
            BWEM::Area area = *choke_point->GetAreas().first;
                if (area.Data()) return false;
            BWEM::Area area2 = *choke_point->GetAreas().second;
                if (area2.Data()) return false;
        }
        return true;
    }
    if (plength) {
        return true;
    }
    return false;
}

bool Mobility::checkSafeGroundPath(const Position &finish) {
    int plength = 0;
    bool unit_sent = false;
    auto cpp = BWEM::Map::Instance().GetPath(pos_, finish, &plength);
    UnitInventory ei_temp;
    if (!Mobility::checkSafeEscapePath(finish)) return false;

    if (plength) {
        BWEM::Area area = *BWEM::Map::Instance().GetArea(TilePosition(finish));
        if (area.Data()) return false;
        return true;
    }
    return false;
}

// returns TRUE if the lurker needed fixing. For Attack.
bool Mobility::prepareLurkerToAttack(const Position position_of_target) {
    int dist_to_threat_or_target = unit_->getDistance(position_of_target);
    bool dist_condition = dist_to_threat_or_target < UnitTypes::Zerg_Lurker.groundWeapon().maxRange() + (UnitTypes::Zerg_Lurker.dimensionRight() + UnitTypes::Zerg_Lurker.dimensionUp())/2;

    if (u_type_ == UnitTypes::Zerg_Lurker) {
        if (!unit_->isBurrowed() && dist_condition) { // If you're within range, burrow.
            return unit_->burrow();
        }
        else if (unit_->isBurrowed() && !dist_condition) {  // If you're burrowed and out of range, get up.
            return unit_->unburrow();
        }
        else if (!unit_->isBurrowed() && !dist_condition) { //If you're not burrowed and out of range, get into range. But where?
            return moveTo(pos_, getPositionThatLetsUsAttack(position_of_target), StoredUnit::Phase::Attacking);
        }
    }

    return false;
}

bool Mobility::prepareLurkerToMove() {

    if (u_type_ == UnitTypes::Zerg_Lurker) {
        bool conditionsToStay = (CUNYAIModule::currentMapInventory.isTileThreatened(TilePosition(pos_)) && !CUNYAIModule::currentMapInventory.getDetectField(TilePosition(pos_))) || unit_->isIrradiated();
        bool conditionsToRun = unit_->isUnderStorm();
        if (unit_->isBurrowed()) {
            if(conditionsToRun)
                unit_->unburrow();
            else if(conditionsToStay)
                return false;
            else
                unit_->unburrow();
            return true;
        }
    }

    return false;
}

//Position Mobility::getVectorTowardsField(const vector<vector<int>> &field) const {
//    Position return_vector = Positions::Origin;
//    int my_spot = CUNYAIModule::currentMapInventory.getFieldValue(pos_, field);
//    int temp_x = 0;
//    int temp_y = 0;
//    int current_best = INT_MAX;
//    double theta = 0;
//
//    SpiralOut spiral; // don't really need to spiral out here anymore
//
//    // we need to spiral out from the center, stopping if we hit an object.
//    TilePosition map_dim = TilePosition({ Broodwar->mapWidth(), Broodwar->mapHeight() });
//    for (int i = 0; i <= 64; i++) {
//        spiral.goNext();
//        int centralize_x = TilePosition(pos_).x + spiral.x;
//        int centralize_y = TilePosition(pos_).y + spiral.y;
//
//        if (centralize_x < map_dim.x &&
//            centralize_y < map_dim.y &&
//            centralize_x > 0 &&
//            centralize_y > 0
//            ) // Is the spot acceptable?
//        {
//            if (field[centralize_x][centralize_y] > my_spot) {
//                temp_x += spiral.x;
//                temp_y += spiral.y;
//            }
//        }
//    }
//
//    if (temp_y != 0 || temp_x != 0) {
//        theta = atan2(temp_y, temp_x);
//        return_vector = Position(static_cast<int>(cos(theta) * distance_metric_), static_cast<int>(sin(theta) * distance_metric_)); //vector * scalar. Note if you try to return just the unit vector, Position will truncate the doubles to ints, and you'll get 0,0.
//    }
//    return  return_vector;
//}
//
//Position Mobility::getVectorAwayField(const vector<vector<int>> &field) const {
//    Position return_vector = Positions::Origin;
//    int my_spot = CUNYAIModule::currentMapInventory.getFieldValue(pos_, field);
//    int temp_x = 0;
//    int temp_y = 0;
//    int current_best = INT_MAX;
//    double theta = 0;
//
//    SpiralOut spiral; // don't really need to spiral out here anymore
//
//                      // we need to spiral out from the center, stopping if we hit an object.
//    TilePosition map_dim = TilePosition({ Broodwar->mapWidth(), Broodwar->mapHeight() });
//    for (int i = 0; i <= 9; i++) {
//        spiral.goNext();
//        int centralize_x = TilePosition(pos_).x + spiral.x;
//        int centralize_y = TilePosition(pos_).y + spiral.y;
//
//        if (centralize_x < map_dim.x &&
//            centralize_y < map_dim.y &&
//            centralize_x > 0 &&
//            centralize_y > 0
//            ) // Is the spot acceptable?
//        {
//            if (field[centralize_x][centralize_y] < my_spot) {
//                temp_x += spiral.x;
//                temp_y += spiral.y;
//            }
//        }
//    }
//
//    if (temp_y != 0 || temp_x != 0) {
//        theta = atan2(temp_y, temp_x);
//        return_vector = Position(static_cast<int>(cos(theta) * distance_metric_), static_cast<int>(sin(theta) * distance_metric_)); //vector * scalar. Note if you try to return just the unit vector, Position will truncate the doubles to ints, and you'll get 0,0.
//    }
//    return  return_vector;
//}

bool Mobility::moveTo(const Position &start, const Position &finish, const StoredUnit::Phase phase, const bool caution)
{

    if (!start.isValid() || !finish.isValid()) {
        Diagnostics::DiagnosticText("We're trying to go somewhere that doesn't exist.");
        return false;
    }

    if (!unit_->isFlying()) {
        //First, let us try to get there with JPS.
        BWEB::Path newPath;
        newPath.createUnitPath(start, finish);
        if (newPath.isReachable() && !newPath.getTiles().empty() && newPath.getDistance() > 0) {
            int i = 0;
            TilePosition tileOfInterest = TilePosition(newPath.getTiles()[0]);
            Position spotOfInterest = getCenterOfTile(tileOfInterest);
            while (newPath.getTiles().size() > 1 && i < newPath.getTiles().size()) { //If you're not travling far, go to the next path. Otherwise, if you're within 5 tiles of your destination, go to the next one.
                bool too_close = start.getDistance(getCenterOfTile(newPath.getTiles()[i])) < 32 * 5;
                if (too_close && i < newPath.getTiles().size())
                    i++;
                else {
                    tileOfInterest = newPath.getTiles()[i];
                    spotOfInterest = getCenterOfTile(tileOfInterest);
                    if (caution && CUNYAIModule::currentMapInventory.isTileThreatened(tileOfInterest))
                        return moveAndUpdate(getSurroundingPosition(spotOfInterest), phase);
                    else
                        return moveAndUpdate(spotOfInterest, phase);
                }
            }
            return moveAndUpdate(spotOfInterest, phase); //We have a move. Update the phase and move along.
        }
        else { // There is no path? Perhaps we are trying to move to an enemy building. Then let us try CPP.
            int plength = 0;
            auto cpp = BWEM::Map::Instance().GetPath(start, finish, &plength);
            if (!cpp.empty() && plength > 0) {
                int i = 0;
                TilePosition tileOfInterest = TilePosition(cpp[0]->Center());
                Position spotOfInterest = getCenterOfTile(tileOfInterest);
                while (i < cpp.size()) { //If you're not travling far, go to the next path. Otherwise, if you're within 5 tiles of your destination, go to the next one.
                    bool too_close = spotOfInterest.getDistance(unit_->getPosition()) < 32 * 5;
                    if (too_close && i < cpp.size() - 1)
                        i++;
                    else {
                        tileOfInterest = TilePosition(cpp[i]->Center());
                        spotOfInterest = getCenterOfTile(tileOfInterest);
                        if (caution && CUNYAIModule::currentMapInventory.isTileThreatened(tileOfInterest))
                            return moveAndUpdate(getSurroundingPosition(spotOfInterest), phase);
                        else
                            return moveAndUpdate(spotOfInterest, phase);
                    }
                }
                return moveAndUpdate(spotOfInterest, phase);
            }
        }
    }
    return simplePathing(finish, phase, caution);
}

bool Mobility::isTileApproachable(const TilePosition tp) {
    bool same_height = Broodwar->getGroundHeight(TilePosition(pos_)) == Broodwar->getGroundHeight(tp);
    bool closer_to_home = CUNYAIModule::currentMapInventory.getRadialDistanceOutFromHome(pos_) < CUNYAIModule::currentMapInventory.getRadialDistanceOutFromHome(getCenterOfTile(tp));
    bool isVisible = Broodwar->isVisible(tp); //allows flying units to retreat anywhere... unless they're blind.
    return same_height || isVisible || closer_to_home;
}

Unit Mobility::pickTarget(int MaxDiveDistance, UnitInventory & ui) {
    Unit target = nullptr;
    int dist_to_enemy = 0;
    int target_surviablity = INT_MAX;
    for (auto t : ui.unit_map_) {
        dist_to_enemy = unit_->getDistance(t.second.pos_);
        bool baseline_requirement = (isTileApproachable(TilePosition(t.second.pos_)) || stored_unit_->is_flying_) && CUNYAIModule::checkCanFight(u_type_, t.second.type_);
        if (t.second.future_fap_value_ <= target_surviablity && dist_to_enemy <= MaxDiveDistance && baseline_requirement) {
            MaxDiveDistance = dist_to_enemy;
            target_surviablity = t.second.future_fap_value_;  //attack most likely to die, not closest!
            target = t.first;
        }
    }
    return target;
}

bool Mobility::moveAndUpdate(const Position p, const StoredUnit::Phase phase)
{
    if (CUNYAIModule::currentMapInventory.getTileThreat(TilePosition(p)))
        cout << "Why are we moving into a threat?" << endl;
    // lurkers should prepare to move if sent a move command, then we evaluate again later.
    if (u_type_ == UnitTypes::Zerg_Lurker && prepareLurkerToMove())
        return CUNYAIModule::updateUnitPhase(unit_, phase);

    if (unit_->move(p)) {
        Diagnostics::drawLine(pos_, p, Colors::White);//Run towards it
        if(!unit_->isFlying()) CUNYAIModule::currentMapInventory.setSurroundField(TilePosition(p), false); // update the tile as "full"
        return CUNYAIModule::updateUnitPhase(unit_, phase); //We have a move. Update the phase and move along.
    }
    else
        return false;
}

bool Mobility::checkGoingDifferentDirections(Unit e) {
    return !checkAngleSimilar(e->getAngle() , unit_->getAngle());
}

bool Mobility::checkEnemyApproachingUs(Unit e) {
    Position vector_to_me = pos_ - e->getPosition();
    double angle_to_me = atan2(vector_to_me.y, vector_to_me.x);
    return checkAngleSimilar(e->getAngle(), angle_to_me);
}

bool Mobility::checkEnemyApproachingUs(StoredUnit & e) {
    Position vector_to_me = pos_ - e.pos_;
    double angle_to_me = atan2(vector_to_me.y, vector_to_me.x);
    return checkAngleSimilar(e.angle_, angle_to_me);
}

bool Mobility::isMoreOpen(TilePosition & tp)
{
    bool is_more_open = false;
    // It is only more open if it is occupied by less than 2 small units or one large unit. Large units will consider anything partially occupied by a small unit (occupied 1) as occupied.
    if (unit_->getType().size() == UnitSizeTypes::Small)
        is_more_open = ((CUNYAIModule::currentMapInventory.getOccupationField(tp) < CUNYAIModule::currentMapInventory.getOccupationField(unit_->getTilePosition()) || CUNYAIModule::currentMapInventory.getOccupationField(tp) == 0) && BWEB::Map::isWalkable(tp)) || unit_->getType().isFlyer();
    else
        is_more_open = ((CUNYAIModule::currentMapInventory.getOccupationField(tp) < CUNYAIModule::currentMapInventory.getOccupationField(unit_->getTilePosition()) - 1 || CUNYAIModule::currentMapInventory.getOccupationField(tp) == 0) && BWEB::Map::isWalkable(tp)) || unit_->getType().isFlyer(); //Do not transfer unless it is better by at least 2 or more, Reasoning: if you have 1 med & 1 small, it does not pay to transfer.

    return is_more_open;
}

double getEnemySpeed(Unit e) {
    return sqrt(pow(e->getVelocityX(),2) + pow(e->getVelocityY(),2));
}

Position getEnemyVector(Unit e) {
    return Position(static_cast<int>(e->getVelocityX()), static_cast<int>(e->getVelocityY()));
}


Position Mobility::getVectorFromUnitToDestination(Position & p)
{
    return p - pos_;
}

Position Mobility::getVectorFromUnitToEnemyDestination(Unit e) {
    Position his_destination = e->getPosition() + getEnemyVector(e);
    return his_destination - pos_;
}

Position Mobility::getVectorFromUnitToBeyondEnemy(Unit e) {
    Position p = getVectorFromUnitToEnemyDestination(e);
    double angle_to_enemy = atan2(p.y, p.x);
    return Position(static_cast<int>(cos(angle_to_enemy) * (e->getType().width() + e->getType().height() + 32) ), static_cast<int>(sin(angle_to_enemy) *  (e->getType().width() + e->getType().height() + 32) ));
}

bool checkSameDirection(const Position vector_a, const Position vector_b) {
    double angle_to_a = atan2(vector_a.y, vector_a.x);
    double angle_to_b = atan2(vector_b.y, vector_b.x);
    return checkAngleSimilar(angle_to_a, angle_to_b);
}

bool checkAngleSimilar(double angle1, double angle2) {
    double diff = min({ abs(angle1 - angle2), abs(angle1 - angle2 - 2 * 3.1415), abs(angle1 - angle2 + 2 * 3.1415) } );
    return diff < 0.50 * 3.1415;
}

Position getCenterOfTile(const TilePosition tpos)
{
    return Position(tpos) + Position(16, 16);
}

Position getCenterOfTile(const Position pos)
{
    return getCenterOfTile(TilePosition(pos));
}
