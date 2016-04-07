#include "UnitBehavior.h"

using namespace BWAPI;
using namespace Filter;

bool build(UnitType structure, Unit worker);
bool train(Unit structure, UnitType type);

#pragma region WorkerLogic

//used to ensure that we don't have multiple workers trying to queue the same goal or depot
static int lastFrameOnWhichStructureEnqueued = 0;

/* A goal is a structure other than a supply depot or a refinery which we wish to
produce when we have the resources available. Goals collectively form a queue from which
elements are removed as they're fulfilled. */
static std::deque<Goal> goals;
static std::list<Goal> goalsUnderConstruction;

/*TODO: if harvesting minerals, and there are more than (mineral field count * 2) workers here, and there is another townhall with less than
that amount, then transfer this worker to go mine near the other townhall*/

///<summary>Issues the highest priority order that can be found for the target worker.</summary>
bool evaluateWorkerLogicFor(BWAPI::Unit worker, int requiredSupplyDepots, int workerCount, resourceProjection unallocatedResources) {
	/* Priorities, from highest to lowest
	-Repair damaged structures and units
	-Construct a supply depot if needed
	-Construct a building from the goals list
	-Transfer to another base
	-Build a refinery if needed (handled in townhall logic)
	-Harvest gas if possible (handled in refinery logic)
	-Harvest minerals
	*/

	if (!worker->getType().isWorker()) {
		Broodwar << "Warning: attempted to evaluate worker logic for a non-worker unit";
		return false;
	}

	//check whether we should build a supply depot
	if (requiredSupplyDepots > 0) {
		//check whether we've already queued a building on this frame
		if (Broodwar->getFrameCount() > lastFrameOnWhichStructureEnqueued + Broodwar->getLatencyFrames()) {

			//check that the worker is able to build a depot
			if (worker->isIdle() || worker->isGatheringMinerals() || worker->isGatheringGas()) {

				//get the unit type of supply depot
				UnitType supplyProviderType = worker->getType().getRace().getSupplyProvider();

				if (unallocatedResources.minerals >= supplyProviderType.mineralPrice()) {
					//order the worker to build the supply depot
					if (build(supplyProviderType, worker))
						return true;
				} //if resources are sufficient
			} //if idle or harvesting
		} //if haven't queued a building this frame
	} //if supply depots required

#pragma region GoalLogic

	//check if any goals under construction need attention
	std::list<Goal>::iterator i = goalsUnderConstruction.begin();
	while (i != goalsUnderConstruction.end()) {

		//0: do nothing; 1: find an SCV and assign to build; 2: re-add to front of goal queue
		int resolution = 0;

		Goal &g = *i;

		//first, check whether the building is finished - if so, we can safely remove this goal from the queue.
		if (g.structure && g.structure->isCompleted()) {
			i = goalsUnderConstruction.erase(i);
			continue;
		}

		if (Broodwar->getFrameCount() < g.gracePeriod) {
			i++;
			continue; //we haven't exceeded our grace period, hold on a couple seconds
		}
		//assignee exists
		if (g.assignee && g.assignee->exists()) {
			//assignee is constructing
			if (g.assignee->isConstructing()) {
				if (g.assignee->getBuildUnit()) {
					if (!g.structure) //set the physical structure of the goal if it isn't already set
						g.structure = g.assignee->getBuildUnit();
					//nothing is amiss - carry on
					resolution = 0;
				}
			}
			else { //assignee exists, but isn't constructing
				if (g.structure && g.structure->exists()) {
					//structure exists
					//assign an SCV to finish construction
					resolution = 1;
				}
				else {
					//structure does not exist
					//re-add to goal queue
					resolution = 2;
				}
			}
		} //assignee does not exist
		else {
			if (g.structure && g.structure->exists()) { //structure exists
				if (g.structure->getBuildUnit()) {
					g.assignee = g.structure->getBuildUnit();
					//somehow our assignee was lost, but an SCV is constructing this building, so there's no problem
					resolution = 0;
				}
				else {
					//structure exists but is not being built
					//assign the current worker to finish it
					resolution = 1;
				}
			}
			else { //both assignee and structure do not exist
				resolution = 2;
			} //neither assignee nor structure exist
		} //assignee does not exist

		if (resolution == 0) { //do nothing
			i++;
			continue;
		}
		else if (resolution == 1) { //construction stopped; assign new SCV to finish construction
			worker->rightClick(g.structure);
			g.assignee = worker;
			i++;
			continue;
		}
		else if (resolution == 2) { //complete failure; re-add goal to goal queue
			g.assignee = nullptr;
			g.structure = nullptr;
			goals.push_front(g);
			i = goalsUnderConstruction.erase(i);
			continue;
		}
	} //goal under construction iterator

	//attempt to build the structure at the front of our goal list
	//check that we haven't queued a building very recently in order to give time for the other SCV to respond to the order
	if (Broodwar->getFrameCount() > lastFrameOnWhichStructureEnqueued + Broodwar->getLatencyFrames() + 24) {

		Goal goal;
		//check that there's a goal to work with
		if (!goals.empty()) {
			goal = goals.front();
			//check whether we have the resources to build the frontmost member of the list
			if (unallocatedResources.minerals >= goal.structureType.mineralPrice() && unallocatedResources.gas >= goal.structureType.gasPrice()) {
				//check that the worker is able to build a structure
				if (worker->isIdle() || worker->isGatheringMinerals() || worker->isGatheringGas()) {
					//issue the build order
					if (build(goal.structureType, worker)) {
						goal.assignee = worker;
						goal.gracePeriod = Broodwar->getFrameCount() + 48;
						goalsUnderConstruction.push_back(goal);
						goals.pop_front();
						return true;
					}
				}
			} //if resources available
		} //if any goals exist
	} //if haven't queued a building this frame

#pragma endregion

	//attempt to harvest minerals
	if (worker->isIdle()) {
		//check whether it's carrying resources
		if (worker->isCarryingGas() || worker->isCarryingMinerals())
		{
			//if so, return those resources
			worker->returnCargo();
		}

		//if we're not carrying a powerup (which would prevent us harvesting resources)
		else if (!worker->getPowerUp()) {
			//start harvesting from the nearest mineral patch
			if (worker->gather((worker->getClosestUnit(Filter::IsMineralField))))
				return true;
			else {
				//if we can't do that, print the reason
				Broodwar << "Worker cannot mine minerals: " << Broodwar->getLastError() << std::endl;
			}
		} //if has no powerup
	} // if idle

	//couldn't find any useful task which this worker could carry out
	return false;
}

///<summary>Adds a structure to the goal list so that it will be constructed when
///resources are available and all goals added previously have been removed from
///the goal list.</summary>
bool addGoal(BWAPI::UnitType structure) {
	Goal newGoal;
	//if it's from a different race or isn't a structure
	if (structure.getRace() != Broodwar->self()->getRace() || !structure.isBuilding())
		return false; //we can't make it, don't add it as a goal
	newGoal.structureType = structure;
	newGoal.structure = nullptr;
	newGoal.assignee = nullptr;
	newGoal.gracePeriod = 0;
	goals.push_back(newGoal);
	return true;
}

///<summary>Issues an order to the specified worker to build the specified structure type.</summary>
bool build(UnitType structure, Unit worker) {
	static BuildingPlacer placer;
	//can't build it if it's not a building or if it's from a different race
	if (!structure.isBuilding() || structure.getRace() != Broodwar->self()->getRace())
		return false;
	TilePosition targetBuildLocation = placer.getBuildLocationNear(worker->getTilePosition(), structure);
	if (targetBuildLocation) {
		if (worker->build(structure, targetBuildLocation)) {
			lastFrameOnWhichStructureEnqueued = Broodwar->getFrameCount();

			//register an event that draws the target build location for a few seconds the build order succeeds
			Broodwar->registerEvent([targetBuildLocation, structure](Game*)
			{
				Broodwar->drawBoxMap(Position(targetBuildLocation),
					Position(targetBuildLocation + structure.tileSize()),
					Colors::Blue);
			},
				nullptr,								//condition
				100);	//duration in frames
			return true;
		}
		else {
			//if the order fails, draw a message over the worker with the reason
			Position pos = worker->getPosition();
			Error lastErr = Broodwar->getLastError();
			Broodwar->registerEvent([pos, lastErr](Game*){
				Broodwar->drawTextMap(pos, "%c%s%s", Text::White, "Failed to build structure: ", lastErr.c_str());
			},
				nullptr,						//condition
				Broodwar->getLatencyFrames());	//duration in frames
		}
	}
	return false;
}

#pragma endregion

#pragma region TownhallLogic

///<summary>Trains a worker from the target townhall if possible and desirable.</summary>
bool evaluateTownhallLogicFor(BWAPI::Unit townhall, int workerCount) {
	if (!townhall->getType().isResourceDepot()) {
		Broodwar << "Warning: attempted to evaluate townhall logic for a non-townhall unit";
		return false;
	}

	//check whether we should build a refinery
	static int gracePeriod = 0;
	if (Broodwar->getFrameCount() > gracePeriod && workerCount > WORKERS_REQUIRED_BEFORE_MINING_GAS) {
		Unit closestGeyser = townhall->getClosestUnit(Filter::GetType == UnitTypes::Resource_Vespene_Geyser);
		Unit closestRefinery = townhall->getClosestUnit(Filter::IsRefinery);

		if (closestGeyser) { //if a geyser exists
			//if the closest geyser is closer than the closest refinery (or no refinery exists), 
			//we have an open geyser in proximity and should build on it
			if (!closestRefinery || townhall->getDistance(closestRefinery) > townhall->getDistance(closestGeyser)) {
				//pick a worker and issue the build order
				for (auto &worker : Broodwar->self()->getUnits()) {
					if (worker->getType().isWorker() && !unitIsDisabled(worker)) {
						gracePeriod = Broodwar->getFrameCount() + 120; //give it 5 seconds to start construction before we try again
						worker->build(UnitTypes::Terran_Refinery, closestGeyser->getTilePosition());
						break;
					} //unit is worker and is not disabled
				} //unit iterator
			} //open geyser in proximity
		} //geyser exists
	} //should build geyser

	// if we have fewer than 60 workers, train more
	if (workerCount < MAXIMUM_WORKER_COUNT && townhall->isIdle() && !townhall->train(townhall->getType().getRace().getWorker()))
	{
		/* For debugging purposes, if we're unable to train a unit for whatever reason, register an event
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

#pragma endregion TownhallLogic

#pragma region RefineryLogic

bool evaluateRefineryLogicFor(BWAPI::Unit refinery, int workerCount) {
	if (!refinery->getType().isRefinery()) {
		Broodwar << "Warning: attempted to evaluate refinery logic for a non-refinery unit";
		return false;
	}

	if (!refinery->isCompleted()) {
		return false;
	}

	static std::list<Refinery> refineries;
	auto iter = std::find_if(refineries.begin(), refineries.end(), [refinery](Refinery ref){ return ref.refinery == refinery; });

	//if refinery does not already exist in list
	if (iter == std::end(refineries)) {
		//add it to the list
		Refinery r;
		r.gracePeriod = Broodwar->getFrameCount();
		r.refinery = refinery;
		refineries.push_front(r);
	}

	Refinery& ref = (iter == std::end(refineries) ? refineries.front() : *iter);

	//wait a bit for workers' orders to settle before reevaluating
	if (ref.gracePeriod > Broodwar->getFrameCount())
		return true;

	//iterate through workers assigned to this refinery
	std::list<Unit>::iterator i = ref.workers.begin();
	while (i != ref.workers.end()) {
		Unit w = (*i);
		//if they're not mining from the refinery, remove them from the list so other workers can be assigned
		if (!w->isGatheringGas() || (w->getOrder() == Orders::HarvestGas && w->getOrderTarget() != refinery))
			i = ref.workers.erase(i);
		else
			i++;
	}

	//get workers in a radius around this refinery
	for (auto &worker : refinery->getUnitsInRadius(TILE_SIZE * 64, Filter::IsWorker)) {

		//a worker is already mining from this refinery
		if (worker->isGatheringGas() && worker->getOrderTarget() == refinery) {
			//if it isn't on our list of workers
			if (std::find(std::begin(ref.workers), std::end(ref.workers), worker) == std::end(ref.workers))
				worker->stop(); //then stop doing that
		}

		//if we have less than three workers mining here
		if (ref.workers.size() < 3) {
			//if our worker is idle or harvesting minerals and we have enough workers to justify mining gas
			if ((worker->isIdle() || worker->isGatheringMinerals()) && workerCount > WORKERS_REQUIRED_BEFORE_MINING_GAS)
			{
				//order the worker to harvest gas
				worker->gather(refinery);
				//add the worker to the list of workers mining from this refinery
				ref.workers.push_front(worker);
			} //worker is able to mine gas and we have enough workers to justify doing so
		} //workers mining gas less than three
	} //workers in radius iterator

	/* This grace period serves to give time for workers to adjust their orders and to
	avoid ordering three workers to the gas simultaneously, which is suboptimal. */
	ref.gracePeriod = Broodwar->getFrameCount() + 24;
	return true;
}

#pragma endregion

#pragma region BarracksLogic

bool evaluateBarracksLogicFor(BWAPI::Unit barracks) {
	if (barracks->isIdle())
		return train(barracks, UnitTypes::Terran_Marine);
	return true;
}

#pragma endregion

bool unitIsDisabled(Unit unit) {

	if (!unit->exists() ||
		unit->isLockedDown() ||
		unit->isMaelstrommed() ||
		unit->isStasised() ||
		unit->isLoaded() ||
		!unit->isPowered() ||
		unit->isStuck() ||
		!unit->isCompleted() ||
		unit->isConstructing()
		)
		return true;

	return false;
}

std::deque<Goal> getGoals() {
	return goals;
}

bool train(Unit structure, UnitType type) {
	if (!structure->getType().isBuilding())
		return false;
	if (structure->train(type))
		return true;
	else {
		/* For debugging purposes, if we're unable to train a unit for whatever reason, register an event
		to draw the error over the townhall until the next evaluation*/
		Position pos;
		//draw across the top of the townhall rather than from the center
		pos.x = structure->getLeft() - 25;
		pos.y = structure->getTop() - 25;

		Error lastErr = Broodwar->getLastError();
		Broodwar->registerEvent([pos, lastErr](Game*){ Broodwar->drawTextMap(pos, "%c%s%s", Text::White, "Cannot train: ", lastErr.c_str()); },   // action
			nullptr,    // condition
			Broodwar->getLatencyFrames());  // frames to run
		return false;
	}
}