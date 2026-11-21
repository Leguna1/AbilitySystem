#pragma once

#include "CoreMinimal.h"
#include "AdventureTypes.h"
#include "Components/ActorComponent.h"
#include "PayloadReceiver.h"
#include "AdventureComponent.generated.h"

class ABaseCharacter;
class APawn;
class APlayerController;
class UAdventureHUDWidget;
class UAdventurePanelWidget;
class UAdventureSaveGame;
class UCombatantComponent;
class UCurrencyDataAsset;
class UEnhancementComponent;
class UInventoryComponent;
class UItemDataAsset;
class UQuestLogComponent;

/**
 * The game layer's hub for the player. Put ONE on the player controller (BP_DefaultController); that is the
 * whole setup. It needs no Blueprint work and no widgets: it builds its own HUD.
 *
 * What it does:
 *   - Interact key: finds the chest, altar, villager... next to the player and uses it.
 *   - HUD: interaction prompt, quest tracker, message feed, zone banner, boss bar, cast bar, death and victory.
 *   - Panel: the window villagers, waystones and the quest log talk through (mouse, or number keys).
 *   - Death: after a delay the player wakes up at the last waystone (or where they started).
 *   - Channels: "stand still for 6 seconds" actions like cleansing an altar, broken by damage or moving.
 *   - Rewards: gold, experience and items given by quests, chests and altars go through here.
 *   - Saving: writes and restores the player's side of the save.
 *
 * It creates the Quest Log Component next to itself and the Enhancement Component on the pawn.
 * Keys and numbers: Project Settings -> Game -> Adventure.
 */
UCLASS(ClassGroup = (Adventure), meta = (BlueprintSpawnableComponent))
class ABILITYSYSTEM_API UAdventureComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAdventureComponent();

	/** The local player's Adventure Component, or null if their controller has none. */
	static UAdventureComponent* Get(const UObject* WorldContextObject);

	/* -------------------- Who -------------------- */

	APlayerController* GetPlayerController() const;

	UFUNCTION(BlueprintPure, Category = "Adventure")
	APawn* GetPlayerPawn() const;

	UFUNCTION(BlueprintPure, Category = "Adventure")
	UInventoryComponent* GetInventory() const;

	UFUNCTION(BlueprintPure, Category = "Adventure")
	UQuestLogComponent* GetQuestLog() const { return QuestLog; }

	UFUNCTION(BlueprintPure, Category = "Adventure")
	UEnhancementComponent* GetEnhancement() const;

	/** True once the pawn is set up and the save (if any) has been applied. */
	bool IsPlayerReady() const { return bPlayerReady; }

	/* -------------------- Messages -------------------- */

	/** Adds a line to the message feed for a few seconds. */
	UFUNCTION(BlueprintCallable, Category = "Adventure|Messages")
	void Notify(const FText& Message);

	/** Big text across the top: entering a zone, an altar taken, a boss down. */
	UFUNCTION(BlueprintCallable, Category = "Adventure|Messages")
	void ShowBanner(const FText& Title, const FText& Subtitle, float Duration = 4.0f);

	const TArray<FAdventureNotification>& GetNotifications() const { return Notifications; }
	bool GetBanner(FText& OutTitle, FText& OutSubtitle) const;

	/* -------------------- Rewards -------------------- */

	UFUNCTION(BlueprintCallable, Category = "Adventure|Rewards")
	void GiveExperience(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Adventure|Rewards")
	void GiveGold(int32 Amount);

	/** Into the bag; whatever doesn't fit lands at the player's feet as a pickup. */
	UFUNCTION(BlueprintCallable, Category = "Adventure|Rewards")
	void GiveItem(UItemDataAsset* Item, int32 Count = 1);

	UFUNCTION(BlueprintPure, Category = "Adventure|Rewards")
	int32 GetGold() const;

	/** Takes Amount of gold if the player has it all. Also true when no Gold Currency is set (nothing to pay with). */
	UFUNCTION(BlueprintCallable, Category = "Adventure|Rewards")
	bool SpendGold(int32 Amount);

	/** Project Settings -> Adventure -> Gold Currency, loaded. Null if not set. */
	UCurrencyDataAsset* GetGoldCurrency() const;

	/* -------------------- Boss bar -------------------- */

	/** Shows Boss's name and health across the top. Null hides the bar. */
	UFUNCTION(BlueprintCallable, Category = "Adventure|Boss")
	void SetActiveBoss(ABaseCharacter* Boss);

	UFUNCTION(BlueprintPure, Category = "Adventure|Boss")
	ABaseCharacter* GetActiveBoss() const { return ActiveBoss.Get(); }

	/* -------------------- Channel -------------------- */

	/**
	 * Starts a "stand still" action with a cast bar. OnCompleted runs if the player stays close to where they
	 * started and takes no damage for Duration seconds. False if another channel is running or the player is dead.
	 */
	bool BeginChannel(const FText& Label, float Duration, const FSimpleDelegate& OnCompleted);

	UFUNCTION(BlueprintCallable, Category = "Adventure|Channel")
	void CancelChannel();

	UFUNCTION(BlueprintPure, Category = "Adventure|Channel")
	bool IsChanneling() const { return bChanneling; }

	/** 0..1 while channeling. */
	float GetChannelProgress() const;
	const FText& GetChannelLabel() const { return ChannelLabel; }

	/* -------------------- Panel -------------------- */

	/**
	 * Opens the panel (or replaces what it shows): a title, some text, and a list of options.
	 * OnChosen gets the index of the option the player picked; it usually opens the next page or closes the panel.
	 * While open the mouse cursor is shown and game input is off; Esc or the Interact key closes it.
	 */
	void OpenPanel(const FText& Title, const FText& Body, const TArray<FAdventurePanelOption>& Options, const FAdventurePanelChoiceDelegate& OnChosen);

	UFUNCTION(BlueprintCallable, Category = "Adventure|Panel")
	void ClosePanel();

	UFUNCTION(BlueprintPure, Category = "Adventure|Panel")
	bool IsPanelOpen() const { return bPanelOpen; }

	/** The quest log page: every active quest with its objectives, and how the world's objectives stand. */
	UFUNCTION(BlueprintCallable, Category = "Adventure|Panel")
	void ToggleQuestLog();

	/* -------------------- Interaction -------------------- */

	UFUNCTION(BlueprintPure, Category = "Adventure|Interaction")
	AActor* GetFocusedInteractable() const { return FocusedInteractable.Get(); }

	const FText& GetFocusPrompt() const { return FocusPrompt; }
	bool CanInteractWithFocus() const { return bFocusCanInteract; }

	/** What the Interact key does: uses the focused interactable. */
	UFUNCTION(BlueprintCallable, Category = "Adventure|Interaction")
	void TryInteract();

	/* -------------------- Death and travel -------------------- */

	/** Where the player wakes up after dying. Waystones call this. */
	UFUNCTION(BlueprintCallable, Category = "Adventure|Respawn")
	void SetCheckpoint(const FTransform& NewCheckpoint);

	UFUNCTION(BlueprintPure, Category = "Adventure|Respawn")
	bool IsRespawning() const { return bRespawning; }

	float GetRespawnTimeRemaining() const;

	/** Moves the player's pawn (waystone travel, respawn). */
	UFUNCTION(BlueprintCallable, Category = "Adventure|Respawn")
	void TeleportPlayer(const FTransform& Destination);

	/** Fills the player's health and focus (resting at a waystone). */
	UFUNCTION(BlueprintCallable, Category = "Adventure|Respawn")
	void RestorePlayer();

	/* -------------------- Saving -------------------- */

	void WriteSaveData(UAdventureSaveGame& Save) const;

	/** Saves the game now (world flags and player). Same as the subsystem's Save Game. */
	UFUNCTION(BlueprintCallable, Category = "Adventure|Saving")
	void RequestSave();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	UFUNCTION()
	void HandlePlayerDied(AActor* Killer);

	UFUNCTION()
	void HandlePlayerDamaged(const FAbilityPayload& Payload, float DamageApplied);

	UFUNCTION()
	void HandleVictory();

	void HandleInteractPressed();
	void HandleQuestLogPressed();
	void HandlePanelOption(int32 OptionIndex);
	void HandleQuestLogOption(int32 OptionIndex);

	/** Binds the keys once the controller's input component exists; retries until it does. */
	void TrySetupInput();

	void SetupPawn(APawn* NewPawn);
	void UnbindFromPawn();

	/** A moment after the pawn is known, so every component on it has begun play: applies the save, tells the world. */
	void FinishPlayerSetup();

	void ApplySaveData(const UAdventureSaveGame& Save);
	void UpdateFocus();
	void UpdateChannel();
	void Respawn();
	void ShowQuestLogPage();
	double GetWorldTime() const;

	UPROPERTY(Transient)
	TObjectPtr<UQuestLogComponent> QuestLog;

	UPROPERTY(Transient)
	TObjectPtr<UAdventureHUDWidget> HUDWidget;

	UPROPERTY(Transient)
	TObjectPtr<UAdventurePanelWidget> PanelWidget;

	UPROPERTY(Transient)
	TObjectPtr<UCombatantComponent> BoundCombatant;

	TWeakObjectPtr<APawn> BoundPawn;
	TWeakObjectPtr<ABaseCharacter> ActiveBoss;

	/* Messages */
	TArray<FAdventureNotification> Notifications;
	FText BannerTitle;
	FText BannerSubtitle;
	double BannerExpiresAt = 0.0;

	/* Interaction */
	TWeakObjectPtr<AActor> FocusedInteractable;
	FText FocusPrompt;
	bool bFocusCanInteract = false;
	double NextFocusUpdateTime = 0.0;

	/* Channel */
	bool bChanneling = false;
	FText ChannelLabel;
	double ChannelStartTime = 0.0;
	float ChannelDuration = 0.0f;
	FVector ChannelStartLocation = FVector::ZeroVector;
	FSimpleDelegate ChannelCompleted;

	/* Panel */
	bool bPanelOpen = false;
	bool bQuestLogShown = false;
	bool bCursorWasShownBeforePanel = false;
	FAdventurePanelChoiceDelegate PanelChoice;

	/* Death */
	bool bRespawning = false;
	double RespawnAtTime = 0.0;
	bool bHasCheckpoint = false;
	FTransform Checkpoint;
	FTransform StartTransform;
	bool bHasStartTransform = false;

	/* Setup and saving */
	bool bInputBound = false;
	bool bPlayerReady = false;
	bool bSaveApplied = false;
	double NextAutosaveTime = 0.0;
	FTimerHandle InputRetryTimer;
	FTimerHandle PlayerSetupTimer;
};
