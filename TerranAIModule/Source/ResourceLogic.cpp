#pragma once

#include "ResourceLogic.h"

using namespace BWAPI;

///<summary>Returns the number of supply depots that should be built immediately
///in order to avoid being supply blocked.
///Note that this function is expensive and should be called as
///little as possible.</summary>
int getRequiredSupplyDepots(int enqueuedDepots) {
	/* Keep count of the most recently provided count of enqueued depots so we don't have to
	evaluate it every time we want to call this function */
	static int previousQueuedDepots = 0;
	if (enqueuedDepots != -1) {
		previousQueuedDepots = enqueuedDepots;
	}
	//start with our current supply usage and add 1 to avoid early supply blocks
	int projectedSupplyUsage = Broodwar->self()->supplyUsed() + 2 + (Broodwar->self()->supplyUsed() / 10);
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

	Broodwar->registerEvent([projectedSupplyUsage](Game*){
		Broodwar->drawTextScreen(450, 20, "Proj. Supply Usage: %d", projectedSupplyUsage / 2);
	},
		nullptr,						//condition
		Broodwar->getLatencyFrames());	//duration in frames

	int requiredSupply = projectedSupplyUsage - Broodwar->self()->supplyTotal();
	//round up
	int requiredSupplyDepots = (requiredSupply + 15) / 16;
	requiredSupplyDepots -= previousQueuedDepots;

	return requiredSupplyDepots > 0 ? requiredSupplyDepots : 0; //don't return negative values
}

///<summary>Returns the number of supply depots that should be built immediately
///in order to avoid being supply blocked.
///Note that this function is expensive and should be called as
///little as possible.</summary>
int getRequiredSupplyDepots() {
	return getRequiredSupplyDepots(-1);
}

///<summary>Returns a structure containing expected mineral and 
///gas income over the specified timeframe.
///Note that this function is expensive and should be called as
///little as possible.</summary>
resourceProjection getProjectedIncome(int timeframe) {
	resourceProjection r;
	r.minerals = 0;
	r.gas = 0;
	r.timeframe = timeframe;
	//TODO: implement logic
	return r;
}

///<summary>Returns a structure containing expected mineral and 
///gas expenditures over the specified timeframe.</summary>
resourceProjection getProjectedExpenditure(int timeframe) {
	resourceProjection r;
	r.minerals = 0;
	r.gas = 0;
	r.timeframe = timeframe;
	//TODO: implement logic
	return r;
}

///<summary>Returns the amount of minerals and gas owned by the player 
///that will not be consumed by currently queued structure build orders. 
///Note that this function is expensive and should be called as
///little as possible.</summary>
resourceProjection getUnallocatedResources() {
	resourceProjection r;
	r.minerals = Broodwar->self()->minerals();
	r.gas = Broodwar->self()->gas();
	r.timeframe = 0; //this metric is an instantaneous count, not a true projection

	UnitType structure;

	for (auto &u : Broodwar->self()->getUnits()) {
		//if the unit is a worker and on the way to build a structure but has not started construction
		if (u->getType().isWorker() && u->isConstructing() && !u->getBuildUnit()) {
			r.minerals -= structure.mineralPrice();
			r.gas -= structure.gasPrice();
		}
	}

	return r;
}