#include "ModifierTypes.h"

void FStatModifierContainer::Apply(const UObject* Source, const FName EntryKey, const TArray<FStatModifier>& InModifiers, const int32 Rank, const FGameplayTagContainer& InGrantedTags, const double ExpiresAt, float Duration)
{
	const FObjectKey SourceKey(Source);
	int32 Index = FindEntry(SourceKey, EntryKey);

	if (Index == INDEX_NONE)
	{
		Index = Entries.AddDefaulted();
		Entries[Index].Source = SourceKey;
		Entries[Index].EntryKey = EntryKey;
	}

	FModifierEntry& Entry = Entries[Index];
	Entry.Modifiers.Reset(InModifiers.Num());

	for (const FStatModifier& Modifier : InModifiers)
	{
		if (!Modifier.Stat.IsValid())
		{
			continue;
		}

		FResolvedStatModifier& Resolved = Entry.Modifiers.AddDefaulted_GetRef();
		Resolved.Stat = Modifier.Stat;
		Resolved.Operation = Modifier.Operation;
		Resolved.Magnitude = Modifier.Magnitude.Get(Rank);
		Resolved.AbilityScope = Modifier.AbilityScope;
		Resolved.RequiredOwnerTags = Modifier.RequiredOwnerTags;
	}

	Entry.GrantedTags = InGrantedTags;
	Entry.ExpiresAt = ExpiresAt;
	Entry.Duration = FMath::Max(Duration, 0.0f);
}

bool FStatModifierContainer::Remove(const UObject* Source, const FName EntryKey)
{
	const int32 Index = FindEntry(FObjectKey(Source), EntryKey);

	if (Index == INDEX_NONE)
	{
		return false;
	}

	Entries.RemoveAt(Index);
	return true;
}

bool FStatModifierContainer::RemoveAll(const UObject* Source)
{
	const FObjectKey SourceKey(Source);

	return Entries.RemoveAll([&SourceKey](const FModifierEntry& Entry)
	{
		return Entry.Source == SourceKey;
	}) > 0;
}

bool FStatModifierContainer::Contains(const UObject* Source, const FName EntryKey) const
{
	return FindEntry(FObjectKey(Source), EntryKey) != INDEX_NONE;
}

float FStatModifierContainer::Evaluate(const FGameplayTag Stat, const float BaseValue, const FGameplayTagContainer& AbilityTags, const FGameplayTagContainer& OwnerTags, const double Now) const
{
	float AddTotal = 0.0f;
	float PercentTotal = 0.0f;

	for (const FModifierEntry& Entry : Entries)
	{
		if (Entry.IsExpired(Now))
		{
			continue;
		}

		for (const FResolvedStatModifier& Modifier : Entry.Modifiers)
		{
			if (!Modifier.Stat.MatchesTagExact(Stat))
			{
				continue;
			}

			if (!Modifier.AbilityScope.IsEmpty() && !AbilityTags.HasAny(Modifier.AbilityScope))
			{
				continue;
			}

			if (!OwnerTags.HasAll(Modifier.RequiredOwnerTags))
			{
				continue;
			}

			if (Modifier.Operation == EModifierOperation::Add)
			{
				AddTotal += Modifier.Magnitude;
			}
			else
			{
				PercentTotal += Modifier.Magnitude;
			}
		}
	}

	return (BaseValue + AddTotal) * FMath::Max(0.0f, 1.0f + PercentTotal);
}

void FStatModifierContainer::AppendGrantedTags(FGameplayTagContainer& OutTags, const double Now) const
{
	for (const FModifierEntry& Entry : Entries)
	{
		if (!Entry.IsExpired(Now))
		{
			OutTags.AppendTags(Entry.GrantedTags);
		}
	}
}

bool FStatModifierContainer::PruneExpired(const double Now)
{
	return Entries.RemoveAll([Now](const FModifierEntry& Entry)
	{
		return Entry.IsExpired(Now);
	}) > 0;
}

double FStatModifierContainer::GetNextExpiryTime() const
{
	double Next = 0.0;

	for (const FModifierEntry& Entry : Entries)
	{
		if (Entry.ExpiresAt > 0.0 && (Next <= 0.0 || Entry.ExpiresAt < Next))
		{
			Next = Entry.ExpiresAt;
		}
	}

	return Next;
}

int32 FStatModifierContainer::FindEntry(const FObjectKey& SourceKey, const FName EntryKey) const
{
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		if (Entries[Index].Source == SourceKey && Entries[Index].EntryKey == EntryKey)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}
bool FStatModifierContainer::SetStacks(const UObject* Source, const FName EntryKey, const int32 Stacks)
{
	const int32 Index = FindEntry(FObjectKey(Source), EntryKey);

	if (Index == INDEX_NONE || Entries[Index].Stacks == Stacks)
	{
		return false;
	}

	Entries[Index].Stacks = Stacks;
	return true;
}