// AbilitySystemTags.cpp
#include "AbilitySystemTags.h"

namespace AbilitySystemTags
{
	/* -------------------- State -------------------- */

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Owner is dead: abilities cannot activate and payloads are refused.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Invulnerable, "State.Invulnerable", "Owner refuses payloads (e.g. dodge i-frames).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_PerfectDodgeWindow, "State.PerfectDodgeWindow", "Early slice of a dodge: evading a hit now counts as a perfect dodge.");

	/* -------------------- Stat -------------------- */

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Damage, "Stat.Damage", "Damage dealt by the ability (multiplier for ranged, per-hit for melee).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_DamageTaken, "Stat.DamageTaken", "Incoming damage multiplier on the receiver.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Cost, "Stat.Cost", "Focus cost per payment.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Cooldown, "Stat.Cooldown", "Cooldown duration.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_ProjectileCount, "Stat.ProjectileCount", "Arrows per release.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_ShotCount, "Stat.ShotCount", "Shots per rapid-fire volley.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_ChargeTime, "Stat.ChargeTime", "Time to reach full charge.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Spread, "Stat.Spread", "Spread angle of multi-arrow shots.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Radius, "Stat.Radius", "Area radius (e.g. volley impact radius).");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_AttackSpeed, "Stat.AttackSpeed", "Montage play-rate multiplier (base 1).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_MoveSpeed, "Stat.MoveSpeed", "Walk speed multiplier (base 1). Only unscoped modifiers apply.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combo_Finished, "Event.Combo.Finished", "Gameplay event: a combo's final step fired.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_PierceCount, "Stat.PierceCount", "Extra targets an arrow passes through.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_PierceDamage, "Stat.PierceDamage", "Damage factor applied per target passed through.");
}