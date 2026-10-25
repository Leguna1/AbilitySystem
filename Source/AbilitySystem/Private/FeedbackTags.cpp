// FeedbackTags.cpp
#include "FeedbackTags.h"

namespace FeedbackTags
{
	/* -------------------- Weapon -------------------- */

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Weapon_Draw, "Cue.Weapon.Draw", "Once: the weapon is drawn.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Weapon_Holster, "Cue.Weapon.Holster", "Once: the weapon is put away.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Weapon_Aura, "Cue.Weapon.Aura", "Loop: on the weapon while it is drawn (glow, embers, hum).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Weapon_Charge, "Cue.Weapon.Charge", "Loop: while the weapon is drawn back or charging (bow string creak).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Weapon_Attack, "Cue.Weapon.Attack", "Once: a swing starts or a shot is released (whoosh, twang).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Weapon_Trail, "Cue.Weapon.Trail", "Loop: during a swing's hit window (blade trail).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Weapon_Hit, "Cue.Weapon.Hit", "Once: the weapon struck a target that took the hit. Surface variants apply.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Weapon_Miss, "Cue.Weapon.Miss", "Once: the weapon struck the world or a target that refused the hit. Surface variants apply.");

	/* -------------------- Projectile -------------------- */

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Projectile_Aura, "Cue.Projectile.Aura", "Loop: the projectile's own look for its whole life - in hand, nocked, flying and stuck - until it returns to the pool (lightning arrow, fire arrow). Overlay Material works here.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Projectile_Nocked, "Cue.Projectile.Nocked", "Loop: on the arrow while it sits nocked on the string, until release (glow, sparks). Start Effect = the moment it's nocked.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Projectile_Launch, "Cue.Projectile.Launch", "Once: the projectile is fired, at its tip.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Projectile_Flight, "Cue.Projectile.Flight", "Loop: while it flies (trail, whistle).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Projectile_Hit, "Cue.Projectile.Hit", "Once: it struck a target that took the hit. Surface variants apply.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Projectile_Hit_Pierce, "Cue.Projectile.Hit.Pierce", "Once: it passed through a target. Falls back to Cue.Projectile.Hit.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Projectile_Miss, "Cue.Projectile.Miss", "Once: it struck the world or a target that refused the hit. Surface variants apply.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Projectile_Expire, "Cue.Projectile.Expire", "Once: it ran out of range or time without hitting anything.");

	/* -------------------- Impact groups -------------------- */

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Group_Hit, "Cue.Group.Hit", "Once per group (volley): its shared hit sound. Empty = each impact's own hit sound.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Group_Miss, "Cue.Group.Miss", "Once per group (volley): its shared miss sound. Empty = each impact's own miss sound.");

	/* -------------------- Hazards -------------------- */

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Hazard_Field, "Cue.Hazard.Field", "Loop: while a ground field is active. Gets the field's radius as a Niagara parameter. Start/Stop Effect = forms/fades.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Hazard_Burst, "Cue.Hazard.Burst", "Once: the hazard bursts (Thunderclap), at its centre. Gets the radius too.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Hazard_Hit, "Cue.Hazard.Hit", "Once per target a burst hits, through the ability's impact group.");

	/* -------------------- Character -------------------- */

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Character_Hurt, "Cue.Character.Hurt", "Once: the character lost health to a hit (flash, grunt). Rate-limited per character.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Character_PoiseBreak, "Cue.Character.PoiseBreak", "Once: a hit broke the character's poise (stagger crunch).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Character_Death, "Cue.Character.Death", "Once: the character died.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Character_Revive, "Cue.Character.Revive", "Once: the character was revived.");

	/* -------------------- Status -------------------- */

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Status, "Cue.Status",
		"Loops on a character while a status lasts. Status.Burning plays Cue.Status.Burning (add that tag), "
		"else Cue.Status itself: a shared look for every status without its own.");

	bool IsLoopCue(const FGameplayTag& Tag)
	{
		return Tag.MatchesTag(Cue_Weapon_Aura) ||
			Tag.MatchesTag(Cue_Weapon_Charge) ||
			Tag.MatchesTag(Cue_Weapon_Trail) ||
			Tag.MatchesTag(Cue_Projectile_Flight) ||
			Tag.MatchesTag(Cue_Projectile_Nocked) ||
			Tag.MatchesTag(Cue_Projectile_Aura) ||
			Tag.MatchesTag(Cue_Hazard_Field) ||
			Tag.MatchesTag(Cue_Status);
	}
}
