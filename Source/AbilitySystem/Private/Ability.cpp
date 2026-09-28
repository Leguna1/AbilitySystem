#include "Ability.h"

#include "AbilityComponent.h"
#include "GameFramework/Character.h"

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