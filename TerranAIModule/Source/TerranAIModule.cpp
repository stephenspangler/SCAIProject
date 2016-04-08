#include <iostream>

#include "TerranAIModule.h"

using namespace BWAPI;
using namespace Filter;
using namespace UnitBehavior;
using namespace ResourceLogic;

void TerranAIModule::onStart()
{
	// Print the map name.
	// BWAPI returns std::string when retrieving a string, don't forget to add .c_str() when printing!
	Broodwar << "The map is " << Broodwar->mapName() << "!" << std::endl;

	// Enable the UserInput flag, which allows us to control the bot and type messages.
	Broodwar->enableFlag(Flag::UserInput);

	// Set the command optimization level so that common commands can be grouped
	// and reduce the bot's APM (Actions Per Minute).
	Broodwar->setCommandOptimizationLevel(2);

	// Check if this is a replay
	if (Broodwar->isReplay())
	{

		// Announce the players in the replay
		Broodwar << "The following players are in this replay:" << std::endl;

		// Iterate all the players in the game using a std:: iterator
		Playerset players = Broodwar->getPlayers();
		for (auto p : players)
		{
			// Only print the player if they are not an observer
			if (!p->isObserver())
				Broodwar << p->getName() << ", playing as " << p->getRace() << std::endl;
		}

	}
	else // if this is not a replay
	{
		// Retrieve you and your enemy's races. enemy() will just return the first enemy.
		// If you wish to deal with multiple enemies then you must use enemies().
		if (Broodwar->enemy()) { // First make sure there is an enemy
			std::string race = (Broodwar->enemy()->getRace().getName() == "Unknown" ? "Random" : Broodwar->enemy()->getRace().getName());
			Broodwar << "The matchup is " << Broodwar->self()->getRace() << " vs " << race << std::endl;
		}

		//Add a basic build order
			addGoal(UnitTypes::Terran_Barracks);
			addGoal(UnitTypes::Terran_Factory);
			addGoal(UnitTypes::Terran_Academy);
			addGoal(UnitTypes::Terran_Barracks);
			addGoal(TechTypes::Stim_Packs);
			addGoal(TechTypes::Tank_Siege_Mode);

		//Have our combat units rally around the command center to start
		
	}
}

void TerranAIModule::onEnd(bool isWinner)
{
	// Called when the game ends
	if (isWinner)
	{
		// Log your win here!
	}
}

void TerranAIModule::onFrame()
{

	if (Broodwar->self()->getRace().getName() != "Terran")
		return; //we don't know how to handle any race other than terran

	// Display the game frame rate as text in the upper left area of the screen
	Broodwar->drawTextScreen(300, 0, "FPS: %d", Broodwar->getFPS());
	Broodwar->drawTextScreen(300, 20, "Average FPS: %f", Broodwar->getAverageFPS());

	// Return if the game is a replay or is paused
	if (Broodwar->isReplay() || Broodwar->isPaused() || !Broodwar->self())
		return;

	/* Prevent spamming by only running onFrame once every number of latency frames.
	Latency frames are the number of frames before commands are processed. This has
	the added benefit of not accidentally issuing unnecessary orders as a result of
	re-evaluating logic before the result of the previous orders has been processed. */
	if (Broodwar->getFrameCount() % Broodwar->getLatencyFrames() != 0)
		return;

	evaluateGoals();

	int enqueuedSupplyDepots = 0;
	int requiredSupplyDepots = 0;
	int workerCount = 0;

	// iterate through all the units that we own for the sake of gathering data about them
	for (auto &u : Broodwar->self()->getUnits()) {
		// ignore the unit if it no longer exists
		if (!u->exists())
			continue;

		//count the number of supply depots enqueued or under construction
		//if the unit is a worker 
		if (u->getType().isWorker()) {
			workerCount++; //(increment worker count while we're at it)
			//and its current order is to construct a supply depot
			if (u->getBuildType().supplyProvided() > 0) {
				//plus one
				enqueuedSupplyDepots++;
			}
		}
	}

	requiredSupplyDepots = getRequiredSupplyDepots(enqueuedSupplyDepots);
	calcUnallocatedResources();
	resourceProjection unallocatedResources = getUnallocatedResources();

	//draw information to screen until the next time it's evaluated
	Broodwar->registerEvent([unallocatedResources](Game*) {
		int ypos = 20;
		Broodwar->drawTextScreen(20, 0, "Goals:");
		for (Goal &g : getGoals()) {
			Broodwar->drawTextScreen(20, ypos, "%s", g.isResearch ? g.tech.getName().c_str() : g.structureType.getName().c_str());
			ypos += 20;
		}
		ypos += 20;
		Broodwar->drawTextScreen(20, ypos, "Unallocated minerals: %d", unallocatedResources.minerals);
		ypos += 20;
		Broodwar->drawTextScreen(20, ypos, "Unallocated gas: %d", unallocatedResources.gas);
	},
		nullptr,    // condition
		Broodwar->getLatencyFrames());  // frames to run

	// iterate through all the units that we own for the sake of issuing orders to them
	for (auto &u : Broodwar->self()->getUnits())
	{
		if (Helpers::unitIsDisabled(u))
			continue;

		//if the unit is a worker
		if (u->getType().isWorker())
		{
			evaluateWorkerLogicFor(u, requiredSupplyDepots, workerCount);
		}

		//if the unit is a townhall
		if (u->getType().isResourceDepot()) {
			evaluateTownhallLogicFor(u, workerCount);
		}

		//if the unit is a refinery
		if (u->getType().isRefinery()) {
			evaluateRefineryLogicFor(u, workerCount);
		}

		if (u->getType() == UnitTypes::Terran_Barracks) {
			//only build medics and firebats against Zerg - useless against Terran and niche at best against Protoss
			bool enemyIsZerg = (Broodwar->enemy()->getRace().getName() == "Zerg");
			evaluateBarracksLogicFor(u, enemyIsZerg, enemyIsZerg);
		}

		if (u->getType() == UnitTypes::Terran_Factory) {
			evaluateFactoryLogicFor(u);
		}

		evaluateAbilityLogicFor(u);
	}
}

void TerranAIModule::onSendText(std::string text)
{
	//send the text as is
	Broodwar->sendText("%s", text.c_str());
}

void TerranAIModule::onReceiveText(BWAPI::Player player, std::string text)
{
}

void TerranAIModule::onPlayerLeft(BWAPI::Player player)
{
}

void TerranAIModule::onNukeDetect(BWAPI::Position target)
{

	// Check if the target is a valid position
	if (target)
	{
		// if so, print the location of the nuclear strike target
		Broodwar << "Nuclear Launch Detected at " << target << std::endl;
	}
	else
	{
	}

	// You can also retrieve all the nuclear missile targets using Broodwar->getNukeDots()!
}

//Called when the Unit interface object representing the unit that has just become accessible.
void TerranAIModule::onUnitDiscover(BWAPI::Unit unit)
{
}

//Called when the Unit interface object representing the unit that has just become inaccessible.
void TerranAIModule::onUnitEvade(BWAPI::Unit unit)
{
}

//Called when a previously invisible unit becomes visible.
void TerranAIModule::onUnitShow(BWAPI::Unit unit)
{
}

//Called just as a visible unit is becoming invisible.
void TerranAIModule::onUnitHide(BWAPI::Unit unit)
{
}

void TerranAIModule::onUnitCreate(BWAPI::Unit unit)
{
	if (Broodwar->isReplay())
	{
		// if we are in a replay, then we will print out the build order of the structures
		if (unit->getType().isBuilding() && !unit->getPlayer()->isNeutral())
		{
			int seconds = Broodwar->getFrameCount() / 24;
			int minutes = seconds / 60;
			seconds %= 60;
			Broodwar->sendText("%.2d:%.2d: %s creates a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str());
		}
	}
}

void TerranAIModule::onUnitDestroy(BWAPI::Unit unit)
{
}

void TerranAIModule::onUnitMorph(BWAPI::Unit unit)
{
	if (Broodwar->isReplay())
	{
		// if we are in a replay, then we will print out the build order of the structures
		if (unit->getType().isBuilding() && !unit->getPlayer()->isNeutral())
		{
			int seconds = Broodwar->getFrameCount() / 24;
			int minutes = seconds / 60;
			seconds %= 60;
			Broodwar->sendText("%.2d:%.2d: %s morphs a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str());
		}
	}
}

// Called when a unit changes ownership. In a normal game, occurs only as a result of the Protoss Dark Archon's ability "Mind Control."
void TerranAIModule::onUnitRenegade(BWAPI::Unit unit)
{
}

void TerranAIModule::onSaveGame(std::string gameName)
{
	Broodwar << "The game was saved to \"" << gameName << "\"" << std::endl;
}

void TerranAIModule::onUnitComplete(BWAPI::Unit unit)
{
}
