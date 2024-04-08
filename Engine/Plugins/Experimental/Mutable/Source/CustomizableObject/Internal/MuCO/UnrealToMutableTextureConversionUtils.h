// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Engine/Texture.h"

struct CUSTOMIZABLEOBJECT_API FMutableSourceTextureData
{
	FTextureSource Source;
	bool bFlipGreenChannel = false;
	bool bHasAlphaChannel = false;
	bool bCompressionForceAlpha = false;
	bool bIsNormalComposite = false;
};


#if WITH_EDITOR

namespace mu
{
	class Image;
}

// Forward declarations
class UTexture2D;

enum class EUnrealToMutableConversionError 
{
    Success,
    UnsupportedFormat,
    CompositeImageDimensionMismatch,
    CompositeUnsupportedFormat,
    Unknown
};

CUSTOMIZABLEOBJECT_API EUnrealToMutableConversionError ConvertTextureUnrealSourceToMutable(mu::Image* OutResult, FMutableSourceTextureData&, uint8 MipmapsToSkip);

#endif
