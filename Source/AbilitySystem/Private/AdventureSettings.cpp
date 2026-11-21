#include "AdventureSettings.h"

UAdventureSettings::UAdventureSettings()
{
	// +1 to +3 always work; after that every level is a gamble, the way an enhancement system should feel.
	EnhancementSuccessChance = { 100.0f, 100.0f, 100.0f, 85.0f, 75.0f, 65.0f, 50.0f, 40.0f, 30.0f, 20.0f };
}
