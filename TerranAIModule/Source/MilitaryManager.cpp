#include "MilitaryManager.h"
#include "UnitBehavior.h"

using namespace BWAPI;
using namespace MilitaryManager;

static Position rallyPoint;
static bool obeyRallyPoint = true;
static std::vector<MilitaryUnit> army;
static std::vector<Unit> enemyUnits;
static Tactic tactic;
static Position enemyBase;

void MilitaryManager::addToArmy(Unit militaryUnit) {
	MilitaryUnit newUnit;
	newUnit.expiry = -1;
	newUnit.reserved = false;
	newUnit.target = nullptr;
	newUnit.task = MilitaryUnitTask::NONE;
	newUnit.unit = militaryUnit;
	newUnit.loader = nullptr;
	army.push_back(newUnit);
}

void MilitaryManager::setRallyPoint(Position pos) {
	rallyPoint = pos;
}

Position MilitaryManager::getRallyPoint() {
	return rallyPoint;
}

///<summary>Removes units that no longer exist from the army</summary>
void MilitaryManager::validateUnits() {
	std::vector<MilitaryUnit>::iterator i = army.begin();
	while (i != army.end()) {
		MilitaryUnit mu = *i;
		if (!mu.unit->exists()) {
			i = army.erase(i);
			continue;
		}
		i++;
	}
}

///<summary>Orders all military units that are not reserved for another task
///to attack-move to the current rally point.</summary>
void MilitaryManager::moveToRally() {
	if (!obeyRallyPoint)
		return;
	for (auto &mu : army) {
		if (!mu.reserved) {
			//check if we're already attack-moving to the rally point or have an independent move order; if so, don't reissue the order
			if (!(mu.unit->getOrder() == Orders::AttackMove || mu.unit->getOrder() == Orders::Move)
				|| mu.unit->getOrderTargetPosition().getDistance(rallyPoint) > 4 * TILE_SIZE) {
				//check that we're not already in close proximity to the rally point or attacking something
				if (mu.unit->getPosition().getDistance(rallyPoint) > 5 * TILE_SIZE || mu.unit->isAttacking()) {
					mu.unit->attack(rallyPoint);
				} //not close to rally
			} //not moving to rally
		} //not reserved
	} //MilitaryUnit iterator
}

int countEnemyUnitsOfType(UnitType type) {
	int count = 0;
	for (auto *u : enemyUnits) {
		if (u->getType() == type)
			count++;
	}
	return count;
}

void MilitaryManager::evaluateStrategy() {
	static bool firstRunAfterEnemyRaceDiscovered = true;
	Race enemyRace = Broodwar->enemy()->getRace();
	if (enemyRace == Races::Unknown || !firstRunAfterEnemyRaceDiscovered)
		return;
	firstRunAfterEnemyRaceDiscovered = false;
	char terranstr[] = "some grimy humans.";
	char protossstr[] = "those religious sycophants.";
	char zergstr[] = "the swarm.";
	char *str =
		enemyRace == Races::Terran ? terranstr :
		enemyRace == Races::Protoss ? protossstr :
		zergstr;
	Broodwar << "We've found the enemy. We're against " << str << std::endl;

	if (enemyRace == Races::Zerg) {
		//against zerg, add an extra barracks and get stim for a quick bust
		UnitBehavior::addGoal(UnitTypes::Terran_Barracks);
		UnitBehavior::addGoal(TechTypes::Stim_Packs);
	}

	if (enemyRace == Races::Protoss || enemyRace == Races::Terran) {
		//add more siege tank production against protoss and terran
		UnitBehavior::addGoal(UnitTypes::Terran_Machine_Shop);
	}

	if (enemyRace == Races::Protoss) {
		UnitBehavior::addGoal(UnitTypes::Terran_Armory);
	}

	//add an extra barracks to the end of each build to help us make use of excess mineral income
	UnitBehavior::addGoal(UnitTypes::Terran_Barracks);
	//add a comsat after that
	UnitBehavior::addGoal(UnitTypes::Terran_Comsat_Station);
}

void MilitaryManager::evaluateScoutingInfo(Position enemyBaseLoc) {
	//store the position of the enemy base
	enemyBase = enemyBaseLoc;

	//we're concerned mainly about the number of basic units the enemy has built
	Race enemyRace = Broodwar->enemy()->getRace();
	Broodwar << "Found enemy townhall." << std::endl;
	if ((enemyRace == Races::Terran && countEnemyUnitsOfType(UnitTypes::Terran_Marine) >= 4) ||
		(enemyRace == Races::Zerg && countEnemyUnitsOfType(UnitTypes::Zerg_Zergling) >= 4) ||
		(enemyRace == Races::Protoss && countEnemyUnitsOfType(UnitTypes::Protoss_Zealot) >= 2))
	{
		//if we're getting rushed, build a couple of bunkers ASAP
		UnitBehavior::addGoal(UnitTypes::Terran_Bunker, true, 2);
	}
}

void MilitaryManager::indexEnemyUnit(Unit unit) {
	//if we haven't seen this enemy unit before, add it to the list
	bool haveSeen = false;
	for (auto &u : enemyUnits) {
		if (unit == u) {
			haveSeen = true;
			break;
		}
	}
	if (!haveSeen)
		enemyUnits.push_back(unit);
}

void MilitaryManager::setTactic(Tactic newTactic) {
	tactic = newTactic;
}

Tactic MilitaryManager::getTactic() {
	return tactic;
}

//counts units on their way to be loaded
int getLoaderAllocatedUnitCount(Unit loader) {
	int count = 0;
	for (MilitaryUnit &mu : army) {
		if (mu.loader == loader) {
			count++;
		}
	}
	return count;
}

void MilitaryManager::executeTactic() {
	if (tactic == Tactic::DONOTHING)
		return;

	else if (tactic == Tactic::DEFEND) {
		//find townhall and station our units around it
		Unit townhall = nullptr;
		for (auto &u : Broodwar->self()->getUnits()) {
			if (u->getType().isResourceDepot())
				townhall = u;
		}
		if (townhall)
			setRallyPoint(townhall->getPosition());

		//find any completed bunkers and make sure there are four marines in them
		for (auto &u : Broodwar->self()->getUnits()) {
			if (u->getType() == UnitTypes::Terran_Bunker) {
				if (u->getLoadedUnits().size() < 4) {
					for (auto &mu : army) {
						if (getLoaderAllocatedUnitCount(u) < 4) {
							if (mu.unit->getType() == UnitTypes::Terran_Marine &&
								!mu.reserved) {
								//prevent our marine from receiving further orders
								mu.reserved = true;
								mu.loader = u;
								//issue the load order
								u->load(mu.unit);
							} //military unit is marine and is not reserved
						} //fewer than 4 units allocated to the bunker
					} //military unit iterator
					for (auto &mu : army) {
						if (mu.loader == u) {
							if (mu.unit->getOrder() != Orders::EnterTransport || mu.unit->getOrderTarget() != u) {
								u->load(mu.unit);
							} //unit is not attempting to enter the bunker
						} //unit should be loaded into the bunker
					} //military unit iterator
				} //fewer than 4 units loaded
			} //is bunker
		} //unit iterator

		//if there are enemies threatening our base, attack them
		Unitset nearbyEnemies = townhall->getUnitsInRadius(32 * TILE_SIZE, Filter::IsEnemy);
		if (nearbyEnemies.size() > 0) {
			setRallyPoint(nearbyEnemies.getPosition());
		} //we're being attacked
		return;
	} //tactic is defend

	else if (tactic == Tactic::ATTACK) {
		Unit target = nullptr;
		static int gracePeriod = 0;
		static Position enemyLocation;
		static bool planningAttack = false;
		static bool attackingBase = false;

		if (Broodwar->getFrameCount() < gracePeriod)
			return;



		//select our target from a list of priorities
		for (auto &u : Broodwar->getAllUnits()) {
			if (u->exists() && Filter::IsEnemy(u)) {
				if (!target)
					target = u;
			}
		}

		//we have a target and we're not already planning an attack
		if (target && !planningAttack) {
			//set our rally point to the average of our units' positions and wait a while for our units to gather up
			Unitset a;
			for (auto &mu : army) {
				a.insert(mu.unit);
			}
			setRallyPoint(a.getPosition());
			enemyLocation = target->getPosition();
			gracePeriod = Broodwar->getFrameCount() + (24 * 20);
			obeyRallyPoint = true;
			planningAttack = true;
			attackingBase = target->getType().isBuilding();
			Broodwar << "Preparing attack against enemy " << (attackingBase ? "base." : "unit.") << std::endl;
		}
		else if (planningAttack) { //we're planning an attack and the grace period has expired
			//let slip the dogs of war
			setRallyPoint(enemyLocation);
			for (auto &mu : army) {
				mu.unit->attack(enemyLocation);
			}
			//let them go for a while before we reevaluate
			gracePeriod = Broodwar->getFrameCount() + (attackingBase ? (24 * 60) : (24 * 20));
			planningAttack = false;
			obeyRallyPoint = true;
			Broodwar << "Launching attack." << std::endl;
		}
		else { //fourth priority (no target) - spread army out at random searching for the enemy
			for (auto &mu : army) {
				if (mu.unit->isIdle())
					mu.unit->attack(Helpers::getRandomPosition());
			}
			obeyRallyPoint = false;
		}
		for (auto &mu : army) {
			if (mu.loader) {
				if (mu.loader->getType() == UnitTypes::Terran_Bunker) {
					if (mu.unit->isLoaded()) {
						mu.loader->unload(mu.unit);
					} //is loaded
					mu.loader = nullptr;
					mu.reserved = false;
				} //loader is a bunker
			} //has a loader
		} //military unit iterator
	} //tactic is attack
}

void MilitaryManager::evaluatePreparedness() {
	if (!Broodwar->enemy())
		return;
	Race enemyRace = Broodwar->enemy()->getRace();
	if ((enemyRace == Races::Zerg &&
		Broodwar->self()->hasResearched(TechTypes::Stim_Packs))
		||
		((enemyRace == Races::Terran || enemyRace == Races::Protoss) &&
		Helpers::getOwnedUnitCountOfType(UnitTypes::Terran_Siege_Tank_Tank_Mode) > 4))
	{
		setTactic(Tactic::ATTACK);
		return;
	}
	else
		setTactic(Tactic::DEFEND);
}