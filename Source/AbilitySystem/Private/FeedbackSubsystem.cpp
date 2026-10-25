// FeedbackSubsystem.cpp
#include "FeedbackSubsystem.h"

#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

#include "Camera/CameraShakeBase.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "FeedbackSet.h"
#include "FeedbackSettings.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

UFeedbackSubsystem* UFeedbackSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	return IsValid(World) ? World->GetSubsystem<UFeedbackSubsystem>() : nullptr;
}

bool UFeedbackSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UFeedbackSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Loaded once at world start so the first miss never hitches.
	const UFeedbackSettings* Settings = GetDefault<UFeedbackSettings>();

	DefaultSet = Settings->DefaultFeedbackSet.LoadSynchronous();
	MaxSameCuePerFrame = Settings->MaxSameCuePerFrame;
	EffectCullDistanceSquared = FMath::Square(Settings->EffectCullDistance);
}

const FFeedbackCue* UFeedbackSubsystem::FindCue(const UFeedbackSet* Set, const FGameplayTag& Tag, const UFeedbackSet* FallbackSet) const
{
	if (const FFeedbackCue* Cue = UFeedbackSet::FindInChain(Set, Tag))
	{
		return Cue;
	}

	if (FallbackSet != Set)
	{
		if (const FFeedbackCue* Cue = UFeedbackSet::FindInChain(FallbackSet, Tag))
		{
			return Cue;
		}
	}

	return Set != DefaultSet ? UFeedbackSet::FindInChain(DefaultSet, Tag) : nullptr;
}

bool UFeedbackSubsystem::ResolveEffect(const UFeedbackSet* Set, const FGameplayTag& Tag, const FFeedbackCueContext& Context, FFeedbackEffect& OutEffect, const UFeedbackSet* FallbackSet) const
{
	const FFeedbackCue* Cue = FindCue(Set, Tag, FallbackSet);

	if (Cue == nullptr)
	{
		return false;
	}

	OutEffect = Cue->GetEffectFor(ResolveSurface(Context.Hit, Context.HitComponent.Get()));
	return true;
}

bool UFeedbackSubsystem::PlayCue(const UFeedbackSet* Set, const FGameplayTag& Tag, const FFeedbackCueContext& Context, const UFeedbackSet* FallbackSet)
{
	FFeedbackEffect Resolved;
	return ResolveEffect(Set, Tag, Context, Resolved, FallbackSet) && PlayEffect(Resolved, Context, Tag);
}

bool UFeedbackSubsystem::PlayEffect(const FFeedbackEffect& Effect, const FFeedbackCueContext& Context, const FGameplayTag& CueForLimits, const bool bPlaySound, const bool bPlayEffect)
{
	UWorld* World = GetWorld();

	const bool bSound = bPlaySound && Effect.HasSound();
	const bool bVisual = bPlayEffect && (Effect.HasEffect() || Effect.HasCameraShake());

	if (!IsValid(World) || (!bSound && !bVisual))
	{
		return false;
	}

	if (CueForLimits.IsValid() && !ConsumeFrameBudget(CueForLimits))
	{
		return false;
	}

	if (bSound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, Effect.Sound, Context.Location, Effect.Volume, Effect.RollPitch());
	}

	if (bVisual && Effect.HasEffect() && !IsCulled(Context.Location))
	{
		UNiagaraComponent* Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			Effect.Effect,
			Context.Location,
			ResolveRotation(Effect, Context),
			Effect.EffectScale,
			true,
			true,
			ENCPoolMethod::AutoRelease);

		if (IsValid(Spawned))
		{
			for (const TPair<FName, float>& Parameter : Context.NiagaraFloats)
			{
				Spawned->SetVariableFloat(Parameter.Key, Parameter.Value);
			}
		}
	}

	if (bVisual && Effect.HasCameraShake())
	{
		UGameplayStatics::PlayWorldCameraShake(World, Effect.CameraShake, Context.Location, 0.0f, Effect.CameraShakeRadius);
	}

	return true;
}

EPhysicalSurface UFeedbackSubsystem::ResolveSurface(const FHitResult& Hit, const UPrimitiveComponent* HitComponent)
{
	// Traces that ask for it carry the exact material (landscape layers, complex collision).
	if (const UPhysicalMaterial* HitMaterial = Hit.PhysMaterial.Get())
	{
		if (HitMaterial->SurfaceType != SurfaceType_Default)
		{
			return HitMaterial->SurfaceType;
		}
	}

	// Overlaps don't: use the struck component's material (Phys Material Override on capsules and meshes).
	const UPrimitiveComponent* Component = IsValid(HitComponent) ? HitComponent : Hit.GetComponent();

	if (IsValid(Component))
	{
		if (const FBodyInstance* Body = Component->GetBodyInstance())
		{
			if (const UPhysicalMaterial* BodyMaterial = Body->GetSimplePhysicalMaterial())
			{
				return BodyMaterial->SurfaceType;
			}
		}
	}

	return SurfaceType_Default;
}

FVector UFeedbackSubsystem::ResolveStrikeLocation(const AActor* HitActor, const FHitResult& Hit, const FVector& StrikeOrigin)
{
	if (!FVector(Hit.ImpactPoint).IsNearlyZero())
	{
		return FVector(Hit.ImpactPoint);
	}

	if (!IsValid(HitActor))
	{
		return StrikeOrigin;
	}

	if (const UPrimitiveComponent* TargetBody = Cast<UPrimitiveComponent>(HitActor->GetRootComponent()))
	{
		FVector ClosestPoint;

		// > 0: found a point on the body. 0 = the origin is inside it; -1 = no collision.
		if (TargetBody->GetClosestPointOnCollision(StrikeOrigin, ClosestPoint) > 0.0f)
		{
			return ClosestPoint;
		}
	}

	return (StrikeOrigin + HitActor->GetActorLocation()) * 0.5f;
}

bool UFeedbackSubsystem::ConsumeFrameBudget(const FGameplayTag& Tag)
{
	if (MaxSameCuePerFrame <= 0)
	{
		return true;
	}

	if (CountedFrame != GFrameCounter)
	{
		CountedFrame = GFrameCounter;
		PlaysThisFrame.Reset();
	}

	int32& Count = PlaysThisFrame.FindOrAdd(Tag);

	if (Count >= MaxSameCuePerFrame)
	{
		return false;
	}

	++Count;
	return true;
}

bool UFeedbackSubsystem::IsCulled(const FVector& Location) const
{
	const UWorld* World = GetWorld();

	if (EffectCullDistanceSquared <= 0.0f || !IsValid(World))
	{
		return false;
	}

	bool bAnyCamera = false;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();

		if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
		{
			continue;
		}

		FVector ViewLocation;
		FRotator ViewRotation;
		PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

		bAnyCamera = true;

		if (FVector::DistSquared(ViewLocation, Location) <= EffectCullDistanceSquared)
		{
			return false;
		}
	}

	// No camera to measure from (dedicated server, early startup): never cull.
	return bAnyCamera;
}

FRotator UFeedbackSubsystem::ResolveRotation(const FFeedbackEffect& Effect, const FFeedbackCueContext& Context) const
{
	const FVector Normal = FVector(Context.Hit.ImpactNormal);

	if (Effect.bAlignToSurface && !Normal.IsNearlyZero())
	{
		return Normal.Rotation();
	}

	return Context.Rotation;
}
