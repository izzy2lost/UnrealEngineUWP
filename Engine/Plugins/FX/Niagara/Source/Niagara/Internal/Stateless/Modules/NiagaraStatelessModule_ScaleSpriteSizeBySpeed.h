// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"
#include "Stateless/NiagaraStatelessParticleSimContext.h"

#include "NiagaraStatelessModule_ScaleSpriteSizeBySpeed.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Scale Sprite Size By Speed"))
class UNiagaraStatelessModule_ScaleSpriteSizeBySpeed : public UNiagaraStatelessModule
{
	GENERATED_BODY()

	struct FModuleBuiltData
	{
		float			VelocityNorm = 0.0f;
		FUintVector2	ScaleDistribution = FUintVector2::ZeroValue;

		int32			PositionVariableOffset = INDEX_NONE;
		int32			PreviousPositionVariableOffset = INDEX_NONE;
		int32			SpriteSizeVariableOffset = INDEX_NONE;
		int32			PreviousSpriteSizeVariableOffset = INDEX_NONE;
	};

public:
	using FParameters = NiagaraStateless::FScaleSpriteSizeBySpeedModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float VelocityThreshold = 1000.0f;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Scale", DisableRangeDistribution, DisableBindingDistribution))
	FNiagaraDistributionVector2 ScaleDistribution = FNiagaraDistributionVector2(1.0f);

	virtual void BuildEmitterData(const FNiagaraStatelessEmitterDataBuildContext& BuildContext) const override
	{
		FModuleBuiltData* BuiltData = BuildContext.AllocateBuiltData<FModuleBuiltData>();
		if (!IsModuleEnabled())
		{
			return;
		}

		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		BuiltData->PositionVariableOffset			= BuildContext.FindParticleVariableIndex(StatelessGlobals.PositionVariable);
		BuiltData->PreviousPositionVariableOffset	= BuildContext.FindParticleVariableIndex(StatelessGlobals.PreviousPositionVariable);
		BuiltData->SpriteSizeVariableOffset			= BuildContext.FindParticleVariableIndex(StatelessGlobals.SpriteSizeVariable);
		BuiltData->PreviousSpriteSizeVariableOffset	= BuildContext.FindParticleVariableIndex(StatelessGlobals.PreviousSpriteSizeVariable);

		if (BuiltData->SpriteSizeVariableOffset == INDEX_NONE && BuiltData->PreviousSpriteSizeVariableOffset == INDEX_NONE)
		{
			return;
		}

		BuiltData->VelocityNorm			= VelocityThreshold > 0.0f ? 1.0f / (VelocityThreshold * VelocityThreshold) : 0.0f;
		if (ScaleDistribution.IsCurve() && ScaleDistribution.Values.Num() > 1)
		{
			BuiltData->ScaleDistribution.X = BuildContext.AddStaticData(ScaleDistribution.Values);
			BuiltData->ScaleDistribution.Y = ScaleDistribution.Values.Num() - 1;
		}
		else
		{
			const FVector2f Values[] = { FVector2f::One(), ScaleDistribution.Values.Num() > 0 ? ScaleDistribution.Values[0] : FVector2f::One() };
			BuiltData->ScaleDistribution.X = BuildContext.AddStaticData(Values);
			BuiltData->ScaleDistribution.Y = 1;
		}
		BuildContext.AddParticleSimulationExecSimulate(&UNiagaraStatelessModule_ScaleSpriteSizeBySpeed::ParticleSimulate);
	}

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		const FModuleBuiltData* ModuleBuiltData = SetShaderParameterContext.ReadBuiltData<FModuleBuiltData>();

		FParameters* Parameters = SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
		Parameters->ScaleSpriteSizeBySpeed_VelocityNorm			= ModuleBuiltData->VelocityNorm;
		Parameters->ScaleSpriteSizeBySpeed_ScaleDistribution	= ModuleBuiltData->ScaleDistribution;
	}

	static void ParticleSimulate(const NiagaraStateless::FParticleSimulationContext& ParticleSimulationContext)
	{
		using namespace NiagaraStateless;

		const FModuleBuiltData* ModuleBuiltData = ParticleSimulationContext.ReadBuiltData<FModuleBuiltData>();

		for (uint32 i = 0; i < ParticleSimulationContext.GetNumInstances(); ++i)
		{
			const FVector3f Position			= ParticleSimulationContext.ReadParticleVariable(ModuleBuiltData->PositionVariableOffset, i, FVector3f::OneVector);
			const FVector3f PreviousPosition	= ParticleSimulationContext.ReadParticleVariable(ModuleBuiltData->PreviousPositionVariableOffset, i, FVector3f::OneVector);
			const float Speed					= (Position - PreviousPosition).SquaredLength() * ParticleSimulationContext.GetInvDeltaTime();
			const float NormSpeed				= FMath::Clamp(Speed * ModuleBuiltData->VelocityNorm, 0.0f, 1.0f);

			FVector2f SpriteSize			= ParticleSimulationContext.ReadParticleVariable(ModuleBuiltData->SpriteSizeVariableOffset, i, FVector2f::One());
			FVector2f PreviousSpriteSize	= ParticleSimulationContext.ReadParticleVariable(ModuleBuiltData->PreviousSpriteSizeVariableOffset, i, FVector2f::One());

			const FVector2f ScaleModifier = ParticleSimulationContext.LerpStaticFloat<FVector2f>(ModuleBuiltData->ScaleDistribution, NormSpeed);
			SpriteSize			*= ScaleModifier;
			PreviousSpriteSize	*= ScaleModifier;

			ParticleSimulationContext.WriteParticleVariable(ModuleBuiltData->SpriteSizeVariableOffset, i, SpriteSize);
			ParticleSimulationContext.WriteParticleVariable(ModuleBuiltData->PreviousSpriteSizeVariableOffset, i, PreviousSpriteSize);
		}
	}
#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }
#endif
#if WITH_EDITORONLY_DATA
	virtual void GetOutputVariables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		OutVariables.AddUnique(StatelessGlobals.SpriteSizeVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousSpriteSizeVariable);
	}
#endif
};
