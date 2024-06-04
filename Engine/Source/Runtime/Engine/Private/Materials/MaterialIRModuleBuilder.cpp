// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialIRModuleBuilder.h"

#if WITH_EDITOR

#include "Materials/MaterialIRModule.h"
#include "Materials/MaterialIR.h"
#include "Materials/MaterialIRTypes.h"
#include "Materials/MaterialIRBuilder.h"
#include "MaterialIRUtility.h"

#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/Material.h"
#include "MaterialExpressionIO.h"
#include "MaterialShared.h"

namespace IR = MaterialIR;

struct FMaterialIRModuleBuilder::FHelper
{
};

void FMaterialIRModuleBuilder::SetSource(FMaterial* InMaterial, const FStaticParameterSet* InStaticParameters)
{
	Material = InMaterial;
	StaticParameters = InStaticParameters;
}

void FMaterialIRModuleBuilder::SetPlatform(EShaderPlatform InShaderPlatform, ERHIFeatureLevel::Type InFeatureLevel, const ITargetPlatform* InTargetPlatform)
{
	ShaderPlatform	= InShaderPlatform;
	FeatureLevel		= InFeatureLevel;
	TargetPlatform	= InTargetPlatform;
}

void FMaterialIRModuleBuilder::SetTarget(FMaterialIRModule* InModule)
{
	Module = InModule;
}

bool FMaterialIRModuleBuilder::Build()
{
	Module->Reset(ShaderPlatform, FeatureLevel, TargetPlatform);

	IR::FBuilder Builder{ Module };

	UMaterial* BaseMaterial = Material->GetMaterialInterface()->GetMaterial();

	for (int32 PropertyIndex = 0; PropertyIndex < MP_MAX; ++PropertyIndex)
	{
		EMaterialProperty Property = (EMaterialProperty)PropertyIndex;
		FMaterialInputDescription Input;

		if (!Utility::IsMaterialPropertyShared(Property) 
			|| Property == MP_SubsurfaceColor
			|| Property == MP_FrontMaterial
			|| !BaseMaterial->GetExpressionInputDescription(Property, Input))
		{
			continue;
		}

		IR::FSetMaterialOutputInstr* Output = Builder.EmitSetMaterialOutput(Property, nullptr);
	
		if (Input.bUseConstant)
		{
			Output->ArgValue = Builder.NewConstantFromShaderValue(Input.ConstantValue);
		}
		else if (!Input.Input->IsConnected())
		{
			Output->ArgValue = Utility::CreateMaterialAttributeDefaultValue(Builder, Material, Property);
		}
		else if (Input.Input->IsConnected())
		{
			UE_MIR_UNREACHABLE();
		}
	}

	return true;
}

#endif
