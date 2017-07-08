#include "MeatAIModule.h"
#include "c:\Users\Bryan\BWAPI\MeatAIModule\UnitUtil.h"
#include <iostream>


using namespace BWAPI;
using namespace Filter;

void MeatAIModule::onStart()
{
	//Initialize state variables
	pool = false;
	extractor = false;
	gas_starved = true;
	army_starved = true;

	//Initialize model variables
	delta = 0.25; //gas starved parameter. Triggers state if: ln_gas/(ln_min + ln_gas) < delta;
	phi = 0.50; //army starved parameter. Triggers state if: army_heurisitic/(army_heuristic+econ_heuristic) < phi;

	//Broodwar << "At what point are we gas starved? 0.75 is standard." << std::endl;
	//Broodwar >> delta;
	//Broodwar << "At what point are we army starved? 0.25 is standard." << std::endl;
	//Broodwar >> phi;

  // Hello World!
  Broodwar->sendText("Hello world! This is MeatShieldAI V0.05");

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
  // Called once every game frame
	
  //Define important variables.

	//econ inventory.
	double ln_min = log( Broodwar->self()->minerals() + 5 );
	double ln_gas = log( Broodwar->self()->gas() + 5 );
	int gas_workers = 0;
	int min_workers = 0;
	int worker_count = MeatAI::UnitUtil::GetAllUnitCount(BWAPI::Broodwar->self()->getRace().getWorker());

	
	//building inventory.
	bool pool = MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Spawning_Pool) > 0 ;
	bool extractor = MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Extractor) > 0 ;
	int hatches = MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Hatchery);
	int ovis = MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Overlord);

	bool builder_exists = false;

	//Army inventory.
	int ling_count = MeatAI::UnitUtil::GetAllUnitCount(UnitTypes::Zerg_Zergling);
	
	double army_heuristic = log( (ling_count) * 50 + worker_count * 5 );
	double econ_heuristic = log( worker_count * (50 + 1 / 24) + exp(ln_gas) + exp(ln_min) + hatches * 350 );  
	// 1 mineral/sec, 24 frames per sec according to Krasi0's author 
	// http://legionbot.blogspot.com/2015/03/worker-payoff-time.html) so this should be the $$ we have on hand at this frame.
	// Hatches represent a drone as well as stock.
	double army_econ_ratio =  army_heuristic / (econ_heuristic + army_heuristic) ; // need work.  Very odd habits. 
																				//army_econ_ratio = (log(pow(Broodwar->self()->getUnitScore(), 2)) / log(pow(Broodwar->self()->gatheredGas() + 5, 2) + pow(Broodwar->self()->gatheredMinerals() + 5, 2))); // Would prefer to use unit powers rather than this.

  //Define important states.
	gas_starved = ( ln_gas/ (ln_min + ln_gas) < delta) ;  //If your gas gathering history is disproportionately low, then you are gas starved.
	army_starved = (army_econ_ratio < phi) ;
	
  // Display the game status indicators at the top of the screen
	
  Broodwar->drawTextScreen(10, 0,  "Inventory:"); // working on a better inventory.
  Broodwar->drawTextScreen(10, 10, "Workers: %d", worker_count);
  Broodwar->drawTextScreen(10, 20, "Lings: %d", ling_count);  //
  Broodwar->drawTextScreen(10, 30, "Pool: %d", pool);  // 
  Broodwar->drawTextScreen(10, 40, "Extractor: %d", extractor);  //
  Broodwar->drawTextScreen(10, 50, "Hatches: %d", hatches);  //
  Broodwar->drawTextScreen(10, 60, "Ovis: %d", ovis);  //

  Broodwar->drawTextScreen(100, 0,  "Econ State:");  // What is the econ state?
  Broodwar->drawTextScreen(100, 10, "Gas Starved: %d", gas_starved); 
  Broodwar->drawTextScreen(100, 30, "Econ Heuristic: %f", econ_heuristic);  //
  Broodwar->drawTextScreen(100, 20, "ln(Gas) Proportion: %f", ln_gas / (ln_min + ln_gas) );

  Broodwar->drawTextScreen(250, 0,  "Army State:");  // What is the army state?
  Broodwar->drawTextScreen(250, 10, "Army Starved: %d", army_starved);  // 
  Broodwar->drawTextScreen(250, 20, "Army Heuristic: %f", army_heuristic ); // 
  Broodwar->drawTextScreen(250, 30, "Ling Count: %d", ling_count);  // 
  Broodwar->drawTextScreen(250, 40, "ln(Army) Proportion: %f", army_econ_ratio);  // 

  Broodwar->drawTextScreen(100, 50, "Performance:");  // 
  Broodwar->drawTextScreen(100, 60, "APM: %d", Broodwar->getAPM() );  // 
  Broodwar->drawTextScreen(100, 70, "FPS: %f", Broodwar->getAverageFPS() );  // 
  Broodwar->drawTextScreen(100, 80, "Frames of Latency: %d", Broodwar->getLatencyFrames() );  // 

  //vision state?
  //supply state?

  // Return if the game is a replay or is paused
  if ( Broodwar->isReplay() || Broodwar->isPaused() || !Broodwar->self() )
    return;

  // Prevent spamming by only running our onFrame once every number of latency frames.
  // Latency frames are the number of frames before commands are processed.
  if ( Broodwar->getFrameCount() % Broodwar->getLatencyFrames() != 0 )
    return;

  // Iterate through all the units that we own
  for (auto &u : Broodwar->self()->getUnits())
  {
    // Ignore the unit if it no longer exists
    // Make sure to include this block when handling any Unit pointer!
    if ( !u->exists() )
      continue;

    // Ignore the unit if it has one of the following status ailments
    if ( u->isLockedDown() || u->isMaelstrommed() || u->isStasised() )
      continue;

    // Ignore the unit if it is in one of the following states
    if ( u->isLoaded() || !u->isPowered() || u->isStuck() )
      continue;

    // Ignore the unit if it is incomplete or busy constructing
    if ( !u->isCompleted() || u->isConstructing() )
      continue;

    // Finally make the unit do some stuff!

    // If the unit is a worker unit
    if ( u->getType().isWorker() ) 
    {
		worker_count++;
      // Mining loop if our worker is idle (includes returning $$$) or not moving while gathering gas, we (re-) evaluate what they should be mining.  Original script uses isIdle() only.
		if ( u->isIdle() || !u->isMoving() || u->isCarryingGas() || u->isCarryingMinerals() )
		{
			// Order workers carrying a resource to return them to the center,
			// otherwise find a mineral patch to harvest.
			if (u->isCarryingGas() || u->isCarryingMinerals() ) // Return $$$
			{
				u->returnCargo(true);
			} //Closeure: returning $$ loop
			else if (!u->getPowerUp())  // The worker cannot harvest anything if it
			{                             // is carrying a powerup such as a flag
			// Idle worker then Harvest from the nearest mineral patch or gas refinery, depending on need.
				if ( extractor && gas_starved && gas_workers < 3 ) // If you have an extractor and you need gas then collect some gas. Queue this command, so complete your current task first.
				{
					u->gather(u->getClosestUnit(IsRefinery), true); 
						gas_workers++;
				}
				if ( !extractor || !gas_starved || gas_workers >= 3)
				{
					u->gather(u->getClosestUnit(IsMineralField), true);
						min_workers++;
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
		if ( ( u->isIdle() || ( u->isGatheringMinerals() && !u->isCarryingMinerals() ) ) && (t + 250 < Broodwar->getFrameCount() ) && !builder_exists) 
		{ //only get those that are gathering minerals, but not carrying them. This always irked me. Don't send a builder if you have a builder in this loop.
		  
			  //Gas Loop
			  if ( (Broodwar->self()->minerals() >= UnitTypes::Zerg_Extractor.mineralPrice()) && gas_starved ) 
			  { //if we have enough minerals, or too many minerals... Get an extractor.
				  TilePosition buildPosition = Broodwar->getBuildLocation(UnitTypes::Zerg_Extractor, u->getTilePosition());
				  u->build(UnitTypes::Zerg_Extractor, buildPosition);
				  builder_exists = true;
			  }
			  //Army Loop
			  else if ((Broodwar->self()->minerals() >= UnitTypes::Zerg_Spawning_Pool.mineralPrice()) && army_starved && !pool)
			  { //if we have enough minerals, and are army starved, build a building.
				  TilePosition buildPosition = Broodwar->getBuildLocation(UnitTypes::Zerg_Spawning_Pool, u->getTilePosition());
				  u->build(UnitTypes::Zerg_Spawning_Pool, buildPosition);
				  builder_exists = true;
			  }

			  //Expo Loop
			  else if ( (Broodwar->self()->minerals() >= UnitTypes::Zerg_Hatchery.mineralPrice() * 1.5) )
			  { //if we are floating too much money, get an expo.
				  TilePosition buildPosition = Broodwar->getBuildLocation(UnitTypes::Zerg_Hatchery, u->getTilePosition());
				  u->build(UnitTypes::Zerg_Hatchery, buildPosition);
				  builder_exists = true;
			  }

			  //Vision Loop
			  //Tech Loop
			  int t = Broodwar->getFrameCount();

		  } // Close Build loop

    } // Close Worker management loop

    else if ( u->getType().isResourceDepot() ) // A resource depot is a Command Center, Nexus, or Hatchery
    {
      // Order the depot to construct more workers! But only when it is idle.
		if (u->isIdle() && !(army_starved && pool) && !u->train(u->getType().getRace().getWorker()))
      { // I should make this into a method? Approach? Call this response.

        // If that fails, draw the error at the location so that you can visibly see what went wrong!
        // However, drawing the error once will only appear for a single frame
        // so create an event that keeps it on the screen for some frames
        Position pos = u->getPosition();
        Error lastErr = Broodwar->getLastError();
        Broodwar->registerEvent([pos,lastErr](Game*){ Broodwar->drawTextMap(pos, "%c%s", Text::White, lastErr.c_str()); },   // action
                                nullptr,    // condition
                                Broodwar->getLatencyFrames());  // frames to run

        // Retrieve the supply provider type in the case that we have run out of supplies
        UnitType supplyProviderType = u->getType().getRace().getSupplyProvider();
        static int lastChecked = 0;

        // If we are supply blocked and haven't tried constructing more recently
        if (  lastErr == Errors::Insufficient_Supply &&
              lastChecked + 400 < Broodwar->getFrameCount() &&
              Broodwar->self()->incompleteUnitCount(supplyProviderType) == 0 )
        {
          lastChecked = Broodwar->getFrameCount();

          // Retrieve a unit that is capable of constructing the supply needed
          Unit supplyBuilder = u->getClosestUnit(  GetType == supplyProviderType.whatBuilds().first &&
                                                    (IsIdle || IsGatheringMinerals) &&
                                                    IsOwned);
          // If a unit was found
          if ( supplyBuilder )
          {
            if ( supplyProviderType.isBuilding() )
            {
              TilePosition targetBuildLocation = Broodwar->getBuildLocation(supplyProviderType, supplyBuilder->getTilePosition());
              if ( targetBuildLocation )
              {
                // Register an event that draws the target build location
                Broodwar->registerEvent([targetBuildLocation,supplyProviderType](Game*)
                                        {
                                          Broodwar->drawBoxMap( Position(targetBuildLocation),
                                                                Position(targetBuildLocation + supplyProviderType.tileSize()),
                                                                Colors::Blue);
                                        },
                                        nullptr,  // condition
                                        supplyProviderType.buildTime() + 100 );  // frames to run

                // Order the builder to construct the supply structure
                supplyBuilder->build( supplyProviderType, targetBuildLocation );
              }
            }
            else
            {
              // Train the supply provider (Overlord) if the provider is not a structure
              supplyBuilder->train( supplyProviderType );
            }
          } // closure: supplyBuilder is valid
        } // closure: insufficient supply
      } // closure: failed to train worker unit

	  //Otherwise, we should build army!
	  if (pool && army_starved && u->isIdle() && !u->train(UnitTypes::Zerg_Zergling)) { // I should make this into a method? Approach? Call this response.

																						// If that fails, draw the error at the location so that you can visibly see what went wrong!
																						// However, drawing the error once will only appear for a single frame
																						// so create an event that keeps it on the screen for some frames
		  Position pos = u->getPosition();
		  Error lastErr = Broodwar->getLastError();
		  Broodwar->registerEvent([pos, lastErr](Game*) { Broodwar->drawTextMap(pos, "%c%s", Text::White, lastErr.c_str()); },   // action
			  nullptr,    // condition
			  Broodwar->getLatencyFrames());  // frames to run

											  // Retrieve the supply provider type in the case that we have run out of supplies
		  UnitType supplyProviderType = u->getType().getRace().getSupplyProvider();
		  static int lastChecked = 0;

		  // If we are supply blocked and haven't tried constructing more recently
		  if (lastErr == Errors::Insufficient_Supply &&
			  lastChecked + 400 < Broodwar->getFrameCount() &&
			  Broodwar->self()->incompleteUnitCount(supplyProviderType) == 0)
		  {
			  lastChecked = Broodwar->getFrameCount();

			  // Retrieve a unit that is capable of constructing the supply needed
			  Unit supplyBuilder = u->getClosestUnit(GetType == supplyProviderType.whatBuilds().first &&
				  (IsIdle || IsGatheringMinerals) &&
				  IsOwned);
			  // If a unit was found
			  if (supplyBuilder)
			  {
				  if (supplyProviderType.isBuilding())
				  {
					  TilePosition targetBuildLocation = Broodwar->getBuildLocation(supplyProviderType, supplyBuilder->getTilePosition());
					  if (targetBuildLocation)
					  {
						  // Register an event that draws the target build location
						  Broodwar->registerEvent([targetBuildLocation, supplyProviderType](Game*)
						  {
							  Broodwar->drawBoxMap(Position(targetBuildLocation),
								  Position(targetBuildLocation + supplyProviderType.tileSize()),
								  Colors::Blue);
						  },
							  nullptr,  // condition
							  supplyProviderType.buildTime() + 100);  // frames to run

																	  // Order the builder to construct the supply structure
						  supplyBuilder->build(supplyProviderType, targetBuildLocation);
					  }
				  }
				  else
				  {
					  // Train the supply provider (Overlord) if the provider is not a structure
					  supplyBuilder->train(supplyProviderType);
				  }
			  } // closure: supplyBuilder is valid
		  } // closure: insufficient supply
	  } // closure: failed to train army.
    } // closure: Error announcement for failure to construct more workers.

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

	if ( u->getType() != UnitTypes::Zerg_Drone  && u->getType() != UnitTypes::Zerg_Overlord && u->isIdle() )
	{
		u->attack(u->getClosestUnit(BWAPI::Filter::IsEnemy));
	} // If it is not a drone or an overlord, then use it to attack the enemy.

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
    if ( unit->getType().isBuilding() && !unit->getPlayer()->isNeutral() )
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
