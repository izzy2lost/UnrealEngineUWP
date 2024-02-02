// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"
#include "Stateless/Modules/NiagaraStatelessModuleCommon.h"

#include "NiagaraParameterBinding.h"
#include "NiagaraParameterStore.h"

#include "NiagaraStatelessModule_InitializeParticle.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Initialize Particle"))
class UNiagaraStatelessModule_InitializeParticle : public UNiagaraStatelessModule
{
	GENERATED_BODY()

	static const uint32 EInitializeParticleModuleFlag_UniformSpriteSize	= 1 << 0;
	static const uint32 EInitializeParticleModuleFlag_UniformMeshScale	= 1 << 1;

	struct FModuleBuiltData
	{
		uint32							ModuleFlags = 0;
		int32							PositionParameterBinding = INDEX_NONE;
		FNiagaraStatelessRangeFloat		LifetimeRange;
		FNiagaraStatelessRangeColor		ColorRange;
		FNiagaraStatelessRangeFloat		MassRange;
		FNiagaraStatelessRangeVector2	SpriteSizeRange;
		FNiagaraStatelessRangeFloat		SpriteRotationRange;
		FNiagaraStatelessRangeVector3	MeshScaleRange;
		FNiagaraStatelessRangeFloat		RibbonWidthRange;
	};

public:
	using FParameters = NiagaraStateless::FInitializeParticleModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Lifetime"))
	FNiagaraDistributionRangeFloat LifetimeDistribution = FNiagaraDistributionRangeFloat(FNiagaraStatelessGlobals::GetDefaultLifetimeValue());

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Color", DisableCurveDistribution))
	FNiagaraDistributionColor ColorDistribution = FNiagaraDistributionColor(FNiagaraStatelessGlobals::GetDefaultColorValue());

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Mass"))
	FNiagaraDistributionRangeFloat MassDistribution = FNiagaraDistributionRangeFloat(FNiagaraStatelessGlobals::GetDefaultMassValue());

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Sprite Size"))
	FNiagaraDistributionRangeVector2 SpriteSizeDistribution = FNiagaraDistributionRangeVector2(FNiagaraStatelessGlobals::GetDefaultSpriteSizeValue());

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Sprite Rotation"))
	FNiagaraDistributionRangeFloat SpriteRotationDistribution = FNiagaraDistributionRangeFloat(FNiagaraStatelessGlobals::GetDefaultSpriteRotationValue());

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Mesh Scale"))
	FNiagaraDistributionRangeVector3 MeshScaleDistribution = FNiagaraDistributionRangeVector3(FNiagaraStatelessGlobals::GetDefaultScaleValue());

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (InlineEditConditionToggle))
	bool bWriteRibbonWidth = false;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Ribbon Width", EditCondition="bWriteRibbonWidth"))
	FNiagaraDistributionRangeFloat RibbonWidthDistribution = FNiagaraDistributionRangeFloat(FNiagaraStatelessGlobals::GetDefaultRibbonWidthValue());

	UPROPERTY(EditAnywhere, Category = "Parameters")
	FNiagaraParameterBindingWithValue	InitialPositionBinding;

	//~Begin: UObject Interface
#if WITH_EDITORONLY_DATA
	virtual void PostInitProperties()
	{
		Super::PostInitProperties();
		if (HasAnyFlags(RF_ClassDefaultObject) == false)
		{
			InitialPositionBinding.SetUsage(ENiagaraParameterBindingUsage::NotParticle);
			InitialPositionBinding.SetAllowedTypeDefinitions({ FNiagaraTypeDefinition::GetVec3Def() });
			InitialPositionBinding.SetDefaultParameter(FNiagaraTypeDefinition::GetVec3Def(), FVector3f::ZeroVector);
		}
	}
#endif
	//~End: UObject Interface

	virtual void BuildEmitterData(FNiagaraStatelessEmitterDataBuildContext& BuildContext) const override
	{
		FModuleBuiltData* BuiltData = BuildContext.AllocateBuiltData<FModuleBuiltData>();
		BuiltData->ModuleFlags				 = SpriteSizeDistribution.IsUniform() ? EInitializeParticleModuleFlag_UniformSpriteSize : 0;
		BuiltData->ModuleFlags				|= MeshScaleDistribution.IsUniform() ? EInitializeParticleModuleFlag_UniformMeshScale : 0;

		BuiltData->PositionParameterBinding	= BuildContext.AddRendererBinding(InitialPositionBinding);
		BuiltData->LifetimeRange			= LifetimeDistribution.CalculateRange(FNiagaraStatelessGlobals::GetDefaultLifetimeValue());
		BuiltData->ColorRange				= ColorDistribution.CalculateRange(FNiagaraStatelessGlobals::GetDefaultColorValue());
		BuiltData->MassRange				= MassDistribution.CalculateRange(FNiagaraStatelessGlobals::GetDefaultMassValue());
		BuiltData->SpriteSizeRange			= SpriteSizeDistribution.CalculateRange(FNiagaraStatelessGlobals::GetDefaultSpriteSizeValue());
		BuiltData->SpriteRotationRange		= SpriteRotationDistribution.CalculateRange(FNiagaraStatelessGlobals::GetDefaultSpriteRotationValue());
		BuiltData->MeshScaleRange			= MeshScaleDistribution.CalculateRange(FNiagaraStatelessGlobals::GetDefaultScaleValue());
		BuiltData->RibbonWidthRange			= RibbonWidthDistribution.CalculateRange(FNiagaraStatelessGlobals::GetDefaultRibbonWidthValue());

		NiagaraStateless::FPhysicsBuildData& PhysicsBuildData = BuildContext.GetTransientBuildData<NiagaraStateless::FPhysicsBuildData>();
		PhysicsBuildData.MassRange			= MassDistribution.CalculateRange(FNiagaraStatelessGlobals::GetDefaultMassValue());
	}

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		FParameters* Parameters = SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
		const FModuleBuiltData* ModuleBuiltData = SetShaderParameterContext.ReadBuiltData<FModuleBuiltData>();

		SetShaderParameterContext.GetRendererParameterValue(Parameters->InitializeParticle_Position, ModuleBuiltData->PositionParameterBinding, InitialPositionBinding.GetDefaultValue<FVector3f>());
		Parameters->InitializeParticle_ModuleFlags			= ModuleBuiltData->ModuleFlags;
		Parameters->InitializeParticle_ColorScale			= ModuleBuiltData->ColorRange.GetScale();
		Parameters->InitializeParticle_ColorBias			= ModuleBuiltData->ColorRange.Min;
		Parameters->InitializeParticle_SpriteSizeScale		= ModuleBuiltData->SpriteSizeRange.GetScale();
		Parameters->InitializeParticle_SpriteSizeBias		= ModuleBuiltData->SpriteSizeRange.Min;
		Parameters->InitializeParticle_SpriteRotationScale	= ModuleBuiltData->SpriteRotationRange.GetScale();
		Parameters->InitializeParticle_SpriteRotationBias	= ModuleBuiltData->SpriteRotationRange.Min;
		Parameters->InitializeParticle_MeshScaleScale		= ModuleBuiltData->MeshScaleRange.GetScale();
		Parameters->InitializeParticle_MeshScaleBias		= ModuleBuiltData->MeshScaleRange.Min;
		Parameters->InitializeParticle_RibbonWidthScale		= ModuleBuiltData->RibbonWidthRange.GetScale();
		Parameters->InitializeParticle_RibbonWidthBias		= ModuleBuiltData->RibbonWidthRange.Min;
	}

#if WITH_EDITORONLY_DATA
	virtual void GetOutputVaruables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		OutVariables.AddUnique(StatelessGlobals.UniqueIDVariable);
		OutVariables.AddUnique(StatelessGlobals.PositionVariable);
		OutVariables.AddUnique(StatelessGlobals.ColorVariable);
		OutVariables.AddUnique(StatelessGlobals.SpriteSizeVariable);
		OutVariables.AddUnique(StatelessGlobals.SpriteRotationVariable);
		OutVariables.AddUnique(StatelessGlobals.ScaleVariable);

		OutVariables.AddUnique(StatelessGlobals.PreviousPositionVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousSpriteSizeVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousSpriteRotationVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousScaleVariable);

		if (bWriteRibbonWidth)
		{
			OutVariables.AddUnique(StatelessGlobals.RibbonWidthVariable);
			OutVariables.AddUnique(StatelessGlobals.PreviousRibbonWidthVariable);
		}
	}
#endif
};
