#include "MeatAIModule.h"
#include <iostream> 
#include <fstream> // for file read/writing
#include <chrono> // for in-game frame clock.


// MeatAI V1.00. Current V goal-> Clean up issues.
// seperate interface, implementation, and usage.
// Unresolved issue: Text coloration in on screen messages. Screen messages doesn't take color commands, but does take printf format commands. Gets messy very fast.
// unresolved issue: workers will overmine a mineral patch area. Or gas geyser.
// use trycatch for /0 errors and return to the true values, no +X's.
// generalize combat logic. Will help with bugcatching.
// roll over from hydras to lings if insufficient gas.
// Crashes upon loss sometimes. I think if my refinery is last? Happened vs terran.  Happened with creep colony alone as last building. Curious.  I think it has to do with the .empty() method. Probably bases.empty() doesn't do what I think it does.

// Bugs and goals.
// better check on being "full of drones"
// generate an inventory system for researches and buildings. Probably as an array. Perhaps expand upon the self class?  Consider your tech inventory.
// Would like to someday not overload unit queue, but spamming is much easier.
// Caution: algorithm gets bamboozled if any of the derivatives are invinity. Logic for starvation fails (I think it returns nulls), so do the derivative comparisons.
// remove all dependance on UnitUtil, move getAllUnit command to my own module.
// interest in actual map exploration vs raw vision? Interested in mineral vision?
// creep colonies should spore on inside, sunken on outside. 
// Check-and-attack function?
// begin pulling out functions by section.
// can upgrade carapace after adrenals still. So tech cap is slightly confused, stops at 2/2/2 not 3/3/3
// consider hp in unit stock.
// extractor trick.

using namespace BWAPI;
using namespace Filter;
using namespace std;

void MeatAIModule::onStart()
{

    diagnostic_mode = true;

    //Initialize state variables
    gas_starved = false;
    army_starved = false;
    supply_starved = false;
    vision_starved = false;
    econ_starved = true;
    tech_starved = false;

     *short_delay_ptr = 0;
     *med_delay_ptr = 0;
     *long_delay_ptr = 0;

    srand( Broodwar->getRandomSeed() );
    //Initialize model variables. Current best: {0.45, 0.70}
    delta = 0.45 * (rand() % 10 + 96) / (double)100; //gas starved parameter. Triggers state if: ln_gas/(ln_min + ln_gas) < delta;  Higher is more gas. Current best: 0.45.  0.35 is basically all lings+tech for lings.
    gamma = 0.70 * (rand() % 10 + 96) / (double)100; //supply starved parameter. Triggers state if: ln_supply_remain/ln_supply_total < gamma; Current best is 0.70. Some good indicators that this is reasonable: ln(4)/ln(9) is around 0.63, ln(3)/ln(9) is around 0.73, so we will build our first overlord at 7/9 supply. ln(18)/ln(100) is also around 0.63, so we will have a nice buffer for midgame.  

    //Cobb-Douglas Production exponents.  Should all sum to one. Current best {0.450,0.700,0.009, 0.002} leads to illustratively reasonable- some drone scouting, etc. Prior to adding hydra tech option.
    alpha_army = 0.525 * (rand() % 10 + 96) / (double)100; // army starved parameter. This comes in a medium scale
    alpha_vis = 0.750 * (rand() % 10 + 96) / (double)100 ; // vision starved parameter. Note the very large scale for vision, vision comes in groups of thousands. Since this is not scale free, the delta must be larger or else it will always be considered irrelevant.
    alpha_econ = 0.009 * (rand() % 10 + 96) / (double)100; // econ starved parameter. This comes in a small scale.
    alpha_tech = 0.002 * (rand() % 10 + 96) / (double)100; // tech starved parameter. This comes in a small

  // Hello World!
    Broodwar->sendText( "Hello world! This is MeatShieldAI V1.00" );

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
{// Called when the game ends
   ofstream output; // Prints to brood war file proper.
   output.open( "output.csv", ios_base::app );
   //output << "delta (gas)" << "," << "gamma (supply)" << ',' << "alpha_army" << ',' << "alpha_vis" << ',' << "alpha_econ" << ',' << "alpha_tech" << ',' << "Race" << "," << "Won" << "Seed" << endl;
   output << delta  << "," << gamma << ',' << alpha_army << ',' << alpha_vis << ',' << alpha_econ << ',' << alpha_tech << ',' << Broodwar->enemy()->getRace() << "," << isWinner << ',' << short_delay << ',' << med_delay << ',' << long_delay << ',' << Broodwar->getRandomSeed() << endl;
   output.close();
}

void MeatAIModule::onFrame()
{ // Called once every game frame

  // Return if the game is a replay or is paused
    if ( Broodwar->isReplay() || Broodwar->isPaused() || !Broodwar->self() )
        return;
    // Start Game clock.
    try {

        // Performance Qeuery Timer
        // http://www.decompile.com/cpp/faq/windows_timer_api.htm
        auto start = std::chrono::high_resolution_clock::now();

        //Define important variables.

        // Game time;
        int t_game = Broodwar->getFrameCount();

        //Unit inventory.
        int hatches = Count_Units( UnitTypes::Zerg_Hatchery ) +
            Count_Units( UnitTypes::Zerg_Lair ) +
            Count_Units( UnitTypes::Zerg_Hive );
        int ovis = Count_Units( UnitTypes::Zerg_Overlord );
        int larve = Count_Units( UnitTypes::Zerg_Larva );
        int morphing_count = Broodwar->self()->incompleteUnitCount( UnitTypes::Buildings );
        int pool = Count_Units( UnitTypes::Zerg_Spawning_Pool );
        int extractor = Count_Units( UnitTypes::Zerg_Extractor );
        int evos = Count_Units( UnitTypes::Zerg_Evolution_Chamber );
        int drones = Count_Units( UnitTypes::Zerg_Drone );

        //Idea: Count_Units(UnitCommandTypes::Build)  What command reads Unitcommandtype chars?

        //Econ inventory 
        int worker_count = 1; // adds 1 to avoid divide by 0 error.
        int* worker_count_ptr = &worker_count;
        int gas_workers = 0;
        int* gas_workers_ptr = &gas_workers;
        int min_workers = 0;
        int* min_workers_ptr = &min_workers;
        int min_fields = 1;
        int* min_fields_ptr = &min_fields;

        // Using myUnits to take inventory on all my units.

        //Army inventory.
        int army_count = Stock_Units( UnitTypes::Zerg_Zergling ) +
            Stock_Units( UnitTypes::Zerg_Hydralisk ) +
            //Stock_Buildings( UnitTypes::Zerg_Sunken_Colony ) +
            //Stock_Buildings( UnitTypes::Zerg_Spore_Colony ) +
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
            Stock_Ups( UpgradeTypes::Grooved_Spines ) +
            Stock_Ups( UpgradeTypes::Muscular_Augments ) +
            Stock_Ups( UpgradeTypes::Zerg_Missile_Attacks ) +
            Stock_Ups( UpgradeTypes::Antennae ) + 5 ); // adds 5 to avoid risk of divides by 0 issues, note log(1)=0. 

        //Inventories for knee-jerk states: Gas, Supply, mineral counter
          //Supply inventory:
        double ln_supply_remain = log( Stock_Supply( UnitTypes::Zerg_Overlord ) +
            Stock_Supply( UnitTypes::Zerg_Hatchery ) +
            Stock_Supply( UnitTypes::Zerg_Lair ) +
            Stock_Supply( UnitTypes::Zerg_Hive ) -
            Broodwar->self()->supplyUsed() + 1 ); // + 1 is a minor adjustment to avoid ln(0) errors or divides by 0 errors.
        double ln_supply_total = log( Broodwar->self()->supplyTotal() + 2 ); // + 5 is a minor adjustment to avoid ln(0) errors or divides by 0 errors.

        //Gas inventory.
        double ln_min = log( Broodwar->self()->minerals() + 2 ); // +2 is a minor adjustment to avoid ln(0) errors or divides by 0 errors.
        double ln_gas = log( Broodwar->self()->gas() + 2 ); // +2 is a minor adjustment to avoid ln(0) errors or divides by 0 errors.


      //Vision inventory: Map area could be initialized on startup, since maps do not vary once made.
        int map_x = Broodwar->mapWidth();
        int map_y = Broodwar->mapHeight();
        int map_area = map_x * map_y; // map area in tiles.
        int vision_tile_count = 1; // starting at 1 to avoid /0 issues. Should be profoundly rare and vision is usually in the thousands anyway.
        int* vision_tile_count_ptr = &vision_tile_count;

        for ( int tile_x = 1; tile_x <= map_x; tile_x++ ) { // there is no tile (0,0)
            for ( int tile_y = 1; tile_y <= map_y; tile_y++ ) {
                if ( Broodwar->isVisible( tile_x, tile_y ) ) {
                    *vision_tile_count_ptr += 1;
                }
            }
        } // this search must be very exhaustive to do every frame. But C++ does it without any problems.

          //maintains an active inventory of minerals near our bases.
        Unitset resource = Broodwar->getMinerals(); // get any mineral field that exists on the map.
        if ( !resource.empty() ) { // check if the minerals exist
            for ( auto r = resource.begin(); r != resource.end() && !resource.empty() ; ++r ) { //for each mineral
                //Broodwar->drawCircleMap( (*r)->getPosition(), 300, Colors::Green ); // Circle each mineral.
                if ( (*r) && (*r)->exists() ) {
                    Unitset mybases = Broodwar->getUnitsInRadius( (*r)->getPosition(), 250, Filter::IsResourceDepot && Filter::IsOwned ); // is there a mining base near there
                    if ( !mybases.empty() ) { // check if there is a base nearby
                        for ( auto base = mybases.begin(); base != mybases.end() && !mybases.empty(); ++base ) {
                            if ( (*base)->exists() ) {
                                Broodwar->drawCircleMap( (*base)->getPosition(), 250, Colors::Green ); // if nearby base, circle all relevant bases.
                            }
                        }
                        (*min_fields_ptr)++; // count this mineral if there is a base near it.
                    } // closure if base is nearby
                }
            }// closure: Checking things nearby minerals.
        } // closure, mineral tally.

          // Get worker tallies.
        Unitset myUnits = Broodwar->self()->getUnits(); // out of my units  Probably easier than searching the map for them.
        if ( !myUnits.empty() ) { // make sure this object is valid!
            for ( auto u = myUnits.begin(); u != myUnits.end() && !myUnits.empty(); ++u )
            {
                if ( (*u) && (*u)->exists() ){
                    if ( (*u)->getType().isWorker() ) {
                        if ( (*u)->isGatheringGas() || (*u)->isCarryingGas() ) // implies exists and isCompleted
                        {
                            ++(*gas_workers_ptr);
                        }
                        if ( (*u)->isGatheringMinerals() || (*u)->isCarryingMinerals() ) // implies exists and isCompleted
                        {
                            ++(*min_workers_ptr);
                        }
                        *worker_count_ptr = gas_workers + min_workers + 1;  //adds 1 to avoid a divide by 0 error
                    } // closure: Only investigate closely if they are drones.
               } // Closure: only investigate on existance of unit..
            } // closure: count all workers
        }

        //Knee-jerk states: gas, supply.
        gas_starved = (ln_gas / (ln_min + ln_gas)) < delta;
        supply_starved = Broodwar->self()->supplyTotal() <= Broodwar->self()->supplyUsed() || // If you're supply blocked then you are supply starved.
            ((ln_supply_remain / ln_supply_total) < gamma &&   //If your supply is disproportionately low, then you are gas starved, unless
            (Broodwar->self()->supplyTotal() <= 400)); // you have not hit your supply limit, in which case you are not supply blocked. The real supply goes from 0-400, since lings are 0.5 observable supply.

        // Cobb-Douglas 
        ln_y = alpha_vis * log( vision_tile_count / worker_count ) + alpha_army * log( army_count / worker_count ) + alpha_tech * log( tech_stock / worker_count ); //Analog to per capita GDP. Worker count has been incremented by 1 to avoid crashing from /0.
        ln_Y = alpha_vis * log( vision_tile_count ) + alpha_army * log( army_count ) + alpha_tech * log( tech_stock ) + alpha_econ * log( worker_count ); //Analog to GDP
            // so prioritization should be whichever is greatest: alpha_vis/vision_tile_count, alpha_army/army_count, alpha_tech/tech_stock, alpha_econ/worker_count.
            // double priority_list[] = { alpha_vis/vision_tile_count, alpha_army/army_count, alpha_econ/worker_count...}; // would be more elegant to use an array here...

        double econ_derivative = alpha_econ / worker_count * ((min_workers <= min_fields * 2) || (min_fields < 24)); // econ is only a possible problem if undersaturated or less than 62 patches.
        double vision_derivative = alpha_vis / vision_tile_count;
        double army_derivative = alpha_army / army_count * (Broodwar->self()->supplyTotal()<=400);  // can't be army starved if you are maxed out.
        double tech_derivative = alpha_tech / tech_stock * 
            !(Broodwar->self()->getUpgradeLevel( UpgradeTypes::Adrenal_Glands ) &&
                Broodwar->self()->getUpgradeLevel( UpgradeTypes::Zerg_Carapace ) == 3 &&
                Broodwar->self()->getUpgradeLevel( UpgradeTypes::Zerg_Melee_Attacks ) == 3 && 
                Broodwar->self()->getUpgradeLevel( UpgradeTypes::Zerg_Missile_Attacks ) == 3); // you can't be tech starved if you have adrenal glands. You're done.

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
        if ( diagnostic_mode ) {
            Broodwar->drawTextScreen( 10, 0, "Inventory:", Text::DarkGreen ); // working on a better inventory.
            Broodwar->drawTextScreen( 10, 10, "Active Workers: %d", worker_count );
            Broodwar->drawTextScreen( 10, 20, "Active Miners: %d", min_workers );
            Broodwar->drawTextScreen( 10, 30, "Active Gas Miners: %d", gas_workers );
            Broodwar->drawTextScreen( 10, 40, "Possessed Min Fields: %d", min_fields );
            Broodwar->drawTextScreen( 10, 50, "Morphing Count: %d", morphing_count );  //
            Broodwar->drawTextScreen( 10, 60, "Idle Larve: %d", larve );  //
            Broodwar->drawTextScreen( 10, 70, "Total Drones: %d", drones );  //
            Broodwar->drawTextScreen( 10, 80, "Ovis: %d", Count_Units( UnitTypes::Zerg_Overlord ) );  //
            Broodwar->drawTextScreen( 10, 90, "Hatches: %d", hatches );  // all hatch types included

            Broodwar->drawTextScreen( 10, 100, "Lings: %d", Count_Units( UnitTypes::Zerg_Zergling ) );  //
            Broodwar->drawTextScreen( 10, 110, "Hydras: %d", Count_Units( UnitTypes::Zerg_Hydralisk ) );  //
            Broodwar->drawTextScreen( 10, 120, "Pool: %d", Count_Units( UnitTypes::Zerg_Spawning_Pool ) );  // 
            Broodwar->drawTextScreen( 10, 130, "Hydra Den: %d", Count_Units( UnitTypes::Zerg_Hydralisk_Den ) );  // 
            Broodwar->drawTextScreen( 10, 140, "Extractor: %d", Count_Units( UnitTypes::Zerg_Extractor ) );  //
            Broodwar->drawTextScreen( 10, 150, "Evos: %d", Count_Units( UnitTypes::Zerg_Evolution_Chamber ) );  //

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

            Broodwar->drawTextScreen( 500, 100, "Gas (Pct. Ln.) : %f", ln_gas / (ln_min + ln_gas) );
            Broodwar->drawTextScreen( 500, 110, "Army Stock (Pct.): %f", army_count / ((double)worker_count + (double)army_count) ); //
            Broodwar->drawTextScreen( 500, 120, "Vision (Pct.): %f", vision_tile_count / (double)map_area );  //
            Broodwar->drawTextScreen( 500, 130, "Supply Heuristic: %f", ln_supply_remain / ln_supply_total );  //
            Broodwar->drawTextScreen( 500, 140, "Vision Tile Count: %d", vision_tile_count );  //
            Broodwar->drawTextScreen( 500, 150, "Map Area: %d", map_area );  //

            Broodwar->drawTextScreen( 500, 20, "Performance:" );  // 
            Broodwar->drawTextScreen( 500, 30, "APM: %d", Broodwar->getAPM() );  // 
            Broodwar->drawTextScreen( 500, 40, "APF: %f", (Broodwar->getAPM() / 60) / Broodwar->getAverageFPS() );  // 
            Broodwar->drawTextScreen( 500, 50, "FPS: %f", Broodwar->getAverageFPS() );  // 
            Broodwar->drawTextScreen( 500, 60, "Frames of Latency: %d", Broodwar->getLatencyFrames() );  //
        }
     // Prevent spamming by only running our onFrame once every number of latency frames.
     // Latency frames are the number of frames before commands are processed.
        if ( Broodwar->getFrameCount() % Broodwar->getLatencyFrames() != 0 )
            return;

        // Iterate through all the units that we own
        for ( auto &u : Broodwar->self()->getUnits() )
        {
            // Ignore the unit if it no longer exists
            // Make sure to include this block when handling any Unit pointer!
            if ( !u || !u->exists() )
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

            //Unit needs some awareness.

            UnitType u_type = u->getType();
            // If the unit is a worker unit
            if ( u_type.isWorker() )
            {
                // Mining loop if our worker is idle (includes returning $$$) or not moving while gathering gas, we (re-) evaluate what they should be mining.  Original script uses isIdle() only. might have queues that are very long which is why they may be unresponsive.
                if ( (isIdleEmpty( u ) || u->isGatheringGas() || u->isCarryingGas() || u->isCarryingMinerals()) && t_game%75==0 )
                {
                    // Order workers carrying a resource to return them to the center, every few seconds. This will refresh their logics as well.
                    // otherwise find a mineral patch to harvest.
                    if ( !(u->isCarryingGas() || u->isCarryingMinerals()) )  
                    {                             
                    // Idle worker then Harvest from the nearest mineral patch or gas refinery, depending on need.
                        bool enough_gas = !gas_starved ||
                            (Count_Units( UnitTypes::Zerg_Extractor ) - Broodwar->self()->incompleteUnitCount( UnitTypes::Zerg_Extractor )) == 0 ||
                            gas_workers >= 3 * Count_Units( UnitTypes::Zerg_Extractor );  // enough gas if (many critera), incomplete extractor, or not enough gas workers for your extractors.  Does not count worker IN extractor.
                        bool excess_minerals = min_workers >= 2 * min_fields;

                        if ( !enough_gas ) // both cases are critical
                        {
                            Unit ref = u->getClosestUnit( IsRefinery && IsOwned );
                            if ( ref && ref->exists() ) {
                                u->gather( ref );
                                ++(*gas_workers_ptr);
                            }
                        } // closure gas
                        else if ( !excess_minerals || enough_gas ) // pull from gas if we are satisfied with our gas count.
                        {

                            Unit min = u->getClosestUnit( IsMineralField );

                            if ( min && min->exists() ) { //stopgap till I figure out this loop better.
                                u->gather( min ); 
                            }

                            Unit base = u->getClosestUnit( IsResourceDepot && IsOwned );
                            if ( base && min && base->exists() && min->exists() ) {
                                    Unitset localmin = Broodwar->getUnitsInRadius( base->getPosition(), 250, IsMineralField ); // are there min near that base?
                                    Unitset localwork = Broodwar->getUnitsInRadius( base->getPosition(), 250, IsWorker ); // how many workers are at that base?
                                    if ( !localmin.empty() &&
                                        !localwork.empty() && 
                                        localmin.size() * 2 + 3 >= localwork.size() ) {   // check for local oversaturation. If it's not oversaturated, mine locally.
                                            u->gather( min ); // go to local mine.
                                            ++(*min_workers_ptr);
                                        } else { // if there is no good local mine, get any other mineral patch.
                                        Broodwar->drawCircleMap( u->getPosition(), 25, Colors::Purple, true ); // Highlight drone considering transfer.
                                        Unitset mybases = Broodwar->getUnitsInRadius( u->getPosition(), 99999, Filter::IsResourceDepot && Filter::IsOwned ); // is there a base?
                                        if ( !mybases.empty() ) { // check the set of bases 
                                            for ( auto base = mybases.begin(); base != mybases.end() && !mybases.empty(); ++base ) {
                                                if ( (*base) && (*base)->exists() ) {
                                                    Broodwar->drawCircleMap( (*base)->getPosition(), 250, Colors::Purple ); // circle each considered base.
                                                    int dist = u->getDistance( *base );
                                                    Unit min_found = (*base)->getClosestUnit( IsMineralField );
                                                    if ( min_found && min_found->exists() ) {  // head to a mine that is not oversaturated.
                                                        u->gather( min_found );
                                                        ++(*min_workers_ptr);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    }
                        }  
                    } // closure: collection assignment.
                    else if ( (u->isCarryingMinerals() || u->isCarryingGas() ) && t_game%175 == 0) // Return $$$
                    {
                            u->returnCargo();
                    }//Closure: returning $$ loop
                }// Closure: idle loop

            // Building subloop. Resets every few frames.
                if ( (isIdleEmpty( u ) || IsGatheringMinerals( u ) || IsGatheringGas( u )) &&
                    t_build + 50 < Broodwar->getFrameCount() )
                { //only get those that are idle or gathering minerals, but not carrying them. This always irked me. 

                    t_build = Broodwar->getFrameCount();

                    //Basic Buildings
                    Check_N_Build( UnitTypes::Zerg_Spawning_Pool, u, !econ_starved &&
                        Count_Units( UnitTypes::Zerg_Spawning_Pool ) == 0 );

                    //Gas Buildings
                    Check_N_Build( UnitTypes::Zerg_Extractor, u, gas_workers >= 3 * Count_Units( UnitTypes::Zerg_Extractor ) &&
                        (gas_starved || tech_starved) );

                    //Tech Buildings

                    Check_N_Build( UnitTypes::Zerg_Hydralisk_Den, u, tech_starved &&
                        Count_Units( UnitTypes::Zerg_Spawning_Pool ) > 0 &&
                        Count_Units( UnitTypes::Zerg_Hydralisk_Den ) == 0 );

                    Check_N_Build( UnitTypes::Zerg_Evolution_Chamber, u, tech_starved &&
                        Count_Units( UnitTypes::Zerg_Evolution_Chamber ) < 2 && // This has resolveed our issues with 4x evo chambers
                        Count_Units( UnitTypes::Zerg_Spawning_Pool ) > 0 );

                    Check_N_Build( UnitTypes::Zerg_Queens_Nest, u, tech_starved &&
                        Count_Units( UnitTypes::Zerg_Queens_Nest ) == 0 &&
                        Count_Units( UnitTypes::Zerg_Lair ) >= 0 );

                    //Expo loop
                    if ( !army_starved || ln_min >= log( UnitTypes::Zerg_Hatchery.mineralPrice() + 2 ) )
                    {  // build expos when safe or desparate or floating minerals) {
                        Unitset resource_grps = Broodwar->getMinerals();
                        double dist = 999999;
                            double* dist_ptr = &dist;
                        int expo_x;
                            int* expo_x_ptr = &expo_x;
                        int expo_y;
                            int* expo_y_ptr = &expo_y;
                        if ( !resource_grps.empty() ) {

                            for ( auto expo = resource_grps.begin(); expo != resource_grps.end() && !resource_grps.empty() ; ++expo ) { // search for closest resource group

                                if ( (*expo) && (*expo)->exists() ) {
                                    int expo_x_tile_temp = (*expo)->getTilePosition().x;
                                    int expo_y_tile_temp = (*expo)->getTilePosition().y;
                                    int expo_x_px_temp = (*expo)->getPosition().x;
                                    int expo_y_px_temp = (*expo)->getPosition().y; //getDistance is not overloaded for tile positions

                                    Broodwar->drawCircleMap( (*expo)->getPosition(), 250, Colors::Cyan );

                                    Unitset bases = Broodwar->getUnitsInRadius( expo_x_px_temp, expo_y_px_temp, 575, Filter::IsResourceDepot && Filter::IsOwned );
                                    Unitset r_check = Broodwar->getUnitsInRadius( expo_x_px_temp, expo_y_px_temp, 150, Filter::ResourceGroup );

                                    if ( bases.empty() && r_check.size() >= 5 ) { // check if there are NO bases with 575p of this resource center, AND 4+ resources are inside 150.
                                        for ( auto r = r_check.begin(); r != r_check.end() && !r_check.empty(); ++r ) {
                                            if ( (*r) && (*r)->exists() && ( (*r)->getType() == UnitTypes::Resource_Vespene_Geyser || (*r)->getType() == UnitTypes::Zerg_Extractor ) ) {
                                                double dist_temp = u->getDistance( { expo_x_px_temp, expo_y_px_temp } ); //getDistance is not overloaded for tile positions
                                                if ( dist_temp < dist ) {
                                                    *dist_ptr = dist_temp;  // if it is closer, but not immediately next to our main, update the new closest distance.
                                                    *expo_x_ptr = expo_x_tile_temp;
                                                    *expo_y_ptr = expo_y_tile_temp; // and update the location of that distance.
                                                } // get location of expo.
                                            } // Closure confirm does the expo have gas?
                                        } // closure begin gas check
                                    } // Look through the geyesers and find the closest with gas.
                                } // need the expo to exist
                            }// closure: loop through all resource group
                        }//closure search through all resource groups.
                        TilePosition buildPosition = Broodwar->getBuildLocation( UnitTypes::Zerg_Hatchery, { expo_x , expo_y }, 50 );  // build the expo near the nearest resource feature that fits this profile.
                            u->build( UnitTypes::Zerg_Hatchery, buildPosition );
                            PrintError_Unit( u );
                            t_build += 125; // give extra long time to get to that position.
                    }// closure: Expo methodology.

                     //Combat Buildings
                    Check_N_Build( UnitTypes::Zerg_Creep_Colony, u, army_starved &&  // army starved.
                        Count_Units( UnitTypes::Zerg_Creep_Colony ) == 0 && // no creep colonies.
                        ((Count_Units( UnitTypes::Zerg_Spawning_Pool ) > 0 && Count_Units( UnitTypes::Zerg_Spawning_Pool ) > Broodwar->self()->incompleteUnitCount( UnitTypes::Zerg_Spawning_Pool )) || (Count_Units( UnitTypes::Zerg_Evolution_Chamber ) > 0 && Count_Units( UnitTypes::Zerg_Evolution_Chamber ) > Broodwar->self()->incompleteUnitCount( UnitTypes::Zerg_Evolution_Chamber ))) && // And there is a building complete that will allow either creep colony upgrade.
                        hatches * 3 > Count_Units( UnitTypes::Zerg_Sunken_Colony ) + Count_Units( UnitTypes::Zerg_Spore_Colony ) ); // and you're not flooded with sunkens/spores.

                    // Macro Hatches, as needed. Don't macro hatch if you are trying to expand.
                    Check_N_Build( UnitTypes::Zerg_Hatchery, u, ln_min >= log( UnitTypes::Zerg_Hatchery.mineralPrice() * 2 + 2 ) );

                } // Close Build loop
            } // Close Worker management loop

            // Unit creation & Hatchery management loop
            if ( u_type == UnitTypes::Zerg_Larva ) // A resource depot is a Command Center, Nexus, or Hatchery.
               //Does a hatchery count as idle if it is researching but has idle larve? Might have to directly select larve at this point.  These should not be ELSE if loops because we can do all of them with multiple larva.
            {
                //Supply blocked protection loop.
                if ( supply_starved &&
                    !u->train( UnitTypes::Zerg_Overlord ) ) {
                    PrintError_Unit( u );
                } // closure: construct more supply
                  //Army build/replenish loop.  Note army also serves the purpose of scouting.
                else if ( !econ_starved &&  //tech starved here is to escape our tech cap, tech can't advance beyond a certain point.
                    Count_Units( UnitTypes::Zerg_Hydralisk_Den ) > 0 &&
                    ln_gas >= log( UnitTypes::Zerg_Hydralisk.gasPrice()+2) &&
                    !u->train( UnitTypes::Zerg_Hydralisk ) )
                {
                    PrintError_Unit( u );
                } // Closure: army loop
                else if ( !econ_starved && 
                    Count_Units( UnitTypes::Zerg_Spawning_Pool ) > 0 &&
                    !u->train( UnitTypes::Zerg_Zergling ) ) // note this will only build lings after it's checked for hydras.
                {
                    PrintError_Unit( u );
                }
                //Econ Build/replenish loop. Will build workers if I have no spawning pool, or if there is a worker shortage.
                else if ( ( econ_starved || Count_Units( UnitTypes::Zerg_Spawning_Pool ) == 0 ) &&  // if you're eco starved, we could use more workers.
                    drones <= min_fields *2 + Count_Units( UnitTypes::Zerg_Extractor ) * 3 && // unless you are fully saturated.
                    !u->train( u_type.getRace().getWorker() ) )
                {
                    PrintError_Unit( u );
                } // closure: failed to train worker unit
            } //Closure : Resource depot loop

            //Upgrade loop:
            if ( (tech_starved || Broodwar->self()->gas() > 100) &&
                isIdleEmpty( u ) &&
                (u->canUpgrade() || u->canResearch() || u->canMorph()) &&
                (u_type == UnitTypes::Zerg_Hive ||
                    u_type == UnitTypes::Zerg_Lair ||
                    u_type == UnitTypes::Zerg_Hatchery ||
                    u_type == UnitTypes::Zerg_Spawning_Pool ||
                    u_type == UnitTypes::Zerg_Evolution_Chamber ||
                    u_type == UnitTypes::Zerg_Hydralisk_Den) ) { // this will need to be revaluated once I buy units that cost gas.

                Check_N_Upgrade( UpgradeTypes::Metabolic_Boost, u, true );

                if ( Count_Units( UnitTypes::Zerg_Zergling ) > Count_Units( UnitTypes::Zerg_Hydralisk ) ) {
                    Check_N_Upgrade( UpgradeTypes::Zerg_Carapace, u, true );
                    Check_N_Upgrade( UpgradeTypes::Zerg_Melee_Attacks, u, true );
                } 

               Check_N_Upgrade( UpgradeTypes::Muscular_Augments, u, true );
               Check_N_Upgrade( UpgradeTypes::Grooved_Spines, u, true );
               Check_N_Upgrade( UpgradeTypes::Zerg_Carapace, u, true );
               Check_N_Upgrade( UpgradeTypes::Zerg_Missile_Attacks, u, true );



                Check_N_Build( UnitTypes::Zerg_Lair, u, (tech_derivative > army_derivative) &&
                    Count_Units( UnitTypes::Zerg_Lair ) == 0 &&
                    Count_Units( UnitTypes::Zerg_Evolution_Chamber ) > 0 &&
                    u_type == UnitTypes::Zerg_Hatchery );
                Check_N_Upgrade( UpgradeTypes::Pneumatized_Carapace, u, Count_Units( UnitTypes::Zerg_Lair ) > 0 );
                Check_N_Upgrade( UpgradeTypes::Antennae, u, Count_Units( UnitTypes::Zerg_Lair ) > 0 );

                Check_N_Build( UnitTypes::Zerg_Hive, u, (tech_derivative > army_derivative) &&
                    Count_Units( UnitTypes::Zerg_Queens_Nest ) >= 0 &&
                    u_type == UnitTypes::Zerg_Lair && Count_Units( UnitTypes::Zerg_Hive ) == 0 ); //If you're tech-starved at this point, don't make random hives.
                Check_N_Upgrade( UpgradeTypes::Adrenal_Glands, u, Count_Units( UnitTypes::Zerg_Hive ) > 0 );

                PrintError_Unit( u );
            } //closure: tech/upgrades loop

              //Scouting/vision loop. Very primative. move or attack-move with brownian motion, its map vision in any direction.  Turns out to be a boids-type algorithm.
            if ( isIdleEmpty( u ) &&
                (u_type == UnitTypes::Zerg_Zergling || u_type == UnitTypes::Zerg_Hydralisk || u_type == UnitTypes::Zerg_Overlord) ) //Scout if you're an overlord or a ling. And can move.
            {
                if ( vision_starved || u_type == UnitTypes::Zerg_Overlord ) {
                    Brownian_Stutter( u, 1 );
                }
                else {
                    Brownian_Stutter( u, 0 );
                }
            } // If it is a combat unit, then use it to attack the enemy.

            //Combat Logic. Has some sophistication at this time. Makes retreat/attack decision.  Only retreat if your army is not up to snuff. Only combat units retreat. Only retreat if the enemy is near. Lings only attack ground. 
            Unit e = u->getClosestUnit( Filter::IsEnemy ); //Consider combat when there is at least a single enemy.
            if ( e && e->exists() && 
                (army_starved || !e->isDetected() || !e->isVisible() || !u->canAttack(e) ) && 
                u->canMove() ) { // Needs there to be an enemy

                int dist = u->getDistance( e );

                if ( dist < 250 ) { //  Run if you're a noncombat unit or army starved.  Retreat function should now be built into stutter program.

                    Position e_pos = e->getPosition();
                    Position pos = u->getPosition();

                    int dist_x = e_pos.x - pos.x;
                    int dist_y = e_pos.y - pos.y;
                    double theta;

                    if ( dist_x != 0 ) {
                        theta = atan2( dist_y , dist_x ); // att_y/att_x = tan (theta).
                    }
                    else if ( dist_x == 0 && dist_y < 0) {
                        theta = 0;
                    }
                    else if ( dist_x == 0 && dist_y > 0 ) {
                        theta = 3.1415;
                    }
                    double retreat_dx = -cos( theta ) * 100; // run 50 units away.
                    double retreat_dy = -sin( theta ) * 100 ; 
                    Position retreat_spot = { pos.x + (int)retreat_dx , pos.y + (int)retreat_dy }; // the > < functions find the signum, no such function exists in c++!
                    Broodwar->drawCircleMap( e_pos, 250, Colors::Red );

                    if ( retreat_spot && retreat_spot.isValid() ) {
                        u->move( retreat_spot ); //identify vector between yourself and e.  go 350 pixels away in the quadrant furthest from them.
                        Diagnostic_Line( pos, retreat_spot, Colors::White);
                    }
                }
            } // closure retreat action
            if ( e && e->exists() &&
                !army_starved && e->isDetected() && e->isVisible() && u->canAttack( e ) &&
                u_type == UnitTypes::Zerg_Hydralisk ) {
                Combat_Logic( u, Colors::Yellow );
            }
            else if ( e && e->exists() &&
                !army_starved && e->isDetected() && e->isVisible() && u->canAttack( e ) &&
                u_type == UnitTypes::Zerg_Zergling) { //Intiatite combat if: we have army, enemies are present, we are combat units and we are not army starved.
                Unit target_caster = u->getClosestUnit( Filter::IsEnemy && !Filter::IsCloaked && Filter::IsSpellcaster && !Filter::IsAddon && !Filter::IsBuilding && !Filter::IsFlying, 250 ); // Charge the closest caster nononbuilding that is not cloaked.
                if ( target_caster && target_caster->exists()  ) { // Neaby casters
                    u->attack( target_caster );
                    Diagnostic_Line( u->getPosition(), target_caster->getPosition(), Colors::Purple );
                }
                else if ( !target_caster || !target_caster->exists()  ) { // then nearby combat units or transports.
                    Unit target_warrior = u->getClosestUnit( Filter::IsEnemy && (Filter::CanAttack || Filter::SpaceProvided) && !Filter::IsCloaked && !Filter::IsFlying, 250 ); // is warrior (or bunker, man those are annoying)
                    if ( target_warrior && target_warrior->exists()  ) {
                        u->attack( target_warrior );
                        Diagnostic_Line( u->getPosition(), target_warrior->getPosition(), Colors::Purple );
                    }
                    else { // No targatable warrior? then whatever is closest and not flying.
                        Unit ground_e = u->getClosestUnit( Filter::IsEnemy && !Filter::IsFlying ); // is warrior (or bunker, man those are 
                        if ( ground_e && ground_e->exists()  ) {
                            u->attack( ground_e );
                            Diagnostic_Line( u->getPosition(), ground_e->getPosition(), Colors::Purple );
                        }
                    }
                }
            }

            // Detectors are called for cloaked units. Recall burrow and cloak are different.
            Unit c = u->getClosestUnit( Filter::IsEnemy && (Filter::IsCloakable || Filter::IsBurrowed || Filter::IsCloaked), 250 ); //some units, DT, Observers, are not cloakable. They are cloaked though. 
            if ( c  && c->exists() && !army_starved ) {
                Position pos_c = c->getPosition();
                Unit d = c->getClosestUnit( IsDetector && CanMove && IsOwned );
                if ( d && d->exists() ) {
                    d->move( pos_c );
                    Broodwar->drawCircleMap( pos_c, 25, Colors::Cyan );
                    Diagnostic_Line( d->getPosition(), pos_c , Colors::Cyan);
                }
            }

            //Creep Colony upgrade loop.  We are more willing to upgrade them than to build them, since the units themselves are useless in the base state.
            if ( u_type == UnitTypes::Zerg_Creep_Colony &&
                ( Count_Units( UnitTypes::Zerg_Spawning_Pool ) > 0 || Count_Units( UnitTypes::Zerg_Evolution_Chamber ) > 0) ) {

                if ( Count_Units( UnitTypes::Zerg_Spawning_Pool ) > 0 &&
                    Count_Units( UnitTypes::Zerg_Evolution_Chamber ) > 0 ) {
                    Unit e_flying = u->getClosestUnit( IsFlying && IsEnemy && CanAttack );
                    if ( e_flying && e_flying->exists() ) { // if they have a flyer (that can attack), get spores.
                        Check_N_Build( UnitTypes::Zerg_Spore_Colony, u, true );
                    }
                    else {
                        Check_N_Build( UnitTypes::Zerg_Sunken_Colony, u, true );
                    }
                } // build one of the two colonies at random if you have both prequisites

                else if ( Count_Units( UnitTypes::Zerg_Spawning_Pool ) > 0 &&
                    Count_Units( UnitTypes::Zerg_Evolution_Chamber ) == 0 ) {
                    Check_N_Build( UnitTypes::Zerg_Sunken_Colony, u, true );
                } // build sunkens if you only have that
                else if ( Count_Units( UnitTypes::Zerg_Spawning_Pool ) == 0 &&
                    Count_Units( UnitTypes::Zerg_Evolution_Chamber ) > 0 ) {
                    Check_N_Build( UnitTypes::Zerg_Spore_Colony, u, true );
                } // build spores if you only have that.
            } // closure: Creep colony loop

        } // closure: unit iterator

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> diff = end - start;

        //Clock App
            if ( diff.count() > 55 ) {
                (*short_delay_ptr)+=1;
            }
            if ( diff.count() > 1000 ) {
                (*med_delay_ptr)+=1;
            }
            if ( diff.count() > 10000 ) {
                (*long_delay_ptr)+=1;
            }
        if ( diagnostic_mode ) {
            Broodwar->drawTextScreen( 375 , 70 ,"Delays:{ S:%d<=320, M:%d<=10, L:%d<1 } Timer: %3.fms", short_delay, med_delay, long_delay, diff.count() );
        }
        if ( short_delay > 320 ) {
            Broodwar->leaveGame();
        }
    } catch ( const std::exception &e ) {
        Broodwar << "EXCEPTION: " << e.what() << std::endl;
    }

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
void MeatAIModule::Brownian_Stutter( Unit unit, double n ) {

    Position pos = unit->getPosition();
    Unitset neighbors = unit->getUnitsInRadius( 75, !IsWorker && !IsBuilding && IsOwned );
    Unitset flock = unit->getUnitsInRadius( 9999999, !IsWorker && !IsBuilding && IsOwned );

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
               if ( (*i)->getType() != UnitTypes::Zerg_Drone ) {
                   tot_x += cos( (*i)->getAngle() ); //get the horiz element.
                   tot_y += sin( (*i)->getAngle() ); // get the vertical element. Averaging angles was trickier than I thought. 
                   flock_count++;
               }
           }
           double theta = atan2( (tot_y - unit->getAngle()) , (tot_x - unit->getAngle()) );
           attune_dx = cos( theta ) * 0.1 * unit->getType().sightRange();
           attune_dy = sin( theta ) * 0.1 * unit->getType().sightRange();
           Diagnostic_Line( unit->getPosition(), { (int)(pos.x + attune_dx), (int)(pos.y + attune_dy)}, Colors::Green );
        }

        //Cohesion
        double coh_dx = 0;
        double coh_dy = 0;

        if ( !flock.empty() ) {
            Position center = flock.getPosition();
            Broodwar->drawCircleMap( center, 25, Colors::Blue, true );
            double cohesion_x = center.x - pos.x;
            double cohesion_y = center.y - pos.y;

            double theta = atan2( cohesion_y, cohesion_x );
            coh_dx = cos( theta ) * 0.01 * unit->getType().sightRange();
            coh_dy = sin( theta ) * 0.01 * unit->getType().sightRange();
            Diagnostic_Line( unit->getPosition(), { (int)(pos.x + coh_dx), (int)(pos.y + coh_dy) }, Colors::Blue );
        }

       //seperation
       double sep_dx = 0;
       double sep_dy = 0;
       Position seperation = neighbors.getPosition();
       if ( seperation ) {
           double seperation_x = seperation.x - pos.x;
           double seperation_y = seperation.y - pos.y;
           double theta = atan2( seperation_y, seperation_x );
           sep_dx = cos(theta) * 25 ;
           sep_dy = sin(theta) * 25 ;
           Diagnostic_Line( unit->getPosition(), { (int)(pos.x - sep_dx), (int)(pos.y - sep_dy) }, Colors::Orange );
       }
        //// Enemy attractor.
        //    double attract_dx = 0;
        //    double attract_dy = 0;
        //if ( enemy && enemy->exists() && unit->getDistance(enemy) > 250 && !army_starved) {
        //    double attraction_x = enemy->getPosition().x - pos.x;
        //    double attraction_y = enemy->getPosition().y - pos.y;
        //    attract_dx = attraction_x * 0.50 ;
        //    attract_dy = attraction_y * 0.50 ;
        //} else if ( enemy && enemy->exists() && army_starved  ) {
        //    double attraction_x = enemy->getPosition().x - pos.x;
        //    double attraction_y = enemy->getPosition().y - pos.y;
        //    double theta = atan( attraction_y / attraction_x );
        //    attract_dx = - sin(theta) * 50 ; // run 50 units away.
        //    attract_dy = - cos(theta) * 50 ; // att_y/att_x = tan (theta).
        //}
        //if ( enemy && enemy->exists()){
        //    Broodwar->drawDotMap( enemy->getPosition(), Colors::Red );
        //}


    Position brownian_pos = { (int)(pos.x + x_stutter + coh_dx - sep_dx /* + attune_dx + attract_dx*/), (int)(pos.y + y_stutter + coh_dy - sep_dy /* + attune_dy + attract_dy*/) };

    if ( brownian_pos.isValid() ) {
        if ( unit->canAttack( brownian_pos ) ) {
            unit->attack( brownian_pos );
        }
        else {
            unit->move( brownian_pos );
        }
    }
};

// Gets units last error and prints it directly onscreen.  From tutorial.
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
    int instances = Count_Units( building );
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
    int instances = Count_Units( unit );
    //double curr_hp_pct = this->getHitPoints() / (double) unit.maxHitPoints();  // how do you call the unitinterface class? 
    int total_stock = cost * instances;
    return total_stock;
}

// evaluates the value of a stock of unit, in terms of supply added.
int MeatAIModule::Stock_Supply( UnitType unit ) {
    int supply = unit.supplyProvided();
    int instances = Count_Units( unit );
    int total_stock = supply * instances;
    return total_stock;
}

//UAB - Based Command

// Counts all units of one type in existance. Includes individual units in production. //Note his return of size_t has actually caused me some problems, so removing it was a benifit.
int MeatAIModule::Count_Units( UnitType type)
{
    int count = 0;
    for ( const auto & unit : Broodwar->self()->getUnits() )
    {
        // If it is obvious.
        if ( unit->getType() == type )
        {
            count++;
        }

        // case where a zerg egg contains the unit type
        if ( unit->getType() == UnitTypes::Zerg_Egg && unit->getBuildType() == type )
        {
            count += type.isTwoUnitsInOneEgg() ? 2 : 1; // this can only be lings, I believe.
        }

        // case where a building has started constructing a unit but it is not complete.  It doesn't yet have a unit associated with it, says UAB.
        if ( unit->getRemainingTrainTime() > 0 )
        {
            BWAPI::UnitType trainType = unit->getLastCommand().getUnitType();
            if ( trainType == type && unit->getRemainingTrainTime() == trainType.buildTime() )
            {
                count++;
            }
        }
    }

    return count;
}

// Checks for if a unit is a combat unit.
bool MeatAIModule::IsFightingUnit( Unit unit )
{
    if ( !unit )
    {
        return false;
    }

    // no workers or buildings allowed. Or overlords, or larva..
    if ( unit && unit->getType().isWorker() ||
        unit->getType().isBuilding() ||
        unit->getType() == BWAPI::UnitTypes::Zerg_Larva ||
        unit->getType() == BWAPI::UnitTypes::Zerg_Overlord )
    {
        return false;
    }

    // This is a last minute check for psi-ops. I removed a bunch of these. Observers and medics are not combat units per se.
    if ( unit->getType().canAttack() ||
        unit->getType() == BWAPI::UnitTypes::Protoss_High_Templar ||
        unit->isFlying() && unit->getType().spaceProvided() > 0 )
    {
        return true;
    }

    return false;
}

// This is basic combat logic for units that attack both air and ground. It is not yet well conceived.
void MeatAIModule::Combat_Logic( Unit unit , Color color = Colors::White ) {
    //Intitiate combat if: we have army, enemies are present, we are combat units and we are not army starved.  These units attack both air and ground.

    Unit target_caster = unit->getClosestUnit( Filter::IsEnemy && !Filter::IsCloaked && Filter::IsSpellcaster && !Filter::IsAddon && !Filter::IsBuilding, 250 ); // Charge the closest caster nononbuilding that is not cloaked.
    if ( target_caster && 
        target_caster->exists() ) { // Neaby casters
        unit->attack( target_caster );
        Diagnostic_Line( unit->getPosition(), target_caster->getPosition(), color);
    }
    else if ( !target_caster || !target_caster->exists() ) { // then nearby combat units or transports.
        Unit target_warrior = unit->getClosestUnit( Filter::IsEnemy && (Filter::CanAttack || Filter::SpaceProvided) && !Filter::IsCloaked, 250 ); // is warrior (or bunker, man those are annoying)
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
}

// This function limits the drawing that needs to be done by the bot.
void MeatAIModule::Diagnostic_Line( Position s_pos, Position f_pos, Color col = Colors::White) {
    if ( diagnostic_mode ) {
        Broodwar->drawLineMap( s_pos, f_pos, col );
    }
}
