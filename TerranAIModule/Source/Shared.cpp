#include "Shared.h"

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