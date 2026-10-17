#include "ActionBarComponent.h"

#include "Ability.h"
#include "AbilityComponent.h"
#include "ActiveAbility.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "ActionBarComponent"

UActionBarComponent::UActionBarComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UActionBarComponent::BeginPlay()
{
	Super::BeginPlay();

	Slots.Init(nullptr, SlotDefinitions.Num());
	AbilityComponent = GetOwner()->FindComponentByClass<UAbilityComponent>();

	if (!IsValid(AbilityComponent))
	{
		return;
	}

	// From here on, bar abilities answer only to their slot.
	AbilityComponent->SetActionBarDrivesInput(true);
	AbilityComponent->AbilityRankChangedEvent.AddDynamic(this, &UActionBarComponent::HandleAbilityRankChanged);
	AbilityComponent->GrantedAbilitiesChangedEvent.AddDynamic(this, &UActionBarComponent::HandleGrantedAbilitiesChanged);

	// Each slot's own starting ability first (basic attack on LMB)...
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		const TSubclassOf<UAbility> StartingAbility = SlotDefinitions[SlotIndex].StartingAbility;

		if (CanPlaceAbility(StartingAbility) && FindSlotOfAbility(StartingAbility) == INDEX_NONE)
		{
			Slots[SlotIndex] = StartingAbility;
		}
	}

	// ...then the starting layout into the free auto-fill slots...
	for (const TSubclassOf<UAbility>& AbilityClass : StartingLayout)
	{
		const int32 Free = FindFirstAutoFillSlot();

		if (Free == INDEX_NONE)
		{
			break;
		}

		if (CanPlaceAbility(AbilityClass) && FindSlotOfAbility(AbilityClass) == INDEX_NONE)
		{
			Slots[Free] = AbilityClass;
		}
	}

	// ...then anything else already learned (starting ranks).
	if (bAutoPlaceLearnedAbilities)
	{
		for (const TSubclassOf<UAbility>& AbilityClass : AbilityComponent->GetGrantedAbilityClasses())
		{
			const int32 Free = FindFirstAutoFillSlot();

			if (Free == INDEX_NONE)
			{
				break;
			}

			if (CanPlaceAbility(AbilityClass) && FindSlotOfAbility(AbilityClass) == INDEX_NONE)
			{
				Slots[Free] = AbilityClass;
			}
		}
	}

	ApplyInputBindings();
	OnActionBarChanged.Broadcast(INDEX_NONE);
}

void UActionBarComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(AbilityComponent))
	{
		AbilityComponent->AbilityRankChangedEvent.RemoveDynamic(this, &UActionBarComponent::HandleAbilityRankChanged);
		AbilityComponent->GrantedAbilitiesChangedEvent.RemoveDynamic(this, &UActionBarComponent::HandleGrantedAbilitiesChanged);
	}

	Super::EndPlay(EndPlayReason);
}

FActionBarSlotDefinition UActionBarComponent::GetSlotDefinition(const int32 SlotIndex) const
{
	return SlotDefinitions.IsValidIndex(SlotIndex) ? SlotDefinitions[SlotIndex] : FActionBarSlotDefinition();
}

TSubclassOf<UAbility> UActionBarComponent::GetSlotAbility(const int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex] : nullptr;
}

int32 UActionBarComponent::FindSlotOfAbility(const TSubclassOf<UAbility> AbilityClass) const
{
	return AbilityClass ? Slots.IndexOfByKey(AbilityClass) : INDEX_NONE;
}

bool UActionBarComponent::CanPlaceAbility(const TSubclassOf<UAbility> AbilityClass) const
{
	if (!AbilityClass || !IsValid(AbilityComponent) || !AbilityClass->IsChildOf(UActiveAbility::StaticClass()))
	{
		return false;
	}

	const UActiveAbility* Defaults = Cast<UActiveAbility>(AbilityComponent->GetAbilityDefaults(AbilityClass));

	return IsValid(Defaults) &&
		Defaults->UsesActionBar() &&
		AbilityComponent->HasAbility(Defaults->GetAbilityId()) &&
		AbilityComponent->GetAbilityRank(AbilityClass) > 0;
}

bool UActionBarComponent::AssignAbility(const int32 SlotIndex, const TSubclassOf<UAbility> AbilityClass)
{
	if (!Slots.IsValidIndex(SlotIndex) || !CanPlaceAbility(AbilityClass))
	{
		return false;
	}

	const int32 CurrentSlot = FindSlotOfAbility(AbilityClass);

	if (CurrentSlot == SlotIndex)
	{
		return true;
	}

	// Already elsewhere on the bar: the two slots trade places.
	if (CurrentSlot != INDEX_NONE)
	{
		return MoveSlot(CurrentSlot, SlotIndex);
	}

	SetSlot(SlotIndex, AbilityClass);
	return true;
}

bool UActionBarComponent::MoveSlot(const int32 FromSlotIndex, const int32 ToSlotIndex)
{
	if (!Slots.IsValidIndex(FromSlotIndex) || !Slots.IsValidIndex(ToSlotIndex) || FromSlotIndex == ToSlotIndex)
	{
		return false;
	}

	Slots.Swap(FromSlotIndex, ToSlotIndex);
	ApplyInputBindings();
	OnActionBarChanged.Broadcast(INDEX_NONE);
	return true;
}

bool UActionBarComponent::ClearSlot(const int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex) || !Slots[SlotIndex])
	{
		return false;
	}

	SetSlot(SlotIndex, nullptr);
	return true;
}

bool UActionBarComponent::AddToFirstEmptySlot(const TSubclassOf<UAbility> AbilityClass)
{
	if (!CanPlaceAbility(AbilityClass) || FindSlotOfAbility(AbilityClass) != INDEX_NONE)
	{
		return false;
	}

	const int32 Empty = FindFirstAutoFillSlot();

	if (Empty == INDEX_NONE)
	{
		return false;
	}

	SetSlot(Empty, AbilityClass);
	return true;
}

bool UActionBarComponent::RemoveAbility(const TSubclassOf<UAbility> AbilityClass)
{
	return ClearSlot(FindSlotOfAbility(AbilityClass));
}

bool UActionBarComponent::ToggleAbility(const TSubclassOf<UAbility> AbilityClass)
{
	return FindSlotOfAbility(AbilityClass) != INDEX_NONE
		? RemoveAbility(AbilityClass)
		: AddToFirstEmptySlot(AbilityClass);
}

int32 UActionBarComponent::FindFirstAutoFillSlot() const
{
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		if (!Slots[SlotIndex] && SlotDefinitions[SlotIndex].bAutoFill)
		{
			return SlotIndex;
		}
	}

	return INDEX_NONE;
}

void UActionBarComponent::SetSlot(const int32 SlotIndex, const TSubclassOf<UAbility> AbilityClass)
{
	Slots[SlotIndex] = AbilityClass;
	ApplyInputBindings();
	OnActionBarChanged.Broadcast(SlotIndex);
}

void UActionBarComponent::ApplyInputBindings() const
{
	if (!IsValid(AbilityComponent))
	{
		return;
	}

	AbilityComponent->ClearAbilityInputOverrides();

	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		const UAbility* Defaults = AbilityComponent->GetAbilityDefaults(Slots[SlotIndex]);

		if (IsValid(Defaults))
		{
			AbilityComponent->SetAbilityInputOverride(Defaults->GetAbilityId(), SlotDefinitions[SlotIndex].InputTag);
		}
	}
}

void UActionBarComponent::HandleAbilityRankChanged(const FGameplayTag AbilityId, const int32 OldRank, const int32 NewRank)
{
	const TSubclassOf<UAbility> AbilityClass = AbilityComponent->FindAbilityClassById(AbilityId);

	if (NewRank <= 0)
	{
		RemoveAbility(AbilityClass);
	}
	else if (OldRank <= 0 && bAutoPlaceLearnedAbilities)
	{
		AddToFirstEmptySlot(AbilityClass);
	}
}

void UActionBarComponent::HandleGrantedAbilitiesChanged()
{
	bool bChanged = false;

	// A revoked ability (weapon swap) leaves the bar.
	for (TSubclassOf<UAbility>& AbilityClass : Slots)
	{
		if (AbilityClass && !CanPlaceAbility(AbilityClass))
		{
			AbilityClass = nullptr;
			bChanged = true;
		}
	}

	// A newly granted, already learned ability (a weapon kit, a companion's new skill) joins it.
	if (bAutoPlaceLearnedAbilities && IsValid(AbilityComponent))
	{
		for (const TSubclassOf<UAbility>& AbilityClass : AbilityComponent->GetGrantedAbilityClasses())
		{
			if (CanPlaceAbility(AbilityClass) && FindSlotOfAbility(AbilityClass) == INDEX_NONE)
			{
				const int32 Free = FindFirstAutoFillSlot();

				if (Free == INDEX_NONE)
				{
					break;
				}

				Slots[Free] = AbilityClass;
				bChanged = true;
			}
		}
	}

	if (bChanged)
	{
		ApplyInputBindings();
		OnActionBarChanged.Broadcast(INDEX_NONE);
	}
}

#if WITH_EDITOR
EDataValidationResult UActionBarComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	TArray<FGameplayTag> Seen;

	for (int32 Index = 0; Index < SlotDefinitions.Num(); ++Index)
	{
		const FGameplayTag InputTag = SlotDefinitions[Index].InputTag;

		if (!InputTag.IsValid())
		{
			Context.AddError(FText::Format(LOCTEXT("NoInput", "Slot Definitions [{0}]: no Input Tag, so this slot can never fire."), Index));
			Result = EDataValidationResult::Invalid;
		}
		else if (Seen.Contains(InputTag))
		{
			Context.AddError(FText::Format(LOCTEXT("DupInput", "Slot Definitions [{0}]: {1} is used by another slot."), Index, FText::FromName(InputTag.GetTagName())));
			Result = EDataValidationResult::Invalid;
		}

		Seen.Add(InputTag);
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
