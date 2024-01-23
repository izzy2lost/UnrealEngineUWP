// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"
#include "Stateless/Modules/NiagaraStatelessModuleCommon.h"

#include "NiagaraStatelessModule_GravityForce.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Gravity Force"))
class UNiagaraStatelessModule_GravityForce : public UNiagaraStatelessModule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Parameters")
	FVector3f GravityForceMin = FVector3f(0.0f, 0.0f, -980.0f);
	UPROPERTY(EditAnywhere, Category = "Parameters")
	FVector3f GravityForceMax = FVector3f(0.0f, 0.0f, -980.0f);

	virtual void BuildEmitterData(FNiagaraStatelessEmitterDataBuildContext& BuildContext) const override
	{
		if (!IsModuleEnabled())
		{
			return;
		}
		NiagaraStateless::FPhysicsBuildData& PhysicsBuildData = BuildContext.GetTransientBuildData<NiagaraStateless::FPhysicsBuildData>();
		PhysicsBuildData.AccelerationMin += GravityForceMin;
		PhysicsBuildData.AccelerationMax += GravityForceMax;
	}

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }
#endif
};
