#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"
#include "TargetableInterface.h"
#include "BaseCharacter.generated.h"

class UAbilityComponent;
class UAttributeComponent;
class UCombatantComponent;
class UResourceComponent;

/**
 * Parent of the player and every enemy: a team, where it sees from, where it is
 * aimed at, and a death hook. Creates no components: they're added by the child
 * (C++ or Blueprint) and found here, so existing Blueprints reparent without losing settings.
 */
UCLASS(Abstract)
class ABILITYSYSTEM_API ABaseCharacter : public ACharacter, public IGenericTeamAgentInterface, public ITargetableInterface
{
	GENERATED_BODY()

public:
	virtual void PostInitializeComponents() override;

	/* -------------------- Team -------------------- */

	virtual FGenericTeamId GetGenericTeamId() const override { return FGenericTeamId(TeamId); }

	/* -------------------- Senses and aiming -------------------- */

	/** Seen from the eye socket, with a level view: perception's cone follows the body, not the head's pitch. */
	virtual void GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;

	virtual FVector GetTargetAimLocation_Implementation() const override;

	/* -------------------- State -------------------- */

	UFUNCTION(BlueprintPure, Category = "Character")
	bool IsDead() const;

	/* -------------------- Components -------------------- */

	UFUNCTION(BlueprintPure, Category = "Character|Components")
	UAbilityComponent* GetAbilityComponent() const { return AbilityComponent; }

	UFUNCTION(BlueprintPure, Category = "Character|Components")
	UCombatantComponent* GetCombatantComponent() const { return CombatantComponent; }

	UFUNCTION(BlueprintPure, Category = "Character|Components")
	UResourceComponent* GetResourceComponent() const { return ResourceComponent; }

	UFUNCTION(BlueprintPure, Category = "Character|Components")
	UAttributeComponent* GetAttributeComponent() const { return AttributeComponent; }
	
	/** Gap between this character's capsule and Other's (center distance minus both radii). 0 or less = touching. */
	UFUNCTION(BlueprintPure, Category = "Character")
	float GetEdgeDistanceTo(const AActor* Other) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Children override for their own death handling (AI stop, corpse); call Super to reach OnCharacterDied. */
	virtual void HandleCharacterDied(AActor* Killer);

	/** Play a death animation or ragdoll here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Character")
	void OnCharacterDied(AActor* Killer);

	/** 0 = player, 1 = enemies. Different teams are hostile; the same team is friendly. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Team")
	uint8 TeamId = 0;

	/** Mesh socket perception sees from. Missing = the default eye height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Senses")
	FName EyeSocketName = TEXT("head");

	/** Mesh socket targeting and arrows aim at. Missing = the capsule's center. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Senses")
	FName AimSocketName = TEXT("spine_03");

private:
	UFUNCTION()
	void HandleCombatantDied(AActor* Killer);

	UPROPERTY(Transient)
	TObjectPtr<UAbilityComponent> AbilityComponent;

	UPROPERTY(Transient)
	TObjectPtr<UCombatantComponent> CombatantComponent;

	UPROPERTY(Transient)
	TObjectPtr<UResourceComponent> ResourceComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAttributeComponent> AttributeComponent;
};