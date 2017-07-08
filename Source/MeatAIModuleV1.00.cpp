#include "MeatAIModule.h"
#include "UnitUtil.h"
#include <iostream>

// MeatAI V0.12. Current V goal-> Clean up issues.
// Unresolved issue: Text coloration in on screen messages. Screen messages doesn't take color commands, but does take printf format commands. Gets messy very fast.
// Unresolved issue: use a more efficient priority list, it is unweildly.
// unresolved issue: workers will overmine a mineral patch area.
 // no units/no overlords leads to program crash.
// generate an inventory system for researches and buildings. Probably as an array. Perhaps expand upon the self class?  Consider your tech inventory.
// Would like to someday not overload unit queue, but spamming is much easier.
// Caution: algorithm gets bamboozled if any of the derivatives are invinity. Logic for starvation fails (I think it returns nulls), so do the derivative comparisons.
// remove all dependance on UnitUtil, move getAllUnit command to my own module.
// interest in actual map exploration vs raw vision? Interested in mineral vision?
// creep colonies should spore on inside, sunken on outside. 
// expos are bad choices

using namespace BWAPI;
using namespace Filter;

void MeatAIModule::onStart()
{
    //Initialize state variables
    gas_starved = false;
    army_starved = false;
    supply_starved = false;
    vision_starved = false;
    econ_starved = true;
    tech_starved = false;

    //Initialize model variables
    delta = 0.45; //gas starved parameter. Triggers state if: ln_gas/(ln_min + ln_gas) < delta;  Higher is more gas. .35 is basically all lings+tech for lings.
    gamma = 0.70; //supply starved parameter. Triggers state if: ln_supply_remain/ln_supply_total < gamma; Some good indicators that this is reasonable: ln(4)/ln(9) is around 0.63, ln(3)/ln(9) is around 0.73, so we will build our first overlord at 7/9 supply. ln(18)/ln(100) is also around 0.63, so we will have a nice buffer for midgame.

    //Cobb-Douglas Production exponents.  Should all sum to one. Current best {0.03000,0.96800,0.00175, 0.00025} leads to illustratively reasonable- some drone scouting, etc. Prior to adding hydra tech option.
    alpha_army = 0.450; // army starved parameter. This comes in a medium scale
    alpha_vis =  0.700; // vision starved parameter. Note the very large scale for vision, vision comes in groups of thousands. Since this is not scale free, the delta must be larger or else it will always be considered irrelevant.
    alpha_econ = 0.009; // econ starved parameter. This comes in a small scale.
    alpha_tech = 0.002; // tech starved parameter. This comes in a medium to large scale.

  // Hello World!
    Broodwar->sendText( "Hello world! This is MeatShieldAI V0.10" );

    // Print the map name.
    // BWAPI returns std::string when retrieving a string, don't forget to add .c_str() when printing!
    Broodwar << "The map is " << Broodwar->mapName() << "!" << std::endl;

    // Enable the UserInput flag, which allows us to control the bot and type messages.
    Broodwar->enableFlag( Flag::UserInput );

    // Uncomment the following line and the bot will know about everything through the fog of war (cheat).
    //Broodwar->enableFlag(Flag::CompleteMapInformation);

    // Set the command optimization level so that common commands can be grouped
    // and reduce the bot's APM (Actions Per Minute).
    Broodwar->setCommandOptimizationLevel( 2 );

    // Check if this is a replay
    if ( Broodwar->isReplay() )
    {
        // Announce the players in the replay
        Broodwar << "The following players are in this replay:" << std::endl;

        // Iterate all the players in the game using a std:: iterator
        Playerset players = Broodwar->getPlayers();
        for ( auto p : players )
        {
            // Only print the player if they are not an observer
            if ( !p->isObserver() )
                Broodwar << p->getName() << ", playing as " << p->getRace() << std::endl;
        }

    }
    else // if this is not a replay
    {
        // Retrieve you and your enemy's races. enemy() will just return the first enemy.
        // If you wish to deal with multiple enemies then you must use enemies().
        if ( Broodwar->enemy() ) // First make sure there is an enemy
            Broodwar << "The matchup is " << Broodwar->self()->getRace() << " vs " << Broodwar->enemy()->getRace() << std::endl;
    }

}

void MeatAIModule::onEnd( bool isWinner )
{
    // Called when the game ends
    if ( isWinner )
    {
        // Log your win here!
    }
}

void MeatAIModule::onFrame()
{ // Called once every game frame

  // Return if the game is a replay or is paused
    if ( Broodwar->isReplay() || Broodwar->isPaused() || !Broodwar->self() )
        return;

    //Define important variables.

    // Game time;
    int t_game = Broodwar->getFrameCount();

    //Unit inventory.
    int hatches = MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Hatchery ) +
        MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Lair ) + 
        MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Hive );
    int ovis = MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Overlord );
    int larve = MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Larva );
    int morphing_count = Broodwar->self()->incompleteUnitCount( UnitTypes::Buildings );
    int* morphing_count_ptr = &morphing_count;
    int pool = MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Spawning_Pool );
    int extractor = MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Extractor );
    int evos = MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Evolution_Chamber );
    int drones = MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Drone );

    //Idea: MeatAI::UnitUtil::GetAllUnitCount(UnitCommandTypes::Build)  What command reads Unitcommandtype chars?

    //Econ inventory 
    int worker_count = 1; // adds 1 to avoid divide by 0 error.
    int* worker_count_ptr = &worker_count;
    int gas_workers = 0;
    int* gas_workers_ptr = &gas_workers;
    int min_workers = 0;
    int* min_workers_ptr = &min_workers;
    int min_fields = 1;
    int* min_fields_ptr = &min_fields;

    //maintains an active inventory of minerals near our bases.
    int home_x = Broodwar->self()->getUnits().getPosition().x;
    int home_y = Broodwar->self()->getUnits().getPosition().y; // Get main building closest to start location.
    Unitset resource = Broodwar->getMinerals(); // get any mineral field that exists on the map.
    if ( !resource.empty() ) { // check if the minerals exist
        for ( auto r = resource.begin(); r != resource.end(); ++r ) { //for each mineral
            //Broodwar->drawCircleMap( (*r)->getPosition(), 300, Colors::Green ); // Circle each mineral.
            Unitset mybases = Broodwar->getUnitsInRadius( (*r)->getPosition(), 300, Filter::IsResourceDepot && Filter::IsOwned ); // is there a mining base near there
            if ( !mybases.empty() ) { // check if there is a base nearby
                for ( auto base = mybases.begin(); base != mybases.end(); ++base ) {
                    Broodwar->drawCircleMap( (*base)->getPosition(), 300, Colors::Green ); // if nearby base, draw a pretty picture.
                } 
                (*min_fields_ptr)++; // count this mineral if there is a base near it.
            } // closure if base is nearby
        }// closure: Checking things nearby minerals.
    } // closure, mineral tally.

  // Get worker tallies.
    Unitset myUnits = Broodwar->getUnitsInRadius( home_x, home_y, 999999, Filter::IsWorker ); // is there a worker near the mineral?
    if ( !myUnits.empty() ) { // make sure this object is valid!
        for ( auto u = myUnits.begin(); u != myUnits.end(); ++u )
        {
            if ( (*u)->isGatheringGas() || (*u)->isCarryingGas() ) // implies exists and isCompleted
            {
                (*gas_workers_ptr)++;
            }
            if ( (*u)->isGatheringMinerals() || (*u)->isCarryingMinerals() ) // implies exists and isCompleted
            {
                (*min_workers_ptr)++;
            }
            *worker_count_ptr = gas_workers + min_workers + 1;  //adds 1 to avoid a divide by 0 error
        } // closure: count all workers
    }

    // Using myUnits to take inventory on all my units.

    //Army inventory.
    int army_count = MeatAIModule::Stock_Units( UnitTypes::Zerg_Zergling) +
        MeatAIModule::Stock_Units( UnitTypes::Zerg_Hydralisk)  +
        MeatAIModule::Stock_Buildings( UnitTypes::Zerg_Sunken_Colony ) +
        MeatAIModule::Stock_Buildings( UnitTypes::Zerg_Spore_Colony ) + 
        5; //ad-hoc, assumes a lot about the translation between these units, +5 is to avoid risk of /0 errors.

    //Tech Inventory.
    double tech_stock = log(
        Stock_Buildings( UnitTypes::Zerg_Extractor ) +
        Stock_Buildings( UnitTypes::Zerg_Spawning_Pool ) +
        Stock_Buildings( UnitTypes::Zerg_Evolution_Chamber ) +
        Stock_Buildings( UnitTypes::Zerg_Hydralisk_Den ) +
        Stock_Buildings( UnitTypes::Zerg_Queens_Nest ) +
        Stock_Ups( UpgradeTypes::Metabolic_Boost ) +
        Stock_Ups( UpgradeTypes::Zerg_Carapace ) +
        Stock_Ups( UpgradeTypes::Zerg_Melee_Attacks ) +
        Stock_Ups( UpgradeTypes::Pneumatized_Carapace ) +
        Stock_Ups( UpgradeTypes::Adrenal_Glands ) +
        Stock_Ups( UpgradeTypes::Grooved_Spines) +
        Stock_Ups( UpgradeTypes::Muscular_Augments) +
        Stock_Ups( UpgradeTypes::Zerg_Missile_Attacks) +
        Stock_Ups( UpgradeTypes::Antennae ) + 5 ); // adds 5 to avoid risk of divides by 0 issues, note log(1)=0. 

    //Inventories for knee-jerk states: Gas, Supply, mineral counter
      //Supply inventory:
    double ln_supply_remain = log( Stock_Supply( UnitTypes::Zerg_Overlord ) +
        Stock_Supply( UnitTypes::Zerg_Hatchery ) +
        Stock_Supply( UnitTypes::Zerg_Lair ) +
        Stock_Supply( UnitTypes::Zerg_Hive ) -
        Broodwar->self()->supplyUsed() + 1 ); // + 1 is a minor adjustment to avoid ln(0) errors or divides by 0 errors.
    double ln_supply_total = log( Broodwar->self()->supplyTotal() + 1 ); // + 5 is a minor adjustment to avoid ln(0) errors or divides by 0 errors.

    //Gas inventory.
    double ln_min = log( Broodwar->self()->minerals() + 5 ); // +5 is a minor adjustment to avoid ln(0) errors or divides by 0 errors.
    double ln_gas = log( Broodwar->self()->gas() + 5 ); // +5 is a minor adjustment to avoid ln(0) errors or divides by 0 errors.

  //Vision inventory: Map area could be initialized on startup, since maps do not vary once made.
    int map_x = Broodwar->mapWidth();
    int map_y = Broodwar->mapHeight();
    int map_area = map_x * map_y; // map area in tiles.
    int vision_tile_count = 5; // starting at 5 to avoid /0 issues. Should be profoundly rare and vision is usually in the thousands anyway.
    int* vision_tile_count_ptr = &vision_tile_count;

    for ( int tile_x = 1; tile_x <= map_x; tile_x++ ) { // there is no tile (0,0)
        for ( int tile_y = 1; tile_y <= map_y; tile_y++ ) {
            if ( Broodwar->isVisible( tile_x, tile_y ) ) {
                *vision_tile_count_ptr += 1;
            }
        }
    } // this search must be very exhaustive to do every frame. But C++ does it without any problems.

    //Knee-jerk states: gas, supply.
    gas_starved = (ln_gas / (ln_min + ln_gas)) < delta;
    supply_starved = Stock_Supply( UnitTypes::Zerg_Overlord ) + Stock_Supply( UnitTypes::Zerg_Hatchery ) + Stock_Supply( UnitTypes::Zerg_Lair ) + Stock_Supply( UnitTypes::Zerg_Hive ) - 
        Broodwar->self()->supplyUsed() <= 0 || // If you supply is negative, then you're supply blocked. no fancy ln 0 errors here.
        ( (ln_supply_remain / ln_supply_total) < gamma &&   //If your supply is disproportionately low, then you are gas starved, unless
            ln_supply_total <= log( 200 + 1 ) ); // you have not hit your supply limit, in which case you are not supply blocked

    // Cobb-Douglas 
    ln_y = alpha_vis * log( vision_tile_count / worker_count ) + alpha_army * log( army_count / worker_count ) + alpha_tech * log( tech_stock / worker_count ); //Analog to per capita GDP. Worker count has been incremented by 1 to avoid crashing from /0.
    ln_Y = alpha_vis * log( vision_tile_count ) + alpha_army * log( army_count ) + alpha_tech * log( tech_stock ) + alpha_econ * log( worker_count ); //Analog to GDP
        // so prioritization should be whichever is greatest: alpha_vis/vision_tile_count, alpha_army/army_count, alpha_tech/tech_stock, alpha_econ/worker_count.
        // double priority_list[] = { alpha_vis/vision_tile_count, alpha_army/army_count, alpha_econ/worker_count...}; // would be more elegant to use an array here...

    double econ_derivative = alpha_econ / worker_count * ( ( min_workers <= min_fields * 2) || (min_fields < 10) ); // econ is only in problems if undersaturated or less than 10 patches.
    double vision_derivative = alpha_vis / vision_tile_count;
    double army_derivative = alpha_army / army_count;
    double tech_derivative = alpha_tech / tech_stock * ( !Broodwar->self()->getUpgradeLevel( UpgradeTypes::Adrenal_Glands ) ); // you can't be tech starved if you have adrenal glands. You're done.

    // Set Priorities
    if ( econ_derivative >= vision_derivative &&
        econ_derivative >= army_derivative &&
        econ_derivative >= tech_derivative )
    {
        tech_starved = false;
        vision_starved = false;
        army_starved = false;
        econ_starved = true;
    }
    else if ( vision_derivative >= econ_derivative &&
        vision_derivative >= army_derivative &&
        vision_derivative >= tech_derivative )
    {
        tech_starved = false;
        vision_starved = true;
        army_starved = false;
        econ_starved = false;
    }
    else if ( army_derivative >= vision_derivative &&
        army_derivative >= econ_derivative &&
        army_derivative >= tech_derivative )
    {
        tech_starved = false;
        vision_starved = false;
        army_starved = true;
        econ_starved = false;
    }
    else if ( tech_derivative >= vision_derivative &&
        tech_derivative >= econ_derivative &&
        tech_derivative >= army_derivative )
    {
        tech_starved = true;
        vision_starved = false;
        army_starved = false;
        econ_starved = false;
    }
    else {
        Broodwar->drawTextScreen( 10, 200, " Status: Error No - Status ", Text::Size::Huge ); //
    }

    // Display the game status indicators at the top of the screen	
    Broodwar->drawTextScreen( 10, 0, "Inventory:", Text::DarkGreen ); // working on a better inventory.
    Broodwar->drawTextScreen( 10, 10, "Active Workers: %d", worker_count );
    Broodwar->drawTextScreen( 10, 20, "Active Miners: %d", min_workers );
    Broodwar->drawTextScreen( 10, 30, "Active Gas Miners: %d", gas_workers );
    Broodwar->drawTextScreen( 10, 40, "Possessed Min Fields: %d", min_fields );
    Broodwar->drawTextScreen( 10, 50, "Morphing Count: %d", morphing_count );  //
    Broodwar->drawTextScreen( 10, 60, "Idle Larve: %d", larve );  //
    Broodwar->drawTextScreen( 10, 70, "Total Drones: %d", drones );  //
    Broodwar->drawTextScreen( 10, 80, "Ovis: %d", (int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Overlord ) );  //
    Broodwar->drawTextScreen( 10, 90, "Hatches: %d", hatches );  // all hatch types included

    Broodwar->drawTextScreen( 10, 100, "Lings: %d", (int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Zergling ) );  //
    Broodwar->drawTextScreen( 10, 110, "Hydras: %d", (int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Hydralisk ) );  //
    Broodwar->drawTextScreen( 10, 120, "Pool: %d", (int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Spawning_Pool ) );  // 
    Broodwar->drawTextScreen( 10, 130, "Hydra Den: %d", (int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Hydralisk_Den ) );  // 
    Broodwar->drawTextScreen( 10, 140, "Extractor: %d", (int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Extractor ) );  //
    Broodwar->drawTextScreen( 10, 150, "Evos: %d", (int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Evolution_Chamber ) );  //

    Broodwar->drawTextScreen( 10, 255, "Game Status (Ln Y/L) : %f", ln_y, Text::Size::Huge ); //
    Broodwar->drawTextScreen( 10, 265, "Game Status (Ln Y) : %f", ln_Y, Text::Size::Huge ); //
    Broodwar->drawTextScreen( 10, 275, "Game Time: %d minutes", (Broodwar->elapsedTime()) / 60, Text::Size::Huge ); //

    Broodwar->drawTextScreen( 125, 0, "Econ Starved: %s", econ_starved ? "TRUE" : "FALSE" );  //
    Broodwar->drawTextScreen( 125, 10, "Army Starved: %s", army_starved ? "TRUE" : "FALSE" );  //
    Broodwar->drawTextScreen( 125, 20, "Vision Starved: %s", vision_starved ? "TRUE" : "FALSE" );  //
    Broodwar->drawTextScreen( 125, 30, "Tech Starved: %s", tech_starved ? "TRUE" : "FALSE" );  //

    Broodwar->drawTextScreen( 125, 50, "Supply Starved: %s", supply_starved ? "TRUE" : "FALSE" );
    Broodwar->drawTextScreen( 125, 60, "Gas Starved: %s", gas_starved ? "TRUE" : "FALSE" );

    Broodwar->drawTextScreen( 250, 0, "Econ Derivative (10k): %f", econ_derivative * 10000 );  //
    Broodwar->drawTextScreen( 250, 10, "Army Derivative (10k): %f", army_derivative * 10000 ); //
    Broodwar->drawTextScreen( 250, 20, "Vision Derivative (10k): %f", vision_derivative * 10000 ); //
    Broodwar->drawTextScreen( 250, 30, "Tech Derivative (10k): %f", tech_derivative * 10000 ); //

    Broodwar->drawTextScreen( 450, 100, "Gas (Pct.) : %f", ln_gas / (ln_min + ln_gas) );
    Broodwar->drawTextScreen( 450, 110, "Army Stock (Pct.): %f", army_count / ((double)worker_count + (double)army_count) ); //
    Broodwar->drawTextScreen( 450, 120, "Vision (Pct.): %f", vision_tile_count / (double)map_area );  //
    Broodwar->drawTextScreen( 450, 130, "Supply Heuristic: %f", ln_supply_remain / ln_supply_total );  //
    Broodwar->drawTextScreen( 450, 140, "Vision Tile Count: %d", vision_tile_count );  //
    Broodwar->drawTextScreen( 450, 150, "Map Area: %d", map_area );  //

    Broodwar->drawTextScreen( 500, 20, "Performance:" );  // 
    Broodwar->drawTextScreen( 500, 30, "APM: %d", Broodwar->getAPM() );  // 
    Broodwar->drawTextScreen( 500, 40, "APF: %f", (Broodwar->getAPM() / 60) / Broodwar->getAverageFPS() );  // 
    Broodwar->drawTextScreen( 500, 50, "FPS: %f", Broodwar->getAverageFPS() );  // 
    Broodwar->drawTextScreen( 500, 60, "Frames of Latency: %d", Broodwar->getLatencyFrames() );  //
    
 // Prevent spamming by only running our onFrame once every number of latency frames.
 // Latency frames are the number of frames before commands are processed.
    if ( Broodwar->getFrameCount() % Broodwar->getLatencyFrames() != 0 )
        return;

    // Iterate through all the units that we own
    for ( auto &u : Broodwar->self()->getUnits() )
    {
        // Ignore the unit if it no longer exists
        // Make sure to include this block when handling any Unit pointer!
        if ( !u->exists() )
            continue;

        // Ignore the unit if it has one of the following status ailments
        if ( u->isLockedDown() ||
            u->isMaelstrommed() ||
            u->isStasised() )
            continue;

        // Ignore the unit if it is in one of the following states
        if ( u->isLoaded() ||
            !u->isPowered() ||
            u->isStuck() )
            continue;

        // Ignore the unit if it is incomplete or busy constructing
        if ( !u->isCompleted() ||
            u->isConstructing() )
            continue;

        // Finally make the unit do some stuff!

        // If the unit is a worker unit
        if ( u->getType().isWorker() )
        {
            // Mining loop if our worker is idle (includes returning $$$) or not moving while gathering gas, we (re-) evaluate what they should be mining.  Original script uses isIdle() only. might have queues that are very long which is why they may be unresponsive.
            if ( isIdleEmpty( u ) || u->isCarryingGas() || u->isCarryingMinerals())
            {
                // Order workers carrying a resource to return them to the center,
                // otherwise find a mineral patch to harvest.
                // Before returning, every second tell them to move to their closest depot to refresh their logics.
                if ( u->isCarryingGas() || u->isCarryingMinerals() ) // Return $$$
                {
                    u->returnCargo( true );
                }//Closure: returning $$ loop
                else if ( !u->getPowerUp() )  // The worker cannot harvest anything if it
                {                             // is carrying a powerup such as a flag

                // Idle worker then Harvest from the nearest mineral patch or gas refinery, depending on need.
                    bool enough_gas = !gas_starved ||
                        ( (int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Extractor ) - Broodwar->self()->incompleteUnitCount( UnitTypes::Zerg_Spawning_Pool ) ) == 0 ||
                        gas_workers >= 3 * (int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Extractor ) ;  // enough gas if (many critera), incomplete extractor, or not enough gas workers for your extractors.

                    bool enough_minerals = min_workers >= 2 * min_fields;

                    if ( enough_minerals || !enough_gas ) // both cases are critical
                    {
                        u->gather( u->getClosestUnit( IsRefinery && IsOwned ), true ); 
                    }
                    if ( enough_gas ) // pull if we are full.
                    {
                        u->gather( u->getClosestUnit( IsMineralField ), true );
                    }
                } // closure:powerup loop
            }// Closure: idle loop

        // Building subloop. Resets every few frames.
            if ( (isIdleEmpty( u ) || IsGatheringMinerals( u )) &&
                t_build + 50 < Broodwar->getFrameCount() )
            { //only get those that are idle or gathering minerals, but not carrying them. This always irked me. 

                t_build = Broodwar->getFrameCount();

                //Gas Buildings
                Check_N_Build( UnitTypes::Zerg_Extractor, u, gas_workers >= 3 * (int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Extractor ) &&
                    (gas_starved || tech_starved) );

                //Basic Buildings
                Check_N_Build( UnitTypes::Zerg_Spawning_Pool, u, !econ_starved &&
                    (int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Spawning_Pool ) == 0 );

                //Tech Buildings

                Check_N_Build( UnitTypes::Zerg_Hydralisk_Den, u, tech_starved && 
                    (int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Spawning_Pool ) > 0 &&
                    (int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Hydralisk_Den ) == 0 );

                Check_N_Build( UnitTypes::Zerg_Evolution_Chamber, u, tech_starved &&
                    (int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Evolution_Chamber ) < 2 && // This has resolveed our issues with 4x evo chambers
                    (int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Spawning_Pool ) > 0 );

                Check_N_Build( UnitTypes::Zerg_Queens_Nest, u, tech_starved &&
                    (int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Queens_Nest ) == 0 &&
                    (int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Lair ) >= 0 );

                //Expo loop
                if ( !army_starved || ln_min >= log( UnitTypes::Zerg_Hatchery.mineralPrice() + 1 ))
                {  // build expos when safe or desparate or floating minerals) {
                    Unitset resource_grps = Broodwar->getMinerals();
                    double dist = 999999;
                        double* dist_ptr = &dist;
                    int expo_x;
                        int* expo_x_ptr = &expo_x;
                    int expo_y;
                        int* expo_y_ptr = &expo_y;
                    if ( !resource_grps.empty() ) {
                        for ( auto expo = resource_grps.begin(); expo != resource_grps.end(); ++expo ) { // search for closest resource group
                            int expo_x_tile_temp = (*expo)->getTilePosition().x;
                            int expo_y_tile_temp = (*expo)->getTilePosition().y;
                            int expo_x_px_temp = (*expo)->getPosition().x;
                            int expo_y_px_temp = (*expo)->getPosition().y; //getDistance is not overloaded for tile positions
                                 Broodwar->drawCircleScreen( (*expo)->getPosition(), 800, Colors::Yellow );

                            Unitset bases = Broodwar->getUnitsInRadius( expo_x_px_temp, expo_y_px_temp, 1024, Filter::IsResourceDepot && Filter::IsOwned ); 
                            Unitset gas_check = Broodwar->getUnitsInRadius( expo_x_px_temp, expo_y_px_temp, 250, Filter::ResourceGroup );

                            if ( bases.empty() && !gas_check.empty() ) { // check if there are NO bases with 1024p of this resource center, AND gases are inside 250.
                                for ( auto g = gas_check.begin(); g != gas_check.end(); ++g ) {
                                    if ( (*g)->getType() == UnitTypes::Resource_Vespene_Geyser ) {
                                        double dist_temp = u->getDistance( { expo_x_px_temp, expo_y_px_temp } ); //getDistance is not overloaded for tile positions
                                        if ( dist_temp < dist ) {
                                            *dist_ptr = dist_temp;  // if it is closer, but not immediately next to our main, update the new closest distance.
                                            *expo_x_ptr = expo_x_tile_temp;
                                            *expo_y_ptr = expo_y_tile_temp; // and update the location of that distance.
                                        } // get location of expo.
                                    } // Closure confirm does the expo have gas?
                                } // closure begin gas check
                            } // Look through the geyesers and find the closest with gas.
                        }// closure: loop through all resource group
                    }//closure search through all resource groups.
                    TilePosition buildPosition = Broodwar->getBuildLocation( UnitTypes::Zerg_Hatchery, { expo_x , expo_y }, 50 );  // build the expo near the nearest resource feature
                    u->build( UnitTypes::Zerg_Hatchery, buildPosition );
                    PrintError_Unit( u );
                    t_build += 125;
                }// closure: Expo methodology.

                 //Combat Buildings
                Check_N_Build( UnitTypes::Zerg_Creep_Colony, u, army_starved &&  // army starved.
                    (int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Creep_Colony ) == 0 && // no creep colonies.
                    (((int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Spawning_Pool ) > 0 && (int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Spawning_Pool ) > Broodwar->self()->incompleteUnitCount( UnitTypes::Zerg_Spawning_Pool )) ||
                    ((int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Evolution_Chamber ) > 0 && (int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Evolution_Chamber ) > Broodwar->self()->incompleteUnitCount( UnitTypes::Zerg_Evolution_Chamber ))) );   // And there is a building complete that will allow either creep colony upgrade.

                // Macro Hatches, as needed. Don't macro hatch if you are trying to expand.
                Check_N_Build( UnitTypes::Zerg_Hatchery, u, ln_min >= log( UnitTypes::Zerg_Hatchery.mineralPrice() * 2 + 1 ) );

            } // Close Build loop
        } // Close Worker management loop

        // Unit creation & Hatchery management loop
        if ( u->getType().isResourceDepot() ) // A resource depot is a Command Center, Nexus, or Hatchery.
           //Does a hatchery count as idle if it is researching but has idle larve? Might have to directly select larve at this point.  These should not be ELSE if loops because we can do all of them with multiple larva.
        {
            //Supply blocked protection loop.
            if ( supply_starved &&
                !u->train( UnitTypes::Zerg_Overlord ) ) {
                PrintError_Unit( u );
            } // closure: construct more supply
              //Army build/replenish loop.  Note army also serves the purpose of scouting.
            if ( !econ_starved &&  //tech starved here is to escape our tech cap, tech can't advance beyond a certain point.
                (int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Hydralisk_Den ) > 0 &&
                !u->train( UnitTypes::Zerg_Hydralisk ) )
            {
                PrintError_Unit( u );
            } // Closure: army loop
            else if ( !econ_starved &&
                (int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Spawning_Pool ) > 0 &&
                !u->train( UnitTypes::Zerg_Zergling ) ) // note this will only build lings after it's checked for hydras.
            {
                PrintError_Unit( u );
            } 
              //Econ Build/replenish loop. Will build workers if I have no spawning pool, or if there is a worker shortage.
            if ( ((int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Spawning_Pool ) == 0 || econ_starved) &&  // if you're eco starved, we could use more workers.
                min_workers < min_fields * 2 && // unless you are fully saturated.
                !u->train( u->getType().getRace().getWorker() ) )
            {
                PrintError_Unit( u );
            } // closure: failed to train worker unit
        } //Closure : Resource depot loop

        //Scouting/vision loop. Very primative. move or attack-move with brownian motion, its map vision in any direction.
        if ( MeatAIModule::isIdleEmpty( u ) &&
            ( u->getType() == UnitTypes::Zerg_Zergling || u->getType() == UnitTypes::Zerg_Overlord ) ) //Scout if you're an overlord or a ling. And can move.
        {
            if( vision_starved ) {
                Brownian_Stutter( u, 5 );
            }
            else if ( army_starved ) {
                Unit r = u->getClosestUnit( Filter::IsResourceDepot && Filter::IsOwned);
                u->move( r->getPosition() );
            }
        } // If it is a combat unit, then use it to attack the enemy.	

        //Upgrade loop:
        if ( tech_starved &&
            MeatAIModule::isIdleEmpty( u ) &&
            (u->canUpgrade() || u->canResearch() || u->canMorph()) &&
            ( u->getType() == UnitTypes::Zerg_Hive ||
                u->getType() == UnitTypes::Zerg_Lair ||
                u->getType() == UnitTypes::Zerg_Hatchery ||
                u->getType() == UnitTypes::Zerg_Spawning_Pool ||
                u->getType() == UnitTypes::Zerg_Evolution_Chamber ||
                u->getType() == UnitTypes::Zerg_Hydralisk_Den)) { // this will need to be revaluated once I buy units that cost gas.

            Check_N_Upgrade( UpgradeTypes::Metabolic_Boost, u, true );
            Check_N_Upgrade( UpgradeTypes::Zerg_Melee_Attacks, u, true );
            Check_N_Upgrade( UpgradeTypes::Zerg_Carapace, u, true );
            Check_N_Upgrade( UpgradeTypes::Muscular_Augments, u, true );
            Check_N_Upgrade( UpgradeTypes::Grooved_Spines, u, true );
            Check_N_Upgrade( UpgradeTypes::Zerg_Missile_Attacks, u, true );

            Check_N_Build( UnitTypes::Zerg_Lair, u, (tech_derivative > army_derivative) &&
                MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Lair ) == 0 &&
                MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Evolution_Chamber ) > 0 &&
                u->getType() == UnitTypes::Zerg_Hatchery );
            Check_N_Upgrade( UpgradeTypes::Pneumatized_Carapace, u, MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Lair ) > 0);
            Check_N_Upgrade( UpgradeTypes::Antennae, u, MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Lair ) > 0 );

            Check_N_Build( UnitTypes::Zerg_Hive, u, (tech_derivative > army_derivative) &&
                MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Queens_Nest ) >= 0 &&
                u->getType() == UnitTypes::Zerg_Lair && MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Hive ) == 0 ); //If you're tech-starved at this point, stop.
            Check_N_Upgrade( UpgradeTypes::Adrenal_Glands, u, MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Hive ) > 0 );

            PrintError_Unit( u );
        } //closure: tech/upgrades loop

        //Combat loop. Has some sophistication at this time. Makes retreat/attack decision.  Only retreat if your army is not up to snuff. Only combat units retreat. Only retreat if the enemy is near.
        bool retreat = false;
            bool* retreat_ptr = &retreat;

        Unit e = u->getClosestUnit( Filter::IsEnemy ); //Consider combat when there is at least a single enemy.
        if ( e && e->canAttack() &&
            army_starved &&
            u->canMove() ) {   // run if you cannot attack him or you are army starved.
                 *retreat_ptr = true;
            }// closure: check for retreat bool.
        if( e && e->canAttack() &&
            retreat &&
            u->canMove() ){ // Needs there to be an enemy
                    Position e_pos = e->getPosition();
                    Position pos = u->getPosition();
                    Broodwar->drawCircleMap( e_pos, 300, Colors::Red );

                    int dist_x = e_pos.x - pos.x;
                    int dist_y = e_pos.y - pos.y;

                    //double dist = pow(pow(dist_x, 2) + pow(dist_y, 2), 0.5);
                    int dist = u->getDistance( e_pos );
                    if ( dist < 350 ) { // a buffer of 50 pixels beyond my "bare mininimum" safe zone of 300. Run if you're a noncombat unit or army starved.
                        Position retreat_spot = { pos.x - ((dist_x > 0) - (dist_x < 0)) * 300 , pos.y - ((dist_y > 0) - (dist_y < 0)) * 300 }; // the > < functions find the signum, no such function exists in c++!
                        if ( retreat_spot.isValid() ) {
                            u->move( retreat_spot ); //identify vector between yourself and e.  go 350 pixels away in the quadrant furthest from them.
                            Broodwar->drawLineMap( pos, retreat_spot, Colors::White );
                        }
                    }
                } // closure retreat action
        else if ( e &&
            MeatAI::UnitUtil::IsCombatUnit( u ) &&
            u->canAttack() &&
            !retreat ) { //Intiatite combat if: we have army, enemies are present, we are combat units and we are not retreating.

            Unit target_caster = u->getClosestUnit( Filter::IsEnemy && (Filter::IsSpellcaster && !Filter::IsAddon && !Filter::IsBuilding) && !Filter::IsCloaked, 500 ); // don't allin buildings pz. Thanks. Only charge casters if they are nearby.
            if ( target_caster ) { // Neaby casters
                u->attack( target_caster );
                Broodwar->drawLineMap( u->getPosition(), target_caster->getPosition(), Colors::Yellow );
            } 
            else if ( !target_caster ) { // then nearby combat units.
                Unit target_warrior = u->getClosestUnit( Filter::IsEnemy && (Filter::CanAttack || Filter::SpaceProvided) && !Filter::IsCloaked, 500 ); // is warrior (or bunker, man those are annoying)
                if ( target_warrior ) {
                    u->attack( target_warrior );
                    Broodwar->drawLineMap( u->getPosition(), target_warrior->getPosition(), Colors::Yellow );
                } 
                else if ( !target_warrior ) { // then whatever is closest (and already found)
                    u->attack( e );
                    Broodwar->drawLineMap( u->getPosition(), e->getPosition(), Colors::Yellow );
                }
            }   
            // grabbing detectors as needed.

            Unit c = u->getClosestUnit( Filter::IsEnemy && (Filter::IsCloakable || Filter::IsBurrowable) ); //Consider combat when there is at least a single enemy. Recall burrow and cloak are different.
            if ( c ) {
                Unit d = c->getClosestUnit( Filter::IsDetector && Filter::CanMove );  // pull the closest detector
                if ( d ) {
                    d->move( c->getPosition() );
                    Broodwar->drawCircleMap( c->getPosition(), 25, Colors::Cyan );
                    Broodwar->drawLineMap( d->getPosition(), c->getPosition(), Colors::Cyan );
                }
            } // Grab detectors as needed. Don't send them to their deaths alone.
        }// closure Combat loop- A logic chain really.

         //Creep Colony upgrade loop.  We are more willing to upgrade them than to build them, since the units themselves are useless in the base state.
        if ( u->getType() == UnitTypes::Zerg_Creep_Colony &&
            (int)(MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Spawning_Pool ) > 0 || (int)MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Evolution_Chamber ) > 0) ) {
            if ( MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Spawning_Pool ) > 0 &&
                MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Evolution_Chamber ) > 0 ) {
                if ( e && e->isFlying()) {
                    Check_N_Build( UnitTypes::Zerg_Spore_Colony, u, true );
                }
                else {
                    Check_N_Build( UnitTypes::Zerg_Sunken_Colony, u, true );
                }
            } // build one of the two colonies at random if you have both prequisites
            else if ( MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Spawning_Pool ) > 0 &&
                MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Evolution_Chamber ) == 0 ) {
                Check_N_Build( UnitTypes::Zerg_Sunken_Colony, u, true );
            } // build sunkens if you only have that
            else if ( MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Spawning_Pool ) == 0 &&
                MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Evolution_Chamber ) > 0 ) {
                Check_N_Build( UnitTypes::Zerg_Spore_Colony, u, true );
            } // build spores if you only have that.
        } // closure: Creep colony loop

    } // closure: unit iterator
     


} // closure: Onframe

void MeatAIModule::onSendText( std::string text )
{

    // Send the text to the game if it is not being processed.
    Broodwar->sendText( "%s", text.c_str() );

    // Make sure to use %s and pass the text as a parameter,
    // otherwise you may run into problems when you use the %(percent) character!

}

void MeatAIModule::onReceiveText( BWAPI::Player player, std::string text )
{
    // Parse the received text
    Broodwar << player->getName() << " said \"" << text << "\"" << std::endl;
}

void MeatAIModule::onPlayerLeft( BWAPI::Player player )
{
    // Interact verbally with the other players in the game by
    // announcing that the other player has left.
    Broodwar->sendText( "Goodbye %s!", player->getName().c_str() );
}

void MeatAIModule::onNukeDetect( BWAPI::Position target )
{

    // Check if the target is a valid position
    if ( target )
    {
        // if so, print the location of the nuclear strike target
        Broodwar << "Nuclear Launch Detected at " << target << std::endl;
    }
    else
    {
        // Otherwise, ask other players where the nuke is!
        Broodwar->sendText( "Where's the nuke?" );
    }

    // You can also retrieve all the nuclear missile targets using Broodwar->getNukeDots()!
}

void MeatAIModule::onUnitDiscover( BWAPI::Unit unit )
{

}

void MeatAIModule::onUnitEvade( BWAPI::Unit unit )
{
}

void MeatAIModule::onUnitShow( BWAPI::Unit unit )
{
}

void MeatAIModule::onUnitHide( BWAPI::Unit unit )
{
}

void MeatAIModule::onUnitCreate( BWAPI::Unit unit )
{
    if ( Broodwar->isReplay() )
    {
        // if we are in a replay, then we will print out the build order of the structures
        if ( unit->getType().isBuilding() && !unit->getPlayer()->isNeutral() )
        {
            int seconds = Broodwar->getFrameCount() / 24;
            int minutes = seconds / 60;
            seconds %= 60;
            Broodwar->sendText( "%.2d:%.2d: %s creates a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str() );
        }
    }
}

void MeatAIModule::onUnitDestroy( BWAPI::Unit unit )
{
}

void MeatAIModule::onUnitMorph( BWAPI::Unit unit )
{
    if ( Broodwar->isReplay() )
    {
        // if we are in a replay, then we will print out the build order of the structures
        if ( unit->getType().isBuilding() &&
            !unit->getPlayer()->isNeutral() )
        {
            int seconds = Broodwar->getFrameCount() / 24;
            int minutes = seconds / 60;
            seconds %= 60;
            Broodwar->sendText( "%.2d:%.2d: %s morphs a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str() );
        }
    }
}

void MeatAIModule::onUnitRenegade( BWAPI::Unit unit )
{
}

void MeatAIModule::onSaveGame( std::string gameName )
{
    Broodwar << "The game was saved to \"" << gameName << "\"" << std::endl;
}

void MeatAIModule::onUnitComplete( BWAPI::Unit unit )
{
}


// Personally assembled functions are here

//Checks if a building can be built, and passes additional boolean criteria.  If all critera are passed, then it builds the building and delays the building timer 25 frames, or ~1 sec. It may now allow morphing, eg, lair, hive and lurkers, but this has not yet been tested.
void MeatAIModule::Check_N_Build( UnitType building, Unit unit, bool extra_critera )
{
    if ( unit->canBuild( building ) &&
        Broodwar->self()->minerals() >= building.mineralPrice() &&
        Broodwar->self()->gas() >= building.gasPrice() &&
        extra_critera )
    {
        TilePosition buildPosition = Broodwar->getBuildLocation( building, unit->getTilePosition(), 64, building == UnitTypes::Zerg_Creep_Colony );
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

//Forces a unit to stutter in a brownian manner. Size of stutter is unit's (vision range * n ). Will attack if it sees something.
void MeatAIModule::Brownian_Stutter( Unit unit, int n ) {

    Position pos = unit->getPosition();

    //if (full_map == true) {
    //    int x_loc = (rand() % 100 + 1) * (32 * Broodwar->mapWidth()) / 100;
    //    int y_loc = (rand() % 100 + 1) * (32 * Broodwar->mapHeight()) / 100;
    //    Position brownian_pos = { x_loc , y_loc }; //tiles are 32x32. Watch for int/double errors.  This is an option considered but not implemented.
    //} else

    int x_stutter = n * (rand() % 100 - 50) / 25 * unit->getType().sightRange();
    int y_stutter = n * (rand() % 100 - 50) / 25 * unit->getType().sightRange();

    Position brownian_pos = { pos.x + x_stutter , pos.y + y_stutter }; // note this function generates a number -49..49 then divides by 25, and is int. So it will truncate the value, producing int -1..+1. The naieve approach of rand()%3 - 1 is "distressingly non-random in lower order bits" according to stackoverflow and other sources.

    if ( unit->canAttack( brownian_pos ) ) {
        unit->attack( brownian_pos );
    }
    else {
        unit->move( brownian_pos );
    }
};

// Gets units last error and prints it directly onscreen.
void MeatAIModule::PrintError_Unit( Unit unit ) {
    Position pos = unit->getPosition();
    Error lastErr = Broodwar->getLastError();
    Broodwar->registerEvent( [pos, lastErr]( Game* ) { Broodwar->drawTextMap( pos, "%c%s", Text::Red, lastErr.c_str() ); },   // action
        nullptr,    // condition
        Broodwar->getLatencyFrames() );  // frames to run.
}

// An improvement on existing idle scripts. Returns true if it is idle. Checks if it is a laden worker, idle, or stopped. 
bool MeatAIModule::isIdleEmpty( Unit unit ) {
    bool laden_worker = unit->isCarryingGas() || unit->isCarryingMinerals();
    bool idle = unit->isIdle() || !unit->isMoving();
    return idle && !laden_worker;
}

// evaluates the value of a stock of buildings, in terms of total cost (min+gas). Assumes building is zerg and therefore, a drone was spent on it.
int MeatAIModule::Stock_Buildings( UnitType building ) {
    int cost = building.mineralPrice() + building.gasPrice() + 50;
    int instances = MeatAI::UnitUtil::GetAllUnitCount( building );
    int total_stock = cost * instances;
    return total_stock;
}

// evaluates the value of a stock of upgrades, in terms of total cost (min+gas).
int MeatAIModule::Stock_Ups( UpgradeType ups ) {
    int cost = ups.mineralPrice() + ups.gasPrice();
    int instances = Broodwar->self()->getUpgradeLevel( ups );
    int total_stock = cost * instances;
    return total_stock;
}

// evaluates the value of a stock of units, in terms of total cost (min+gas). Doesn't consider the counterfactual larva.
int MeatAIModule::Stock_Units( UnitType unit ) {
    int cost = unit.mineralPrice() + unit.gasPrice();
    int instances = MeatAI::UnitUtil::GetAllUnitCount( unit );
    int total_stock = cost * instances;
    return total_stock;
}

// evaluates the value of a stock of unit, in terms of supply added.
int MeatAIModule::Stock_Supply( UnitType unit ) {
    int supply = unit.supplyProvided();
    int instances = MeatAI::UnitUtil::GetAllUnitCount( unit );
    int total_stock = supply * instances;
    return total_stock;
}

//void MeatAIModule::Inventory()
