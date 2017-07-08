#include "MeatAIModule.h"
#include "UnitUtil.h"
#include <iostream>

// MeatAI V0.12. Current V goal-> Clean up issues.
// Unresolved issue: Text coloration in on screen messages. Screen messages doesn't take color commands, but does take printf format commands. Gets messy very fast.
// Unresolved issue: use a more efficient priority list, it is unweildly.
// Evaluate enemy units in some manner. Retreat when appropriate.
// build other units than lings.
// generate an inventory system for researches and buildings. Probably as an array. Perhaps expand upon the self class?  Consider your tech inventory.
// ad-hoc solution to avoid dividing by 0 in ln_y.
// Would like to someday not overload unit queue, but spamming is much easier.
// algorithm gets bamboozled if any of the derivatives are invinity. Logic for starvation fails (I think it returns nulls), so do the derivative comparisons.
// signed/unsigned mismatch line 324. 348?  Happens when using getAllUnits. Probably happening in my methods as well, a silent error would be dissapointing.
// upgrade loop logic is a little bamboozled as well. Why just eco starved?
// remove all dependance on UnitUtil, move getAllUnit command to my own module.
// makes 3 evo chambers sometimes.
// inelegant solution to divide by 0 item, block unit production when tech starved?
// interest in actual map exploration vs raw vision?
// bot does not understand resources at expos raise cap on number of drones.



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
	delta = 0.35; //gas starved parameter. Triggers state if: ln_gas/(ln_min + ln_gas) < delta;  Higher is more gas.
    gamma = 0.61; //supply starved parameter. Triggers state if: ln_supply_remain/ln_supply_total < gamma; Some good indicators that this is reasonable: ln(4)/ln(9) is around 0.63, so we will build our first overlord at 7/9 supply. ln(18)/ln(100) is also around 0.63, so we will have a nice buffer for midgame.

    //Cobb-Douglas Production exponents.  Should all sum to one. Current best {0.03000,0.96800,0.00175, 0.00025} leads to illustratively reasonable- some drone scouting, etc.
	alpha_army = 0.07688; // army starved parameter.  
    alpha_vis =  0.91800; // vision starved parameter. Note the scale for vision, vision comes in groups of thousands. 
    alpha_econ = 0.00450; // econ starved parameter. 
    alpha_tech = 0.00082; // tech starved parameter.

  // Hello World!
  Broodwar->sendText("Hello world! This is MeatShieldAI V0.10");

  // Print the map name.
  // BWAPI returns std::string when retrieving a string, don't forget to add .c_str() when printing!
  Broodwar << "The map is " << Broodwar->mapName() << "!" << std::endl;

  // Enable the UserInput flag, which allows us to control the bot and type messages.
  Broodwar->enableFlag(Flag::UserInput);

  // Uncomment the following line and the bot will know about everything through the fog of war (cheat).
  //Broodwar->enableFlag(Flag::CompleteMapInformation);

  // Set the command optimization level so that common commands can be grouped
  // and reduce the bot's APM (Actions Per Minute).
  Broodwar->setCommandOptimizationLevel(2);

  // Check if this is a replay
  if ( Broodwar->isReplay() )
  {
    // Announce the players in the replay
    Broodwar << "The following players are in this replay:" << std::endl;
    
    // Iterate all the players in the game using a std:: iterator
    Playerset players = Broodwar->getPlayers();
    for(auto p : players)
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

void MeatAIModule::onEnd(bool isWinner)
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
  int hatches = MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Hatchery);
  int ovis = MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Overlord);
  int larve = MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Larva);  
  int morphing_count = Broodwar->self()->incompleteUnitCount(UnitTypes::Buildings);
    int* morphing_count_ptr = &morphing_count;
  int pool = MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Spawning_Pool);
  int extractor = MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Extractor);
  int evos = MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Evolution_Chamber);


    //Idea: MeatAI::UnitUtil::GetAllUnitCount(UnitCommandTypes::Build)  What command reads Unitcommandtype chars?

  //Econ inventory
  //int worker_count = MeatAI::UnitUtil::GetAllUnitCount( BWAPI::Broodwar->self()->getRace().getWorker() );
  int worker_count = 1; // adds 1 to avoid divide by 0 error.
    int* worker_count_ptr = &worker_count;
  int gas_workers = 0; 
    int* gas_workers_ptr = &gas_workers;
  int min_workers = 0;
    int* min_workers_ptr = &min_workers;
  int min_fields = 1;
    int* min_fields_ptr = &min_fields;
  
    //maintains an active inventory of minerals on the map.
    // Get main building closest to start location.
    int home_x = Broodwar->self()->getUnits().getPosition().x;
    int home_y = Broodwar->self()->getUnits().getPosition().y;
    Unitset bases = BWAPI::Broodwar->getUnitsInRadius(home_x,home_y,999999, Filter::IsResourceDepot && Filter::IsOwned );

    if (!bases.empty()) { // check if the base is valid.
        for (auto expo = bases.begin(); expo != bases.end(); ++expo) { //for each base I have found
            Broodwar->drawCircleMap( (*expo)->getPosition(), 250, Colors::Green);
            // Get all resources near there
            Unitset myResources = (*expo)->getUnitsInRadius(250, Filter::IsMineralField);
            //Unitset myexpos = (*expo)->getUnitsInRadius(250, Filter::IsResourceDepot);
            if ( !myResources.empty() ) { // check if myresources are valid
                for (auto min = myResources.begin(); min != myResources.end(); ++min) {
                    if ((*min)->getType().isMineralField()) {
                        (*min_fields_ptr)++;
                    }
                }// add up all resources nearby those expos.
            } // closure, existance of resources.
        } // closure: for each base.
    } // closure, mineral tally.

  // Set of all of my units.
  Unitset myUnits = Broodwar->self()->getUnits();

  // Using myUnits to take inventory on all my units.
  if ( !myUnits.empty() ) { // make sure this object is valid!
      for (auto u = myUnits.begin(); u != myUnits.end(); ++u)
      {
          if ((*u)->isGatheringGas() || (*u)->isCarryingGas()) // implies exists and isCompleted
          {
              (*gas_workers_ptr)++;
          }
          if ((*u)->isGatheringMinerals() || (*u)->isCarryingMinerals()) // implies exists and isCompleted
          {
              (*min_workers_ptr)++;
          }
          *worker_count_ptr = gas_workers + min_workers + 1;  //adds 1 to avoid a divide by 0 error
      } // closure: worker tally
  }
  //double econ_heuristic = worker_count * (UnitTypes::Zerg_Drone.mineralPrice() + 1 / 24) + exp(ln_min) /*+ exp(ln_gas) */ );
  // 1 mineral/sec, 24 frames per sec according to Krasi0's author, so this should be the $$ we have on hand at this frame. (http://legionbot.blogspot.com/2015/03/worker-payoff-time.html) 

  //Army inventory.
  int ling_count = MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Zergling );
  int army_count = ling_count + 
      worker_count/4 +
      MeatAIModule::Stock_Buildings(UnitTypes::Zerg_Sunken_Colony)*2 +
      MeatAIModule::Stock_Buildings(UnitTypes::Zerg_Spore_Colony)*2 + 5; //ad-hoc, assumes a lot about the translation between these units, +5 is to avoid risk of /0 errors.

  //Tech Inventory.
  double tech_stock = log( 
      Stock_Buildings(UnitTypes::Zerg_Extractor) +
      Stock_Buildings(UnitTypes::Zerg_Spawning_Pool) + 
      Stock_Buildings(UnitTypes::Zerg_Evolution_Chamber) +
      Stock_Buildings(UnitTypes::Zerg_Queens_Nest) +
      Stock_Ups(UpgradeTypes::Metabolic_Boost) +
      Stock_Ups(UpgradeTypes::Zerg_Carapace) +
      Stock_Ups(UpgradeTypes::Zerg_Melee_Attacks) +
      Stock_Ups(UpgradeTypes::Pneumatized_Carapace)+
      Stock_Ups(UpgradeTypes::Adrenal_Glands) +
      Stock_Ups(UpgradeTypes::Antennae) + 5); // adds 5 to avoid risk of divides by 0 issues, note log(1)=0. 

  //Inventories for knee-jerk states: Gas, Supply, mineral counter
    //Supply inventory:
    double ln_supply_remain = log( Stock_Supply(UnitTypes::Zerg_Overlord) +
        Stock_Supply(UnitTypes::Zerg_Hatchery) +
        Stock_Supply(UnitTypes::Zerg_Lair) +
        Stock_Supply(UnitTypes::Zerg_Hive) - 
        Broodwar->self()->supplyUsed() + 5 ); // + 5 is a minor adjustment to avoid ln(0) errors or divides by 0 errors.
    double ln_supply_total = log( Broodwar->self()->supplyTotal() + 5 ); // + 5 is a minor adjustment to avoid ln(0) errors or divides by 0 errors.


    //Gas inventory.
    double ln_min = log(Broodwar->self()->minerals() + 5); // +5 is a minor adjustment to avoid ln(0) errors or divides by 0 errors.
    double ln_gas = log(Broodwar->self()->gas() + 5); // +5 is a minor adjustment to avoid ln(0) errors or divides by 0 errors.

  //Vision inventory:
  // Map area could be initialized on startup, since maps do not vary once made.
  int map_x = Broodwar->mapWidth();
  int map_y = Broodwar->mapHeight();
  int map_area = map_x * map_y; // map area in tiles.
  int vision_tile_count = 5; // starting at 5 to avoid /0 issues. Should be profoundly rare and vision is usually in the thousands anyway.
  int* vision_tile_count_ptr = &vision_tile_count;

  for (int tile_x = 1; tile_x <= map_x; tile_x++) { // there is no tile (0,0)
      for (int tile_y = 1; tile_y <= map_y; tile_y++) {
          if ( Broodwar->isVisible(tile_x, tile_y) ) {
              *vision_tile_count_ptr += 1;
          }
      }
  } // this search must be very exhaustive to do every frame. But C++ does it without any problems.

  //Knee-jerk states: gas, supply.
  gas_starved = (ln_gas / (ln_min + ln_gas)) < delta;  //If your gas is disproportionately low, then you are gas starved. +1 to avoid a divide by 0 error.
  supply_starved = (ln_supply_remain / ln_supply_total) < gamma && //If you have eaten up of your supply
      !(ln_supply_total >= log(200) ) ; // you have a hard cap at 200, you are not starved at that limit

  // Cobb-Douglas 
  ln_y = alpha_vis * log( vision_tile_count / worker_count  ) + alpha_army * log( army_count / worker_count ) + alpha_tech * log(tech_stock / worker_count ); //Analog to per capita GDP. Worker count has been incremented by 1 to avoid crashing from /0.
  ln_Y = alpha_vis * log(vision_tile_count) + alpha_army * log(army_count)  + alpha_tech * log(tech_stock)  + alpha_econ * log(worker_count); //Analog to GDP
      // so prioritization should be whichever is greatest: alpha_vis/vision_tile_count, alpha_army/army_count, alpha_tech/tech_stock, alpha_econ/worker_count.
      // double priority_list[] = { alpha_vis/vision_tile_count, alpha_army/army_count, alpha_econ/worker_count...}; // would be more elegant to use an array here...

      double econ_derivative = alpha_econ / worker_count;
      double vision_derivative = alpha_vis / vision_tile_count;
      double army_derivative = alpha_army / army_count;
      double tech_derivative = alpha_tech / tech_stock;

      // Set Priorities
      if ( econ_derivative >= vision_derivative &&
          econ_derivative >= army_derivative &&
          econ_derivative >= tech_derivative) 
      {
          tech_starved = false;
          vision_starved = false;
          army_starved = false;
          econ_starved = true;
      }
      else if (vision_derivative >= econ_derivative && 
          vision_derivative >= army_derivative &&
          vision_derivative >= tech_derivative)
      {
          tech_starved = false;
          vision_starved = true;
          army_starved = false;
          econ_starved = false;
      }   
      else if (army_derivative >= vision_derivative && 
          army_derivative >= econ_derivative &&
          army_derivative >= tech_derivative)
      {
          tech_starved = false;
          vision_starved = false;
          army_starved = true;
          econ_starved = false;
      }
      else if (tech_derivative >= vision_derivative &&
          tech_derivative >= econ_derivative &&
          tech_derivative >= army_derivative)
      {
          tech_starved = true;
          vision_starved = false;
          army_starved = false;
          econ_starved = false;
      }
      else {
          Broodwar->drawTextScreen(10, 200, " Status: Error No - Status ", Text::Size::Huge); //
      }

  // Display the game status indicators at the top of the screen	
	  Broodwar->drawTextScreen(10, 0 , "Inventory:", Text::DarkGreen); // working on a better inventory.
	  Broodwar->drawTextScreen(10, 10, "Active Workers: %d", worker_count);
      Broodwar->drawTextScreen(10, 20, "Active Miners: %d", min_workers);
      Broodwar->drawTextScreen(10, 30, "Active Gas Miners: %d", gas_workers);
      Broodwar->drawTextScreen(10, 40, "Visible Min Fields: %d", min_fields);
      Broodwar->drawTextScreen(10, 50, "Morphing Count: %d", morphing_count);  //
      Broodwar->drawTextScreen(10, 60, "Idle Larve: %d", larve);  //

	  Broodwar->drawTextScreen(10, 80, "Lings: %d", ling_count);  //
	  Broodwar->drawTextScreen(10, 90, "Pool: %d", pool);  // 
	  Broodwar->drawTextScreen(10, 100, "Extractor: %d", extractor);  //
	  Broodwar->drawTextScreen(10, 110, "Hatches: %d", hatches);  //
	  Broodwar->drawTextScreen(10, 120, "Ovis: %d", ovis);  //
      Broodwar->drawTextScreen(10, 130, "Evos: %d", evos);  //

      Broodwar->drawTextScreen(10, 255, "Game Status (Ln Y/L) : %f", ln_y, Text::Size::Huge); //
      Broodwar->drawTextScreen(10, 265, "Game Status (Ln Y) : %f", ln_Y, Text::Size::Huge); //
	  Broodwar->drawTextScreen(10, 275, "Game Time: %d minutes", ( Broodwar->elapsedTime() ) / 60 , Text::Size::Huge ); //

      Broodwar->drawTextScreen(125, 0, "Econ Starved: %s", econ_starved ? "TRUE" : "FALSE");  //
      Broodwar->drawTextScreen(125, 10, "Army Starved: %s", army_starved ? "TRUE" : "FALSE");  //
      Broodwar->drawTextScreen(125, 20, "Vision Starved: %s", vision_starved ? "TRUE" : "FALSE");  //
      Broodwar->drawTextScreen(125, 30, "Tech Starved: %s", tech_starved ? "TRUE" : "FALSE");  //

      Broodwar->drawTextScreen(125, 50, "Supply Starved: %s", supply_starved ? "TRUE" : "FALSE");
      Broodwar->drawTextScreen(125, 60, "Gas Starved: %s", gas_starved ? "TRUE" : "FALSE");
      
      Broodwar->drawTextScreen(250, 0, "Econ Derivative (10k): %f", econ_derivative * 10000 );  //
      Broodwar->drawTextScreen(250, 10, "Army Derivative (10k): %f", army_derivative * 10000 ); //
      Broodwar->drawTextScreen(250, 20, "Vision Derivative (10k): %f", vision_derivative * 10000 ); //
      Broodwar->drawTextScreen(250, 30, "Tech Derivative (10k): %f", tech_derivative * 10000); //

      Broodwar->drawTextScreen(450, 100, "Gas (Pct.) : %f", ln_gas / (ln_min + ln_gas));
	  Broodwar->drawTextScreen(450, 110, "Army Stock (Pct.): %f", army_count / ((double) worker_count + (double)army_count ) ); //
      Broodwar->drawTextScreen(450, 120, "Vision (Pct.): %f", vision_tile_count / (double) map_area);  //
      Broodwar->drawTextScreen(450, 130, "Supply Heuristic: %f", ln_supply_remain / ln_supply_total);  //
      Broodwar->drawTextScreen(450, 140, "Vision Tile Count: %d", vision_tile_count);  //
      Broodwar->drawTextScreen(450, 150, "Map Area: %d", map_area);  //

      Broodwar->drawTextScreen(500, 20, "Performance:");  // 
	  Broodwar->drawTextScreen(500, 30, "APM: %d", Broodwar->getAPM());  // 
	  Broodwar->drawTextScreen(500, 40, "APF: %f", (Broodwar->getAPM() / 60) / Broodwar->getAverageFPS());  // 
	  Broodwar->drawTextScreen(500, 50, "FPS: %f", Broodwar->getAverageFPS());  // 
	  Broodwar->drawTextScreen(500, 60, "Frames of Latency: %d", Broodwar->getLatencyFrames());  //

   // Prevent spamming by only running our onFrame once every number of latency frames.
   // Latency frames are the number of frames before commands are processed.
  if (Broodwar->getFrameCount() % Broodwar->getLatencyFrames() != 0)
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
    if ( u->getType().isWorker())
    {
      // Mining loop if our worker is idle (includes returning $$$) or not moving while gathering gas, we (re-) evaluate what they should be mining.  Original script uses isIdle() only.
		if ( isIdleEmpty(u) || u->isCarryingGas() || u->isCarryingMinerals() )
		{
			// Order workers carrying a resource to return them to the center,
			// otherwise find a mineral patch to harvest.
			if (u->isCarryingGas() ||
                u->isCarryingMinerals() ) // Return $$$
			{
				u->returnCargo(true);
			} //Closure: returning $$ loop
			else if (!u->getPowerUp())  // The worker cannot harvest anything if it
			{                             // is carrying a powerup such as a flag
			// Idle worker then Harvest from the nearest mineral patch or gas refinery, depending on need.
                bool enough_gas = MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Extractor) == 0 ||
                    !gas_starved ||
                    gas_workers >= 3 * MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Extractor) ||
                    ln_gas / (ln_min + ln_gas) > delta;
				if ( min_workers < (min_fields * 2) &&
                    enough_gas ) // If we have enough gas, more minerals.
				{
					u->gather(u->getClosestUnit(IsMineralField), true );
				}
				else if ( !enough_gas ) // If we are short on gas, then we should get that.
				{
					u->gather(u->getClosestUnit(IsRefinery), true );
				}
			} // closure:powerup loop
		}// Closure: idle loop

	// Building subloop. Resets every few frames.
		if ( ( isIdleEmpty(u) || IsGatheringMinerals(u) ) &&
            t_build + 50 < Broodwar->getFrameCount() )
		{ //only get those that are idle or gathering minerals, but not carrying them. This always irked me. 

            t_build = Broodwar->getFrameCount();
            
            //Gas Buildings
            Check_N_Build(UnitTypes::Zerg_Extractor, u, gas_workers >= 3 * MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Extractor)  &&
                gas_starved );

            //Basic Buildings
            Check_N_Build(UnitTypes::Zerg_Spawning_Pool, u, !econ_starved &&
                MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Spawning_Pool) == 0);

            //Tech Buildings
            Check_N_Build(UnitTypes::Zerg_Evolution_Chamber, u, (tech_derivative > army_derivative) &&
                MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Evolution_Chamber) < 2 && 
                MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Spawning_Pool) > 0);

            Check_N_Build(UnitTypes::Zerg_Queens_Nest, u, (tech_derivative > army_derivative) && 
                MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Queens_Nest) == 0 &&
                MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Lair) >= 0 );

            //Combat Buildings
            Check_N_Build(UnitTypes::Zerg_Creep_Colony, u, army_derivative > tech_derivative  &&
                army_derivative > econ_derivative &&
                MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Creep_Colony) == 0 &&
                (MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Spawning_Pool) > 0 || MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Evolution_Chamber) > 0) );

            //Extra critera for expos
            if (!vision_starved && 
                !army_starved &&
                ( ln_min >= log(UnitTypes::Zerg_Hatchery.mineralPrice() + 1 ) ) ) {
                //Unit gas = Broodwar->getClosestUnit(Broodwar->self()->getUnits().getPosition(), Filter::ResourceGroup);
                //Unit base = BWAPI::Broodwar->getClosestUnit(Broodwar->self()->getUnits().getPosition(), Filter::IsResourceDepot && Filter::IsOwned);
                Unitset resource_grps = BWAPI::Broodwar->getUnitsInRadius(u->getPosition().x, u->getPosition().y, 99999, Filter::ResourceGroup);
                    double dist = 999999;
                        double* dist_ptr = &dist;
                    int expo_x;
                        int* expo_x_ptr = &expo_x;
                    int expo_y;
                        int* expo_y_ptr = &expo_y;
                if (!resource_grps.empty()){
                    for (auto expo = resource_grps.begin(); expo != resource_grps.end(); ++expo) { // search for closest resource group

                        int expo_x_tile_temp = (*expo)->getTilePosition().x;
                        int expo_y_tile_temp = (*expo)->getTilePosition().y;
                        int expo_x_px_temp = (*expo)->getPosition().x;
                        int expo_y_px_temp = (*expo)->getPosition().y; //getDistance is not overloaded for tile positions

                        Unitset bases = BWAPI::Broodwar->getUnitsInRadius(expo_x_px_temp, expo_y_px_temp, 750, Filter::IsResourceDepot && Filter::IsOwned);
                        if ( bases.empty() ) { // check if there are NO bases with 750 of this resource center.  if there are, don't consider that an expo.
                            double dist_temp = u->getDistance({ expo_x_px_temp, expo_y_px_temp }); //getDistance is not overloaded for tile positions
                            if (dist_temp < dist ) {
                                *dist_ptr = dist_temp;  // if it is closer, but not immediately next to our main, update the new closest distance.
                                *expo_x_ptr = expo_x_tile_temp;
                                *expo_y_ptr = expo_y_tile_temp; // and update the location of that distance.
                            }  
                        } // Look through the geyesers and find the closest. 
                    }// closure: loop through all resource group
               }//closure search through all resource groups.
                TilePosition buildPosition = Broodwar->getBuildLocation(UnitTypes::Zerg_Hatchery, { expo_x , expo_y }, 64);  // build the expo near the nearest resource depot.
                u->build(UnitTypes::Zerg_Hatchery, buildPosition);
                PrintError_Unit(u);
                t_build += 125;
            }// closure: Expo methodology.

            // Macro Hatches, as needed.
            Check_N_Build(UnitTypes::Zerg_Hatchery, u, ln_min >= log(UnitTypes::Zerg_Hatchery.mineralPrice() * 1.5 + 1)); 

		  } // Close Build loop
    } // Close Worker management loop

    // Hatchery management loop
    if ( u->getType().isResourceDepot() ) // A resource depot is a Command Center, Nexus, or Hatchery. Don't build troops if we are tech starved, they will be shit tier units.
       //Does a hatchery count as idle if it is researching but has idle larve? Might have to directly select larve at this point.
    {
       //Supply blocked protection loop.
       if (supply_starved &&
           !u->train(UnitTypes::Zerg_Overlord)) { 
       PrintError_Unit(u);
      } // closure: construct more supply
        //Army build/replenish loop.  Note army also serves the purpose of scouting.
	   else if ( (army_derivative >= econ_derivative || vision_derivative >=econ_derivative) &&
           pool > 0 &&
          !u->train(UnitTypes::Zerg_Zergling)) 
      {
        PrintError_Unit(u);
	  } // Closure: army loop
        //Econ Build/replenish loop. Will build workers if I have no spawning pool, or if there is a worker shortage.
       else if ( ( MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Spawning_Pool) == 0 || econ_derivative > army_derivative ) &&
           (min_workers < min_fields * 2 || gas_workers >= 3 * MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Extractor) ) &&
          !u->train(u->getType().getRace().getWorker()))
      { 
        PrintError_Unit(u);
      } // closure: failed to train worker unit
    } //Closure : Resource depot loop

    //Combat loop. Has some sophistication at this time.

        int pos_x = u->getPosition().x;
        int pos_y = u->getPosition().y;

        Unitset e_set = Broodwar->enemy()->getUnits();
        if (!e_set.empty()) {
            for (auto e = e_set.begin(); e != e_set.end(); ++e) {
                Broodwar->drawCircleMap( (*e)->getPosition(), 250, Colors::Red); // not conserving APM well here ... might actually experience some delays.

                if (MeatAI::UnitUtil::IsCombatUnit(*e)) // make retreat/attack decision.  Only retreat if your army is not up to snuff.
                {
                    int e_pos_x = (*e)->getPosition().x;
                    int e_pos_y = (*e)->getPosition().y;

                    int dist_x = pos_x - e_pos_x;
                    int dist_y = pos_y - e_pos_y;

                    double dist = pow(pow(dist_x, 2) + pow(dist_y, 2), 0.5);
                    if (dist < 300 && ( army_starved || !MeatAI::UnitUtil::IsCombatUnit(u) ) ){ // a buffer of 50 pixels beyond my "bare mininimum" safe zone of 250. Run if you're a noncombat unit or army starved.
                        u->move({ -abs(dist_x) * 50 , -abs(dist_y) * 50 }); //identify vector between yourself and e.  go 50 pixels away from them             
                    }
                    else {
                        u->attack( u->getClosestUnit(BWAPI::Filter::IsEnemy) );
                    }
                } // closure make retreat/attack decision.
            }



        // an attempted kiting loop. Not very good for zerglings, for sure.
        //Unitset myUnits = Broodwar->self()->getUnits();
        //if (!myUnits.empty())
        //{
        //    for (auto u = myUnits.begin(); u != myUnits.end(); ++u)
        //    {
        //        if ( !(*u)->isFlying() && (*u)->isUnderAttack() ) // implies exists and isCompleted
        //        {
        //            Position pos = (*u)->getPosition(); // the unit must exist somewhere
        //            Unitset melee = (*u)->getUnitsInWeaponRange(WeaponTypes::Claws, Filter::IsEnemy); // The unit might just be in combat, mate.

        //            if (pos && !melee.empty())
        //            {
        //                double v_x = round( (*u)->getVelocityX() );  // will round to in in next lines
        //                double v_y = round( (*u)->getVelocityY() );

        //                (*u)->move( { (int)pos.x - (int)v_x, (int)pos.y - (int)v_y } ); // Retreat to inaccessible region, round those doubles to ints.

        //            } // unit's location is not empty..
        //        }// unit is of the relevant type. flying and under attack.
        //    }//  iterate over my units.
        //}// If my units are not empty.
	} 

    //Scouting loop. Very primative. move or attack-move with brownian motion, its map vision in any direction.
    if ( MeatAIModule::isIdleEmpty(u) && 
        u->getType() != UnitTypes::Zerg_Larva && 
        u->getType() != UnitTypes::Buildings && 
        u->getType() != UnitTypes::Zerg_Drone &&
        (vision_starved || u->getType() == UnitTypes::Zerg_Overlord) ) //Scout if you're an overlord.  Don't scout if you're a larva or a building please, please.
    {
        Brownian_Stutter(u, 2);

    //    //example code to save overlords. Now redundant, eats a lot of apm.
    //    Unitset myUnits = Broodwar->self()->getUnits();
    //    if (!myUnits.empty()) 
    //    {
    //        for (auto u = myUnits.begin(); u != myUnits.end(); ++u)
    //        {
    //            if ((*u)->isFlying() && (*u)->isUnderAttack()) // implies exists and isCompleted
    //            {
    //                Region r = (*u)->getRegion(); // the unit must exist in a region.
    //                if (r)
    //                {
    //                    Region i = r->getClosestInaccessibleRegion();
    //                    if (i) { // there better be an inacessable region somewhere.
    //                        Position pos = i->getCenter();
    //                        if (pos) { // Check if the center of that position is valid
    //                            (*u)->move(pos); // Retreat to inaccessible region
    //                        } // center is not empty
    //                    } //nearyby inacessable is not empty
    //                } // unit's region is not empty.
    //            }// unit is of the relevant type. flying and under attack.
    //        }//  iterate over my units.
    //    }// If my units are not empty.
    } // If it is a combat unit, then use it to attack the enemy.	

    //Upgrade loop:
    if (u->getType().isBuilding() && 
        MeatAIModule::isIdleEmpty(u) &&
        (u->canUpgrade() || u->canResearch() || u->canMorph()) && 
        (tech_starved > army_starved || ln_gas >= log(150) ) ) { // this will need to be revaluated once I buy units that cost gas.

        Check_N_Upgrade(UpgradeTypes::Metabolic_Boost, u, true);
        Check_N_Upgrade(UpgradeTypes::Zerg_Melee_Attacks,  u, true);
        Check_N_Upgrade(UpgradeTypes::Zerg_Carapace, u, true);

        Check_N_Build(UnitTypes::Zerg_Lair, u, (tech_derivative > army_derivative) &&
            MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Lair) == 0);
        Check_N_Upgrade(UpgradeTypes::Pneumatized_Carapace, u, true);
        Check_N_Upgrade(UpgradeTypes::Antennae, u, true);

        Check_N_Build(UnitTypes::Zerg_Hive, u, (tech_derivative > army_derivative) &&
            MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Queens_Nest) >= 0 &&
            MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Lair) >= 1); //If you're tech-starved at this point. build as many hives or lairs as you would like. You don't know what to do with yourself.
        Check_N_Upgrade(UpgradeTypes::Adrenal_Glands, u, true);

        PrintError_Unit(u);
    } //closure: tech/upgrades loop

    //Creep Colony Loop.
    if (u->getType() == UnitTypes::Zerg_Creep_Colony &&
        (MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Spawning_Pool) > 0 || MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Evolution_Chamber) > 0) &&
        army_derivative > econ_derivative &&
        army_derivative > tech_derivative) {
            if (MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Spawning_Pool) > 0 && 
                MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Evolution_Chamber) > 0) {
                if (rand() % 100 + 1 > 50) {
                    Check_N_Build(UnitTypes::Zerg_Spore_Colony, u, true);
                }
                else {
                    Check_N_Build(UnitTypes::Zerg_Sunken_Colony, u, true);
                } 
            } // build one of the two colonies at random if you have both prequisites
            if (MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Spawning_Pool) > 0 &&
                MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Evolution_Chamber) == 0) {
                    Check_N_Build(UnitTypes::Zerg_Sunken_Colony, u, true);
                } // build sunkens if you only have that
            if (MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Spawning_Pool) == 0 &&
                MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Evolution_Chamber) > 0) {
                Check_N_Build(UnitTypes::Zerg_Spore_Colony, u, true);
            } // build spores if you only have that.
        } // closure: Creep colony loop
  } // closure: unit iterator
} // closure: Onframe

void MeatAIModule::onSendText(std::string text)
{

  // Send the text to the game if it is not being processed.
  Broodwar->sendText("%s", text.c_str());

  // Make sure to use %s and pass the text as a parameter,
  // otherwise you may run into problems when you use the %(percent) character!

}

void MeatAIModule::onReceiveText(BWAPI::Player player, std::string text)
{
  // Parse the received text
  Broodwar << player->getName() << " said \"" << text << "\"" << std::endl;
}

void MeatAIModule::onPlayerLeft(BWAPI::Player player)
{
  // Interact verbally with the other players in the game by
  // announcing that the other player has left.
  Broodwar->sendText("Goodbye %s!", player->getName().c_str());
}

void MeatAIModule::onNukeDetect(BWAPI::Position target)
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
    Broodwar->sendText("Where's the nuke?");
  }

  // You can also retrieve all the nuclear missile targets using Broodwar->getNukeDots()!
}

void MeatAIModule::onUnitDiscover(BWAPI::Unit unit)
{
}

void MeatAIModule::onUnitEvade(BWAPI::Unit unit)
{
}

void MeatAIModule::onUnitShow(BWAPI::Unit unit)
{
}

void MeatAIModule::onUnitHide(BWAPI::Unit unit)
{
}

void MeatAIModule::onUnitCreate(BWAPI::Unit unit)
{
  if ( Broodwar->isReplay() )
  {
    // if we are in a replay, then we will print out the build order of the structures
    if ( unit->getType().isBuilding() && !unit->getPlayer()->isNeutral() )
    {
      int seconds = Broodwar->getFrameCount()/24;
      int minutes = seconds/60;
      seconds %= 60;
      Broodwar->sendText("%.2d:%.2d: %s creates a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str());
    }
  }
}

void MeatAIModule::onUnitDestroy(BWAPI::Unit unit)
{
}

void MeatAIModule::onUnitMorph(BWAPI::Unit unit)
{
  if ( Broodwar->isReplay() )
  {
    // if we are in a replay, then we will print out the build order of the structures
    if ( unit->getType().isBuilding() && 
        !unit->getPlayer()->isNeutral() )
    {
      int seconds = Broodwar->getFrameCount()/24;
      int minutes = seconds/60;
      seconds %= 60;
      Broodwar->sendText("%.2d:%.2d: %s morphs a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str());
    }
  }
}

void MeatAIModule::onUnitRenegade(BWAPI::Unit unit)
{
}

void MeatAIModule::onSaveGame(std::string gameName)
{
  Broodwar << "The game was saved to \"" << gameName << "\"" << std::endl;
}

void MeatAIModule::onUnitComplete(BWAPI::Unit unit)
{
}


// Personally assembled functions are here

//Checks if a building can be built, and passes additional boolean criteria.  If all critera are passed, then it builds the building and delays the building timer 25 frames, or ~1 sec. It may now allow morphing, eg, lair, hive and lurkers, but this has not yet been tested.
void MeatAIModule::Check_N_Build(UnitType building, Unit unit, bool extra_critera)
{
    if (unit->canBuild(building) &&
        Broodwar->self()->minerals() >= building.mineralPrice() &&
        Broodwar->self()->gas() >= building.gasPrice() &&
        extra_critera)
    {   TilePosition buildPosition = Broodwar->getBuildLocation(building, unit->getTilePosition(), 64, building == UnitTypes::Zerg_Creep_Colony);
        unit ->build( building , buildPosition);
        t_build += 25;
    }

    if (unit->canMorph(building) &&
        Broodwar->self()->minerals() >= building.mineralPrice() &&
        Broodwar->self()->gas() >= building.gasPrice() &&
        extra_critera)
    {
        unit->morph(building);
        t_build += 25;
    }
}

//Checks if an upgrade can be built, and passes additional boolean criteria.  If all critera are passed, then it performs the upgrade. Requires extra critera.
void MeatAIModule::Check_N_Upgrade(UpgradeType ups, Unit unit, bool extra_critera)
{
    if (unit->canUpgrade( ups ) &&
        Broodwar->self()->minerals() >= ups.mineralPrice() &&
        Broodwar->self()->gas() >= ups.gasPrice() && 
        extra_critera) {
        unit->upgrade( ups );
    }
}

//Forces a unit to stutter in a brownian manner. Size of stutter is unit's (vision range * n ). Will attack if it sees something.
void MeatAIModule::Brownian_Stutter(Unit unit , int n) {
    
    Position pos = unit->getPosition();
    
    //if (full_map == true) {
    //    int x_loc = (rand() % 100 + 1) * (32 * Broodwar->mapWidth()) / 100;
    //    int y_loc = (rand() % 100 + 1) * (32 * Broodwar->mapHeight()) / 100;
    //    Position brownian_pos = { x_loc , y_loc }; //tiles are 32x32. Watch for int/double errors.  This is an option considered but not implemented.
    //} else

    int x_stutter = n * (rand() % 100 - 50) / 25 * unit->getType().sightRange();
    int y_stutter = n * (rand() % 100 - 50) / 25 * unit->getType().sightRange();

    Position brownian_pos = { pos.x + x_stutter , pos.y + y_stutter }; // note this function generates a number -49..49 then divides by 25, and is int. So it will truncate the value, producing int -1..+1. The naieve approach of rand()%3 - 1 is "distressingly non-random in lower order bits" according to stackoverflow and other sources.

    if ( unit->canAttack(brownian_pos) ) {
        unit->attack(brownian_pos);
    }
    else {
        unit->move(brownian_pos);
    }
};

// Gets units last error and prints it directly onscreen.
void MeatAIModule::PrintError_Unit(Unit unit ) {
    Position pos = unit ->getPosition();
    Error lastErr = Broodwar->getLastError();
    Broodwar->registerEvent([pos, lastErr](Game*) { Broodwar->drawTextMap(pos, "%c%s", Text::Red, lastErr.c_str()); },   // action
        nullptr,    // condition
        Broodwar->getLatencyFrames());  // frames to run.
}

// An improvement on existing idle scripts. Returns true if it is idle. Checks if it is a laden worker, idle, or stopped. 
bool MeatAIModule::isIdleEmpty(Unit unit ) {
    bool laden_worker = unit->isCarryingGas() || unit->isCarryingMinerals();
    bool idle = unit->isIdle() || !unit->isMoving() ;
    return idle && !laden_worker;
}

// evaluates the value of a stock of buildings, in terms of total cost (min+gas). Assumes building is zerg and therefore, a drone was spent on it.
int MeatAIModule::Stock_Buildings(UnitType building) {
    int cost = building.mineralPrice() + building.gasPrice() + 50;
    int instances = MeatAI::UnitUtil::GetAllUnitCount(building);
    int total_stock = cost * instances;
    return total_stock;
}

// evaluates the value of a stock of upgrades, in terms of total cost (min+gas).
int MeatAIModule::Stock_Ups(UpgradeType ups) {
    int cost = ups.mineralPrice() + ups.gasPrice();
    int instances = Broodwar->self()->getUpgradeLevel(ups);
    int total_stock = cost * instances;
    return total_stock;
}

// evaluates the value of a stock of units, in terms of total cost (min+gas). Doesn't consider the counterfactual larva.
int MeatAIModule::Stock_Units(UnitType unit) {
    int cost = unit.mineralPrice() + unit.gasPrice();
    int instances = MeatAI::UnitUtil::GetAllUnitCount(unit);
    int total_stock = cost * instances;
    return total_stock;
}

// evaluates the value of a stock of unit, in terms of supply added.
int MeatAIModule::Stock_Supply(UnitType unit) {
    int supply = unit.supplyProvided();
    int instances = MeatAI::UnitUtil::GetAllUnitCount(unit);
    int total_stock = supply * instances;
    return total_stock;
}

//void MeatAIModule::Inventory()
