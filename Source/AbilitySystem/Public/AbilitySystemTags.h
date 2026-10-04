// AbilitySystemTags.h
#pragma once

#include "NativeGameplayTags.h"

/**
 * Native tags: only the tags C++ reads by name live here. Everything that is
 * content (ability ids, inputs, animation events, skill tree nodes) lives in
 * Config/Tags/AbilitySystemTags.ini, so adding content never needs a recompile.
 */
namespace AbilitySystemTags
{
	/* -------------------- State: tags on the character -------------------- */

	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invulnerable);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_PerfectDodgeWindow);

	/* -------------------- Stat: what modifiers change -------------------- */

	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Damage);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_DamageTaken);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Cost);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Cooldown);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_ProjectileCount);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_ShotCount);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_ChargeTime);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Spread);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Radius);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_AttackSpeed);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_MoveSpeed);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combo_Finished);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_PierceCount);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_PierceDamage);
}