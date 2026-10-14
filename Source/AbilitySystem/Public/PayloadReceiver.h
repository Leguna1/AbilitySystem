#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "UObject/Interface.h"
#include "ModifierTypes.h"
#include "HitReactionTypes.h"
#include "PayloadReceiver.generated.h"

/** Which power a hit scales with and which defense reduces it. */
UENUM(BlueprintType)
enum class EDamageType : uint8
{
	/** Scales with Attack Power, reduced by Armor. Swords, arrows, fists. */
	Physical UMETA(DisplayName = "Physical"),

	/** Scales with Magic Power, reduced by Magic Resist. Spells and hexes. */
	Magic UMETA(DisplayName = "Magic")
};

/**
 * What a hit delivers to its target. Starts with damage and the context needed
 * to attribute it; designed to grow (status tags, knockback, effect handles)
 * without changing the interface signature.
 */
USTRUCT(BlueprintType)
struct FAbilityPayload
{
	GENERATED_BODY()

	/**
	 * How hard the hit is. If the instigator has the power for DamageType, this is a multiple
	 * of that power (1 = 100%, rolled between its Min and Max). Otherwise it is flat damage.
	 * The receiver then applies its Armor or Magic Resist, and Damage Taken.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Payload")
	float Damage = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Payload")
	EDamageType DamageType = EDamageType::Physical;

	/** Actor that caused this payload (the shooter), for attribution. May be null. */
	UPROPERTY(BlueprintReadWrite, Category = "Payload")
	TObjectPtr<AActor> Instigator = nullptr;

	/** The source that physically delivered it (e.g. the arrow). May be null. */
	UPROPERTY(BlueprintReadWrite, Category = "Payload")
	TObjectPtr<AActor> Causer = nullptr;

	/** Impact details, when available. */
	UPROPERTY(BlueprintReadWrite, Category = "Payload")
	FHitResult Hit;
	
	/** Tags of the ability that delivered this. Empty for non-ability sources. */
	UPROPERTY(BlueprintReadWrite, Category = "Payload")
	FGameplayTagContainer SourceAbilityTags;

	/** Statuses applied to the receiver if it accepts the payload. */
	UPROPERTY(BlueprintReadWrite, Category = "Payload")
	TArray<FStatusApplication> Statuses;
	
	/** How hard this hit is. The receiver decides the actual reaction. */
	UPROPERTY(BlueprintReadWrite, Category = "Payload")
	FHitImpact Impact;
	
	// Future growth (uncomment/add as systems land):
	// FGameplayTagContainer EffectTags;   // statuses to apply
	// FVector KnockbackImpulse;           // directional force
};
/** What each hit of an attack carries to its target besides damage. Built once by the attacking ability. */
USTRUCT(BlueprintType)
struct FHitSpec
{
	GENERATED_BODY()

	/** Tags of the attacking ability, so the receiver and passives know what hit them. */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	FGameplayTagContainer SourceAbilityTags;

	/** Which power the damage scales with and which defense reduces it. */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	EDamageType DamageType = EDamageType::Physical;

	/** How hard the hit is (poise damage, reaction). */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	FHitImpact Impact;

	/** Statuses the hit applies if it's accepted. */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	TArray<FStatusApplication> Statuses;

	void ApplyTo(FAbilityPayload& Payload) const
	{
		Payload.SourceAbilityTags = SourceAbilityTags;
		Payload.DamageType = DamageType;
		Payload.Impact = Impact;
		Payload.Statuses = Statuses;
	}
};

UINTERFACE(BlueprintType, MinimalAPI)
class UPayloadReceiver : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by actors that can receive an ability payload (take damage / react
 * to a hit). Deliberately separate from ITargetableInterface: being aimable and
 * being hittable are different concerns with different implementers.
 */
class ABILITYSYSTEM_API IPayloadReceiver
{
	GENERATED_BODY()

public:
	/**
	 * Deliver a payload to this actor. Implementation decides how to react
	 * (subtract health, apply status, ignore if invulnerable, etc.).
	 * Return true if the payload was accepted/applied.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Payload")
	bool ReceivePayload(const FAbilityPayload& Payload);
	virtual bool ReceivePayload_Implementation(const FAbilityPayload& Payload) { return false; }
	
	
};