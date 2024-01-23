// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"

#include "NiagaraStatelessModule_ScaleMeshSize.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Scale Mesh Size"))
class UNiagaraStatelessModule_ScaleMeshSize : public UNiagaraStatelessModule
{
	GENERATED_BODY()

	struct FModuleBuiltData
	{
		int32	TableOffset = 0;
		int32	TableLength = 0;
	};

public:
	using FParameters = NiagaraStateless::FScaleMeshSizeModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	TArray<FVector3f>	ScaleValues = { FVector3f::OneVector, FVector3f::OneVector, FVector3f::OneVector, FVector3f::ZeroVector };

	virtual void BuildEmitterData(FNiagaraStatelessEmitterDataBuildContext& BuildContext) const override
	{
		FModuleBuiltData* BuiltData = BuildContext.AllocateBuiltData<FModuleBuiltData>();
		if (IsModuleEnabled())
		{
			BuiltData->TableOffset = BuildContext.AddStaticData(ScaleValues);
			BuiltData->TableLength = ScaleValues.Num();
		}
	}

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		const FModuleBuiltData* ModuleBuiltData = SetShaderParameterContext.ReadBuiltData<FModuleBuiltData>();

		FParameters* Parameters = SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
		Parameters->ScaleMeshSize_Offset = ModuleBuiltData->TableOffset;
		Parameters->ScaleMeshSize_Length = ModuleBuiltData->TableLength;
	}

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }
#endif
#if WITH_EDITORONLY_DATA
	virtual void GetOutputVaruables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		OutVariables.AddUnique(StatelessGlobals.ScaleVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousScaleVariable);
	}
#endif
};
