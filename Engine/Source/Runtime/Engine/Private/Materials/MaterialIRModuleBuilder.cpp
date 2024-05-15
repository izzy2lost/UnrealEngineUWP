// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialIRModuleBuilder.h"

#if WITH_EDITOR

#include "Materials/MaterialIRModule.h"
#include "Materials/MaterialIR.h"
#include "Materials/MaterialIRTypes.h"
#include "Materials/MaterialIRBuilder.h"
#include "MaterialShared.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/Material.h"
#include "MaterialExpressionIO.h"

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

	MaterialIR::FBuilder Builder{ Module };

	UMaterial* BaseMaterial = Material->GetMaterialInterface()->GetMaterial();

	for (int32 PropertyIndex = 0; PropertyIndex < MP_MAX; ++PropertyIndex)
	{
		EMaterialProperty Property = (EMaterialProperty)PropertyIndex;
		FMaterialInputDescription Input;

		if (Property == MP_CustomOutput || Property == MP_MaterialAttributes || !BaseMaterial->GetExpressionInputDescription(Property, Input))
		{
			continue;
		}

		MaterialIR::FSetMaterialOutputInstr* Output = Builder.EmitSetMaterialOutput(Property, nullptr);
	
		if (Input.bUseConstant)
		{
			Output->ArgValue = Builder.NewConstantFromShaderValue(Input.ConstantValue);
		}
		else if (!Input.Input->IsConnected() && Input.Type != UE::Shader::EValueType::Void)
		{
			UE::Shader::FValue Zero{ Input.Type };
			Output->ArgValue = Builder.NewConstantFromShaderValue(Zero);
		}
		else if (Input.Input->IsConnected())
		{
			UE_MIR_UNREACHABLE();
		}
	}

	return true;
}

#endif
