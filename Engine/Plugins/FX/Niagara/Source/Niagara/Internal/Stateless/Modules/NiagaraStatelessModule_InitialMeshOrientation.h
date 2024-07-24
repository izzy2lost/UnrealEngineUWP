// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"
#include "Stateless/NiagaraStatelessParticleSimContext.h"

#include "NiagaraStatelessModule_InitialMeshOrientation.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Initial Mesh Orientation"))
class UNiagaraStatelessModule_InitialMeshOrientation : public UNiagaraStatelessModule
{
	GENERATED_BODY()

	struct FModuleBuiltData
	{
		FVector3f	Rotation = FVector3f::ZeroVector;
		FVector3f	RandomRotationRange = FVector3f::ZeroVector;
		int32		MeshOrientationVariableOffset = INDEX_NONE;
		int32		PreviousMeshOrientationVariableOffset = INDEX_NONE;
	};

public:
	using FParameters = NiagaraStateless::FInitialMeshOrientationModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta=(Units="deg"))
	FVector3f	Rotation = FVector3f::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta=(Units="deg"))
	FVector3f	RandomRotationRange = FVector3f(360.0f, 360.0f, 360.0f);

	virtual void BuildEmitterData(const FNiagaraStatelessEmitterDataBuildContext& BuildContext) const override
	{
		FModuleBuiltData* BuiltData = BuildContext.AllocateBuiltData<FModuleBuiltData>();

		const FNiagaraStatelessGlobals& StatelessGlobals	= FNiagaraStatelessGlobals::Get();
		BuiltData->MeshOrientationVariableOffset			= BuildContext.FindParticleVariableIndex(StatelessGlobals.MeshOrientationVariable);
		BuiltData->PreviousMeshOrientationVariableOffset	= BuildContext.FindParticleVariableIndex(StatelessGlobals.PreviousMeshOrientationVariable);

		if (IsModuleEnabled())
		{
			BuiltData->Rotation				= Rotation / 360.0f;
			BuiltData->RandomRotationRange	= RandomRotationRange / 360.0f;
		}

		if (BuiltData->MeshOrientationVariableOffset != INDEX_NONE || BuiltData->PreviousMeshOrientationVariableOffset != INDEX_NONE)
		{
			BuildContext.AddParticleSimulationExecSimulate(&UNiagaraStatelessModule_InitialMeshOrientation::ParticleSimulate);
		}
	}

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		FParameters* Parameters = SetShaderParameterContext.GetParameterNestedStruct<FParameters>();

		const FModuleBuiltData* ModuleBuiltData = SetShaderParameterContext.ReadBuiltData<FModuleBuiltData>();
		Parameters->InitialMeshOrientation_Rotation			= ModuleBuiltData->Rotation;
		Parameters->InitialMeshOrientation_RandomRangeScale	= ModuleBuiltData->RandomRotationRange;
	}

	static void ParticleSimulate(const NiagaraStateless::FParticleSimulationContext& ParticleSimulationContext)
	{
		using namespace NiagaraStateless;

		const FModuleBuiltData* ModuleBuiltData = ParticleSimulationContext.ReadBuiltData<FModuleBuiltData>();
		for (uint32 i = 0; i < ParticleSimulationContext.GetNumInstances(); ++i)
		{
			const FVector3f	Rotation = ModuleBuiltData->Rotation + (ParticleSimulationContext.RandomFloat3(i, 0) * ModuleBuiltData->RandomRotationRange);
			const FQuat4f Quat = ParticleSimulationContext.RotatorToQuat(Rotation);

			ParticleSimulationContext.WriteParticleVariable(ModuleBuiltData->MeshOrientationVariableOffset, i, Quat);
			ParticleSimulationContext.WriteParticleVariable(ModuleBuiltData->PreviousMeshOrientationVariableOffset, i, Quat);
		}
	}

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }
#endif
#if WITH_EDITORONLY_DATA
	virtual void GetOutputVariables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		OutVariables.AddUnique(StatelessGlobals.MeshOrientationVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousMeshOrientationVariable);
	}
#endif
};
