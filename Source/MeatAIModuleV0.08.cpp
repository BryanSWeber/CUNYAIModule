#include "MeatAIModule.h"
#include "UnitUtil.h"
#include <iostream>

// MeatAI V0.08. Current V goal-> Have unit move at random.
// unresolved issue: Sometimes pulls more than one worker for a command at a time. Ad hoc time delay was added. Probably a pointer issue. See Vision loop for solution.
// Unresolved issue: Text coloration in on screen messages. Screen messages doesn't take color commands, but does take printf format commands. Gets messy very fast.
// Unresolved issue: Multiple gas gysers when it isn't using any.
// Unresolved issue: rename

using namespace BWAPI;
using namespace Filter;

void MeatAIModule::onStart()
{
	//Initialize state variables
	gas_starved = false;
	army_starved = false;
	supply_starved = false;
    vision_starved = false;

	//Initialize model variables
	delta = 0.20; //gas starved parameter. Triggers state if: ln_gas/(ln_min + ln_gas) < delta;  Higher is more gas.
	phi = 0.5275; //army starved parameter. Triggers state if: army_heurisitic/(army_heuristic+econ_heuristic) < phi; Higher is more army. Seems good somewhere slightly above 50.
	gamma = 0.15; //supply starved parameter. Triggers state if: ln_supply_remain/ln_supply_total < gamma;
    rho = 0.95; //vision starved parameter. Triggers state if:  vision(%)< rho * an ad-hoc game progress adjustement;

  // Hello World!
  Broodwar->sendText("Hello world! This is MeatShieldAI V0.08");

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

  //econ inventory.
  double ln_min = log(Broodwar->self()->minerals() + 5);
  double ln_gas = log(Broodwar->self()->gas() + 5);

  int gas_workers = 0; // this approach is pretty weak
  int min_workers = 0;

  int worker_count = MeatAI::UnitUtil::GetAllUnitCount(BWAPI::Broodwar->self()->getRace().getWorker());
  
  //Unit inventory.
  int pool = MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Spawning_Pool);
  int extractor = MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Extractor); 
  int hatches = MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Hatchery);
  int ovis = MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Overlord);
  int larve = MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Larva);
  int morphing_count = Broodwar->self()->incompleteUnitCount(UnitTypes::Buildings);
    //int* morphing_count_ptr = &morphing_count;
    //Idea: MeatAI::UnitUtil::GetAllUnitCount(UnitCommandTypes::Build)  What command reads Unitcommandtype chars?

  //Army inventory.
  int ling_count = MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Zergling);
  double army_heuristic = log( ling_count * UnitTypes::Zerg_Zergling.mineralPrice() + worker_count * 5);
  double econ_heuristic = log( worker_count * ( UnitTypes::Zerg_Drone.mineralPrice() + 1 / 24) + exp(ln_min) /*+ exp(ln_gas) */ );
	// 1 mineral/sec, 24 frames per sec according to Krasi0's author, so this should be the $$ we have on hand at this frame. (http://legionbot.blogspot.com/2015/03/worker-payoff-time.html) 
  double army_econ_ratio = army_heuristic / (econ_heuristic + army_heuristic);  //army_econ_ratio = (log(pow(Broodwar->self()->getUnitScore(), 2)) / log(pow(Broodwar->self()->gatheredGas() + 5, 2) + pow(Broodwar->self()->gatheredMinerals() + 5, 2))); // Would prefer to use unit powers rather than this.

  //Supply inventory:
  double ln_supply_remain = log( Broodwar->self()->supplyTotal() - Broodwar->self()->supplyUsed() + 
      Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Overlord) * UnitTypes::Zerg_Overlord.supplyProvided() +
      Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Hatchery) * UnitTypes::Zerg_Hatchery.supplyProvided() + 1 );
  double ln_supply_total = log( Broodwar->self()->supplyTotal() + 1 );
  
  //Vision inventory:
  // Map area could be initialized on startup, since maps do not vary once made.
  int map_x = Broodwar->mapWidth();
  int map_y = Broodwar->mapHeight();
  int map_area = map_x * map_y;
  int vision_tile_count = 0;
  int* vision_tile_count_ptr = &vision_tile_count;

  for (int tile_x = 1; tile_x <= map_x; tile_x++) {
      for (int tile_y = 1; tile_y <= map_y; tile_y++) {
          if ( Broodwar->isVisible(tile_x, tile_y) ) {
              *vision_tile_count_ptr += 1;
          }
      }
  } // this search must be very exhaustive to do every frame.  Move to once every 24 frames?
  double vision_pct = vision_tile_count / (double)map_area;  // so this was initially the ratio of 2 ints. If two ints divide by one another, the result is an int. The int (without decimal values) is then recast to double. That was a problem. So we are now casting one of them as a double, so it knows the result is a double.

  //Define important states.
  gas_starved = (ln_gas / (ln_min + ln_gas)) < delta;  //If your gas collection disproportionately low, then you are gas starved.
  army_starved = (army_econ_ratio < phi); // If your army is far smaller than your economy can support, then you are army starved.
  supply_starved = ln_supply_remain / ln_supply_total < gamma && //If you have eaten up of your supply
      !(ln_supply_total >= 200) ; // you have a hard cap at 200, you are not starved at that limit
  vision_starved = vision_pct < rho; // as real game progress goes on you expect more map vision.  Very ad-hoc.

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

	  Broodwar->drawTextScreen(10, 275, "Game Time: %d minutes", ( Broodwar->elapsedTime() ) / 60 , Text::Size::Huge ); //

	  Broodwar->drawTextScreen(100, 0 , "Econ State:", Text::DarkGreen );  // What is the econ state?
	  Broodwar->drawTextScreen(100, 10, "Gas Starved: %s", gas_starved ? "TRUE" : "FALSE", Text::BrightRed);
	  Broodwar->drawTextScreen(100, 20, "Econ Heuristic: %f", econ_heuristic);  //
	  Broodwar->drawTextScreen(100, 30, "Proportion Gas: %f", ln_gas / (ln_min + ln_gas));

	  Broodwar->drawTextScreen(100, 50, "Supply State:", Text::DarkGreen );  // What is the Supply state?
	  Broodwar->drawTextScreen(100, 60, "Supply Starved: %s", supply_starved ? "TRUE" : "FALSE", Text::BrightRed);
	  Broodwar->drawTextScreen(100, 70, "Supply Heuristic: %f", ln_supply_remain / ln_supply_total );  //

	  Broodwar->drawTextScreen(250, 0 , "Army State:", Text::DarkGreen );  // What is the army state?
	  Broodwar->drawTextScreen(250, 10, "Army Starved: %s", army_starved ? "TRUE" : "FALSE", Text::BrightRed );  //
	  Broodwar->drawTextScreen(250, 20, "Proportion Army: %f", army_econ_ratio); // 
	  Broodwar->drawTextScreen(250, 30, "Army Heuristic: %f", army_heuristic); // 

      Broodwar->drawTextScreen(250, 50, "Vision State:", Text::DarkGreen);  // What is the vision state?
      Broodwar->drawTextScreen(250, 60, "Vision Starved: %s", vision_starved ? "TRUE" : "FALSE");  //
      Broodwar->drawTextScreen(250, 70, "Vision Heuristic: %f", rho);  //
      Broodwar->drawTextScreen(250, 80, "Vision (Pct.): %f", vision_pct);  //
      Broodwar->drawTextScreen(250, 90, "Vision of (0,0): %d", Broodwar->isVisible(0,0));  //
      Broodwar->drawTextScreen(250, 100, "Vision of (128,128): %d", Broodwar->isVisible(128, 128));  //
      Broodwar->drawTextScreen(250, 110, "Map X: %d", map_x );  //
      Broodwar->drawTextScreen(250, 120, "Map Y: %d", map_y );  //
      Broodwar->drawTextScreen(250, 130, "Vision tile count: %d", vision_tile_count);  //
      Broodwar->drawTextScreen(250, 140, "Map area: %d", map_area);  //


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
			
			  //Gas Loop
			  if ( ln_min >= log( UnitTypes::Zerg_Extractor.mineralPrice() + 5 ) &&
                  gas_starved ) 
			  { //if we have enough minerals, and too many minerals... Get an extractor.
				  TilePosition buildPosition = Broodwar->getBuildLocation(UnitTypes::Zerg_Extractor, u->getTilePosition());
				  u->build(UnitTypes::Zerg_Extractor, buildPosition);
				  t += UnitTypes::Zerg_Extractor.buildTime();
                  // ++*morphing_count_ptr; //this approach does not work because the value resets every frame.
              }

			  //Army Loop
			  if ( ln_min >= log( UnitTypes::Zerg_Spawning_Pool.mineralPrice() + 5 ) &&
                  army_starved && pool==0 
				  )
	 		  { //if we have enough minerals, and are army starved, build a building.
				  TilePosition buildPosition = Broodwar->getBuildLocation(UnitTypes::Zerg_Spawning_Pool, u->getTilePosition());
				  u->build(UnitTypes::Zerg_Spawning_Pool, buildPosition);
				  t += UnitTypes::Zerg_Spawning_Pool.buildTime();
                  // ++*morphing_count_ptr; //this approach does not work because the value resets every frame.
              }

			  //Macro Hatch Loop
			  if ( ln_min >= log( UnitTypes::Zerg_Hatchery.mineralPrice() * 1.5 + 5 ) &&
                  larve == 0
				  )
			  { //if we are floating too much money, get a macro hatch.
				  TilePosition buildPosition = Broodwar->getBuildLocation(UnitTypes::Zerg_Hatchery, u->getTilePosition());
				  u->build(UnitTypes::Zerg_Hatchery, buildPosition);
				  t += 50;  //about a 2 second search time on macro hatches.
				  // t += UnitTypes::Zerg_Hatchery.buildTime();  // No such delay on spam hatcheries.
				  // ++*morphing_count_ptr; //this approach does not work because the value resets every frame.
			  }

			  //Vision Loop
			  //Tech Loop
			  
			  int t = Broodwar->getFrameCount();

		  } // Close Build loop
    } // Close Worker management loop

    else if ( u->getType().isResourceDepot() ) // A resource depot is a Command Center, Nexus, or Hatchery
    {
      // Order the depot to construct more workers! But only when it is idle.
	  if ( u->isIdle() &&
          !(army_starved && pool > 0) &&
          !supply_starved &&
          !u->train( u->getType().getRace().getWorker() ) )
      { // I should make this into a method? Approach? Call this response.

        // If that fails, draw the error at the location so that you can visibly see what went wrong!
        // However, drawing the error once will only appear for a single frame
        // so create an event that keeps it on the screen for some frames
        Position pos = u->getPosition();
        Error lastErr = Broodwar->getLastError();
        Broodwar->registerEvent([pos,lastErr](Game*){ Broodwar->drawTextMap(pos, "%c%s", Text::Red, lastErr.c_str() ); },   // action
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

	  //Otherwise, we should build army.
	  if ( u->isIdle() && 
          pool > 0 && 
          army_starved && 
          !supply_starved && 
          !u->train(UnitTypes::Zerg_Zergling)) { // I should make this into a method? Approach? Call this response.
		// If that fails, draw the error at the location so that you can visibly see what went wrong!
		// However, drawing the error once will only appear for a single frame
		// so create an event that keeps it on the screen for some frames
		  Position pos = u->getPosition();
		  Error lastErr = Broodwar->getLastError();
		  Broodwar->registerEvent([pos, lastErr](Game*) { Broodwar->drawTextMap(pos, "%c%s", Text::Red, lastErr.c_str()); },   // action
			  nullptr,    // condition
			  Broodwar->getLatencyFrames());  // frames to run } // closure: construct more army.
	  } // Closure: army loop

	  //Otherwise, if you're supply blocked.
	  if (u->isIdle() && 
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
	  } // closure: construct more army.
    } //Closure : Resource depot loop

	/*
	if (u->isIdle() && (u->getType() == UnitTypes::Zerg_Zergling))
	{ // Attack Loop
		Unit closestEnemy = NULL;
		for (auto &e : Broodwar->enemy()->getUnits())
		{
			if ((closestEnemy == NULL) || ( u->getDistance(e) < closestEnemy->getDistance(u) ) )
			{
				closestEnemy = e;
			}
		}
		u->attack(closestEnemy, false);
	} // closure: attack loop long, interesting but inelegant.
	*/

	//if (MeatAI::UnitUtil::IsCombatUnit(u)
	//	&& u->isIdle()
	//	)
	//{
	//	u->attack(u->getClosestUnit(BWAPI::Filter::IsEnemy));
	//} // If it is a combat unit, then use it to attack the enemy.

	if (MeatAI::UnitUtil::IsCombatUnit(u) && 
        u->isIdle() && 
        !u->attack(u->getClosestUnit(BWAPI::Filter::IsEnemy) ) )
	{
		Position pos = u->getPosition();
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
