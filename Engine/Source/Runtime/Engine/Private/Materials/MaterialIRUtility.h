// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Materials/MaterialIRCommon.h"

#if WITH_EDITOR

namespace UE::Utility {

//
bool IsMaterialPropertyShared(EMaterialProperty InProperty);

//
bool NextMaterialAttributeInput(UMaterial* BaseMaterial, int32& PropertyIndex, FMaterialInputDescription& Input);

//
UE::MIR::FValue* CreateMaterialAttributeDefaultValue(UE::MIR::FEmitter& Emitter, const UMaterial* Material, EMaterialProperty Property);

} // namespace Utility

#endif
