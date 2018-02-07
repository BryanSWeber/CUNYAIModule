#pragma once

# include "Source\MeatAIModule.h"
# include "Source\Fight_MovementManager.h"
# include <random> // C++ base random is low quality.


using namespace BWAPI;
using namespace Filter;
using namespace std;

//Forces a unit to stutter in a boids manner. Size of stutter is unit's (vision range * n ). Will attack if it sees something.  Overlords & lings stop if they can see minerals.
void Boids::Boids_Movement( const Unit &unit, const double &n, const Unit_Inventory &ui, Unit_Inventory &ei, Inventory &inventory, const bool &army_starved ) {

    Position pos = unit->getPosition();

    bool armed = unit->getType().airWeapon() != WeaponTypes::None || unit->getType().groundWeapon() != WeaponTypes::None;
    bool healthy = unit->getHitPoints() > 0.25 * unit->getType().maxHitPoints();
    bool ready_to_fight = ei.stock_total_ <= ui.stock_total_;
    bool enemy_scouted = ei.getMeanBuildingLocation() != Position(0, 0);
    bool unit_safe_for_scouting = (ei.stock_shoots_up_ == 0 && unit->getType() == UnitTypes::Zerg_Overlord) || unit->getType() != UnitTypes::Zerg_Overlord;
    bool strong_cohesion_needed = true;
    Unit_Inventory local_neighborhood = MeatAIModule::getUnitInventoryInRadius(ui, unit->getPosition(), 1500);

    // Units should scout when there is a large gap in our knowledge.
    if ( !enemy_scouted && healthy && unit_safe_for_scouting && !inventory.start_positions_.empty() ) { // check his bases first.
        scoutEnemyBase(unit, pos, ei, inventory);
    }
    else if (!enemy_scouted && healthy && unit_safe_for_scouting && inventory.start_positions_.empty() ) { // then wander about searching.
        setStutter(unit, n);
        setSeperationScout( unit, pos, local_neighborhood );
        strong_cohesion_needed = false;
    }

    // Units should go to the enemy when it's time to pick a fight, home otherwise.
    if ( enemy_scouted && healthy && ready_to_fight && unit_safe_for_scouting) {
        setAttractionEnemy(unit, pos, ei, inventory); 
    }
    else if (enemy_scouted && (!healthy || !ready_to_fight || !unit_safe_for_scouting) ) {
        setAttractionHome(unit, pos, ei, inventory);
    }
   
    // Units should stick together if we're not scouting.
    if (strong_cohesion_needed) {
        setAlignment(unit, local_neighborhood);
        setCohesion(unit, pos, ui);
        if (!unit->getType().isFlyer() || unit->getType() == UnitTypes::Zerg_Scourge) {
            setSeperation(unit, pos, local_neighborhood);
        } // closure: flyers
    }

    // lurkers should move when we need them to scout.
    if ( unit->getType() == UnitTypes::Zerg_Lurker && unit->isBurrowed() && unit->getLastCommandFrame() < Broodwar->getFrameCount() - 7 && !MeatAIModule::getClosestThreatOrTargetStored(ei, UnitTypes::Zerg_Lurker,pos,max(UnitTypes::Zerg_Lurker.groundWeapon().maxRange(), ei.max_range_) ) ) {
        unit->unburrow();
        return;
    }

    //Move to the final position.
    Position brownian_pos = { (int)(pos.x + x_stutter_ + cohesion_dx_ - seperation_dx_ + attune_dx_ - walkability_dx_ + attract_dx_ + centralization_dx_), 
                              (int)(pos.y + y_stutter_ + cohesion_dy_ - seperation_dy_ + attune_dy_ - walkability_dy_ + attract_dy_ + centralization_dy_) };

    if (unit->getLastCommand().getTargetPosition() != brownian_pos) {
        unit->move(brownian_pos);
    }


    MeatAIModule::Diagnostic_Line( unit->getPosition(), { (int)(pos.x + x_stutter_)        , (int)(pos.y + y_stutter_) }, Colors::Black );//Stutter
    MeatAIModule::Diagnostic_Line( unit->getPosition(), { (int)(pos.x + attune_dx_)        , (int)(pos.y + attune_dy_) }, Colors::Green );//Alignment
    MeatAIModule::Diagnostic_Line( unit->getPosition(), { (int)(pos.x + centralization_dx_), (int)(pos.y + centralization_dy_) }, Colors::Blue ); // Centraliziation.
    MeatAIModule::Diagnostic_Line( unit->getPosition(), { (int)(pos.x + cohesion_dx_)      , (int)(pos.y + cohesion_dy_) }, Colors::Purple ); // Cohesion
    MeatAIModule::Diagnostic_Line( unit->getPosition(), { (int)(pos.x + attract_dx_)       , (int)(pos.y + attract_dy_) }, Colors::Red ); //Attraction towards attackable enemies.
    MeatAIModule::Diagnostic_Line( unit->getPosition(), { (int)(pos.x - seperation_dx_)    , (int)(pos.y - seperation_dy_) }, Colors::Orange ); // Seperation, does not apply to fliers.
    MeatAIModule::Diagnostic_Line( unit->getPosition(), { (int)(pos.x - walkability_dx_)   , (int)(pos.y - walkability_dy_) }, Colors::Cyan ); // Push from unwalkability, different regions. May tilt to become parallel with obstructions to get around them.

};

// This is basic combat logic for nonspellcasting units.
void Boids::Tactical_Logic( const Unit &unit, const Unit_Inventory &ei, const Unit_Inventory &ui, const Color &color = Colors::White )
{
    UnitType u_type = unit->getType();
    Stored_Unit target;
    int priority = 0;
     
    int chargeable_dist = MeatAIModule::getChargableDistance(unit, ei);
    int helpless_e = unit->isFlying() ? ei.stock_total_ - ei.stock_shoots_up_ : ei.stock_total_ - ei.stock_shoots_down_;
    int helpful_e = unit->isFlying() ? ei.stock_shoots_up_ : ui.stock_shoots_down_; // both forget value of psi units.
    int helpful_u = unit->isFlying() ? ui.stock_fliers_ : ui.stock_ground_units_;

    double limit_units_diving = ( helpful_e - helpful_u <= 1 ? 1 : log( helpful_e - helpful_u));
    int max_dist = ei.max_range_ / (double)limit_units_diving + chargeable_dist;
    int max_dist_no_priority = 9999999;
    bool target_sentinel = false;
    bool target_sentinel_poor_target_atk = false;

    for ( auto e = ei.unit_inventory_.begin(); e != ei.unit_inventory_.end() && !ei.unit_inventory_.empty(); ++e ) {
        if ( e->second.valid_pos_ ) {
            UnitType e_type = e->second.type_;
            int e_priority = 0;

            if ( MeatAIModule::Can_Fight( unit, e->second ) ) { // if we can fight this enemy and there is a clear path to them:
                int dist_to_enemy = unit->getDistance( e->second.pos_ );
                bool critical_target = e_type.groundWeapon().innerSplashRadius() > 0 ||
                    (e_type.isSpellcaster() && !e_type.isBuilding()) ||
                    (e_type.isDetector() && ui.cloaker_count_ > ei.detector_count_) ||
                    e_type == UnitTypes::Protoss_Carrier ||
                    (e->second.bwapi_unit_ && e->second.bwapi_unit_->exists() && e->second.bwapi_unit_->isRepairing()) ||
                    //(e->second.current_hp_ < 0.25 * e_type.maxHitPoints() && MeatAIModule::Can_Fight( e->second, unit )) ||
                    e_type == UnitTypes::Protoss_Reaver; // Prioritise these guys: Splash, crippled combat units

                if ( critical_target ) {
                    e_priority = 6;
                }
                else if (e->second.bwapi_unit_ && unit->getLastCommand().getTarget() && MeatAIModule::Can_Fight(e->second, unit) && unit->getDistance(e->second.bwapi_unit_) < chargeable_dist &&
                   ( unit->getLastCommand().getTarget() == e->second.bwapi_unit_  || 
                        (e->second.type_ == unit->getLastCommand().getTarget()->getType() && e->second.current_hp_ < unit->getLastCommand().getTarget()->getHitPoints() ) ) ) {
                    e_priority = 5;
                }
                else if (MeatAIModule::Can_Fight(e->second, unit)) {
                    e_priority = 4;
                }
                else if ( e_type.isWorker() ) {
                    e_priority = 3;
                }
                else if ( e->second.bwapi_unit_ && e->second.bwapi_unit_->exists() && MeatAIModule::IsFightingUnit(e->second.bwapi_unit_) || e_type.spaceProvided() > 0 ) {
                    e_priority = 2;
                }
                else if ( e->second.type_.mineralPrice() > 25 && e->second.type_ != UnitTypes::Zerg_Egg && e->second.type_ != UnitTypes::Zerg_Larva) {
                    e_priority = 1; // or if they cant fight back we'll get those last.
                }
                else {
                    e_priority = 0; // should leave stuff like larvae and eggs in here. Low, low priority.
                }


                if ( e_priority >= priority && dist_to_enemy <= max_dist) { // closest target of equal priority, or target of higher priority. Don't hop to enemies across the map when there are undefended things to destroy here.
                    if ( e_priority > 4 ) {
                        target_sentinel = true;
                    }
                    priority = e_priority;
                    max_dist = dist_to_enemy; // now that we have one within range, let's tighten our existing range.
                    target = e->second;
                }

                if ( !target_sentinel && priority <= 4 && dist_to_enemy < max_dist_no_priority ) {
                    target_sentinel_poor_target_atk = true;
                    max_dist_no_priority = dist_to_enemy; // then we will get the closest of these.
                    target = e->second;
                }
            }
        }
    }

    bool spam_guard = !unit->getLastCommand().getTarget() || (unit->getLastCommand().getTarget() != target.bwapi_unit_);
    if ( (target_sentinel || target_sentinel_poor_target_atk) && unit->hasPath(target.pos_) && spam_guard ) {
        if ( target.bwapi_unit_ && target.bwapi_unit_->exists() ) {
            if (fix_lurker_burrow(unit, ui, ei, target.pos_)) {
                //
            }
            else {
                unit->attack(target.bwapi_unit_);
                MeatAIModule::Diagnostic_Line(unit->getPosition(), target.pos_, color);
            }
        }
        else if (target.valid_pos_ && unit->hasPath( target.pos_ ) ) {
            if (fix_lurker_burrow(unit, ui, ei, target.pos_)) {
                //
            }
            else {
                unit->attack(target.pos_);
                MeatAIModule::Diagnostic_Line(unit->getPosition(), target.pos_, color);
            }
        }
    }
}

// Basic retreat logic, range = enemy range
void Boids::Retreat_Logic( const Unit &unit, const Stored_Unit &e_unit, Unit_Inventory &ei, const Unit_Inventory &ui, Inventory &inventory, const Color &color = Colors::White ) {

    int dist = unit->getDistance(e_unit.pos_);
    int air_range = e_unit.type_.airWeapon().maxRange();
    int ground_range = e_unit.type_.groundWeapon().maxRange();
    int chargable_distance_net = (MeatAIModule::getProperSpeed(unit) + e_unit.type_.topSpeed()) * unit->isFlying() ? e_unit.type_.airWeapon().damageCooldown() : e_unit.type_.groundWeapon().damageCooldown();
    int range = unit->isFlying() ? air_range : ground_range;
    Position pos = unit->getPosition();
    Unit_Inventory local_neighborhood = MeatAIModule::getUnitInventoryInRadius(ui, unit->getPosition(), 1500);

    if (dist < range + chargable_distance_net + 96) { //  Run if you're a noncombat unit or army starved. +3 tiles for safety. Retreat function now accounts for walkability.

        Broodwar->drawCircleMap(e_unit.pos_, range, Colors::Red);

        //initial retreat spot from enemy.
        if (unit->isFlying()) { // go to it if the path is clear,
            setDirectRetreat(pos, e_unit.pos_, unit->getType(), ei);
        }

        //setAlignment( unit, ui );
        if (MeatAIModule::isClearRayTrace(pos, e_unit.pos_, inventory)) {
            setDirectRetreat(pos, e_unit.pos_, unit->getType(), ei);
        }
        setAlignment( unit, local_neighborhood);
        setCohesion( unit, pos, local_neighborhood);
        setAttractionHome( unit, pos, ei, inventory ); 

        // Flying units to not need to seperate.
        if ( !unit->getType().isFlyer() || unit->getType() == UnitTypes::Zerg_Scourge ) {
            Unit_Inventory neighbors = MeatAIModule::getUnitInventoryInRadius( ui, pos, 32 );
            setSeperation( unit, pos, neighbors );
        } // closure: flyers

        if (unit->getType() == UnitTypes::Zerg_Lurker && unit->isBurrowed() && unit->isDetected() && !MeatAIModule::Can_Fight(unit, e_unit) && unit->getLastCommandFrame() < Broodwar->getFrameCount() - 7) {
            unit->unburrow();
            return;
        }
        else if (unit->getType() == UnitTypes::Zerg_Lurker && !unit->isBurrowed() && MeatAIModule::Can_Fight(unit, e_unit) && ei.detector_count_ == 0 && unit->getLastCommandFrame() < Broodwar->getFrameCount() - 7) {
            unit->burrow();
            return;
        }

          //Make sure the end destination is one suitable for you.
        Position retreat_spot = { (int)(pos.x + attract_dx_ + cohesion_dx_ - seperation_dx_ + attune_dx_ - walkability_dx_ + centralization_dx_ + retreat_dx_), 
                                  (int)(pos.y + attract_dy_ + cohesion_dy_ - seperation_dy_ + attune_dy_ - walkability_dy_ + centralization_dy_ + retreat_dy_) }; //attract is zero when it's not set.

        bool walkable_plus = retreat_spot.isValid() &&
            (unit->isFlying() || // can I fly, rendering the idea of walkablity moot?
            (MeatAIModule::isClearRayTrace( pos, retreat_spot, inventory ) && //or does it cross an unwalkable position? 
                MeatAIModule::checkOccupiedArea( ui, retreat_spot, 32 ))); // or does it end on a unit?

        if (retreat_spot && !unit->isBurrowed() && walkable_plus ) {
            unit->move( retreat_spot ); //identify vector between yourself and e.  go 350 pixels away in the quadrant furthest from them.
        }
        //else if ( unit->getLastCommandFrame() < Broodwar->getFrameCount() - 7 ) {
        //    unit->attack( retreat_spot );
        //}
    }
    else if ((unit->getLastCommand().getType() == UnitCommandTypes::Attack_Move) || (unit->getLastCommand().getType() == UnitCommandTypes::Attack_Unit) && !unit->isAttackFrame() ) {
        unit->stop();
    }

    MeatAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + retreat_dx_)       , (int)(pos.y + retreat_dy_) }, Colors::White);//Run directly away
    MeatAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + x_stutter_)        , (int)(pos.y + y_stutter_) }, Colors::Black);//Stutter
    MeatAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + attune_dx_)        , (int)(pos.y + attune_dy_) }, Colors::Green);//Alignment
    MeatAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + centralization_dx_), (int)(pos.y + centralization_dy_) }, Colors::Blue); // Centraliziation.
    MeatAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + cohesion_dx_)      , (int)(pos.y + cohesion_dy_) }, Colors::Purple); // Cohesion
    MeatAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + attract_dx_)       , (int)(pos.y + attract_dy_) }, Colors::Red); //Attraction towards attackable enemies or home base.
    MeatAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x - seperation_dx_)    , (int)(pos.y - seperation_dy_) }, Colors::Orange); // Seperation, does not apply to fliers.
    MeatAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x - walkability_dx_)   , (int)(pos.y - walkability_dy_) }, Colors::Cyan); // Push from unwalkability, different 

}


//Brownian Stuttering, causes unit to move about randomly.
void Boids::setStutter( const Unit &unit, const double &n ) {

    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen( rd() ); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<double> dis( -1, 1 );

    x_stutter_ = n * dis( gen ) * 32;
    y_stutter_ = n * dis( gen ) * 32; // The naieve approach of rand()%3 - 1 is "distressingly non-random in lower order bits" according to stackoverflow and other sources.
}

//Alignment. Convinces all units in unit inventory to move at similar velocities.
void Boids::setAlignment( const Unit &unit, const Unit_Inventory &ui ) {
    int temp_tot_x = 0;
    int temp_tot_y = 0;

    int flock_count = 0;
    if ( !ui.unit_inventory_.empty() ) {
        for ( auto i = ui.unit_inventory_.begin(); i != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); ++i ) {
            if ( i->second.type_ != UnitTypes::Zerg_Drone && i->second.type_ != UnitTypes::Zerg_Overlord && i->second.type_ != UnitTypes::Buildings ) {
                temp_tot_x += cos(i->second.bwapi_unit_->getAngle()); //get the horiz element.
                temp_tot_y += sin(i->second.bwapi_unit_->getAngle()); // get the vertical element. Averaging angles was trickier than I thought. 
                flock_count++;
            }
        }
        //double theta = atan2( temp_tot_y - unit->getVelocityY() , temp_tot_x - unit->getVelocityX() );  // subtract out the unit's personal heading.

        if ( flock_count > 1 ) {
            attune_dx_ = ( ( temp_tot_x - cos(unit->getAngle()) ) / (flock_count - 1) + cos(unit->getAngle()) ) * 64;
            attune_dy_ = ( ( temp_tot_y - sin(unit->getAngle()) ) / (flock_count - 1) + sin(unit->getAngle()) ) * 64; // think the velocity is per frame, I'd prefer it per second so its scale is sensical.
        }
        else {
            attune_dx_ = cos(unit->getAngle()) * 64;
            attune_dy_ = sin(unit->getAngle()) * 64;
        }
    }
}

void Boids::setDirectRetreat(const Position &pos, const Position &e_pos, const UnitType &type, const Unit_Inventory &ei) {
    int dist_x = e_pos.x - pos.x;
    int dist_y = e_pos.y - pos.y;
    double theta = atan2(dist_y, dist_x); // att_y/att_x = tan (theta).
    retreat_dx_ = -cos(theta) * (MeatAIModule::getProperSpeed(type) * ei.max_range_);
    retreat_dy_ = -sin(theta) * (MeatAIModule::getProperSpeed(type) * ei.max_range_); // get -range- outside of their range.  Should be safe.
}

//Centralization, all units prefer sitting along map veins to edges.
void Boids::setCentralize( const Position &pos, const Inventory &inventory ) {
    double temp_centralization_dx_ = 0;
    double temp_centralization_dy_ = 0;
    WalkPosition map_dim = WalkPosition( TilePosition( { Broodwar->mapWidth(), Broodwar->mapHeight() } ) );
    for ( int x = -5; x <= 5; ++x ) {
        for ( int y = -5; y <= 5; ++y ) {
            int mini_x = WalkPosition( pos ).x;
            int mini_y = WalkPosition( pos ).y;
            double centralize_x = mini_x + x;
            double centralize_y = mini_y + y;
            if ( !(x == 0 && y == 0) &&
                centralize_x < map_dim.x &&
                centralize_y < map_dim.y &&
                centralize_x > 0 &&
                centralize_y > 0 &&
                (inventory.map_veins_[centralize_x][centralize_y] > inventory.map_veins_[mini_x][mini_y] || inventory.map_veins_[centralize_x][centralize_y] > 175) )
            {
                double theta = atan2( y, x );
                temp_centralization_dx_ += cos( theta );
                temp_centralization_dy_ += sin( theta );
            }
        }
    }
    if ( temp_centralization_dx_ != 0 && temp_centralization_dy_ != 0 ) {
        double theta = atan2( temp_centralization_dy_, temp_centralization_dx_ );
        int distance_metric = abs( temp_centralization_dy_ * 8 ) + abs( temp_centralization_dx_ * 8 );
        centralization_dx_ = cos( theta ) * distance_metric * 0.25;
        centralization_dy_ = sin( theta ) * distance_metric * 0.25;
    }
}

//Cohesion, all units tend to prefer to be together.
void Boids::setCohesion( const Unit &unit, const Position &pos, const Unit_Inventory &ui ) {

    const Position loc_center = ui.getMeanArmyLocation();
    if ( loc_center != Position( 0, 0 ) ) {
        double cohesion_x = loc_center.x - pos.x;
        double cohesion_y = loc_center.y - pos.y;
        double theta = atan2( cohesion_y, cohesion_x );
        cohesion_dx_ = cos( theta ) * 0.25 * unit->getDistance( loc_center );
        cohesion_dy_ = sin( theta ) * 0.25 * unit->getDistance( loc_center );
    }
}

//Attraction, pull towards enemy units that we can attack. Requires some macro variables to be in place. Only sees visible units.
void Boids::setAttractionEnemy( const Unit &unit, const Position &pos, Unit_Inventory &ei, Inventory &inv ) {

    bool armed = unit->getType().airWeapon() != WeaponTypes::None || unit->getType().groundWeapon() != WeaponTypes::None;
    double health = unit->getHitPoints() / (double) unit->getType().maxHitPoints();

    if ( armed ) { 
        int dist = 999999;
        bool visible_unit_found = false;
        if ( !ei.unit_inventory_.empty() ) { // if there isn't a visible targetable enemy, but we have an inventory of them...

            Stored_Unit* e = MeatAIModule::getClosestAttackableStored( ei, unit->getType(), unit->getPosition(), dist ); 

            if ( e && e->pos_ && e->pos_.getDistance(pos) > ei.max_range_ + 512 ) {
                if ( !inv.map_veins_in_.empty() ){
                    double temp_attract_dx_ = 0;
                    double temp_attract_dy_ = 0;
                    WalkPosition map_dim = WalkPosition( TilePosition( { Broodwar->mapWidth(), Broodwar->mapHeight() } ) );
                    int enemy_spot = inv.getRadialDistanceOutFromEnemy( e->pos_ );
                    int my_spot = inv.getRadialDistanceOutFromEnemy( pos );
                    if ( enemy_spot < my_spot ) { // if he's inside my ground dist from base.
                        for ( int x = -5; x <= 5; ++x ) {
                            for ( int y = -5; y <= 5; ++y ) {
                                double centralize_x = WalkPosition( pos ).x + x;
                                double centralize_y = WalkPosition( pos ).y + y;
                                if ( !(x == 0 && y == 0) &&
                                    centralize_x < map_dim.x &&
                                    centralize_y < map_dim.y &&
                                    centralize_x > 0 &&
                                    centralize_y > 0 &&
                                    inv.map_veins_in_[centralize_x][centralize_y] < my_spot ) // if they are closer to their home than we are, we go to their base.
                                {
                                    double theta = atan2( y, x );
                                    temp_attract_dx_ += cos( theta );
                                    temp_attract_dy_ += sin( theta );
                                }
                            }
                        }
                        if ( temp_attract_dx_ != 0 && temp_attract_dy_ != 0 ) {
                            double theta = atan2( temp_attract_dy_, temp_attract_dx_ );
                            int distance_metric = inv.getDifferentialDistanceOutFromEnemy( e->pos_, pos );
                            attract_dx_ = cos( theta ) * (distance_metric * 0.01 * health);
                            attract_dy_ = sin( theta ) * (distance_metric * 0.01 * health);
                        }
                    } else if ( enemy_spot > my_spot ) { // if he's outside my ground dist from base.
                        my_spot = inv.getRadialDistanceOutFromHome( pos );
                        for ( int x = -5; x <= 5; ++x ) {
                            for ( int y = -5; y <= 5; ++y ) {
                                double centralize_x = WalkPosition( pos ).x + x;
                                double centralize_y = WalkPosition( pos ).y + y;
                                if ( !(x == 0 && y == 0) &&
                                    centralize_x < map_dim.x &&
                                    centralize_y < map_dim.y &&
                                    centralize_x > 0 &&
                                    centralize_y > 0 &&
                                    inv.map_veins_out_[centralize_x][centralize_y] < my_spot ) // if they are further away fron their home than we are, we go back to OUR home.
                                {
                                    double theta = atan2( y, x );
                                    temp_attract_dx_ += cos( theta );
                                    temp_attract_dy_ += sin( theta );
                                }
                            }
                        }
                        if ( temp_attract_dx_ != 0 && temp_attract_dy_ != 0 ) {
                            double theta = atan2( temp_attract_dy_, temp_attract_dx_ );
                            int distance_metric = inv.getDifferentialDistanceOutFromHome( e->pos_, pos );
                            attract_dx_ = cos( theta ) * (distance_metric * 0.01 * health);
                            attract_dy_ = sin( theta ) * (distance_metric * 0.01 * health);
                        }
                    }
                }
            }
        }
    }
}

void Boids::scoutEnemyBase(const Unit &unit, const Position &pos, Unit_Inventory &ei, Inventory &inv) {
        if (!inv.start_positions_.empty()) {
            Position possible_base = inv.start_positions_[0];
            int dist = unit->getDistance(possible_base);
            int dist_x = possible_base.x - pos.x;
            int dist_y = possible_base.y - pos.y;
            double theta = atan2(dist_y, dist_x);
            attract_dx_ = cos(theta) * dist; // run 100% towards them.
            attract_dy_ = sin(theta) * dist;

            std::rotate(inv.start_positions_.begin(), inv.start_positions_.begin() + 1, inv.start_positions_.end());
        }
}

//Attraction, pull towards homes that we can attack. Requires some macro variables to be in place.
void Boids::setAttractionHome( const Unit &unit, const Position &pos, Unit_Inventory &ei, Inventory &inv ) {
    if (!ei.unit_inventory_.empty()) { // if there i an existant enemy..
        if (!inv.map_veins_out_.empty()) {
            double temp_attract_dx_ = 0;
            double temp_attract_dy_ = 0;
            WalkPosition map_dim = WalkPosition(TilePosition({ Broodwar->mapWidth(), Broodwar->mapHeight() }));
            int my_spot = inv.getRadialDistanceOutFromHome(pos);
            for (int x = -5; x <= 5; ++x) {
                for (int y = -5; y <= 5; ++y) {
                    double centralize_x = WalkPosition(pos).x + x;
                    double centralize_y = WalkPosition(pos).y + y;
                    if (!(x == 0 && y == 0) &&
                        centralize_x < map_dim.x &&
                        centralize_y < map_dim.y &&
                        centralize_x > 0 &&
                        centralize_y > 0 &&
                        inv.map_veins_out_[centralize_x][centralize_y] < my_spot) // go directly home to our base if they are outside of us.
                    {
                        double theta = atan2(y, x);
                        temp_attract_dx_ += cos(theta);
                        temp_attract_dy_ += sin(theta);
                    }
                }
            }
            if (temp_attract_dx_ != 0 && temp_attract_dy_ != 0) {
                double theta = atan2(temp_attract_dy_, temp_attract_dx_);
                attract_dx_ = cos(theta) * (128);
                attract_dy_ = sin(theta) * (128);
            }
        }
    }
}

//Seperation from nearby units, search very local neighborhood of 2 tiles.
void Boids::setSeperation( const Unit &unit, const Position &pos, const Unit_Inventory &ui ) {
    Unit_Inventory neighbors = MeatAIModule::getUnitInventoryInRadius(ui, pos, 32);
    Position seperation = neighbors.getMeanLocation();
    if ( seperation != pos ) { // don't seperate from yourself, that would be a disaster.
        double seperation_x = seperation.x - pos.x;
        double seperation_y = seperation.y - pos.y;
        double theta = atan2( seperation_y, seperation_x );
        seperation_dx_ = cos( theta ) * 64; // run 2 tiles away from everyone. Should help avoid being stuck in those wonky spots.
        seperation_dy_ = sin( theta ) * 64;
    }
}

//Seperation from nearby units, search very local neighborhood of 2 tiles.
void Boids::setSeperationScout(const Unit &unit, const Position &pos, const Unit_Inventory &ui) {
    Unit_Inventory neighbors = MeatAIModule::getUnitInventoryInRadius(ui, pos, unit->getType().sightRange());
    Position seperation = ui.getMeanLocation();
    if (seperation != pos) { // don't seperate from yourself, that would be a disaster.
        double seperation_x = seperation.x - pos.x;
        double seperation_y = seperation.y - pos.y;
        double theta = atan2(seperation_y, seperation_x);
        seperation_dx_ = cos(theta) * unit->getType().sightRange(); // run 2 tiles away from everyone. Should help avoid being stuck in those wonky spots.
        seperation_dy_ = sin(theta) * unit->getType().sightRange();
    }
    cohesion_dx_ = 0;
    cohesion_dy_ = 0;
    attract_dx_ = 0;
    attract_dy_ = 0;
}

void Boids::setObjectAvoid( const Unit &unit, const Position &pos, const Inventory &inventory ) {

    double temp_walkability_dx_ = 0;
    double temp_walkability_dy_ = 0;
    if ( !unit->isFlying() ) {
        for ( int x = -5; x <= 5; ++x ) {
            for ( int y = -5; y <= 5; ++y ) {
                double walkability_x = TilePosition( pos ).x + x;
                double walkability_y = TilePosition( pos ).y + y;
                if ( !(x == 0 && y == 0) ) {
                    if ( walkability_x >= Broodwar->mapWidth() * 32 || walkability_y >= Broodwar->mapHeight() * 32 || // out of bounds by above map value.
                        walkability_x < 0 || walkability_y < 0 ||  // out of bounds below map,  0.
                        Broodwar->getGroundHeight( TilePosition( walkability_x, walkability_y ) ) != Broodwar->getGroundHeight( unit->getTilePosition() ) ||  //If a position is on a different level, it's essentially unwalkable.
                        !MeatAIModule::isClearRayTrace( pos, Position( walkability_x, walkability_y ), inventory ) ) { // or if the path to the target is relatively unwalkable.
                                                                                                                       //!Broodwar->getUnitsOnTile( walkability_x / 32, walkability_y / 32, !IsFlying ).empty() ) { // or if the position is occupied.
                        double theta = atan2( y, x );
                        temp_walkability_dx_ += cos( theta );
                        temp_walkability_dy_ += sin( theta );
                    }
                }
            }
        }
    }
    if ( temp_walkability_dx_ != 0 && temp_walkability_dy_ != 0 ) {
        double theta = atan2( temp_walkability_dy_, temp_walkability_dx_ );
        double temp_dist = abs( temp_walkability_dx_ * 8 ) + abs( temp_walkability_dy_ * 8 ); // we should move away based on the number of tiles rejecting us.


        double tilt = rng_direction_ * 0.25 * 3.1415; // random number -1..1 times 0.75 * pi, should rotate it 45 degrees to the left or right of its original target. 
        walkability_dx_ = cos( theta + tilt ) * temp_dist * 0.25;
        walkability_dy_ = sin( theta + tilt ) * temp_dist * 0.25;
    }
}


// returns TRUE if the lurker needed fixing. For Attack.
bool Boids::fix_lurker_burrow(const Unit &unit, const Unit_Inventory &ui, const Unit_Inventory &ei, const Position position_of_target) {
    int dist_to_threat_or_target = unit->getDistance(position_of_target);
    bool hide_condition = ((dist_to_threat_or_target < ei.max_range_ && ei.detector_count_ == 0) || dist_to_threat_or_target < unit->getType().groundWeapon().maxRange());

    if (unit->getType() == UnitTypes::Zerg_Lurker && !unit->isBurrowed() && hide_condition && unit->getLastCommandFrame() < Broodwar->getFrameCount() - 7) {
        unit->burrow();
        return true;
    }
    else if (unit->getType() == UnitTypes::Zerg_Lurker && unit->isBurrowed() && !hide_condition && unit->getLastCommandFrame() < Broodwar->getFrameCount() - 7) {
        unit->unburrow();
        return true;
    }
    else if (unit->getType() == UnitTypes::Zerg_Lurker && !unit->isBurrowed() && !hide_condition && unit->getLastCommandFrame() < Broodwar->getFrameCount() - 7) {
        double theta = atan2(position_of_target.y - unit->getPosition().y, position_of_target.x - unit->getPosition().x);
        Position closest_loc_to_permit_attacking = Position(position_of_target.x + cos(theta) * unit->getType().groundWeapon().maxRange() * 0.75, position_of_target.y + sin(theta) * unit->getType().groundWeapon().maxRange() * 0.75);
        unit->move(closest_loc_to_permit_attacking);
        return true;
    }

    return false;
}
