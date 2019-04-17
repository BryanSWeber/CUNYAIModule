#pragma once

# include "Source\CUNYAIModule.h"
# include "Source\Fight_MovementManager.h"
# include <random> // C++ base random is low quality.
# include <numeric>
# include <math.h>

#define TOO_FAR_FROM_FRONT (CUNYAIModule::current_map_inventory.getRadialDistanceOutFromEnemy(pos_) > (CUNYAIModule::friendly_player_model.closest_ground_combatant_ + 3.0 * 0.125 * distance_metric_ )); //radial distance is in minitiles, distance is in pixels.
//#define DISTANCE_METRIC (2.760 * 24.0);

using namespace BWAPI;
using namespace Filter;
using namespace std;


//Forces a unit to stutter in a Mobility manner. Size of stutter is unit's (vision range * n ). Will attack if it sees something.  Overlords & lings stop if they can see minerals.

void Mobility::Pathing_Movement(const int &passed_distance, const Position &e_pos) {

    Unit_Inventory local_neighborhood = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, pos_, 250);
    local_neighborhood.updateUnitInventorySummary();
    bool pathing_confidently = false;

    bool healthy = unit_->getHitPoints() > 0.25 * u_type_.maxHitPoints();
    bool ready_to_fight = CUNYAIModule::checkSuperiorFAPForecast2(CUNYAIModule::friendly_player_model.units_, CUNYAIModule::enemy_player_model.units_, false);
    bool enemy_scouted = CUNYAIModule::enemy_player_model.units_.getMeanBuildingLocation() != Positions::Origin;
    bool scouting_returned_nothing = CUNYAIModule::current_map_inventory.checked_all_expo_positions_ && !enemy_scouted;
    bool too_far_away_from_front_line = TOO_FAR_FROM_FRONT;

    if (u_type_ == UnitTypes::Zerg_Overlord) { // If you are an overlord float about as safely as possible.

        if (!ready_to_fight) { // Otherwise, return to safety.
            setRepulsionField(CUNYAIModule::current_map_inventory.pf_aa_, CUNYAIModule::current_map_inventory.safe_base_);
        }
        else {
            setRepulsionField(CUNYAIModule::current_map_inventory.pf_aa_, CUNYAIModule::current_map_inventory.safe_base_);
            setAttractionField(CUNYAIModule::current_map_inventory.pf_explore_, CUNYAIModule::current_map_inventory.safe_base_);
            pathing_confidently = true;
            Unit_Inventory e_neighborhood = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::enemy_player_model.units_, pos_, 250);
            e_neighborhood.updateUnitInventorySummary();

            if (e_neighborhood.stock_shoots_up_ > 0) {
                setSeperationScout(e_neighborhood);
            }
            else {
                setSeperationScout(local_neighborhood);
                pathing_confidently = true;
            }
        }
    }
    else {
        // Units should head towards enemies when there is a large gap in our knowledge, OR when it's time to pick a fight.
        if (healthy && (ready_to_fight || too_far_away_from_front_line)) {
            if (u_type_.airWeapon() != WeaponTypes::None) setAttractionMap(CUNYAIModule::current_map_inventory.map_out_from_enemy_air_, CUNYAIModule::current_map_inventory.enemy_base_air_);
            else setAttractionMap(CUNYAIModule::current_map_inventory.map_out_from_enemy_ground_, CUNYAIModule::current_map_inventory.enemy_base_ground_);
            pathing_confidently = true;
        }
        else { // Otherwise, return to home.
            setAttractionMap(CUNYAIModule::current_map_inventory.map_out_from_safety_, CUNYAIModule::current_map_inventory.safe_base_);
        }

        int average_side = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit_)->second.circumference_ / 4;
        Unit_Inventory neighbors = CUNYAIModule::getUnitInventoryInRadius(local_neighborhood, pos_, 32 + average_side * 2);
        if (u_type_ != UnitTypes::Zerg_Mutalisk) setSeperation(neighbors);

        if (healthy && scouting_returned_nothing) { // If they don't exist, then wander about searching. Overwrites natural seperation.
            setSeperationScout(local_neighborhood); //This is triggering too often and your army is scattering, not everything else.  
            pathing_confidently = true;
        }

        setCohesion(local_neighborhood);
    }

    //Avoidance vector:
    Position avoidance_vector = stutter_vector_ + cohesion_vector_ - seperation_vector_ + attune_vector_ - walkability_vector_ + attract_vector_ + repulse_vector_ + centralization_vector_;
    Position avoidance_pos = pos_ + avoidance_vector;

    //Which way should we avoid objects?
    if (pathing_confidently && u_type_.airWeapon() != WeaponTypes::None) setObjectAvoid(pos_, avoidance_pos, CUNYAIModule::current_map_inventory.map_out_from_enemy_air_);
    else if (pathing_confidently) setObjectAvoid(pos_, avoidance_pos, CUNYAIModule::current_map_inventory.map_out_from_enemy_ground_);
    else setObjectAvoid(pos_, avoidance_pos, CUNYAIModule::current_map_inventory.map_out_from_safety_);

    //Move to the final position.
    Position final_vector = avoidance_vector - walkability_vector_;
    //Make sure the end destination is one suitable for you.
    Position final_pos = pos_ + final_vector; //attract is zero when it's not set.
    
    //If you're not starting or ending too close to the "bad guys" you can continue to path.
    if ( final_pos != pos_ && final_pos.getDistance(e_pos) > passed_distance && pos_.getDistance(e_pos) > passed_distance) {

        // lurkers should move when we need them to scout.
        if (u_type_ == UnitTypes::Zerg_Lurker && unit_->isBurrowed() && !CUNYAIModule::getClosestThreatOrTargetStored(CUNYAIModule::enemy_player_model.units_, unit_, max(UnitTypes::Zerg_Lurker.groundWeapon().maxRange(), CUNYAIModule::enemy_player_model.units_.max_range_))) {
            unit_->unburrow();
            Stored_Unit& changing_unit = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit_)->second;
            changing_unit.phase_ = pathing_confidently ? "Pathing Out" : "Pathing Home";
            changing_unit.updateStoredUnit(unit_);
            return;
        }

        unit_->move(final_pos);
        Position last_out1 = Positions::Origin; // Could be a better way to do this, but here's a nice test case of the problem:
        Position last_out2 = Positions::Origin;

        //#include <iostream>
        //using namespace std;
        //int sample_fun(int X, int Y) { return X + Y; };
        //int main()
        //{
        //    cout << "Hello World";
        //    int Z = 4;
        //    int out = sample_fun(Z, Z += 1);
        //    cout << " We got:"; // 10. So it redefines first.
        //    cout << out;
        //}


        CUNYAIModule::Diagnostic_Line(pos_, last_out1 = pos_ + retreat_vector_, CUNYAIModule::current_map_inventory.screen_position_, Colors::White);//Run directly away
        CUNYAIModule::Diagnostic_Line(last_out1, last_out2 = last_out1 + attune_vector_, CUNYAIModule::current_map_inventory.screen_position_, Colors::Red);//Alignment
        CUNYAIModule::Diagnostic_Line(last_out2, last_out1 = last_out2 + centralization_vector_, CUNYAIModule::current_map_inventory.screen_position_, Colors::Blue); // Centraliziation.
        CUNYAIModule::Diagnostic_Line(last_out1, last_out2 = last_out1 + cohesion_vector_, CUNYAIModule::current_map_inventory.screen_position_, Colors::Purple); // Cohesion
        CUNYAIModule::Diagnostic_Line(last_out2, last_out1 = last_out2 + attract_vector_, CUNYAIModule::current_map_inventory.screen_position_, Colors::Green); //Attraction towards attackable enemies or home base.
        CUNYAIModule::Diagnostic_Line(last_out1, last_out2 = last_out1 + repulse_vector_, CUNYAIModule::current_map_inventory.screen_position_, Colors::Black); //Repulsion towards attackable enemies or home base.
        CUNYAIModule::Diagnostic_Line(last_out2, last_out1 = last_out2 - seperation_vector_, CUNYAIModule::current_map_inventory.screen_position_, Colors::Orange); // Seperation, does not apply to fliers.
        CUNYAIModule::Diagnostic_Line(last_out1, last_out2 = last_out1 - walkability_vector_, CUNYAIModule::current_map_inventory.screen_position_, Colors::Cyan); // Push from unwalkability, different unwalkability, different 

        Stored_Unit& changing_unit = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit_)->second;
        changing_unit.phase_ = pathing_confidently ? "Pathing Out" : "Pathing Home";
        changing_unit.updateStoredUnit(unit_);
        return;
    }

    // If you end too close to the bad guys, hold position.
    if (final_pos != pos_ && final_pos.getDistance(e_pos) < passed_distance && pos_.getDistance(e_pos) > passed_distance) {
        Stored_Unit& changing_unit = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit_)->second;
        unit_->holdPosition();
        changing_unit.phase_ = "Surrounding";
        changing_unit.updateStoredUnit(unit_);
        return;
    }

    // If you start too close to the bad guys, we have other issues.
    if (final_pos != pos_ && final_pos.getDistance(e_pos) < passed_distance && pos_.getDistance(e_pos) < passed_distance) {
        CUNYAIModule::DiagnosticText("We've been overtaken...");
        // This option should never happen! It ought to trigger retreat/attack.
        return;
    }
}

bool Mobility::BWEM_Movement() const
{
    bool pathing_confidently = false;

    bool healthy = unit_->getHitPoints() > 0.25 * u_type_.maxHitPoints();
    bool ready_to_fight = CUNYAIModule::checkSuperiorFAPForecast2(CUNYAIModule::friendly_player_model.units_, CUNYAIModule::enemy_player_model.units_, false);
    bool enemy_scouted = CUNYAIModule::enemy_player_model.units_.getMeanBuildingLocation() != Positions::Origin;
    bool scouting_returned_nothing = CUNYAIModule::current_map_inventory.checked_all_expo_positions_ && !enemy_scouted;
    bool too_far_away_from_front_line = TOO_FAR_FROM_FRONT;

    auto retreat_path = BWEM::Map::Instance().GetPath(pos_, CUNYAIModule::current_map_inventory.safe_base_);
    auto ground_attack_path = BWEM::Map::Instance().GetPath(pos_, CUNYAIModule::current_map_inventory.enemy_base_ground_);
    auto air_attack_path = BWEM::Map::Instance().GetPath(pos_, CUNYAIModule::current_map_inventory.enemy_base_air_);

    bool it_worked = false;

    if (u_type_ != UnitTypes::Zerg_Overlord) { // If you are an overlord float about as safely as possible.
        // Units should head towards enemies when there is a large gap in our knowledge, OR when it's time to pick a fight.
        if (healthy && (ready_to_fight || too_far_away_from_front_line)) {
            if (u_type_.airWeapon() != WeaponTypes::None) { 
                it_worked = move_to_next(air_attack_path);
            }
            else it_worked = move_to_next(ground_attack_path);
            pathing_confidently = true;
        }
        else { // Otherwise, return to home.
            it_worked = move_to_next(retreat_path);
        }

    }


    if (it_worked) {
        Stored_Unit& changing_unit = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit_)->second;
        changing_unit.phase_ = pathing_confidently ? "Pathing Out" : "Pathing Home";
        changing_unit.updateStoredUnit(unit_);
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
    double limit_units_diving = weak_enemy_or_small_armies ? 10 : 10 * log(helpful_e - helpful_u);
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

                if (CUNYAIModule::Can_Fight(e->second, unit_) && critical_target && dist_to_enemy <= max_diveable_dist && !lurkers_diving) {
                    e_priority = 7;
                }
                else if (critical_target) {
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


                if (e_priority >= priority && e_priority >= 3 && dist_to_enemy < max_dist) { // closest target of equal priority, or target of higher priority. Don't hop to enemies across the map when there are undefended things to destroy here.
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
        Stored_Unit& changing_unit = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit_)->second;
        changing_unit.phase_ = "Attacking";
        changing_unit.updateStoredUnit(unit_);
    } 
    else {
        Stored_Unit* closest = CUNYAIModule::getClosestThreatOrTargetStored(ei, unit_);
        if (closest) {
            encircle(closest->pos_);
            unit_->move(pos_ + encircle_vector_);
            CUNYAIModule::Diagnostic_Line(pos_, pos_ + encircle_vector_, CUNYAIModule::current_map_inventory.screen_position_, Colors::White);//Run around it.
        }
        else {
            unit_->holdPosition();
        }
        Stored_Unit& changing_unit = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit_)->second;
        changing_unit.phase_ = "Surrounding";
        changing_unit.updateStoredUnit(unit_);
    }// if I'm not attacking and I'm in range, I'm 'surrounding'
    return;
}


//Essentially, we would like to call the movement script BUT disable any attraction to the enemy since we are trying to only surround.
//void Mobility::Surrounding_Movement(const Unit & unit, const Unit_Inventory & ui, Unit_Inventory & ei, const Map_Inventory & inv){
//}

// Basic retreat logic
void Mobility::Retreat_Logic(const Stored_Unit &e_unit, const Unit_Inventory &u_squad, Unit_Inventory &e_squad, Unit_Inventory &ei, const Unit_Inventory &ui, const int &passed_distance, const Color &color = Colors::White, const bool &force = false) {

    int dist = unit_->getDistance(e_unit.pos_);
   
    int e_range = ei.max_range_;
    //int f_range = ui.max_range_;
    bool order_sent = false;
    Unit_Inventory e_squad_threatening = CUNYAIModule::getThreateningUnitInventoryInRadius(e_squad, pos_, 999, unit_->isFlying());
    // If there are bad guys nearby, run from the immediate threat, otherwise run home.
    if (CUNYAIModule::getThreateningStocks(unit_, e_squad) > 0) {
        // All units seperate from nearby enemy units- threat or not.
        setSeperation(e_squad_threatening);
        // flying units repulse from their air units since they can kite nearly indefinently, ground units head to the safest possible place.
        if (e_unit.is_flying_) setRepulsionMap(CUNYAIModule::current_map_inventory.map_out_from_enemy_air_, CUNYAIModule::current_map_inventory.enemy_base_air_);
        else setAttractionMap(CUNYAIModule::current_map_inventory.map_out_from_safety_, CUNYAIModule::current_map_inventory.safe_base_);
    }
    else {
        setAttractionMap(CUNYAIModule::current_map_inventory.map_out_from_safety_, CUNYAIModule::current_map_inventory.safe_base_); // otherwise a flying unit will be saticated by simply not having a dangerous weapon directly under them.
    }
    
    //Avoidance vector:
    Position avoidance_vector = stutter_vector_ + cohesion_vector_ - seperation_vector_ + attune_vector_ - walkability_vector_ + attract_vector_ + centralization_vector_ + retreat_vector_;
    Position avoidance_pos = pos_ + avoidance_vector;
    setObjectAvoid(pos_, avoidance_pos, CUNYAIModule::current_map_inventory.map_out_from_safety_);

    //final vector
    Position final_vector = avoidance_vector - walkability_vector_;

    //Make sure the end destination is one suitable for you.
    Position retreat_spot = pos_ + final_vector; //attract is zero when it's not set.

    //bool clear_walkable = retreat_spot.isValid() &&
    //    (unit->isFlying() || // can I fly, rendering the idea of walkablity moot?
    //        CUNYAIModule::isClearRayTrace(pos, retreat_spot, CUNYAIModule::current_map_inventory.unwalkable_barriers_with_buildings_, 1)); //or does it cross an unwalkable position? Includes buildings.
    bool cooldown = unit_->getGroundWeaponCooldown() > 0 || unit_->getAirWeaponCooldown() > 0;
    bool kiting = !cooldown && dist < 64 && CUNYAIModule::getProperRange(unit_) > 64 && CUNYAIModule::getProperRange(e_unit.bwapi_unit_) < 64 && CUNYAIModule::Can_Fight(e_unit, unit_); // only kite if he's in range,

    //bool scourge_retreating = unit->getType() == UnitTypes::Zerg_Scourge && dist < e_range;
    //bool unit_death_in_moments = Stored_Unit::unitAliveinFuture(ui.unit_inventory_.at(unit), 96); 
    //bool squad_death_in_moments = u_squad.squadAliveinFuture(96); 
    //bool never_suicide = unit->getType() == UnitTypes::Zerg_Mutalisk || unit->getType() == UnitTypes::Zerg_Overlord || unit->getType() == UnitTypes::Zerg_Drone;
    bool melee_fight = CUNYAIModule::getProperRange(unit_) < 64 && e_squad.max_range_ < 64;
    bool is_retreating = false;

    if ( force || ( retreat_spot && !kiting /*&& !(unit_death_in_moments && squad_death_in_moments && clear_walkable && melee_fight)*/ ) ) {
        if (u_type_ == UnitTypes::Zerg_Lurker && unit_->isBurrowed() && unit_->isDetected() && ei.stock_ground_units_ == 0) {
            unit_->unburrow();
            is_retreating = true;
        }
        else if (unit_->move(retreat_spot)) { //run away.  Don't need immobile units retreating.
            Position last_out1 = Positions::Origin; // Could be a better way to do this, but here's a nice test case of the problem:
            Position last_out2 = Positions::Origin;

            CUNYAIModule::Diagnostic_Line(pos_, last_out1 = pos_ + retreat_vector_, CUNYAIModule::current_map_inventory.screen_position_, Colors::White);//Run directly away
            CUNYAIModule::Diagnostic_Line(last_out1, last_out2 = last_out1 + attune_vector_, CUNYAIModule::current_map_inventory.screen_position_, Colors::Red);//Alignment
            CUNYAIModule::Diagnostic_Line(last_out2, last_out1 = last_out2 + centralization_vector_, CUNYAIModule::current_map_inventory.screen_position_, Colors::Blue); // Centraliziation.
            CUNYAIModule::Diagnostic_Line(last_out1, last_out2 = last_out1 + cohesion_vector_, CUNYAIModule::current_map_inventory.screen_position_, Colors::Purple); // Cohesion
            CUNYAIModule::Diagnostic_Line(last_out2, last_out1 = last_out2 + attract_vector_, CUNYAIModule::current_map_inventory.screen_position_, Colors::Green); //Attraction towards attackable enemies or home base.
            CUNYAIModule::Diagnostic_Line(last_out1, last_out2 = last_out1 - seperation_vector_, CUNYAIModule::current_map_inventory.screen_position_, Colors::Orange); // Seperation, does not apply to fliers.
            CUNYAIModule::Diagnostic_Line(last_out2, last_out1 = last_out2 - walkability_vector_, CUNYAIModule::current_map_inventory.screen_position_, Colors::Cyan); // Push from unwalkability, different 
            is_retreating = true;
        }

        if (is_retreating) {
            Stored_Unit& changing_unit = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit_)->second;
            changing_unit.phase_ = "Retreating";
            changing_unit.updateStoredUnit(unit_);
            //if (retreat_spot.getDistance(pos) < 32) CUNYAIModule::DiagnosticText("Hey, this was a very small retreat order!");

            return;
        }
    }
    else {
        Stored_Unit& changing_unit = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit_)->second;
        changing_unit.phase_ = "No Retreat.";
        changing_unit.updateStoredUnit(unit_);
        return;
    }// if I'm not retreating, I'm announcing I'm bugged.

}


//Brownian Stuttering, causes unit to move about randomly.
Position Mobility::setStutter(const double &n) {

    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<double> dis(-1, 1);

    // The naieve approach of rand()%3 - 1 is "distressingly non-random in lower order bits" according to stackoverflow and other sources.

    return stutter_vector_ = Position(static_cast<int>(32.0 * n * dis(gen)), static_cast<int>(32.0 * n * dis(gen)) );
}

//Alignment. Convinces all units in unit inventory to move at similar velocities.
Position Mobility::setAlignment(const Unit_Inventory &ui) {
    double temp_tot_x = 0;
    double temp_tot_y = 0;
    double speed = CUNYAIModule::getProperSpeed(u_type_);
    int flock_count = 0;
    if (!ui.unit_map_.empty()) {
        for (auto i = ui.unit_map_.begin(); i != ui.unit_map_.end() && !ui.unit_map_.empty(); ++i) {
            if (i->second.type_ != UnitTypes::Zerg_Drone && i->second.type_ != UnitTypes::Zerg_Overlord && i->second.type_ != UnitTypes::Buildings) {
                temp_tot_x += i->second.bwapi_unit_->getVelocityX(); //get the horiz element.
                temp_tot_y += i->second.bwapi_unit_->getVelocityY(); // get the vertical element. Averaging angles was trickier than I thought. 

                flock_count++;
            }
        }
        //double theta = atan2( temp_tot_y - unit->getVelocityY() , temp_tot_x - unit->getVelocityX() );  // subtract out the unit's personal heading.

        if (flock_count > 1) {
            int x = static_cast<int> ( ((temp_tot_x - unit_->getVelocityX()) / static_cast<double>(flock_count - 1)) + unit_->getVelocityX() );
            int y = static_cast<int> ( ((temp_tot_y - unit_->getVelocityY()) / static_cast<double>(flock_count - 1)) + unit_->getVelocityY() );
            return attune_vector_ =  Position( x , y ); // the velocity is per frame, I'd prefer it per second so its scale is 
        }
        else {
            return attune_vector_ = Position( static_cast<int>(cos(unit_->getAngle()) * speed * 6.0) , static_cast<int>(sin(unit_->getAngle()) * speed * 6.0) );
        }
    }
}

Position Mobility::setDirectRetreat(const Position &e_pos) {
    int dist_x = e_pos.x - pos_.x;
    int dist_y = e_pos.y - pos_.y;
    double theta = atan2(dist_y, dist_x); // att_y/att_x = tan (theta).
    return retreat_vector_ = Position ( static_cast<int>(-cos(theta) * distance_metric_ * 4.0), static_cast<int>(-sin(theta) * distance_metric_ * 4.0)); // get slightly away from opponent.
}

//Centralization, all units prefer sitting along map veins to edges.
//void Mobility::setCentralize(const Position &pos, const Map_Inventory &inv) {
//    double temp_centralization_dx_ = 0;
//    double temp_centralization_dy_ = 0;
//    WalkPosition map_dim = WalkPosition(TilePosition({ Broodwar->mapWidth(), Broodwar->mapHeight() }));
//    for (int x = -5; x <= 5; ++x) {
//        for (int y = -5; y <= 5; ++y) {
//            int mini_x = WalkPosition(pos).x;
//            int mini_y = WalkPosition(pos).y;
//            double centralize_x = mini_x + x;
//            double centralize_y = mini_y + y;
//            if (!(x == 0 && y == 0) &&
//                centralize_x < map_dim.x &&
//                centralize_y < map_dim.y &&
//                centralize_x > 0 &&
//                centralize_y > 0 &&
//                (inventory.map_veins_[centralize_x][centralize_y] > inventory.map_veins_[mini_x][mini_y] /*|| inventory.map_veins_[centralize_x][centralize_y] > 20*/))
//            {
//                double theta = atan2(y, x);
//                temp_centralization_dx_ += static_cast<int>(cos(theta));
//                temp_centralization_dy_ += static_cast<int>(sin(theta));
//            }
//        }
//    }
//    if (temp_centralization_dx_ != 0 && temp_centralization_dy_ != 0) {
//        double theta = atan2(temp_centralization_dy_, temp_centralization_dx_);
//        centralization_dx_ = cos(theta) * distance_metric * 0.125;
//        centralization_dy_ = sin(theta) * distance_metric * 0.125;
//    }
//}

//Cohesion, all units tend to prefer to be together.
Position Mobility::setCohesion(const Unit_Inventory &ui) {
    cohesion_vector_ = Positions::Origin;
    const Position loc_center = ui.getMeanArmyLocation();
    if (loc_center != Positions::Origin) {
        double cohesion_x = loc_center.x - pos_.x;
        double cohesion_y = loc_center.y - pos_.y;
        double theta = atan2(cohesion_y, cohesion_x);
        cohesion_vector_ = Position(static_cast<int>(cos(theta) * 0.25 * distance_metric_), static_cast<int>(sin(theta) * 0.25 * distance_metric_) );
    }
    return cohesion_vector_;
}

Position Mobility::encircle(const Position & p) {
    Position vector_to = p - pos_;
    double theta = atan2(vector_to.y, vector_to.x);
    Position encircle_left = Position(static_cast<int>(-sin(theta) * 0.25 * distance_metric_), static_cast<int>(cos(theta) * 0.25 * distance_metric_)); // either {x,y}->{-y,x} or {x,y}->{y,-x} to rotate
    Position encircle_right = Position(static_cast<int>(sin(theta) * 0.25 * distance_metric_), static_cast<int>(-cos(theta) * 0.25 * distance_metric_)); // either {x,y}->{-y,x} or {x,y}->{y,-x} to rotate
    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<double> dis(0, 1);    // default values for output.

    return encircle_vector_ = (dis(gen) > 0.5) ? encircle_left : encircle_right; // only one direction for now.
}

Position Mobility::scoutEnemyBase(Map_Inventory &inv) {
    if (!CUNYAIModule::current_map_inventory.start_positions_.empty() && find(CUNYAIModule::current_map_inventory.start_positions_.begin(), CUNYAIModule::current_map_inventory.start_positions_.end(), unit_->getLastCommand().getTargetPosition()) == CUNYAIModule::current_map_inventory.start_positions_.end()) {
        Position possible_base = CUNYAIModule::current_map_inventory.start_positions_[0];
        double dist = static_cast<double>(unit_->getDistance(possible_base));
        int dist_x = possible_base.x - pos_.x;
        int dist_y = possible_base.y - pos_.y;
        double theta = atan2(dist_y, dist_x);

        Position stutter_vector_ = Positions::Origin;
        Position attune_vector_ = Positions::Origin;
        Position cohesion_vector_ = Positions::Origin;
        Position centralization_vector_ = Positions::Origin;
        Position seperation_vector_ = Positions::Origin;
        //Position attract_vector_ = Positions::Origin;
        Position retreat_vector_ = Positions::Origin;
        Position walkability_vector_ = Positions::Origin;

        std::rotate(CUNYAIModule::current_map_inventory.start_positions_.begin(), CUNYAIModule::current_map_inventory.start_positions_.begin() + 1, CUNYAIModule::current_map_inventory.start_positions_.end());

        return attract_vector_ = Position(static_cast<int>(cos(theta) * dist), static_cast<int>(sin(theta) * dist)); // run 100% towards them.
    }
}

//Attraction, pull towards map center.
Position Mobility::setAttractionMap(const vector<vector<int>> &map, const Position &map_center) {
    attract_vector_ = Positions::Origin;
    if (map.empty() || unit_->isFlying()) {
        int dist_x = map_center.x - pos_.x;
        int dist_y = map_center.y - pos_.y;
        double theta = atan2(dist_y, dist_x);
        attract_vector_ = Position(static_cast<int>(cos(theta) * distance_metric_), static_cast<int>(sin(theta) * distance_metric_));  // run to (map)!
    }
    else {
        attract_vector_ = getVectorTowardsMap(map); // move downhill (vector times scalar)
    }
    return attract_vector_;
}

//Attraction, pull towards higher values.
Position Mobility::setAttractionField(const vector<vector<int>> &field, const Position &map_center) {
    attract_vector_ = Positions::Origin;
    if (field.empty()) {
        int dist_x = map_center.x - pos_.x;
        int dist_y = map_center.y - pos_.y;
        double theta = atan2(dist_y, dist_x);
        attract_vector_ = Position(static_cast<int>(-cos(theta) * distance_metric_), static_cast<int>(-sin(theta) * distance_metric_)); // run to (map)!
    }
    else {
        attract_vector_ = getVectorTowardsField(field);
    }
    return attract_vector_;
}

//Repulsion, pull away from map center. Literally just a negative of the previous.
Position Mobility::setRepulsionMap(const vector<vector<int>> &map, const Position &map_center) {
    repulse_vector_ = Positions::Origin;
    if (map.empty() || unit_->isFlying()) {
        int dist_x = map_center.x - pos_.x;
        int dist_y = map_center.y - pos_.y;
        double theta = atan2(dist_y, dist_x);
        repulse_vector_ = Position(static_cast<int>(-cos(theta) * distance_metric_), static_cast<int>(-sin(theta) * distance_metric_ )); // run to (map)!
    }
    else {
        Position direction = getVectorTowardsMap(map);
        repulse_vector_ = Position(-direction.x, -direction.y); // move uphill. (invert previous direction) Don't use, seems buggy!
    }
    return repulse_vector_;
}

//Repulsion, pull away from field values. Literally just a negative of the previous.
Position Mobility::setRepulsionField(const vector<vector<int>> &field, const Position &map_center) {
    repulse_vector_ = Positions::Origin;
    if (field.empty()) {
        int dist_x = map_center.x - pos_.x;
        int dist_y = map_center.y - pos_.y;
        double theta = atan2(dist_y, dist_x);
        repulse_vector_ = Position(static_cast<int>(-cos(theta) * distance_metric_), static_cast<int>(-sin(theta) * distance_metric_)); // run to (map)!
    }
    else {
        repulse_vector_ = getVectorAwayField(field);
    }
    return repulse_vector_;
}


//Seperation from nearby units, search very local neighborhood.
Position Mobility::setSeperation(const Unit_Inventory &ui) {
    int seperation_x = 0;
    int seperation_y = 0;
    int unit_count = 1;
    seperation_vector_ = Positions::Origin;
    for (auto &u : ui.unit_map_) { // don't seperate from yourself, that would be a disaster.
        if (unit_ != u.first && (unit_->isFlying() == u.second.is_flying_) ) { // only seperate if the unit is on the same plane.
            seperation_x += u.second.pos_.x - pos_.x;
            seperation_y += u.second.pos_.y - pos_.y;
            unit_count++;
        }
    }

    if (seperation_y != 0 || seperation_x != 0) {
        double theta = atan2(seperation_y, seperation_x);
        seperation_vector_ = Position(static_cast<int>(cos(theta) * 0.75 * distance_metric_)  , static_cast<int>(sin(theta) * 0.75 * distance_metric_) ); // run away from everyone. Should help avoid being stuck in those wonky spots.
        //seperation_vector_ = Position(cos(theta) * seperation_x / unit_count, sin(theta) * seperation_y / unit_count); // run away from everyone. Should help avoid being stuck in those wonky spots.
    }
    return seperation_vector_;
}

//Seperation from nearby units, search very local neighborhood of 2 tiles.
Position Mobility::setSeperationScout(const Unit_Inventory &ui) {
    bool overlord_with_upgrades = u_type_ == UnitTypes::Zerg_Overlord && Broodwar->self()->getUpgradeLevel(UpgradeTypes::Antennae) > 0;
    double distance = (u_type_.sightRange() + overlord_with_upgrades * 2 * 32);
    int largest_dim = max(u_type_.height(), u_type_.width());
    Unit_Inventory neighbors = CUNYAIModule::getUnitInventoryInRadius(ui, pos_, static_cast<int>(distance * 2.0 + largest_dim));
    int seperation_x = 0;
    int seperation_y = 0;
    for (auto &u : neighbors.unit_map_) { // don't seperate from yourself, that would be a disaster.
        seperation_x += u.second.pos_.x - pos_.x;
        seperation_y += u.second.pos_.y - pos_.y;
    }

    // move away from map edge too, as if they were barriers. Don't waste vision. Note how the above is enemy_pos-our_pos, it will assist with the +/- below.
    seperation_x += pos_.x + distance > Broodwar->mapWidth() * 32;
    seperation_x -= pos_.x - distance < 0;
    seperation_y += pos_.y + distance > Broodwar->mapHeight() * 32;
    seperation_y -= pos_.y - distance < 0;

    if (seperation_y != 0 || seperation_x != 0) {
        double theta = atan2(seperation_y, seperation_x);
        return seperation_vector_ = Position(static_cast<int>(cos(theta) * distance * 2.0), static_cast<int>(sin(theta) * distance * 2.0)); // run sight ranges away from everyone. Should help avoid being stuck in those wonky spots.
    }
    return seperation_vector_ = Positions::Origin;
}

//Position Mobility::setObjectAvoid(const Unit &unit, const Position &current_pos, const Position &future_pos, const Map_Inventory &inv, const vector<vector<int>> &map) {
//        double temp_walkability_dx_ = 0;
//        double temp_walkability_dy_ = 0;
//        double theta = 0;
//        WalkPosition map_dim = WalkPosition(TilePosition({ Broodwar->mapWidth(), Broodwar->mapHeight() }));
//        //Position avoidance_pos = { 2 * future_pos.x - current_pos.x, 2 * future_pos.y - current_pos.y};
//        vector<Position> trial_positions = { current_pos, future_pos };
//
//        bool unwalkable_tiles = false;
//        pair<int, int> min_tile = { INT_MAX, 0 }; // Thie stile is the LEAST walkable.
//        pair<int, int> max_tile = { INT_MIN, 0 }; //this tile is the MOST walkable.
//        pair<int,int> temp_int = {0,0};
//        vector<WalkPosition> min_max_minitiles = { WalkPosition(future_pos), WalkPosition(future_pos) };
//
//        if (!unit->isFlying()) {
//            for (auto considered_pos : trial_positions) {
//                for (int x = -8; x <= 8; ++x) {
//                    for (int y = -8; y <= 8; ++y) {
//                        double centralize_x = WalkPosition(considered_pos).x + x;
//                        double centralize_y = WalkPosition(considered_pos).y + y;
//                        if (!(x == 0 && y == 0) &&
//                            centralize_x < map_dim.x &&
//                            centralize_y < map_dim.y &&
//                            centralize_x > 0 &&
//                            centralize_y > 0 &&
//                            centralize_y > 0) // Is the spot acceptable?
//                        {
//                            temp_int = { inventory.map_veins_[centralize_x][centralize_y], map[centralize_x][centralize_y] };
//                            if (temp_int.first <= 1 && !unwalkable_tiles) // repulse from unwalkable.
//                            {
//                                unwalkable_tiles = true;
//                            }
//                            if (temp_int.first < min_tile.first || (temp_int.first == min_tile.first && temp_int.second < min_tile.second)) {
//                                min_tile = temp_int;
//                                min_max_minitiles[0] = WalkPosition(centralize_x, centralize_y);
//                            }
//                            if (temp_int.first > max_tile.first || (temp_int.first == min_tile.first && temp_int.second < min_tile.second)) {
//                                max_tile = temp_int;
//                                min_max_minitiles[1] = WalkPosition(centralize_x, centralize_y);
//                            }
//                        }
//                    }
//                }
//                if (unwalkable_tiles) {
//                    double vector_push_x = min_max_minitiles[0].x - min_max_minitiles[1].x;
//                    double vector_push_y = min_max_minitiles[0].y - min_max_minitiles[1].y;
//
//                    double theta = atan2(future_pos.x, future_pos.y);
//                    //int when_did_we_stop = std::distance(trial_positions.begin(), std::find(trial_positions.begin(), trial_positions.end(), considered_pos)); // should go to 0,1,2.
//                    return walkability_vector_ += Position(static_cast<int>(cos(theta)) * vector_push_x * 4 , static_cast<int>(sin(theta)) * vector_push_y * 4 /** (3 - when_did_we_stop) / trial_positions.size()*/);
//                }
//            }
//        }
//
//}

Position Mobility::setObjectAvoid(const Position &current_pos, const Position &future_pos, const vector<vector<int>> &map) {
    double temp_walkability_dx_ = 0;
    double temp_walkability_dy_ = 0;
    double theta = 0;
    WalkPosition map_dim = WalkPosition(TilePosition({ Broodwar->mapWidth(), Broodwar->mapHeight() }));

    vector<Position> trial_positions = { current_pos, future_pos };

    bool unwalkable_tiles = false;
    vector<WalkPosition> unwalkable_minitiles;
    Position obstacle_found_near_this_position = Positions::Origin;
    int obstacle_x = 0;
    int obstacle_y = 0;

    if (!unit_->isFlying()) {
        for (auto considered_pos : trial_positions) {
            for (int x = -8; x <= 8; ++x) {
                for (int y = -8; y <= 8; ++y) {
                    int centralize_x = WalkPosition(considered_pos).x + x;
                    int centralize_y = WalkPosition(considered_pos).y + y;
                    if (!(x == 0 && y == 0) &&
                        centralize_x < map_dim.x &&
                        centralize_y < map_dim.y &&
                        centralize_x > 0 &&
                        centralize_y > 0 &&
                        centralize_y > 0) // Is the spot acceptable?
                    {
                        if (CUNYAIModule::current_map_inventory.unwalkable_barriers_[centralize_x][centralize_y] == 1) // repulse from unwalkable.
                        {
                            unwalkable_tiles = true;
                            unwalkable_minitiles.push_back(WalkPosition(centralize_x,centralize_y));
                        }
                    }
                }
            }
            if (unwalkable_tiles) {
                obstacle_found_near_this_position = considered_pos;
                break;
            }
        }

        if (unwalkable_tiles) {
            if (unwalkable_minitiles.size() > 0) {
                for (auto i : unwalkable_minitiles) {
                    obstacle_x += i.x;
                    obstacle_y += i.y;
                }
                obstacle_x /= unwalkable_minitiles.size();
                obstacle_y /= unwalkable_minitiles.size();

                int vector_push_x = obstacle_x - WalkPosition(obstacle_found_near_this_position).x;
                int vector_push_y = obstacle_y - WalkPosition(obstacle_found_near_this_position).y;

                //int when_did_we_stop = std::distance(trial_positions.begin(), std::find(trial_positions.begin(), trial_positions.end(), considered_pos)); // should go to 0,1,2.
                return walkability_vector_ = Position(vector_push_x * 6, vector_push_y * 6 /** (3 - when_did_we_stop) / trial_positions.size()*/);
            }
        }
    }
    return walkability_vector_ = Positions::Origin;
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

//Position Mobility::getVectorTowardsMap(const Position &pos, const Map_Inventory &inv, const vector<vector<int>> &map) const {
//    Position return_vector = Positions::Origin;
//    int my_spot = inv.getMapValue(pos, map);
//    double temp_x = 0;
//    double temp_y = 0;
//    double adj_x = 0;
//    double adj_y = 0;
//
//    double theta = 0;
//    WalkPosition map_dim = WalkPosition(TilePosition({ Broodwar->mapWidth(), Broodwar->mapHeight() }));
//    for (int x = -3; x <= 3; ++x) {
//        for (int y = -3; y <= 3; ++y) {
//            if (x != 3 && y != 3 && x != -3 && y != -3) continue;  // what was this added for? Only explore periphery for movement locations.
//            double centralize_x = WalkPosition(pos).x + x;
//            double centralize_y = WalkPosition(pos).y + y;
//            if (!(x == 0 && y == 0) &&
//                centralize_x < map_dim.x &&
//                centralize_y < map_dim.y &&
//                centralize_x > 0 &&
//                centralize_y > 0 &&
//                centralize_y > 0) // Is the spot acceptable?
//            {
//                theta = atan2(y, x);
//
//                if (inv.map_veins_[centralize_x][centralize_y] > 1 && // avoid buildings
//                    map[centralize_x][centralize_y] < my_spot) // go directly to my base.
//                {
//                    temp_x += static_cast<int>(cos(theta));
//                    temp_y += static_cast<int>(sin(theta));
//                }
//            }
//        }
//    }
//
//    if (temp_y != 0 || temp_x != 0) {
//        theta = atan2(temp_y + adj_y, temp_x + adj_x);
//        return_vector = Position(static_cast<int>(cos(theta)), static_cast<int>(sin(theta)));
//    }
//    return  return_vector;
//}


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

bool Mobility::move_to_next(const BWEM::CPPath &cpp) const
{
    if (cpp.size()) {
        bool too_close = Position(cpp.front()->Center()).getApproxDistance(unit_->getPosition()) < 32 * 4;
        if (too_close && cpp.size() >= 2) return unit_->move(Position(cpp[1]->Center())); // if you're too close to one choke point, move to the next one!
        else if (cpp.size() >= 2) return unit_->move(Position(cpp[0]->Center())); //Otherwise, go to the next choke point.
    }
    return false;
}
