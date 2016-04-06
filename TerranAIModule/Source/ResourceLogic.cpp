#pragma once

#include "ResourceLogic.h"

using namespace BWAPI;

int getRequiredSupplyDepots(int enqueuedDepots) {
	/* Keep count of the most recently provided count of enqueued depots so we don't have to 
	evaluate it every time we want to call this function */
	static int previousQueuedDepots = 0;
	if (enqueuedDepots != -1) {
		previousQueuedDepots = enqueuedDepots;
	}
	//start with our current supply usage and add 1 to avoid early supply blocks
	int projectedSupplyUsage = Broodwar->self()->supplyUsed() + 1;
	for (auto &u : Broodwar->self()->getUnits())
	{
		//consider all structures that are currently training units
		if (u->isTraining()) {
			/* Get the unit at the front of the queue and add its supply cost to the estimate. 
			We do this because it's reasonable to expect that if we're training a unit now, we
			will train another unit of the same type once that unit is complete. */
			projectedSupplyUsage += (u->getTrainingQueue()[0]).supplyRequired();
		}
	}

	int requiredSupply = projectedSupplyUsage - Broodwar->self()->supplyTotal();
	//round up
	int requiredSupplyDepots = (requiredSupply + 7) / 8;
	requiredSupplyDepots -= previousQueuedDepots;

	return requiredSupplyDepots > 0 ? requiredSupplyDepots : 0; //don't return negative values
}

int getRequiredSupplyDepots() {
	return getRequiredSupplyDepots(-1);
}

resourceProjection getProjectedIncome(int timeframe) {
	resourceProjection r;
	r.minerals = 0;
	r.gas = 0;
	r.timeframe = timeframe;
	//TODO: implement logic
	return r;
}

resourceProjection getProjectedExpenditure(int timeframe) {
	resourceProjection r;
	r.minerals = 0;
	r.gas = 0;
	r.timeframe = timeframe;
	//TODO: implement logic
	return r;
}