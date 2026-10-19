#include "AttributeComponent.h"

#include "AbilitySystemTags.h"
#include "CombatantComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "AttributeComponent"

UAttributeComponent::UAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;
}

void UAttributeComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// Before any BeginPlay, so an Experience Component starting first can already hand out points.
	SpentPoints.Init(0, CoreStats.Num());
	UnspentAttributePoints = StartingAttributePoints;
}

void UAttributeComponent::BeginPlay()
{
	Super::BeginPlay();

	BuildRecalculationOrder();
	CurrentValues.SetNum(Attributes.Num());

	for (int32 Index = 0; Index < Attributes.Num(); ++Index)
	{
		const FAttributeDefinition& Definition = Attributes[Index];
		CurrentValues[Index] = FMath::Clamp(Definition.BaseValue, Definition.MinValue, FMath::Max(Definition.MaxValue, Definition.MinValue));
	}

	// Effects live on the combatant; attributes recalculate whenever they change.
	CombatantComponent = GetOwner()->FindComponentByClass<UCombatantComponent>();

	if (IsValid(CombatantComponent))
	{
		CombatantComponent->OnEffectsChanged.AddDynamic(this, &UAttributeComponent::HandleEffectsChanged);
	}

	RecalculateAttributes();
	ApplyWalkSpeed();

	OnAttributePointsChanged.Broadcast(UnspentAttributePoints);
}

void UAttributeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(CombatantComponent))
	{
		CombatantComponent->OnEffectsChanged.RemoveDynamic(this, &UAttributeComponent::HandleEffectsChanged);
	}

	Super::EndPlay(EndPlayReason);
}

bool UAttributeComponent::HasAttribute(const FGameplayTag Attribute) const
{
	return FindAttributeIndex(Attribute) != INDEX_NONE;
}

float UAttributeComponent::GetAttributeValue(const FGameplayTag Attribute, const float Fallback) const
{
	const int32 Index = FindAttributeIndex(Attribute);

	if (Index == INDEX_NONE)
	{
		return Fallback;
	}

	// Before BeginPlay there are no current values yet: the clamped base is the honest answer.
	if (!CurrentValues.IsValidIndex(Index))
	{
		const FAttributeDefinition& Definition = Attributes[Index];
		return FMath::Clamp(Definition.BaseValue, Definition.MinValue, FMath::Max(Definition.MaxValue, Definition.MinValue));
	}

	return CurrentValues[Index];
}

float UAttributeComponent::GetAttributeValueForAbility(const FGameplayTag Attribute, const FGameplayTagContainer& AbilityTags, const float Fallback) const
{
	const int32 Index = FindAttributeIndex(Attribute);

	if (Index == INDEX_NONE)
	{
		return Fallback;
	}

	return ClampToDefinition(Attributes[Index], EvaluateAttribute(Attributes[Index], AbilityTags));
}

float UAttributeComponent::GetAttributeBase(const FGameplayTag Attribute, const float Fallback) const
{
	const int32 Index = FindAttributeIndex(Attribute);
	return Index != INDEX_NONE ? Attributes[Index].BaseValue : Fallback;
}

void UAttributeComponent::SetAttributeBase(const FGameplayTag Attribute, const float NewBase)
{
	const int32 Index = FindAttributeIndex(Attribute);

	if (Index == INDEX_NONE)
	{
		return;
	}

	Attributes[Index].BaseValue = NewBase;
	RecalculateAttributes();
}

void UAttributeComponent::HandleEffectsChanged()
{
	RecalculateAttributes();
}

void UAttributeComponent::RecalculateAttributes()
{
	// Not started yet: BeginPlay calculates everything once it runs.
	if (CurrentValues.Num() != Attributes.Num())
	{
		return;
	}

	// Core stats first: derived attributes read their fresh values.
	for (const int32 Index : RecalculationOrder)
	{
		const FAttributeDefinition& Definition = Attributes[Index];
		const float NewValue = ClampToDefinition(Definition, EvaluateAttribute(Definition, FGameplayTagContainer()));

		if (FMath::IsNearlyEqual(NewValue, CurrentValues[Index]))
		{
			continue;
		}

		const float OldValue = CurrentValues[Index];
		CurrentValues[Index] = NewValue;

		if (Definition.Attribute == WalkSpeedAttribute)
		{
			ApplyWalkSpeed();
		}

		OnAttributeChanged.Broadcast(Definition.Attribute, OldValue, NewValue);
	}
}

float UAttributeComponent::EvaluateAttribute(const FAttributeDefinition& Definition, const FGameplayTagContainer& AbilityTags) const
{
	const float ProgressionBase = GetProgressionBase(Definition);

	if (!IsValid(CombatantComponent))
	{
		return ProgressionBase;
	}

	// Empty tags: only unscoped effects (the character's own value). With tags: scoped ones that match count too.
	const float OwnValue = CombatantComponent->GetModifiedValue(Definition.Attribute, ProgressionBase, AbilityTags);

	// The group: Stat.AttackPower for Stat.AttackPower.Min. Top-level tags (Stat.MoveSpeed) have none, since "Stat" itself isn't a group.
	const FGameplayTag Group = Definition.Attribute.RequestDirectParent();
	const bool bHasGroup = Group.IsValid() && Group.RequestDirectParent().IsValid();

	return bHasGroup
		? CombatantComponent->GetModifiedValue(Group, OwnValue, AbilityTags)
		: OwnValue;
}

float UAttributeComponent::ClampToDefinition(const FAttributeDefinition& Definition, const float Value)
{
	return FMath::Clamp(Value, Definition.MinValue, FMath::Max(Definition.MaxValue, Definition.MinValue));
}

void UAttributeComponent::ApplyWalkSpeed() const
{
	if (!WalkSpeedAttribute.IsValid() || !HasAttribute(WalkSpeedAttribute))
	{
		return;
	}

	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Movement = IsValid(OwnerCharacter) ? OwnerCharacter->GetCharacterMovement() : nullptr;

	if (IsValid(Movement))
	{
		Movement->MaxWalkSpeed = FMath::Max(GetAttributeValue(WalkSpeedAttribute), 0.0f);
	}
}

/* -------------------- Level and core stats -------------------- */

namespace AttributeProgression
{
	/** Target names Attribute itself or its group (Stat.AttackPower for Stat.AttackPower.Min). */
	bool Targets(const FGameplayTag Target, const FGameplayTag Attribute)
	{
		if (!Target.IsValid() || !Attribute.IsValid())
		{
			return false;
		}

		if (Target == Attribute)
		{
			return true;
		}

		const FGameplayTag Group = Attribute.RequestDirectParent();
		return Group.IsValid() && Group.RequestDirectParent().IsValid() && Target == Group;
	}
}

void UAttributeComponent::SetCharacterLevel(const int32 NewLevel)
{
	const int32 ClampedLevel = FMath::Max(NewLevel, 1);

	if (ClampedLevel == CharacterLevel)
	{
		return;
	}

	CharacterLevel = ClampedLevel;
	RecalculateAttributes();
}

bool UAttributeComponent::GetCoreStatDefinition(const FGameplayTag CoreStat, FCoreStatDefinition& OutDefinition) const
{
	const int32 CoreIndex = FindCoreStatIndex(CoreStat);

	if (CoreIndex == INDEX_NONE)
	{
		return false;
	}

	OutDefinition = CoreStats[CoreIndex];
	return true;
}

int32 UAttributeComponent::GetSpentPoints(const FGameplayTag CoreStat) const
{
	const int32 CoreIndex = FindCoreStatIndex(CoreStat);
	return SpentPoints.IsValidIndex(CoreIndex) ? SpentPoints[CoreIndex] : 0;
}

void UAttributeComponent::AddAttributePoints(const int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}

	UnspentAttributePoints += Amount;
	OnAttributePointsChanged.Broadcast(UnspentAttributePoints);
}

bool UAttributeComponent::CanSpendAttributePoints(const FGameplayTag CoreStat, const int32 Count) const
{
	const int32 CoreIndex = FindCoreStatIndex(CoreStat);
	const int32 AttributeIndex = FindAttributeIndex(CoreStat);

	if (Count <= 0 || Count > UnspentAttributePoints || !SpentPoints.IsValidIndex(CoreIndex) || AttributeIndex == INDEX_NONE)
	{
		return false;
	}

	// Points past the stat's Max Value would be wasted: refuse them instead.
	const FAttributeDefinition& Definition = Attributes[AttributeIndex];
	return GetProgressionBase(Definition) + Count <= Definition.MaxValue + KINDA_SMALL_NUMBER;
}

bool UAttributeComponent::SpendAttributePoints(const FGameplayTag CoreStat, const int32 Count)
{
	if (!CanSpendAttributePoints(CoreStat, Count))
	{
		return false;
	}

	SpentPoints[FindCoreStatIndex(CoreStat)] += Count;
	UnspentAttributePoints -= Count;

	RecalculateAttributes();
	OnAttributePointsChanged.Broadcast(UnspentAttributePoints);
	return true;
}

void UAttributeComponent::ResetSpentPoints()
{
	int32 Refunded = 0;

	for (int32& Points : SpentPoints)
	{
		Refunded += Points;
		Points = 0;
	}

	if (Refunded == 0)
	{
		return;
	}

	UnspentAttributePoints += Refunded;

	RecalculateAttributes();
	OnAttributePointsChanged.Broadcast(UnspentAttributePoints);
}

float UAttributeComponent::GetContributionPreview(const FGameplayTag CoreStat, const FGameplayTag Attribute, const int32 Count) const
{
	const int32 CoreIndex = FindCoreStatIndex(CoreStat);
	return CoreIndex != INDEX_NONE ? GetContributionPerPoint(CoreStats[CoreIndex], Attribute) * Count : 0.0f;
}

float UAttributeComponent::GetProgressionBase(const FAttributeDefinition& Definition) const
{
	float Value = Definition.BaseValue;

	for (const FAttributeGrowth& Growth : GrowthPerLevel)
	{
		if (AttributeProgression::Targets(Growth.Attribute, Definition.Attribute))
		{
			Value += Growth.AmountPerLevel * (CharacterLevel - 1);
		}
	}

	// A core stat: its own spent points.
	const int32 OwnCoreIndex = FindCoreStatIndex(Definition.Attribute);

	if (SpentPoints.IsValidIndex(OwnCoreIndex))
	{
		Value += SpentPoints[OwnCoreIndex];
	}

	// A derived attribute: what every core stat gives it, from the core stat's current value (buffs included).
	for (const FCoreStatDefinition& CoreStatDefinition : CoreStats)
	{
		const float PerPoint = GetContributionPerPoint(CoreStatDefinition, Definition.Attribute);

		if (!FMath::IsNearlyZero(PerPoint))
		{
			Value += PerPoint * GetAttributeValue(CoreStatDefinition.CoreStat);
		}
	}

	return Value;
}

float UAttributeComponent::GetContributionPerPoint(const FCoreStatDefinition& CoreStatDefinition, const FGameplayTag Attribute)
{
	float PerPoint = 0.0f;

	for (const FAttributeContribution& Contribution : CoreStatDefinition.Contributions)
	{
		if (AttributeProgression::Targets(Contribution.Attribute, Attribute))
		{
			PerPoint += Contribution.AmountPerPoint;
		}
	}

	return PerPoint;
}

void UAttributeComponent::BuildRecalculationOrder()
{
	RecalculationOrder.Reset(Attributes.Num());

	for (int32 Index = 0; Index < Attributes.Num(); ++Index)
	{
		if (FindCoreStatIndex(Attributes[Index].Attribute) != INDEX_NONE)
		{
			RecalculationOrder.Add(Index);
		}
	}

	for (int32 Index = 0; Index < Attributes.Num(); ++Index)
	{
		if (FindCoreStatIndex(Attributes[Index].Attribute) == INDEX_NONE)
		{
			RecalculationOrder.Add(Index);
		}
	}
}

int32 UAttributeComponent::FindCoreStatIndex(const FGameplayTag CoreStat) const
{
	return CoreStat.IsValid()
		? CoreStats.IndexOfByPredicate([CoreStat](const FCoreStatDefinition& Definition) { return Definition.CoreStat == CoreStat; })
		: INDEX_NONE;
}

int32 UAttributeComponent::FindAttributeIndex(const FGameplayTag Attribute) const
{
	return Attribute.IsValid()
		? Attributes.IndexOfByPredicate([Attribute](const FAttributeDefinition& Definition) { return Definition.Attribute == Attribute; })
		: INDEX_NONE;
}

#if WITH_EDITOR

EDataValidationResult UAttributeComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TArray<FGameplayTag> Seen;

	for (int32 Index = 0; Index < Attributes.Num(); ++Index)
	{
		const FAttributeDefinition& Definition = Attributes[Index];
		const FText Where = FText::Format(LOCTEXT("Where", "Attributes [{0}]"), Index);

		if (!Definition.Attribute.IsValid())
		{
			Context.AddError(FText::Format(LOCTEXT("NoTag", "{0}: no Attribute tag is set."), Where));
			Result = EDataValidationResult::Invalid;
			continue;
		}

		if (Seen.Contains(Definition.Attribute))
		{
			Context.AddError(FText::Format(LOCTEXT("Duplicate", "{0}: {1} is listed twice. Keep one."), Where, FText::FromName(Definition.Attribute.GetTagName())));
			Result = EDataValidationResult::Invalid;
		}

		Seen.Add(Definition.Attribute);

		if (Definition.MinValue > Definition.MaxValue)
		{
			Context.AddError(FText::Format(LOCTEXT("MinMax", "{0}: Min Value is above Max Value."), Where));
			Result = EDataValidationResult::Invalid;
		}
		else if (Definition.BaseValue < Definition.MinValue || Definition.BaseValue > Definition.MaxValue)
		{
			Context.AddWarning(FText::Format(LOCTEXT("BaseOutside", "{0}: Base Value is outside [Min, Max] and will be clamped."), Where));
		}
	}

	// A power roll needs both ends: a lone Min or Max is almost certainly a mistake.
	const auto WarnHalfPair = [&Context, &Seen](const FGameplayTag MinTag, const FGameplayTag MaxTag)
	{
		if (Seen.Contains(MinTag) != Seen.Contains(MaxTag))
		{
			Context.AddWarning(FText::Format(LOCTEXT("HalfPair", "{0} and {1} go together: list both."),
				FText::FromName(MinTag.GetTagName()), FText::FromName(MaxTag.GetTagName())));
		}
	};

	WarnHalfPair(AbilitySystemTags::Stat_AttackPower_Min, AbilitySystemTags::Stat_AttackPower_Max);
	WarnHalfPair(AbilitySystemTags::Stat_MagicPower_Min, AbilitySystemTags::Stat_MagicPower_Max);

	// Core stats: listed as attributes, feeding attributes that exist, never feeding each other.
	const auto IsListed = [&Seen](const FGameplayTag Target)
	{
		return Seen.ContainsByPredicate([Target](const FGameplayTag Attribute) { return AttributeProgression::Targets(Target, Attribute); });
	};

	TArray<FGameplayTag> SeenCoreStats;

	for (int32 CoreIndex = 0; CoreIndex < CoreStats.Num(); ++CoreIndex)
	{
		const FCoreStatDefinition& CoreStatDefinition = CoreStats[CoreIndex];
		const FText Where = FText::Format(LOCTEXT("CoreWhere", "Core Stats [{0}]"), CoreIndex);

		if (!CoreStatDefinition.CoreStat.IsValid())
		{
			Context.AddError(FText::Format(LOCTEXT("CoreNoTag", "{0}: no Core Stat tag is set."), Where));
			Result = EDataValidationResult::Invalid;
			continue;
		}

		const FText CoreName = FText::FromName(CoreStatDefinition.CoreStat.GetTagName());

		if (SeenCoreStats.Contains(CoreStatDefinition.CoreStat))
		{
			Context.AddError(FText::Format(LOCTEXT("CoreDuplicate", "{0}: {1} is listed twice."), Where, CoreName));
			Result = EDataValidationResult::Invalid;
		}

		SeenCoreStats.Add(CoreStatDefinition.CoreStat);

		if (!Seen.Contains(CoreStatDefinition.CoreStat))
		{
			Context.AddError(FText::Format(LOCTEXT("CoreNotListed", "{0}: add {1} to Attributes too. Its Base Value is the starting value and Max Value the cap."), Where, CoreName));
			Result = EDataValidationResult::Invalid;
		}

		for (const FAttributeContribution& Contribution : CoreStatDefinition.Contributions)
		{
			const FText TargetName = FText::FromName(Contribution.Attribute.GetTagName());

			if (CoreStats.ContainsByPredicate([&Contribution](const FCoreStatDefinition& Other) { return Other.CoreStat == Contribution.Attribute; }))
			{
				Context.AddError(FText::Format(LOCTEXT("CoreFeedsCore", "{0}: {1} is itself a core stat. Core stats can't feed each other."), Where, TargetName));
				Result = EDataValidationResult::Invalid;
			}
			else if (!IsListed(Contribution.Attribute))
			{
				Context.AddWarning(FText::Format(LOCTEXT("CoreTargetMissing", "{0}: gives {1}, which isn't in Attributes, so the points do nothing."), Where, TargetName));
			}
		}
	}

	for (const FAttributeGrowth& Growth : GrowthPerLevel)
	{
		if (!IsListed(Growth.Attribute))
		{
			Context.AddWarning(FText::Format(LOCTEXT("GrowthMissing", "Growth Per Level: {0} isn't in Attributes, so levels don't change it."),
				FText::FromName(Growth.Attribute.GetTagName())));
		}
	}

	if (WalkSpeedAttribute.IsValid() && !Seen.Contains(WalkSpeedAttribute))
	{
		Context.AddWarning(LOCTEXT("WalkMissing", "Walk Speed Attribute is set but not listed in Attributes, so walk speed is never driven."));
	}

	return Result;
}

#endif

#undef LOCTEXT_NAMESPACE