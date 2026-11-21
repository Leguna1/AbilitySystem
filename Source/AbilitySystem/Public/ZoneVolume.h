#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoneVolume.generated.h"

class APawn;
class UBoxComponent;

/**
 * A named area of the world. Walking in shows its name across the screen; the first time it also
 * counts as discovered, which gives experience and sets the world flag Zone Id (quests can ask for it:
 * "Find the Sunken Shrine").
 *
 * Place one over each village, altar site, boss zone and landmark, and scale the box to cover it.
 */
UCLASS()
class ABILITYSYSTEM_API AZoneVolume : public AActor
{
	GENERATED_BODY()

public:
	AZoneVolume();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** For Blueprint extras: music, ambience, fog color. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Zone")
	void OnPlayerEntered(APawn* PlayerPawn, bool bFirstTime);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> Area;

	/** World flag set on discovery. Must be unique per zone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone")
	FName ZoneId = TEXT("Zone.One");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone")
	FText ZoneName;

	/** Second line under the name, e.g. "Recommended level 5 - 8". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone")
	FText Subtitle;

	/** Experience for finding the zone the first time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone", meta = (ClampMin = "0"))
	int32 DiscoveryExperience = 50;

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** The player may start the game already standing in the zone, before anything could announce it. */
	UFUNCTION()
	void HandlePlayerReady(APawn* PlayerPawn);

	void Enter(APawn* PlayerPawn);
};
