#include "AbilityDescription.h"

#include "AbilityTypes.h"
#include "Engine/DataAsset.h"
#include "ModifierTypes.h"
#include "UObject/UnrealType.h"

namespace AbilityDescription
{
	namespace
	{
		/** Deep enough for ability -> effect -> status spec -> modifier, shallow enough to stay cheap. */
		constexpr int32 MaxDepth = 6;

		FText FormatNumber(const double Value)
		{
			FNumberFormattingOptions Options;
			Options.MinimumFractionalDigits = 0;
			Options.MaximumFractionalDigits = 2;
			return FText::AsNumber(Value, &Options);
		}

		FText FormatPercent(const double Fraction)
		{
			FNumberFormattingOptions Options;
			Options.MinimumFractionalDigits = 0;
			Options.MaximumFractionalDigits = 1;
			return FText::AsPercent(Fraction, &Options);
		}

		/** DamageByRank -> Damage. Authored name, so Blueprint variables don't carry their internal suffix. */
		FString MakeTokenName(const FProperty* Property)
		{
			FString Name = Property->GetAuthoredName().Replace(TEXT(" "), TEXT(""));
			Name.RemoveFromEnd(TEXT("ByRank"));
			return Name;
		}

		struct FTokenCollector
		{
			FTokenCollector(const int32 InRank, TMap<FString, FText>& InTokens)
				: Rank(InRank), Tokens(InTokens)
			{
			}

			void VisitObject(const UObject* Object, const int32 Depth)
			{
				if (!IsValid(Object) || Depth > MaxDepth || Visited.Contains(Object))
				{
					return;
				}

				Visited.Add(Object);
				VisitProperties(Object->GetClass(), Object, Depth);
			}

		private:
			int32 Rank = 1;
			TMap<FString, FText>& Tokens;
			TSet<const UObject*> Visited;

			void VisitProperties(const UStruct* Type, const void* Container, const int32 Depth)
			{
				for (TFieldIterator<FProperty> It(Type); It; ++It)
				{
					const FProperty* Property = *It;

					// Only what designers author; runtime state (rank, timers) is transient and not editable.
					if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Deprecated))
					{
						continue;
					}

					const FString Name = MakeTokenName(Property);

					for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
					{
						VisitValue(Property, Property->ContainerPtrToValuePtr<void>(Container, Index), Name, Depth);
					}
				}
			}

			void VisitValue(const FProperty* Property, const void* Value, const FString& Name, const int32 Depth)
			{
				if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					const UScriptStruct* Struct = StructProperty->Struct;

					if (Struct == FAbilityRankedFloat::StaticStruct())
					{
						AddNumber(Name, static_cast<const FAbilityRankedFloat*>(Value)->Get(Rank));
					}
					else if (Struct == FAbilityRankedInt::StaticStruct())
					{
						AddNumber(Name, static_cast<const FAbilityRankedInt*>(Value)->Get(Rank));
					}
					else if (Struct == FStatModifier::StaticStruct())
					{
						AddModifier(*static_cast<const FStatModifier*>(Value));
					}
					else if (Depth < MaxDepth)
					{
						// Any other struct (status specs, hit impact...): look inside.
						VisitProperties(Struct, Value, Depth + 1);
					}

					return;
				}

				if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
				{
					FScriptArrayHelper Helper(ArrayProperty, Value);

					for (int32 Index = 0; Index < Helper.Num(); ++Index)
					{
						VisitValue(ArrayProperty->Inner, Helper.GetRawPtr(Index), Name, Depth);
					}

					return;
				}

				if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
				{
					// Class references (TSubclassOf) point at other things entirely; skip them.
					if (Property->IsA<FClassProperty>())
					{
						return;
					}

					const UObject* Object = ObjectProperty->GetObjectPropertyValue(Value);

					if (!IsValid(Object))
					{
						return;
					}

					// Pieces authored inside the ability (triggers, effects, fragments) and data assets it uses (projectile data).
					const bool bAuthoredInside =
						ObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference | CPF_PersistentInstance) ||
						Object->GetClass()->HasAnyClassFlags(CLASS_EditInlineNew | CLASS_DefaultToInstanced);

					if (bAuthoredInside || Object->IsA<UDataAsset>())
					{
						VisitObject(Object, Depth + 1);
					}

					return;
				}

				if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
				{
					if (NumericProperty->IsFloatingPoint())
					{
						AddNumber(Name, NumericProperty->GetFloatingPointPropertyValue(Value));
					}
					else if (NumericProperty->IsInteger() && !NumericProperty->IsEnum())
					{
						AddNumber(Name, static_cast<double>(NumericProperty->GetSignedIntPropertyValue(Value)));
					}
				}
			}

			/** Name, or Name2, Name3... if taken. */
			FString MakeUnique(const FString& Name) const
			{
				FString Unique = Name;

				for (int32 Suffix = 2; Tokens.Contains(Unique); ++Suffix)
				{
					Unique = FString::Printf(TEXT("%s%d"), *Name, Suffix);
				}

				return Unique;
			}

			void AddNumber(const FString& Name, const double Number)
			{
				if (Name.IsEmpty())
				{
					return;
				}

				const FString Unique = MakeUnique(Name);
				Tokens.Add(Unique, FormatNumber(Number));
				Tokens.Add(Unique + TEXT("Percent"), FormatPercent(Number));
			}

			void AddModifier(const FStatModifier& Modifier)
			{
				if (!Modifier.Stat.IsValid())
				{
					return;
				}

				// Stat.AttackPower.Min -> AttackPowerMin
				FString Name = Modifier.Stat.GetTagName().ToString();
				Name.RemoveFromStart(TEXT("Stat."));
				Name.ReplaceInline(TEXT("."), TEXT(""));

				const float Magnitude = FMath::Abs(Modifier.Magnitude.Get(Rank));

				Tokens.Add(MakeUnique(Name), Modifier.Operation == EModifierOperation::Percent
					? FormatPercent(Magnitude)
					: FormatNumber(Magnitude));
			}
		};
	}

	void CollectTokens(const UObject* Source, const int32 Rank, TMap<FString, FText>& OutTokens)
	{
		FTokenCollector Collector(FMath::Max(Rank, 1), OutTokens);
		Collector.VisitObject(Source, 0);
	}
}
