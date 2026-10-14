#pragma once

#include "CoreMinimal.h"

/**
 * Finds the numbers an ability's description can show, so one description serves every rank.
 *
 * Write the description once with tokens, e.g.
 *     "Fires {ProjectileCount} arrows, each dealing {DamagePercent} Attack Power. Cooldown: {Cooldown}s."
 * and each token is filled in with that rank's value.
 *
 * Where tokens come from (searched on the ability, the pieces authored inside it such as
 * triggers, conditions, effects and fragments, and data assets it points to):
 *  - Ranked values, named after the property without "ByRank": DamageByRank -> {Damage}.
 *    Each also has a percent form: ChanceByRank 0.2 -> {Chance} = 0.2, {ChancePercent} = 20%.
 *  - Plain numbers, by property name: BaseDamage -> {BaseDamage}, FocusCost -> {FocusCost}.
 *  - Enums, as their display name: DamageType -> {DamageType} = "Physical".
 *  - Modifiers, by stat tag without "Stat.": Stat.AttackSpeed 0.15 (Percent) -> {AttackSpeed} = 15%.
 *    Shown without a sign, so the text says which way it goes ("slows by {MoveSpeed}").
 *  - The same name twice (two Buff Self effects): the second is {Name2}, then {Name3}...
 */
namespace AbilityDescription
{
	/** Every token Source offers at Rank, in the order found. */
	ABILITYSYSTEM_API void CollectTokens(const UObject* Source, int32 Rank, TMap<FString, FText>& OutTokens);
}
