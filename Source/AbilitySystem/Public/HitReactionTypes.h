#pragma once

#include "CoreMinimal.h"
#include "HitReactionTypes.generated.h"

/** How a character reacts to a hit, weakest to strongest. */
UENUM(BlueprintType)
enum class EHitReactionType : uint8
{
	None,

	/** Small additive twitch. Never interrupts. */
	Flinch,

	/** Interrupts the current action briefly. */
	Stagger,

	/** Stagger plus a push away from the hit. */
	Knockback,

	/** Knocked to the ground, then gets up. */
	Knockdown
};

/** Side the hit came from, relative to the receiver's facing. */
UENUM(BlueprintType)
enum class EHitDirection : uint8
{
	Front,
	Back,
	Left,
	Right
};

/** How hard a hit is. Authored on attacks and carried by the payload. */
USTRUCT(BlueprintType)
struct FHitImpact
{
	GENERATED_BODY()

	FHitImpact() = default;

	FHitImpact(const EHitReactionType InReaction, const float InPoiseDamage)
		: Reaction(InReaction)
		, PoiseDamage(InPoiseDamage)
	{
	}

	/** Reaction when this hit breaks the receiver's poise. Hits that don't break it only flinch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	EHitReactionType Reaction = EHitReactionType::None;

	/** Poise this hit removes, before the attacker's Stat.PoiseDamage modifiers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact", meta = (ClampMin = "0.0"))
	float PoiseDamage = 0.0f;

	/** Breaks poise however much is left. Super armor and stagger immunity still stop it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	bool bForceReaction = false;

	/** Horizontal push speed for Knockback and Knockdown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact", meta = (ClampMin = "0.0"))
	float KnockbackSpeed = 0.0f;

	/** Upward push speed for Knockback and Knockdown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact", meta = (ClampMin = "0.0"))
	float KnockbackLift = 0.0f;
};

/** What the receiver decided. Broadcast through OnHitReaction. */
USTRUCT(BlueprintType)
struct FHitReactionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Hit Reaction")
	EHitReactionType Reaction = EHitReactionType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Hit Reaction")
	EHitDirection Direction = EHitDirection::Front;

	/** Flattened world direction the hit travelled, from the attacker toward the receiver. */
	UPROPERTY(BlueprintReadOnly, Category = "Hit Reaction")
	FVector HitDirection = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "Hit Reaction")
	float KnockbackSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Hit Reaction")
	float KnockbackLift = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Hit Reaction")
	TObjectPtr<AActor> Instigator;
};