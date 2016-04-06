#include "UnitBehavior.h"

using namespace BWAPI;
using namespace Filter;

static BuildingPlacer placer;

bool evaluateWorkerLogicFor(BWAPI::Unit worker, int requiredSupplyDepots) {
	//so we don't have multiple workers trying to queue a structure on the same frame
	static int lastFrameOnWhichSupplyDepotEnqueued = 0;

	if (!worker->getType().isWorker()) {
		Broodwar << "Warning: attempted to evaluate worker logic for a non-worker unit";
		return false;
	}

	// if our worker is idle
	if (worker->isIdle())
	{
		// check whether it's carrying resources
		if (worker->isCarryingGas() || worker->isCarryingMinerals())
		{
			//if so, return those resources
			worker->returnCargo();
		}

		//if we're not carrying a powerup (which would prevent us harvesting resources)
		else if (!worker->getPowerUp()) {
			// start harvesting from the nearest mineral patch
			if (!worker->gather((worker->getClosestUnit(Filter::IsMineralField))))
			{
				// If the call fails, then print the last error message
				Broodwar << Broodwar->getLastError() << std::endl;
			}
		} //if has no powerup
	} // if idle

	//TODO: if harvesting gas, make sure there are no more than three workers on gas
	/*TODO: if harvesting minerals, and more than WORKERS_REQUIRED_BEFORE_MINING_GAS workers are present mining minerals, make sure that there are
	at least three workers mining gas */
	/*TODO: if harvesting minerals, and there are more than (mineral field count * 2) workers here, and there is another townhall with less than
	that amount, then transfer this worker to go mine near the other townhall*/
	/*TODO: if idle or harvesting, and our projected supply usage is in excess of our current supply, and there are fewer supply depots
	enqueued or under construction than are required to satisfy our projected supply usage, then build a supply depot*/
	//TODO: if idle or harvesting, and we have the resources available to take a goal off of our list, then do so

	// check whether we should build a supply depot
	if (requiredSupplyDepots > 0 && Broodwar->getFrameCount() > lastFrameOnWhichSupplyDepotEnqueued + Broodwar->getLatencyFrames()) {

		//check that the worker is able to build a depot
		if (worker->isIdle() || worker->isGatheringMinerals() || worker->isGatheringGas()) {
			//get the unit type of supply depot
			
			UnitType supplyProviderType = worker->getType().getRace().getSupplyProvider();
			if (Broodwar->self()->minerals() > supplyProviderType.mineralPrice()) {
				TilePosition targetBuildLocation = placer.getBuildLocationNear(worker->getTilePosition(), supplyProviderType);
				if (targetBuildLocation)
				{
					// order the worker to build the supply depot
					if (worker->build(supplyProviderType, targetBuildLocation)) {
						lastFrameOnWhichSupplyDepotEnqueued = Broodwar->getFrameCount();
						// register an event that draws the target build location if the build order succeeds
						Broodwar->registerEvent([targetBuildLocation, supplyProviderType](Game*)
						{
							Broodwar->drawBoxMap(Position(targetBuildLocation),
								Position(targetBuildLocation + supplyProviderType.tileSize()),
								Colors::Blue);
						},
							nullptr,  // condition
							supplyProviderType.buildTime() + 100);  // frames to run
					}
					else {
						Position pos = worker->getPosition();
						Error lastErr = Broodwar->getLastError();
						Broodwar->registerEvent([pos, lastErr](Game*){ Broodwar->drawTextMap(pos, "%c%s%s", Text::White, "Cannot build: ", lastErr.c_str()); },   // action
							nullptr,    // condition
							Broodwar->getLatencyFrames());  // frames to run
					} //if unable to build
				} //if build location valid
			} //if resources are sufficient
		} //if idle or harvesting
	} //if supply depots required


	return true;
}

bool evaluateTownhallLogicFor(BWAPI::Unit townhall, int workerCount) {
	if (!townhall->getType().isResourceDepot()) {
		Broodwar << "Warning: attempted to evaluate townhall logic for a non-townhall unit";
		return false;
	}

	// if we have fewer than 60 workers, train more
	if (workerCount < 60 && townhall->isIdle() && !townhall->train(townhall->getType().getRace().getWorker()))
	{
		/* For debugging purposes, if we're unable to train a worker for whatever reason, register an event
		to draw the error over the townhall until the next evaluation*/
		Position pos;
		//draw across the top of the townhall rather than from the center
		pos.x = townhall->getLeft() - 25;
		pos.y = townhall->getTop() - 25;

		Error lastErr = Broodwar->getLastError();
		Broodwar->registerEvent([pos, lastErr](Game*){ Broodwar->drawTextMap(pos, "%c%s%s", Text::White, "Cannot train: ", lastErr.c_str()); },   // action
			nullptr,    // condition
			Broodwar->getLatencyFrames());  // frames to run
	}

	return true;
}