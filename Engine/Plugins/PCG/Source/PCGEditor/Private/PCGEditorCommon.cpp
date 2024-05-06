// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGEditorCommon.h"

namespace FPCGEditorCommon
{
	TAutoConsoleVariable<bool> CVarShowAdvancedAttributesFields(
		TEXT("pcg.graph.ShowAdvancedAttributes"),
		false,
		TEXT("Control whether advanced attributes/properties are shown in the PCG graph editor"));
}
