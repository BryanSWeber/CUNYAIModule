#pragma once

# include "Source\CUNYAIModule.h"
# include "Source\MobilityManager.h"
# include <random> // C++ base random is low quality.
# include <numeric>
# include <math.h>

#define TOO_FAR_FROM_FRONT (CUNYAIModule::current_map_inventory.getRadialDistanceOutFromEnemy(pos_) > (CUNYAIModule::friendly_player_model.closest_ground_combatant_ + 3.0 * 0.125 * distance_metric_ )); //radial distance is in minitiles, distance is in pixels.
//#define DISTANCE_METRIC (2.760 * 24.0);

using namespace BWAPI;
using namespace Filter;
using namespace std;


//Forces a unit to stutter in a Mobility manner. Size of stutter is unit's (vision range * n ). Will attack if it sees something.  Overlords & lings stop if they can see minerals.

bool Mobility::local_pathing(const int &passed_distance, const Position &e_pos) {

    // lurkers should move when we need them to scout.
    if (u_type_ == UnitTypes::Zerg_Lurker && unit_->isBurrowed() && !CUNYAIModule::getClosestThreatOrTargetStored(CUNYAIModule::enemy_player_model.units_, unit_, max(UnitTypes::Zerg_Lurker.groundWeapon().maxRange(), CUNYAIModule::enemy_player_model.units_.max_range_))) {
        unit_->unburrow();
        return CUNYAIModule::updateUnitPhase(unit_, Stored_Unit::Phase::PathingOut);
    }

    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<double> dis(0, 1);    // default values for output.

    if (pos_.getDistance(e_pos) > CUNYAIModule::enemy_player_model.units_.max_range_ + distance_metric_) {
        approach(e_pos);
        unit_->move(pos_ + attract_vector_);
        CUNYAIModule::Diagnostic_Line(pos_, pos_ + attract_vector_, CUNYAIModule::current_map_inventory.screen_position_, Colors::White);//Run towards it.
        CUNYAIModule::Diagnostic_Line(pos_, e_pos, CUNYAIModule::current_map_inventory.screen_position_, Colors::Red);//Run around 
    }
    else {
        encircle(e_pos);
        unit_->move(pos_ + encircle_vector_);
        CUNYAIModule::Diagnostic_Line(pos_, pos_ + encircle_vector_, CUNYAIModule::current_map_inventory.screen_position_, Colors::White);//Run around 
    }
    return CUNYAIModule::updateUnitPhase(unit_, Stored_Unit::Phase::Surrounding);
}

bool Mobility::BWEM_Movement(const int &in_or_out) {
    bool it_worked = false;

    // Units should head towards enemies when there is a large gap in our knowledge, OR when it's time to pick a fight.
    if (in_or_out > 0) {
        if (u_type_.airWeapon() != WeaponTypes::None) {
            it_worked = moveTo(pos_, CUNYAIModule::current_map_inventory.enemy_base_air_);
        }
        else {
            it_worked = moveTo(pos_, CUNYAIModule::current_map_inventory.enemy_base_ground_);
        }
    }
    else { // Otherwise, return to home.
        it_worked = moveTo(pos_, CUNYAIModule::current_map_inventory.home_base_);
    }


    if (it_worked) {
        in_or_out > 0 ? CUNYAIModule::updateUnitPhase(unit_, Stored_Unit::Phase::PathingOut) : CUNYAIModule::updateUnitPhase(unit_, Stored_Unit::Phase::PathingHome);
    }
    else {
        CUNYAIModule::updateUnitPhase(unit_, Stored_Unit::Phase::None);
    }
    return it_worked;
}


// This is basic combat logic for nonspellcasting units.
void Mobility::Tactical_Logic(const Stored_Unit &e_unit, Unit_Inventory &ei, const Unit_Inventory &ui, const int &passed_distance, const Color &color = Colors::White)

{
    Stored_Unit* target;
    //vector<int> useful_stocks = CUNYAIModule::getUsefulStocks(ui, ei);
    Unit last_target = unit_->getLastCommand().getTarget();
    Position targets_pos;

    int widest_dim = max(u_type_.height(), u_type_.width());
    int priority = 0;

    //auto path = BWEM::Map::Instance().GetPath(pos_, e_unit.pos_); // maybe useful later.
    int helpful_u = ui.moving_average_fap_stock_;
    int helpful_e = ei.moving_average_fap_stock_; // both forget value of psi units.
    int max_dist_no_priority = INT_MAX;
    int max_dist = passed_distance; // copy, to be modified later.
    bool weak_enemy_or_small_armies = (helpful_e < helpful_u || helpful_e < 150);
    bool target_sentinel = false;
    bool target_sentinel_poor_target_atk = false;
    bool melee = CUNYAIModule::getProperRange(unit_) < 32;
    double limit_units_diving = weak_enemy_or_small_armies ? 2 : 2 * log(helpful_e - helpful_u);
    double max_diveable_dist = passed_distance / static_cast<double>(limit_units_diving);


    for (auto e = ei.unit_map_.begin(); e != ei.unit_map_.end() && !ei.unit_map_.empty(); ++e) {
        if (e->second.valid_pos_ && e->first && e->first->exists()) { // only target observable units.
            UnitType e_type = e->second.type_;
            int e_priority = 0;
            bool can_continue_to_surround = !melee || (melee && e->second.circumference_remaining_ > widest_dim * 0.75);
            if (CUNYAIModule::Can_Fight(unit_, e->second) && can_continue_to_surround && !(e_type == UnitTypes::Protoss_Interceptor && u_type_ == UnitTypes::Zerg_Scourge)) { // if we can fight this enemy, do not suicide into cheap units.
                int dist_to_enemy = unit_->getDistance(e->second.pos_);

                bool critical_target = e_type.groundWeapon().innerSplashRadius() > 0 ||
                    (e_type.isSpellcaster() && !e_type.isBuilding()) ||
                    (e_type.isDetector() && ui.cloaker_count_ >= ei.detector_count_) ||
                    e_type == UnitTypes::Protoss_Carrier ||
                    (e->second.bwapi_unit_ && e->second.bwapi_unit_->exists() && e->second.bwapi_unit_->isRepairing()) ||
                    e_type == UnitTypes::Protoss_Reaver; // Prioritise these guys: Splash, crippled combat units
                bool lurkers_diving = u_type_ == UnitTypes::Zerg_Lurker && dist_to_enemy > UnitTypes::Zerg_Lurker.groundWeapon().maxRange();

                bool matching_area = e->second.areaID_ == stored_unit_->areaID_ || dist_to_enemy < 32;

                if (CUNYAIModule::Can_Fight(e->second, unit_) && critical_target && dist_to_enemy <= max_diveable_dist && !lurkers_diving) {
                    e_priority = 7;
                }
                else if (critical_target && dist_to_enemy <= max_diveable_dist) {
                    e_priority = 6;
                }
                else if (e->second.bwapi_unit_ && CUNYAIModule::Can_Fight(e->second, unit_) &&
                    dist_to_enemy < 32 &&
                    last_target &&
                    (last_target == e->second.bwapi_unit_ || (e->second.type_ == last_target->getType() && e->second.current_hp_ < last_target->getHitPoints()))) {
                    e_priority = 5;
                }
                else if (CUNYAIModule::Can_Fight(e->second, unit_)) {
                    e_priority = 4;
                }
                else if (e_type.isWorker()) {
                    e_priority = 3;
                }
                else if (e_type.isResourceDepot()) {
                    e_priority = 2;
                }
                else if (CUNYAIModule::IsFightingUnit(e->second) || e_type.spaceProvided() > 0) {
                    e_priority = 1;
                }
                else if (e->second.type_.mineralPrice() > 25 && e->second.type_ != UnitTypes::Zerg_Egg && e->second.type_ != UnitTypes::Zerg_Larva) {
                    e_priority = 0; // or if they cant fight back we'll get those last.
                }
                else {
                    e_priority = -1; // should leave stuff like larvae and eggs in here. Low, low priority.
                }


                if (e_priority >= priority && e_priority >= 3 && dist_to_enemy < max_dist && (matching_area || !melee) ) { // closest target of equal priority, or target of higher priority. Don't hop to enemies across the map when there are undefended things to destroy here.
                    target_sentinel = true;
                    priority = e_priority;
                    max_dist = dist_to_enemy; // now that we have one within range, let's tighten our existing range.
                    target = &e->second;
                }
                else if (e_priority >= priority && e_priority < 3 && dist_to_enemy < max_dist_no_priority && target_sentinel == false) {
                    target_sentinel_poor_target_atk = true;
                    max_dist_no_priority = dist_to_enemy; // then we will get the closest of these.
                    target = &e->second;
                }

            }
        }
    }

    bool attack_order_issued = false;
    //bool target_is_on_wrong_plane = target && target->bwapi_unit_ && target->elevation_ != ui.getStoredUnitValue(unit).elevation_ && melee && !ui.getStoredUnitValue(unit).is_flying_; // keeps crashing from nullptr target.

    if ((target_sentinel || target_sentinel_poor_target_atk) && unit_->hasPath(target->pos_) ){
        if (target->bwapi_unit_ && target->bwapi_unit_->exists()) {
            if (!adjust_lurker_burrow(target->pos_) ) {// adjust lurker if neccesary, otherwise attack.
                unit_->attack(target->bwapi_unit_);
                if (melee) { 
                    Stored_Unit& permenent_target = *CUNYAIModule::enemy_player_model.units_.getStoredUnit(target->bwapi_unit_);
                    permenent_target.circumference_remaining_ -= widest_dim;
                }
                CUNYAIModule::Diagnostic_Line(unit_->getPosition(), target->pos_, CUNYAIModule::current_map_inventory.screen_position_, color);
            }
            attack_order_issued = true;
        }
    }

    if (attack_order_issued) {
        CUNYAIModule::updateUnitPhase(unit_, Stored_Unit::Phase::Attacking);
    } 
    else {
        BWEM_Movement(1);
    }// if I'm not attacking and I'm in range, let's try the default action.
    return;
}


//Essentially, we would like to call the movement script BUT disable any attraction to the enemy since we are trying to only surround.
//void Mobility::Surrounding_Movement(const Unit & unit, const Unit_Inventory & ui, Unit_Inventory & ei, const Map_Inventory & inv){
//}

// Basic retreat logic
void Mobility::Retreat_Logic() {

    // lurkers should move when we need them to scout.
    if (u_type_ == UnitTypes::Zerg_Lurker && unit_->isBurrowed() && stored_unit_->time_since_last_dmg_ < 14) {
        unit_->unburrow();
        if (CUNYAIModule::updateUnitPhase(unit_, Stored_Unit::Phase::Retreating)) return;
    }
   
    moveTo(pos_, CUNYAIModule::current_map_inventory.home_base_);
    if (CUNYAIModule::updateUnitPhase(unit_, Stored_Unit::Phase::Retreating)) return;

}

Position Mobility::encircle(const Position & p) {
    Position vector_to = p - pos_;
    double theta = atan2(vector_to.y, vector_to.x);
    Position encircle_left = Position(static_cast<int>(-sin(theta) * distance_metric_), static_cast<int>(cos(theta) * distance_metric_)); // either {x,y}->{-y,x} or {x,y}->{y,-x} to rotate
    Position encircle_right = Position(static_cast<int>(sin(theta) * distance_metric_), static_cast<int>(-cos(theta) * distance_metric_)); // either {x,y}->{-y,x} or {x,y}->{y,-x} to rotate
    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<double> dis(0, 1);    // default values for output.

    return encircle_vector_ = (dis(gen) > 0.5) ? encircle_left : encircle_right; // only one direction for now.
}

Position Mobility::approach(const Position & p) {
    Position vector_to = p - pos_;
    double theta = atan2(vector_to.y, vector_to.x);
    Position approach_vector = Position(static_cast<int>(cos(theta) * distance_metric_), static_cast<int>(sin(theta) * distance_metric_)); // either {x,y}->{-y,x} or {x,y}->{y,-x} to rotate

    return attract_vector_ = approach_vector; // only one direction for now.
}

// returns TRUE if the lurker needed fixing. For Attack.
bool Mobility::adjust_lurker_burrow(const Position position_of_target) {
    int dist_to_threat_or_target = unit_->getDistance(position_of_target);
    bool dist_condition = dist_to_threat_or_target < UnitTypes::Zerg_Lurker.groundWeapon().maxRange();

    if (u_type_ == UnitTypes::Zerg_Lurker) {
        if ( !unit_->isBurrowed() && dist_condition) {
            unit_->burrow();
            return true;
        }
        else if ( unit_->isBurrowed() && !dist_condition) {
            unit_->unburrow();
            return true;
        }
        else if ( !unit_->isBurrowed() && !dist_condition) {
            double theta = atan2(position_of_target.y - unit_->getPosition().y, position_of_target.x - unit_->getPosition().x);
            Position closest_loc_to_permit_attacking = Position(position_of_target.x + static_cast<int>(cos(theta) * 0.75 * UnitTypes::Zerg_Lurker.groundWeapon().maxRange()), position_of_target.y + static_cast<int>(sin(theta) * 0.75 * UnitTypes::Zerg_Lurker.groundWeapon().maxRange()));
            unit_->move(closest_loc_to_permit_attacking);
            return true;
        }
    }

    return false;
}

class SpiralOut { // from SO
protected:
    unsigned layer;
    unsigned leg;
public:
    int x, y; //read these as output from next, do not modify.
    SpiralOut() :layer(1), leg(0), x(0), y(0) {}
    void goNext() {
        switch (leg) {
        case 0: ++x; if (x == layer)  ++leg;                break;
        case 1: ++y; if (y == layer)  ++leg;                break;
        case 2: --x; if (-x == layer)  ++leg;                break;
        case 3: --y; if (-y == layer) { leg = 0; ++layer; }   break;
        }
    }
};

Position Mobility::getVectorTowardsMap(const vector<vector<int>> &map) const {
    Position return_vector = Positions::Origin;
    int my_spot = CUNYAIModule::current_map_inventory.getMapValue(pos_, map);
    int temp_x = 0;
    int temp_y = 0;
    int current_best = INT_MAX;
    double theta = 0;
    vector<Position> barrier_points;
  
    SpiralOut spiral;

    // we need to spiral out from the center, stopping if we hit an object.
    WalkPosition map_dim = WalkPosition(TilePosition({ Broodwar->mapWidth(), Broodwar->mapHeight() }));
    for (int i = 0; i <= 256; i++) {
        spiral.goNext();
        int centralize_x = WalkPosition(pos_).x + spiral.x;
        int centralize_y = WalkPosition(pos_).y + spiral.y;
        bool shadow_check = false;

        for (auto barrier_point : barrier_points) {
            shadow_check = abs(centralize_x) >= abs(barrier_point.x) && abs(centralize_y) >= abs(barrier_point.y) && 
                           signbit(static_cast<float>(centralize_x)) == signbit(static_cast<float>(barrier_point.x)) && signbit(static_cast<float>(centralize_y)) == signbit(static_cast<float>(barrier_point.y)); // is it further out and in the same quadrant? If so it's in the "shadow". Rough, incredibly lazy.
            if (shadow_check) break;
        }

        if (centralize_x < map_dim.x &&
            centralize_y < map_dim.y &&
            centralize_x > 0 &&
            centralize_y > 0 &&
            !shadow_check
            ) // Is the spot acceptable?
        {
            if (map[centralize_x][centralize_y] <= 2) // if it's a barrier (or right on top of one), make a shadow.
            {
                barrier_points.push_back(Position(centralize_x, centralize_y));
            }
            else if (map[centralize_x][centralize_y] < current_best && map[centralize_x][centralize_y] > 2) // otherwise, if it's an improvement, go directly to the best destination
            {
                temp_x = spiral.x;
                temp_y = spiral.y;
                current_best = map[centralize_x][centralize_y];
            }
        }
    }

    if (temp_y != 0 || temp_x != 0) {
        theta = atan2(temp_y, temp_x);
        return_vector = Position(static_cast<int>(cos(theta) * distance_metric_), static_cast<int>(sin(theta) * distance_metric_)); //vector * scalar. Note if you try to return just the unit vector, Position will truncate the doubles to ints, and you'll get 0,0.
    }
    return  return_vector;
}

Position Mobility::getVectorTowardsField(const vector<vector<int>> &field) const {
    Position return_vector = Positions::Origin;
    int my_spot = CUNYAIModule::current_map_inventory.getFieldValue(pos_, field);
    int temp_x = 0;
    int temp_y = 0;
    int current_best = INT_MAX;
    double theta = 0;

    SpiralOut spiral; // don't really need to spiral out here anymore

    // we need to spiral out from the center, stopping if we hit an object.
    TilePosition map_dim = TilePosition({ Broodwar->mapWidth(), Broodwar->mapHeight() });
    for (int i = 0; i <= 64; i++) {
        spiral.goNext();
        int centralize_x = TilePosition(pos_).x + spiral.x;
        int centralize_y = TilePosition(pos_).y + spiral.y;

        if (centralize_x < map_dim.x &&
            centralize_y < map_dim.y &&
            centralize_x > 0 &&
            centralize_y > 0 
            ) // Is the spot acceptable?
        {
            if (field[centralize_x][centralize_y] > my_spot) {
                temp_x += spiral.x;
                temp_y += spiral.y;
            }
        }
    }

    if (temp_y != 0 || temp_x != 0) {
        theta = atan2(temp_y, temp_x);
        return_vector = Position(static_cast<int>(cos(theta) * distance_metric_), static_cast<int>(sin(theta) * distance_metric_)); //vector * scalar. Note if you try to return just the unit vector, Position will truncate the doubles to ints, and you'll get 0,0.
    }
    return  return_vector;
}

Position Mobility::getVectorAwayField(const vector<vector<int>> &field) const {
    Position return_vector = Positions::Origin;
    int my_spot = CUNYAIModule::current_map_inventory.getFieldValue(pos_, field);
    int temp_x = 0;
    int temp_y = 0;
    int current_best = INT_MAX;
    double theta = 0;

    SpiralOut spiral; // don't really need to spiral out here anymore

                      // we need to spiral out from the center, stopping if we hit an object.
    TilePosition map_dim = TilePosition({ Broodwar->mapWidth(), Broodwar->mapHeight() });
    for (int i = 0; i <= 64; i++) {
        spiral.goNext();
        int centralize_x = TilePosition(pos_).x + spiral.x;
        int centralize_y = TilePosition(pos_).y + spiral.y;

        if (centralize_x < map_dim.x &&
            centralize_y < map_dim.y &&
            centralize_x > 0 &&
            centralize_y > 0
            ) // Is the spot acceptable?
        {
            if (field[centralize_x][centralize_y] < my_spot) {
                temp_x += spiral.x;
                temp_y += spiral.y;
            }
        }
    }

    if (temp_y != 0 || temp_x != 0) {
        theta = atan2(temp_y, temp_x);
        return_vector = Position(static_cast<int>(cos(theta) * distance_metric_), static_cast<int>(sin(theta) * distance_metric_)); //vector * scalar. Note if you try to return just the unit vector, Position will truncate the doubles to ints, and you'll get 0,0.
    }
    return  return_vector;
}

bool Mobility::moveTo(const Position &start, const Position &finish)
{
    int plength = 0;
    bool unit_sent = false;
    auto cpp = BWEM::Map::Instance().GetPath(start, finish, &plength);

    if (!cpp.empty() && !unit_->isFlying()) {
        bool too_close = Position(cpp.front()->Center()).getApproxDistance(unit_->getPosition()) < 32 * 3;
        // first try traveling with CPP.
        if (!too_close && cpp.size() >= 1)  unit_sent = unit_->move(Position(cpp[0]->Center())); // if you're not too close, get closer.
        if (too_close && cpp.size() > 1) unit_sent = unit_->move(Position(cpp[1]->Center())); // if you're too close to one choke point, move to the next one!
        //if (too_close && cpp.size() == 1) continue; // we're too close too the end of the CPP. Congratulations!  now use your local pathing.
    }

    // then try traveling with local travel.
    if (!unit_sent) unit_sent = local_pathing(-1, finish);

    return unit_sent;
}
