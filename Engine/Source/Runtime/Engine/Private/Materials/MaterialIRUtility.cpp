// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialIRUtility.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/MaterialIRBuilder.h"
#include "MaterialShared.h"

#if WITH_EDITOR

namespace Utility {

namespace MIR = MaterialIR;

bool IsMaterialPropertyShared(EMaterialProperty InProperty)
{
	switch (InProperty)
	{
	case MP_Normal:
	case MP_Tangent:
	case MP_EmissiveColor:
	case MP_Opacity:
	case MP_OpacityMask:
	case MP_BaseColor:
	case MP_Metallic:
	case MP_Specular:
	case MP_Roughness:
	case MP_Anisotropy:
	case MP_AmbientOcclusion:
	case MP_Refraction:
	case MP_PixelDepthOffset:
	case MP_SubsurfaceColor:
	case MP_ShadingModel:
	case MP_SurfaceThickness:
	case MP_FrontMaterial:
	case MP_Displacement:
		return true;
	default:
		return false;
	}
}

MIR::FValuePtr CreateMaterialAttributeDefaultValue(MIR::FBuilder& Builder, const FMaterial* Material, EMaterialProperty Property)
{
	EMaterialValueType Type = FMaterialAttributeDefinitionMap::GetValueType(Property);
	FVector4f DefaultValue = FMaterialAttributeDefinitionMap::GetDefaultValue(Property);

	switch (Type)
	{
		case MCT_ShadingModel: return Builder.NewConstantInt1(Material->GetShadingModels().GetFirstShadingModel());

		case MCT_Float1: return Builder.NewConstantFloat1(  DefaultValue.X );
		case MCT_Float2: return Builder.NewConstantFloat2({ DefaultValue.X, DefaultValue.Y });
		case MCT_Float3: return Builder.NewConstantFloat3({ DefaultValue.X, DefaultValue.Y, DefaultValue.Z });
		case MCT_Float4: return Builder.NewConstantFloat4(DefaultValue);

		case MCT_UInt1: return Builder.NewConstantInt1(  (int32)DefaultValue.X );
		case MCT_UInt2: return Builder.NewConstantInt2({ (int32)DefaultValue.X, (int32)DefaultValue.Y });
		case MCT_UInt3: return Builder.NewConstantInt3({ (int32)DefaultValue.X, (int32)DefaultValue.Y, (int32)DefaultValue.Z });
		case MCT_UInt4: return Builder.NewConstantInt4({ (int32)DefaultValue.X, (int32)DefaultValue.Y, (int32)DefaultValue.Z, (int32)DefaultValue.W });

		default: UE_MIR_UNREACHABLE();
	}
}

} // namespace Utility
#endif
