// FeedbackComponent.cpp
#include "FeedbackComponent.h"

#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

#include "Components/AudioComponent.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Components/SceneComponent.h"
#include "FeedbackSet.h"
#include "FeedbackSubsystem.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

UFeedbackComponent::UFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAllLoops(true);
	Super::EndPlay(EndPlayReason);
}

/* -------------------- Set -------------------- */

UFeedbackSet* UFeedbackComponent::GetActiveSet() const
{
	return IsValid(OverrideSet) ? OverrideSet.Get() : FeedbackSet.Get();
}

UFeedbackSet* UFeedbackComponent::GetFallbackSet() const
{
	return IsValid(OverrideSet) ? FeedbackSet.Get() : nullptr;
}

void UFeedbackComponent::SetFeedbackSet(UFeedbackSet* NewSet)
{
	if (FeedbackSet == NewSet)
	{
		return;
	}

	const UFeedbackSet* Before = GetActiveSet();
	FeedbackSet = NewSet;

	if (GetActiveSet() != Before)
	{
		RefreshLoops();
	}
}

void UFeedbackComponent::SetOverrideSet(UFeedbackSet* NewOverride)
{
	if (OverrideSet == NewOverride)
	{
		return;
	}

	const UFeedbackSet* Before = GetActiveSet();
	OverrideSet = NewOverride;

	if (GetActiveSet() != Before)
	{
		RefreshLoops();
	}
}

/* -------------------- One-shot -------------------- */

FFeedbackCueContext UFeedbackComponent::MakeOwnerContext() const
{
	FFeedbackCueContext Context;

	if (const AActor* OwnerActor = GetOwner())
	{
		Context.Location = OwnerActor->GetActorLocation();
		Context.Rotation = OwnerActor->GetActorRotation();
		Context.Instigator = OwnerActor->GetInstigator();
	}

	return Context;
}

bool UFeedbackComponent::PlayCue(const FGameplayTag Cue)
{
	return PlayCueWithContext(Cue, MakeOwnerContext());
}

bool UFeedbackComponent::PlayCueAt(const FGameplayTag Cue, const FVector Location, const FRotator Rotation)
{
	FFeedbackCueContext Context = MakeOwnerContext();
	Context.Location = Location;
	Context.Rotation = Rotation;

	return PlayCueWithContext(Cue, Context);
}

bool UFeedbackComponent::PlayCueWithContext(const FGameplayTag& Cue, const FFeedbackCueContext& Context) const
{
	UFeedbackSubsystem* Subsystem = UFeedbackSubsystem::Get(this);
	return IsValid(Subsystem) && Subsystem->PlayCue(GetActiveSet(), Cue, Context, GetFallbackSet());
}

/* -------------------- Loops -------------------- */

FFeedbackLoop* UFeedbackComponent::FindLoop(const FGameplayTag& Cue)
{
	return Loops.FindByPredicate([&Cue](const FFeedbackLoop& Loop) { return Loop.Cue == Cue; });
}

const FFeedbackLoop* UFeedbackComponent::FindLoop(const FGameplayTag& Cue) const
{
	return Loops.FindByPredicate([&Cue](const FFeedbackLoop& Loop) { return Loop.Cue == Cue; });
}

bool UFeedbackComponent::StartLoop(const FGameplayTag Cue, USceneComponent* AttachTo)
{
	if (!Cue.IsValid())
	{
		return false;
	}

	if (FFeedbackLoop* Existing = FindLoop(Cue))
	{
		++Existing->RefCount;
		return IsValid(Existing->Effect) || IsValid(Existing->Sound);
	}

	FFeedbackLoop& Loop = Loops.AddDefaulted_GetRef();
	Loop.Cue = Cue;
	Loop.RefCount = 1;
	Loop.AttachTo = AttachTo;

	// Kept even when nothing defines it yet, so a set assigned later can start it.
	return SpawnLoopVisuals(Loop, true);
}

void UFeedbackComponent::StopLoop(const FGameplayTag Cue)
{
	FFeedbackLoop* Loop = FindLoop(Cue);

	if (Loop == nullptr || --Loop->RefCount > 0)
	{
		return;
	}

	ClearLoop(Cue, false);
}

void UFeedbackComponent::ClearLoop(const FGameplayTag Cue, const bool bImmediate)
{
	const int32 Index = Loops.IndexOfByPredicate([&Cue](const FFeedbackLoop& Loop) { return Loop.Cue == Cue; });

	if (Index == INDEX_NONE)
	{
		return;
	}

	DespawnLoopVisuals(Loops[Index], bImmediate, !bImmediate);
	Loops.RemoveAtSwap(Index);
}

void UFeedbackComponent::StopAllLoops(const bool bImmediate)
{
	for (FFeedbackLoop& Loop : Loops)
	{
		DespawnLoopVisuals(Loop, bImmediate, !bImmediate);
	}

	Loops.Reset();
}

bool UFeedbackComponent::IsLoopActive(const FGameplayTag Cue) const
{
	return FindLoop(Cue) != nullptr;
}

void UFeedbackComponent::SetLoopFloat(const FGameplayTag Cue, const FName Parameter, const float Value)
{
	FFeedbackLoop* Loop = FindLoop(Cue);

	if (Loop == nullptr || Parameter.IsNone())
	{
		return;
	}

	Loop->NiagaraFloats.Add(Parameter, Value);

	if (IsValid(Loop->Effect))
	{
		Loop->Effect->SetVariableFloat(Parameter, Value);
	}
}

bool UFeedbackComponent::SpawnLoopVisuals(FFeedbackLoop& Loop, const bool bPlayStartEffect) const
{
	const UFeedbackSubsystem* Subsystem = UFeedbackSubsystem::Get(this);
	const FFeedbackCue* Cue = IsValid(Subsystem) ? Subsystem->FindCue(GetActiveSet(), Loop.Cue, GetFallbackSet()) : nullptr;

	if (Cue == nullptr)
	{
		return false;
	}

	USceneComponent* AttachComponent = Loop.AttachTo.Get();

	if (!IsValid(AttachComponent) && IsValid(GetOwner()))
	{
		AttachComponent = GetOwner()->GetRootComponent();
	}

	if (!IsValid(AttachComponent))
	{
		return false;
	}

	const FName Socket = !Cue->AttachSocket.IsNone() && AttachComponent->DoesSocketExist(Cue->AttachSocket)
		? Cue->AttachSocket
		: NAME_None;

	const FFeedbackEffect& Effect = Cue->Effect;

	Loop.bDetachWhenStopped = Cue->bDetachWhenStopped;
	Loop.FadeOutTime = Cue->FadeOutTime;
	Loop.StopEffect = Cue->StopEffect;
	Loop.Socket = Socket;
	Loop.AttachTo = AttachComponent;

	if (bPlayStartEffect)
	{
		PlayAtLoop(Loop, Cue->StartEffect);
	}

	if (IsValid(Cue->OverlayMaterial))
	{
		if (UMeshComponent* Mesh = Cast<UMeshComponent>(AttachComponent))
		{
			Loop.PreviousOverlay = Mesh->GetOverlayMaterial();
			Loop.AppliedOverlay = Cue->OverlayMaterial;
			Mesh->SetOverlayMaterial(Cue->OverlayMaterial);
		}
	}

	if (Effect.HasEffect())
	{
		// Pooled: stopping deactivates it and the pool takes it back once its particles finish.
		Loop.Effect = UNiagaraFunctionLibrary::SpawnSystemAttached(
			Effect.Effect,
			AttachComponent,
			Socket,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			Effect.EffectScale,
			EAttachLocation::SnapToTarget,
			true,
			ENCPoolMethod::AutoRelease);
	}

	if (IsValid(Loop.Effect))
	{
		for (const TPair<FName, float>& Parameter : Loop.NiagaraFloats)
		{
			Loop.Effect->SetVariableFloat(Parameter.Key, Parameter.Value);
		}
	}

	if (Effect.HasSound())
	{
		Loop.Sound = UGameplayStatics::SpawnSoundAttached(
			Effect.Sound,
			AttachComponent,
			Socket,
			FVector::ZeroVector,
			EAttachLocation::SnapToTarget,
			true,
			Effect.Volume,
			Effect.RollPitch());
	}

	return IsValid(Loop.Effect) || IsValid(Loop.Sound) || IsValid(Loop.AppliedOverlay) || !Cue->StartEffect.IsEmpty();
}

void UFeedbackComponent::PlayAtLoop(const FFeedbackLoop& Loop, const FFeedbackEffect& Effect) const
{
	UFeedbackSubsystem* Subsystem = UFeedbackSubsystem::Get(this);
	const USceneComponent* AttachComponent = Loop.AttachTo.Get();

	if (Effect.IsEmpty() || !IsValid(Subsystem) || !IsValid(AttachComponent))
	{
		return;
	}

	FFeedbackCueContext Context = MakeOwnerContext();
	Context.Location = AttachComponent->GetSocketLocation(Loop.Socket);
	Context.Rotation = AttachComponent->GetSocketRotation(Loop.Socket);

	Subsystem->PlayEffect(Effect, Context, Loop.Cue);
}

void UFeedbackComponent::DespawnLoopVisuals(FFeedbackLoop& Loop, const bool bImmediate, const bool bPlayStopEffect) const
{
	if (bPlayStopEffect)
	{
		PlayAtLoop(Loop, Loop.StopEffect);
	}

	// Only undo our own overlay: another loop may have put its own on since.
	if (IsValid(Loop.AppliedOverlay))
	{
		UMeshComponent* Mesh = Cast<UMeshComponent>(Loop.AttachTo.Get());

		if (IsValid(Mesh) && Mesh->GetOverlayMaterial() == Loop.AppliedOverlay)
		{
			Mesh->SetOverlayMaterial(Loop.PreviousOverlay);
		}
	}

	Loop.AppliedOverlay = nullptr;
	Loop.PreviousOverlay = nullptr;

	if (IsValid(Loop.Effect))
	{
		if (bImmediate)
		{
			Loop.Effect->DeactivateImmediate();
		}
		else
		{
			if (Loop.bDetachWhenStopped)
			{
				Loop.Effect->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			}

			Loop.Effect->Deactivate();
		}
	}

	if (IsValid(Loop.Sound))
	{
		if (bImmediate || Loop.FadeOutTime <= 0.0f)
		{
			Loop.Sound->Stop();
		}
		else
		{
			Loop.Sound->FadeOut(Loop.FadeOutTime, 0.0f);
		}
	}

	// Both now belong to their pool / auto-destroy; never touch them again.
	Loop.Effect = nullptr;
	Loop.Sound = nullptr;
}

void UFeedbackComponent::RefreshLoops()
{
	for (FFeedbackLoop& Loop : Loops)
	{
		DespawnLoopVisuals(Loop, false, false);
		SpawnLoopVisuals(Loop, false);
	}
}
