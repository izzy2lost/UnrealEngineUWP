// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"

#include "NiagaraStatelessModule_SpriteRotationRate.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Sprite Rotation Rate"))
class UNiagaraStatelessModule_SpriteRotationRate : public UNiagaraStatelessModule
{
	GENERATED_BODY()

public:
	using FParameters = NiagaraStateless::FSpriteRotationRateModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	float	RotationRateMin = 360.0f;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	float	RotationRateMax = 360.0f;

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		FParameters* Parameters = SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
		if (IsModuleEnabled())
		{
			Parameters->SpriteRotationRate_Scale	= RotationRateMax - RotationRateMin;
			Parameters->SpriteRotationRate_Bias		= RotationRateMin;
		}
		else
		{
			Parameters->SpriteRotationRate_Scale	= 0.0f;
			Parameters->SpriteRotationRate_Bias		= 0.0f;
		}
	}

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }
#endif
#if WITH_EDITORONLY_DATA
	virtual void GetOutputVaruables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		OutVariables.AddUnique(StatelessGlobals.SpriteRotationVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousSpriteRotationVariable);
	}
#endif
};
