#pragma once

#include "CoreMinimal.h"
#include "AdventureProp.h"
#include "Altar.generated.h"

class AEnemySpawner;

/**
 * An altar held by guards. Kill every guard, then stand at the altar and cleanse it (Interact, then stay put
 * for Capture Time without being hit). A cleansed altar sets the world flag Altar Id, which is what opens
 * boss zones: give a Progress Gate that flag in its Required Flags.
 *
 * Setup in the level:
 *   1. Place the altar, set Altar Id (e.g. Altar.Dawn) and Display Name.
 *   2. Place Enemy Spawners for its guards around it and add them to Guard Spawners
 *      (or add enemies placed by hand to Placed Guards).
 *   3. On the gate of the boss zone it opens, add the same Altar Id to Required Flags.
 *
 * Once cleansed, its guards stop respawning. Altars count toward winning the game.
 */
UCLASS()
class ABILITYSYSTEM_API AAltar : public AAdventureProp
{
	GENERATED_BODY()

public:
	AAltar();

	UFUNCTION(BlueprintPure, Category = "Altar")
	bool IsCleansed() const;

	/** Guards still alive, across Guard Spawners and Placed Guards. */
	UFUNCTION(BlueprintPure, Category = "Altar")
	int32 GetLivingGuardCount() const;

	virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;

	/** For Blueprint looks: a beam of light, a sound, a change of material. Also fires when a saved game loads it cleansed. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Altar")
	void OnCleansed(bool bFromSave);

	/** The world flag set when cleansed. Must be unique; gates and quests refer to it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Altar")
	FName AltarId = TEXT("Altar.One");

	/** Spawners whose enemies guard this altar. All of them must be dead before it can be cleansed. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Altar")
	TArray<TObjectPtr<AEnemySpawner>> GuardSpawners;

	/** Guards placed in the level by hand (a mini-boss). Anything with a Combatant Component. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Altar")
	TArray<TObjectPtr<AActor>> PlacedGuards;

	/** Seconds the player must stand at the altar, unharmed, to cleanse it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Altar", meta = (ClampMin = "0.5", Units = "s"))
	float CaptureTime = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Altar", meta = (ClampMin = "0"))
	int32 ExperienceReward = 500;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Altar")
	FLinearColor CorruptedColor = FLinearColor(0.7f, 0.1f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Altar")
	FLinearColor CleansedColor = FLinearColor(1.0f, 0.9f, 0.5f);

private:
	void CompleteCapture();
	void ApplyCleansedState(bool bFromSave);
};
