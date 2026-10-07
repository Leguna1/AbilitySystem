#include "Ability.h"

#include "AbilityComponent.h"
#include "GameFramework/Character.h"

#include "AbilitySystemLog.h"
#include "UObject/UnrealType.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "Ability"

void UAbility::InitializeAbility(UAbilityComponent* InAbilityComponent, ACharacter* InOwningCharacter)
{
	AbilityComponent = InAbilityComponent;
	OwningCharacter = InOwningCharacter;

	AbilityRank = IsValid(AbilityComponent)
		? AbilityComponent->GetAbilityRank(GetClass())
		: 0;
}

UWorld* UAbility::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	return IsValid(AbilityComponent) ? AbilityComponent->GetWorld() : nullptr;
}

bool UAbility::OwnerHasTag(const FGameplayTag Tag) const
{
	return IsValid(AbilityComponent) && AbilityComponent->HasOwnerTag(Tag);
}

bool UAbility::OwnerHasAllTags(const FGameplayTagContainer& Tags) const
{
	return IsValid(AbilityComponent) && AbilityComponent->HasAllOwnerTags(Tags);
}

bool UAbility::OwnerHasAnyTags(const FGameplayTagContainer& Tags) const
{
	return IsValid(AbilityComponent) && AbilityComponent->HasAnyOwnerTags(Tags);
}
float UAbility::GetModifiedFloat(const FGameplayTag Stat, const float BaseValue) const
{
	return IsValid(AbilityComponent)
		? AbilityComponent->GetModifiedValue(Stat, BaseValue, AbilityTags)
		: BaseValue;
}

int32 UAbility::GetModifiedInt(const FGameplayTag Stat, const int32 BaseValue) const
{
	return FMath::RoundToInt(GetModifiedFloat(Stat, static_cast<float>(BaseValue)));
}
void UAbility::PostInitProperties()
{
	Super::PostInitProperties();
	EnsureIdInTags();
}

void UAbility::PostLoad()
{
	Super::PostLoad();

	// Fixes assets saved before the id was added automatically.
	EnsureIdInTags();
}

void UAbility::EnsureIdInTags()
{
	if (AbilityId.IsValid())
	{
		AbilityTags.AddTag(AbilityId);
	}
}

#if WITH_EDITOR

void UAbility::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	if (PropertyAboutToChange && PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UAbility, AbilityId))
	{
		PreviousAbilityId = AbilityId;
	}
}

void UAbility::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UAbility, AbilityId))
	{
		// A renamed id replaces the old one in the tags.
		if (PreviousAbilityId.IsValid() && PreviousAbilityId != AbilityId)
		{
			AbilityTags.RemoveTag(PreviousAbilityId);
		}

		PreviousAbilityId = FGameplayTag();
	}

	EnsureIdInTags();
}

EDataValidationResult UAbility::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// Only the classes people author need checking, not the native bases.
	if (HasAnyFlags(RF_ClassDefaultObject) && GetClass()->HasAnyClassFlags(CLASS_Abstract | CLASS_Native))
	{
		return Result;
	}

	if (!AbilityId.IsValid())
	{
		Context.AddError(LOCTEXT("NoId", "Ability Id is not set. Every ability needs a unique id, e.g. Ability.Bow.ChargedShot."));
		Result = EDataValidationResult::Invalid;
	}

	if (DisplayName.IsEmpty())
	{
		Context.AddWarning(LOCTEXT("NoName", "Display Name is empty. The hotbar and skill tree will show nothing."));
	}

	if (StartingRank > MaxRank)
	{
		Context.AddWarning(FText::Format(LOCTEXT("RankOrder",
			"Starting Rank ({0}) is above Max Rank ({1}); the ability will sit at rank {0}."), StartingRank, MaxRank));
	}

	// Every ranked value on the class: empty is an error, a short list is worth a heads-up.
	const int32 HighestRank = GetMaxRank();

	for (TFieldIterator<FStructProperty> It(GetClass()); It; ++It)
	{
		const FStructProperty* Property = *It;
		int32 Count = INDEX_NONE;

		if (Property->Struct == FAbilityRankedFloat::StaticStruct())
		{
			Count = Property->ContainerPtrToValuePtr<FAbilityRankedFloat>(this)->Values.Num();
		}
		else if (Property->Struct == FAbilityRankedInt::StaticStruct())
		{
			Count = Property->ContainerPtrToValuePtr<FAbilityRankedInt>(this)->Values.Num();
		}

		if (Count == INDEX_NONE)
		{
			continue;
		}

		const FText Name = Property->GetDisplayNameText();

		if (Count == 0)
		{
			Context.AddError(FText::Format(LOCTEXT("RankedEmpty", "{0} has no values. Add one per rank, or a single value for all ranks."), Name));
			Result = EDataValidationResult::Invalid;
		}
		else if (Count > 1 && Count < HighestRank)
		{
			Context.AddWarning(FText::Format(LOCTEXT("RankedShort",
				"{0} has {1} values but Max Rank is {2}; ranks above {1} reuse the last value."), Name, Count, HighestRank));
		}
	}

	return Result;
}

#endif