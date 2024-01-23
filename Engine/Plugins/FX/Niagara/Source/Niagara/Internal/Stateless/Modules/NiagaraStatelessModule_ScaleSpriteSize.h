// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"

#include "NiagaraStatelessModule_ScaleSpriteSize.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Scale Sprite Size"))
class UNiagaraStatelessModule_ScaleSpriteSize : public UNiagaraStatelessModule
{
	GENERATED_BODY()

	struct FModuleBuiltData
	{
		int32	TableOffset = 0;
		int32	TableLength = 0;
	};

public:
	using FParameters = NiagaraStateless::FScaleSpriteSizeModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	TArray<FVector2f>	ScaleValues = {FVector2f::One(), FVector2f::One(), FVector2f::One(), FVector2f::ZeroVector};

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
		Parameters->ScaleSpriteSize_Offset = ModuleBuiltData->TableOffset;
		Parameters->ScaleSpriteSize_Length = ModuleBuiltData->TableLength;
	}

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }
#endif
#if WITH_EDITORONLY_DATA
	virtual void GetOutputVaruables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		OutVariables.AddUnique(StatelessGlobals.SpriteSizeVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousSpriteSizeVariable);
	}
#endif
};
