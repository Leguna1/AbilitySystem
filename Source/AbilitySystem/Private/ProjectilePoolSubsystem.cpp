#include "ProjectilePoolSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "ProjectileBase.h"
#include "ProjectileDataAsset.h"

UProjectilePoolSubsystem* UProjectilePoolSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = IsValid(GEngine)
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	return IsValid(World) ? World->GetSubsystem<UProjectilePoolSubsystem>() : nullptr;
}

bool UProjectilePoolSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

AProjectileBase* UProjectilePoolSubsystem::AcquireProjectile(UProjectileDataAsset* Data, AActor* NewOwner, APawn* NewInstigator)
{
	if (!IsValid(Data) || !Data->ProjectileClass)
	{
		return nullptr;
	}

	AProjectileBase* Projectile = TakeAvailable(Data->ProjectileClass);

	if (!IsValid(Projectile))
	{
		return nullptr;
	}

	Projectile->SetOwner(NewOwner);
	Projectile->SetInstigator(NewInstigator);

	if (!Projectile->ActivateFromPool(Data))
	{
		Projectile->ReturnToPool();
		return nullptr;
	}

	return Projectile;
}

void UProjectilePoolSubsystem::PrewarmProjectiles(UProjectileDataAsset* Data, const int32 Count)
{
	if (!IsValid(Data) || !Data->ProjectileClass)
	{
		return;
	}

	int32 Existing = 0;

	for (const AProjectileBase* Projectile : AllProjectiles)
	{
		if (IsValid(Projectile) && Projectile->GetClass() == Data->ProjectileClass)
		{
			++Existing;
		}
	}

	for (int32 Index = Existing; Index < Count; ++Index)
	{
		if (AProjectileBase* Projectile = SpawnPooledProjectile(Data->ProjectileClass))
		{
			AvailableProjectiles.Add(Projectile);
		}
	}
}

void UProjectilePoolSubsystem::Deinitialize()
{
	// The world destroys the actors itself; just drop our references.
	AllProjectiles.Reset();
	AvailableProjectiles.Reset();

	Super::Deinitialize();
}

AProjectileBase* UProjectilePoolSubsystem::SpawnPooledProjectile(const TSubclassOf<AProjectileBase> ProjectileClass)
{
	UWorld* World = GetWorld();

	if (!IsValid(World) || !ProjectileClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AProjectileBase* Projectile = World->SpawnActor<AProjectileBase>(ProjectileClass, FTransform::Identity, SpawnParameters);

	if (!IsValid(Projectile))
	{
		return nullptr;
	}

	Projectile->OnReadyToRecycle.AddDynamic(this, &UProjectilePoolSubsystem::HandleProjectileReadyToRecycle);
	Projectile->ResetForPool();

	AllProjectiles.Add(Projectile);
	return Projectile;
}

AProjectileBase* UProjectilePoolSubsystem::TakeAvailable(const TSubclassOf<AProjectileBase> ProjectileClass)
{
	for (int32 Index = AvailableProjectiles.Num() - 1; Index >= 0; --Index)
	{
		AProjectileBase* Candidate = AvailableProjectiles[Index];

		if (!IsValid(Candidate))
		{
			AvailableProjectiles.RemoveAtSwap(Index);
			continue;
		}

		// Exact class: a Blueprint child may carry components a plain projectile doesn't have.
		if (Candidate->GetClass() == ProjectileClass)
		{
			AvailableProjectiles.RemoveAtSwap(Index);
			return Candidate;
		}
	}

	return SpawnPooledProjectile(ProjectileClass);
}

void UProjectilePoolSubsystem::HandleProjectileReadyToRecycle(AProjectileBase* Projectile)
{
	if (IsValid(Projectile) && AllProjectiles.Contains(Projectile))
	{
		AvailableProjectiles.AddUnique(Projectile);
	}
}
