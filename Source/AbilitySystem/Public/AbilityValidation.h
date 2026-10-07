#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Misc/DataValidation.h"

struct FStatModifier;
struct FStatusEffectSpec;

/** Validation shared by everything that authors modifiers or statuses. */
namespace AbilityValidation
{
	/** Context is the owner's name for messages, e.g. "Modifiers" or "On Hit Statuses [0]". */
	ABILITYSYSTEM_API EDataValidationResult ValidateModifiers(
		const TArray<FStatModifier>& Modifiers,
		const FString& Location,
		FDataValidationContext& Context);

	ABILITYSYSTEM_API EDataValidationResult ValidateStatusSpecs(
		const TArray<FStatusEffectSpec>& Specs,
		const FString& Location,
		FDataValidationContext& Context);
}

#endif