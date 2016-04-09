#pragma once

#include <BWAPI.h>

#include "BuildingPlacer.h"

//minimum workers we must control before attempting to harvest gas
#define WORKERS_REQUIRED_BEFORE_MINING_GAS 12
//maximum number workers we will attempt to train
#define MAXIMUM_WORKER_COUNT 21
//minimum workers before attempting to scout
#define WORKERS_REQUIRED_TO_SCOUT 14

namespace Helpers {

	extern bool unitIsDisabled(BWAPI::Unit unit);
	extern int getOwnedUnitCountOfType(BWAPI::UnitType type);
	extern BWAPI::Position getRandomPosition();
	extern bool requirementsMet(BWAPI::UnitType type);
}