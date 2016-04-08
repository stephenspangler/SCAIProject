#include "UnitBehavior.h"

using namespace BWAPI;
using namespace Filter;
using namespace UnitBehavior;

bool build(UnitType structure, Unit worker);
bool train(Unit structure, UnitType type);
bool research(Unit structure, TechType type);
bool workerIsAvailable(Unit worker);

Unit scout = nullptr;
bool exploredAllStartLocs = false;
bool foundOpponent = false;

#pragma region WorkerLogic

//used to ensure that we don't have multiple workers trying to queue the same goal or depot
static int lastFrameOnWhichStructureEnqueued = 0;

static std::deque<Goal> goals;
static std::list<Goal> goalsUnderConstruction;

///<summary>Checks that we're able to build/research the current goal, or that we will 
///be once currently queued structures are completed. If not, pushes new goals in front
///of the current goal as needed to satisfy its requirements. If the goal is a tech,
///tries to find an appropriate building to research it.</summary>
void UnitBehavior::evaluateGoals() {
	if (!goals.empty()) {
		Goal& g = goals.front();
		if (g.isResearch) {
			UnitType req = g.tech.whatResearches();
			bool foundResearchBuilding = false;
			bool foundRequirement = false;
			for (auto &u : Broodwar->self()->getUnits()) {
				if (u->exists()) {
					if (req == u->getType() || u->getBuildType() == req)
						foundResearchBuilding = true;
					if (g.tech.requiredUnit() == u->getType() || 
						u->getBuildType() == g.tech.requiredUnit())
						foundRequirement = true;
					if (g.tech.whatResearches() == u->getType() && u->isIdle() && u->isCompleted()) {
						if (research(u, g.tech))
							goals.pop_front();
					}
				} //if unit exists
			} //unit iterator
			//search through goals under construction to see if it's scheduled to be queued
			for (auto &goal : goalsUnderConstruction) {
				if (goal.structureType == g.tech.whatResearches())
					foundResearchBuilding = true;
				if (goal.structureType == g.tech.requiredUnit())
					foundRequirement = true;
			}
			//if we haven't built and aren't attempting to build the building that researches this tech, 
			//put it at the front of the goals queue
			if (!foundResearchBuilding && !g.tech.whatResearches().isAddon()) {
				addGoal(g.tech.whatResearches(), true);
			}
			//likewise - if we're missing a prereq, add it ahead of this goal
			if (!foundRequirement && !g.tech.requiredUnit().isAddon() && g.tech.requiredUnit() != g.tech.whatResearches()) {
				addGoal(g.tech.requiredUnit(), true);
			}
		} //if is tech
	} //goal exists
}

///<summary>Issues the highest priority order that can be found for the target worker.</summary>
bool UnitBehavior::evaluateWorkerLogicFor(BWAPI::Unit worker, int requiredSupplyDepots, int workerCount) {
	/* Priorities, from highest to lowest
	-Scout if required
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

	//if we haven't explored all start locations or found our opponent and our worker count is 10 or more
	if (!exploredAllStartLocs && !foundOpponent && workerCount >= 10) {
		//if we don't have a scout
		if (!scout) {
			//and this worker isn't doing anything crucial
			if (workerIsAvailable(worker))
				scout = worker; //congrats, you're our new scout!
		}
		else if (scout == worker) { //if we ARE the scout, then move to unexplored start locations
			bool isMovingToUnexploredStartLoc = false;
			bool unexploredStartLocExists = false;
			Point<int, 1> unexploredStartLocCoords;
			for (auto &startLoc : Broodwar->getStartLocations()) {
				if (!Broodwar->isExplored(startLoc)) {
					unexploredStartLocExists = true;
					Point<int, 1> startLocCoords;
					startLocCoords.x = startLoc.x * TILE_SIZE;
					startLocCoords.y = startLoc.y * TILE_SIZE;
					if (worker->getOrderTargetPosition().getDistance(startLocCoords) < 8 * TILE_SIZE)
						isMovingToUnexploredStartLoc = true;
					else
						unexploredStartLocCoords = startLocCoords;
				}
			}
			if (!unexploredStartLocExists) { //no unexplored start location exists
				exploredAllStartLocs = true;
				//scout's job is done, send him home
				scout->move(scout->getClosestUnit(Filter::IsOwned && Filter::IsResourceDepot)->getPosition());
				scout = nullptr;
			}
			else {
				for (auto &u : Broodwar->getAllUnits()) {
					if (u->exists())
						if (u->getPlayer()->isEnemy(Broodwar->self()))
							if (u->getType().isResourceDepot())
								foundOpponent = true;
				}
				if (foundOpponent) {
					scout->move(scout->getClosestUnit(Filter::IsOwned && Filter::IsResourceDepot)->getPosition());
					scout = nullptr;
				}
				else
					if (!isMovingToUnexploredStartLoc)
						scout->move(unexploredStartLocCoords);
			} //there is/are unexplored start location(s)
		} //this unit are the scout
	} //there is still a reason to scout

	if (scout == worker)
		return true;

	if (worker->isAttacking()) { //if we're already attacking, keep going
		return true;
	}

	//check whether we should build a supply depot
	if (requiredSupplyDepots > 0) {
		//check whether we've already queued a building on this frame
		if (Broodwar->getFrameCount() > lastFrameOnWhichStructureEnqueued + Broodwar->getLatencyFrames()) {

			//check that the worker is able to build a depot
			if (workerIsAvailable(worker)) {

				//get the unit type of supply depot
				UnitType supplyProviderType = worker->getType().getRace().getSupplyProvider();

				//order the worker to build the supply depot
				if (build(supplyProviderType, worker))
					return true;
			} //if idle or harvesting
		} //if haven't queued a building this frame
	} //if supply depots required

#pragma region GoalLogic

	//check if any goals under construction need attention
	std::list<Goal>::iterator i = goalsUnderConstruction.begin();
	while (i != goalsUnderConstruction.end()) {


		Goal &g = *i;
		//0: do nothing; 1: find an SCV and assign to build; 2: re-add to front of goal queue
		int resolution = 0;

		if (g.isResearch)
			continue; //I'm an SCV, not a scientist. Not my problem.

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
			addGoal(g, true);
			i = goalsUnderConstruction.erase(i);
			continue;
		}
	} //goal under construction iterator

	//attempt to build the structure at the front of our goal list
	//check that we haven't queued a building very recently in order to give time for the other SCV to respond to the order
	if (Broodwar->getFrameCount() > lastFrameOnWhichStructureEnqueued + Broodwar->getLatencyFrames() + 100) {

		Goal goal;
		//check that there's a goal to work with
		if (!goals.empty()) {
			goal = goals.front();
			//check that the current goal is a structure
			if (!goal.isResearch) {
				//check that the worker is able to build a structure
				if (workerIsAvailable(worker)) {
					//check that we meet the prerequisites to build the structure
					bool techAvailable = true; //assume tech is available
					for (auto &t : goal.structureType.requiredUnits()) {
						bool foundTech = false;
						//tech isn't immediately available
						if (!Broodwar->self()->hasUnitTypeRequirement(t.first, t.second)) {
							techAvailable = false;
							//search through units to see if the tech requirement exists but is under construction
							for (auto &u : Broodwar->self()->getUnits()) {
								if (u->exists() && u->getType() == t.first || u->getBuildType() == t.first)
									foundTech = true;
							} //unit iterator
							//also search through goals under construction to see if it's scheduled to be queued
							for (auto &goal : goalsUnderConstruction) {
								if (goal.structureType == t.first)
									foundTech = true;
							}
							//if we don't have the tech available and we're not trying to build it
							if (!techAvailable && !foundTech) {
								addGoal(t.first, true); //add it to the front of our goals list
							}
						} //player has tech requirement
					} //tech requirement iterator
					//issue the build order
					if (techAvailable) {
						if (build(goal.structureType, worker)) {
							goal.assignee = worker;
							goal.gracePeriod = Broodwar->getFrameCount() + 48;
							goalsUnderConstruction.push_back(goal);
							goals.pop_front();
							return true;
						}
					} //tech available
				} //if worker capable of building
			} //if goal is a structure
		} //if any goals exist
	} //if haven't queued a building recently

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
bool UnitBehavior::addGoal(BWAPI::UnitType structure, bool front) {
	Goal newGoal;
	//if it's from a different race or isn't a structure
	if (structure.getRace() != Broodwar->self()->getRace() || !structure.isBuilding())
		return false; //we can't make it, don't add it as a goal
	newGoal.structureType = structure;
	newGoal.structure = nullptr;
	newGoal.assignee = nullptr;
	newGoal.tech = TechTypes::None;
	newGoal.isResearch = false;
	newGoal.gracePeriod = 0;
	if (front)
		goals.push_front(newGoal);
	else
		goals.push_back(newGoal);
	return true;
}

bool UnitBehavior::addGoal(Goal &goal, bool front) {
	if (goal.isResearch) {
		if (goal.tech.getRace() != Broodwar->self()->getRace())
			return false; //we can't research it, don't add it as a goal
	}
	else {
		if (goal.structureType.getRace() != Broodwar->self()->getRace() || !goal.structureType.isBuilding())
			return false; //we can't make it, don't add it as a goal
	}

	goal.assignee = nullptr;
	goal.structure = nullptr;
	goal.gracePeriod = 0;

	if (front)
		goals.push_front(goal);
	else
		goals.push_back(goal);
	return true;
}

///<summary>Adds a tech to the goal list so that it will be researched when
///resources are available and all goals added previously have been removed from
///the goal list.</summary>
bool UnitBehavior::addGoal(BWAPI::TechType tech, bool front) {
	if (tech.getRace() != Broodwar->self()->getRace())
		return false; //we can't research it, don't add it as a goal
	Goal newGoal;
	newGoal.structureType = UnitTypes::None;
	newGoal.structure = nullptr;
	newGoal.assignee = nullptr;
	newGoal.tech = tech;
	newGoal.isResearch = true;
	newGoal.gracePeriod = 0;
	goals.push_back(newGoal);
	return true;
}

#pragma endregion

#pragma region TownhallLogic

///<summary>Trains a worker from the target townhall if possible and desirable.</summary>
bool UnitBehavior::evaluateTownhallLogicFor(BWAPI::Unit townhall, int workerCount) {
	if (!townhall->getType().isResourceDepot()) {
		Broodwar << "Warning: attempted to evaluate townhall logic for a non-townhall unit";
		return false;
	}

	//check whether we should build a refinery
	static int gracePeriod = 0;
	if (Broodwar->getFrameCount() > gracePeriod &&
		workerCount > WORKERS_REQUIRED_BEFORE_MINING_GAS &&
		ResourceLogic::canAfford(UnitTypes::Terran_Refinery)) {
		gracePeriod = Broodwar->getFrameCount() + 120;
		Unit closestGeyser = townhall->getClosestUnit(Filter::GetType == UnitTypes::Resource_Vespene_Geyser);
		Unit closestRefinery = townhall->getClosestUnit(Filter::IsRefinery);

		if (closestGeyser) { //if a geyser exists
			//if the closest geyser is closer than the closest refinery (or no refinery exists), 
			//we have an open geyser in proximity and should build on it
			if (!closestRefinery || townhall->getDistance(closestRefinery) > townhall->getDistance(closestGeyser)) {
				//pick a worker and issue the build order
				for (auto &worker : Broodwar->self()->getUnits()) {
					if (worker->exists() && worker->getType().isWorker() && !Helpers::unitIsDisabled(worker) && worker->isGatheringMinerals()) {
						worker->build(UnitTypes::Terran_Refinery, closestGeyser->getTilePosition());
						break;
					} //unit is worker and is not disabled
				} //unit iterator
			} //open geyser in proximity
		} //geyser exists
	} //should build geyser

	//check if there are nearby threats that need to be attacked by our workers
	//this call is extremely expensive, so we only run it once every 100 frames
	static int counter = 0;
	if (counter >= 100) {
		counter -= 100;
		Unitset nearbyGroundEnemies = townhall->getUnitsInRadius(32 * TILE_SIZE, !Filter::IsFlyer && Filter::IsEnemy);
		if (nearbyGroundEnemies.size() > 0) {
			for (auto &worker : townhall->getUnitsInRadius(16 * TILE_SIZE, Filter::IsOwned && Filter::IsWorker)) {
				worker->attack(nearbyGroundEnemies.getPosition());
			}
		}
	}
	else
		counter++;

	// if we have fewer than our ideal number of workers, train more
	if (workerCount < MAXIMUM_WORKER_COUNT && townhall->isIdle())
	{
		if (!townhall->train(townhall->getType().getRace().getWorker())) {
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
	}
	return true;
}

#pragma endregion TownhallLogic

#pragma region RefineryLogic

bool UnitBehavior::evaluateRefineryLogicFor(BWAPI::Unit refinery, int workerCount) {
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

	ref.gracePeriod = Broodwar->getFrameCount() + 120;
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
			if (workerIsAvailable(worker) && workerCount > WORKERS_REQUIRED_BEFORE_MINING_GAS)
			{
				//order the worker to harvest gas
				worker->gather(refinery);
				//add the worker to the list of workers mining from this refinery
				ref.workers.push_front(worker);
			} //worker is able to mine gas and we have enough workers to justify doing so
		} //workers mining gas less than three
	} //workers in radius iterator
	return true;
}

#pragma endregion

#pragma region BarracksLogic

bool UnitBehavior::evaluateBarracksLogicFor(BWAPI::Unit barracks, bool includeFirebats, bool includeMedics) {
	if (!(barracks->getType() == UnitTypes::Terran_Barracks)) {
		Broodwar << "Warning: attempted to evaluate barracks logic for a non-barracks unit";
		return false;
	}

	if (barracks->isIdle()) {
		//Get the unit type we should build
		int medicCount = 0;
		int firebatCount = 0;
		int marineCount = 0;
		if (Broodwar->self()->hasUnitTypeRequirement(UnitTypes::Terran_Academy)) {
			for (auto &u : Broodwar->self()->getUnits()) {
				if (!u->exists())
					continue;
				if (u->getType() == UnitTypes::Terran_Medic)
					medicCount++;
				if (u->getType() == UnitTypes::Terran_Firebat)
					firebatCount++;
				if (u->getType() == UnitTypes::Terran_Marine)
					marineCount++;

			}
			int gas = Broodwar->self()->gas();
			//producing medics is higher priority than producing firebats
			if (medicCount <= (marineCount / 4) && includeMedics && ResourceLogic::canAfford(UnitTypes::Terran_Medic))
				return train(barracks, UnitTypes::Terran_Medic);

			else if (firebatCount <= (marineCount / 4) && includeFirebats && ResourceLogic::canAfford(UnitTypes::Terran_Firebat))
				return train(barracks, UnitTypes::Terran_Firebat);
			//can't afford or don't want to build medics or firebats; instead we'll build marines
			return train(barracks, UnitTypes::Terran_Marine);
		}
		else { //if we can only train marines, then do that
			return train(barracks, UnitTypes::Terran_Marine);
		}
	}
	return true;
}

bool UnitBehavior::evaluateFactoryLogicFor(BWAPI::Unit factory) {
	if (!(factory->getType() == UnitTypes::Terran_Factory)) {
		Broodwar << "Warning: attempted to evaluate factory logic for a non-factory unit";
		return false;
	}

	if (factory->isIdle()) {
		//if we don't have an addon, build one - we need it to make tanks
		if (!factory->getAddon() && ResourceLogic::canAfford(UnitTypes::Terran_Machine_Shop)) {
			return factory->buildAddon(UnitTypes::Terran_Machine_Shop);
		}
		else {
			return train(factory, UnitTypes::Terran_Siege_Tank_Tank_Mode);
		}
	}
	return true;
}

bool UnitBehavior::evaluateAbilityLogicFor(BWAPI::Unit unit) {
	/* Units in combat should attempt to use stim - for units that don't
	have this ability, this call will fail with no side effects */
	if (unit->isAttacking() && unit->getStimTimer() <= 0)
		unit->useTech(TechTypes::Stim_Packs);

	if (unit->getType() == UnitTypes::Terran_Siege_Tank_Siege_Mode ||
		unit->getType() == UnitTypes::Terran_Siege_Tank_Tank_Mode) {

		Unitset nearbyEnemies = unit->getUnitsInRadius(TILE_SIZE * 8, Filter::IsEnemy);
		int closestEnemyDistance = nearbyEnemies.size() > 0 ? (int)nearbyEnemies.getPosition().getDistance(unit->getPosition()) : 99999;
		int siegeModeMaxRange = UnitTypes::Terran_Siege_Tank_Siege_Mode.groundWeapon().maxRange();
		int siegeModeMinRange = UnitTypes::Terran_Siege_Tank_Siege_Mode.groundWeapon().minRange();

		if (unit->getType() == UnitTypes::Terran_Siege_Tank_Tank_Mode &&
			closestEnemyDistance <= siegeModeMaxRange &&
			closestEnemyDistance > siegeModeMinRange
			) {
			//enter siege mode if the enemy is within max range while sieged but outside min range while sieged
			unit->siege();
		}

		if (unit->getType() == UnitTypes::Terran_Siege_Tank_Siege_Mode &&
			(closestEnemyDistance > siegeModeMaxRange ||
			closestEnemyDistance <= siegeModeMinRange)) {
			//exit siege mode if there are no enemies within range or the closest enemy is within our minimum range
			unit->unsiege();
		}
	}

	return true;
}

#pragma endregion



std::deque<Goal> UnitBehavior::getGoals() {
	return goals;
}

///<summary>Issues an order to the specified worker to build the specified structure type.</summary>
bool build(UnitType structure, Unit worker) {
	static BuildingPlacer placer;
	//can't build it if it's not a building or if it's from a different race
	if (!structure.isBuilding() || structure.getRace() != Broodwar->self()->getRace())
		return false;
	if (ResourceLogic::canAfford(structure)) {
		TilePosition targetBuildLocation = placer.getBuildLocationNear(worker->getTilePosition(), structure);
		if (targetBuildLocation) {
			if (worker->build(structure, targetBuildLocation)) {
				lastFrameOnWhichStructureEnqueued = Broodwar->getFrameCount();

				//register an event that draws the target build location for a few seconds
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
		} //placement valid
	} //can afford
	return false;
}

bool train(Unit structure, UnitType type) {
	if (!structure->getType().isBuilding())
		return false;
	if (ResourceLogic::canAfford(type)) {
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
	return false;
}

bool research(Unit structure, TechType type) {
	if (!structure->getType().isBuilding())
		return false;
	if (ResourceLogic::canAfford(type))
		if (structure->research(type))
			return true;

	return false;
}

bool workerIsAvailable(Unit worker) {
	if (!worker || !worker->exists() || worker->getType() != Broodwar->self()->getRace().getWorker())
		return false;
	if (worker->isIdle() || worker->isGatheringMinerals() || worker->isGatheringGas())
		return true;
	return false;
}