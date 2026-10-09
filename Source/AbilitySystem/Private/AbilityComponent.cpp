#include "AbilityComponent.h"

#include "ActiveAbility.h"
#include "GameFramework/Character.h"
#include "InputBufferComponent.h"
#include "MotionWarpingComponent.h"
#include "ResourceComponent.h"
#include "TargetingComponent.h"
#include "AbilitySystemTags.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "PassiveAbility.h"
#include "CombatantComponent.h"
#include "HitReactionAbility.h"
#include "AbilitySystemLog.h"
#include "AttributeComponent.h"

UAbilityComponent::UAbilityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bWantsInitializeComponent = true;
}

void UAbilityComponent::BeginPlay()
{
	Super::BeginPlay();

	OwningCharacter = Cast<ACharacter>(GetOwner());
	InputBufferComponent = GetOwner()->FindComponentByClass<UInputBufferComponent>();
	TargetingComponent = GetOwner()->FindComponentByClass<UTargetingComponent>();
	MotionWarpingComponent = GetOwner()->FindComponentByClass<UMotionWarpingComponent>();
	ResourceComponent = GetOwner()->FindComponentByClass<UResourceComponent>();
	AttributeComponent = GetOwner()->FindComponentByClass<UAttributeComponent>();

	if (!IsValid(OwningCharacter))
	{
		UE_LOG(LogTemp, Error, TEXT("UAbilityComponent requires an ACharacter owner."));
		return;
	}
	
	if (!IsValid(OwningCharacter))
	{
		UE_LOG(LogTemp, Error, TEXT("UAbilityComponent requires an ACharacter owner."));
		return;
	}

	// Effects (passive bonuses, procs, statuses) live on the combatant, so it's needed before passives activate.
	CombatantComponent = GetOwner()->FindComponentByClass<UCombatantComponent>();

	if (IsValid(CombatantComponent))
	{
		CombatantComponent->OnDamageDealt.AddDynamic(this, &UAbilityComponent::HandleOwnerDamageDealt);
		CombatantComponent->OnPayloadReceived.AddDynamic(this, &UAbilityComponent::HandleOwnerPayloadReceived);
		CombatantComponent->OnKilled.AddDynamic(this, &UAbilityComponent::HandleOwnerKilled);
		CombatantComponent->OnDied.AddDynamic(this, &UAbilityComponent::HandleOwnerDied);
		CombatantComponent->OnHitReaction.AddDynamic(this, &UAbilityComponent::HandleOwnerHitReaction);
		CombatantComponent->OnEffectsChanged.AddDynamic(this, &UAbilityComponent::HandleCombatantEffectsChanged);
	}
	else
	{
		UE_LOG(LogAbilitySystem, Error, TEXT("UAbilityComponent on %s requires a UCombatantComponent: passives, buffs and stat modifiers will not work."),
			*GetNameSafe(GetOwner()));
	}

	// Passives need the owning character, so their instances are created here,
	// not at grant time during InitializeComponent.
	for (const TSubclassOf<UAbility>& AbilityClass : GrantedAbilityClasses)
	{
		CreatePassiveInstance(AbilityClass);
	}
	
	if (!IsValid(InputBufferComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("UAbilityComponent requires UInputBufferComponent on %s."), *GetNameSafe(GetOwner()));
		return;
	}

	if (!IsValid(MotionWarpingComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAbilityComponent could not find UMotionWarpingComponent on %s."), *GetNameSafe(GetOwner()));
	}

	if (!IsValid(TargetingComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAbilityComponent could not find UTargetingComponent on %s."), *GetNameSafe(GetOwner()));
	}
	
}
void UAbilityComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// Runs for every component on the actor before any BeginPlay, so base
	// abilities always precede weapon kits in GrantedAbilityClasses.
	GrantAbilities(StartingAbilityClasses, this);
	
	if (HitReactionAbilityClass)
	{
		GrantAbility(HitReactionAbilityClass, this);
	}
	
}
void UAbilityComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(ActiveAbility))
	{
		EndActiveAbilityInternal(EAbilityEndReason::Cancelled, false);
	}
	
	for (UPassiveAbility* Passive : PassiveInstances)
	{
		if (IsValid(Passive))
		{
			Passive->DeactivatePassive();
		}
	}
	
	if (IsValid(CombatantComponent))
	{
		CombatantComponent->OnDamageDealt.RemoveDynamic(this, &UAbilityComponent::HandleOwnerDamageDealt);
		CombatantComponent->OnPayloadReceived.RemoveDynamic(this, &UAbilityComponent::HandleOwnerPayloadReceived);
		CombatantComponent->OnKilled.RemoveDynamic(this, &UAbilityComponent::HandleOwnerKilled);
		CombatantComponent->OnDied.RemoveDynamic(this, &UAbilityComponent::HandleOwnerDied);
		CombatantComponent->OnHitReaction.RemoveDynamic(this, &UAbilityComponent::HandleOwnerHitReaction);
		CombatantComponent->OnEffectsChanged.RemoveDynamic(this, &UAbilityComponent::HandleCombatantEffectsChanged);
	}

	

	PassiveInstances.Reset();
	
	Super::EndPlay(EndPlayReason);
}

void UAbilityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsValid(ActiveAbility) || !bAbilityTickEnabled || bEndingAbility)
	{
		return;
	}

	UActiveAbility* TickingAbility = ActiveAbility;

	DispatchAbilityCallback([TickingAbility, DeltaTime]()
	{
		TickingAbility->TickAbility(DeltaTime);
	});
}

bool UAbilityComponent::GrantAbility(const TSubclassOf<UAbility> AbilityClass, const UObject* Source)
{
	bool bClassAdded = false;
	const bool bGranted = AddGrantInternal(AbilityClass, Source, bClassAdded);

	if (bClassAdded)
	{
		GrantedAbilitiesChangedEvent.Broadcast();
	}

	return bGranted;
}

int32 UAbilityComponent::GrantAbilities(const TArray<TSubclassOf<UAbility>>& AbilityClasses, const UObject* Source)
{
	int32 GrantedCount = 0;
	bool bAnyClassAdded = false;

	for (const TSubclassOf<UAbility>& AbilityClass : AbilityClasses)
	{
		bool bClassAdded = false;

		if (AddGrantInternal(AbilityClass, Source, bClassAdded))
		{
			++GrantedCount;
		}

		bAnyClassAdded |= bClassAdded;
	}

	if (bAnyClassAdded)
	{
		GrantedAbilitiesChangedEvent.Broadcast();
	}

	return GrantedCount;
}

bool UAbilityComponent::RemoveAbility(const FGameplayTag AbilityId, const UObject* Source)
{
	const TSubclassOf<UAbility> AbilityClass = FindAbilityClassById(AbilityId);
	const int32 GrantIndex = FindGrantIndex(AbilityClass);

	if (GrantIndex == INDEX_NONE)
	{
		return false;
	}

	const FObjectKey SourceKey(ResolveGrantSource(Source));
	TArray<FObjectKey>& Sources = GrantedAbilitySources[GrantIndex].Sources;

	if (Sources.Remove(SourceKey) == 0)
	{
		return false;
	}

	if (Sources.IsEmpty())
	{
		EraseEmptyGrants({ AbilityClass });
	}

	return true;
}

int32 UAbilityComponent::RevokeAbilitiesFromSource(const UObject* Source)
{
	const FObjectKey SourceKey(ResolveGrantSource(Source));

	TArray<TSubclassOf<UAbility>> Emptied;
	int32 RevokedCount = 0;

	for (int32 GrantIndex = 0; GrantIndex < GrantedAbilitySources.Num(); ++GrantIndex)
	{
		TArray<FObjectKey>& Sources = GrantedAbilitySources[GrantIndex].Sources;

		if (Sources.Remove(SourceKey) == 0)
		{
			continue;
		}

		++RevokedCount;

		if (Sources.IsEmpty())
		{
			Emptied.Add(GrantedAbilityClasses[GrantIndex]);
		}
	}

	EraseEmptyGrants(Emptied);
	return RevokedCount;
}

bool UAbilityComponent::IsAbilityGrantedBySource(const FGameplayTag AbilityId, const UObject* Source) const
{
	const int32 GrantIndex = FindGrantIndex(FindAbilityClassById(AbilityId));

	return GrantIndex != INDEX_NONE &&
		GrantedAbilitySources[GrantIndex].Sources.Contains(FObjectKey(ResolveGrantSource(Source)));
}

bool UAbilityComponent::HasAbility(FGameplayTag AbilityId) const
{
	return FindAbilityClassById(AbilityId) != nullptr;
}

TSubclassOf<UAbility> UAbilityComponent::FindAbilityClassById(const FGameplayTag AbilityId) const
{
	if (!AbilityId.IsValid())
	{
		return nullptr;
	}

	for (const TSubclassOf<UAbility> AbilityClass : GrantedAbilityClasses)
	{
		const UAbility* AbilityCDO = GetAbilityCDO(AbilityClass);

		if (IsValid(AbilityCDO) && AbilityCDO->GetAbilityId().MatchesTagExact(AbilityId))
		{
			return AbilityClass;
		}
	}

	return nullptr;
}

const UAbility* UAbilityComponent::GetAbilityDefaults(const TSubclassOf<UAbility> AbilityClass) const
{
	return GetAbilityCDO(AbilityClass);
}

bool UAbilityComponent::TryActivateAbility(FGameplayTag AbilityId)
{
	const TSubclassOf<UAbility> AbilityClass = FindAbilityClassById(AbilityId);

	if (!AbilityClass || bEndingAbility)
	{
		return false;
	}

	if (IsValid(ActiveAbility))
	{
		if (ActiveAbility->GetClass() == AbilityClass)
		{
			return ExecuteRepeatedAbilityRequest(ActiveAbility);
		}

		return ReplaceActiveAbility(AbilityClass);
	}

	UActiveAbility* NewAbility = CreateExecutionInstance(AbilityClass);

	if (!IsValid(NewAbility) || !CanActivateAbilityInstance(NewAbility, false))
	{
		return false;
	}

	return ActivateAbilityInstance(NewAbility);
}

bool UAbilityComponent::ResolveBufferedAbilityInput()
{
	if (!IsValid(InputBufferComponent) || bResolvingBufferedInput || bEndingAbility)
	{
		return false;
	}

	bResolvingBufferedInput = true;

	TArray<FAbilityInputCandidate> Candidates;
	BuildAbilityInputCandidates(Candidates);

	bool bExecuted = false;

	for (const FAbilityInputCandidate& Candidate : Candidates)
	{
		if (!ExecuteAbilityInputCandidate(Candidate))
		{
			continue;
		}

		InputBufferComponent->ConsumeInput(Candidate.BufferedInput);
		bExecuted = true;
		break;
	}

	bResolvingBufferedInput = false;
	return bExecuted;
}

void UAbilityComponent::CancelActiveAbility(const bool bResolveBufferedInput)
{
	if (IsValid(ActiveAbility))
	{
		EndActiveAbilityInternal(EAbilityEndReason::Cancelled, bResolveBufferedInput);
	}
}

void UAbilityComponent::HandleInputPressed(const FGameplayTag InputTag)
{
	if (!IsValid(InputBufferComponent) || !InputTag.IsValid())
	{
		return;
	}

	const float BufferDuration = GetLongestBufferDurationForInput(InputTag);

	if (!InputBufferComponent->InputPressed(InputTag, BufferDuration))
	{
		return;
	}

	if (!IsValid(ActiveAbility) || bEndingAbility)
	{
		ResolveBufferedAbilityInput();
		return;
	}

	UActiveAbility* InputAbility = ActiveAbility;

	DispatchAbilityCallback([InputAbility, InputTag]()
	{
		InputAbility->OnInputPressed(InputTag);
	});

	if (!IsValid(ActiveAbility) ||
		bEndingAbility ||
		bResolvingBufferedInput)
	{
		return;
	}

	/*
	 * Repeated input belonging to the active ability remains controlled by
	 * that ability's InputCheckPoint logic. This prevents combo steps from
	 * advancing immediately when the attack button is pressed.
	 */
	if (ActiveAbility->GetActivationInputTag().MatchesTagExact(InputTag))
	{
		return;
	}

	/*
	 * A different ability input should be evaluated immediately while the
	 * active ability still permits early cancellation.
	 */
	if (!ActiveAbility->IsEarlyCancellationClosed() &&
		!ActiveAbility->IsCommitted() &&
		!ActiveAbility->GetAllowedEarlyCancellationAbilityTags().IsEmpty())
	{
		ResolveBufferedAbilityInput();
	}
}

void UAbilityComponent::HandleInputReleased(FGameplayTag InputTag)
{
	if (!IsValid(InputBufferComponent) || !InputTag.IsValid())
	{
		return;
	}

	if (!InputBufferComponent->InputReleased(InputTag))
	{
		return;
	}

	if (!IsValid(ActiveAbility) || bEndingAbility)
	{
		return;
	}

	UActiveAbility* InputAbility = ActiveAbility;

	DispatchAbilityCallback([InputAbility, InputTag]()
	{
		InputAbility->OnInputReleased(InputTag);
	});
}

void UAbilityComponent::MovementInputReceived(FVector2D MovementInput)
{
	if (IsValid(InputBufferComponent))
	{
		InputBufferComponent->SetMovementInput(MovementInput);
	}

	if (!IsValid(ActiveAbility) || bEndingAbility)
	{
		return;
	}

	UActiveAbility* InputAbility = ActiveAbility;

	DispatchAbilityCallback([InputAbility, MovementInput]()
	{
		InputAbility->OnMovementInputReceived(MovementInput);
	});
}

FVector2D UAbilityComponent::GetMovementInput() const
{
	return IsValid(InputBufferComponent)
		? InputBufferComponent->GetMovementInput()
		: FVector2D::ZeroVector;
}
bool UAbilityComponent::IsInputHeld(FGameplayTag InputTag) const
{
	return IsValid(InputBufferComponent) && InputBufferComponent->IsInputHeld(InputTag);
}

void UAbilityComponent::ClearBufferedInputs()
{
	if (IsValid(InputBufferComponent))
	{
		InputBufferComponent->ClearBufferedInputs();
	}
}

void UAbilityComponent::HandleAbilityEvent(FGameplayTag EventTag, const UAnimSequenceBase* SourceAnimation)
{
	if (!IsValid(ActiveAbility) || !EventTag.IsValid() || bEndingAbility)
	{
		return;
	}

	UActiveAbility* EventAbility = ActiveAbility;
	
	if (!EventAbility->AcceptsAnimationEvent(SourceAnimation))
	{
		return;
	}

	// Cost before delivery: a failed payment means the event (e.g. the release) never happens.
	if (!ApplyEventCost(EventAbility, EventTag) || ActiveAbility != EventAbility)
	{
		return;
	}

	DispatchAbilityCallback([EventAbility, EventTag]()
	{
		EventAbility->OnAnimationEvent(EventTag);
	});
}

void UAbilityComponent::AddLooseOwnerTag(FGameplayTag Tag)
{
	if (!Tag.IsValid())
	{
		return;
	}

	int32& Count = LooseOwnerTagCounts.FindOrAdd(Tag);
	const bool bWasAbsent = Count <= 0;

	++Count;

	if (bWasAbsent)
	{
		BroadcastOwnedTagsChanged();
	}
}

void UAbilityComponent::RemoveLooseOwnerTag(FGameplayTag Tag)
{
	int32* Count = LooseOwnerTagCounts.Find(Tag);

	if (!Count)
	{
		return;
	}

	if (*Count > 1)
	{
		--(*Count);
		return;
	}

	LooseOwnerTagCounts.Remove(Tag);
	BroadcastOwnedTagsChanged();
}

bool UAbilityComponent::HasOwnerTag(FGameplayTag Tag) const
{
	return Tag.IsValid() && GetOwnedGameplayTags().HasTag(Tag);
}

bool UAbilityComponent::HasAllOwnerTags(const FGameplayTagContainer& Tags) const
{
	return GetOwnedGameplayTags().HasAll(Tags);
}

bool UAbilityComponent::HasAnyOwnerTags(const FGameplayTagContainer& Tags) const
{
	return GetOwnedGameplayTags().HasAny(Tags);
}

FGameplayTagContainer UAbilityComponent::GetOwnedGameplayTags() const
{
	FGameplayTagContainer Result = BuildOwnedTagsWithoutActiveAbility();
	Result.AppendTags(ActiveGrantedTags);
	return Result;
}


bool UAbilityComponent::IsActiveAbilityCommitted() const
{
	return IsValid(ActiveAbility) && ActiveAbility->IsCommitted();
}

bool UAbilityComponent::CommitAbility(UActiveAbility* RequestingAbility)
{
	if (!IsValid(RequestingAbility) ||
		RequestingAbility != ActiveAbility ||
		RequestingAbility->GetAbilityStatus() != EAbilityStatus::Active)
	{
		return false;
	}

	if (RequestingAbility->IsCommitted())
	{
		return true;
	}

	if (!RequestingAbility->CanCommitAbility() ||
		!CanAffordAbility(RequestingAbility->GetAbilityId()))
	{
		return false;
	}

	RequestingAbility->SetCommitted(true);
	RequestingAbility->SetEarlyCancellationClosed(true);

	SpendAbilityCost(RequestingAbility);

	if (RequestingAbility->GetCooldownTrigger() == EAbilityCooldownTrigger::OnCommit)
	{
		StartCooldown(RequestingAbility->GetAbilityId(), ResolveCooldownDuration(RequestingAbility));
	}

	DispatchAbilityCallback([this, RequestingAbility]()
	{
		RequestingAbility->OnAbilityCommitted();
		AbilityCommittedEvent.Broadcast(RequestingAbility->GetAbilityId(), RequestingAbility);
		
		ForEachActivePassive([RequestingAbility](UPassiveAbility* Passive)
		{
			Passive->OnAbilityCommitted(RequestingAbility);
		});
	});

	return true;
}

void UAbilityComponent::EndAbility(UActiveAbility* RequestingAbility, EAbilityEndReason EndReason)
{
	if (IsValid(RequestingAbility) && RequestingAbility == ActiveAbility)
	{
		EndActiveAbilityInternal(EndReason);
	}
}

void UAbilityComponent::SetAbilityTickEnabled(UActiveAbility* RequestingAbility, bool bEnabled)
{
	if (!IsValid(RequestingAbility) || RequestingAbility != ActiveAbility || bEndingAbility)
	{
		return;
	}

	bAbilityTickEnabled = bEnabled;
	SetComponentTickEnabled(bAbilityTickEnabled);
}

void UAbilityComponent::BuildAbilityInputCandidates(TArray<FAbilityInputCandidate>& OutCandidates)
{
	OutCandidates.Reset();

	if (!IsValid(InputBufferComponent))
	{
		return;
	}

	TArray<FBufferedInput> BufferedInputs;
	InputBufferComponent->GetInputCandidates(BufferedInputs, true);

	for (const FBufferedInput& BufferedInput : BufferedInputs)
	{
		for (const TSubclassOf<UAbility> AbilityClass : GrantedAbilityClasses)
		{
			// Passives fail the cast and never become input candidates.
			const UActiveAbility* AbilityCDO = Cast<UActiveAbility>(GetAbilityCDO(AbilityClass));

			if (!IsValid(AbilityCDO) ||
				!AbilityCDO->GetActivationInputTag().IsValid() ||
				!AbilityCDO->GetActivationInputTag().MatchesTagExact(BufferedInput.InputTag))
			{
				continue;
			}
			if (GetAbilityRank(AbilityClass) <= 0)
			{
				continue;
			}

			if (BufferedInput.Source == EBufferedInputSource::Held &&
				!AbilityCDO->CanActivateFromHeldInput())
			{
				continue;
			}

			if (AbilityCDO->RequiresInputHeldAtResolution() &&
				!InputBufferComponent->IsInputHeld(BufferedInput.InputTag))
			{
				continue;
			}

			FAbilityInputCandidate& Candidate = OutCandidates.AddDefaulted_GetRef();
			Candidate.AbilityClass = AbilityClass;
			Candidate.BufferedInput = BufferedInput;
			Candidate.Priority = AbilityCDO->GetActivationPriority();
		}
	}

	OutCandidates.Sort([](const FAbilityInputCandidate& A, const FAbilityInputCandidate& B)
	{
		if (A.Priority != B.Priority)
		{
			return A.Priority > B.Priority;
		}

		return A.BufferedInput.Sequence < B.BufferedInput.Sequence;
	});
}

bool UAbilityComponent::ExecuteAbilityInputCandidate(const FAbilityInputCandidate& Candidate)
{
	if (!Candidate.AbilityClass)
	{
		return false;
	}

	if (!IsValid(ActiveAbility))
	{
		UActiveAbility* NewAbility = CreateExecutionInstance(Candidate.AbilityClass);

		if (!IsValid(NewAbility) || !CanActivateAbilityInstance(NewAbility, false))
		{
			return false;
		}

		return ActivateAbilityInstance(NewAbility);
	}

	if (ActiveAbility->GetClass() == Candidate.AbilityClass)
	{
		// Combo abilities can consume their own input internally without
		// replacing the active execution.
		if (ActiveAbility->CanHandleRepeatedActivationRequest())
		{
			return ExecuteRepeatedAbilityRequest(ActiveAbility);
		}

		// Other abilities may transition into a fresh execution of themselves.
		return ReplaceActiveAbility(Candidate.AbilityClass);
	}

	return ReplaceActiveAbility(Candidate.AbilityClass);
}

bool UAbilityComponent::ExecuteRepeatedAbilityRequest(UActiveAbility* Ability)
{
	if (!IsValid(Ability) ||
		Ability != ActiveAbility ||
		!Ability->CanHandleRepeatedActivationRequest())
	{
		return false;
	}

	bool bConsumed = false;

	DispatchAbilityCallback([Ability, &bConsumed]()
	{
		bConsumed = Ability->HandleRepeatedActivationRequest();
	});

	return bConsumed;
}

bool UAbilityComponent::ReplaceActiveAbility(TSubclassOf<UAbility> IncomingAbilityClass)
{
	if (!IsValid(ActiveAbility) || !IncomingAbilityClass)
	{
		return false;
	}

	UActiveAbility* IncomingAbility = CreateExecutionInstance(IncomingAbilityClass);

	if (!IsValid(IncomingAbility))
	{
		return false;
	}

	if (!CanActivateAbilityInstance(IncomingAbility, true))
	{
		return false;
	}

	EAbilityEndReason ReplacementReason = EAbilityEndReason::Interrupted;

	if (!CanReplaceActiveAbility(ActiveAbility, IncomingAbility, ReplacementReason))
	{
		return false;
	}

	EndActiveAbilityInternal(ReplacementReason, false);

	if (IsValid(ActiveAbility))
	{
		return false;
	}

	return ActivateAbilityInstance(IncomingAbility);
}

bool UAbilityComponent::CanActivateAbilityInstance(const UActiveAbility* Ability, const bool bReplacingActiveAbility) const
{
	if (!IsValid(Ability) || !Ability->GetAbilityId().IsValid())
	{
		return false;
	}

	const FGameplayTagContainer OwnerTags = bReplacingActiveAbility
		? BuildOwnedTagsWithoutActiveAbility()
		: GetOwnedGameplayTags();

	if (!OwnerTags.HasAll(Ability->GetRequiredOwnerTags()))
	{
		return false;
	}

	if (OwnerTags.HasAny(Ability->GetBlockedOwnerTags()))
	{
		return false;
	}
	
	if (OwnerTags.HasTag(AbilitySystemTags::State_Dead))
	{
		return false;
	}
	
	if (Ability->GetAbilityRank() <= 0)
	{
		return false;
	}
	
	if (IsAbilityOnCooldown(Ability->GetAbilityId()))
	{
		return false;
	}

	if (!CanAffordAbility(Ability->GetAbilityId()))
	{
		return false;
	}

	return Ability->CanActivateAbility();
}

bool UAbilityComponent::CanReplaceActiveAbility(const UActiveAbility* CurrentAbility, const UActiveAbility* IncomingAbility, EAbilityEndReason& OutReplacementReason)
{
	OutReplacementReason = EAbilityEndReason::Interrupted;

	if (!IsValid(CurrentAbility) || !IsValid(IncomingAbility))
	{
		return false;
	}

	if (CurrentAbility->CanTransitionTo(IncomingAbility))
	{
		OutReplacementReason = EAbilityEndReason::Transitioned;
		return true;
	}

	if (CurrentAbility->CanEarlyCancelTo(IncomingAbility))
	{
		OutReplacementReason = EAbilityEndReason::EarlyCancelled;
		return true;
	}

	if (CurrentAbility->GetBlockAbilitiesWithTags().HasAny(IncomingAbility->GetAbilityTags()))
	{
		return false;
	}

	const bool bIncomingExplicitlyCancels =
		IncomingAbility->GetCancelAbilitiesWithTags().HasAny(CurrentAbility->GetAbilityTags());

	if (bIncomingExplicitlyCancels)
	{
		if (!IncomingAbility->CanReplaceActiveAbility(CurrentAbility))
		{
			return false;
		}

		OutReplacementReason = EAbilityEndReason::Interrupted;
		return true;
	}

	const bool bCanInterrupt =
		CurrentAbility->CanBeCancelledBy(IncomingAbility) &&
		IncomingAbility->CanReplaceActiveAbility(CurrentAbility);

	if (bCanInterrupt)
	{
		OutReplacementReason = EAbilityEndReason::Interrupted;
	}

	return bCanInterrupt;
}
UActiveAbility* UAbilityComponent::CreateExecutionInstance(const TSubclassOf<UAbility> AbilityClass)
{
	// Only active abilities have executions; passives are never instanced here.
	if (!AbilityClass ||
		!AbilityClass->IsChildOf(UActiveAbility::StaticClass()) ||
		!IsValid(OwningCharacter))
	{
		return nullptr;
	}

	UActiveAbility* NewAbility = NewObject<UActiveAbility>(this, AbilityClass);

	if (!IsValid(NewAbility))
	{
		return nullptr;
	}

	NewAbility->InitializeAbility(this, OwningCharacter);
	return NewAbility;
}

bool UAbilityComponent::ActivateAbilityInstance(UActiveAbility* Ability)
{
	if (!IsValid(Ability) || IsValid(ActiveAbility) || bEndingAbility)
	{
		return false;
	}

	ActiveAbility = Ability;
	ActiveAbility->SetAbilityStatus(EAbilityStatus::Active);
	ActiveAbility->SetCommitted(false);
	ActiveAbility->SetTransitionOpen(false);
	ActiveAbility->SetEarlyCancellationClosed(false);
	
	bAbilityTickEnabled = false;
	SetComponentTickEnabled(false);

	ApplyActiveAbilityTags();

	
	UActiveAbility* ActivatedAbility = ActiveAbility;
	
	
	
	DispatchAbilityCallback([this, ActivatedAbility]()
	{
		AbilityActivatedEvent.Broadcast(ActivatedAbility->GetAbilityId(), ActivatedAbility);
		
		// Before ActivateAbility, so an effect a passive applies here still reaches this execution's snapshot.
		ForEachActivePassive([ActivatedAbility](UPassiveAbility* Passive)
		{
			Passive->OnAbilityActivated(ActivatedAbility);
		});
		
		ActivatedAbility->NotifyFragmentsStarted();
		ActivatedAbility->ActivateAbility();
	});
	
	

	if (ActiveAbility != ActivatedAbility)
	{
		return false;
	}

	// Paid only once activation has actually succeeded (e.g. the montage is playing).
	if (ActivatedAbility->GetCostTrigger() == EAbilityCostTrigger::OnActivate &&
		!ActivatedAbility->IsCommitted() &&
		!CommitAbility(ActivatedAbility))
	{
		EndActiveAbilityInternal(EAbilityEndReason::Failed);
		return false;
	}

	return ActiveAbility == ActivatedAbility;
}

void UAbilityComponent::EndActiveAbilityInternal(EAbilityEndReason EndReason, bool bResolveBufferedInput)
{
	if (!IsValid(ActiveAbility) || bEndingAbility)
	{
		return;
	}

	bEndingAbility = true;
	bResolveBufferedInputAfterCallback = false;

	UActiveAbility* EndingAbility = ActiveAbility;
	const FGameplayTag EndingAbilityId = EndingAbility->GetAbilityId();

	EndingAbility->SetAbilityStatus(EAbilityStatus::Ending);
	EndingAbility->SetTransitionOpen(false);
	EndingAbility->SetEarlyCancellationClosed(true);

	bAbilityTickEnabled = false;
	SetComponentTickEnabled(false);

	RemoveActiveAbilityTags();
	
	// Started before listeners hear about the end, so UI sees the cooldown immediately.
	if (EndingAbility->IsCommitted() &&
		EndingAbility->GetCooldownTrigger() == EAbilityCooldownTrigger::OnAbilityEnd)
	{
		StartCooldown(EndingAbilityId, ResolveCooldownDuration(EndingAbility));
	}

	DispatchAbilityCallback([this, EndingAbility, EndingAbilityId, EndReason]()
	{
		EndingAbility->OnAbilityEnded(EndReason);
		AbilityEndedEvent.Broadcast(EndingAbilityId, EndingAbility, EndReason);
		
		ForEachActivePassive([EndingAbility, EndReason](UPassiveAbility* Passive)
		{
			Passive->OnAbilityEnded(EndingAbility, EndReason);
			
		});
		EndingAbility->NotifyFragmentsEnded(EndReason);
	});

	EndingAbility->SetAbilityStatus(EAbilityStatus::Inactive);

	ActiveAbility = nullptr;
	bEndingAbility = false;

	if (bResolveBufferedInput && !bResolvingBufferedInput && IsValid(InputBufferComponent))
	{
		ResolveBufferedAbilityInput();
	}
	
}
void UAbilityComponent::SetAbilityEarlyCancellationClosed(UActiveAbility* RequestingAbility, const bool bClosed) const
{
	if (!IsValid(RequestingAbility) ||
		RequestingAbility != ActiveAbility ||
		RequestingAbility->GetAbilityStatus() != EAbilityStatus::Active ||
		bEndingAbility)
	{
		return;
	}

	RequestingAbility->SetEarlyCancellationClosed(bClosed);
}
void UAbilityComponent::DispatchAbilityCallback(const TFunctionRef<void()>& Callback)
{
	const bool bWasAlreadyDispatching = bDispatchingAbilityCallback;

	bDispatchingAbilityCallback = true;
	Callback();
	bDispatchingAbilityCallback = bWasAlreadyDispatching;

	if (bWasAlreadyDispatching)
	{
		return;
	}

	if (!bResolveBufferedInputAfterCallback ||
		bEndingAbility ||
		bResolvingBufferedInput)
	{
		return;
	}

	bResolveBufferedInputAfterCallback = false;

	const bool bResolved = ResolveBufferedAbilityInput();

	UE_LOG(LogTemp, Verbose, TEXT("[Transition] Deferred resolution result=%d"), bResolved);
}

FGameplayTagContainer UAbilityComponent::BuildLooseOwnerTags() const
{
	FGameplayTagContainer Result;

	for (const TPair<FGameplayTag, int32>& Pair : LooseOwnerTagCounts)
	{
		if (Pair.Key.IsValid() && Pair.Value > 0)
		{
			Result.AddTag(Pair.Key);
		}
	}

	return Result;
}

FGameplayTagContainer UAbilityComponent::BuildOwnedTagsWithoutActiveAbility() const
{
	// Loose tags plus tags granted by active modifier entries (boosters, procs, states).
	FGameplayTagContainer Result = BuildLooseOwnerTags();
	
	// Tags granted by effects (path passives, procs, statuses).
	if (IsValid(CombatantComponent))
	{
		Result.AppendTags(CombatantComponent->GetEffectTags());
	}
	
	return Result;
}

void UAbilityComponent::ApplyActiveAbilityTags()
{
	ActiveGrantedTags.Reset();

	if (IsValid(ActiveAbility))
	{
		ActiveGrantedTags.AppendTags(ActiveAbility->GetGrantedOwnerTags());
	}

	BroadcastOwnedTagsChanged();
}

void UAbilityComponent::RemoveActiveAbilityTags()
{
	if (ActiveGrantedTags.IsEmpty())
	{
		return;
	}

	ActiveGrantedTags.Reset();
	BroadcastOwnedTagsChanged();
}

void UAbilityComponent::BroadcastOwnedTagsChanged() const
{
	OwnedTagsChangedEvent.Broadcast(GetOwnedGameplayTags());
}

const UAbility* UAbilityComponent::GetAbilityCDO(const TSubclassOf<UAbility> AbilityClass)
{
	return AbilityClass ? AbilityClass.GetDefaultObject() : nullptr;
}

int32 UAbilityComponent::FindCooldownIndex(const FGameplayTag AbilityId) const
{
	for (int32 Index = 0; Index < CooldownState.AbilityIds.Num(); ++Index)
	{
		if (CooldownState.AbilityIds[Index].MatchesTagExact(AbilityId))
		{
			return Index;
		}
	}

	return INDEX_NONE;
}
const UObject* UAbilityComponent::ResolveGrantSource(const UObject* Source) const
{
	return IsValid(Source) ? Source : this;
}

int32 UAbilityComponent::FindGrantIndex(const TSubclassOf<UAbility> AbilityClass) const
{
	return AbilityClass ? GrantedAbilityClasses.IndexOfByKey(AbilityClass) : INDEX_NONE;
}

bool UAbilityComponent::AddGrantInternal(const TSubclassOf<UAbility> AbilityClass, const UObject* Source, bool& bOutClassAdded)
{
	bOutClassAdded = false;

	const UAbility* AbilityCDO = GetAbilityCDO(AbilityClass);

	if (!IsValid(AbilityCDO) ||
		AbilityClass->HasAnyClassFlags(CLASS_Abstract) ||
		!AbilityCDO->GetAbilityId().IsValid())
	{
		return false;
	}

	const FObjectKey SourceKey(ResolveGrantSource(Source));
	const int32 ExistingIndex = FindGrantIndex(AbilityClass);

	if (ExistingIndex != INDEX_NONE)
	{
		GrantedAbilitySources[ExistingIndex].Sources.AddUnique(SourceKey);
		return true;
	}

	// Ability ids must stay unique across classes: cooldowns, ranks and UI key on them.
	if (HasAbility(AbilityCDO->GetAbilityId()))
	{
		UE_LOG(LogTemp, Warning, TEXT("GrantAbility: %s shares AbilityId %s with an already granted class."),
			*GetNameSafe(AbilityClass), *AbilityCDO->GetAbilityId().ToString());
		return false;
	}

	GrantedAbilityClasses.Add(AbilityClass);
	GrantedAbilitySources.AddDefaulted_GetRef().Sources.Add(SourceKey);

	bOutClassAdded = true;
	
	// Grants during InitializeComponent are instanced in BeginPlay instead.
	if (HasBegunPlay())
	{
		CreatePassiveInstance(AbilityClass);
	}
	
	return true;
}

void UAbilityComponent::EraseEmptyGrants(const TArray<TSubclassOf<UAbility>>& Candidates)
{
	if (Candidates.IsEmpty())
	{
		return;
	}

	// Cancel while the class is still granted, and without resolving the buffer:
	// a resolution here could start a sibling that is about to be removed.
	if (IsValid(ActiveAbility) && Candidates.Contains(ActiveAbility->GetClass()))
	{
		EndActiveAbilityInternal(EAbilityEndReason::Cancelled, false);
	}

	bool bAnyErased = false;

	for (const TSubclassOf<UAbility>& AbilityClass : Candidates)
	{
		const int32 GrantIndex = FindGrantIndex(AbilityClass);

		// Re-check: an end callback may have re-granted it during the cancel.
		if (GrantIndex == INDEX_NONE || !GrantedAbilitySources[GrantIndex].Sources.IsEmpty())
		{
			continue;
		}

		DestroyPassiveInstance(AbilityClass);
		GrantedAbilityClasses.RemoveAt(GrantIndex);
		GrantedAbilitySources.RemoveAt(GrantIndex);
		bAnyErased = true;
	}

	if (bAnyErased)
	{
		GrantedAbilitiesChangedEvent.Broadcast();
	}
}

void UAbilityComponent::StartCooldown(const FGameplayTag AbilityId, const float Duration)
{
	if (!AbilityId.IsValid() || Duration <= 0.0f)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const double EndTime = World->GetTimeSeconds() + Duration;
	const int32 Index = FindCooldownIndex(AbilityId);

	if (Index != INDEX_NONE)
	{
		CooldownState.EndTimes[Index] = EndTime;
	}
	else
	{
		CooldownState.AbilityIds.Add(AbilityId);
		CooldownState.EndTimes.Add(EndTime);
		CooldownState.Durations.Add(Duration);
	}
}

float UAbilityComponent::GetAbilityCooldownRemaining(const FGameplayTag AbilityId) const
{
	const int32 Index = FindCooldownIndex(AbilityId);
	if (Index == INDEX_NONE)
	{
		return 0.0f;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	const double Remaining = CooldownState.EndTimes[Index] - World->GetTimeSeconds();
	return Remaining > 0.0 ? static_cast<float>(Remaining) : 0.0f;
}

bool UAbilityComponent::IsAbilityOnCooldown(const FGameplayTag AbilityId) const
{
	return GetAbilityCooldownRemaining(AbilityId) > 0.0f;
}

float UAbilityComponent::GetAbilityCooldownProgress(const FGameplayTag AbilityId) const
{
	const int32 Index = FindCooldownIndex(AbilityId);

	if (Index == INDEX_NONE || CooldownState.Durations[Index] <= 0.0f)
	{
		return 0.0f;
	}

	const float Remaining = GetAbilityCooldownRemaining(AbilityId);

	if (Remaining <= 0.0f)
	{
		return 0.0f;
	}

	// Remaining fraction: 1 at start of cooldown, 0 when ready.
	return FMath::Clamp(Remaining / CooldownState.Durations[Index], 0.0f, 1.0f);
}

float UAbilityComponent::GetCurrentFocus() const
{
	return IsValid(ResourceComponent)
		? ResourceComponent->GetResourceValue(EResourceType::Focus, EResourceValueType::Current)
		: 0.0f;
}

bool UAbilityComponent::CanAffordAbility(const FGameplayTag AbilityId) const
{
	const TSubclassOf<UAbility> AbilityClass = FindAbilityClassById(AbilityId);
	const UActiveAbility* Defaults = Cast<UActiveAbility>(GetAbilityCDO(AbilityClass));

	if (!IsValid(Defaults))
	{
		return false;
	}

	const float Cost = ResolveFocusCost(Defaults);
	if (Cost <= 0.0f)
	{
		return true;
	}

	// No resource component means nothing can pay a non-zero cost.
	if (!IsValid(ResourceComponent))
	{
		return false;
	}

	return GetCurrentFocus() >= Cost;
}

void UAbilityComponent::SpendAbilityCost(const UActiveAbility* Ability)
{
	const float Cost = ResolveFocusCost(Ability);

	if (Cost > 0.0f && IsValid(ResourceComponent))
	{
		ResourceComponent->ModifyResource(EResourceType::Focus, EResourceValueType::Current, -Cost);
	}
}

bool UAbilityComponent::ChargeRepeatedCost(const UActiveAbility* Ability)
{
	if (!CanAffordAbility(Ability->GetAbilityId()))
	{
		return false;
	}

	SpendAbilityCost(Ability);
	return true;
}

bool UAbilityComponent::ApplyEventCost(UActiveAbility* Ability, const FGameplayTag EventTag)
{
	const EAbilityCostTrigger Trigger = Ability->GetCostTrigger();

	const bool bEventTrigger =
		Trigger == EAbilityCostTrigger::OnAnimationEvent ||
		Trigger == EAbilityCostTrigger::OnEveryAnimationEvent;

	if (!bEventTrigger || !EventTag.MatchesTagExact(Ability->GetCostEventTag()))
	{
		return true;
	}

	bool bPaid = false;

	if (!Ability->IsCommitted())
	{
		bPaid = CommitAbility(Ability);
	}
	else
	{
		// Once-only triggers have already paid; repeating triggers pay again.
		bPaid = Trigger == EAbilityCostTrigger::OnAnimationEvent || ChargeRepeatedCost(Ability);
	}

	if (!bPaid)
	{
		DispatchAbilityCallback([Ability]()
		{
			Ability->OnCostPaymentFailed();
		});
	}

	return bPaid;
}

float UAbilityComponent::GetLongestBufferDurationForInput(FGameplayTag InputTag) const
{
	float LongestDuration = 0.0f;

	for (const TSubclassOf<UAbility> AbilityClass : GrantedAbilityClasses)
	{
		const UActiveAbility* AbilityCDO = Cast<UActiveAbility>(GetAbilityCDO(AbilityClass));

		if (!IsValid(AbilityCDO) ||
			!AbilityCDO->GetActivationInputTag().MatchesTagExact(InputTag))
		{
			continue;
		}

		LongestDuration = FMath::Max(LongestDuration, AbilityCDO->GetInputBufferDuration());
	}

	return LongestDuration;
}
void UAbilityComponent::SetAbilityTransitionOpen(UActiveAbility* RequestingAbility, const bool bOpen)
{
	if (!IsValid(RequestingAbility) ||
		RequestingAbility != ActiveAbility ||
		RequestingAbility->GetAbilityStatus() != EAbilityStatus::Active ||
		bEndingAbility)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Transition] Open rejected."));
		return;
	}

	RequestingAbility->SetTransitionOpen(bOpen);

	UE_LOG(
		LogTemp,
		Verbose,
		TEXT("[Transition] %s | Ability=%s | Dispatching=%d | Resolving=%d"),
		bOpen ? TEXT("OPENED") : TEXT("CLOSED"),
		*GetNameSafe(RequestingAbility),
		bDispatchingAbilityCallback,
		bResolvingBufferedInput
	);

	if (!bOpen)
	{
		return;
	}

	if (bDispatchingAbilityCallback)
	{
		bResolveBufferedInputAfterCallback = true;
		UE_LOG(LogTemp, Verbose, TEXT("[Transition] Resolution deferred until callback returns."));
		return;
	}

	if (!bResolvingBufferedInput)
	{
		const bool bResolved = ResolveBufferedAbilityInput();
		UE_LOG(LogTemp, Verbose, TEXT("[Transition] Immediate resolution result=%d"), bResolved);
	}
}
int32 UAbilityComponent::FindRankIndex(const FGameplayTag AbilityId) const
{
	for (int32 Index = 0; Index < RankState.AbilityIds.Num(); ++Index)
	{
		if (RankState.AbilityIds[Index].MatchesTagExact(AbilityId))
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

int32 UAbilityComponent::GetLearnedRank(const TSubclassOf<UAbility> AbilityClass) const
{
	const UAbility* AbilityCDO = GetAbilityCDO(AbilityClass);

	if (!IsValid(AbilityCDO))
	{
		return 0;
	}

	const int32 Index = FindRankIndex(AbilityCDO->GetAbilityId());
	return Index != INDEX_NONE ? RankState.LearnedRanks[Index] : 0;
}

int32 UAbilityComponent::GetAbilityRank(const TSubclassOf<UAbility> AbilityClass) const
{
	const UAbility* AbilityCDO = GetAbilityCDO(AbilityClass);

	if (!IsValid(AbilityCDO))
	{
		return 0;
	}

	return FMath::Clamp(
		AbilityCDO->GetStartingRank() + GetLearnedRank(AbilityClass),
		0,
		AbilityCDO->GetMaxRank()
	);
}

int32 UAbilityComponent::SetLearnedRank(const TSubclassOf<UAbility> AbilityClass, const int32 LearnedRank)
{
	const UAbility* AbilityCDO = GetAbilityCDO(AbilityClass);

	if (!IsValid(AbilityCDO) || !AbilityCDO->GetAbilityId().IsValid())
	{
		return 0;
	}

	const FGameplayTag AbilityId = AbilityCDO->GetAbilityId();
	const int32 MaxLearned = FMath::Max(AbilityCDO->GetMaxRank() - AbilityCDO->GetStartingRank(), 0);
	const int32 NewLearned = FMath::Clamp(LearnedRank, 0, MaxLearned);

	const int32 OldRank = GetAbilityRank(AbilityClass);
	const int32 Index = FindRankIndex(AbilityId);

	if (Index != INDEX_NONE)
	{
		RankState.LearnedRanks[Index] = NewLearned;
	}
	else if (NewLearned > 0)
	{
		RankState.AbilityIds.Add(AbilityId);
		RankState.LearnedRanks.Add(NewLearned);
	}

	const int32 NewRank = GetAbilityRank(AbilityClass);

	if (NewRank == OldRank)
	{
		return NewRank;
	}

	// Losing the last rank mid-use ends the execution; upgrades wait for the next activation.
	if (NewRank <= 0 &&
		IsValid(ActiveAbility) &&
		ActiveAbility->GetAbilityId().MatchesTagExact(AbilityId))
	{
		EndActiveAbilityInternal(EAbilityEndReason::Cancelled, false);
	}

	RefreshPassiveRank(AbilityClass);
	AbilityRankChangedEvent.Broadcast(AbilityId, OldRank, NewRank);
	return NewRank;
}
void UAbilityComponent::HandleInput(const FGameplayTag InputTag, const bool bPressed)
{
	if (bPressed)
	{
		HandleInputPressed(InputTag);
	}
	else
	{
		HandleInputReleased(InputTag);
	}
}

float UAbilityComponent::GetModifiedValue(const FGameplayTag Stat, const float BaseValue, const FGameplayTagContainer& AbilityTags) const
{
	return IsValid(CombatantComponent)
		? CombatantComponent->GetModifiedValue(Stat, BaseValue, AbilityTags)
		: BaseValue;
}

double UAbilityComponent::GetWorldTime() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.0;
}
float UAbilityComponent::ResolveFocusCost(const UActiveAbility* Ability) const
{
	return FMath::Max(
		GetModifiedValue(AbilitySystemTags::Stat_Cost, Ability->GetFocusCost(), Ability->GetAbilityTags()),
		0.0f
	);
}

float UAbilityComponent::ResolveCooldownDuration(const UActiveAbility* Ability) const
{
	return FMath::Max(
		GetModifiedValue(AbilitySystemTags::Stat_Cooldown, Ability->GetCooldownDuration(), Ability->GetAbilityTags()),
		0.0f
	);
}

float UAbilityComponent::GetAbilityFocusCost(const FGameplayTag AbilityId) const
{
	const UActiveAbility* Defaults = Cast<UActiveAbility>(GetAbilityCDO(FindAbilityClassById(AbilityId)));
	return IsValid(Defaults) ? ResolveFocusCost(Defaults) : 0.0f;
}
UPassiveAbility* UAbilityComponent::FindPassiveInstance(const TSubclassOf<UAbility> AbilityClass) const
{
	for (UPassiveAbility* Passive : PassiveInstances)
	{
		if (IsValid(Passive) && Passive->GetClass() == AbilityClass)
		{
			return Passive;
		}
	}

	return nullptr;
}

void UAbilityComponent::CreatePassiveInstance(const TSubclassOf<UAbility> AbilityClass)
{
	if (!AbilityClass ||
		!AbilityClass->IsChildOf(UPassiveAbility::StaticClass()) ||
		IsValid(FindPassiveInstance(AbilityClass)))
	{
		return;
	}

	UPassiveAbility* Passive = NewObject<UPassiveAbility>(this, AbilityClass);

	if (!IsValid(Passive))
	{
		return;
	}

	Passive->InitializeAbility(this, OwningCharacter);
	PassiveInstances.Add(Passive);

	// No-op while the passive is rank 0 (granted but not learned).
	Passive->ActivatePassive();
}

void UAbilityComponent::DestroyPassiveInstance(const TSubclassOf<UAbility> AbilityClass)
{
	UPassiveAbility* Passive = FindPassiveInstance(AbilityClass);

	if (!IsValid(Passive))
	{
		return;
	}

	// Out of the list first, so its own deactivation can't receive further events.
	PassiveInstances.Remove(Passive);
	Passive->DeactivatePassive();
}

void UAbilityComponent::RefreshPassiveRank(const TSubclassOf<UAbility> AbilityClass)
{
	UPassiveAbility* Passive = FindPassiveInstance(AbilityClass);

	if (!IsValid(Passive))
	{
		return;
	}

	Passive->DeactivatePassive();
	Passive->InitializeAbility(this, OwningCharacter);
	Passive->ActivatePassive();
}

void UAbilityComponent::ForEachActivePassive(const TFunctionRef<void(UPassiveAbility*)> Func)
{
	const TArray<TObjectPtr<UPassiveAbility>> Snapshot = PassiveInstances;

	for (UPassiveAbility* Passive : Snapshot)
	{
		if (IsValid(Passive) && Passive->IsPassiveActive())
		{
			Func(Passive);
		}
	}
}

void UAbilityComponent::HandleOwnerDamageDealt(AActor* Target, const FAbilityPayload& Payload, const float DamageApplied)
{
	ForEachActivePassive([Target, &Payload, DamageApplied](UPassiveAbility* Passive)
	{
		Passive->OnDamageDealt(Target, Payload, DamageApplied);
	});
}

void UAbilityComponent::HandleOwnerPayloadReceived(const FAbilityPayload& Payload, const float DamageApplied)
{
	ForEachActivePassive([&Payload, DamageApplied](UPassiveAbility* Passive)
	{
		Passive->OnPayloadReceived(Payload, DamageApplied);
	});
}

void UAbilityComponent::HandleOwnerKilled(AActor* Victim)
{
	ForEachActivePassive([Victim](UPassiveAbility* Passive)
	{
		Passive->OnKilled(Victim);
	});
}

void UAbilityComponent::HandleOwnerDied(AActor* Killer)
{
	ForEachActivePassive([Killer](UPassiveAbility* Passive)
	{
		Passive->OnDied(Killer);
	});
}
void UAbilityComponent::BroadcastGameplayEvent(const FGameplayTag EventTag, UActiveAbility* Source)
{
	if (!EventTag.IsValid())
	{
		return;
	}

	ForEachActivePassive([EventTag, Source](UPassiveAbility* Passive)
	{
		Passive->OnGameplayEvent(EventTag, Source);
	});
}
bool UAbilityComponent::TriggerHitReaction(const FHitReactionResult& Reaction)
{
	if (Reaction.Reaction < EHitReactionType::Stagger ||
		!HitReactionAbilityClass ||
		bEndingAbility ||
		HasOwnerTag(AbilitySystemTags::State_Dead))
	{
		return false;
	}

	UActiveAbility* ReactionAbility = CreateExecutionInstance(HitReactionAbilityClass);

	if (!IsValid(ReactionAbility))
	{
		return false;
	}

	PendingHitReaction = Reaction;

	// Presses made before the hit must not fire out of the stagger.
	ClearBufferedInputs();

	// A hit always wins: no transition, early-cancel or block rules apply.
	if (IsValid(ActiveAbility))
	{
		EndActiveAbilityInternal(EAbilityEndReason::Interrupted, false);
	}

	if (IsValid(ActiveAbility))
	{
		return false;
	}

	return ActivateAbilityInstance(ReactionAbility);
}

void UAbilityComponent::HandleOwnerHitReaction(const FHitReactionResult& Result)
{
	TriggerHitReaction(Result);
}
void UAbilityComponent::HandleCombatantEffectsChanged()
{
	// Effects grant tags and change stats: refresh tag listeners and ability-side stat listeners.
	BroadcastOwnedTagsChanged();
	ModifiersChangedEvent.Broadcast();
}