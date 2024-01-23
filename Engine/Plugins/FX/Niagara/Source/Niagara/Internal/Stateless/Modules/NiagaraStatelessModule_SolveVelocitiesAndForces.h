// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"
#include "Stateless/Modules/NiagaraStatelessModuleCommon.h"

#include "VectorField/VectorField.h"
#include "VectorField/VectorFieldStatic.h"
#include "RHIStaticStates.h"

#include "NiagaraStatelessModule_SolveVelocitiesAndForces.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Solve Forces And Velocity"))
class UNiagaraStatelessModule_SolveVelocitiesAndForces : public UNiagaraStatelessModule
{
	GENERATED_BODY()

public:
	using FModuleBuiltData = NiagaraStateless::FPhysicsBuildData;
	using FParameters = NiagaraStateless::FSolveVelocitiesAndForcesModule_ShaderParameters;

	virtual void BuildEmitterData(FNiagaraStatelessEmitterDataBuildContext& BuildContext) const override
	{
		NiagaraStateless::FPhysicsBuildData& PhysicsBuildData = BuildContext.GetTransientBuildData<NiagaraStateless::FPhysicsBuildData>();

		FModuleBuiltData* BuiltData		= BuildContext.AllocateBuiltData<FModuleBuiltData>();
		BuiltData->MassMin				= PhysicsBuildData.MassMin;
		BuiltData->MassMax				= PhysicsBuildData.MassMax;
		BuiltData->DragMin				= FMath::Clamp(PhysicsBuildData.DragMin, 0.01f, 1.0f);
		BuiltData->DragMax				= FMath::Clamp(PhysicsBuildData.DragMax, 0.01f, 1.0f);
		BuiltData->VelocityMin			= PhysicsBuildData.VelocityMin;
		BuiltData->VelocityMax			= PhysicsBuildData.VelocityMax;
		BuiltData->WindMin				= PhysicsBuildData.WindMin;
		BuiltData->WindMax				= PhysicsBuildData.WindMax;
		BuiltData->AccelerationMin		= PhysicsBuildData.AccelerationMin;
		BuiltData->AccelerationMax		= PhysicsBuildData.AccelerationMax;
		BuiltData->bConeVelocity		= PhysicsBuildData.bConeVelocity;
		BuiltData->ConeQuat				= PhysicsBuildData.ConeQuat;
		BuiltData->ConeVelocityMin		= PhysicsBuildData.ConeVelocityMin;
		BuiltData->ConeVelocityMax		= PhysicsBuildData.ConeVelocityMax;
		BuiltData->ConeOuterAngle		= PhysicsBuildData.ConeOuterAngle;
		BuiltData->ConeInnerAngle		= PhysicsBuildData.ConeInnerAngle;
		BuiltData->ConeVelocityFalloff	= PhysicsBuildData.ConeVelocityFalloff;
		BuiltData->bPointVelocity		= PhysicsBuildData.bPointVelocity;
		BuiltData->PointVelocityMin		= PhysicsBuildData.PointVelocityMin;
		BuiltData->PointVelocityMax		= PhysicsBuildData.PointVelocityMax;
		BuiltData->PointOrigin			= PhysicsBuildData.PointOrigin;
		BuiltData->bNoiseEnabled		= PhysicsBuildData.bNoiseEnabled;
		BuiltData->NoiseAmplitude		= PhysicsBuildData.NoiseAmplitude;
		BuiltData->NoiseFrequency		= PhysicsBuildData.NoiseFrequency;
		BuiltData->NoiseTexture			= PhysicsBuildData.NoiseTexture;
		BuiltData->NoiseMode			= PhysicsBuildData.NoiseMode;
		BuiltData->NoiseLUTOffset		= PhysicsBuildData.NoiseLUTOffset;
		BuiltData->NoiseLUTNumChannel	= PhysicsBuildData.NoiseLUTNumChannel;
		BuiltData->NoiseLUTChannelWidth	= PhysicsBuildData.NoiseLUTChannelWidth;
	}

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		const FModuleBuiltData* ModuleBuiltData = SetShaderParameterContext.ReadBuiltData<FModuleBuiltData>();

		FParameters* Parameters = SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
		Parameters->SolveVelocitiesAndForces_MassScale				= ModuleBuiltData->MassMax - ModuleBuiltData->MassMin;
		Parameters->SolveVelocitiesAndForces_MassBias				= ModuleBuiltData->MassMin;
		Parameters->SolveVelocitiesAndForces_DragScale				= ModuleBuiltData->DragMax - ModuleBuiltData->DragMin;
		Parameters->SolveVelocitiesAndForces_DragBias				= ModuleBuiltData->DragMin;
		Parameters->SolveVelocitiesAndForces_VelocityScale			= ModuleBuiltData->VelocityMax - ModuleBuiltData->VelocityMin;
		Parameters->SolveVelocitiesAndForces_VelocityBias			= ModuleBuiltData->VelocityMin;
		Parameters->SolveVelocitiesAndForces_WindScale				= ModuleBuiltData->WindMax - ModuleBuiltData->WindMin;
		Parameters->SolveVelocitiesAndForces_WindBias				= ModuleBuiltData->WindMin;
		Parameters->SolveVelocitiesAndForces_AccelerationScale		= ModuleBuiltData->AccelerationMax - ModuleBuiltData->AccelerationMin;
		Parameters->SolveVelocitiesAndForces_AccelerationBias		= ModuleBuiltData->AccelerationMin;

		Parameters->SolveVelocitiesAndForces_ConeVelocityEnabled	= ModuleBuiltData->bConeVelocity ? 1 : 0;
		Parameters->SolveVelocitiesAndForces_ConeQuat				= ModuleBuiltData->ConeQuat;
		Parameters->SolveVelocitiesAndForces_ConeVelocityScale		= ModuleBuiltData->ConeVelocityMax - ModuleBuiltData->ConeVelocityMin;
		Parameters->SolveVelocitiesAndForces_ConeVelocityBias		= ModuleBuiltData->ConeVelocityMin;
		Parameters->SolveVelocitiesAndForces_ConeAngleScale			= (ModuleBuiltData->ConeOuterAngle - ModuleBuiltData->ConeInnerAngle) * (UE_PI / 360.0f);
		Parameters->SolveVelocitiesAndForces_ConeAngleBias			= ModuleBuiltData->ConeInnerAngle * (UE_PI / 360.0f);
		Parameters->SolveVelocitiesAndForces_ConeVelocityFalloff	= ModuleBuiltData->ConeVelocityFalloff;

		Parameters->SolveVelocitiesAndForces_PontVelocityEnabled	= ModuleBuiltData->bPointVelocity ? 1 : 0;
		Parameters->SolveVelocitiesAndForces_PointVelocityScale		= ModuleBuiltData->PointVelocityMax - ModuleBuiltData->PointVelocityMin;
		Parameters->SolveVelocitiesAndForces_PointVelocityBias		= ModuleBuiltData->PointVelocityMax;
		Parameters->SolveVelocitiesAndForces_PointOrigin			= ModuleBuiltData->PointOrigin;

		Parameters->SolveVelocitiesAndForces_NoiseEnabled			= ModuleBuiltData->bNoiseEnabled ? 1 : 0;
		Parameters->SolveVelocitiesAndForces_NoiseAmplitude			= ModuleBuiltData->NoiseAmplitude;
		Parameters->SolveVelocitiesAndForces_NoiseFrequency			= FVector3f(ModuleBuiltData->NoiseFrequency, ModuleBuiltData->NoiseFrequency, ModuleBuiltData->NoiseFrequency);
		//SetShaderParameterContext.SetTextureResource(&Parameters->SolveVelocitiesAndForces_NoiseTexture, ModuleBuildData->NoiseTexture);
		Parameters->SolveVelocitiesAndForces_NoiseMode				= ModuleBuiltData->NoiseMode;
		Parameters->SolveVelocitiesAndForces_NoiseLUTOffset			= ModuleBuiltData->NoiseLUTOffset;
		Parameters->SolveVelocitiesAndForces_NoiseLUTNumChannel		= ModuleBuiltData->NoiseLUTNumChannel;
		Parameters->SolveVelocitiesAndForces_NoiseLUTChannelWidth	= ModuleBuiltData->NoiseLUTChannelWidth;

		FVectorFieldTextureAccessor TextureAccessor(Cast<UVectorField>(ModuleBuiltData->NoiseTexture));

		ENQUEUE_RENDER_COMMAND(FNaughtyTest)(
			[Parameters, TextureAccessor](FRHICommandListImmediate& RHICmdList)
			{
				FRHITexture* NoiseTextureRHI = TextureAccessor.GetTexture();
				Parameters->SolveVelocitiesAndForces_NoiseTexture = NoiseTextureRHI;
				Parameters->SolveVelocitiesAndForces_NoiseSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();

				if (Parameters->SolveVelocitiesAndForces_NoiseMode < 2)
				{
					const FIntVector TextureSize = NoiseTextureRHI ? NoiseTextureRHI->GetSizeXYZ() : FIntVector(1, 1, 1);
					Parameters->SolveVelocitiesAndForces_NoiseFrequency.X *= 1.0f / float(TextureSize.X);
					Parameters->SolveVelocitiesAndForces_NoiseFrequency.Y *= 1.0f / float(TextureSize.Y);
					Parameters->SolveVelocitiesAndForces_NoiseFrequency.Z *= 1.0f / float(TextureSize.Z);
				}
			}
		);

		//ModuleBuildData->NoiseTexture
		//SetShaderParameterContext.AddSetResource(
		//	[]()
		//	{
		//	}
		//);

		//FRHITexture**		TextureRef = &Parameters->SolveVelocitiesAndForces_NoiseTexture;
		//FRHISamplerState**	SamplerRef = &Parameters->SolveVelocitiesAndForces_NoiseSampler;
		//FTextureRHIRef
		//if (ModuleBuiltData->NoiseTexture)
		//{
		//}
		//SetShaderParameterContext.AddRenderThreadSet(
		//	[]()
		//	{
		//	}
		//);
	}

#if WITH_EDITORONLY_DATA
	virtual void GetOutputVaruables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		OutVariables.AddUnique(StatelessGlobals.PositionVariable);
		OutVariables.AddUnique(StatelessGlobals.VelocityVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousPositionVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousVelocityVariable);
	}
#endif
};
