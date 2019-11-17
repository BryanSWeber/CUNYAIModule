#pragma once

# include "Source\CUNYAIModule.h"
# include "Source\MobilityManager.h"
# include "Source\Diagnostics.h"
# include <random> // C++ base random is low quality.
# include <numeric>
# include <math.h>

#define TOO_FAR_FROM_FRONT (CUNYAIModule::current_map_inventory.getRadialDistanceOutFromEnemy(pos_) > (CUNYAIModule::friendly_player_model.closest_ground_combatant_ + 3.0 * 0.125 * distance_metric_ )); //radial distance is in minitiles, distance is in pixels.
//#define DISTANCE_METRIC (2.760 * 24.0);

using namespace BWAPI;
using namespace Filter;
using namespace std;


//Forces a unit to stutter in a Mobility manner. Size of stutter is unit's (vision range * n ). Will attack if it sees something.  Overlords & lings stop if they can see minerals.

bool Mobility::local_pathing(const int &passed_distance, const Position &e_pos, const Stored_Unit::Phase phase) {

    // lurkers should move when we need them to scout.
    if (u_type_ == UnitTypes::Zerg_Lurker && unit_->isBurrowed() && !CUNYAIModule::getClosestThreatOrTargetStored(CUNYAIModule::enemy_player_model.units_, unit_, max(UnitTypes::Zerg_Lurker.groundWeapon().maxRange(), CUNYAIModule::enemy_player_model.units_.max_range_))) {
        unit_->unburrow();
        return CUNYAIModule::updateUnitPhase(unit_, Stored_Unit::Phase::PathingOut);
    }

    Unit_Inventory friendly_blocks = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, e_pos, 64);
    friendly_blocks.updateUnitInventorySummary();
    bool has_a_blocking_item = (BWEM::Map::Instance().GetTile(TilePosition(e_pos)).GetNeutral() || BWEM::Map::Instance().GetTile(TilePosition(e_pos)).Doodad() || friendly_blocks.building_count_ > 0);

    if (has_a_blocking_item && !unit_->isFlying())
        encircle(e_pos);

    approach(e_pos);
    if (unit_->move(pos_ + attract_vector_ + encircle_vector_)) {
        Diagnostics::drawLine(pos_, pos_ + encircle_vector_, CUNYAIModule::current_map_inventory.screen_position_, Colors::Blue);//Run around an obstacle.
        Diagnostics::drawLine(pos_, pos_ + attract_vector_, CUNYAIModule::current_map_inventory.screen_position_, Colors::White);//Run towards it.
        Diagnostics::drawLine(pos_, e_pos, CUNYAIModule::current_map_inventory.screen_position_, Colors::Red);//Run around 
        return CUNYAIModule::updateUnitPhase(unit_, phase);
    }
    return false;
}

bool Mobility::BWEM_Movement(const bool &forward_movement) {
    bool it_worked = false;
    Position target_pos = Positions::Origin;
    // Units should head towards enemies when there is a large gap in our knowledge, OR when it's time to pick a fight.
    if (forward_movement) {
        if (CUNYAIModule::combat_manager.isScout(unit_)) {
            it_worked = moveTo(pos_, CUNYAIModule::current_map_inventory.scouting_base_, Stored_Unit::Phase::PathingOut);
            target_pos = CUNYAIModule::current_map_inventory.scouting_base_;
        }
        else if (u_type_.airWeapon() == WeaponTypes::None && u_type_.groundWeapon() != WeaponTypes::None) { // if you can't help air go ground.
            it_worked = moveTo(pos_, CUNYAIModule::current_map_inventory.enemy_base_ground_, Stored_Unit::Phase::PathingOut);
            target_pos = CUNYAIModule::current_map_inventory.enemy_base_ground_;
        }
        else if (u_type_.airWeapon() != WeaponTypes::None && u_type_.groundWeapon() == WeaponTypes::None) { // if you can't help ground go air.
            it_worked = moveTo(pos_, CUNYAIModule::current_map_inventory.enemy_base_air_, Stored_Unit::Phase::PathingOut);
            target_pos = CUNYAIModule::current_map_inventory.enemy_base_air_;
        }
        else if (u_type_.groundWeapon() != WeaponTypes::None && u_type_.airWeapon() != WeaponTypes::None) { // otherwise go to whicheve type has an active problem..
            if (CUNYAIModule::friendly_player_model.u_have_active_air_problem_) {
                it_worked = moveTo(pos_, CUNYAIModule::current_map_inventory.enemy_base_air_, Stored_Unit::Phase::PathingOut);
                target_pos = CUNYAIModule::current_map_inventory.enemy_base_air_;
            }
            else {
                it_worked = moveTo(pos_, CUNYAIModule::current_map_inventory.enemy_base_ground_, Stored_Unit::Phase::PathingOut);
                target_pos = CUNYAIModule::current_map_inventory.enemy_base_ground_;
            }
        }
    }
    else { // Otherwise, return to home.
        it_worked = moveTo(pos_, CUNYAIModule::current_map_inventory.front_line_base_, Stored_Unit::Phase::PathingHome);
        target_pos = CUNYAIModule::current_map_inventory.front_line_base_;
    }

    if (target_pos != Positions::Origin && stored_unit_->type_ == UnitTypes::Zerg_Lurker) it_worked = adjust_lurker_burrow(target_pos) || it_worked;

    if (it_worked && target_pos != Positions::Origin && pos_.getDistance(target_pos) > stored_unit_->type_.sightRange()) {
        forward_movement ? CUNYAIModule::updateUnitPhase(unit_, Stored_Unit::Phase::PathingOut) : CUNYAIModule::updateUnitPhase(unit_, Stored_Unit::Phase::PathingHome);
    }
    else {
        CUNYAIModule::updateUnitPhase(unit_, Stored_Unit::Phase::None);
    }
    return it_worked;
}

bool Mobility::surround(const Position & pos)
{
    encircle(pos);
    //avoid_edges();//Prototyping
    if (unit_->move(pos_ + encircle_vector_)) {
        Diagnostics::drawLine(pos_, pos_ + encircle_vector_, CUNYAIModule::current_map_inventory.screen_position_, Colors::Blue);//show we're running around it
        Diagnostics::drawLine(pos_ + encircle_vector_, pos_ + encircle_vector_ + walkability_vector_, CUNYAIModule::current_map_inventory.screen_position_, Colors::White);//show we're avoiding low ground.
        Diagnostics::drawLine(pos_, pos, CUNYAIModule::current_map_inventory.screen_position_, Colors::Red);//show what we're surrounding.
        //Diagnostics::DiagnosticTrack(pos_ + encircle_vector_ + walkability_vector_);
        return CUNYAIModule::updateUnitPhase(unit_, Stored_Unit::Phase::Surrounding);
    }
    return false;
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
    //int max_dist = passed_distance; // copy, to be modified later.
    bool weak_enemy_or_small_armies = (helpful_e < helpful_u || helpful_e < 500 || ei.worker_count_ == static_cast<int>(ei.unit_map_.size()) );
    bool target_sentinel = false;
    bool target_sentinel_poor_target_atk = false;
    bool suicide_unit = stored_unit_->type_ == UnitTypes::Zerg_Scourge || stored_unit_->type_ == UnitTypes::Zerg_Infested_Terran;
    bool melee = CUNYAIModule::getProperRange(unit_) < 32;
    double limit_units_diving = weak_enemy_or_small_armies ? (FAP_SIM_DURATION/12) : (FAP_SIM_DURATION / 12) * log(helpful_e - helpful_u); // should be relatively stable if I reduce the duration.
    
    // Let us bin all potentially interesting units.
    Unit_Inventory DiveableTargets;
    Unit_Inventory ThreateningTargets;
    Unit_Inventory SecondOrderThreats;
    Unit_Inventory LowPriority;

    for (auto e = ei.unit_map_.begin(); e != ei.unit_map_.end() && !ei.unit_map_.empty(); ++e) {
        if (e->second.valid_pos_ && e->first && e->first->exists() && e->first->isDetected()) { // only target observable units.
            UnitType e_type = e->second.type_;
            int e_priority = 0;
            //bool can_continue_to_surround = !melee || (melee && e->second.circumference_remaining_ > widest_dim * 0.75);
            if (!suicide_unit || (suicide_unit && e->second.stock_value_ >= stored_unit_->stock_value_ && CUNYAIModule::isFightingUnit(e->second.type_))) {
                bool critical_target = e_type.groundWeapon().innerSplashRadius() > 0 ||
                    (e_type.isSpellcaster() && !e_type.isBuilding()) ||
                    (e_type.isDetector() && ui.cloaker_count_ >= ei.detector_count_) ||
                    e_type == UnitTypes::Protoss_Carrier ||
                    (e->second.bwapi_unit_ && e->second.bwapi_unit_->exists() && e->second.bwapi_unit_->isRepairing()) ||
                    e_type == UnitTypes::Protoss_Reaver; // Prioritise these guys: Splash, crippled combat units

                if (e_type.isWorker() || critical_target && CUNYAIModule::canContributeToFight(e_type, ui)) {
                    DiveableTargets.addStored_Unit(e->second);
                }

                if (CUNYAIModule::Can_Fight(e_type, unit_)) {
                    ThreateningTargets.addStored_Unit(e->second);
                }

                if (CUNYAIModule::canContributeToFight(e_type, ui) || e_type.spaceProvided() > 0) {
                    SecondOrderThreats.addStored_Unit(e->second);
                }

                if ((e->second.type_.mineralPrice() > 25 || e->second.type_.gasPrice() > 25) && e->second.type_ != UnitTypes::Zerg_Egg && e->second.type_ != UnitTypes::Zerg_Larva) { // don't target larva or noncosting units.
                    LowPriority.addStored_Unit(e->second);
                }
            }
        }
    }

    double dist_to_enemy = passed_distance;
    Unit target = nullptr;

    ThreateningTargets.unit_map_.insert(DiveableTargets.unit_map_.begin(), DiveableTargets.unit_map_.end());
    SecondOrderThreats.unit_map_.insert(ThreateningTargets.unit_map_.begin(), ThreateningTargets.unit_map_.end());
    LowPriority.unit_map_.insert(SecondOrderThreats.unit_map_.begin(), SecondOrderThreats.unit_map_.end());

    DiveableTargets.updateUnitInventorySummary();
    ThreateningTargets.updateUnitInventorySummary();
    SecondOrderThreats.updateUnitInventorySummary();
    LowPriority.updateUnitInventorySummary();

    // Dive some modest distance if they're critical to kill.
    double temp_max_divable = max(CUNYAIModule::getChargableDistance(unit_) / static_cast<double>(limit_units_diving), static_cast<double>(CUNYAIModule::getProperRange(unit_)));
    if (!target) { // repeated calls should be functionalized.
        for (auto t : DiveableTargets.unit_map_) {
            dist_to_enemy = unit_->getDistance(t.second.pos_);
            bool baseline_requirement = (!isOnDifferentHill(t.second) || stored_unit_->is_flying_) && CUNYAIModule::Can_Fight(u_type_, t.second.type_);
            if (dist_to_enemy < temp_max_divable && baseline_requirement) {
                temp_max_divable = dist_to_enemy;
                target = t.first;
            }
        }
    }

    // Shoot closest threat if they can shoot you or vis versa.
    temp_max_divable = max(ei.max_range_, CUNYAIModule::getProperRange(unit_));
    if (!target) { // repeated calls should be functionalized.
        for (auto t : ThreateningTargets.unit_map_) {
            dist_to_enemy = unit_->getDistance(t.second.pos_);
            bool baseline_requirement = (!isOnDifferentHill(t.second) || stored_unit_->is_flying_) && CUNYAIModule::Can_Fight(u_type_, t.second.type_);
            if (dist_to_enemy < temp_max_divable && baseline_requirement) {
                temp_max_divable = dist_to_enemy;
                target = t.first;
            }
        }
    }

    // If they are threatening something, feel free to dive some distance to them, but not too far as to trigger another fight.
    temp_max_divable = max(ei.max_range_, CUNYAIModule::getProperRange(unit_));
    if (!target) { // repeated calls should be functionalized.
        for (auto t : SecondOrderThreats.unit_map_) {
            dist_to_enemy = unit_->getDistance(t.second.pos_);
            bool baseline_requirement = (!isOnDifferentHill(t.second) || stored_unit_->is_flying_) && CUNYAIModule::Can_Fight(u_type_, t.second.type_);
            if (dist_to_enemy < temp_max_divable && baseline_requirement) {
                temp_max_divable = dist_to_enemy;
                target = t.first;
            }
        }
    }

    temp_max_divable = INT_MAX;
    if (!target) { // repeated calls should be functionalized.
        for (auto t : LowPriority.unit_map_) {
            dist_to_enemy = unit_->getDistance(t.second.pos_);
            if (dist_to_enemy < temp_max_divable && (!isOnDifferentHill(t.second) || stored_unit_->is_flying_) && CUNYAIModule::Can_Fight(u_type_, t.second.type_)) {
                temp_max_divable = dist_to_enemy;
                target = t.first;
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
        }
        Diagnostics::drawLine(pos_, target->getPosition(), CUNYAIModule::current_map_inventory.screen_position_, color);
        return CUNYAIModule::updateUnitPhase(unit_, Stored_Unit::Phase::Attacking);
    }
    else if (u_type_ == UnitTypes::Zerg_Lurker && unit_->isBurrowed()) {
        if (unit_->unburrow()) return CUNYAIModule::updateUnitPhase(unit_, Stored_Unit::Phase::Attacking);
    }

    Diagnostics::DiagnosticText("No target found");

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

    if (stored_unit_->shoots_down_ || stored_unit_->shoots_up_) {
        moveTo(pos_, CUNYAIModule::current_map_inventory.front_line_base_, Stored_Unit::Phase::Retreating);
    }
    else if (CUNYAIModule::combat_manager.isScout(unit_)) {
        auto threat = CUNYAIModule::getClosestThreatStored(CUNYAIModule::enemy_player_model.units_, unit_, 400);
        if (threat) {
            approach(CUNYAIModule::current_map_inventory.safe_base_);
            //encircle(threat->pos_);
            moveTo(pos_, pos_ + attract_vector_ /*+ encircle_vector_*/, Stored_Unit::Phase::Retreating);
        }
    }
    else {
        moveTo(pos_, CUNYAIModule::current_map_inventory.safe_base_, Stored_Unit::Phase::Retreating);
    }
    return CUNYAIModule::updateUnitPhase(unit_, Stored_Unit::Phase::Retreating);
}

bool Mobility::Scatter_Logic(const Position pos)
{
    Position problem_pos = Positions::Origin;

    // lurkers should move when we need them to scout.
    if (u_type_ == UnitTypes::Zerg_Lurker && unit_->isBurrowed() && stored_unit_->time_since_last_dmg_ < 14) {
        unit_->unburrow();
        return CUNYAIModule::updateUnitPhase(unit_, Stored_Unit::Phase::Retreating);
    }


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

    approach(problem_pos);
    if (unit_->move(pos_ - attract_vector_))
        return CUNYAIModule::updateUnitPhase(unit_, Stored_Unit::Phase::Retreating);
    else
        return false;
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

Position Mobility::avoid_edges() {

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
        Diagnostics::DiagnosticText("No higher ground?");
        return Positions::Origin;
    }
    else {
        pair<BWEM::altitude_t, WalkPosition> targeted_pair = *CUNYAIModule::select_randomly(higher_ground.begin(), higher_ground.end());
        Position vector_to = Position(targeted_pair.second) - pos_;
        double theta = atan2(vector_to.y, vector_to.x);
        walkability_vector_ = Position(static_cast<int>(cos(theta) * distance_metric_ * 0.25), static_cast<int>(sin(theta) * distance_metric_* 0.25)); // either {x,y}->{-y,x} or {x,y}->{y,-x} to rotate
        return walkability_vector_;
    }
}

Position Mobility::approach(const Position & p) {
    Position vector_to = p - pos_;
    double theta = atan2(vector_to.y, vector_to.x);
    Position approach_vector = Position(static_cast<int>(cos(theta) * distance_metric_), static_cast<int>(sin(theta) * distance_metric_)); // either {x,y}->{-y,x} or {x,y}->{y,-x} to rotate

    return attract_vector_ = approach_vector; // only one direction for now.
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

bool Mobility::checkSafePath(const Position &finish) {
    int plength = 0;
    bool unit_sent = false;
    auto cpp = BWEM::Map::Instance().GetPath(pos_, finish, &plength);
    Unit_Inventory ei_temp;
    if (!Mobility::checkSafeEscapePath(finish)) return false;

    if (plength) {
        BWEM::Area area = *BWEM::Map::Instance().GetArea(TilePosition(finish));
        if (area.Data()) return false;
        return true;
    }
    return false;
}

// returns TRUE if the lurker needed fixing. For Attack.
bool Mobility::adjust_lurker_burrow(const Position position_of_target) {
    int dist_to_threat_or_target = unit_->getDistance(position_of_target);
    bool dist_condition = dist_to_threat_or_target < UnitTypes::Zerg_Lurker.groundWeapon().maxRange();

    if (u_type_ == UnitTypes::Zerg_Lurker) {
        if (!unit_->isBurrowed() && dist_condition) {
            unit_->burrow();
            return true;
        }
        else if (unit_->isBurrowed() && !dist_condition) {
            unit_->unburrow();
            return true;
        }
        else if (!unit_->isBurrowed() && !dist_condition) {
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

bool Mobility::moveTo(const Position &start, const Position &finish, const Stored_Unit::Phase phase)
{
    int plength = 0;
    bool unit_sent = false;
    if (!start.isValid() || !finish.isValid()) {
        return false;
    }
    auto cpp = BWEM::Map::Instance().GetPath(start, finish, &plength);

    if (!cpp.empty() && !unit_->isFlying()) {
        // first try traveling with CPP.
        Unit_Inventory friendly_blocks = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, Position(cpp.front()->Center()), 64);
        friendly_blocks.updateUnitInventorySummary();
        bool has_a_blocking_item = (BWEM::Map::Instance().GetTile(TilePosition(cpp.front()->Center())).GetNeutral() || BWEM::Map::Instance().GetTile(TilePosition(cpp.front()->Center())).Doodad() || friendly_blocks.building_count_ > 0 );
        bool too_close = Position(cpp.front()->Center()).getApproxDistance(unit_->getPosition()) < 32 * (2 + 3.5 * has_a_blocking_item);
        if (!too_close && cpp.size() >= 1)  unit_sent = unit_->move(Position(cpp[0]->Center())); // if you're not too close, get closer.
        if (too_close && cpp.size() > 1) unit_sent = unit_->move(Position(cpp[1]->Center())); // if you're too close to one choke point, move to the next one!
        //if (too_close && cpp.size() == 1) continue; // we're too close too the end of the CPP. Congratulations!  now use your local pathing.
    }


    // then try traveling with local travel. Should have plength > 0
    if (!unit_sent && plength) unit_sent = local_pathing(plength, finish, phase);

    return unit_sent;
}

int Mobility::getDistanceMetric()
{
    return distance_metric_;
}

bool Mobility::isOnDifferentHill(const Stored_Unit &e) {
    int altitude = BWEM::Map::Instance().GetMiniTile(WalkPosition(e.pos_)).Altitude();
    return stored_unit_->areaID_ != e.areaID_ && (stored_unit_->elevation_ != e.elevation_ && stored_unit_->elevation_ % 2 != 0 && e.elevation_ % 2 != 0) && altitude + 96 < CUNYAIModule::getProperRange(unit_);
}