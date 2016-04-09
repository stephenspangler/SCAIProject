#pragma once

#include "Shared.h"

namespace MilitaryManager {

	enum MilitaryUnitTask {
		NONE,
		ATTACK_TARGET
	};

	enum Tactic {
		DONOTHING,
		DEFEND,
		ATTACK
	};

	void addToArmy(BWAPI::Unit militaryUnit);
	void setRallyPoint(BWAPI::Position pos);
	BWAPI::Position getRallyPoint();
	void validateUnits();
	void moveToRally();
	void evaluateStrategy();
	void evaluateScoutingInfo(BWAPI::Position enemyBaseLoc);
	void indexEnemyUnit(BWAPI::Unit unit);
	void setTactic(Tactic newTactic);
	Tactic getTactic();
	void executeTactic();
	void evaluatePreparedness();

	typedef struct MilitaryUnit_t {
		//the unit itself
		BWAPI::Unit unit;
		//whether the unit is reserved for another task
		bool reserved;
		//the task itself
		MilitaryUnitTask task;
		//the time at which that task expires
		int expiry;
		//the unit which this unit is assigned to killing; task expires when target is dead or out of sight
		BWAPI::Unit target;
		//the unit which this unit should be loaded into
		BWAPI::Unit loader;
	} MilitaryUnit;

}