#pragma once

#include "CoreMinimal.h"
#include "AdventureProp.h"
#include "ModifierTypes.h"
#include "UpgradeShrine.generated.h"

class APawn;

/**
 * A one-time, permanent reward for exploring: this game's heart container. Using it applies its
 * Modifiers to the player for good (+25 Max Health, +3% Move Speed, +5 Attack Power...).
 *
 * Hide them off the path, behind puzzles, after hard fights. Each needs its own Shrine Id; the bonus
 * is given again by the shrine every time a saved game loads, so it never has to be stored anywhere else.
 */
UCLASS()
class ABILITYSYSTEM_API AUpgradeShrine : public AAdventureProp
{
	GENERATED_BODY()

public:
	AUpgradeShrine();

	UFUNCTION(BlueprintPure, Category = "Shrine")
	bool IsClaimed() const;

	virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override { return !IsClaimed(); }
	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** For Blueprint looks: a burst of light, a chime. Not fired for shrines claimed in a loaded game. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Shrine")
	void OnClaimed();

	/** World flag set when claimed. Must be unique per shrine. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shrine")
	FName ShrineId = TEXT("Shrine.One");

	/** What the player keeps. E.g. Stat.MaxHealth, Add, 25. Read at rank 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shrine")
	TArray<FStatModifier> Modifiers;

	/** Told to the player when claimed, e.g. "Your life force grows. (+25 Max Health)". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shrine")
	FText RewardText;

private:
	UFUNCTION()
	void HandlePlayerReady(APawn* PlayerPawn);

	void ApplyBonus(AActor* Player) const;
	void ApplyClaimedLook();
};
