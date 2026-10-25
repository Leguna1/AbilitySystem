// FeedbackTags.h
#pragma once

#include "NativeGameplayTags.h"

/**
 * Cue tags: the moments that can look or sound like something. Code fires these;
 * Feedback Sets decide what each one plays.
 *
 * A missing cue falls back up its tag: Cue.Projectile.Hit.Pierce plays
 * Cue.Projectile.Hit when a set has no Pierce entry. So add children (a .Crit,
 * a .Fire) only when they should differ.
 */
namespace FeedbackTags
{
	/* -------------------- Weapon -------------------- */

	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Weapon_Draw);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Weapon_Holster);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Weapon_Aura);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Weapon_Charge);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Weapon_Attack);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Weapon_Trail);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Weapon_Hit);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Weapon_Miss);

	/* -------------------- Projectile -------------------- */

	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Projectile_Launch);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Projectile_Flight);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Projectile_Hit);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Projectile_Hit_Pierce);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Projectile_Miss);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Projectile_Expire);

	/* -------------------- Impact groups (a volley's shared sound) -------------------- */

	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Group_Hit);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Group_Miss);

	/* -------------------- Hazards (fields, traps, zones) -------------------- */

	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Hazard_Field);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Hazard_Burst);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Hazard_Hit);

	/* -------------------- Character (on its Character Feedback component) -------------------- */

	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Character_Hurt);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Character_PoiseBreak);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Character_Death);
	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Character_Revive);

	/* -------------------- Status (looping, on the character) -------------------- */

	ABILITYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Status);

	/** Cues that are started and stopped (auras, trails, flight, statuses) rather than played once. */
	ABILITYSYSTEM_API bool IsLoopCue(const FGameplayTag& Tag);
}
