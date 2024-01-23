// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"
#include "Stateless/Modules/NiagaraStatelessModuleCommon.h"

#include "NiagaraParameterBinding.h"
#include "NiagaraParameterStore.h"

#include "NiagaraStatelessModule_InitializeParticle.generated.h"

UENUM()
enum class ENSPositionDistributionType
{
	UniqueIndexClamp,
	UniqueIndexWrap,
	Random,
	RandomInterpolateToNext,
	RandomInterpolateToRandom,
};

USTRUCT()
struct FPositionDistributionTest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameters")
	ENSPositionDistributionType Type = ENSPositionDistributionType::UniqueIndexClamp;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	TArray<FVector3f> Values;
};

//UENUM()
//enum class NiagaraStatelessModuleInitializeParticle_ColorMode
//{
//	Unset,
//	DirectSet,
//	RandomRange,
//	//RandomHSV UMETA(DisplayName="Random Hue/Saturation/Value"),
//};

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Initialize Particle"))
class UNiagaraStatelessModule_InitializeParticle : public UNiagaraStatelessModule
{
	GENERATED_BODY()

	struct FModuleBuiltData
	{
		int32	PositionParameterBinding = INDEX_NONE;
		int32	PositionTableOffset = 0;
		int32	PositionTableLength = 0;
	};

public:
	using FParameters = NiagaraStateless::FInitializeParticleModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	float LifetimeMin = 1.0f;
	UPROPERTY(EditAnywhere, Category = "Parameters")
	float LifetimeMax = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	FVector3f	InitialPosition = FVector3f(0.0f);

	UPROPERTY(EditAnywhere, Category = "Parameters")
	FLinearColor	ColorMin = FLinearColor::White;
	UPROPERTY(EditAnywhere, Category = "Parameters")
	FLinearColor	ColorMax = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	float MassMin = 1.0f;
	UPROPERTY(EditAnywhere, Category = "Parameters")
	float MassMax = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	FVector2f	SpriteSizeMin = FVector2f(10.0f);
	UPROPERTY(EditAnywhere, Category = "Parameters")
	FVector2f	SpriteSizeMax = FVector2f(10.0f);

	UPROPERTY(EditAnywhere, Category = "Parameters")
	float	SpriteRotationMin = 0.0f;
	UPROPERTY(EditAnywhere, Category = "Parameters")
	float	SpriteRotationMax = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	FVector3f	MeshScaleMin = FVector3f(1.0f);
	UPROPERTY(EditAnywhere, Category = "Parameters")
	FVector3f	MeshScaleMax = FVector3f(1.0f);

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (InlineEditConditionToggle))
	bool bWriteRibbonWidth = false;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditCondition="bWriteRibbonWidth"))
	FVector2f	RibbonWidthMinMax = FVector2f(10.0f, 10.0f);

	UPROPERTY(EditAnywhere, Category = "Parameters")
	FNiagaraParameterBindingWithValue	InitialPositionBinding;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	FPositionDistributionTest PositionDistribution;

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
		const FVector3f DefaultPosition(0.0f);
		TConstArrayView<FVector3f> PositionValues = PositionDistribution.Values.Num() > 0 ? MakeArrayView(PositionDistribution.Values) : MakeArrayView(&DefaultPosition, 1);

		FModuleBuiltData* BuiltData = BuildContext.AllocateBuiltData<FModuleBuiltData>();
		BuiltData->PositionTableOffset = BuildContext.AddStaticData(PositionValues);
		BuiltData->PositionTableLength = PositionValues.Num();

		BuiltData->PositionParameterBinding = BuildContext.AddRendererBinding(InitialPositionBinding);
	
		NiagaraStateless::FPhysicsBuildData& PhysicsBuildData = BuildContext.GetTransientBuildData<NiagaraStateless::FPhysicsBuildData>();
		PhysicsBuildData.MassMin = MassMin;
		PhysicsBuildData.MassMax = MassMax;
	}

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		FParameters* Parameters = SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
		const FModuleBuiltData* ModuleBuiltData = SetShaderParameterContext.ReadBuiltData<FModuleBuiltData>();

		//Parameters->InitializeParticle_Position			= InitialPosition;
		SetShaderParameterContext.GetRendererParameterValue(Parameters->InitializeParticle_Position, ModuleBuiltData->PositionParameterBinding, InitialPositionBinding.GetDefaultValue<FVector3f>());
		Parameters->InitializeParticle_ColorScale			= ColorMax - ColorMin;
		Parameters->InitializeParticle_ColorBias			= ColorMin;
		Parameters->InitializeParticle_SpriteSizeScale		= SpriteSizeMax - SpriteSizeMin;
		Parameters->InitializeParticle_SpriteSizeBias		= SpriteSizeMin;
		Parameters->InitializeParticle_SpriteRotationScale	= SpriteRotationMax - SpriteRotationMin;
		Parameters->InitializeParticle_SpriteRotationBias	= SpriteRotationMin;
		Parameters->InitializeParticle_MeshScaleScale		= MeshScaleMax - MeshScaleMin;
		Parameters->InitializeParticle_MeshScaleBias		= MeshScaleMin;
		Parameters->InitializeParticle_RibbonWidthScale		= RibbonWidthMinMax.Y - RibbonWidthMinMax.X;
		Parameters->InitializeParticle_RibbonWidthBias		= RibbonWidthMinMax.X;

		Parameters->InitializeParticle_PositionMode			= 0;
		Parameters->InitializeParticle_PositionTableMod		= ModuleBuiltData->PositionTableLength;
		Parameters->InitializeParticle_PositionTableOffset	= ModuleBuiltData->PositionTableOffset;
		Parameters->InitializeParticle_PositionTableLength	= ModuleBuiltData->PositionTableLength;
		switch (PositionDistribution.Type)
		{
			case ENSPositionDistributionType::UniqueIndexClamp:
				Parameters->InitializeParticle_PositionMode			= 0;
				Parameters->InitializeParticle_PositionTableMod		= 0xffffffff;
				break;
			case ENSPositionDistributionType::UniqueIndexWrap:
				Parameters->InitializeParticle_PositionMode			= 0;
				break;
			case ENSPositionDistributionType::Random:
				Parameters->InitializeParticle_PositionMode			= 2;
				break;
			case ENSPositionDistributionType::RandomInterpolateToNext:
				Parameters->InitializeParticle_PositionMode			= 1;
				break;
			case ENSPositionDistributionType::RandomInterpolateToRandom:
				Parameters->InitializeParticle_PositionMode			= 3;
				break;
		}
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
