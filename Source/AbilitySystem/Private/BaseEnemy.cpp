#include "BaseEnemy.h"

#include "EnemyAIController.h"
#include "LootDropComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

ABaseEnemy::ABaseEnemy()
{
	LootDropComponent = CreateDefaultSubobject<ULootDropComponent>(TEXT("LootDropComponent"));

	TeamId = 1;
	AIControllerClass = AEnemyAIController::StaticClass();
}

void ABaseEnemy::BeginPlay()
{
	Super::BeginPlay();
	HomeLocation = GetActorLocation();
}

#if WITH_EDITOR
EDataValidationResult ABaseEnemy::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (RoamWaitMax < RoamWaitMin)
	{
		Context.AddError(NSLOCTEXT("Enemy", "BadRoamWait", "Roam Wait Max is smaller than Min."));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
