#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "UObject/Interface.h"
#include "ModifierTypes.h"
#include "HitReactionTypes.h"
#include "PayloadReceiver.generated.h"

/**
 * What a hit delivers to its target. Starts with damage and the context needed
 * to attribute it; designed to grow (status tags, knockback, effect handles)
 * without changing the interface signature.
 */
USTRUCT(BlueprintType)
struct FAbilityPayload
{
	GENERATED_BODY()

	/** Damage to apply. */
	UPROPERTY(BlueprintReadWrite, Category = "Payload")
	float Damage = 0.0f;

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