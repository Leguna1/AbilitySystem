#include "HitCountStatusFragment.h"

#include "AbilityComponent.h"
#include "CombatantComponent.h"
#include "GameFramework/Character.h"
#include "ProjectileBase.h"
#include "RangedAttackAbility.h"

#if WITH_EDITOR
#include "AbilityValidation.h"
#include "Misc/DataValidation.h"
#endif

/* -------------------- Fragment -------------------- */

void UHitCountStatusFragment::OnProjectilesReleased(URangedAttackAbility& Ability, TConstArrayView<AProjectileBase*> Projectiles) const
{
	UWorld* World = Ability.GetWorld();
	ACharacter* Character = Ability.GetOwningCharacter();
	const UAbilityComponent* Component = Ability.GetAbilityComponent();

	// Per release, fewer arrows than hits needed can never reach the count.
	if (Statuses.IsEmpty() ||
		Projectiles.IsEmpty() ||
		(!bCountAcrossReleases && Projectiles.Num() < HitsRequired) ||
		!IsValid(World) ||
		!IsValid(Character) ||
		!IsValid(Component) ||
		!Component->HasAllOwnerTags(RequiredOwnerTags))
	{
		return;
	}

	TArray<FStatusApplication> Resolved;
	Ability.ResolveStatusSpecs(Statuses, Resolved);

	if (Resolved.IsEmpty())
	{
		return;
	}

	// Still counting from the previous release: keep that count going.
	if (bCountAcrossReleases)
	{
		AHitCountTracker* Running = SharedTracker.Get();

		if (IsValid(Running) && !Running->IsActorBeingDestroyed() && Running->GetInstigator() == Character)
		{
			Running->ExtendTracker(Resolved, CountingWindow);

			for (AProjectileBase* Projectile : Projectiles)
			{
				Running->RegisterProjectile(Projectile);
			}

			return;
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Character;
	SpawnParams.Instigator = Character;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AHitCountTracker* Tracker = World->SpawnActor<AHitCountTracker>(AHitCountTracker::StaticClass(), Character->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);

	if (!IsValid(Tracker))
	{
		return;
	}

	Tracker->InitializeTracker(HitsRequired, Resolved, CountingWindow);

	if (bCountAcrossReleases)
	{
		SharedTracker = Tracker;
	}

	for (AProjectileBase* Projectile : Projectiles)
	{
		Tracker->RegisterProjectile(Projectile);
	}
}

#if WITH_EDITOR
EDataValidationResult UHitCountStatusFragment::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (Statuses.IsEmpty())
	{
		Context.AddWarning(NSLOCTEXT("HitCountStatus", "NoStatuses", "Status On Repeated Hits has no Statuses, so it does nothing."));
	}

	return CombineDataValidationResults(Result,
		AbilityValidation::ValidateStatusSpecs(Statuses, TEXT("Statuses"), Context));
}
#endif

/* -------------------- Tracker -------------------- */

AHitCountTracker::AHitCountTracker()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	SetHidden(true);
}

void AHitCountTracker::InitializeTracker(const int32 InHitsRequired, const TArray<FStatusApplication>& InStatuses, const float Window)
{
	HitsRequired = FMath::Max(InHitsRequired, 1);
	Statuses = InStatuses;
	SetLifeSpan(FMath::Max(Window, 0.5f));
}

void AHitCountTracker::ExtendTracker(const TArray<FStatusApplication>& InStatuses, const float Window)
{
	Statuses = InStatuses;
	SetLifeSpan(FMath::Max(Window, 0.5f));
}

void AHitCountTracker::RegisterProjectile(AProjectileBase* Arrow)
{
	if (!IsValid(Arrow))
	{
		return;
	}

	TrackedArrows.Add(Arrow, Arrow->GetFlightSerial());
	Arrow->OnProjectileHit.AddUniqueDynamic(this, &AHitCountTracker::HandleArrowHit);
}

void AHitCountTracker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindAll();
	Super::EndPlay(EndPlayReason);
}

void AHitCountTracker::HandleArrowHit(AProjectileBase* Arrow, AActor* HitActor, float Damage, const FHitResult& Hit)
{
	const uint32* RegisteredSerial = TrackedArrows.Find(Arrow);

	// A pooled arrow fired again by something else is no longer part of this release.
	if (!RegisteredSerial || !IsValid(Arrow) || *RegisteredSerial != Arrow->GetFlightSerial())
	{
		if (IsValid(Arrow))
		{
			Arrow->OnProjectileHit.RemoveDynamic(this, &AHitCountTracker::HandleArrowHit);
		}

		TrackedArrows.Remove(Arrow);
		return;
	}

	// Ground hits don't count. A piercing arrow keeps reporting, one hit per target it passes.
	UCombatantComponent* Combatant = IsValid(HitActor) ? HitActor->FindComponentByClass<UCombatantComponent>() : nullptr;

	if (!IsValid(Combatant) || Combatant->IsDead() || HitActor == GetInstigator())
	{
		return;
	}

	int32& Count = HitCounts.FindOrAdd(HitActor);
	++Count;

	if (Count < HitsRequired)
	{
		return;
	}
	
	// Every Hits Required hits: the count starts over, so a long volley can trigger again.
	Count = 0;

	for (const FStatusApplication& Status : Statuses)
	{
		Combatant->ApplyStatus(Status, GetInstigator());
	}
}

void AHitCountTracker::UnbindAll()
{
	for (const TPair<TWeakObjectPtr<AProjectileBase>, uint32>& Entry : TrackedArrows)
	{
		if (AProjectileBase* Arrow = Entry.Key.Get())
		{
			Arrow->OnProjectileHit.RemoveDynamic(this, &AHitCountTracker::HandleArrowHit);
		}
	}

	TrackedArrows.Reset();
}
