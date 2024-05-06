// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stateless/NiagaraStatelessCommon.h"

#include "Stateless/NiagaraStatelessEmitterTemplate.h"
#include "NiagaraSystem.h"

#include "UObject/UObjectIterator.h"

namespace NiagaraStatelessCommon
{
	static FNiagaraStatelessGlobals					GGlobals;
	static TOptional<ENiagaraStatelessFeatureMask>	GUpdatedFeatureMask;

	static void SetUpdateFeatureMask(ENiagaraStatelessFeatureMask Mask, bool bEnabled)
	{
		ENiagaraStatelessFeatureMask NewMask = GUpdatedFeatureMask.Get(GGlobals.FeatureMask);
		if (bEnabled)
		{
			EnumAddFlags(NewMask, Mask);
		}
		else
		{
			EnumRemoveFlags(NewMask, Mask);
		}
		GUpdatedFeatureMask = NewMask;
	}

	#define DEFINE_FEATURE_MASK(ENUM, DESC) \
		static bool bFeatureMask_##ENUM = true; \
		static FAutoConsoleVariableRef CVarFeatureMask_##ENUM( \
			TEXT("fx.NiagaraStateless.Feature."#ENUM), \
			bFeatureMask_##ENUM, \
			TEXT(DESC), \
			FConsoleVariableDelegate::CreateLambda([](IConsoleVariable*) {SetUpdateFeatureMask(ENiagaraStatelessFeatureMask::ENUM, bFeatureMask_##ENUM);}), \
			ECVF_Scalability | ECVF_Default \
		)

		DEFINE_FEATURE_MASK(ExecuteGPU, "When enabled simulations are allowed to execute on the GPU");
		DEFINE_FEATURE_MASK(ExecuteCPU, "When enabled simulations are allowed to execute on the CPU");

	#undef DEFINE_FEATURE_MASK

	void Initialize()
	{
		GGlobals.CameraOffsetVariable				= FNiagaraVariableBase(FNiagaraTypeDefinition::GetFloatDef(), TEXT("CameraOffset"));
		GGlobals.ColorVariable						= FNiagaraVariableBase(FNiagaraTypeDefinition::GetColorDef(), TEXT("Color"));
		GGlobals.DynamicMaterialParameters0Variable = FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec4Def(), TEXT("DynamicMaterialParameter"));
		GGlobals.DynamicMaterialParameters1Variable = FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec4Def(), TEXT("DynamicMaterialParameter1"));
		GGlobals.DynamicMaterialParameters2Variable = FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec4Def(), TEXT("DynamicMaterialParameter2"));
		GGlobals.DynamicMaterialParameters3Variable = FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec4Def(), TEXT("DynamicMaterialParameter3"));
		GGlobals.MeshIndexVariable					= FNiagaraVariableBase(FNiagaraTypeDefinition::GetIntDef(), TEXT("MeshIndex"));
		GGlobals.MeshOrientationVariable			= FNiagaraVariableBase(FNiagaraTypeDefinition::GetQuatDef(), TEXT("MeshOrientation"));
		GGlobals.PositionVariable					= FNiagaraVariableBase(FNiagaraTypeDefinition::GetPositionDef(), TEXT("Position"));
		GGlobals.RibbonWidthVariable				= FNiagaraVariableBase(FNiagaraTypeDefinition::GetFloatDef(), TEXT("RibbonWidth"));
		GGlobals.ScaleVariable						= FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Scale"));
		GGlobals.SpriteAlignmentVariable			= FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec3Def(), TEXT("SpriteAlignment"));
		GGlobals.SpriteFacingVariable				= FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec3Def(), TEXT("SpriteFacing"));
		GGlobals.SpriteSizeVariable					= FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec2Def(), TEXT("SpriteSize"));
		GGlobals.SpriteRotationVariable				= FNiagaraVariableBase(FNiagaraTypeDefinition::GetFloatDef(), TEXT("SpriteRotation"));
		GGlobals.SubImageIndexVariable				= FNiagaraVariableBase(FNiagaraTypeDefinition::GetFloatDef(), TEXT("SubImageIndex"));
		GGlobals.UniqueIDVariable					= FNiagaraVariableBase(FNiagaraTypeDefinition::GetIntDef(), TEXT("UniqueID"));
		GGlobals.VelocityVariable					= FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity"));

		GGlobals.PreviousCameraOffsetVariable		= FNiagaraVariableBase(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Previous.CameraOffset"));
		//GGlobals.PreviousColorVariable			= FNiagaraVariableBase(FNiagaraTypeDefinition::GetColorDef(), TEXT("Previous.Color"));
		//GGlobals.PreviousDynamicMaterialParameters0Variable		= FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec4Def(), TEXT("Previous.DynamicMaterialParameter"));
		GGlobals.PreviousMeshOrientationVariable	= FNiagaraVariableBase(FNiagaraTypeDefinition::GetQuatDef(), TEXT("Previous.MeshOrientation"));
		GGlobals.PreviousPositionVariable			= FNiagaraVariableBase(FNiagaraTypeDefinition::GetPositionDef(), TEXT("Previous.Position"));
		GGlobals.PreviousRibbonWidthVariable		= FNiagaraVariableBase(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Previous.RibbonWidth"));
		GGlobals.PreviousScaleVariable				= FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Previous.Scale"));
		GGlobals.PreviousSpriteAlignmentVariable	= FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Previous.SpriteAlignment"));
		GGlobals.PreviousSpriteFacingVariable		= FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Previous.SpriteFacing"));
		GGlobals.PreviousSpriteSizeVariable			= FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec2Def(), TEXT("Previous.SpriteSize"));
		GGlobals.PreviousSpriteRotationVariable		= FNiagaraVariableBase(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Previous.SpriteRotation"));
		GGlobals.PreviousVelocityVariable			= FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Previous.Velocity"));

		UNiagaraStatelessEmitterTemplate::InitCDOPropertiesAfterModuleStartup();
	}

	void UpdateSettings()
	{
		if (!GUpdatedFeatureMask.IsSet())
		{
			return;
		}

		if (GUpdatedFeatureMask.GetValue() != GGlobals.FeatureMask)
		{
			GGlobals.FeatureMask = GUpdatedFeatureMask.GetValue();

			for (TObjectIterator<UNiagaraSystem> It; It; ++It)
			{
				if (UNiagaraSystem* System = *It)
				{
					System->UpdateScalability();
				}
			}
		}
		GUpdatedFeatureMask.Reset();
	}
} //NiagaraStatelessCommon

const FNiagaraStatelessGlobals& FNiagaraStatelessGlobals::Get()
{
	return NiagaraStatelessCommon::GGlobals;
}
