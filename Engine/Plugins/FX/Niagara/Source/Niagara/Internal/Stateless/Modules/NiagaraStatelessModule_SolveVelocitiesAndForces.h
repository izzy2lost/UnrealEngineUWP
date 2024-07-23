// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"
#include "Stateless/NiagaraStatelessParticleSimContext.h"
#include "Stateless/Modules/NiagaraStatelessModuleCommon.h"

#include "VectorField/VectorField.h"
#include "VectorField/VectorFieldStatic.h"
#include "RHIStaticStates.h"

#include "NiagaraStatelessModule_SolveVelocitiesAndForces.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Solve Forces And Velocity"))
class UNiagaraStatelessModule_SolveVelocitiesAndForces : public UNiagaraStatelessModule
{
	GENERATED_BODY()

	struct FModuleBuiltData
	{
		NiagaraStateless::FPhysicsBuildData	PhysicsData;
		int32								PositionVariableOffset = INDEX_NONE;
		int32								VelocityVariableOffset = INDEX_NONE;
		int32								PreviousPositionVariableOffset = INDEX_NONE;
		int32								PreviousVelocityVariableOffset = INDEX_NONE;
	};

public:
	using FParameters = NiagaraStateless::FSolveVelocitiesAndForcesModule_ShaderParameters;

	virtual void BuildEmitterData(const FNiagaraStatelessEmitterDataBuildContext& BuildContext) const override
	{
		NiagaraStateless::FPhysicsBuildData& PhysicsBuildData = BuildContext.GetTransientBuildData<NiagaraStateless::FPhysicsBuildData>();

		FModuleBuiltData* BuiltData				= BuildContext.AllocateBuiltData<FModuleBuiltData>();
		BuiltData->PhysicsData					= PhysicsBuildData;
		BuiltData->PhysicsData.DragRange.Min	= FMath::Max(PhysicsBuildData.DragRange.Min, 0.01f);
		BuiltData->PhysicsData.DragRange.Max	= FMath::Max(PhysicsBuildData.DragRange.Max, 0.01f);

		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		BuiltData->PositionVariableOffset				= BuildContext.FindParticleVariableIndex(StatelessGlobals.PositionVariable);
		BuiltData->VelocityVariableOffset				= BuildContext.FindParticleVariableIndex(StatelessGlobals.VelocityVariable);
		BuiltData->PreviousPositionVariableOffset		= BuildContext.FindParticleVariableIndex(StatelessGlobals.PreviousPositionVariable);
		BuiltData->PreviousVelocityVariableOffset		= BuildContext.FindParticleVariableIndex(StatelessGlobals.PreviousVelocityVariable);
		
		const bool bAttributesUsed =
			BuiltData->PositionVariableOffset != INDEX_NONE ||
			BuiltData->VelocityVariableOffset != INDEX_NONE ||
			BuiltData->PreviousPositionVariableOffset != INDEX_NONE ||
			BuiltData->PreviousVelocityVariableOffset != INDEX_NONE;

		if (bAttributesUsed)
		{
			BuildContext.AddParticleSimulationExecSimulate(&UNiagaraStatelessModule_SolveVelocitiesAndForces::ParticleSimulate);
		}
	}

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		const FModuleBuiltData* ModuleBuiltData = SetShaderParameterContext.ReadBuiltData<FModuleBuiltData>();

		FParameters* Parameters = SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
		Parameters->SolveVelocitiesAndForces_MassScale				= ModuleBuiltData->PhysicsData.MassRange.GetScale();
		Parameters->SolveVelocitiesAndForces_MassBias				= ModuleBuiltData->PhysicsData.MassRange.Min;
		Parameters->SolveVelocitiesAndForces_DragScale				= ModuleBuiltData->PhysicsData.DragRange.GetScale();
		Parameters->SolveVelocitiesAndForces_DragBias				= ModuleBuiltData->PhysicsData.DragRange.Min;
		Parameters->SolveVelocitiesAndForces_VelocityScale			= ModuleBuiltData->PhysicsData.VelocityRange.GetScale();
		Parameters->SolveVelocitiesAndForces_VelocityBias			= ModuleBuiltData->PhysicsData.VelocityRange.Min;
		Parameters->SolveVelocitiesAndForces_WindScale				= ModuleBuiltData->PhysicsData.WindRange.GetScale();
		Parameters->SolveVelocitiesAndForces_WindBias				= ModuleBuiltData->PhysicsData.WindRange.Min;
		Parameters->SolveVelocitiesAndForces_AccelerationScale		= ModuleBuiltData->PhysicsData.AccelerationRange.GetScale();
		Parameters->SolveVelocitiesAndForces_AccelerationBias		= ModuleBuiltData->PhysicsData.AccelerationRange.Min;

		Parameters->SolveVelocitiesAndForces_ConeVelocityEnabled	= ModuleBuiltData->PhysicsData.bConeVelocity ? 1 : 0;
		Parameters->SolveVelocitiesAndForces_ConeQuat				= ModuleBuiltData->PhysicsData.ConeQuat;
		Parameters->SolveVelocitiesAndForces_ConeVelocityScale		= ModuleBuiltData->PhysicsData.ConeVelocityRange.GetScale();
		Parameters->SolveVelocitiesAndForces_ConeVelocityBias		= ModuleBuiltData->PhysicsData.ConeVelocityRange.Min;
		Parameters->SolveVelocitiesAndForces_ConeAngleScale			= (ModuleBuiltData->PhysicsData.ConeOuterAngle - ModuleBuiltData->PhysicsData.ConeInnerAngle) * (UE_PI / 360.0f);
		Parameters->SolveVelocitiesAndForces_ConeAngleBias			= ModuleBuiltData->PhysicsData.ConeInnerAngle * (UE_PI / 360.0f);
		Parameters->SolveVelocitiesAndForces_ConeVelocityFalloff	= ModuleBuiltData->PhysicsData.ConeVelocityFalloff;

		Parameters->SolveVelocitiesAndForces_PontVelocityEnabled	= ModuleBuiltData->PhysicsData.bPointVelocity ? 1 : 0;
		Parameters->SolveVelocitiesAndForces_PointVelocityScale		= ModuleBuiltData->PhysicsData.PointVelocityRange.GetScale();
		Parameters->SolveVelocitiesAndForces_PointVelocityBias		= ModuleBuiltData->PhysicsData.PointVelocityRange.Min;
		Parameters->SolveVelocitiesAndForces_PointOrigin			= ModuleBuiltData->PhysicsData.PointOrigin;

		Parameters->SolveVelocitiesAndForces_NoiseEnabled			= ModuleBuiltData->PhysicsData.bNoiseEnabled ? 1 : 0;
		Parameters->SolveVelocitiesAndForces_NoiseAmplitude			= ModuleBuiltData->PhysicsData.NoiseAmplitude;
		Parameters->SolveVelocitiesAndForces_NoiseFrequency			= FVector3f(ModuleBuiltData->PhysicsData.NoiseFrequency, ModuleBuiltData->PhysicsData.NoiseFrequency, ModuleBuiltData->PhysicsData.NoiseFrequency);
		//SetShaderParameterContext.SetTextureResource(&Parameters->SolveVelocitiesAndForces_NoiseTexture, ModuleBuildData->NoiseTexture);
		Parameters->SolveVelocitiesAndForces_NoiseMode				= ModuleBuiltData->PhysicsData.NoiseMode;
		Parameters->SolveVelocitiesAndForces_NoiseLUTOffset			= ModuleBuiltData->PhysicsData.NoiseLUTOffset;
		Parameters->SolveVelocitiesAndForces_NoiseLUTNumChannel		= ModuleBuiltData->PhysicsData.NoiseLUTNumChannel;
		Parameters->SolveVelocitiesAndForces_NoiseLUTChannelWidth	= ModuleBuiltData->PhysicsData.NoiseLUTChannelWidth;

		FVectorFieldTextureAccessor TextureAccessor(Cast<UVectorField>(ModuleBuiltData->PhysicsData.NoiseTexture));

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
	}

	static FVector3f IntegratePosition(float Age, float Mass, float Drag, const FVector3f& Velocity, const FVector3f& Wind, const FVector3f& Acceleration)
	{
		const FVector3f IntVelocity = (Velocity - Wind) + (Wind * Age * Age);
		const float LambdaDragMass = FMath::Max(Drag * (1.0f / Mass), 0.0001f);
		const float LambdaAge = (1.0f - FMath::Exp(-(LambdaDragMass * Age))) / LambdaDragMass;
		FVector3f Position = IntVelocity * LambdaAge;
		Position += (Age - LambdaAge) * (Acceleration / LambdaDragMass);
		return Position;
	}

	static void ParticleSimulate(const NiagaraStateless::FParticleSimulationContext& ParticleSimulationContext)
	{
		const FModuleBuiltData* ModuleBuiltData = ParticleSimulationContext.ReadBuiltData<FModuleBuiltData>();

		const float* AgeData			= ParticleSimulationContext.GetParticleAge();
		const float* PreviousAgeData	= ParticleSimulationContext.GetParticlePreviousAge();

		for (uint32 i = 0; i < ParticleSimulationContext.GetNumInstances(); ++i)
		{
			const float Mass				= ParticleSimulationContext.RandomScaleBiasFloat(i, 0, ModuleBuiltData->PhysicsData.MassRange);
			const float Drag				= ParticleSimulationContext.RandomScaleBiasFloat(i, 1, ModuleBuiltData->PhysicsData.DragRange);
			FVector3f InitialVelocity		= ParticleSimulationContext.RandomScaleBiasFloat(i, 2, ModuleBuiltData->PhysicsData.VelocityRange);
			const FVector3f Wind			= ParticleSimulationContext.RandomScaleBiasFloat(i, 3, ModuleBuiltData->PhysicsData.WindRange);
			const FVector3f Acceleration	= ParticleSimulationContext.RandomScaleBiasFloat(i, 4, ModuleBuiltData->PhysicsData.AccelerationRange);

			FVector3f Position			= ParticleSimulationContext.ReadParticleVariable(ModuleBuiltData->PositionVariableOffset, i, FVector3f::ZeroVector);
			FVector3f PreviousPosition	= ParticleSimulationContext.ReadParticleVariable(ModuleBuiltData->PreviousPositionVariableOffset, i, FVector3f::ZeroVector);

			if (ModuleBuiltData->PhysicsData.bConeVelocity)
			{
				const float ConeAngle = ParticleSimulationContext.RandomScaleBiasFloat(i, 5, FNiagaraStatelessRangeFloat(ModuleBuiltData->PhysicsData.ConeInnerAngle, ModuleBuiltData->PhysicsData.ConeOuterAngle)) * (UE_PI / 360.0f);
				const float ConeRotation = ParticleSimulationContext.RandomFloat(i, 6) * UE_TWO_PI;
				const FVector2f scAng = FVector2f(FMath::Sin(ConeAngle), FMath::Cos(ConeAngle));
				const FVector2f scRot = FVector2f(FMath::Sin(ConeRotation), FMath::Cos(ConeRotation));
				const FVector3f Direction = FVector3f(scRot.X * scAng.X, scRot.Y * scAng.X, scAng.Y);

				float VelocityScale = ParticleSimulationContext.RandomScaleBiasFloat(i, 7, ModuleBuiltData->PhysicsData.ConeVelocityRange);
				if (ModuleBuiltData->PhysicsData.bConeVelocity)
				{
					const float pf = FMath::Pow(FMath::Clamp(scAng.Y, 0.0f, 1.0f), ModuleBuiltData->PhysicsData.ConeVelocityFalloff * 10.0f);
					VelocityScale *= FMath::Lerp(1.0f, pf, ModuleBuiltData->PhysicsData.ConeVelocityFalloff);
				}

				InitialVelocity += ModuleBuiltData->PhysicsData.ConeQuat.RotateVector(Direction) * VelocityScale;
			}

			if (ModuleBuiltData->PhysicsData.bPointVelocity)
			{
				const FVector3f FallbackDir	= ParticleSimulationContext.RandomUnitFloat3(i, 8);
				const FVector3f Delta		= Position - ModuleBuiltData->PhysicsData.PointOrigin;
				const FVector3f Direction	= ParticleSimulationContext.SafeNormalize(Delta, FallbackDir);
				const float		VelocityScale = ParticleSimulationContext.RandomScaleBiasFloat(i, 9, ModuleBuiltData->PhysicsData.PointVelocityRange);

				InitialVelocity += Direction * VelocityScale;
			}

			if (ModuleBuiltData->PhysicsData.bNoiseEnabled)
			{
				//-TODO:
			}

			Position += IntegratePosition(AgeData[i], Mass, Drag, InitialVelocity, Wind, Acceleration);
			PreviousPosition += IntegratePosition(PreviousAgeData[i], Mass, Drag, InitialVelocity, Wind, Acceleration);

			ParticleSimulationContext.WriteParticleVariable(ModuleBuiltData->PositionVariableOffset, i, Position);
			ParticleSimulationContext.WriteParticleVariable(ModuleBuiltData->PreviousPositionVariableOffset, i, PreviousPosition);

			const FVector3f Velocity = Position - PreviousPosition;
			ParticleSimulationContext.WriteParticleVariable(ModuleBuiltData->VelocityVariableOffset, i, Velocity);
			ParticleSimulationContext.WriteParticleVariable(ModuleBuiltData->PreviousVelocityVariableOffset, i, Velocity);
		}
	}

#if WITH_EDITORONLY_DATA
	virtual void GetOutputVariables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		OutVariables.AddUnique(StatelessGlobals.PositionVariable);
		OutVariables.AddUnique(StatelessGlobals.VelocityVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousPositionVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousVelocityVariable);
	}
#endif
};
