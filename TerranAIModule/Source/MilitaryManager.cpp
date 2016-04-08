#include "MilitaryManager.h"

using namespace BWAPI;

static Position rallyPoint;

void MilitaryManager::setRallyPoint(Position pos) {
	rallyPoint = pos;
}

Position MilitaryManager::getRallyPoint() {
	return rallyPoint;
}