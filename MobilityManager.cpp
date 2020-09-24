#pragma once

# include "Source\CUNYAIModule.h"
# include "Source\Diagnostics.h"
# include "Source\MobilityManager.h"
# include <random> // C++ base random is low quality.
# include <numeric>
# include <math.h>

//#define TOO_FAR_FROM_FRONT (CUNYAIModule::current_MapInventory.getRadialDistanceOutFromEnemy(pos_) > (CUNYAIModule::friendly_player_model.closest_ground_combatant_ + 3.0 * 0.125 * distance_metric_ )); //radial distance is in minitiles, distance is in pixels.
//#define DISTANCE_METRIC (2.760 * 24.0);

using namespace BWAPI;
using namespace Filter;
using namespace std;


//Forces a unit to stutter in a Mobility manner. Size of stutter is unit's (vision range * n ). Will attack if it sees something.  Overlords & lings stop if they can see minerals.

bool Mobility::local_pathing(const Position &e_pos, const StoredUnit::Phase phase) {

    // lurkers should move when we need them to scout.
    if (u_type_ == UnitTypes::Zerg_Lurker && unit_->isBurrowed() && !CUNYAIModule::getClosestThreatOrTargetStored(CUNYAIModule::enemy_player_model.units_, unit_, max(UnitTypes::Zerg_Lurker.groundWeapon().maxRange(), CUNYAIModule::enemy_player_model.units_.max_range_))) {
        unit_->unburrow();
        return CUNYAIModule::updateUnitPhase(unit_, StoredUnit::Phase::PathingOut);
    }

    UnitInventory friendly_blocks = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, e_pos, 64);
    friendly_blocks.updateUnitInventorySummary();
    bool has_a_blocking_item = (BWEM::Map::Instance().GetTile(TilePosition(e_pos)).GetNeutral() || BWEM::Map::Instance().GetTile(TilePosition(e_pos)).Doodad() || friendly_blocks.building_count_ > 0);

    if (has_a_blocking_item && !unit_->isFlying())
        encircle(e_pos);

    approach(e_pos);
    if (unit_->move(pos_ + attract_vector_ + encircle_vector_)) {
        Diagnostics::drawLine(pos_, pos_ + encircle_vector_, CUNYAIModule::currentMapInventory.screen_position_, Colors::Blue);//Run around an obstacle.
        Diagnostics::drawLine(pos_, pos_ + attract_vector_, CUNYAIModule::currentMapInventory.screen_position_, Colors::White);//Run towards it.
        Diagnostics::drawLine(pos_, e_pos, CUNYAIModule::currentMapInventory.screen_position_, Colors::Red);//Run around 
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
            //if (unit_->isFlying()) {
            //    it_worked = moveTo(pos_, CUNYAIModule::current_MapInventory.air_scouting_base_, StoredUnit::Phase::PathingOut);
            //    target_pos = CUNYAIModule::current_MapInventory.air_scouting_base_;
            //}
            //else {
            int scouts = CUNYAIModule::combat_manager.scoutPosition(unit_);
            it_worked = moveTo(pos_, CUNYAIModule::currentMapInventory.scouting_bases_.at(scouts), StoredUnit::Phase::PathingOut);
            target_pos = CUNYAIModule::currentMapInventory.scouting_bases_.at(scouts);
            //}
        }
        else if (u_type_.airWeapon() == WeaponTypes::None && u_type_.groundWeapon() != WeaponTypes::None) { // if you can't help air go ground.
            it_worked = moveTo(pos_, CUNYAIModule::currentMapInventory.enemy_base_ground_, StoredUnit::Phase::PathingOut);
            target_pos = CUNYAIModule::currentMapInventory.enemy_base_ground_;
        }
        else if (u_type_.airWeapon() != WeaponTypes::None && u_type_.groundWeapon() == WeaponTypes::None) { // if you can't help ground go air.
            it_worked = moveTo(pos_, CUNYAIModule::currentMapInventory.enemy_base_air_, StoredUnit::Phase::PathingOut);
            target_pos = CUNYAIModule::currentMapInventory.enemy_base_air_;
        }
        else if (u_type_.groundWeapon() != WeaponTypes::None && u_type_.airWeapon() != WeaponTypes::None) { // otherwise go to whicheve type has an active problem..
            if (CUNYAIModule::friendly_player_model.u_have_active_air_problem_) {
                it_worked = moveTo(pos_, CUNYAIModule::currentMapInventory.enemy_base_air_, StoredUnit::Phase::PathingOut);
                target_pos = CUNYAIModule::currentMapInventory.enemy_base_air_;
            }
            else {
                it_worked = moveTo(pos_, CUNYAIModule::currentMapInventory.enemy_base_ground_, StoredUnit::Phase::PathingOut);
                target_pos = CUNYAIModule::currentMapInventory.enemy_base_ground_;
            }
        }
    }
    else { // Otherwise, return to home.
        it_worked = moveTo(pos_, CUNYAIModule::currentMapInventory.front_line_base_, StoredUnit::Phase::PathingHome);
        target_pos = CUNYAIModule::currentMapInventory.front_line_base_;
    }

    if (target_pos != Positions::Origin && stored_unit_->type_ == UnitTypes::Zerg_Lurker) it_worked = prepareLurkerToAttack(target_pos) || it_worked;

    if (it_worked && target_pos != Positions::Origin && pos_.getDistance(target_pos) > stored_unit_->type_.sightRange()) {
        forward_movement ? CUNYAIModule::updateUnitPhase(unit_, StoredUnit::Phase::PathingOut) : CUNYAIModule::updateUnitPhase(unit_, StoredUnit::Phase::PathingHome);
    }
    else {
        CUNYAIModule::updateUnitPhase(unit_, StoredUnit::Phase::None);
    }
    return it_worked;
}

bool Mobility::surroundLogic(const Position & pos)
{
    encircle(pos);
    //avoid_edges();//Prototyping
    //isolate();
    //Position get_proper_surround_distance = getVectorTowardsMap(CUNYAIModule::current_MapInventory.map_out_from_enemy_ground_, 250 / 4);
    if (unit_->move(pos_ + encircle_vector_)) {
        //Diagnostics::drawLine(pos_, pos_ + seperation_vector_, CUNYAIModule::current_MapInventory.screen_position_, Colors::Blue);//show we're seperating from others.
        //Diagnostics::drawLine(pos_ + seperation_vector_, pos_ + seperation_vector_ + walkability_vector_, CUNYAIModule::current_MapInventory.screen_position_, Colors::White);//show we're avoiding low ground.
        //Diagnostics::drawLine(pos_ + seperation_vector_ + walkability_vector_, pos_ + seperation_vector_ + walkability_vector_ + get_proper_surround_distance, CUNYAIModule::current_MapInventory.screen_position_, Colors::White);//show we're avoiding low ground.
        //Diagnostics::drawLine(pos_ + seperation_vector_ + walkability_vector_ + get_proper_surround_distance, pos_ + seperation_vector_ + walkability_vector_ + get_proper_surround_distance + encircle_vector_, CUNYAIModule::current_MapInventory.screen_position_, Colors::White);//show we're avoiding low ground.
        //Diagnostics::drawLine(pos_, pos, CUNYAIModule::current_MapInventory.screen_position_, Colors::Red);//show what we're surrounding.
        //Diagnostics::DiagnosticTrack(pos_ + encircle_vector_ + walkability_vector_);
        return CUNYAIModule::updateUnitPhase(unit_, StoredUnit::Phase::Surrounding);
    }
    return false;
}

bool Mobility::isolate()
{
    UnitInventory u_loc = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, pos_, distance_metric_);
    
    Position central_pos = Positions::Origin;
    for (auto u : u_loc.unit_map_) {
        central_pos += u.second.pos_ - pos_;
    }

    Position vector_away = Positions::Origin - (central_pos - pos_); 
    double theta = atan2(vector_away.y, vector_away.x); // we want to go away from them.
    Position seperation_vector_ = Position(static_cast<int>(cos(theta) * 64), static_cast<int>(sin(theta) * 64)); // either {x,y}->{-y,x} or {x,y}->{y,-x} to rotate

}

// This is basic combat logic for nonspellcasting units.
bool Mobility::Tactical_Logic(const StoredUnit &e_unit, UnitInventory &ei, const UnitInventory &ui, const int &passed_distance, const Color &color = Colors::White)

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

                if (CUNYAIModule::Can_Fight(e_type, unit_)) {
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
    temp_max_divable = max(ei.max_range_, CUNYAIModule::getFunctionalRange(unit_));
    if (!target) { // repeated calls should be functionalized.
        target = pickTarget(temp_max_divable, ThreateningTargets);
    }

    // If they are threatening something, feel free to dive some distance to them, but not too far as to trigger another fight.
    temp_max_divable = max( ei.max_range_, CUNYAIModule::getFunctionalRange(unit_));
    if (!target) { // repeated calls should be functionalized.
        target = pickTarget(temp_max_divable, SecondOrderThreats);
    }

    temp_max_divable = INT_MAX;
    if (!target) { // repeated calls should be functionalized.
        target = pickTarget(temp_max_divable, LowPriority);
    }

    if (target && !DISABLE_ATTACKING) {
        if (!prepareLurkerToAttack(target->getPosition())) {// adjust lurker if neccesary, otherwise attack.
            if (melee && !unit_->isFlying()) { // Attempting surround code.
                StoredUnit& permenent_target = *CUNYAIModule::enemy_player_model.units_.getStoredUnit(target);
                permenent_target.circumference_remaining_ -= widest_dim;
                if (permenent_target.circumference_remaining_ < permenent_target.circumference_ / 4 && unit_->getDistance(target) > CUNYAIModule::getFunctionalRange(unit_) && permenent_target.type_.isBuilding()) {
                        unit_->move(pos_ + getVectorToEnemyDestination(target) + getVectorToBeyondEnemy(target));
                    return CUNYAIModule::updateUnitPhase(unit_, StoredUnit::Phase::Surrounding);
                }
            }
            if (target->exists())
                unit_->attack(target);
            else
                unit_->attack(pos_ + getVectorToEnemyDestination(target) + getVectorToBeyondEnemy(target));
        }
        Diagnostics::drawLine(pos_, target->getPosition(), CUNYAIModule::currentMapInventory.screen_position_, color);
        return CUNYAIModule::updateUnitPhase(unit_, StoredUnit::Phase::Attacking);
    }
    //else if (u_type_ == UnitTypes::Zerg_Lurker && unit_->isBurrowed()) {
    //    if (unit_->unburrow()) return CUNYAIModule::updateUnitPhase(unit_, StoredUnit::Phase::Attacking);
    //}

    if (CUNYAIModule::countUnits(UnitTypes::Zerg_Larva, ei) + CUNYAIModule::countUnits(UnitTypes::Zerg_Overlord, ei) < ei.unit_map_.size()) {
        Diagnostics::DiagnosticText("An enemy %s vs. friendly %s began this tactical logic.", e_unit.type_.c_str(), u_type_.c_str());
        Diagnostics::DiagnosticText("This is the passed unit map");
        for (auto u : ei.unit_map_) {
            Diagnostics::DiagnosticText("%s", u.second.type_.c_str());
        }
    }
    return false; // no target, we got a falsehood.
}

//Essentially, we would like to call the movement script BUT disable any attraction to the enemy since we are trying to only surround.
//void Mobility::Surrounding_Movement(const Unit & unit, const UnitInventory & ui, UnitInventory & ei, const MapInventory & inv){
//}

// Basic retreat logic
bool Mobility::Retreat_Logic(const StoredUnit &e) {

    // lurkers should move when we need them to scout.
    if (u_type_ == UnitTypes::Zerg_Lurker && unit_->isBurrowed() && CUNYAIModule::currentMapInventory.isTileDetected(pos_) ) {
        unit_->unburrow();
        return CUNYAIModule::updateUnitPhase(unit_, StoredUnit::Phase::Retreating);
    }

    Position next_waypoint = getNextWaypoint(pos_, CUNYAIModule::currentMapInventory.safe_base_);

    if ( (!unit_->isFlying() && checkSameDirection(next_waypoint-pos_,e.pos_-pos_)) || (stored_unit_ && stored_unit_->phase_ == StoredUnit::Phase::Surrounding)) {
        approach(e.pos_ + Position(e.velocity_x_, e.velocity_y_) );
        moveTo(pos_, pos_ - attract_vector_, StoredUnit::Phase::Retreating);
    }
    else if (stored_unit_->shoots_down_ || stored_unit_->shoots_up_) {
        moveTo(pos_, CUNYAIModule::currentMapInventory.front_line_base_, StoredUnit::Phase::Retreating);
    }
    else if (CUNYAIModule::combat_manager.isScout(unit_)) {
        approach(CUNYAIModule::currentMapInventory.safe_base_);
        //encircle(threat->pos_);
        moveTo(pos_, pos_ + attract_vector_ /*+ encircle_vector_*/, StoredUnit::Phase::Retreating);
    }
    else {
        moveTo(pos_, CUNYAIModule::currentMapInventory.safe_base_, StoredUnit::Phase::Retreating);
    }
    return CUNYAIModule::updateUnitPhase(unit_, StoredUnit::Phase::Retreating);
}

bool Mobility::Scatter_Logic(const Position pos)
{
    Position problem_pos = Positions::Origin;

    // lurkers should move when we need them to scout.
    if (u_type_ == UnitTypes::Zerg_Lurker && unit_->isBurrowed() && CUNYAIModule::currentMapInventory.isTileDetected(pos_)) {
        unit_->unburrow();
        return CUNYAIModule::updateUnitPhase(unit_, StoredUnit::Phase::Retreating);
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
        return CUNYAIModule::updateUnitPhase(unit_, StoredUnit::Phase::Retreating);
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
        walkability_vector_ = Position(static_cast<int>(cos(theta) * distance_metric_), static_cast<int>(sin(theta) * distance_metric_)); // either {x,y}->{-y,x} or {x,y}->{y,-x} to rotate
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

Position Mobility::getVectorTowardsField(const vector<vector<int>> &field) const {
    Position return_vector = Positions::Origin;
    int my_spot = CUNYAIModule::currentMapInventory.getFieldValue(pos_, field);
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
    int my_spot = CUNYAIModule::currentMapInventory.getFieldValue(pos_, field);
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

bool Mobility::moveTo(const Position &start, const Position &finish, const StoredUnit::Phase phase)
{
    bool unit_sent = false;
    if (!start.isValid() || !finish.isValid()) {
        return false;
    }

    if (!unit_->isFlying()) {
        //First, let us try to get there with JPS.
        BWEB::Path newPath;
        newPath.createUnitPath(start, finish);
        if (newPath.isReachable() && !newPath.getTiles().empty()) {
            // lurker fix
            if (u_type_ == UnitTypes::Zerg_Lurker && unit_->isBurrowed() && !CUNYAIModule::getClosestThreatOrTargetStored(CUNYAIModule::enemy_player_model.units_, unit_, max(UnitTypes::Zerg_Lurker.groundWeapon().maxRange(), CUNYAIModule::enemy_player_model.units_.max_range_))) {
                unit_->unburrow();
                return CUNYAIModule::updateUnitPhase(unit_, StoredUnit::Phase::PathingOut);
            }
            else {
                unit_sent = unit_->move(Position(newPath.getTiles()[0]) + Position(16, 16));
            }
        }

        // Then let us try CPP.
        if (!unit_sent) {
            int plength = 0;
            auto cpp = BWEM::Map::Instance().GetPath(start, finish, &plength);
            if (!cpp.empty() && plength > 0) {
                // first try traveling with CPP.
                UnitInventory friendly_blocks = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, Position(cpp.front()->Center()), 64);
                friendly_blocks.updateUnitInventorySummary();
                bool has_a_blocking_item = (BWEM::Map::Instance().GetTile(TilePosition(cpp.front()->Center())).GetNeutral() || BWEM::Map::Instance().GetTile(TilePosition(cpp.front()->Center())).Doodad() || friendly_blocks.building_count_ > 0);
                bool too_close = Position(cpp.front()->Center()).getApproxDistance(unit_->getPosition()) < 32 * (2 + 3.5 * has_a_blocking_item);
                if (!too_close && cpp.size() >= 1)  unit_sent = unit_->move(Position(cpp[0]->Center())); // if you're not too close, get closer.
                if (too_close && cpp.size() > 1) unit_sent = unit_->move(Position(cpp[1]->Center())); // if you're too close to one choke point, move to the next one!
                //if (too_close && cpp.size() == 1) continue; // we're too close too the end of the CPP. Congratulations!  now use your local pathing.
            }
        }
    }

    // then try traveling with local travel. 
    if (!unit_sent) unit_sent = local_pathing(finish, phase);


    return unit_sent;
}

Position Mobility::getNextWaypoint(const Position &start, const Position &finish)
{
    int plength = 0;
    Position waypoint = Positions::Invalid;
    if (!start.isValid() || !finish.isValid()) {
        return waypoint;
    }

    BWEB::Path newPath;
    newPath.createUnitPath(start, finish);
    if (newPath.isReachable() && !unit_->isFlying() && !newPath.getTiles().empty()) {
        waypoint = Position(newPath.getTiles()[0]);
    }

    return waypoint;
}


int Mobility::getDistanceMetric()
{
    return static_cast<int>(distance_metric_);
}

bool Mobility::isOnDifferentHill(const StoredUnit &e) {
    int altitude = BWEM::Map::Instance().GetMiniTile(WalkPosition(e.pos_)).Altitude();
    return stored_unit_->areaID_ != e.areaID_ && (stored_unit_->elevation_ != e.elevation_ && stored_unit_->elevation_ % 2 != 0 && e.elevation_ % 2 != 0) && altitude + 96 < CUNYAIModule::getFunctionalRange(unit_);
}

Unit Mobility::pickTarget(int MaxDiveDistance, UnitInventory & ui) {
    Unit target = nullptr;
    int dist_to_enemy = 0;
    int target_surviablity = INT_MAX;
    for (auto t : ui.unit_map_) {
        dist_to_enemy = unit_->getDistance(t.second.pos_);
        bool baseline_requirement = (!isOnDifferentHill(t.second) || stored_unit_->is_flying_) && CUNYAIModule::Can_Fight(u_type_, t.second.type_);
        if (t.second.future_fap_value_ <= target_surviablity && dist_to_enemy <= MaxDiveDistance && baseline_requirement) {
            MaxDiveDistance = dist_to_enemy;
            target_surviablity = t.second.future_fap_value_;  //attack most likely to die, not closest!
            target = t.first;
        }
    }
    return target;
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

double getEnemySpeed(Unit e) {
    return sqrt(pow(e->getVelocityX(),2) + pow(e->getVelocityY(),2));
}

Position getEnemyVector(Unit e) {
    return Position(static_cast<int>(e->getVelocityX()), static_cast<int>(e->getVelocityY()));
}

Position getEnemyUnitaryVector(Unit e) {

}

Position Mobility::getVectorToEnemyDestination(Unit e) {
    Position his_destination = e->getPosition() + getEnemyVector(e);
    return his_destination - pos_;
}

Position Mobility::getVectorToBeyondEnemy(Unit e) {
    Position p = getVectorToEnemyDestination(e);
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