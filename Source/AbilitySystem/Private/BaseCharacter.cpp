#include "BaseCharacter.h"

#include "AbilityComponent.h"
#include "ActiveAbility.h"
#include "AttributeComponent.h"
#include "CombatantComponent.h"
#include "PayloadReceiver.h"
#include "ResourceComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

void ABaseCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Every component exists by now, whether a C++ child or a Blueprint added it.
	AbilityComponent = FindComponentByClass<UAbilityComponent>();
	CombatantComponent = FindComponentByClass<UCombatantComponent>();
	ResourceComponent = FindComponentByClass<UResourceComponent>();
	AttributeComponent = FindComponentByClass<UAttributeComponent>();
}

void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(CombatantComponent))
	{
		CombatantComponent->OnDied.AddDynamic(this, &ABaseCharacter::HandleCombatantDied);
		CombatantComponent->OnPayloadReceived.AddDynamic(this, &ABaseCharacter::HandlePayloadReceivedForVoice);
	}

	if (IsValid(AbilityComponent))
	{
		AbilityComponent->AbilityCommittedEvent.AddDynamic(this, &ABaseCharacter::HandleAbilityCommitted);
	}
}

void ABaseCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(CombatantComponent))
	{
		CombatantComponent->OnDied.RemoveDynamic(this, &ABaseCharacter::HandleCombatantDied);
		CombatantComponent->OnPayloadReceived.RemoveDynamic(this, &ABaseCharacter::HandlePayloadReceivedForVoice);
	}

	if (IsValid(AbilityComponent))
	{
		AbilityComponent->AbilityCommittedEvent.RemoveDynamic(this, &ABaseCharacter::HandleAbilityCommitted);
	}

	Super::EndPlay(EndPlayReason);
}

void ABaseCharacter::GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	const USkeletalMeshComponent* MeshComponent = GetMesh();

	OutLocation = IsValid(MeshComponent) && MeshComponent->DoesSocketExist(EyeSocketName)
		? MeshComponent->GetSocketLocation(EyeSocketName)
		: GetPawnViewLocation();

	OutRotation = FRotator(0.0f, GetActorRotation().Yaw, 0.0f);
}

FVector ABaseCharacter::GetTargetAimLocation_Implementation() const
{
	const USkeletalMeshComponent* MeshComponent = GetMesh();

	return IsValid(MeshComponent) && MeshComponent->DoesSocketExist(AimSocketName)
		? MeshComponent->GetSocketLocation(AimSocketName)
		: GetActorLocation();
}

bool ABaseCharacter::IsDead() const
{
	return IsValid(CombatantComponent) && CombatantComponent->IsDead();
}

void ABaseCharacter::HandleCombatantDied(AActor* Killer)
{
	HandleCharacterDied(Killer);
}

void ABaseCharacter::HandleCharacterDied(AActor* Killer)
{
	PlayDeathFeedback(Killer);
	OnCharacterDied(Killer);
}

/* -------------------- Death -------------------- */

void ABaseCharacter::PlayDeathFeedback(AActor* Killer)
{
	// Whatever was mid-swing stops: its montage would fight the death montage.
	if (IsValid(AbilityComponent))
	{
		AbilityComponent->CancelActiveAbility(false);
	}

	UAnimMontage* DeathMontage = SelectDeathMontage(Killer);
	UAnimInstance* AnimInstance = IsValid(GetMesh()) ? GetMesh()->GetAnimInstance() : nullptr;

	if (IsValid(DeathMontage) && IsValid(AnimInstance))
	{
		AnimInstance->StopAllMontages(0.1f);
		AnimInstance->Montage_Play(DeathMontage);
	}

	// Death cuts off any effort or hurt line still playing.
	PlayVoice(DeathSound);
}

UAnimMontage* ABaseCharacter::SelectDeathMontage_Implementation(AActor* Killer) const
{
	TArray<UAnimMontage*> Valid;

	for (UAnimMontage* Montage : DeathMontages)
	{
		if (IsValid(Montage))
		{
			Valid.Add(Montage);
		}
	}

	return Valid.IsEmpty() ? nullptr : Valid[FMath::RandRange(0, Valid.Num() - 1)];
}

/* -------------------- Voice -------------------- */

void ABaseCharacter::PlayVoice(USoundBase* Sound)
{
	if (!IsValid(Sound))
	{
		return;
	}

	// One voice at a time: a new line replaces the old one instead of stacking.
	if (IsValid(VoiceAudio))
	{
		VoiceAudio->Stop();
	}

	USkeletalMeshComponent* MeshComponent = GetMesh();
	USceneComponent* AttachTo = IsValid(MeshComponent) ? static_cast<USceneComponent*>(MeshComponent) : GetRootComponent();
	const FName Socket = IsValid(MeshComponent) && MeshComponent->DoesSocketExist(VoiceSocketName) ? VoiceSocketName : NAME_None;

	VoiceAudio = UGameplayStatics::SpawnSoundAttached(Sound, AttachTo, Socket);
}

bool ABaseCharacter::TryVoiceCooldown(double& NextAllowedTime, const float Chance, const float CooldownMin, const float CooldownMax) const
{
	const UWorld* World = GetWorld();

	if (!IsValid(World) || World->GetTimeSeconds() < NextAllowedTime || FMath::FRand() >= Chance)
	{
		return false;
	}

	// A fresh random cooldown every time, so the lines never fall into a rhythm.
	NextAllowedTime = World->GetTimeSeconds() + FMath::FRandRange(CooldownMin, FMath::Max(CooldownMin, CooldownMax));
	return true;
}

void ABaseCharacter::HandleAbilityCommitted(FGameplayTag AbilityId, UActiveAbility* Ability)
{
	if (!IsValid(EffortSound) || IsDead())
	{
		return;
	}

	if (IsValid(Ability) && Ability->GetAbilityTags().HasAny(EffortExcludedAbilityTags))
	{
		return;
	}

	if (TryVoiceCooldown(NextEffortTime, EffortSoundChance, EffortCooldownMin, EffortCooldownMax))
	{
		PlayVoice(EffortSound);
	}
}

void ABaseCharacter::HandlePayloadReceivedForVoice(const FAbilityPayload& Payload, const float DamageApplied)
{
	if (!IsValid(HurtSound) || DamageApplied <= 0.0f || !IsValid(ResourceComponent))
	{
		return;
	}

	const float CurrentHealth = ResourceComponent->GetResourceValue(EResourceType::Health, EResourceValueType::Current);
	const float MaxHealth = ResourceComponent->GetResourceValue(EResourceType::Health, EResourceValueType::Max);

	// The killing blow gets the death sound instead.
	if (CurrentHealth <= 0.0f || IsDead())
	{
		return;
	}

	if (MaxHealth > 0.0f && DamageApplied / MaxHealth < HurtMinDamageFraction)
	{
		return;
	}

	if (TryVoiceCooldown(NextHurtTime, HurtSoundChance, HurtCooldownMin, HurtCooldownMax))
	{
		PlayVoice(HurtSound);
	}
}

#if WITH_EDITOR
EDataValidationResult ABaseCharacter::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (EffortCooldownMax < EffortCooldownMin)
	{
		Context.AddError(NSLOCTEXT("Character", "EffortCooldown", "Effort Cooldown Max is smaller than Min."));
		Result = EDataValidationResult::Invalid;
	}

	if (HurtCooldownMax < HurtCooldownMin)
	{
		Context.AddError(NSLOCTEXT("Character", "HurtCooldown", "Hurt Cooldown Max is smaller than Min."));
		Result = EDataValidationResult::Invalid;
	}

	for (const UAnimMontage* Montage : DeathMontages)
	{
		if (IsValid(Montage) && Montage->bEnableAutoBlendOut)
		{
			Context.AddWarning(FText::Format(NSLOCTEXT("Character", "DeathBlendOut",
				"Death montage {0} has Enable Auto Blend Out on: the body will stand back up when it ends."),
				FText::FromString(Montage->GetName())));
		}
	}

	return Result;
}
#endif
float ABaseCharacter::GetEdgeDistanceTo(const AActor* Other) const
{
	if (!IsValid(Other))
	{
		return TNumericLimits<float>::Max();
	}

	// Measured flat, so a tall golem and a short player aren't kept apart by their height difference.
	const float CenterDistance = FVector::Dist2D(GetActorLocation(), Other->GetActorLocation());

	const ACharacter* OtherCharacter = Cast<ACharacter>(Other);
	const float OtherRadius = IsValid(OtherCharacter)
		? OtherCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius()
		: 0.0f;

	return CenterDistance - GetCapsuleComponent()->GetScaledCapsuleRadius() - OtherRadius;
}