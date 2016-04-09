#include <iostream>

#include "TerranAIModule.h"

using namespace BWAPI;
using namespace Filter;
using namespace UnitBehavior;
using namespace ResourceLogic;
using namespace MilitaryManager;

void TerranAIModule::onStart()
{
	//print the map name
	Broodwar << "The map is " << Broodwar->mapName() << "!" << std::endl;

	//enable the UserInput flag, which allows us to control the bot and type messages
	Broodwar->enableFlag(Flag::UserInput);

	Broodwar->setCommandOptimizationLevel(2);

	// Check if this is a replay
	if (Broodwar->isReplay())
	{
		return;
	}
	else // if this is not a replay
	{
		//against any race, we'll get tank production going as fast as possible
		addGoal(TechTypes::Tank_Siege_Mode);
		//we'll wait for scouting info before making any further decisions

		//have our combat units rally around the command center to start
		//find the command center
		Unit townhall = nullptr;
		for (auto &u : Broodwar->self()->getUnits()) {
			if (u->getType().isResourceDepot())
				townhall = u;
		}
		if (townhall)
			setRallyPoint(townhall->getPosition());
		setTactic(MilitaryManager::Tactic::DEFEND);
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
	if (!Broodwar->enemy())
		return; //we have nothing to do without an enemy

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
	validateUnits();
	moveToRally();
	evaluatePreparedness();
	executeTactic();
	evaluateStrategy();

	int enqueuedSupplyDepots = 0;
	int requiredSupplyDepots = 0;
	int workerCount = 0;

	//iterate through all the units that we own for the sake of gathering data about them
	for (auto &u : Broodwar->self()->getUnits()) {
		//ignore the unit if it no longer exists
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
		if (!u->exists())
			continue;
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
			//only build medics and firebats against Zerg - useless against Terran
			//and niche at best against Protoss
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
	//check if the target is a valid position
	if (target)
	{
		//print the location of the nuclear strike target
		Broodwar << "Nuclear Launch Detected at " << target << std::endl;
	}
}

//Called when the Unit interface object representing the unit that has just become accessible.
void TerranAIModule::onUnitDiscover(BWAPI::Unit unit)
{
	//if we own this unit
	if (unit->getPlayer() == Broodwar->self()) {
		//and it's a military unit
		if (!unit->getType().isWorker() && !unit->getType().isBuilding()) {
			//add it to our list of military units
			addToArmy(unit);
		}
	}

	//if it's an enemy unit, index it
	if (unit->getPlayer()->isEnemy(Broodwar->self()))
		indexEnemyUnit(unit);
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
