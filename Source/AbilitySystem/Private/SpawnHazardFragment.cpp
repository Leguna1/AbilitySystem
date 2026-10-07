#include "SpawnHazardFragment.h"

#include "AbilityComponent.h"
#include "AbilitySystemTags.h"
#include "ArrowLinkHazard.h"
#include "GameFramework/Character.h"
#include "RangedAttackAbility.h"
#include "Misc/DataValidation.h"

void USpawnHazardFragment::OnProjectilesReleased(URangedAttackAbility& Ability, TConstArrayView<AArrowBase*> Projectiles) const
{
	UWorld* World = Ability.GetWorld();
	ACharacter* Character = Ability.GetOwningCharacter();
	const UAbilityComponent* Component = Ability.GetAbilityComponent();

	if (!HazardClass ||
		!IsValid(World) ||
		!IsValid(Character) ||
		!IsValid(Component) ||
		!Component->HasAllOwnerTags(RequiredOwnerTags))
	{
		return;
	}

	// The hazard's own defaults are the base; the firing ability's rank and modifiers scale them.
	const AArrowLinkHazard* Definition = HazardClass.GetDefaultObject();

	FArrowLinkHazardParams Params;
	Ability.ResolveStatusSpecs(Definition->GetStatusSpecs(), Params.Statuses);
	Params.Duration = FMath::Max(Ability.GetModifiedFloat(AbilitySystemTags::Stat_HazardDuration, Definition->GetBaseDuration()), 0.1f);
	Params.LinkDistance = FMath::Max(Ability.GetModifiedFloat(AbilitySystemTags::Stat_LinkDistance, Definition->GetBaseLinkDistance()), 0.0f);
	Params.CollectionTime = LandingWindow;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Character;
	SpawnParams.Instigator = Character;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Where the actor sits doesn't matter: its lines are built from where the arrows land.
	AArrowLinkHazard* Hazard = World->SpawnActor<AArrowLinkHazard>(HazardClass, Character->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);

	if (!IsValid(Hazard))
	{
		return;
	}

	Hazard->InitializeHazard(Params);

	for (AArrowBase* Arrow : Projectiles)
	{
		Hazard->RegisterProjectile(Arrow);
	}
}

#if WITH_EDITOR
EDataValidationResult USpawnHazardFragment::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!HazardClass)
	{
		Context.AddError(NSLOCTEXT("SpawnHazard", "NoClass", "Spawn Hazard has no Hazard Class, so nothing spawns."));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif