# include "Source\MeatAIModule.h"

using namespace BWAPI;
using namespace Filter;
using namespace std;

//Checks if a building can be built, and passes additional boolean criteria.  If all critera are passed, then it builds the building and delays the building timer 25 frames, or ~1 sec. It may now allow morphing, eg, lair, hive and lurkers, but this has not yet been tested.  It now has an extensive creep colony script that prefers centralized locations.
void MeatAIModule::Check_N_Build( UnitType building, Unit unit, bool extra_critera )
{
    if ( unit->canBuild( building ) &&
        Broodwar->self()->minerals() >= building.mineralPrice() &&
        Broodwar->self()->gas() >= building.gasPrice() &&
        extra_critera &&
        building != UnitTypes::Zerg_Creep_Colony )
    {
        TilePosition buildPosition = Broodwar->getBuildLocation( building, unit->getTilePosition(), 64, building == UnitTypes::Zerg_Creep_Colony );
        unit->build( building, buildPosition );
        t_build += 25;
    }
    else if ( unit->canBuild( building ) &&
        Broodwar->self()->minerals() >= building.mineralPrice() &&
        Broodwar->self()->gas() >= building.gasPrice() &&
        extra_critera &&
        building == UnitTypes::Zerg_Creep_Colony ) {

        Unitset base_core = unit->getUnitsInRadius( INT_MAX, IsBuilding && IsResourceDepot && IsCompleted );
        int middle_x = Broodwar->mapWidth() / 2 * 32;
        int middle_y = Broodwar->mapHeight() / 2 * 32;

        int central_base_x = 0;
        int central_base_y = 0;

        int furth_x_dist = 0;
        int furth_y_dist = 0;

        if ( !base_core.empty() ) {

            for ( auto base = base_core.begin(); base != base_core.end(); ++base ) {

                int x_dist = pow( middle_x - (*base)->getPosition().x, 2 );
                int y_dist = pow( middle_y - (*base)->getPosition().y, 2 );

                int new_dist = sqrt( (double)x_dist + (double)y_dist );

                int furth_x_dist = pow( middle_x - central_base_x, 2 );
                int furth_y_dist = pow( middle_y - central_base_y, 2 );

                int old_dist = sqrt( (double)furth_x_dist + (double)furth_y_dist );

                if ( new_dist <= old_dist ) {
                    central_base_x = (*base)->getPosition().x;
                    central_base_y = (*base)->getPosition().y;
                }
            }
        }
        double theta = atan2( middle_y - central_base_y, middle_x - central_base_x );
        int adj_dx = cos( theta ) * 4; // move n tiles closer to the center of the map.
        int adj_dy = sin( theta ) * 4;

        TilePosition buildPosition = Broodwar->getBuildLocation( building, { central_base_x / 32 + adj_dx , central_base_y / 32 + adj_dy }, 5 );
        unit->build( building, buildPosition );
        t_build += 25;
    }

    if ( unit->canMorph( building ) &&
        Broodwar->self()->minerals() >= building.mineralPrice() &&
        Broodwar->self()->gas() >= building.gasPrice() &&
        extra_critera )
    {
        unit->morph( building );
        t_build += 25;
    }
}

//Checks if an upgrade can be built, and passes additional boolean criteria.  If all critera are passed, then it performs the upgrade. Requires extra critera.
void MeatAIModule::Check_N_Upgrade( UpgradeType ups, Unit unit, bool extra_critera )
{
    if ( unit->canUpgrade( ups ) &&
        Broodwar->self()->minerals() >= ups.mineralPrice() &&
        Broodwar->self()->gas() >= ups.gasPrice() &&
        extra_critera ) {
        unit->upgrade( ups );
    }
}

//Checks if a unit can be built from a larva, and passes additional boolean criteria.  If all critera are passed, then it performs the upgrade. Requires extra critera.
void MeatAIModule::Check_N_Grow( UnitType unittype, Unit larva, bool extra_critera )
{
    if ( larva->canTrain( unittype ) &&
        Broodwar->self()->minerals() >= unittype.mineralPrice() &&
        Broodwar->self()->gas() >= unittype.gasPrice() &&
        extra_critera )
    {
        larva->train( unittype );
    }
}
