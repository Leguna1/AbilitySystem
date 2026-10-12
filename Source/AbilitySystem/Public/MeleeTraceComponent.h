#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MeleeTraceComponent.generated.h"

class UAnimSequenceBase;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMeleeTraceWindowSignature, UAnimSequenceBase*, SourceAnimation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FMeleeTraceHitSignature, AActor*, HitActor, const FHitResult&, Hit, FName, ShapeName, UAnimSequenceBase*, SourceAnimation);

/** One body part that can hit: a capsule between two bones, or a sphere at one. */
USTRUCT(BlueprintType)
struct FMeleeTraceShape
{
	GENERATED_BODY()

	/** Name the montage's Melee Trace notify refers to, e.g. RightFist. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee Trace")
	FName ShapeName;

	/** Bone or socket at one end of the part, e.g. lowerarm_r. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee Trace")
	FName StartSocket;

	/** Bone or socket at the other end, e.g. hand_r. None = a sphere at Start Socket. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee Trace")
	FName EndSocket;

	/** Thickness of the part. Large creatures need large values. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee Trace", meta = (ClampMin = "1.0"))
	float Radius = 30.0f;
};

/**
 * Hits with body parts (fists, feet, tails) instead of a weapon. A Melee Trace
 * notify on the montage turns parts on for the impact frames; each frame they
 * are swept from their previous pose to the current one, so fast swings never
 * skip a target. Each target is hit once per window.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ABILITYSYSTEM_API UMeleeTraceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMeleeTraceComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Turns the named parts on. The first part turned on starts a new window (targets can be hit again). */
	UFUNCTION(BlueprintCallable, Category = "Melee Trace")
	void BeginTrace(const TArray<FName>& ShapeNames, UAnimSequenceBase* SourceAnimation);

	UFUNCTION(BlueprintCallable, Category = "Melee Trace")
	void EndTrace(const TArray<FName>& ShapeNames);

	UFUNCTION(BlueprintCallable, Category = "Melee Trace")
	void EndAllTraces();

	UFUNCTION(BlueprintPure, Category = "Melee Trace")
	bool IsTracing() const { return !ActiveShapes.IsEmpty(); }

	/** A window opened: the first part turned on. */
	UPROPERTY(BlueprintAssignable, Category = "Melee Trace")
	FMeleeTraceWindowSignature OnTraceStarted;

	/** A window closed: the last part turned off. */
	UPROPERTY(BlueprintAssignable, Category = "Melee Trace")
	FMeleeTraceWindowSignature OnTraceEnded;

	UPROPERTY(BlueprintAssignable, Category = "Melee Trace")
	FMeleeTraceHitSignature OnMeleeHit;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	virtual void BeginPlay() override;

	/** This character's hitting parts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee Trace", meta = (TitleProperty = "ShapeName"))
	TArray<FMeleeTraceShape> Shapes;

	/** What the parts can hit. Characters' capsules are Pawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee Trace")
	TEnumAsByte<ECollisionChannel> TraceObjectChannel = ECC_Pawn;

	/** Skip characters on the same team (pack members). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee Trace")
	bool bIgnoreFriendly = true;

	/** Draws the swept parts while tracing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee Trace|Debug")
	bool bDrawDebug = false;

private:
	struct FActiveShape
	{
		int32 ShapeIndex = INDEX_NONE;
		TArray<FVector> PreviousPoints;
		TWeakObjectPtr<UAnimSequenceBase> Source;
	};

	struct FPendingHit
	{
		TWeakObjectPtr<AActor> Actor;
		FHitResult Hit;
		FName ShapeName;
		TWeakObjectPtr<UAnimSequenceBase> Source;
	};

	void SamplePoints(const FMeleeTraceShape& Shape, TArray<FVector>& OutPoints) const;
	void TraceShape(FActiveShape& Active, TArray<FPendingHit>& OutHits);
	bool ShouldHit(const AActor* Actor) const;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> Mesh;

	TArray<FActiveShape> ActiveShapes;
	TArray<TWeakObjectPtr<AActor>> HitActorsThisWindow;
	TWeakObjectPtr<UAnimSequenceBase> WindowSource;
};