#include "Shared.h"

using namespace BWAPI;

bool Helpers::unitIsDisabled(BWAPI::Unit unit) {

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

int Helpers::getOwnedUnitCountOfType(BWAPI::UnitType type) {
	int count = 0;
	for (auto &u : Broodwar->self()->getUnits()) {
		if (u->exists() && u->isCompleted() && u->getType() == type) {
			count++;
		}
	}
	return count;
}

Position Helpers::getRandomPosition() {
	int maxX = Broodwar->mapWidth() * TILE_SIZE;
	int maxY = Broodwar->mapHeight() * TILE_SIZE;

	Position p;
	p.x = rand() % maxX;
	p.y = rand() % maxY;

	return p;
}

bool Helpers::requirementsMet(UnitType type) {
	bool techAvailable = true;
	for (auto &m : type.requiredUnits()) {
		if (!Broodwar->self()->hasUnitTypeRequirement(m.first, m.second))
			techAvailable = false;
	}
	return techAvailable;
}