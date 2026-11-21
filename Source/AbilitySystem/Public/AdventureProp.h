#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractableInterface.h"
#include "AdventureProp.generated.h"

class UAdventureComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UWorldProgressSubsystem;

/** The engine's basic shapes, used as stand-in art so every prop is visible the moment it is placed. */
UENUM()
enum class EAdventurePropShape : uint8
{
	Cube,
	Sphere,
	Cylinder,
	Cone
};

/**
 * Parent of the game's world objects: altars, chests, gates, switches, waystones, shrines.
 *
 * Gives each one a mesh (an engine basic shape until you set your own), a name floating above it that
 * turns to the camera, and a colored light that shows its state. It registers itself as interactable.
 *
 * To use your own art: make a Blueprint child, select Mesh and set its Static Mesh and materials.
 * Turn Show Label off if the mesh speaks for itself.
 */
UCLASS(Abstract)
class ABILITYSYSTEM_API AAdventureProp : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	AAdventureProp();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Prop")
	FText GetDisplayName() const { return DisplayName; }

	/* IInteractableInterface: nothing to do by default; children override what they need. */
	virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override { return FText::GetEmpty(); }
	virtual bool CanInteract_Implementation(AActor* Interactor) const override { return false; }
	virtual void Interact_Implementation(AActor* Interactor) override {}

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Called from a child's constructor: which basic shape stands in, how big, and how high the label floats. */
	void SetPlaceholder(EAdventurePropShape Shape, const FVector& Scale, float LabelHeight);

	/** The light's color and the mesh's tint (the tint only shows on the stand-in material). */
	UFUNCTION(BlueprintCallable, Category = "Prop")
	void SetStateColor(FLinearColor Color);

	/** Light on or off (an opened chest goes dark). */
	UFUNCTION(BlueprintCallable, Category = "Prop")
	void SetGlowEnabled(bool bEnabled);

	/** Name, plus a second line when Status isn't empty ("Altar of Dawn" / "Cleansed"). */
	UFUNCTION(BlueprintCallable, Category = "Prop")
	void SetLabelStatus(const FText& Status);

	UAdventureComponent* GetAdventure() const;
	UWorldProgressSubsystem* GetProgress() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** What the player sees and bumps into. Set your own Static Mesh in a Blueprint child. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> Glow;

	/** Shown above the prop, in prompts and in messages. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prop")
	FText DisplayName;

	/** The floating name. Off for props whose mesh speaks for itself. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prop")
	bool bShowLabel = true;

private:
	FText LabelStatus;
	void RefreshLabel();
};
