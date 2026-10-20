#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "CompanionTactics.generated.h"

class ABaseCharacter;
class ACompanionAIController;
class ACompanionCharacter;
class UCompanionConsideration;

/** What a tactic is for. Later stages scale tactics by what the leader needs (and what the wolf has learned). */
UENUM(BlueprintType)
enum class ECompanionPurpose : uint8
{
	/** Hurt enemies, help the leader's focus. */
	Damage UMETA(DisplayName = "Damage"),

	/** Peel enemies off the leader. */
	Protect UMETA(DisplayName = "Protect"),

	/** Slow, stagger, hold enemies. */
	Control UMETA(DisplayName = "Control"),

	/** Buffs and help for the leader and itself. */
	Support UMETA(DisplayName = "Support"),

	/** Finish weakened or broken enemies. */
	Finish UMETA(DisplayName = "Finish")
};

/** Who a tactic is used on. */
UENUM(BlueprintType)
enum class ECompanionTacticTarget : uint8
{
	/** One of the enemies in the fight: scored per enemy, the wolf goes to it and faces it. */
	Enemy UMETA(DisplayName = "Enemy"),

	/** No target: used where the wolf stands (a howl, a self buff). */
	Self UMETA(DisplayName = "Self"),

	/** The leader: the wolf goes within Max Range of the leader first (a heal, a guard). */
	Leader UMETA(DisplayName = "Leader")
};

/**
 * "Teaching" the wolf one of its abilities: what it's for, and when it's a good idea.
 * Every think, each ready tactic is scored for each possible target:
 *   Base Weight x every Consideration x distance and focus preferences.
 * The best score is what the wolf does.
 */
USTRUCT(BlueprintType)
struct ABILITYSYSTEM_API FCompanionTactic
{
	GENERATED_BODY()

	/** Ability id of one of the wolf's granted abilities (Ability.Wolf.Bite). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactic", meta = (Categories = "Ability"))
	FGameplayTag AbilityTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactic")
	ECompanionPurpose Purpose = ECompanionPurpose::Damage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactic")
	ECompanionTacticTarget TargetType = ECompanionTacticTarget::Enemy;

	/** How much the wolf likes this tactic before the situation is considered. A basic bite ~0.5, a signature move ~1.5. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactic", meta = (ClampMin = "0.0"))
	float BaseWeight = 1.0f;

	/** The wolf uses this from this level on (its level follows the leader's). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactic", meta = (ClampMin = "1"))
	int32 UnlockLevel = 1;

	/** Seconds after a use before the wolf considers it again, on top of the ability's cooldown. Stops buff spam. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactic", meta = (ClampMin = "0.0", Units = "s"))
	float MinInterval = 0.0f;

	/** Enemy: gap to the target (edge to edge) where it can be used. Leader: how close to the leader it must be. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactic|Range", meta = (ClampMin = "0", Units = "cm"))
	float MinRange = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactic|Range", meta = (ClampMin = "0", Units = "cm"))
	float MaxRange = 150.0f;

	/** Enemy: off = only used when the target is already in range (never runs in for it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactic|Range")
	bool bChaseIntoRange = true;

	/** Enemy: needs a clear view of the target; without one, the wolf moves in until it has it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactic|Range")
	bool bRequiresLineOfSight = true;

	/**
	 * Enemy: used on the run. The wolf doesn't stop when it gets in range; it keeps chasing the target
	 * while the ability plays (a running bite). The montage must not use root motion for this, so the
	 * movement goes on. Off = stops, turns to face, then uses it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactic|Range", meta = (EditCondition = "TargetType == ECompanionTacticTarget::Enemy"))
	bool bUseWhileMoving = false;

	/** When it's a good idea. Each one multiplies the score; an empty list = always fine. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Tactic")
	TArray<TObjectPtr<UCompanionConsideration>> Considerations;
};

/** Everything a consideration may look at, for one tactic and one target. */
struct ABILITYSYSTEM_API FCompanionTacticContext
{
	const ACompanionAIController* Brain = nullptr;
	const ACompanionCharacter* Companion = nullptr;
	const ABaseCharacter* Leader = nullptr;

	/** The enemy being scored (Enemy tactics), the leader (Leader tactics), or null (Self). */
	const AActor* Target = nullptr;

	/** Edge-to-edge gap from the wolf to Target. 0 without a target. */
	float Gap = 0.0f;

	/** Health / max health of Actor, or 1 when it has none. */
	static float GetHealthFraction(const AActor* Actor);

	/** Status and state tags on Actor (its effects, plus its ability component's owner tags). */
	static FGameplayTagContainer GetActorTags(const AActor* Actor);
};

/**
 * One reason a tactic is (or isn't) a good idea right now.
 * Scores Score If Met when its condition holds, Score If Not Met otherwise. Leave Not Met at 0 for a hard
 * requirement ("only when..."), or raise it for a preference ("better when...", e.g. 0.3).
 * Met above 1 boosts the tactic ("much better when...").
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class ABILITYSYSTEM_API UCompanionConsideration : public UObject
{
	GENERATED_BODY()

public:
	virtual float Score(const FCompanionTacticContext& Context) const;

protected:
	virtual bool IsMet(const FCompanionTacticContext& Context) const { return false; }

	UPROPERTY(EditAnywhere, Category = "Score", meta = (ClampMin = "0.0"))
	float ScoreIfMet = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Score", meta = (ClampMin = "0.0"))
	float ScoreIfNotMet = 0.0f;
};

/* ============================== Leader ============================== */

/** The leader's health is below Fraction (0.5 = half). */
UCLASS(meta = (DisplayName = "Leader Health Below"))
class ABILITYSYSTEM_API UConsideration_LeaderHealthBelow : public UCompanionConsideration
{
	GENERATED_BODY()

protected:
	virtual bool IsMet(const FCompanionTacticContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Fraction = 0.5f;
};

/** At least Count living enemies within Radius of the leader. */
UCLASS(meta = (DisplayName = "Enemies Near Leader"))
class ABILITYSYSTEM_API UConsideration_EnemiesNearLeader : public UCompanionConsideration
{
	GENERATED_BODY()

protected:
	virtual bool IsMet(const FCompanionTacticContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (ClampMin = "1"))
	int32 Count = 3;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (ClampMin = "0", Units = "cm"))
	float Radius = 600.0f;
};

/** The leader has any of Tags (a status, a state, e.g. while channelling if your channel grants a tag). */
UCLASS(meta = (DisplayName = "Leader Has Tags"))
class ABILITYSYSTEM_API UConsideration_LeaderHasTags : public UCompanionConsideration
{
	GENERATED_BODY()

protected:
	virtual bool IsMet(const FCompanionTacticContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition")
	FGameplayTagContainer Tags;
};

/** The leader is using an ability with any of Ability Tags right now (e.g. Rapid Fire's channel). Empty = any ability. */
UCLASS(meta = (DisplayName = "Leader Using Ability"))
class ABILITYSYSTEM_API UConsideration_LeaderUsingAbility : public UCompanionConsideration
{
	GENERATED_BODY()

protected:
	virtual bool IsMet(const FCompanionTacticContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (Categories = "Ability"))
	FGameplayTagContainer AbilityTags;
};

/** What the leader needs right now (the companion's Needs Component) is at least Threshold for Purpose. */
UCLASS(meta = (DisplayName = "Leader Need Above"))
class ABILITYSYSTEM_API UConsideration_LeaderNeedAbove : public UCompanionConsideration
{
	GENERATED_BODY()

protected:
	virtual bool IsMet(const FCompanionTacticContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition")
	ECompanionPurpose Need = ECompanionPurpose::Protect;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Threshold = 0.5f;
};

/* ============================== Self ============================== */

/** The wolf's own health is below Fraction. */
UCLASS(meta = (DisplayName = "Self Health Below"))
class ABILITYSYSTEM_API UConsideration_SelfHealthBelow : public UCompanionConsideration
{
	GENERATED_BODY()

protected:
	virtual bool IsMet(const FCompanionTacticContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Fraction = 0.5f;
};

/** The fight began less than Within seconds ago (an opening howl). */
UCLASS(meta = (DisplayName = "Fight Just Started"))
class ABILITYSYSTEM_API UConsideration_FightJustStarted : public UCompanionConsideration
{
	GENERATED_BODY()

protected:
	virtual bool IsMet(const FCompanionTacticContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (ClampMin = "0", Units = "s"))
	float Within = 4.0f;
};

/* ============================== Target ============================== */

/** The target has any of Tags: Status.Marked, State.Staggered (poise broken), Status.Crippled... */
UCLASS(meta = (DisplayName = "Target Has Tags"))
class ABILITYSYSTEM_API UConsideration_TargetHasTags : public UCompanionConsideration
{
	GENERATED_BODY()

protected:
	virtual bool IsMet(const FCompanionTacticContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition")
	FGameplayTagContainer Tags;
};

/** The target's health is below Fraction (finishers). */
UCLASS(meta = (DisplayName = "Target Health Below"))
class ABILITYSYSTEM_API UConsideration_TargetHealthBelow : public UCompanionConsideration
{
	GENERATED_BODY()

protected:
	virtual bool IsMet(const FCompanionTacticContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Fraction = 0.3f;
};

/** The target is going for the leader: it hit them in the last Within seconds, or it's targeting them. */
UCLASS(meta = (DisplayName = "Target Threatens Leader"))
class ABILITYSYSTEM_API UConsideration_TargetThreatensLeader : public UCompanionConsideration
{
	GENERATED_BODY()

protected:
	virtual bool IsMet(const FCompanionTacticContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (ClampMin = "0", Units = "s"))
	float Within = 3.0f;
};

/** The target is going for the wolf itself. */
UCLASS(meta = (DisplayName = "Target Threatens Self"))
class ABILITYSYSTEM_API UConsideration_TargetThreatensSelf : public UCompanionConsideration
{
	GENERATED_BODY()

protected:
	virtual bool IsMet(const FCompanionTacticContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (ClampMin = "0", Units = "s"))
	float Within = 3.0f;
};

/** The target is the leader's current target (help them focus). */
UCLASS(meta = (DisplayName = "Target Is Leader's Target"))
class ABILITYSYSTEM_API UConsideration_TargetIsLeaderTarget : public UCompanionConsideration
{
	GENERATED_BODY()

protected:
	virtual bool IsMet(const FCompanionTacticContext& Context) const override;
};

/** At least Count enemies (the target included) within Radius of the target: area attacks. */
UCLASS(meta = (DisplayName = "Enemies Near Target"))
class ABILITYSYSTEM_API UConsideration_EnemiesNearTarget : public UCompanionConsideration
{
	GENERATED_BODY()

protected:
	virtual bool IsMet(const FCompanionTacticContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (ClampMin = "1"))
	int32 Count = 3;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (ClampMin = "0", Units = "cm"))
	float Radius = 400.0f;
};

/** The target moves faster than Speed: running away, charging in, kiting. */
UCLASS(meta = (DisplayName = "Target Is Moving"))
class ABILITYSYSTEM_API UConsideration_TargetIsMoving : public UCompanionConsideration
{
	GENERATED_BODY()

protected:
	virtual bool IsMet(const FCompanionTacticContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (ClampMin = "0", Units = "cm/s"))
	float Speed = 150.0f;
};

/** The target is within Distance of the leader (bodyguard: deal with what's close to them first). */
UCLASS(meta = (DisplayName = "Target Near Leader"))
class ABILITYSYSTEM_API UConsideration_TargetNearLeader : public UCompanionConsideration
{
	GENERATED_BODY()

protected:
	virtual bool IsMet(const FCompanionTacticContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (ClampMin = "0", Units = "cm"))
	float Distance = 400.0f;
};
