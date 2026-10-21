#include "CompanionNeedsComponent.h"

#include "AbilityComponent.h"
#include "AbilitySystemTags.h"
#include "BaseCharacter.h"
#include "CombatantComponent.h"
#include "CompanionAIController.h"
#include "CompanionCharacter.h"
#include "CompanionComponent.h"
#include "Engine/OverlapResult.h"
#include "ResourceComponent.h"
#include "TargetingComponent.h"
#include "TimerManager.h"

namespace
{
	constexpr int32 NumPurposes = static_cast<int32>(ECompanionPurpose::Finish) + 1;

	float GetFocusFraction(const AActor* Actor)
	{
		const UResourceComponent* Resources = IsValid(Actor) ? Actor->FindComponentByClass<UResourceComponent>() : nullptr;

		if (!IsValid(Resources))
		{
			return 1.0f;
		}

		const float MaxFocus = Resources->GetResourceValue(EResourceType::Focus, EResourceValueType::Max);

		return MaxFocus > 0.0f
			? FMath::Clamp(Resources->GetResourceValue(EResourceType::Focus, EResourceValueType::Current) / MaxFocus, 0.0f, 1.0f)
			: 1.0f;
	}
}

UCompanionNeedsComponent::UCompanionNeedsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	Needs.Init(0.0f, NumPurposes);
}

void UCompanionNeedsComponent::BeginPlay()
{
	Super::BeginPlay();

	Needs.Init(0.0f, NumPurposes);
	LastUpdateTime = GetWorldTime();

	GetWorld()->GetTimerManager().SetTimer(UpdateTimerHandle, this, &UCompanionNeedsComponent::UpdateNeeds, UpdateInterval, true);
}

void UCompanionNeedsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}

	UnbindFromLeader();
	Super::EndPlay(EndPlayReason);
}

/* ============================== Reading ============================== */

float UCompanionNeedsComponent::GetNeed(const ECompanionPurpose Purpose) const
{
	const int32 Index = static_cast<int32>(Purpose);
	return Needs.IsValidIndex(Index) ? Needs[Index] : 0.0f;
}

float UCompanionNeedsComponent::GetNeedMultiplier(const ECompanionPurpose Purpose) const
{
	return 1.0f + NeedInfluence * (2.0f * GetNeed(Purpose) - 1.0f);
}

ECompanionPurpose UCompanionNeedsComponent::GetStrongestNeed() const
{
	int32 Best = 0;

	for (int32 Index = 1; Index < Needs.Num(); ++Index)
	{
		if (Needs[Index] > Needs[Best])
		{
			Best = Index;
		}
	}

	return static_cast<ECompanionPurpose>(Best);
}

FString UCompanionNeedsComponent::DescribeNeeds() const
{
	FString Text;

	for (int32 Index = 0; Index < Needs.Num(); ++Index)
	{
		const ECompanionPurpose Purpose = static_cast<ECompanionPurpose>(Index);
		Text += FString::Printf(TEXT("%s %.2f%s"), *UEnum::GetDisplayValueAsText(Purpose).ToString(), Needs[Index],
			Index + 1 < Needs.Num() ? TEXT("  ") : TEXT(""));
	}

	return Text;
}

ACompanionAIController* UCompanionNeedsComponent::GetBrain() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return IsValid(Pawn) ? Cast<ACompanionAIController>(Pawn->GetController()) : nullptr;
}

double UCompanionNeedsComponent::GetWorldTime() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.0;
}

void UCompanionNeedsComponent::UpdateNeeds()
{
	const double Now = GetWorldTime();
	const float DeltaTime = static_cast<float>(FMath::Max(Now - LastUpdateTime, 0.0));
	LastUpdateTime = Now;

	const ACompanionCharacter* Companion = Cast<ACompanionCharacter>(GetOwner());
	ABaseCharacter* Leader = IsValid(Companion) ? Companion->GetLeader() : nullptr;
	const ACompanionAIController* Brain = GetBrain();

	TArray<float> Raw;
	Raw.Init(0.0f, NumPurposes);

	if (IsValid(Leader) && !Leader->IsDead() && IsValid(Brain) && !Companion->IsDead())
	{
		BindToLeader(Leader);

		Raw[static_cast<int32>(ECompanionPurpose::Protect)] = ReadProtect(*Brain, *Leader);
		Raw[static_cast<int32>(ECompanionPurpose::Damage)] = ReadDamage(*Brain, *Leader);
		Raw[static_cast<int32>(ECompanionPurpose::Control)] = ReadControl(*Brain, *Leader);
		Raw[static_cast<int32>(ECompanionPurpose::Support)] = ReadSupport(*Brain, *Leader);
		Raw[static_cast<int32>(ECompanionPurpose::Finish)] = ReadFinish(*Brain);
	}

	// Up fast, down slowly: quick to react, slow to forget.
	for (int32 Index = 0; Index < NumPurposes; ++Index)
	{
		const float Target = FMath::Clamp(Raw[Index], 0.0f, 1.0f);
		const float Rate = Target > Needs[Index] ? RiseRate : FallRate;
		Needs[Index] = FMath::FInterpConstantTo(Needs[Index], Target, DeltaTime, Rate);
	}

	// Fights teach the companion what this leader tends to need.
	if (IsValid(Leader) && IsValid(Brain) && Brain->IsInFight())
	{
		if (UCompanionComponent* Bond = Leader->FindComponentByClass<UCompanionComponent>())
		{
			Bond->ObserveNeeds(Needs, DeltaTime);
		}
	}
}

float UCompanionNeedsComponent::ReadProtect(const ACompanionAIController& Brain, const ABaseCharacter& Leader) const
{
	const int32 OnLeader = Brain.CountHostilesNear(Leader.GetActorLocation(), MeleeRadius);
	const float Crowded = ProtectPerMeleeEnemy * static_cast<float>(OnLeader);

	// Damage taken lately, as a share of max health.
	float Recent = 0.0f;
	const double Now = GetWorldTime();

	for (const TPair<double, float>& Hit : RecentLeaderDamage)
	{
		if (Now - Hit.Key <= RecentDamageWindow)
		{
			Recent += Hit.Value;
		}
	}

	const UResourceComponent* Resources = Leader.GetResourceComponent();
	const float MaxHealth = IsValid(Resources) ? Resources->GetResourceValue(EResourceType::Health, EResourceValueType::Max) : 0.0f;
	const float Hurt = MaxHealth > 0.0f ? RecentDamageWeight * Recent / MaxHealth : 0.0f;

	// Nothing on them and nothing hitting them: no matter how low they are, there's nobody to peel.
	const float Fragile = OnLeader > 0 ? 0.5f * (1.0f - FCompanionTacticContext::GetHealthFraction(&Leader)) : 0.0f;

	return FMath::Max3(Crowded, Hurt, Fragile);
}

float UCompanionNeedsComponent::ReadDamage(const ACompanionAIController& Brain, const ABaseCharacter& Leader)
{
	AActor* LeaderTarget = Brain.GetLeaderTarget();
	const double Now = GetWorldTime();

	if (LeaderTarget != FocusTarget.Get())
	{
		FocusTarget = LeaderTarget;
		FocusSince = Now;
	}

	if (!IsValid(LeaderTarget))
	{
		return 0.0f;
	}

	float Focus = FMath::Clamp(static_cast<float>(Now - FocusSince) / FocusBuildTime, 0.0f, 1.0f);

	if (!FocusTargetTags.IsEmpty() && FCompanionTacticContext::GetActorTags(LeaderTarget).HasAny(FocusTargetTags))
	{
		Focus += 0.3f;
	}

	const UAbilityComponent* Abilities = Leader.GetAbilityComponent();
	const UActiveAbility* Active = IsValid(Abilities) ? Abilities->GetActiveAbility() : nullptr;

	if (IsValid(Active) && !FocusAbilityTags.IsEmpty() && Active->GetAbilityTags().HasAny(FocusAbilityTags))
	{
		Focus += 0.3f;
	}

	return Focus;
}

float UCompanionNeedsComponent::ReadControl(const ACompanionAIController& Brain, const ABaseCharacter& Leader) const
{
	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return 0.0f;
	}

	const FVector LeaderLocation = Leader.GetActorLocation();

	// Near ones count fully; those just outside and heading in count half.
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CompanionNeedsControl), false, GetOwner());
	QueryParams.AddIgnoredActor(&Leader);
	TArray<FOverlapResult> Overlaps;

	World->OverlapMultiByObjectType(Overlaps, LeaderLocation, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(ControlRadius * 1.5f), QueryParams);

	TSet<const AActor*> Seen;
	float Pressure = 0.0f;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		const AActor* Hostile = Overlap.GetActor();

		if (!Brain.IsValidTarget(Hostile) || Seen.Contains(Hostile))
		{
			continue;
		}

		Seen.Add(Hostile);

		const FVector ToLeader = LeaderLocation - Hostile->GetActorLocation();

		if (ToLeader.SizeSquared() <= FMath::Square(ControlRadius))
		{
			Pressure += 1.0f;
		}
		else if (FVector::DotProduct(Hostile->GetVelocity(), ToLeader.GetSafeNormal()) > 100.0f)
		{
			Pressure += 0.5f;
		}
	}

	// One enemy is a duel, not a crowd.
	return FMath::Clamp((Pressure - 1.0f) / FMath::Max(static_cast<float>(CrowdSize - 1), 1.0f), 0.0f, 1.0f);
}

float UCompanionNeedsComponent::ReadSupport(const ACompanionAIController& Brain, const ABaseCharacter& Leader) const
{
	float Opening = 0.0f;

	if (Brain.IsInFight() && FightOpeningTime > 0.0f)
	{
		const float SinceStart = static_cast<float>(GetWorldTime() - Brain.GetFightStartTime());
		Opening = 0.8f * FMath::Clamp(1.0f - SinceStart / FightOpeningTime, 0.0f, 1.0f);
	}

	const float Health = FCompanionTacticContext::GetHealthFraction(&Leader);
	const float LowHealth = LowHealthFraction > 0.0f ? FMath::Clamp((LowHealthFraction - Health) / LowHealthFraction, 0.0f, 1.0f) : 0.0f;

	const float Focus = GetFocusFraction(&Leader);
	const float LowFocus = LowFocusFraction > 0.0f ? 0.5f * FMath::Clamp((LowFocusFraction - Focus) / LowFocusFraction, 0.0f, 1.0f) : 0.0f;

	return FMath::Max3(Opening, LowHealth, LowFocus);
}

float UCompanionNeedsComponent::ReadFinish(const ACompanionAIController& Brain) const
{
	for (AActor* Enemy : Brain.GetFightEnemies())
	{
		if (FCompanionTacticContext::GetHealthFraction(Enemy) < FinishHealthFraction)
		{
			return 1.0f;
		}

		const FGameplayTagContainer EnemyTags = FCompanionTacticContext::GetActorTags(Enemy);

		if ((bPoiseBrokenIsFinish && EnemyTags.HasTag(AbilitySystemTags::State_Staggered)) ||
			(!FinishTargetTags.IsEmpty() && EnemyTags.HasAny(FinishTargetTags)))
		{
			return 1.0f;
		}
	}

	return 0.0f;
}

/* ============================== Leader damage ============================== */

void UCompanionNeedsComponent::HandleLeaderHit(const FAbilityPayload& Payload, const float DamageApplied)
{
	if (DamageApplied <= 0.0f)
	{
		return;
	}

	const double Now = GetWorldTime();
	RecentLeaderDamage.Add(TPair<double, float>(Now, DamageApplied));

	// Only the window matters.
	RecentLeaderDamage.RemoveAll([this, Now](const TPair<double, float>& Hit)
	{
		return Now - Hit.Key > RecentDamageWindow;
	});
}

void UCompanionNeedsComponent::BindToLeader(ABaseCharacter* Leader)
{
	UCombatantComponent* LeaderCombatant = IsValid(Leader) ? Leader->GetCombatantComponent() : nullptr;

	if (LeaderCombatant == BoundLeaderCombatant.Get())
	{
		return;
	}

	UnbindFromLeader();

	if (IsValid(LeaderCombatant))
	{
		LeaderCombatant->OnPayloadReceived.AddDynamic(this, &UCompanionNeedsComponent::HandleLeaderHit);
		BoundLeaderCombatant = LeaderCombatant;
	}
}

void UCompanionNeedsComponent::UnbindFromLeader()
{
	if (UCombatantComponent* LeaderCombatant = BoundLeaderCombatant.Get())
	{
		LeaderCombatant->OnPayloadReceived.RemoveDynamic(this, &UCompanionNeedsComponent::HandleLeaderHit);
	}

	BoundLeaderCombatant.Reset();
	RecentLeaderDamage.Reset();
}
