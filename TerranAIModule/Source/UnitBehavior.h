#pragma once

#include "Shared.h"
#include "ResourceLogic.h"

extern bool evaluateWorkerLogicFor(BWAPI::Unit worker, int requiredSupplyDepots);
extern bool evaluateTownhallLogicFor(BWAPI::Unit townhall, int workerCount);