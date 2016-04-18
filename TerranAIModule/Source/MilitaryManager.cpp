#include "MilitaryManager.h"
#include "UnitBehavior.h"

using namespace BWAPI;

namespace MilitaryManager {

	static Position rallyPoint;
	static bool obeyRallyPoint = true;
	static std::vector<MilitaryUnit> army;
	static std::vector<Unit> enemyUnits;
	static Tactic tactic;
	static Position enemyBase;
	static bool attacking = false;

	void addToArmy(Unit militaryUnit) {
		MilitaryUnit newUnit;
		newUnit.expiry = -1;
		newUnit.reserved = false;
		newUnit.target = nullptr;
		newUnit.task = MilitaryUnitTask::NONE;
		newUnit.unit = militaryUnit;
		newUnit.loader = nullptr;
		army.push_back(newUnit);
	}

	void setRallyPoint(Position pos) {
		rallyPoint = pos;
	}

	Position getRallyPoint() {
		return rallyPoint;
	}

	///<summary>Removes units that no longer exist from the army</summary>
	void validateUnits() {
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
	void moveToRally() {
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

	bool getUnitsGathered() {
		double muPercent = 0.0;
		for (auto &mu : army) {
			//if the unit is within 10 tiles of the rallyPoint, add 1 to muPercent
			if (mu.unit->getPosition().getDistance(rallyPoint) <= 10 * TILE_SIZE){
				muPercent++;
			}
		}
		//if the value stored in muPercent is >= 75% of the army size, 
		//then at least 75% of the army has gathered at the rally point
		if (muPercent >= army.size() * 0.75){
			Broodwar << "Gathered " << muPercent << " of " << army.size() << " units" <<std::endl; //for debugging purposes
			return true;
		}
		else{
			return false;
		}
	}

	int countEnemyUnitsOfType(UnitType type) {
		int count = 0;
		for (auto *u : enemyUnits) {
			if (u->getType() == type)
				count++;
		}
		return count;
	}

	void evaluateStrategy() {
		static bool firstRunAfterEnemyRaceDiscovered = true;
		Race enemyRace = Broodwar->enemy()->getRace();
		if (enemyRace == Races::Unknown || !firstRunAfterEnemyRaceDiscovered)
			return;
		firstRunAfterEnemyRaceDiscovered = false;
		char terranstr[] = "other Terrans.";
		char protossstr[] = "the Protoss.";
		char zergstr[] = "the Swarm.";
		char *str =
			enemyRace == Races::Terran ? terranstr :
			enemyRace == Races::Protoss ? protossstr :
			zergstr;
		Broodwar << "We've found the enemy. We're against " << str << std::endl;

		if (enemyRace == Races::Zerg) {
			//against zerg, add an extra barracks and get stim for a quick bust
			UnitBehavior::addGoal(UnitTypes::Terran_Barracks);
			UnitBehavior::addGoal(TechTypes::Stim_Packs);
			//and a comsat station so we can deal with lurkers
			UnitBehavior::addGoal(UnitTypes::Terran_Comsat_Station);
		}

		if (enemyRace == Races::Protoss || enemyRace == Races::Terran) {
			//add more siege tank production against protoss and terran
			UnitBehavior::addGoal(UnitTypes::Terran_Machine_Shop);
		}

		if (enemyRace == Races::Protoss) {
			UnitBehavior::addGoal(UnitTypes::Terran_Comsat_Station);
		}
		//add an extra barracks to the end of each build to help us make use of excess mineral income
		UnitBehavior::addGoal(UnitTypes::Terran_Barracks);
		
	}

	void evaluateScoutingInfo(Position enemyBaseLoc) {
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

		if (enemyRace == Races::Protoss && countEnemyUnitsOfType(UnitTypes::Protoss_Citadel_of_Adun) > 0) {
			//decent chance we're getting DT rushed; spam detection
			UnitBehavior::addGoal(UnitTypes::Terran_Missile_Turret, true, 2);
		}
	}

	void indexEnemyUnit(Unit unit) {
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

	void setTactic(Tactic newTactic) {
		tactic = newTactic;
	}

	Tactic getTactic() {
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

	void executeTactic() {
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
			static int gatheringPeriod = 0;
			static Position enemyLocation;
			static bool planningAttack = false;


			if (Broodwar->getFrameCount() < gracePeriod)
				return;

			//select a target
			for (auto &u : Broodwar->getAllUnits()) {
				if (!target && u->getPlayer()->isEnemy(Broodwar->self()))
					target = u;
			}

			if (attacking && target) { //if we're attacking and we can see an enemy unit, go kill it
				setRallyPoint(target->getPosition());
				return;
			}

			attacking = false; //we have no target - give up the attack

			//we have a target and we're not already planning an attack
			if (target && !planningAttack) {
				//set our rally point to the midpoint of the average of our units' positions and the target's position and wait til our units gather
				Unitset a;
				for (auto &mu : army) {
					a.insert(mu.unit);
				}
				enemyLocation = target->getPosition();
				setRallyPoint((a.getPosition() + enemyLocation) / 2);
				gatheringPeriod = Broodwar->getFrameCount() + (24 * 60); //timeout so we don't get stuck gathering forever

				obeyRallyPoint = true;
				planningAttack = true;
				Broodwar << "Preparing attack against enemy " << (target->getType().isBuilding() ? "base." : "unit.") << std::endl;
			}
			else if (planningAttack && (getUnitsGathered() || Broodwar->getFrameCount() > gatheringPeriod)) { //we're planning an attack and our units are ready, or the gathering period has expired
				//let slip the dogs of war
				setRallyPoint(enemyLocation);
				for (auto &mu : army) {
					mu.unit->attack(enemyLocation);
				}
				//let them go for a while before we reevaluate
				gracePeriod = Broodwar->getFrameCount() + (24 * 10);
				planningAttack = false;
				obeyRallyPoint = true;
				Broodwar << "Launching attack." << std::endl;
				attacking = true;
			}
			else if (!planningAttack) { //we have no target and we're not planning an attack - spread out and search for the enemy
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
		//if we have 4+ siege tanks
		if (Helpers::getOwnedUnitCountOfType(UnitTypes::Terran_Siege_Tank_Tank_Mode) >= 4 ||
			attacking) //or we're currently attacking
		{
			setTactic(Tactic::ATTACK);
			return;
		}
		else {
			setTactic(Tactic::DEFEND);
		}
	}

}