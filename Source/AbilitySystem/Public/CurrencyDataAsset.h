#pragma once

#include "CoreMinimal.h"
#include "ItemDataAsset.h"
#include "CurrencyDataAsset.generated.h"

/**
 * Money, e.g. Gold. Picked up like any item, but it never takes a bag slot: the Inventory Component
 * keeps a running total per currency (its wallet) that has no limit and never fills up.
 *
 * Gold coins as loot: make DA_Gold from this class (Icon, Pickup Mesh of a coin pile), then add it to a
 * Loot Drop Component's Loot list with Min Count / Max Count as the range of coins dropped
 * (e.g. 5 to 20). The whole roll lies on the ground as one pickup.
 */
UCLASS(BlueprintType)
class ABILITYSYSTEM_API UCurrencyDataAsset : public UItemDataAsset
{
	GENERATED_BODY()

public:
	UCurrencyDataAsset();

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
