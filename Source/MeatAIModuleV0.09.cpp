#include "MeatAIModule.h"
#include "UnitUtil.h"
#include <iostream>

// MeatAI V0.09. Current V goal-> Clean up issues.
// unresolved issue: Sometimes pulls more than one worker for a command at a time. Ad hoc time delay was added. 
// Unresolved issue: Text coloration in on screen messages. Screen messages doesn't take color commands, but does take printf format commands. Gets messy very fast.
// Unresolved issue: will build Multiple gas gysers when it isn't using any.
// Unresolved issue: % vision is relatively weak idea. do you need to scout with more desperation on bigger maps. Perhaps ln(Vision squares )/ln( vision squares + total gathering)
// Unresolved issue: workers run from gas to mineral patch with gas in hand. Solved by removing queuing from -returngas command. Now workers idle with minerals in their hands, not returning them. maybe something with getOrder could be used?
// use that priority list once we get to 4. it will be unweildly otherwise
// proportion army will need updating.

using namespace BWAPI;
using namespace Filter;

void MeatAIModule::onStart()
{
  // Hello World!
  Broodwar->sendText("Hello world! This is MeatShieldAI V0.09");

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
{
  
  // Return if the game is a replay or is paused
  if ( Broodwar->isReplay() || Broodwar->isPaused() || !Broodwar->self() )
    return;

  // Called once every game frame

  //Define important variables.
  //Initialize state variables
  gas_starved = false;
  army_starved = false;
  supply_starved = false;
  vision_starved = false;
  econ_starved = true;

  //Initialize model variables
  delta = 0.00; //gas starved parameter. Triggers state if: ln_gas/(ln_min + ln_gas) < delta;  Higher is more gas. Seems to be ok around .1-.3
  gamma = 0.22; //supply starved parameter. Triggers state if: ln_supply_remain/ln_supply_total < gamma;

                //Cobb-Douglas Production exponents.  Should all sum to one 
  alpha_army = 0.009999999999; // army starved parameter.  
  alpha_vis = 0.99; // vision starved parameter. 
  alpha_econ = 0.000000000001; // econ starved parameter. 

  //Unit inventory.
  int pool = MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Spawning_Pool);
  int extractor = MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Extractor); 
  int hatches = MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Hatchery);
  int ovis = MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Overlord);
  int larve = MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Larva);
  int morphing_count = Broodwar->self()->incompleteUnitCount(UnitTypes::Buildings);
    int* morphing_count_ptr = &morphing_count;
    //Idea: MeatAI::UnitUtil::GetAllUnitCount(UnitCommandTypes::Build)  What command reads Unitcommandtype chars?

  //Econ inventory
  int worker_count = MeatAI::UnitUtil::GetAllUnitCount( BWAPI::Broodwar->self()->getRace().getWorker() );
  //double econ_heuristic = worker_count * (UnitTypes::Zerg_Drone.mineralPrice() + 1 / 24) + exp(ln_min) /*+ exp(ln_gas) */ );
  // 1 mineral/sec, 24 frames per sec according to Krasi0's author, so this should be the $$ we have on hand at this frame. (http://legionbot.blogspot.com/2015/03/worker-payoff-time.html) 

  //Army inventory.
  int ling_count = MeatAI::UnitUtil::GetAllUnitCount( UnitTypes::Zerg_Zergling );
  int army_count = ling_count;


  //Inventories for knee-jerk states: Gas, Supply.
    //Supply inventory:
    double ln_supply_remain = log( MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Overlord) * UnitTypes::Zerg_Overlord.supplyProvided() +
        MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Hatchery) * UnitTypes::Zerg_Hatchery.supplyProvided() 
        - Broodwar->self()->supplyUsed() );
    double ln_supply_total = log(Broodwar->self()->supplyTotal() + 1);

    //Gas inventory.
    double ln_min = log(Broodwar->self()->minerals() + 1);
    double ln_gas = log(Broodwar->self()->gas() + 1); // + 1 is a minor adjustment to avoid ln(0) errors
    int gas_workers = 0; // this approach has been troublesome.
    int min_workers = 0;
  
  //Vision inventory:
  // Map area could be initialized on startup, since maps do not vary once made.
  int map_x = Broodwar->mapWidth();
  int map_y = Broodwar->mapHeight();
  int map_area = map_x * map_y; // map area in tiles.
  int vision_tile_count = 0;
  int* vision_tile_count_ptr = &vision_tile_count;

  for (int tile_x = 1; tile_x <= map_x; tile_x++) { // there is no tile (0,0)
      for (int tile_y = 1; tile_y <= map_y; tile_y++) {
          if ( Broodwar->isVisible(tile_x, tile_y) ) {
              *vision_tile_count_ptr += 1;
          }
      }
  } // this search must be very exhaustive to do every frame. But C++ does it without any problems.
  //double vision_pct = vision_tile_count / (double)map_area;  // so this was initially the ratio of 2 ints. If two ints divide by one another, the result is an int. The int (without decimal values) is then recast to double. That was a problem. So we are now casting one of them as a double, so it knows the result is a double.

  //Knee-jerk states: gas, supply.
  gas_starved = (ln_gas / (ln_min + ln_gas)) < delta;  //If your gas is disproportionately low, then you are gas starved.
  supply_starved = ln_supply_remain / ln_supply_total < gamma && //If you have eaten up of your supply
      !(ln_supply_total >= log(200) ) ; // you have a hard cap at 200, you are not starved at that limit

  // Cobb-Douglas 
  // ln_y = alpha_1 * log(vision_tile_count/worker_count)  + alpha_2 * log(army_count/worker_count) + alpha_3 * log(tech_heuristic/worker_count)
  ln_Y = alpha_vis * log(vision_tile_count) + alpha_army * log(army_count) /* + alpha_tech * log(tech_heuristic) */ + alpha_econ * log(worker_count);
      // so prioritization should be whichever is greatest: alpha_vis/vision_tile_count, alpha_army/army_count, alpha_tech/tech_heuristic, alpha_econ/worker_count.
      // double priority_list[] = { alpha_vis/vision_tile_count, alpha_army/army_count, alpha_econ/worker_count}; // would be more elegant to use this.

      double econ_derivative = alpha_econ / worker_count;
      double vision_derivative = alpha_vis / vision_tile_count;
      double army_derivative = alpha_army / army_count;

      // Set Priorities
      if ( econ_derivative > vision_derivative &&
          econ_derivative > army_derivative ) 
      {
          vision_starved = false;
          army_starved = false;
          econ_starved = true;
      }
      else if (vision_derivative > econ_derivative && 
          vision_derivative > army_derivative)
      {
          vision_starved = true;
          army_starved = false;
          econ_starved = false;
      }   
      else if (army_derivative > vision_derivative && 
          army_derivative > econ_derivative)
      {
          vision_starved = false;
          army_starved = true;
          econ_starved = false;
      }
      else {
          Broodwar->drawTextScreen(10, 200, " Status: Error No - Status ", Text::Size::Huge); //
      }

  // Display the game status indicators at the top of the screen	
	  Broodwar->drawTextScreen(10, 0 , "Inventory:", Text::DarkGreen); // working on a better inventory.
	  Broodwar->drawTextScreen(10, 10, "Workers: %d", worker_count);
	  Broodwar->drawTextScreen(10, 20, "Lings: %d", ling_count);  //
	  Broodwar->drawTextScreen(10, 30, "Pool: %d", pool);  // 
	  Broodwar->drawTextScreen(10, 40, "Extractor: %d", extractor);  //
	  Broodwar->drawTextScreen(10, 50, "Hatches: %d", hatches);  //
	  Broodwar->drawTextScreen(10, 60, "Ovis: %d", ovis);  //
	  Broodwar->drawTextScreen(10, 70, "Morphing Count: %d", morphing_count);  //
	  Broodwar->drawTextScreen(10, 80, "Idle Larve: %d", larve);  //

      Broodwar->drawTextScreen(10, 265, "Game Status (Ln Y) : %f", ln_Y, Text::Size::Huge); //
	  Broodwar->drawTextScreen(10, 275, "Game Time: %d minutes", ( Broodwar->elapsedTime() ) / 60 , Text::Size::Huge ); //

      Broodwar->drawTextScreen(100, 0, "Econ Starved: %s", econ_starved ? "TRUE" : "FALSE");  //
      Broodwar->drawTextScreen(100, 10, "Army Starved: %s", army_starved ? "TRUE" : "FALSE");  //
      Broodwar->drawTextScreen(100, 20, "Vision Starved: %s", vision_starved ? "TRUE" : "FALSE");  //
      Broodwar->drawTextScreen(100, 40, "Supply Starved: %s", supply_starved ? "TRUE" : "FALSE");
      Broodwar->drawTextScreen(100, 50, "Gas Starved: %s", gas_starved ? "TRUE" : "FALSE");
      
      Broodwar->drawTextScreen(250, 0, "Econ Derivative: %f", econ_derivative );  //
      Broodwar->drawTextScreen(250, 10, "Army Derivative: %f", army_derivative ); //
      Broodwar->drawTextScreen(250, 20, "Vision Derivative: %f", vision_derivative ); //

      Broodwar->drawTextScreen(450, 100, "Proportion Gas: %f", ln_gas / (ln_min + ln_gas));
	  Broodwar->drawTextScreen(450, 110, "Proportion Army Stock: %f", army_count * 50 / ((double) worker_count * 50 + army_count * 50 ) ); //
      Broodwar->drawTextScreen(450, 120, "Vision (Pct.): %f", vision_tile_count / (double) map_area);  //
      Broodwar->drawTextScreen(450, 130, "Supply Heuristic: %f", ln_supply_remain / ln_supply_total);  //
      Broodwar->drawTextScreen(450, 140, "Vision tile count: %d", vision_tile_count);  //
      Broodwar->drawTextScreen(450, 150, "Map area: %d", map_area);  //

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
  for (auto &u : Broodwar->self()->getUnits())
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
      // Mining loop if our worker is idle (includes returning $$$) or not moving while gathering gas, we (re-) evaluate what they should be mining.  Original script uses isIdle() only.
		if ( u->isIdle() ||
            !u->isMoving() )
		{
			// Order workers carrying a resource to return them to the center,
			// otherwise find a mineral patch to harvest.
			if (u->isCarryingGas() ||
                u->isCarryingMinerals() ) // Return $$$
			{
				u->returnCargo(true);
			} //Closeure: returning $$ loop
			else if (!u->getPowerUp())  // The worker cannot harvest anything if it
			{                             // is carrying a powerup such as a flag
			// Idle worker then Harvest from the nearest mineral patch or gas refinery, depending on need.

				if ( extractor == 0 || 
                    !gas_starved || 
                    gas_workers >= 3 * extractor )
				{
					u->gather(u->getClosestUnit(IsMineralField), true);
						++min_workers;
				}
				else if (extractor > 0	&& 
                    gas_starved && 
                    gas_workers < 3 * extractor ) // If you have an extractor and you need gas then collect some gas. Queue this command, so complete your current task first.
				{
					u->gather(u->getClosestUnit(IsRefinery), true);
						++gas_workers;
				}
				/*
				else
				{
					// If the call fails, then print the last error message
					Broodwar << Broodwar->getLastError() << std::endl;
				}
				*/

			} // closure:powerup loop
		}// Closure: idle loop

	// Begin a loop for creating buildings of the appropriate type. Note builder resets every few frames.
		if ( ( u->isIdle() || ( u->isGatheringMinerals() && !u->isCarryingMinerals() ) ) &&
            t + 50 < Broodwar->getFrameCount() &&
            morphing_count==0 )
		{ //only get those that are idle or gathering minerals, but not carrying them. This always irked me. Don't send a new builder if you have an existing builder in this loop.

            t = Broodwar->getFrameCount();

			  //Gas Loop
			  if ( ln_min >= log( UnitTypes::Zerg_Extractor.mineralPrice() + 1 ) &&
                  gas_starved ) 
			  { //if we have enough minerals, and too many minerals... Get an extractor.
				  TilePosition buildPosition = Broodwar->getBuildLocation(UnitTypes::Zerg_Extractor, u->getTilePosition());
				  u->build(UnitTypes::Zerg_Extractor, buildPosition);
				  t += 25;
                  //*morphing_count_ptr += 1; 
              }

			  //Army Loop
			  if ( ln_min >= log( UnitTypes::Zerg_Spawning_Pool.mineralPrice() + 1 ) &&
                  army_starved && pool==0 
				  )
	 		  { //if we have enough minerals, and are army starved, build a building.
				  TilePosition buildPosition = Broodwar->getBuildLocation(UnitTypes::Zerg_Spawning_Pool, u->getTilePosition());
				  u->build(UnitTypes::Zerg_Spawning_Pool, buildPosition);
				  t += 25;
              }

			  //Macro Hatch Loop
			  if ( ln_min >= log( UnitTypes::Zerg_Hatchery.mineralPrice() * 1.5 + 1 ) )
			  { //if we are floating too much money, get a macro hatch.
				  TilePosition buildPosition = Broodwar->getBuildLocation(UnitTypes::Zerg_Hatchery, u->getTilePosition());
				  u->build(UnitTypes::Zerg_Hatchery, buildPosition);
				  t += 25;  //about a 1 second search time on macro hatches.
				  // t += UnitTypes::Zerg_Hatchery.buildTime();  // No such delay on spam hatcheries.
			  }

			  //Tech Loop

		  } // Close Build loop
    } // Close Worker management loop

    // Hatchery management loop
    else if ( u->getType().isResourceDepot() ) // A resource depot is a Command Center, Nexus, or Hatchery
    {
       //If you're supply blocked:
       if ( u->isIdle() &&
           supply_starved &&
           !u->train(UnitTypes::Zerg_Overlord)) { // I should make this into a method? Approach? Call this response.
                                                  // If that fails, draw the error at the location so that you can visibly see what went wrong!
                                                  // However, drawing the error once will only appear for a single frame
                                                  // so create an event that keeps it on the screen for some frames
           Position pos = u->getPosition();
           Error lastErr = Broodwar->getLastError();
           Broodwar->registerEvent([pos, lastErr](Game*) { Broodwar->drawTextMap(pos, "%c%s", Text::Red, lastErr.c_str()); },   // action
               nullptr,    // condition
               Broodwar->getLatencyFrames());  // frames to run
      } // closure: construct more supply
        //Otherwise, we should build army.
	   if ( u->isIdle() && 
          pool > 0 && 
          army_starved && 
          !u->train(UnitTypes::Zerg_Zergling)) 
      {
		  Position pos = u->getPosition();
		  Error lastErr = Broodwar->getLastError();
		  Broodwar->registerEvent([pos, lastErr](Game*) { Broodwar->drawTextMap(pos, "%c%s", Text::Red, lastErr.c_str()); },   // action
			  nullptr,    // condition
			  Broodwar->getLatencyFrames());  // frames to run } // closure: construct more army.
	  } // Closure: army loop
        // Econ_loop
       if ( u->isIdle() &&
          !(army_starved && pool > 0) &&
          !u->train(u->getType().getRace().getWorker()))
      { // I should make this into a method? Approach? Call this response.

          Position pos = u->getPosition();
          Error lastErr = Broodwar->getLastError();
          Broodwar->registerEvent([pos, lastErr](Game*) { Broodwar->drawTextMap(pos, "%c%s", Text::Red, lastErr.c_str()); },   // action
              nullptr,    // condition
              Broodwar->getLatencyFrames());  // frames to run

              //// Retrieve the supply provider type in the case that we have run out of supplies
                                              //UnitType supplyProviderType = u->getType().getRace().getSupplyProvider();
                                              //static int lastChecked = 0;

              //// If we are supply blocked and haven't tried constructing more recently
                                              //if (  lastErr == Errors::Insufficient_Supply &&
                                              //      lastChecked + 400 < Broodwar->getFrameCount() &&
                                              //      Broodwar->self()->incompleteUnitCount(supplyProviderType) == 0 )
                                              //{
                                              //  lastChecked = Broodwar->getFrameCount();

              //  // Retrieve a unit that is capable of constructing the supply needed
                                              //  Unit supplyBuilder = u->getClosestUnit(  GetType == supplyProviderType.whatBuilds().first &&
                                              //                                            (IsIdle || IsGatheringMinerals) &&
                                              //                                            IsOwned);
                                              //  // If a unit was found
                                              //  if ( supplyBuilder )
                                              //  {
                                              //    if ( supplyProviderType.isBuilding() )
                                              //    {
                                              //      TilePosition targetBuildLocation = Broodwar->getBuildLocation(supplyProviderType, supplyBuilder->getTilePosition());
                                              //      if ( targetBuildLocation )
                                              //      {
                                              //        // Register an event that draws the target build location
                                              //        Broodwar->registerEvent([targetBuildLocation,supplyProviderType](Game*)
                                              //                                {
                                              //                                  Broodwar->drawBoxMap( Position(targetBuildLocation),
                                              //                                                        Position(targetBuildLocation + supplyProviderType.tileSize()),
                                              //                                                        Colors::Blue);
                                              //                                },
                                              //                                nullptr,  // condition
                                              //                                supplyProviderType.buildTime() + 100 );  // frames to run

              //        // Order the builder to construct the supply structure
                                              //        supplyBuilder->build( supplyProviderType, targetBuildLocation );
                                              //      }
                                              //    }
                                              //    else
                                              //    {
                                              //      // Train the supply provider (Overlord) if the provider is not a structure
                                              //      supplyBuilder->train( supplyProviderType );
                                              //    }
                                              //  } // closure: supplyBuilder is valid
                                              //} // closure: insufficient supply
      } // closure: failed to train worker unit
    } //Closure : Resource depot loop

    //Combat loop. Very primative, literally attacks the nearest enemy
	else if (MeatAI::UnitUtil::IsCombatUnit(u) && 
        !u->attack(u->getClosestUnit(BWAPI::Filter::IsEnemy) ) )
	{
		//Position pos = u->getPosition();
		//Error lastErr = Broodwar->getLastError();
		//Broodwar->registerEvent([pos, lastErr](Game*) { Broodwar->drawTextMap(pos, "%c%s", Text::Red , lastErr.c_str()); },   // action
		//	nullptr,    // condition
		//	Broodwar->getLatencyFrames());  // frames to run.
	} // If it is a combat unit, then use it to attack the enemy.	

    //Scouting loop. Very primative. Move to a random location - literally, a random one.
    if ( u->isIdle()
        && vision_starved )
    {
        Position pos = u->getPosition();
        u->move( { (rand() % 100 + 1) * (32 * map_x) / 100 , (rand() % 100 + 1) * (32 * map_y) / 100 } ); //tiles are 32x32. Care about int/double errors.
        Error lastErr = Broodwar->getLastError();
        Broodwar->registerEvent([pos, lastErr](Game*) { Broodwar->drawTextMap(pos, "%c%s", Text::Red , lastErr.c_str()); },   // action
        	nullptr,    // condition
        	Broodwar->getLatencyFrames());  // frames to run.
    } // If it is a combat unit, then use it to attack the enemy.	
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
