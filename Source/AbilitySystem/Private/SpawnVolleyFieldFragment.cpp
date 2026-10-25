#include "SpawnVolleyFieldFragment.h"

#include "AbilityComponent.h"
#include "AbilitySystemTags.h"
#include "ArrowFieldHazard.h"
#include "GameFramework/Character.h"
#include "ProjectileBarrageAbility.h"
#include "RangedAttackAbility.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

void USpawnVolleyFieldFragment::OnProjectilesReleased(URangedAttackAbility& Ability, TConstArrayView<AProjectileBase*> Projectiles) const
{
	UWorld* World = Ability.GetWorld();
	ACharacter* Character = Ability.GetOwningCharacter();
	const UAbilityComponent* Component = Ability.GetAbilityComponent();

	if (!FieldClass ||
		Projectiles.IsEmpty() ||
		!IsValid(World) ||
		!IsValid(Character) ||
		!IsValid(Component) ||
		!Component->HasAllOwnerTags(RequiredOwnerTags))
	{
		return;
	}

	// The field's own defaults are the base; the firing ability's rank and modifiers scale them.
	const AArrowFieldHazard* Definition = FieldClass.GetDefaultObject();

	FArrowFieldParams FieldParams;
	Ability.ResolveStatusSpecs(Definition->GetStatusSpecs(), FieldParams.Statuses);
	FieldParams.Duration = FMath::Max(Ability.GetModifiedFloat(AbilitySystemTags::Stat_HazardDuration, Definition->GetBaseDuration()), 0.1f);
	FieldParams.CollectionTime = LandingWindow;
	FieldParams.ImpactGroup = Ability.GetImpactGroup();

	// A volley knows where it rains and how wide (Stat.Radius already applied).
	if (const UProjectileBarrageAbility* Volley = Cast<UProjectileBarrageAbility>(&Ability))
	{
		FieldParams.Center = Volley->GetBarrageCenter();
		FieldParams.Radius = Volley->GetImpactRadius() * Definition->GetRadiusScale();
	}
	else
	{
		FieldParams.bCenterOnFirstLanding = true;
		FieldParams.Radius = Definition->GetFallbackRadius();
	}

	if (FieldParams.Radius <= 1.0f)
	{
		FieldParams.Radius = Definition->GetFallbackRadius();
	}

	FieldParams.bThunderclap = !ThunderclapRequiredOwnerTags.IsEmpty() && Component->HasAllOwnerTags(ThunderclapRequiredOwnerTags);

	if (FieldParams.bThunderclap)
	{
		FieldParams.ThunderclapHit = Ability.BuildHitSpec();
		FieldParams.ThunderclapHit.Impact.PoiseDamage *= ThunderclapPoiseMultiplier;
		FieldParams.ThunderclapHit.Impact.Reaction = ThunderclapReaction;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Character;
	SpawnParams.Instigator = Character;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FVector SpawnLocation = FieldParams.bCenterOnFirstLanding ? Character->GetActorLocation() : FieldParams.Center;
	AArrowFieldHazard* Field = World->SpawnActor<AArrowFieldHazard>(FieldClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);

	if (!IsValid(Field))
	{
		return;
	}

	Field->InitializeField(FieldParams);

	for (AProjectileBase* Projectile : Projectiles)
	{
		Field->RegisterProjectile(Projectile);
	}
}

#if WITH_EDITOR
EDataValidationResult USpawnVolleyFieldFragment::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!FieldClass)
	{
		Context.AddError(NSLOCTEXT("SpawnVolleyField", "NoClass", "Spawn Volley Field has no Field Class, so nothing spawns."));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
