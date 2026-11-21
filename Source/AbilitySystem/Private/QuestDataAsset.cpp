#include "QuestDataAsset.h"

#include "ItemDataAsset.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult UQuestDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (Title.IsEmpty())
	{
		Context.AddError(NSLOCTEXT("Quest", "NoTitle", "The quest has no Title."));
		Result = EDataValidationResult::Invalid;
	}

	if (Objectives.IsEmpty())
	{
		Context.AddWarning(NSLOCTEXT("Quest", "NoObjectives", "The quest has no Objectives: it can be handed in the moment it is taken."));
	}

	for (int32 Index = 0; Index < Objectives.Num(); ++Index)
	{
		const FQuestObjective& Objective = Objectives[Index];

		const bool bMissingTarget =
			(Objective.Type == EQuestObjectiveType::Kill && !Objective.TargetClass)
			|| (Objective.Type == EQuestObjectiveType::Collect && !IsValid(Objective.Item))
			|| (Objective.Type == EQuestObjectiveType::WorldFlag && Objective.Flag.IsNone())
			|| (Objective.Type == EQuestObjectiveType::TalkTo && Objective.NpcId.IsNone());

		if (bMissingTarget)
		{
			Context.AddError(FText::Format(NSLOCTEXT("Quest", "NoTarget", "Objectives[{0}] doesn't say what to kill, collect, reach or talk to."), Index));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif
