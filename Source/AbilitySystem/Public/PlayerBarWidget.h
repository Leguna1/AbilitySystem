// PlayerBarWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UIBarMaterial.h"
#include "PlayerBarWidget.generated.h"

class APawn;
class ACompanionCharacter;
class UAbilityBarWidget;
class UAbilityComponent;
class UAbilityProgressWidget;
class UCombatantComponent;
class UCompanionComponent;
class UExperienceComponent;
class UImage;
class UMaterialInstanceDynamic;
class UPoiseBarWidget;
class UResourceComponent;
class UResourceWidget;
class UTextBlock;
class UWidget;

/**
 * The whole player bar: health, focus, stagger, level medallion, cast bar, the action bar
 * (left slots, LMB/RMB pod, right slots), XP, and the pet panel that slides out of the
 * right crest while a companion is out.
 *
 * Make a Widget Blueprint of this, lay it out with the kit's pieces, and name the parts
 * below exactly (every one is optional, so you can build it up piece by piece).
 * Add it to your HUD; it finds the player's pawn by itself, also after respawns.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UPlayerBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Points the bar at a pawn. Called automatically for the owning player's pawn. */
	UFUNCTION(BlueprintCallable, Category = "Player Bar")
	void SetPawn(APawn* NewPawn);

	UFUNCTION(BlueprintPure, Category = "Player Bar")
	ACompanionCharacter* GetShownCompanion() const { return ShownCompanion; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/* -------------------- Player parts -------------------- */

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Player Bar|Player")
	TObjectPtr<UResourceWidget> HealthBar;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Player Bar|Player")
	TObjectPtr<UResourceWidget> FocusBar;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Player Bar|Player")
	TObjectPtr<UPoiseBarWidget> StaggerBar;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Player Bar|Player")
	TObjectPtr<UAbilityProgressWidget> CastBar;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Player Bar|Player")
	TObjectPtr<UAbilityBarWidget> LeftSlots;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Player Bar|Player")
	TObjectPtr<UAbilityBarWidget> PodSlots;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Player Bar|Player")
	TObjectPtr<UAbilityBarWidget> RightSlots;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Player Bar|Player")
	TObjectPtr<UTextBlock> LevelText;

	/** On top of the medallion, with MI_UI_MedallionGlow. Lights up on level-up. */
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Player Bar|Player")
	TObjectPtr<UImage> MedallionGlow;

	/** Image with MI_UI_Bar_XP inside the thin XP frame. */
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Player Bar|Player")
	TObjectPtr<UImage> XPFill;

	/** "38 %". */
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Player Bar|Player")
	TObjectPtr<UTextBlock> XPText;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Player Bar|Player")
	TObjectPtr<UTextBlock> PlayerNameText;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Player Bar|Player")
	TObjectPtr<UImage> PlayerPortrait;

	/* -------------------- Pet parts -------------------- */

	/** Everything in the pet panel; slides out of the crest while a companion is out. */
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Player Bar|Pet")
	TObjectPtr<UWidget> PetPanel;

	/** Closes the socket while no companion is out. */
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Player Bar|Pet")
	TObjectPtr<UImage> SocketGem;

	/** The pet's portrait (circular-masked material or a plain image). Gets the Portrait from the companion. */
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Player Bar|Pet")
	TObjectPtr<UImage> PetPortrait;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Player Bar|Pet")
	TObjectPtr<UTextBlock> PetLevelText;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Player Bar|Pet")
	TObjectPtr<UTextBlock> PetNameText;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Player Bar|Pet")
	TObjectPtr<UResourceWidget> PetHealthBar;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Player Bar|Pet")
	TObjectPtr<UPoiseBarWidget> PetStaggerBar;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Player Bar|Pet")
	TObjectPtr<UAbilityBarWidget> PetSlots;

	/* -------------------- Settings -------------------- */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player Bar|XP")
	FUIBarMaterialSettings XPBarSettings;

	/** Seconds the level-up glow takes to fade out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player Bar|Level", meta = (ClampMin = "0.0", Units = "s"))
	float LevelUpGlowDuration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player Bar|Level", AdvancedDisplay)
	FName GlowIntensityParameter = TEXT("Intensity");

	/** Seconds the pet panel takes to slide out (and back). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player Bar|Pet", meta = (ClampMin = "0.0", Units = "s"))
	float PetSlideDuration = 0.25f;

	/** Where the panel starts, relative to its place (negative = tucked left, behind the crest). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player Bar|Pet")
	float PetSlideOffset = -120.0f;

	/* -------------------- Events (optional extras) -------------------- */

	UFUNCTION(BlueprintImplementableEvent, Category = "Player Bar")
	void OnLevelUp(int32 NewLevel);

	/** A companion came out (Companion set) or went away (null). The slide is already handled. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Player Bar")
	void OnCompanionShown(ACompanionCharacter* Companion);

private:
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	UFUNCTION()
	void HandleExperienceChanged(int32 Experience, int32 ExperienceToNextLevel, int32 Level);

	UFUNCTION()
	void HandleLevelUp(int32 NewLevel);

	UFUNCTION()
	void HandleCompanionChanged(ACompanionCharacter* Companion);

	void BindPawn(APawn* NewPawn);
	void UnbindPawn();

	void RefreshExperience();
	void RefreshCompanionInfo();

	/** Points every pet part at Companion (null clears them) and starts the slide. */
	void ShowCompanion(ACompanionCharacter* Companion);

	void TickLevelGlow(float DeltaTime);
	void TickPetSlide(float DeltaTime);
	void ApplyPetSlide(float Alpha);

	UPROPERTY(Transient)
	TObjectPtr<APawn> BoundPawn;

	UPROPERTY(Transient)
	TObjectPtr<UExperienceComponent> ExperienceComponent;

	UPROPERTY(Transient)
	TObjectPtr<UCompanionComponent> Companions;

	UPROPERTY(Transient)
	TObjectPtr<ACompanionCharacter> ShownCompanion;

	UPROPERTY(Transient)
	FUIBarMaterial XPBar;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GlowMaterial;

	float GlowRemaining = 0.0f;

	/** 0 = tucked away, 1 = fully out. */
	float PetSlideAlpha = 0.0f;
	float PetSlideTarget = 0.0f;
};
