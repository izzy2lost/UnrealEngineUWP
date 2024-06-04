// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Materials/MaterialIRCommon.h"

#if WITH_EDITOR

namespace Utility {

bool IsMaterialPropertyShared(EMaterialProperty InProperty);

//
MaterialIR::FValuePtr CreateMaterialAttributeDefaultValue(MaterialIR::FBuilder& Builder, const FMaterial* Material, EMaterialProperty Property);

} // namespace Utility

#endif
