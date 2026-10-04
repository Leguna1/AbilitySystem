#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ModifierTypes.h"
#include "ArrowLinkHazard.generated.h"

class AArrowBase;
class UNiagaraSystem;

/** What a volley hands its hazard. */
USTRUCT(BlueprintType)
struct FArrowLinkHazardParams
{
	GENERATED_BODY()

	/** Applied to, and refreshed on, combatants touching a line. */
	UPROPERTY(BlueprintReadWrite, Category = "Hazard")
	TArray<FStatusApplication> Statuses;

	/** Seconds the lines last, counted from the first landed arrow. */
	UPROPERTY(BlueprintReadWrite, Category = "Hazard")
	float Duration = 4.0f;

	/** Landed arrows within this distance of each other are linked. */
	UPROPERTY(BlueprintReadWrite, Category = "Hazard")
	float LinkDistance = 400.0f;

	/** Seconds to wait for arrows to land. Later landings are ignored. */
	UPROPERTY(BlueprintReadWrite, Category = "Hazard")
	float CollectionTime = 3.0f;
};

/**
 * Ground hazard fed by a volley. Each arrow that lands adds a point on the
 * ground, linked to its nearest neighbours within LinkDistance. Combatants
 * touching a line receive Statuses, refreshed every PulseInterval.
 * Lasts Duration from the first landing, then destroys itself.
 */
UCLASS(Blueprintable)
class ABILITYSYSTEM_API AArrowLinkHazard : public AActor
{
	GENERATED_BODY()

public:
	AArrowLinkHazard();

	void InitializeHazard(const FArrowLinkHazardParams& InParams);

	/** Feeds this arrow's landing into the hazard (its current flight only). */
	void RegisterProjectile(AArrowBase* Arrow);

	UFUNCTION(BlueprintPure, Category = "Hazard")
	const TArray<FVector>& GetLinkPoints() const { return Points; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Hazard")
	void OnLinkPointAdded(FVector Location);

	UFUNCTION(BlueprintImplementableEvent, Category = "Hazard")
	void OnLinkCreated(FVector Start, FVector End);

	/** A combatant touched a line this pulse (good for a zap sound or effect). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hazard")
	void OnActorAffected(AActor* AffectedActor);

	UFUNCTION(BlueprintImplementableEvent, Category = "Hazard")
	void OnHazardEnded();

	/** Each new point links to at most this many existing points. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard|Links", meta = (ClampMin = "1"))
	int32 MaxLinksPerPoint = 2;

	/** Thickness of a line for touch tests. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard|Links", meta = (ClampMin = "1.0"))
	float LineRadius = 40.0f;

	/** Lines are drawn and tested this far above the ground under each landing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard|Links", meta = (ClampMin = "0.0"))
	float LineHeight = 40.0f;

	/** How far below a landing to look for the ground. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard|Links", meta = (ClampMin = "0.0"))
	float GroundTraceDistance = 600.0f;

	/** Seconds between touch tests. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard|Effect", meta = (ClampMin = "0.05"))
	float PulseInterval = 0.2f;

	/** Object channel of what the lines affect. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard|Effect")
	TEnumAsByte<ECollisionChannel> AffectedObjectChannel = ECC_Pawn;

	/** Beam effect per line; receives the line's ends in the parameters below. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard|Visuals")
	TObjectPtr<UNiagaraSystem> LineEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard|Visuals")
	FName LineStartParameter = TEXT("BeamStart");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard|Visuals")
	FName LineEndParameter = TEXT("BeamEnd");

	/** Optional effect at each landed point. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard|Visuals")
	TObjectPtr<UNiagaraSystem> PointEffect;

	/** Draws the touch capsules every pulse. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard|Debug")
	bool bDrawDebugLines = false;
	
	/** Every landing links to its nearest point regardless of LinkDistance, so all arrows form one web. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard|Links")
	bool bAlwaysLinkNearest = true;

	/** No line is ever longer than this, even the nearest link. 0 = no limit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard|Links", meta = (ClampMin = "0.0"))
	float MaxLineLength = 1500.0f;
	
	/** The archer who fired the volley is affected by their own lines too. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard|Effect")
	bool bAffectsInstigator = false;

private:
	struct FLinkSegment
	{
		FVector Start;
		FVector End;
	};

	UFUNCTION()
	void HandleArrowHit(AArrowBase* Arrow, AActor* HitActor, float Damage, const FHitResult& Hit);

	void AddLinkPoint(const FVector& LandingLocation);
	void CreateSegment(const FVector& Start, const FVector& End);
	void Pulse();
	void HandleCollectionTimeout();
	void StopCollecting();
	void UnbindArrow(AArrowBase* Arrow);
	void EndHazard();

	FArrowLinkHazardParams Params;
	TArray<FVector> Points;
	TArray<FLinkSegment> Segments;

	/** Arrows still expected to land, with the flight serial they were registered for. */
	TMap<TWeakObjectPtr<AArrowBase>, uint32> TrackedArrows;

	FTimerHandle CollectionTimerHandle;
	FTimerHandle PulseTimerHandle;
	FTimerHandle LifetimeTimerHandle;

	bool bEnded = false;
};