// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"
#include "Stateless/Modules/NiagaraStatelessModuleCommon.h"

#include "NiagaraStatelessModule_AccelerationForce.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Acceleration Force"))
class UNiagaraStatelessModule_AccelerationForce : public UNiagaraStatelessModule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Parameters")
	FVector3f AccelerationMin = FVector3f(0.0f, 0.0f, 100.0f);
	UPROPERTY(EditAnywhere, Category = "Parameters")
	FVector3f AccelerationMax = FVector3f(0.0f, 0.0f, 100.0f);

	virtual void BuildEmitterData(FNiagaraStatelessEmitterDataBuildContext& BuildContext) const override
	{
		if (!IsModuleEnabled())
		{
			return;
		}
		NiagaraStateless::FPhysicsBuildData& PhysicsBuildData = BuildContext.GetTransientBuildData<NiagaraStateless::FPhysicsBuildData>();
		PhysicsBuildData.AccelerationMin += AccelerationMin;
		PhysicsBuildData.AccelerationMax += AccelerationMax;
	}

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }
#endif
};
