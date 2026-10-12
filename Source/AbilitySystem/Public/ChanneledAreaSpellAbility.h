#pragma once

#include "CoreMinimal.h"
#include "OffensiveAbilityBase.h"
#include "ChanneledAreaSpellAbility.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;

/**
 * A channeled area spell. At cast start it marks the ground under the target (the area
 * stays there), shows a telegraph, and starts the cooldown. On the release event every
 * hostile inside the circle is hit. Ending before the release (e.g. staggered) cancels it.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class ABILITYSYSTEM_API UChanneledAreaSpellAbility : public UOffensiveAbilityBase
{
	GENERATED_BODY()

public:
	virtual bool CanActivateAbility_Implementation() const override;
	virtual void ActivateAbility_Implementation() override;
	virtual void OnAnimationEvent_Implementation(FGameplayTag EventTag) override;
	virtual void OnAbilityEnded_Implementation(EAbilityEndReason EndReason) override;

	/** Ground point the area is centered on, locked at cast start. */
	UFUNCTION(BlueprintPure, Category = "Ability|Spell")
	FVector GetAreaCenter() const { return AreaCenter; }

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Spell")
	FAbilityRankedFloat DamageByRank = FAbilityRankedFloat(40.0f);

	/** Anim event at the end of the channel, where the area goes off. Required. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Spell")
	FGameplayTag ReleaseEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Spell", meta = (ClampMin = "1", Units = "cm"))
	float AreaRadius = 350.0f;

	/** How high above the ground the area reaches. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Spell", meta = (ClampMin = "1", Units = "cm"))
	float AreaHeight = 300.0f;

	/** Shown on the ground for the whole channel. Gets a float User parameter (below) set to the radius. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Spell|Visuals")
	TObjectPtr<UNiagaraSystem> TelegraphEffect;

	/** Float User parameter on the telegraph that receives Area Radius, so one effect fits any size. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Spell|Visuals")
	FName TelegraphRadiusParameter = TEXT("Radius");

	/** Spawned at the area center when it goes off. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Spell|Visuals")
	TObjectPtr<UNiagaraSystem> ReleaseEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Spell|Visuals")
	TObjectPtr<USoundBase> ReleaseSound;

	/** The channel started and the area is marked. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|Spell")
	void OnChannelStarted(FVector Center, float Radius);

	/** The area went off. TargetsHit = how many accepted the hit. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|Spell")
	void OnAreaReleased(FVector Center, float Radius, int32 TargetsHit);

	/** The ability ended before the release (staggered, killed, cancelled). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|Spell")
	void OnChannelInterrupted(FVector Center);

private:
	FVector FindGroundBelow(const AActor* SpellTarget) const;
	void ReleaseArea();
	void RemoveTelegraph();

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> TelegraphComponent;

	FVector AreaCenter = FVector::ZeroVector;
	float ResolvedDamage = 0.0f;
	bool bReleased = false;
};