#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ModifierTypes.h"
#include "PayloadReceiver.h"
#include "ArrowFieldHazard.generated.h"

class AProjectileBase;
class UNiagaraComponent;
class UNiagaraSystem;

/** What a volley hands its field. Filled by the Spawn Volley Field fragment. */
USTRUCT(BlueprintType)
struct FArrowFieldParams
{
	GENERATED_BODY()

	/** Applied to, and refreshed on, everyone standing in the field. */
	UPROPERTY(BlueprintReadWrite, Category = "Field")
	TArray<FStatusApplication> Statuses;

	/** Ground point the field is centred on. Ignored when bCenterOnFirstLanding. */
	UPROPERTY(BlueprintReadWrite, Category = "Field")
	FVector Center = FVector::ZeroVector;

	/** No known centre (not a volley): the field forms where the first arrow lands. */
	UPROPERTY(BlueprintReadWrite, Category = "Field")
	bool bCenterOnFirstLanding = false;

	UPROPERTY(BlueprintReadWrite, Category = "Field")
	float Radius = 400.0f;

	/** Seconds the field lasts, counted from the first landing. */
	UPROPERTY(BlueprintReadWrite, Category = "Field")
	float Duration = 4.0f;

	/** Seconds to wait for arrows to land. Later landings are ignored. */
	UPROPERTY(BlueprintReadWrite, Category = "Field")
	float CollectionTime = 4.0f;

	/** When the rain is over, hit everyone in the field once with ThunderclapHit (poise, no damage). */
	UPROPERTY(BlueprintReadWrite, Category = "Field")
	bool bThunderclap = false;

	UPROPERTY(BlueprintReadWrite, Category = "Field")
	FHitSpec ThunderclapHit;
};

/**
 * A circular ground field left by a volley (Storm Volley). It forms when the first arrow lands,
 * applies Statuses to everyone inside every pulse (a status's own duration is how long it lingers
 * after they step out), and fades after Duration.
 *
 * Optional Thunderclap: once every arrow has landed, one poise burst staggers everyone in the field.
 *
 * Make a Blueprint of this for the look: Field Effect (Niagara, gets a "Radius" float), or build
 * your own in On Field Started / On Field Ended / On Thunderclap.
 */
UCLASS(Blueprintable)
class ABILITYSYSTEM_API AArrowFieldHazard : public AActor
{
	GENERATED_BODY()

public:
	AArrowFieldHazard();

	void InitializeField(const FArrowFieldParams& InParams);

	/** Feeds this arrow's landing into the field (its current flight only). */
	void RegisterProjectile(AProjectileBase* Arrow);

	const TArray<FStatusEffectSpec>& GetStatusSpecs() const { return Statuses; }
	float GetBaseDuration() const { return Duration; }
	float GetRadiusScale() const { return RadiusScale; }
	float GetFallbackRadius() const { return FallbackRadius; }

	UFUNCTION(BlueprintPure, Category = "Field")
	FVector GetFieldCenter() const { return FieldCenter; }

	UFUNCTION(BlueprintPure, Category = "Field")
	float GetFieldRadius() const { return Params.Radius; }

	UFUNCTION(BlueprintPure, Category = "Field")
	bool IsFieldActive() const { return bFieldActive; }

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Applied to whoever stands in the field, refreshed every pulse. Values are read at the firing
	 * ability's rank. Example: Status.Crippled, Stat.MoveSpeed -40% (Percent), Duration 1.5.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field|Effect")
	TArray<FStatusEffectSpec> Statuses;

	/** Seconds the field lasts from the first landing. Passives can change it (Stat.HazardDuration). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field|Effect", meta = (ClampMin = "0.1"))
	float Duration = 4.0f;

	/** Field radius = the volley's landing radius (after Stat.Radius) x this. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field|Shape", meta = (ClampMin = "0.1"))
	float RadiusScale = 1.0f;

	/** Radius when the ability isn't a volley and has no landing radius. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field|Shape", meta = (ClampMin = "1.0"))
	float FallbackRadius = 400.0f;

	/** Enemies this far above or below the field's centre still count as inside. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field|Shape", meta = (ClampMin = "1.0"))
	float HalfHeight = 200.0f;

	/** Seconds between checks for who is inside. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field|Effect", meta = (ClampMin = "0.05"))
	float PulseInterval = 0.25f;

	/** Object channel of what the field affects. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field|Effect")
	TEnumAsByte<ECollisionChannel> AffectedObjectChannel = ECC_Pawn;

	/** The archer is affected by their own field. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field|Effect")
	bool bAffectsInstigator = false;

	/** Allies of the archer (the companion) are affected too. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field|Effect")
	bool bAffectsFriendlies = false;

	/** Looping ground effect, spawned when the field forms. Receives the radius in Radius Parameter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field|Visuals")
	TObjectPtr<UNiagaraSystem> FieldEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field|Visuals")
	FName RadiusParameter = TEXT("Radius");

	/** One-shot effect for the Thunderclap, at the field's centre. Receives the radius too. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field|Visuals")
	TObjectPtr<UNiagaraSystem> ThunderclapEffect;

	/** Seconds the Field Effect gets to fade out (deactivated) before the actor is removed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field|Visuals", meta = (ClampMin = "0.0"))
	float FadeOutTime = 1.0f;

	/** Draws the field's edge every pulse. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field|Debug")
	bool bDrawDebugField = false;

	UFUNCTION(BlueprintImplementableEvent, Category = "Field")
	void OnFieldStarted(FVector Center, float Radius);

	/** Someone was inside this pulse (a sound, a slow-down effect on them...). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Field")
	void OnActorAffected(AActor* AffectedActor);

	UFUNCTION(BlueprintImplementableEvent, Category = "Field")
	void OnThunderclap(FVector Center, float Radius);

	/** Called when the field stops working; the actor stays for Fade Out Time. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Field")
	void OnFieldEnded();

private:
	UFUNCTION()
	void HandleArrowHit(AProjectileBase* Arrow, AActor* HitActor, float Damage, const FHitResult& Hit);

	void StartField(const FVector& FirstLanding);
	void Pulse();
	void Thunderclap();
	void HandleCollectionTimeout();
	void HandleRainFinished();
	void StopCollecting();
	void UnbindArrow(AProjectileBase* Arrow);
	void EndField();

	/** Living combatants inside the field that it may affect. */
	void GatherAffectedActors(TArray<AActor*>& OutActors) const;
	bool CanAffect(const AActor* Actor) const;

	FArrowFieldParams Params;
	FVector FieldCenter = FVector::ZeroVector;

	/** Arrows still expected to land, with the flight serial they were registered for. */
	TMap<TWeakObjectPtr<AProjectileBase>, uint32> TrackedArrows;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> FieldEffectComponent;

	FTimerHandle CollectionTimerHandle;
	FTimerHandle PulseTimerHandle;
	FTimerHandle LifetimeTimerHandle;

	bool bFieldActive = false;
	bool bRainFinished = false;
	bool bEnded = false;
};
