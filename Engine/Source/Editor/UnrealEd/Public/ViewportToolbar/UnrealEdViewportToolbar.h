// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointerFwd.h"

class FText;
class SEditorViewport;

namespace UE::UnrealEd
{

UNREALED_API FText GetViewModesSubmenuLabel(TWeakPtr<SEditorViewport> InViewport);

} // namespace UE::UnrealEd
