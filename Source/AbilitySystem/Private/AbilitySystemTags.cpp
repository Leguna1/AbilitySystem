// AbilitySystemTags.cpp
#include "AbilitySystemTags.h"

namespace AbilitySystemTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Owner is dead: abilities cannot activate and payloads are refused.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Invulnerable, "State.Invulnerable", "Owner refuses payloads (e.g. dodge i-frames).");
}