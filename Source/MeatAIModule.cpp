#include "MeatAIModule.h"
#include <iostream> 
#include <fstream> // for file read/writing
#include <chrono> // for in-game frame clock.


// MeatAI V1.00. Current V goal-> Clean up issues.
// Unresolved issue: Text coloration in on screen messages. Screen messages doesn't take color commands, but does take printf format commands. Gets messy very fast.
// use trycatch for /0 errors and return to the true values, no +X's.
// check_n_train command.
// Scourge have tendancy to overpopulate.
// workers are pulled back to closest in the middle of a transfer.
// geyser logic is a little wonky. Check fastest map for demonstration.
// Count larvae in purchasing choices.
// Rework expos and mining decisions.




// Bugs and goals.
// generate an inventory system for researches and buildings. Probably as an array. Perhaps expand upon the self class?  Consider your tech inventory.
// Caution: algorithm gets bamboozled if any of the derivatives are invinity. Logic for starvation fails (I think it returns nulls), so do the derivative comparisons.
// remove all dependance on UnitUtil
// interest in actual map exploration vs raw vision? Interested in mineral vision?
// consider hp in unit stock.
// extractor trick.

using namespace BWAPI;
using namespace Filter;
using namespace std;

void MeatAIModule::onStart()
{

  // Hello World!
    Broodwar->sendText( "Hello world! This is MeatShieldAI V1.10" );

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
    //Initialize model variables. From genetic history, random parent for each gene. Proturb 1 of the 6.
    int mutation_1 = rand() % 6;

    delta = (mutation_1 == 0 ) ? Win_History( "output.csv", 0 ) * (rand() % 20 + 91) / (double)100 : Win_History( "output.csv", 0 ) ; //gas starved parameter. Triggers state if: ln_gas/(ln_min + ln_gas) < delta;  Higher is more gas. Current best: 0.45.  0.35 is basically all lings+tech for lings.
    gamma = (mutation_1 == 1 ) ? Win_History( "output.csv", 1 ) * (rand() % 20 + 91) / (double)100 : Win_History( "output.csv", 1 ); //supply starved parameter. Triggers state if: ln_supply_remain/ln_supply_total < gamma; Current best is 0.70. Some good indicators that this is reasonable: ln(4)/ln(9) is around 0.63, ln(3)/ln(9) is around 0.73, so we will build our first overlord at 7/9 supply. ln(18)/ln(100) is also around 0.63, so we will have a nice buffer for midgame.  

    //Cobb-Douglas Production exponents.  can be normalized to sum to 1.
    alpha_army = (mutation_1 == 2 ) ? Win_History( "output.csv", 2 ) * (rand() % 50 + 76) / (double)100 : Win_History( "output.csv", 2 ); // army starved parameter. This comes in a medium scale
    alpha_vis = (mutation_1 == 3 ) ? Win_History( "output.csv", 3 ) * (rand() % 50 + 76) / (double)100 : Win_History( "output.csv", 3 ); // vision starved parameter. Note the very large scale for vision, vision comes in groups of thousands. Since this is not scale free, the delta must be larger or else it will always be considered irrelevant.  
    alpha_econ = (mutation_1 == 4 ) ? Win_History( "output.csv", 4 ) * (rand() % 50 + 76) / (double)100 : Win_History( "output.csv", 4 ); // econ starved parameter. This comes in a small scale.
    alpha_tech = (mutation_1 == 5 ) ? Win_History( "output.csv", 5 ) * (rand() % 50 + 76) / (double)100 : Win_History( "output.csv", 5 ); // tech starved parameter. This comes in a small
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
            Stock_Units( UnitTypes::Zerg_Scourge ) +
            Stock_Units( UnitTypes::Zerg_Mutalisk ) +
            Stock_Units( UnitTypes::Zerg_Ultralisk ) +
            //Stock_Buildings( UnitTypes::Zerg_Sunken_Colony ) +
            //Stock_Buildings( UnitTypes::Zerg_Spore_Colony ) +
            5; //ad-hoc, assumes a lot about the translation between these units, +5 is to avoid risk of /0 errors.

        //Tech Inventory.
        double tech_stock = log(
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
            Stock_Ups( UpgradeTypes::Antennae ) +
            Stock_Ups( UpgradeTypes::Anabolic_Synthesis ) +
            Stock_Ups( UpgradeTypes::Chitinous_Plating ) + 5 ); // adds 5 to avoid risk of divides by 0 issues, note log(1)=0. 

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

        double econ_derivative = alpha_econ / worker_count * ( ( min_workers <= min_fields * 1.75 || min_fields < 36 ) && (worker_count < 90) ); // econ is only a possible problem if undersaturated or less than 62 patches, and worker count less than 90.
        double vision_derivative = alpha_vis / vision_tile_count;
        double army_derivative = alpha_army / army_count * (Broodwar->self()->supplyUsed() < 350);  // can't be army starved if you are maxed out (or close to it).
        double tech_derivative = alpha_tech / tech_stock * 
            !(Broodwar->self()->getUpgradeLevel( UpgradeTypes::Adrenal_Glands ) &&
                Broodwar->self()->getUpgradeLevel( UpgradeTypes::Chitinous_Plating ) &&
                Broodwar->self()->getUpgradeLevel( UpgradeTypes::Anabolic_Synthesis ) &&
                Broodwar->self()->getUpgradeLevel( UpgradeTypes::Zerg_Carapace ) == 3 &&
                Broodwar->self()->getUpgradeLevel( UpgradeTypes::Zerg_Melee_Attacks ) == 3 && 
                Broodwar->self()->getUpgradeLevel( UpgradeTypes::Zerg_Missile_Attacks ) == 3); // you can't be tech starved if you have these researches. You're done.

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
            Broodwar->drawTextScreen( 10, 0, "Total Drones: %d", drones );  //
            Broodwar->drawTextScreen( 10, 10, "Reached Min Fields: %d", min_fields );
            Broodwar->drawTextScreen( 10, 20, "Active Workers: %d", worker_count );
            Broodwar->drawTextScreen( 10, 30, "Active Miners: %d", min_workers );
            Broodwar->drawTextScreen( 10, 40, "Active Gas Miners: %d", gas_workers );

            Broodwar->drawTextScreen( 10, 60, "Idle Larve: %d", larve );  //
            Broodwar->drawTextScreen( 10, 70, "Ovis: %d", Count_Units( UnitTypes::Zerg_Overlord ) );  //
            Broodwar->drawTextScreen( 10, 80, "Hatches: %d", hatches );  // all hatch types included

            Broodwar->drawTextScreen( 10, 90, "Lings: %d", Count_Units( UnitTypes::Zerg_Zergling ) );  //
            Broodwar->drawTextScreen( 10, 100, "Hydras: %d", Count_Units( UnitTypes::Zerg_Hydralisk ) );  //
            Broodwar->drawTextScreen( 10, 110, "Scourge: %d", Count_Units( UnitTypes::Zerg_Scourge ) );  //
            Broodwar->drawTextScreen( 10, 120, "Ultras: %d", Count_Units( UnitTypes::Zerg_Ultralisk) );  //

            Broodwar->drawTextScreen( 10, 140, "Pool: %d", Count_Units( UnitTypes::Zerg_Spawning_Pool ) );  // 
            Broodwar->drawTextScreen( 10, 150, "Hydra Den: %d", Count_Units( UnitTypes::Zerg_Hydralisk_Den ) );  // 
            Broodwar->drawTextScreen( 10, 160, "Extractor: %d", Count_Units( UnitTypes::Zerg_Extractor ) );  //
            Broodwar->drawTextScreen( 10, 170, "Evos: %d", Count_Units( UnitTypes::Zerg_Evolution_Chamber ) );  //
            Broodwar->drawTextScreen( 10, 180, "Spire: %d", Count_Units( UnitTypes::Zerg_Spire ) );  //
            Broodwar->drawTextScreen( 10, 190, "Ultra Cavern: %d", Count_Units( UnitTypes::Zerg_Ultralisk_Cavern ) );  //

            Broodwar->drawTextScreen( 10, 255, "Game Status (Ln Y/L) : %f", ln_y, Text::Size::Huge ); //
            Broodwar->drawTextScreen( 10, 265, "Game Status (Ln Y) : %f", ln_Y, Text::Size::Huge ); //
            Broodwar->drawTextScreen( 10, 275, "Game Time: %d minutes", (Broodwar->elapsedTime()) / 60, Text::Size::Huge ); //

            Broodwar->drawTextScreen( 125, 0, "Econ Starved: %s", econ_starved ? "TRUE" : "FALSE" );  //
            Broodwar->drawTextScreen( 125, 10, "Army Starved: %s", army_starved ? "TRUE" : "FALSE" );  //
            Broodwar->drawTextScreen( 125, 20, "Vision Starved: %s", vision_starved ? "TRUE" : "FALSE" );  //
            Broodwar->drawTextScreen( 125, 30, "Tech Starved: %s", tech_starved ? "TRUE" : "FALSE" );  //

            Broodwar->drawTextScreen( 125, 50, "Supply Starved: %s", supply_starved ? "TRUE" : "FALSE" );
            Broodwar->drawTextScreen( 125, 60, "Gas Starved: %s", gas_starved ? "TRUE" : "FALSE" );

            Broodwar->drawTextScreen( 250, 0, "Econ Derivative: %g", econ_derivative );  //
            Broodwar->drawTextScreen( 250, 10, "Army Derivative: %g", army_derivative ); //
            Broodwar->drawTextScreen( 250, 20, "Vision Derivative: %g", vision_derivative ); //
            Broodwar->drawTextScreen( 250, 30, "Tech Derivative: %g", tech_derivative ); //
            Broodwar->drawTextScreen( 250, 50, "Alpha_Econ: %f", alpha_econ );  //
            Broodwar->drawTextScreen( 250, 60, "Alpha_Army: %f", alpha_army ); //
            Broodwar->drawTextScreen( 250, 70, "Alpha_Vis: %f", alpha_vis ); //
            Broodwar->drawTextScreen( 250, 80, "Alpha_Tech: %f", alpha_tech ); //
            Broodwar->drawTextScreen( 250, 90, "Delta_gas: %f", delta ); //
            Broodwar->drawTextScreen( 250, 100, "Gamma_supply: %f", gamma ); //

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

            // Unit creation & Hatchery management loop
            if ( u_type == UnitTypes::Zerg_Larva ) // A resource depot is a Command Center, Nexus, or Hatchery.
            {
                //Supply blocked protection 
                Check_N_Grow( UnitTypes::Zerg_Overlord, u, supply_starved );

                //Army build/replenish.  Note lings also serve the purpose of scouting.

                Unit e_flying = u->getClosestUnit( IsFlying && IsEnemy && CanAttack );
                if ( e_flying && e_flying->exists() ) { // if they have a flyer (that can attack), get AA
                    Check_N_Grow( UnitTypes::Zerg_Mutalisk, u, army_starved && Count_Units( UnitTypes::Zerg_Spire ) > 0 );  //nice to know but there's no point in this unit since I don't use it effectively. 
                    Check_N_Grow( UnitTypes::Zerg_Scourge, u, army_starved && Count_Units( UnitTypes::Zerg_Spire ) > 0 && Count_Units( UnitTypes::Zerg_Scourge ) < 10 ); // hard cap on scourges since they end up flooding my map against skytoss.
                    Check_N_Grow( UnitTypes::Zerg_Hydralisk, u, army_starved && Count_Units( UnitTypes::Zerg_Hydralisk_Den ) > 0 );
                }
                else {
                    Check_N_Grow( UnitTypes::Zerg_Ultralisk, u, army_starved && Count_Units( UnitTypes::Zerg_Ultralisk_Cavern ) > 0 );
                    Check_N_Grow( UnitTypes::Zerg_Mutalisk, u, army_starved && Count_Units( UnitTypes::Zerg_Spire ) > 0 );
                    Check_N_Grow( UnitTypes::Zerg_Hydralisk, u, army_starved && Count_Units( UnitTypes::Zerg_Hydralisk_Den ) > 0 );
                    Check_N_Grow( UnitTypes::Zerg_Zergling, u, army_starved && Count_Units( UnitTypes::Zerg_Spawning_Pool ) > 0 );
                }

                //Econ Build/replenish loop. Will build workers if I have no spawning pool, or if there is a worker shortage.
                bool early_game = Count_Units( UnitTypes::Zerg_Spawning_Pool ) - Broodwar->self()->incompleteUnitCount( UnitTypes::Zerg_Spawning_Pool ) == 0 && drones <= 10;

                Check_N_Grow( u_type.getRace().getWorker(), u, (econ_starved || early_game ) && // or it is early game and you have nothing to build. // if you're eco starved 
                    drones <= min_fields * 1.75 + Count_Units( UnitTypes::Zerg_Extractor ) * 3 && drones < 85 ); // build more workers to fill in our missing slots, to a maximum of 85.
            }

            //Upgrade loop:
            if ( tech_starved &&
                isIdleEmpty( u ) &&
                (u->canUpgrade() || u->canResearch() || u->canMorph()) &&
                (u_type == UnitTypes::Zerg_Hive ||
                    u_type == UnitTypes::Zerg_Lair ||
                    u_type == UnitTypes::Zerg_Hatchery ||
                    u_type == UnitTypes::Zerg_Spawning_Pool ||
                    u_type == UnitTypes::Zerg_Evolution_Chamber ||
                    u_type == UnitTypes::Zerg_Hydralisk_Den ||
                    u_type == UnitTypes::Zerg_Ultralisk_Cavern) ) { // this will need to be revaluated once I buy units that cost gas.

                Check_N_Upgrade( UpgradeTypes::Metabolic_Boost, u, true );

                if ( Count_Units( UnitTypes::Zerg_Zergling ) > Count_Units( UnitTypes::Zerg_Hydralisk ) ) {
                    Check_N_Upgrade( UpgradeTypes::Zerg_Carapace, u, true );
                    Check_N_Upgrade( UpgradeTypes::Zerg_Melee_Attacks, u, true );
                }

                Check_N_Upgrade( UpgradeTypes::Muscular_Augments, u, true );
                Check_N_Upgrade( UpgradeTypes::Grooved_Spines, u, true );
                Check_N_Upgrade( UpgradeTypes::Zerg_Carapace, u, true );
                Check_N_Upgrade( UpgradeTypes::Zerg_Missile_Attacks, u, true );

                Check_N_Build( UnitTypes::Zerg_Lair, u, (tech_starved) &&
                    Count_Units( UnitTypes::Zerg_Lair ) == 0 &&
                    Count_Units( UnitTypes::Zerg_Evolution_Chamber ) > 0 &&
                    Count_Units( UnitTypes::Zerg_Hive ) == 0 &&//don't need lair if we have a hive.
                    u_type == UnitTypes::Zerg_Hatchery );

                Check_N_Upgrade( UpgradeTypes::Pneumatized_Carapace, u, Count_Units( UnitTypes::Zerg_Lair ) > 0 || Count_Units( UnitTypes::Zerg_Hive ) > 0 );
                Check_N_Upgrade( UpgradeTypes::Antennae, u, Count_Units( UnitTypes::Zerg_Lair ) > 0 || Count_Units( UnitTypes::Zerg_Hive ) > 0 ); //don't need lair if we have a hive.

                Check_N_Build( UnitTypes::Zerg_Hive, u, (tech_starved) &&
                    Count_Units( UnitTypes::Zerg_Queens_Nest ) >= 0 &&
                    u_type == UnitTypes::Zerg_Lair &&
                    Count_Units( UnitTypes::Zerg_Hive ) == 0 ); //If you're tech-starved at this point, don't make random hives.

                Check_N_Upgrade( UpgradeTypes::Adrenal_Glands, u, Count_Units( UnitTypes::Zerg_Hive ) > 0 );
                Check_N_Upgrade( UpgradeTypes::Anabolic_Synthesis, u, Count_Units( UnitTypes::Zerg_Ultralisk_Cavern ) > 0 );
                Check_N_Upgrade( UpgradeTypes::Chitinous_Plating, u, Count_Units( UnitTypes::Zerg_Ultralisk_Cavern ) > 0 );

                PrintError_Unit( u );
            } //closure: tech/upgrades loop

            // If the unit is a worker unit
            if ( u_type.isWorker() )
            {
                // Mining loop if our worker is idle (includes returning $$$) or not moving while gathering gas, we (re-) evaluate what they should be mining.  Original script uses isIdle() only. might have queues that are very long which is why they may be unresponsive.
                if ( ( isIdleEmpty( u ) || u->isGatheringMinerals() || u->isGatheringGas() || u->isCarryingGas() || u->isCarryingMinerals()) && t_game % 75 == 0 )
                {
                    // Order workers carrying a resource to return them to the center, every few seconds. This will refresh their logics as well.
                    // otherwise find a mineral patch to harvest.
                    if ( !u->isCarryingGas() && !u->isCarryingMinerals() )
                    {
                        // Idle worker then Harvest from the nearest mineral patch or gas refinery, depending on need.

                        bool enough_gas = !gas_starved ||
                            (Count_Units( UnitTypes::Zerg_Extractor ) - Broodwar->self()->incompleteUnitCount( UnitTypes::Zerg_Extractor )) == 0 ||
                            gas_workers >= 3 * Count_Units( UnitTypes::Zerg_Extractor );  // enough gas if (many critera), incomplete extractor, or not enough gas workers for your extractors.  Does not count worker IN extractor.

                        bool excess_minerals = min_workers >= 1.75 * min_fields; //Some extra leeway over the optimal 1.5/patch, since they will be useless overgathering gas but not useless overgathering minerals.

                        if ( !enough_gas && !excess_minerals) // both cases are critical
                        {
                            Unit ref = u->getClosestUnit( IsRefinery && IsOwned, 250 );
                            if ( ref && ref->exists() ) {
                                u->gather( ref );
                                ++(*gas_workers_ptr);
                            }
                        } // closure gas
                        else if ( !excess_minerals || enough_gas ) // pull from gas if we are satisfied with our gas count.
                        {
                            Worker_Mine( u );
                            ++(*min_workers_ptr);
                        }
                    } // closure: collection assignment.
                    else if ( u->isCarryingMinerals() || u->isCarryingGas() ) // Return $$$
                    {
                        Unit base = u->getClosestUnit( IsResourceDepot && IsOwned );
                        if ( base && base->exists() ) {
                            u->move( base->getPosition() );
                        }
                        u->returnCargo( true );
                    }//Closure: returning $$ loop
                }// Closure: mining loop
            // Building subloop. Resets every few frames.
                if ( (isIdleEmpty( u ) || IsGatheringMinerals( u ) || IsGatheringGas( u )) &&
                    t_build + 50 < Broodwar->getFrameCount() )
                { //only get those that are idle or gathering minerals, but not carrying them. This always irked me. 

                    t_build = Broodwar->getFrameCount();

                    //Expo loop, whenever you can.
                    Expo( u );
                    //Gas Buildings
                    Check_N_Build( UnitTypes::Zerg_Extractor, u, gas_workers >= 2 * Count_Units( UnitTypes::Zerg_Extractor ) &&
                        gas_starved && Count_Units( UnitTypes::Zerg_Spawning_Pool ) > 0 );  // wait till you have a spawning pool to start gathering gas. If your gas is full (or nearly full) get another extractor.

                    //Basic Buildings
                    Check_N_Build( UnitTypes::Zerg_Spawning_Pool, u, !econ_starved &&
                        Count_Units( UnitTypes::Zerg_Spawning_Pool ) == 0 );

                    //Tech Buildings

                    Check_N_Build( UnitTypes::Zerg_Evolution_Chamber, u, tech_starved &&
                        Count_Units( UnitTypes::Zerg_Evolution_Chamber ) < 2 && // This has resolved our issues with 4x evo chambers
                        Count_Units( UnitTypes::Zerg_Spawning_Pool ) > 0 );

                    Check_N_Build( UnitTypes::Zerg_Hydralisk_Den, u, tech_starved &&
                        Count_Units( UnitTypes::Zerg_Spawning_Pool ) > 0 &&
                        Count_Units( UnitTypes::Zerg_Hydralisk_Den ) == 0 );

                    Check_N_Build( UnitTypes::Zerg_Spire, u, tech_starved &&
                        Count_Units( UnitTypes::Zerg_Spire ) == 0 &&
                        Count_Units( UnitTypes::Zerg_Lair ) >= 0 );

                    Check_N_Build( UnitTypes::Zerg_Queens_Nest, u, tech_starved &&
                        Count_Units( UnitTypes::Zerg_Queens_Nest ) == 0 &&
                        Count_Units( UnitTypes::Zerg_Lair ) >= 0 &&
                        Count_Units( UnitTypes::Zerg_Spire ) >= 0 );  // Spires are expensive and it will probably skip them unless it is floating a lot of gas.

                    Check_N_Build( UnitTypes::Zerg_Ultralisk_Cavern, u, tech_starved &&
                        Count_Units( UnitTypes::Zerg_Ultralisk_Cavern ) == 0 &&
                        Count_Units( UnitTypes::Zerg_Hive ) >= 0 );

                    //Combat Buildings
                    Check_N_Build( UnitTypes::Zerg_Creep_Colony, u, army_starved &&  // army starved.
                        Count_Units( UnitTypes::Zerg_Creep_Colony ) == 0 && // no creep colonies waiting to upgrade
                        ((Count_Units( UnitTypes::Zerg_Spawning_Pool ) > 0 && Count_Units( UnitTypes::Zerg_Spawning_Pool ) > Broodwar->self()->incompleteUnitCount( UnitTypes::Zerg_Spawning_Pool )) ||
                        (Count_Units( UnitTypes::Zerg_Evolution_Chamber ) > 0 && Count_Units( UnitTypes::Zerg_Evolution_Chamber ) > Broodwar->self()->incompleteUnitCount( UnitTypes::Zerg_Evolution_Chamber ))) && // And there is a building complete that will allow either creep colony upgrade.
                        hatches * 3 > Count_Units( UnitTypes::Zerg_Sunken_Colony ) &&  // and you're not flooded with sunkens. Spores could be ok if you need AA.
                        min_fields > 10 ); // and don't build them if you're on one base.

                    //knee-jerk emergency.
                    //Check_N_Build( UnitTypes::Zerg_Hatchery, u, !econ_starved && larve < hatches); // Macro Hatches, as needed. Don't macro hatch if you are trying to expand/need $$, or are floating larvae.

                } // Close Build loop
            } // Close Worker management loop

            //Combat Logic. Has some sophistication at this time. Makes retreat/attack decision.  Only retreat if your army is not up to snuff. Only combat units retreat. Only retreat if the enemy is near. Lings only attack ground. 
            if (u_type != UnitTypes::Zerg_Larva &&
                u->canMove() ) 
            {
                Unit e = u->getClosestUnit( IsEnemy ); //Consider combat when there is at least a single enemy.
                Unit n = u->getClosestUnit( IsNeutral, 25); // check for disruption web or dark swarm. They are neutral units.
                if ( e && e->exists() && 
                        u->canMove() &&
                        ( Futile_Fight(u,e) || (army_starved && !Futile_Fight(e,u)) || u->getType()==UnitTypes::Zerg_Mutalisk && !Futile_Fight( e, u ) ) // if we cannot win OR they can resist at all, then run. Mutas run if they face ANY resistance.
                    )
                { // Needs there to be an enemy we can't deal with. If so, run.
                    Retreat_Logic( u, e, Colors::White );
                }  else if ( n && n->exists() && n->getType().isSpell() &&
                    u->canMove() )
                { // We should run from disruption web or dark swarm (DS from either side, might I add.)
                    Retreat_Logic( u, n, Colors::Black );
                }  else if ( e && e->exists() && e->isDetected() &&
                     ( (!army_starved && u_type != UnitTypes::Zerg_Drone) || (u_type == UnitTypes::Zerg_Drone && army_count < 100 && u->getDistance( e ) < 50 ) ) &&  // consider "pulling the boys" if situation is dire.
                    Can_Fight(u,e) && u_type != UnitTypes::Zerg_Overlord ) 
                {
                    Combat_Logic( u, Colors::Orange );
                }
            }

            // Detectors are called for cloaked units. 
            if ( u->getType().isDetector() ) {
                Unit c = u->getClosestUnit( Filter::IsEnemy && (Filter::IsCloakable || Filter::IsBurrowed || Filter::IsCloaked) ); //some units, DT, Observers, are not cloakable. They are cloaked though. Recall burrow and cloak are different.
                if ( c  && c->exists() && !army_starved ) {
                    Position pos_c = c->getPosition();
                    Unit d = c->getClosestUnit( IsDetector && CanMove && IsOwned );
                    if ( d && d->exists() ) {
                        d->move( pos_c );
                        Broodwar->drawCircleMap( pos_c, 25, Colors::Cyan );
                        Diagnostic_Line( d->getPosition(), pos_c, Colors::Cyan );
                    }
                }
            }

            //Scouting/vision loop. Intially just brownian motion, now a fully implemented boids-type algorithm.
            if ( isIdleEmpty( u ) && u_type != UnitTypes::Zerg_Drone && u_type != UnitTypes::Zerg_Larva && u->canMove() )
            { //Scout if you're not a drone or larva and can move.
                if ( (vision_starved || u_type == UnitTypes::Zerg_Overlord) && isIdleEmpty( u ) ) {
                    Brownian_Stutter( u, 2 );
                }
                else if ( u_type != UnitTypes::Zerg_Overlord ) {
                    Brownian_Stutter( u, 0 );
                }
            } // If it is a combat unit, then use it to attack the enemy.
            //Creep Colony upgrade loop.  We are more willing to upgrade them than to build them, since the units themselves are useless in the base state.
            if ( u_type == UnitTypes::Zerg_Creep_Colony && army_starved &&
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
        if ( (short_delay > 320 || Broodwar->elapsedTime() > 90 * 60) && diagnostic_mode == true) //if game times out or lags out, end game with resignation.
        {
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
