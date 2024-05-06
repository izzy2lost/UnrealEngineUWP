// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"
#include "Stateless/NiagaraStatelessParticleSimContext.h"

#include "NiagaraStatelessModule_ScaleMeshSizeBySpeed.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Scale Mesh Size By Speed"))
class UNiagaraStatelessModule_ScaleMeshSizeBySpeed : public UNiagaraStatelessModule
{
	GENERATED_BODY()

	struct FModuleBuiltData
	{
		float			VelocityNorm = 0.0f;
		FUintVector2	ScaleDistribution = FUintVector2::ZeroValue;

		int32			PositionVariableOffset = INDEX_NONE;
		int32			PreviousPositionVariableOffset = INDEX_NONE;
		int32			ScaleVariableVariableOffset = INDEX_NONE;
		int32			PreviousScaleVariableVariableOffset = INDEX_NONE;
	};

public:
	using FParameters = NiagaraStateless::FScaleMeshSizeBySpeedModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float VelocityThreshold = 1000.0f;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Scale", DisableRangeDistribution, DisableBindingDistribution))
	FNiagaraDistributionVector3 ScaleDistribution = FNiagaraDistributionVector3(1.0f);

	virtual void BuildEmitterData(const FNiagaraStatelessEmitterDataBuildContext& BuildContext) const override
	{
		FModuleBuiltData* BuiltData = BuildContext.AllocateBuiltData<FModuleBuiltData>();
		if (!IsModuleEnabled())
		{
			return;
		}

		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		BuiltData->PositionVariableOffset				= BuildContext.FindParticleVariableIndex(StatelessGlobals.PositionVariable);
		BuiltData->PreviousPositionVariableOffset		= BuildContext.FindParticleVariableIndex(StatelessGlobals.PreviousPositionVariable);
		BuiltData->ScaleVariableVariableOffset			= BuildContext.FindParticleVariableIndex(StatelessGlobals.ScaleVariable);
		BuiltData->PreviousScaleVariableVariableOffset	= BuildContext.FindParticleVariableIndex(StatelessGlobals.PreviousScaleVariable);

		if (BuiltData->ScaleVariableVariableOffset == INDEX_NONE && BuiltData->PreviousScaleVariableVariableOffset == INDEX_NONE)
		{
			return;
		}

		BuiltData->VelocityNorm = VelocityThreshold > 0.0f ? 1.0f / (VelocityThreshold * VelocityThreshold) : 0.0f;
		if (ScaleDistribution.IsCurve() && ScaleDistribution.Values.Num() > 1)
		{
			BuiltData->ScaleDistribution.X = BuildContext.AddStaticData(ScaleDistribution.Values);
			BuiltData->ScaleDistribution.Y = ScaleDistribution.Values.Num() - 1;
		}
		else
		{
			const FVector3f Values[] = { FVector3f::One(), ScaleDistribution.Values.Num() > 0 ? ScaleDistribution.Values[0] : FVector3f::One() };
			BuiltData->ScaleDistribution.X = BuildContext.AddStaticData(Values);
			BuiltData->ScaleDistribution.Y = 1;
		}
		BuildContext.AddParticleSimulationExecSimulate(&UNiagaraStatelessModule_ScaleMeshSizeBySpeed::ParticleSimulate);
	}

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		const FModuleBuiltData* ModuleBuiltData = SetShaderParameterContext.ReadBuiltData<FModuleBuiltData>();

		FParameters* Parameters = SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
		Parameters->ScaleMeshSizeBySpeed_VelocityNorm		= ModuleBuiltData->VelocityNorm;
		Parameters->ScaleMeshSizeBySpeed_ScaleDistribution	= ModuleBuiltData->ScaleDistribution;
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

			FVector3f Scale			= ParticleSimulationContext.ReadParticleVariable(ModuleBuiltData->ScaleVariableVariableOffset, i, FVector3f::OneVector);
			FVector3f PreviousScale	= ParticleSimulationContext.ReadParticleVariable(ModuleBuiltData->PreviousScaleVariableVariableOffset, i, FVector3f::OneVector);

			const FVector3f ScaleModifier = ParticleSimulationContext.LerpStaticFloat<FVector3f>(ModuleBuiltData->ScaleDistribution, NormSpeed);
			Scale			*= ScaleModifier;
			PreviousScale	*= ScaleModifier;

			ParticleSimulationContext.WriteParticleVariable(ModuleBuiltData->ScaleVariableVariableOffset, i, Scale);
			ParticleSimulationContext.WriteParticleVariable(ModuleBuiltData->PreviousScaleVariableVariableOffset, i, PreviousScale);
		}
	}

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }
#endif
#if WITH_EDITORONLY_DATA
	virtual void GetOutputVariables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		OutVariables.AddUnique(StatelessGlobals.ScaleVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousScaleVariable);
	}
#endif
};
