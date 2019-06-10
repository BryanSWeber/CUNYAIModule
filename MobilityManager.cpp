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
bool Mobility::Tactical_Logic(const Stored_Unit &e_unit, Unit_Inventory &ei, const Unit_Inventory &ui, const int &passed_distance, const Color &color = Colors::White)

{
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
    

    // Let us bin all potentially interesting units.
    Unit_Inventory HighPriority;
    Unit_Inventory ThreatPriority;
    Unit_Inventory LowPriority;

    for (auto e = ei.unit_map_.begin(); e != ei.unit_map_.end() && !ei.unit_map_.empty(); ++e) {
        if (e->second.valid_pos_ && e->first && e->first->exists()) { // only target observable units.
            UnitType e_type = e->second.type_;
            int e_priority = 0;
            //bool can_continue_to_surround = !melee || (melee && e->second.circumference_remaining_ > widest_dim * 0.75);

            bool critical_target = e_type.groundWeapon().innerSplashRadius() > 0 ||
                (e_type.isSpellcaster() && !e_type.isBuilding()) ||
                (e_type.isDetector() && ui.cloaker_count_ >= ei.detector_count_) ||
                e_type == UnitTypes::Protoss_Carrier ||
                (e->second.bwapi_unit_ && e->second.bwapi_unit_->exists() && e->second.bwapi_unit_->isRepairing()) ||
                e_type == UnitTypes::Protoss_Reaver; // Prioritise these guys: Splash, crippled combat units

            if (e_type.isWorker() || (critical_target && CUNYAIModule::canContributeToFight(e_type, ui)) ) {
                HighPriority.addStored_Unit(e->second);
            }
            else if (CUNYAIModule::canContributeToFight(e_type, ui) || e_type.spaceProvided() > 0) {
                ThreatPriority.addStored_Unit(e->second);
            }
            else if (e->second.type_.mineralPrice() > 25 && e->second.type_ != UnitTypes::Zerg_Egg && e->second.type_ != UnitTypes::Zerg_Larva) { // don't target larva or noncosting units.
                LowPriority.addStored_Unit(e->second);
            }
        }
    }

    double dist_to_enemy = max_diveable_dist;
    Unit target = nullptr;

    HighPriority.updateUnitInventorySummary();
    ThreatPriority.updateUnitInventorySummary();
    LowPriority.updateUnitInventorySummary();

    for (auto h : HighPriority.unit_map_) {
        dist_to_enemy = unit_->getDistance(h.second.pos_);
        bool lurkers_diving = u_type_ == UnitTypes::Zerg_Lurker && dist_to_enemy > UnitTypes::Zerg_Lurker.groundWeapon().maxRange();
        bool diving_uphill = stored_unit_->areaID_ != h.second.areaID_ && melee && (stored_unit_->elevation_ != h.second.elevation_ && stored_unit_->elevation_ % 2 != 0); // they are on different elevations and my unit is not on a doodad (ramp, tunnel, etc.)
        if (dist_to_enemy < max_diveable_dist && !diving_uphill && !lurkers_diving && CUNYAIModule::Can_Fight_Type(u_type_, h.second.type_) && h.first &&  h.first->exists()) {
            max_diveable_dist = dist_to_enemy;
            target = h.first;
        }
    }
    
    if (!target) { // repeated calls should be functionalized.
        max_diveable_dist = 9999;
        for (auto t : ThreatPriority.unit_map_) {
            dist_to_enemy = unit_->getDistance(t.second.pos_);
            bool diving_uphill = stored_unit_->areaID_ != t.second.areaID_ && melee && (stored_unit_->elevation_ != t.second.elevation_ && stored_unit_->elevation_ % 2 != 0);
            if (dist_to_enemy < max_diveable_dist && !diving_uphill && CUNYAIModule::Can_Fight_Type(u_type_, t.second.type_) && t.first &&  t.first->exists()) {
                max_diveable_dist = dist_to_enemy;
                target = t.first;
            }
        }
    }

    if (!target) { // repeated calls should be functionalized.
        max_diveable_dist = 9999;
        for (auto l : LowPriority.unit_map_) {
            dist_to_enemy = unit_->getDistance(l.second.pos_);
            bool diving_uphill = stored_unit_->areaID_ != l.second.areaID_ && melee && (stored_unit_->elevation_ != l.second.elevation_ && stored_unit_->elevation_ % 2 != 0);
            if (dist_to_enemy < max_diveable_dist && !diving_uphill && CUNYAIModule::Can_Fight_Type(u_type_, l.second.type_) && l.first && l.first->exists()) {
                max_diveable_dist = dist_to_enemy;
                target = l.first;
            }
        }
    }

    if (target && target->exists()) {
        if (!adjust_lurker_burrow(target->getPosition())) {// adjust lurker if neccesary, otherwise attack.
            unit_->attack(target);
            //if (melee) {
            //    Stored_Unit& permenent_target = *CUNYAIModule::enemy_player_model.units_.getStoredUnit(target);
            //    permenent_target.circumference_remaining_ -= widest_dim;
            //}
            CUNYAIModule::Diagnostic_Line(pos_, target->getPosition(), CUNYAIModule::current_map_inventory.screen_position_, color);
            return CUNYAIModule::updateUnitPhase(unit_, Stored_Unit::Phase::Attacking);
        }
    }

    Broodwar->sendText("No target found");

    return false; // no target, we got a falsehood.
}


//Essentially, we would like to call the movement script BUT disable any attraction to the enemy since we are trying to only surround.
//void Mobility::Surrounding_Movement(const Unit & unit, const Unit_Inventory & ui, Unit_Inventory & ei, const Map_Inventory & inv){
//}

// Basic retreat logic
bool Mobility::Retreat_Logic() {

    // lurkers should move when we need them to scout.
    if (u_type_ == UnitTypes::Zerg_Lurker && unit_->isBurrowed() && stored_unit_->time_since_last_dmg_ < 14) {
        unit_->unburrow();
        return CUNYAIModule::updateUnitPhase(unit_, Stored_Unit::Phase::Retreating);
    }
   
    moveTo(pos_, CUNYAIModule::current_map_inventory.home_base_);
    return CUNYAIModule::updateUnitPhase(unit_, Stored_Unit::Phase::Retreating);
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
        // first try traveling with CPP.
        bool has_a_blocking_item = (BWEM::Map::Instance().GetTile(TilePosition(cpp.front()->Center())).GetNeutral() || BWEM::Map::Instance().GetTile(TilePosition(cpp.front()->Center())).Doodad());
        bool too_close = Position(cpp.front()->Center()).getApproxDistance(unit_->getPosition()) < 32 * (2 + 3.5 * has_a_blocking_item );
        if (!too_close && cpp.size() >= 1)  unit_sent = unit_->move(Position(cpp[0]->Center())); // if you're not too close, get closer.
        if (too_close && cpp.size() > 1) unit_sent = unit_->move(Position(cpp[1]->Center())); // if you're too close to one choke point, move to the next one!
        //if (too_close && cpp.size() == 1) continue; // we're too close too the end of the CPP. Congratulations!  now use your local pathing.
    }

    // then try traveling with local travel.
    if (!unit_sent) unit_sent = local_pathing(-1, finish);

    return unit_sent;
}