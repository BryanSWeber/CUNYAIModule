# include "Source\MeatAIModule.h"

using namespace BWAPI;
using namespace Filter;
using namespace std;

//Forces a unit to stutter in a brownian manner. Size of stutter is unit's (vision range * n ). Will attack if it sees something.  Overlords stop if they can see minerals.
void MeatAIModule::Brownian_Stutter( Unit unit, double n ) {

    Position pos = unit->getPosition();
    Unitset neighbors = unit->getUnitsInRadius( 75, !IsEnemy );
    Unitset flock = unit->getUnitsInRadius( 1024, !IsWorker && !IsBuilding && IsOwned );
    Unitset enemy_set = unit->getUnitsInRadius( 350, IsEnemy );

    double x_stutter = n * (rand() % 101 - 50) / 50 * unit->getType().sightRange();
    double y_stutter = n * (rand() % 101 - 50) / 50 * unit->getType().sightRange();// The naieve approach of rand()%3 - 1 is "distressingly non-random in lower order bits" according to stackoverflow and other sources.

    //Alignment.
    double tot_x = 0;
    double tot_y = 0;
    double attune_dx = 0;
    double attune_dy = 0;
    int flock_count = 0;
    if ( !flock.empty() ) {
        for ( auto i = flock.begin(); i != flock.end() && !flock.empty(); ++i ) {
            if ( (*i)->getType() != UnitTypes::Zerg_Drone && (*i)->getType() != UnitTypes::Zerg_Overlord ) {
                tot_x += cos( (*i)->getAngle() ); //get the horiz element.
                tot_y += sin( (*i)->getAngle() ); // get the vertical element. Averaging angles was trickier than I thought. 
                flock_count++;
            }
        }
        double theta = atan2( (tot_y - sin( unit->getAngle() ) ), (tot_x - cos( unit->getAngle() ) ) );  // subtract out the unit's personal heading.
        attune_dx = cos( theta ) * 0.40 * unit->getType().sightRange();
        attune_dy = sin( theta ) * 0.40 * unit->getType().sightRange();
        Diagnostic_Line( unit->getPosition(), { (int)(pos.x + attune_dx), (int)(pos.y + attune_dy) }, Colors::Green );
    }

    //Cohesion
    double coh_dx = 0;
    double coh_dy = 0;

    if ( !unit->isFlying() ) {
        if ( !flock.empty() ) {
            Position loc_center = flock.getPosition();
            Broodwar->drawCircleMap( loc_center, 5, Colors::Blue, true );
            double cohesion_x = loc_center.x - pos.x;
            double cohesion_y = loc_center.y - pos.y;
            double theta = atan2( cohesion_y, cohesion_x );
            coh_dx = cos( theta ) * 0.15 * unit->getType().sightRange();
            coh_dy = sin( theta ) * 0.15 * unit->getType().sightRange();
            Diagnostic_Line( unit->getPosition(), { (int)(pos.x + coh_dx), (int)(pos.y + coh_dy) }, Colors::Blue );
        }
    }

    //Centraliziation
    double cen_dx = 0;
    double cen_dy = 0;
    Position center = { Broodwar->mapWidth() * 32 / 2, Broodwar->mapHeight() * 32 / 2 };
    double centralize_x = center.x - pos.x;
    double centralize_y = center.y - pos.y;
    double theta = atan2( centralize_y, centralize_x );
    cen_dx = cos( theta ) * 0.075 * unit->getType().sightRange();
    cen_dy = sin( theta ) * 0.075 * unit->getType().sightRange();
    Diagnostic_Line( unit->getPosition(), { (int)(pos.x + cen_dx), (int)(pos.y + cen_dy) }, Colors::Blue );

    //seperation
    double sep_dx = 0;
    double sep_dy = 0;
    Position seperation = neighbors.getPosition();
    if ( seperation != pos ) {
        double seperation_x = seperation.x - pos.x;
        double seperation_y = seperation.y - pos.y;
        double theta = atan2( seperation_y, seperation_x );
        sep_dx = cos( theta ) * 50;
        sep_dy = sin( theta ) * 50;
        Diagnostic_Line( unit->getPosition(), { (int)(pos.x - sep_dx), (int)(pos.y - sep_dy) }, Colors::Orange );
    }

    // Push towards enemy (or away if army starved)
    int attract_dx = 0;
    int attract_dy = 0;

    if ( !enemy_set.empty() ) {
        int dist_x = enemy_set.getPosition().x - pos.x;
        int dist_y = enemy_set.getPosition().y - pos.y;
        double theta = atan2( dist_y, dist_x ); // att_y/att_x = tan (theta).
        if ( army_starved ) {
            attract_dx = -cos( theta ) * unit->getType().sightRange() * 1; // run __ sight range away.
            attract_dy = -sin( theta ) * unit->getType().sightRange() * 1;
        }
        else {
            attract_dx = cos( theta ) * unit->getType().sightRange() * 1; // run ___ sight range towards them.
            attract_dy = sin( theta ) * unit->getType().sightRange() * 1;
        }
        Position retreat_spot = { pos.x + (int)attract_dx , pos.y + (int)attract_dy };
        Diagnostic_Line( unit->getPosition(), { pos.x + (int)attract_dx , pos.y + (int)attract_dy }, Colors::White );

    }

    //Push from unwalkability
    double wlk_dx = 0;
    double wlk_dy = 0;
    double temp_wlk_dx = 0;
    double temp_wlk_dy = 0;
    if ( !unit->isFlying() ) {
        for ( int x = -1; x <= 1; ++x ) {
            for ( int y = -1; y <= 1; ++y ) {
                double walkability_x = pos.x + unit->getType().sightRange() * x;
                double walkability_y = pos.y + unit->getType().sightRange() * y;
                if ( !(x == 0 && y == 0) ) {
                    if ( walkability_x > Broodwar->mapWidth() * 32 || walkability_y > Broodwar->mapHeight() * 32 || // out of bounds by above map value.
                        walkability_x < 0 || walkability_y < 0 ||  // out of bounds by 0.
                        !Broodwar->isWalkable( (int)walkability_x / 8, (int)walkability_y / 8 ) ) { //Mapheight/width get map dimensions in TILES 1tile=32 pixels. Iswalkable gets it in minitiles, 1 minitile=8 pixels.
                        double theta = atan2( y, x );
                        temp_wlk_dx += cos( theta );
                        temp_wlk_dy += sin( theta );
                    }
                }
            }
        }
    }
    if ( temp_wlk_dx != 0 && temp_wlk_dy != 0 ) {
        double theta = atan2( temp_wlk_dy, temp_wlk_dx );
        wlk_dx = cos( theta ) * unit->getType().sightRange() * 0.5;
        wlk_dy = sin( theta ) * unit->getType().sightRange() * 0.5;
        Diagnostic_Line( unit->getPosition(), { (int)(pos.x - wlk_dx), (int)(pos.y - wlk_dy) }, Colors::Purple );
    }

    Position brownian_pos = { (int)(pos.x + x_stutter + coh_dx - sep_dx + attune_dx - wlk_dx + attract_dx + cen_dx ), (int)(pos.y + y_stutter + coh_dy - sep_dy + attune_dy - wlk_dy + attract_dy + cen_dy ) };

    if ( brownian_pos.isValid() ) {
        if ( unit->canAttack( brownian_pos ) && !army_starved) {
            unit->attack( brownian_pos );
        }
        else {
            unit->move( brownian_pos );
        }
    }

    if ( unit->getType() == UnitTypes::Zerg_Overlord ) {

        Unitset min = unit->getUnitsInRadius( 250, IsMineralField );
        Unitset bases = unit->getUnitsInRadius( 500, IsResourceDepot && IsOwned );        
        Unitset e = unit->getUnitsInRadius( 250, IsEnemy );

        if ( min.size() >= 1 && bases.empty() && e.empty() ) {
            Unit min = unit->getClosestUnit( IsMineralField );
            unit->move( min->getPosition() );
        }
    }
};

// This is basic combat logic for nonspellcasting units.
void MeatAIModule::Combat_Logic( Unit unit, Color color = Colors::White )
{
    //Intitiate combat if: we have army, enemies are present, we are combat units and we are not army starved.  These units attack both air and ground.
    if ( unit->getType().groundWeapon().targetsGround() && unit->getType().airWeapon().targetsAir() ) {
        Unit target_caster = unit->getClosestUnit( Filter::IsEnemy && Filter::IsDetected && (Filter::IsSpellcaster || Filter::IsSieged || Filter::ScarabCount) && !Filter::IsAddon && !Filter::IsBuilding, 250 ); // Charge the closest caster nononbuilding that is not cloaked.
        if ( target_caster &&
            target_caster->exists() ) { // Neaby casters
            unit->attack( target_caster );
            Diagnostic_Line( unit->getPosition(), target_caster->getPosition(), color );
        }
        else if ( !target_caster || !target_caster->exists() ) { // then nearby combat units or transports.
            Unit target_warrior = unit->getClosestUnit( Filter::IsEnemy && (Filter::CanAttack || Filter::SpaceProvided) && Filter::IsDetected, 250 ); // is warrior (or bunker, man those are annoying)
            if ( target_warrior && target_warrior->exists() ) {
                unit->attack( target_warrior );
                Diagnostic_Line( unit->getPosition(), target_warrior->getPosition(), color );
            }
            else { // No targatable warrior? then whatever is closest (and already found)
                Unit e = unit->getClosestUnit( Filter::IsEnemy );
                if ( e && e->exists() ) {
                    unit->attack( e );
                    Diagnostic_Line( unit->getPosition(), e->getPosition(), color );
                }
            }
        }
    } // closure for G and A weapons.
    // Combat for units with only G weapons.
    else if ( unit->getType().groundWeapon().targetsGround() && !unit->getType().airWeapon().targetsAir() ) {  
        //Intiatite combat if: we have army, enemies are present, we are combat units and we are not army starved.
        Unit target_caster = unit->getClosestUnit( Filter::IsEnemy && Filter::IsDetected && (Filter::IsSpellcaster || Filter::IsSieged || Filter::ScarabCount) && !Filter::IsAddon && !Filter::IsBuilding && !Filter::IsFlying, 250 ); // Charge the closest caster nononbuilding that is not cloaked.
        if ( target_caster && target_caster->exists() ) { // Neaby casters
            unit->attack( target_caster );
            Diagnostic_Line( unit->getPosition(), target_caster->getPosition(), color );
        }
        else if ( !target_caster || !target_caster->exists() ) { // then nearby combat units or transports.
            Unit target_warrior = unit->getClosestUnit( Filter::IsEnemy && (Filter::CanAttack || Filter::SpaceProvided) && Filter::IsDetected && !Filter::IsFlying, 250 ); // is warrior (or bunker, man those are annoying)
            if ( target_warrior && target_warrior->exists() ) {
                unit->attack( target_warrior );
                Diagnostic_Line( unit->getPosition(), target_warrior->getPosition(), color );
            }
            else { // No targatable warrior? then whatever is closest and not flying.
                Unit ground_e = unit->getClosestUnit( Filter::IsEnemy && !Filter::IsFlying ); // is warrior (or bunker, man those are 
                if ( ground_e && ground_e->exists() ) {
                    unit->attack( ground_e );
                    Diagnostic_Line( unit->getPosition(), ground_e->getPosition(), color );
                }
            }
        }
    } // closure for only G weapons.
    // Combat for units with only A weapons.
    else if ( !unit->getType().groundWeapon().targetsGround() && unit->getType().airWeapon().targetsAir() ) {
        //Intiatite combat if: we have army, enemies are present, we are combat units and we are not army starved.
        Unit target_caster = unit->getClosestUnit( Filter::IsEnemy && Filter::IsDetected && (Filter::IsSpellcaster || Filter::IsSieged || Filter::ScarabCount) && !Filter::IsAddon && !Filter::IsBuilding && Filter::IsFlying, 250 ); // Charge the closest caster nononbuilding that is not cloaked.
        if ( target_caster && target_caster->exists() ) { // Neaby casters
            unit->attack( target_caster );
            Diagnostic_Line( unit->getPosition(), target_caster->getPosition(), color );
        }
        else if ( !target_caster || !target_caster->exists() ) { // then nearby combat units or transports.
            Unit target_warrior = unit->getClosestUnit( Filter::IsEnemy && (Filter::CanAttack || Filter::SpaceProvided) && Filter::IsDetected && Filter::IsFlying, 250 ); // is warrior (or bunker, man those are annoying)
            if ( target_warrior && target_warrior->exists() ) {
                unit->attack( target_warrior );
                Diagnostic_Line( unit->getPosition(), target_warrior->getPosition(), color );
            }
            else { // No targatable warrior? then whatever is closest and not flying.
                Unit ground_e = unit->getClosestUnit( Filter::IsEnemy && Filter::IsFlying ); // is warrior (or bunker, man those are 
                if ( ground_e && ground_e->exists() ) {
                    unit->attack( ground_e );
                    Diagnostic_Line( unit->getPosition(), ground_e->getPosition(), color );
                }
            }
        }
    } // closure for only anti air weapons.
}

// Basic retreat logic, range = enemy range
void MeatAIModule::Retreat_Logic( Unit unit, Unit enemy, Color color = Colors::White ) {

    int dist = unit->getDistance( enemy );
    int air_range = enemy->getType().airWeapon().maxRange();
    int ground_range = enemy->getType().groundWeapon().maxRange();
    int range = air_range > ground_range ? air_range : ground_range;
    if ( dist < range + 50 ) { //  Run if you're a noncombat unit or army starved. +50 for safety Retreat function now accounts for walkability.

        Position e_pos = enemy->getPosition();
        Position pos = unit->getPosition();
        Unitset neighbors = unit->getUnitsInRadius( 125, !IsWorker && !IsBuilding && IsOwned );

        Broodwar->drawCircleMap( e_pos, range , Colors::Red );

        //initial retreat spot.
        int dist_x = e_pos.x - pos.x;
        int dist_y = e_pos.y - pos.y;
        double theta = atan2( dist_y, dist_x ); // att_y/att_x = tan (theta).
        double retreat_dx = -cos( theta ) * unit->getType().sightRange() ;
        double retreat_dy = -sin( theta ) * unit->getType().sightRange() ;

        // Scatter- 
        double sep_dx = 0;
        double sep_dy = 0;
        if ( !unit->isFlying() ) {
            Position seperation = neighbors.getPosition();
            if ( seperation ) {
                double seperation_x = seperation.x - pos.x;
                double seperation_y = seperation.y - pos.y;
                double theta = atan2( seperation_y, seperation_x );
                sep_dx = cos( theta ) * 50;
                sep_dy = sin( theta ) * 50;
                Diagnostic_Line( unit->getPosition(), { (int)(pos.x - sep_dx), (int)(pos.y - sep_dy) }, Colors::Orange );
            }
        }

        // account for walkability.
        double wlk_dx = 0;
        double wlk_dy = 0;
        double temp_wlk_dx = 0;
        double temp_wlk_dy = 0;
        if ( !unit->isFlying() ) {
            for ( int x = -1; x <= 1; ++x ) {
                for ( int y = -1; y <= 1; ++y ) {
                    double walkability_x = pos.x + unit->getType().sightRange() * x;
                    double walkability_y = pos.y + unit->getType().sightRange() * y;
                    if ( !(x == 0 && y == 0) ) {
                        if ( walkability_x > Broodwar->mapWidth() * 32 || walkability_y > Broodwar->mapHeight() * 32 || // out of bounds by above map value.
                            walkability_x < 0 || walkability_y < 0 ||  // out of bounds by 0.
                            !Broodwar->isWalkable( (int)walkability_x / 8, (int)walkability_y / 8 ) ) { //Mapheight/width get map dimensions in TILES 1tile=32 pixels. Iswalkable gets it in minitiles, 1 minitile=8 pixels.
                            double theta = atan2( y, x );
                            temp_wlk_dx += cos( theta );
                            temp_wlk_dy += sin( theta );
                        }
                    }
                }
            }

            if ( temp_wlk_dx != 0 && temp_wlk_dy != 0 ) {
                double theta = atan2( temp_wlk_dy, temp_wlk_dx );
                wlk_dx = cos( theta ) * unit->getType().sightRange() * 1.25;
                wlk_dy = sin( theta ) * unit->getType().sightRange() * 1.25;
                Diagnostic_Line( unit->getPosition(), { (int)(pos.x - wlk_dx), (int)(pos.y - wlk_dy) }, Colors::Brown );
            }
        }
        Position retreat_spot = { pos.x + (int)retreat_dx - (int)wlk_dx -(int)sep_dx, pos.y + (int)retreat_dy - (int)wlk_dy - (int)sep_dy }; // the > < functions find the signum, no such function exists in c++!

        if ( retreat_spot && retreat_spot.isValid() ) {
            unit->move( retreat_spot ); //identify vector between yourself and e.  go 350 pixels away in the quadrant furthest from them.
            Diagnostic_Line( pos, retreat_spot, color );
        }
    }
}