// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/EnumAsByte.h"
#include "Types/SlateEnums.h"
#include "AvaViewportGuide.generated.h"

UENUM()
enum class EAvaViewportGuideState_Deprecated : uint8
{
	Disabled,
	Enabled,
	SnappedTo
};

USTRUCT()
struct FAvaViewportGuideInfo_Deprecated
{
	GENERATED_BODY()

	UPROPERTY()
	TEnumAsByte<EOrientation> Orientation = EOrientation::Orient_Horizontal;

	UPROPERTY()
	float OffsetFraction = 0.0f;

	UPROPERTY()
	EAvaViewportGuideState_Deprecated State = EAvaViewportGuideState_Deprecated::Disabled;

	bool IsEnabled() const { return State != EAvaViewportGuideState_Deprecated::Disabled; }
};
