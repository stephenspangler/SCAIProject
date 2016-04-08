#pragma once

#include "Shared.h"

//represents projected resource income or expenditure over a specified length of time
typedef struct resourceProjection_t {
	int timeframe; //in seconds
	int minerals;
	int gas;
} resourceProjection;

extern int getRequiredSupplyDepots(int enqueuedDepots);
extern int getRequiredSupplyDepots();
extern resourceProjection getProjectedIncome(int timeframe = 60);
extern resourceProjection getProjectedExpenditure(int timeframe = 60);
extern void calcUnallocatedResources();
extern resourceProjection getUnallocatedResources();
extern bool canAfford(BWAPI::UnitType type);
extern bool canAfford(BWAPI::TechType type);