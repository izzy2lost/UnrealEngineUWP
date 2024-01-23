// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"
#include "Stateless/Modules/NiagaraStatelessModuleCommon.h"

#include "NiagaraStatelessModule_Drag.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Drag"))
class UNiagaraStatelessModule_Drag : public UNiagaraStatelessModule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Parameters")
	float DragMin = 0.1f;
	UPROPERTY(EditAnywhere, Category = "Parameters")
	float DragMax = 1.0f;

	virtual void BuildEmitterData(FNiagaraStatelessEmitterDataBuildContext& BuildContext) const override
	{
		if (!IsModuleEnabled())
		{
			return;
		}

		NiagaraStateless::FPhysicsBuildData& PhysicsBuildData = BuildContext.GetTransientBuildData<NiagaraStateless::FPhysicsBuildData>();
		PhysicsBuildData.DragMin += DragMin;
		PhysicsBuildData.DragMax += DragMax;
	}

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }
#endif
};
