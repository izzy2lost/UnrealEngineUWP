// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "ColorManagementDefines.h"

namespace ElectraColorimetryUtils
{
	static constexpr uint8 DefaultMPEGColorPrimaries = 2;
	static constexpr uint8 DefaultMPEGMatrixCoefficients = 2;
	static constexpr uint8 DefaultMPEGTransferCharacteristics = 2;

	UE::Color::EColorSpace TranslateMPEGColorPrimaries(uint8 InPrimaries)
	{
		switch (InPrimaries)
		{
			case 1:				// Rec709
			case 2:				// unknown
				return UE::Color::EColorSpace::sRGB;
			case 9:				// Rec2020
				return UE::Color::EColorSpace::Rec2020;
			default:
				check(!"Unexpected MPEG color primaries value!");
		}
		return UE::Color::EColorSpace::sRGB;
	}

	UE::Color::EColorSpace TranslateMPEGMatrixCoefficients(uint8 InMatrixCoefficients)
	{
		switch (InMatrixCoefficients)
		{
			case 0:				// ID (RGB)
				return UE::Color::EColorSpace::None;
			case 1:				// Rec709
			case 2:				// unknown
				return UE::Color::EColorSpace::sRGB;
			case 9:				// Rec2020
				return UE::Color::EColorSpace::Rec2020;
			default:
				check(!"Unexpected MPEG color primaries value!");
		}
		return UE::Color::EColorSpace::sRGB;
	}

	UE::Color::EEncoding TranslateMPEGTransferCharacteristics(uint8 InTransferCharacteristics)
	{
		switch (InTransferCharacteristics)
		{
			case 1:
			case 6:
			case 14:
			case 15:
			case 2:				// unknown
				return UE::Color::EEncoding::sRGB;
			case 8:
				return UE::Color::EEncoding::Linear;
			case 16:
				return UE::Color::EEncoding::ST2084;
			case 18:
				return UE::Color::EEncoding::sRGB;	// using sRGB in place of HLG for now
			default:
				check(!"*** Unexpected MPEG color transfer characteristics!");
		}
		return UE::Color::EEncoding::sRGB;
	}

	const FVector2d* GetColorPrimaries(UE::Color::EColorSpace InColorSpace)
	{
		static const FVector2d DP_sRGB[3] = { {0.64, 0.33} , {0.30, 0.60}, {0.15, 0.06} };
		static const FVector2d DP_Rec2020[3] = { {0.708, 0.292} , {0.170, 0.797}, {0.131, 0.046} };

		switch (InColorSpace)
		{
		case UE::Color::EColorSpace::sRGB:
			return DP_sRGB;
		case UE::Color::EColorSpace::Rec2020:
			return DP_Rec2020;
		default:
			check(!"*** Unexpected color primaries!");
		}
		return DP_sRGB;
	}
};
