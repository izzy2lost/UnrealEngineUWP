// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"
#include "Stateless/NiagaraStatelessDistribution.h"

#include "NiagaraSystemEmitterState.generated.h"

UENUM()
enum class ENiagaraSystemInactiveResponse
{
	/** Let Emitters Finish Then Kill Emitter */
	Complete,
	/** Emitter & Particles Die Immediatly */
	Kill,
};

UENUM()
enum class ENiagaraEmitterInactiveResponse
{
	/** Let Particles Finish Then Kill Emitter */
	Complete,
	/** Emitter & Particles Die Immediatly */
	Kill,
	/** Emitter deactivates but doesn't die until the system does */
	//Continue,
};

UENUM()
enum class ENiagaraLoopBehavior
{
	Infinite,
	Multiple,
	Once,
};

USTRUCT()
struct FNiagaraSystemStateData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "System State")
	uint32 bRunSpawnScript : 1 = true;

	UPROPERTY(EditAnywhere, Category = "System State")
	uint32 bRunUpdateScript : 1 = true;

	UPROPERTY(EditAnywhere, Category = "System State")
	uint32 bIgnoreSystemState : 1 = true;

	UPROPERTY(EditAnywhere, Category = "System State", meta = (DisplayAfter = "LoopCount", EditCondition = "LoopBehavior != ENiagaraLoopBehavior::Once", EditConditionHides))
	uint32 bRecalculateDurationEachLoop : 1 = false;

	UPROPERTY(EditAnywhere, Category = "System State", meta = (DisplayAfter = "LoopDelay", EditCondition = "LoopBehavior != ENiagaraLoopBehavior::Once", EditConditionHides))
	uint32 bDelayFirstLoopOnly : 1 = false;

	UPROPERTY(EditAnywhere, Category = "System State", meta = (DisplayAfter = "bDelayFirstLoopOnly", EditCondition = "LoopBehavior != ENiagaraLoopBehavior::Once && !bDelayFirstLoopOnly", EditConditionHides))
	uint32 bRecalculateDelayEachLoop : 1 = false;

	UPROPERTY(EditAnywhere, Category = "System State")
	ENiagaraSystemInactiveResponse InactiveResponse = ENiagaraSystemInactiveResponse::Complete;

	UPROPERTY(EditAnywhere, Category = "System State")
	ENiagaraLoopBehavior LoopBehavior = ENiagaraLoopBehavior::Once;

	UPROPERTY(EditAnywhere, Category = "System State", meta = (ClampMin = "0.0", Units="s"))
	FNiagaraDistributionRangeFloat LoopDuration = FNiagaraDistributionRangeFloat(0.0f);

	UPROPERTY(EditAnywhere, Category = "System State", meta = (ClampMin = "1", EditCondition = "LoopBehavior == ENiagaraLoopBehavior::Multiple", EditConditionHides))
	int LoopCount = 1;

	UPROPERTY(EditAnywhere, Category = "System State", meta = (ClampMin = "0.0", Units="s"))
	FNiagaraDistributionRangeFloat LoopDelay = FNiagaraDistributionRangeFloat(0.0f);
};

USTRUCT()
struct FNiagaraEmitterStateData
{
	GENERATED_BODY()
		
	//UPROPERTY(EditAnywhere, Category="Emitter State")
	//ENiagaraStatelessEmitterState_SelfSystem LifeCycleMode = ENiagaraStatelessEmitterState_SelfSystem::Self;

	UPROPERTY(EditAnywhere, Category="Emitter State", meta=(SegmentedDisplay))
	ENiagaraEmitterInactiveResponse InactiveResponse = ENiagaraEmitterInactiveResponse::Complete;

	UPROPERTY(EditAnywhere, Category="Emitter State", meta=(SegmentedDisplay))
	ENiagaraLoopBehavior LoopBehavior = ENiagaraLoopBehavior::Infinite;

	UPROPERTY(EditAnywhere, Category = "Emitter State", meta = (ClampMin = "1", EditCondition = "LoopBehavior == ENiagaraLoopBehavior::Multiple", EditConditionHides))
	int32 LoopCount = 1;

	UPROPERTY(EditAnywhere, Category = "Emitter State", meta = (ClampMin = "0.0", Units="s"))
	FNiagaraDistributionRangeFloat LoopDuration = FNiagaraDistributionRangeFloat(1.0f);

	UPROPERTY(EditAnywhere, Category = "Emitter State", meta = (ClampMin = "0.0", Units="s"))
	FNiagaraDistributionRangeFloat LoopDelay = FNiagaraDistributionRangeFloat(0.0f);

	UPROPERTY(EditAnywhere, Category = "Emitter State", meta = (EditCondition = "LoopBehavior != ENiagaraLoopBehavior::Once", EditConditionHides, DisplayAfter = "LoopDuration"))
	uint32 bRecalculateDurationEachLoop : 1 = false;

	UPROPERTY(EditAnywhere, Category = "Emitter State", meta = (EditCondition = "LoopBehavior != ENiagaraLoopBehavior::Once", EditConditionHides))
	uint32 bDelayFirstLoopOnly : 1 = false;

	UPROPERTY(EditAnywhere, Category = "Emitter State", meta = (EditCondition = "LoopBehavior != ENiagaraLoopBehavior::Once && !bDelayFirstLoopOnly", EditConditionHides))
	uint32 bRecalculateDelayEachLoop : 1 = false;

	UPROPERTY(EditAnywhere, Category = "Scalability")
	uint32 bEnableDistanceCulling : 1 = false;

	UPROPERTY(EditAnywhere, Category = "Scalability", meta = (DisplayAfter = "MaxDistanceReaction"))
	uint32 bEnableVisibilityCulling : 1 = false;

	UPROPERTY(EditAnywhere, Category = "Scalability", meta = (EditCondition = "bEnableDistanceCulling", EditConditionHides))
	uint32 bMinDistanceEnabled : 1 = false;

	UPROPERTY(EditAnywhere, Category = "Scalability", meta = (EditCondition = "bEnableDistanceCulling", EditConditionHides))
	uint32 bMaxDistanceEnabled : 1 = false;

	UPROPERTY(EditAnywhere, Category = "Scalability", meta = (DisplayAfter = "VisibilityCullDelay"))
	uint32 bResetAgeOnAwaken : 1 = false;

	UPROPERTY(EditAnywhere, Category = "Scalability", meta = (EditCondition = "bEnableDistanceCulling && bMinDistanceEnabled", EditConditionHides))
	float MinDistance = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Scalability", meta = (EditCondition = "bEnableDistanceCulling && bMinDistanceEnabled", EditConditionHides))
	ENiagaraExecutionStateManagement MinDistanceReaction = ENiagaraExecutionStateManagement::Awaken;

	UPROPERTY(EditAnywhere, Category = "Scalability", meta = (EditCondition = "bEnableDistanceCulling && bMaxDistanceEnabled", EditConditionHides))
	float MaxDistance = 5000.0f;

	UPROPERTY(EditAnywhere, Category = "Scalability", meta = (EditCondition = "bEnableDistanceCulling && bMaxDistanceEnabled", EditConditionHides))
	ENiagaraExecutionStateManagement MaxDistanceReaction = ENiagaraExecutionStateManagement::SleepAndLetParticlesFinish;

	UPROPERTY(EditAnywhere, Category = "Scalability", meta = (EditCondition = "bEnableVisibilityCulling", EditConditionHides))
	ENiagaraExecutionStateManagement VisibilityCullReaction = ENiagaraExecutionStateManagement::SleepAndLetParticlesFinish;

	UPROPERTY(EditAnywhere, Category = "Scalability", meta = (EditCondition = "bEnableVisibilityCulling", EditConditionHides))
	float VisibilityCullDelay = 1.0f;
};
