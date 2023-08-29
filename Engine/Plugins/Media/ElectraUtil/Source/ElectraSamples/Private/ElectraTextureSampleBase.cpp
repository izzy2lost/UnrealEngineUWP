// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/Build.h"

#if !UE_SERVER

#include "HAL/Platform.h"
#include "ElectraTextureSample.h"
#include "ElectraTextureSampleUtils.h"
#include "ElectraSamplesModule.h"

// -------------------------------------------------------------------------------------------------------------------------------------------------------

void IElectraTextureSampleBase::Initialize(FVideoDecoderOutput* InVideoDecoderOutput)
{
	VideoDecoderOutput = StaticCastSharedPtr<FVideoDecoderOutput, IDecoderOutputPoolable, ESPMode::ThreadSafe>(InVideoDecoderOutput->AsShared());
	HDRInfo = VideoDecoderOutput->GetHDRInformation();
	Colorimetry = VideoDecoderOutput->GetColorimetry();

	// Get various basic MP4-style colorimetry values (we default to video range Rec709 SDR)
	bool bFullRange = false;
	uint8 ColorPrimaries = ElectraColorimetryUtils::DefaultMPEGColorPrimaries;
	uint8 TransferCharacteristics = ElectraColorimetryUtils::DefaultMPEGMatrixCoefficients;
	uint8 MatrixCoefficients = ElectraColorimetryUtils::DefaultMPEGTransferCharacteristics;
	if (auto PinnedColorimetry = Colorimetry.Pin())
	{
		bFullRange = (PinnedColorimetry->GetMPEGDefinition()->VideoFullRangeFlag != 0);
		ColorPrimaries = PinnedColorimetry->GetMPEGDefinition()->ColourPrimaries;
		TransferCharacteristics = PinnedColorimetry->GetMPEGDefinition()->TransferCharacteristics;
		MatrixCoefficients = PinnedColorimetry->GetMPEGDefinition()->MatrixCoefficients;
	}

	// Compute the bits per component in the data we get passed in
	EPixelFormat PixFmt = VideoDecoderOutput->GetFormat();
	uint8 NumBits = 8;
	if (!IsDXTCBlockCompressedTextureFormat(PixFmt))
	{
		if (PixFmt == PF_NV12)
		{
			NumBits = 8;
		}
		else if (PixFmt == PF_A2B10G10R10)
		{
			NumBits = 10;
		}
		else if (PixFmt == PF_P010)
		{
			NumBits = 16;
		}
		else
		{
			NumBits = (8 * GPixelFormats[PixFmt].BlockBytes) / GPixelFormats[PixFmt].NumComponents;
		}
	}

	FVector Off = FVector::Zero();
	const FMatrix* Mtx = nullptr;

	// Do we have specific HDR information, so we can assume a standard?
	if (auto PinnedHDRInfo = HDRInfo.Pin())
	{
		//
		// HDR information present
		//

		if (auto ColorVolume = PinnedHDRInfo->GetMasteringDisplayColourVolume())
		{
			WhitePoint = FVector2d(ColorVolume->white_point_x, ColorVolume->white_point_y);
			for (uint32 I = 0; I < 3; ++I)
			{
				DisplayPrimaries[I] = FVector2d(ColorVolume->display_primaries_x[I], ColorVolume->display_primaries_y[I]);
			}
		}
		else
		{
			WhitePoint = FVector2d(UE::Color::GetWhitePoint(UE::Color::EWhitePoint::CIE1931_D65));

			const FVector2d* DPs = ElectraColorimetryUtils::GetColorPrimaries(UE::Color::EColorSpace::Rec2020);
			DisplayPrimaries[0] = DPs[0];
			DisplayPrimaries[1] = DPs[1];
			DisplayPrimaries[2] = DPs[2];
		}

		switch (PinnedHDRInfo->GetHDRType())
		{
		case IVideoDecoderHDRInformation::EType::PQ10:
		case IVideoDecoderHDRInformation::EType::HDR10:
			ColorEncoding = UE::Color::EEncoding::ST2084;
			break;
		case IVideoDecoderHDRInformation::EType::HLG10:
			UE_LOG(LogElectraSamples, Warning, TEXT("Detected use of HLG EOTF in video material. Mapping to Rec709/sRGB as HLG is not supported."));
			ColorEncoding = UE::Color::EEncoding::sRGB;	// using sRGB in place of HLG for now
			break;
		default:
			check(!"Unknown HDR type!");
			ColorEncoding = UE::Color::EEncoding::sRGB;
			break;
		}

		ColorGamut = EDisplayColorGamut::Rec2020_D65;
		Mtx = bFullRange ? &MediaShaders::YuvToRgbRec2020Unscaled : &MediaShaders::YuvToRgbRec2020Scaled;
	}
	else
	{
		//
		// No HDR information present
		//

		const FVector2d* DPs = ElectraColorimetryUtils::GetColorPrimaries(ElectraColorimetryUtils::TranslateMPEGColorPrimaries(ColorPrimaries));
		DisplayPrimaries[0] = DPs[0];
		DisplayPrimaries[1] = DPs[1];
		DisplayPrimaries[2] = DPs[2];

		switch (ElectraColorimetryUtils::TranslateMPEGMatrixCoefficients(MatrixCoefficients))
		{
			case UE::Color::EColorSpace::None:	// ID (RGB)
				// no conversion, data is RGB
				break;
			case UE::Color::EColorSpace::sRGB:
				Mtx = bFullRange ? &MediaShaders::YuvToRgbRec709Unscaled : &MediaShaders::YuvToRgbRec709Scaled;
				ColorGamut = EDisplayColorGamut::sRGB_D65;
				break;
			case UE::Color::EColorSpace::Rec2020:
				Mtx = bFullRange ? &MediaShaders::YuvToRgbRec2020Unscaled : &MediaShaders::YuvToRgbRec2020Scaled;
				ColorGamut = EDisplayColorGamut::Rec2020_D65;
				break;
			default:
				check(!"*** Unexpected matrix coefficients!");
				Mtx = bFullRange ? &MediaShaders::YuvToRgbRec709Unscaled : &MediaShaders::YuvToRgbRec709Scaled;
				ColorGamut = EDisplayColorGamut::sRGB_D65;
		}

		ColorEncoding = ElectraColorimetryUtils::TranslateMPEGTransferCharacteristics(TransferCharacteristics);

		// No matter what: we will have the D65 white point (for now at least)
		WhitePoint = FVector2d(UE::Color::GetWhitePoint(UE::Color::EWhitePoint::CIE1931_D65));
	}

	if (Mtx)
	{
		// Select the offsets prior to YUV conversion needed per the incoming data
		switch (NumBits)
		{
		case 8:		Off = bFullRange ? MediaShaders::YUVOffsetNoScale8bits : MediaShaders::YUVOffset8bits; break;
		case 10:	Off = bFullRange ? MediaShaders::YUVOffsetNoScale10bits : MediaShaders::YUVOffset10bits; break;
		case 16:	Off = bFullRange ? MediaShaders::YUVOffsetNoScale16bits : MediaShaders::YUVOffset16bits; break;
		case 32:	Off = bFullRange ? MediaShaders::YUVOffsetNoScaleFloat : MediaShaders::YUVOffsetFloat; break;
		default:	check(!"Unexpected number of bits per channel!");
		}
	}

	// Correctional scale for input data
	// (data should be placed in the upper 10-bits of the 16-bit texture channels, but some platforms do not do this - they provide a correctional factor here)
	float DataScale = GetSampleDataScale(NumBits == 10);

	// Compute scale to make correct towards the max value (P010 will max out at 0xffc0 not 0xffff - so if it is present we need to adjust the scale a bit)
	float NormScale = (VideoDecoderOutput->GetFormat() == PF_P010) ? (65535.0f / 65472.0f) : 1.0f;
 
	// Matrix to transform sample data to standard YUV values
	FMatrix PreMtx = FMatrix::Identity;
	PreMtx.M[0][0] = DataScale * NormScale;
	PreMtx.M[1][1] = DataScale * NormScale;
	PreMtx.M[2][2] = DataScale * NormScale;
	PreMtx.M[0][3] = -Off.X;
	PreMtx.M[1][3] = -Off.Y;
	PreMtx.M[2][3] = -Off.Z;

	// Combine this with the actual YUV-RGB conversion
	SampleToRgbMtx = FMatrix44f(Mtx ? (*Mtx * PreMtx) : PreMtx);

	// Also store the plain YUV->RGB matrix (pointer) for later reference
	YuvToRgbMtx = Mtx;
}

void IElectraTextureSampleBase::InitializePoolable()
{
}

void IElectraTextureSampleBase::ShutdownPoolable()
{
	VideoDecoderOutput.Reset();
}

FIntPoint IElectraTextureSampleBase::GetDim() const
{
	if (VideoDecoderOutput)
	{
		return VideoDecoderOutput->GetDim();
	}
	return FIntPoint::ZeroValue;
}


FIntPoint IElectraTextureSampleBase::GetOutputDim() const
{
	if (VideoDecoderOutput)
	{
		return VideoDecoderOutput->GetOutputDim();
	}
	return FIntPoint::ZeroValue;
}


FMediaTimeStamp IElectraTextureSampleBase::GetTime() const
{
	if (VideoDecoderOutput)
	{
		const FDecoderTimeStamp TimeStamp = VideoDecoderOutput->GetTime();
		return FMediaTimeStamp(TimeStamp.Time, TimeStamp.SequenceIndex);
	}
	return FMediaTimeStamp();
}


FTimespan IElectraTextureSampleBase::GetDuration() const
{
	if (VideoDecoderOutput)
	{
		return VideoDecoderOutput->GetDuration();
	}
	return FTimespan::Zero();
}

bool IElectraTextureSampleBase::IsOutputSrgb() const
{
	return ColorEncoding == UE::Color::EEncoding::sRGB;
}

const FMatrix& IElectraTextureSampleBase::GetYUVToRGBMatrix() const
{
	return YuvToRgbMtx ? *YuvToRgbMtx : FMatrix::Identity;
}

bool IElectraTextureSampleBase::GetFullRange() const
{
	if (auto PinnedColorimetry = Colorimetry.Pin())
	{
		return (PinnedColorimetry->GetMPEGDefinition()->VideoFullRangeFlag != 0);
	}
	return false;
}

FMatrix44f IElectraTextureSampleBase::GetSampleToRGBMatrix() const
{
	return SampleToRgbMtx;
}

FMatrix44f IElectraTextureSampleBase::GetGamutToXYZMatrix() const
{
	return GamutToXYZMatrix(ColorGamut);
}

FVector2f IElectraTextureSampleBase::GetWhitePoint() const
{
	return FVector2f(WhitePoint);
}

FVector2f IElectraTextureSampleBase::GetDisplayPrimaryRed() const
{
	return FVector2f(DisplayPrimaries[0]);
}

FVector2f IElectraTextureSampleBase::GetDisplayPrimaryGreen() const
{
	return FVector2f(DisplayPrimaries[1]);
}

FVector2f IElectraTextureSampleBase::GetDisplayPrimaryBlue() const
{
	return FVector2f(DisplayPrimaries[2]);
}

UE::Color::EEncoding IElectraTextureSampleBase::GetEncodingType() const
{
	return ColorEncoding;
}

#endif
