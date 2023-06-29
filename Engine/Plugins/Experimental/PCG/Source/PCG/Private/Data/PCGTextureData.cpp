// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGTextureData.h"

#include "PCGTextureReadback.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGAsync.h"
#include "Helpers/PCGHelpers.h"

#include "TextureResource.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGTextureData)

namespace PCGTextureSampling
{
	template<typename ValueType>
	bool Sample(const FVector2D& InPosition, 
		const FBox2D& InSurface, 
		const UPCGBaseTextureData* InTextureData, 
		int32 Width, 
		int32 Height, 
		ValueType& SampledValue,
		TFunctionRef<ValueType(int32 Index)> SamplingFunction)
	{
		check(Width > 0 && Height > 0);
		if (Width <= 0 || Height <= 0 || InSurface.GetSize().SquaredLength() <= 0)
		{
			return false;
		}

		check(InTextureData);

		const FVector2D LocalSpacePos = (InPosition - InSurface.Min) / InSurface.GetSize();
		FVector2D Pos = FVector2D::ZeroVector;
		if (!InTextureData->bUseAdvancedTiling)
		{
			// TODO: There seems to be a bias issue here, as the bounds size are not in the same space as the texels.
			Pos = LocalSpacePos * FVector2D(Width, Height);
		}
		else
		{
			// Conceptually, we are building "tiles" in texture space with the origin being in the middle of the [0, 0] tile.
			// The offset is given in a ratio of [0, 1], applied "before" scaling & rotation.
			// Rotation is done around the center given, where the center is (0.5, 0.5) + offset
			// Scaling controls the horizon of tiles, and the tile selection is done through min-max bounds, in tile space,
			// with the origin tile being from -0.5 to 0.5.
			const FRotator Rotation = FRotator(0.0, -InTextureData->Rotation, 0.0);
			FVector Scale = FVector(InTextureData->Tiling, 1.0);
			Scale.X = ((FMath::Abs(Scale.X) > SMALL_NUMBER) ? (1.0 / Scale.X) : 0.0);
			Scale.Y = ((FMath::Abs(Scale.Y) > SMALL_NUMBER) ? (1.0 / Scale.Y) : 0.0);
			const FVector Translation = FVector(0.5 + InTextureData->CenterOffset.X, 0.5 + InTextureData->CenterOffset.Y, 0);

			FTransform Transform = FTransform(Rotation, Translation, Scale);
			
			// Transform to tile-space
			const FVector2D SamplePosition = FVector2D(Transform.InverseTransformPosition(FVector(LocalSpacePos, 0.f)));

			if (InTextureData->bUseTileBounds && !InTextureData->TileBounds.IsInsideOrOn(SamplePosition))
			{
				return false;
			}

			FVector::FReal X = FMath::Frac(SamplePosition.X + 0.5);
			FVector::FReal Y = FMath::Frac(SamplePosition.Y + 0.5);

			X *= Width;
			Y *= Height;

			Pos = FVector2D(X, Y);
		}

		// TODO: this isn't super robust, if that becomes an issue
		int32 X0 = FMath::FloorToInt(Pos.X);
		if (X0 < 0 || X0 >= Width)
		{
			X0 = 0;
		}

		int32 X1 = FMath::CeilToInt(Pos.X);
		if (X1 < 0 || X1 >= Width)
		{
			X1 = 0;
		}

		int32 Y0 = FMath::FloorToInt(Pos.Y);
		if (Y0 < 0 || Y0 >= Height)
		{
			Y0 = 0;
		}

		int32 Y1 = FMath::CeilToInt(Pos.Y);
		if (Y1 < 0 || Y1 >= Height)
		{
			Y1 = 0;
		}

		ValueType SampleX0Y0 = SamplingFunction(X0 + Y0 * Width);
		ValueType SampleX1Y0 = SamplingFunction(X1 + Y0 * Width);
		ValueType SampleX0Y1 = SamplingFunction(X0 + Y1 * Width);
		ValueType SampleX1Y1 = SamplingFunction(X1 + Y1 * Width);

		SampledValue = FMath::BiLerp(SampleX0Y0, SampleX1Y0, SampleX0Y1, SampleX1Y1, Pos.X - X0, Pos.Y - Y0);
		return true;
	}

	float SampleFloatChannel(const FLinearColor& InColor, EPCGTextureColorChannel ColorChannel)
	{
		switch (ColorChannel)
		{
		case EPCGTextureColorChannel::Red:
			return InColor.R;
		case EPCGTextureColorChannel::Green:
			return InColor.G;
		case EPCGTextureColorChannel::Blue:
			return InColor.B;
		case EPCGTextureColorChannel::Alpha:
		default:
			return InColor.A;
		}
	}
}

FBox UPCGBaseTextureData::GetBounds() const
{
	return Bounds;
}

FBox UPCGBaseTextureData::GetStrictBounds() const
{
	return Bounds;
}

bool UPCGBaseTextureData::SamplePoint(const FTransform& InTransform, const FBox& InBounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	// TODO: add metadata support
	// TODO: add sampling along the bounds

	// TODO: needs unpicking of sample vs projection. I believe the below is a projection.. But semantics are slightly different.
	// 1 - We have some information telling us the 'z' size of the surface allowing us to reject points that would be too far from the surface, maybe including some density falloff by distance
	// 2 - We suppose that the surface has an infinite 'z' size, in which case the sampling is basically the same as the sampling, except that it does not change the position
	// 3 - The surface is infinitesimal - we'll return something if and only if the point overlaps with the projected position

	if (!IsValid())
	{
		return false;
	}

	// Compute transform
	// TODO: embed local bounds center offset at this time?
	OutPoint.Transform = InTransform;
	FVector PointPositionInLocalSpace = Transform.InverseTransformPosition(InTransform.GetLocation());
	PointPositionInLocalSpace.Z = 0;
	OutPoint.Transform.SetLocation(Transform.TransformPosition(PointPositionInLocalSpace));
	OutPoint.SetLocalBounds(InBounds); // TODO: should set Min.Z = Max.Z = 0;

	// Compute density & color (& metadata)
	// TODO: sample in the bounds given, not only on a single pixel
	FVector2D Position2D(PointPositionInLocalSpace.X, PointPositionInLocalSpace.Y);
	FBox2D Surface(FVector2D(-1.0f, -1.0f), FVector2D(1.0f, 1.0f));

	FLinearColor Color = FLinearColor(EForceInit::ForceInit);
	if (PCGTextureSampling::Sample<FLinearColor>(Position2D, Surface, this, Width, Height, Color, [this](int32 Index) { return ColorData[Index]; }))
	{
		OutPoint.Color = Color;
		OutPoint.Density = ((DensityFunction == EPCGTextureDensityFunction::Ignore) ? 1.0f : PCGTextureSampling::SampleFloatChannel(Color, ColorChannel));
		return OutPoint.Density > 0;
	}
	else
	{
		return false;
	}
}

const UPCGPointData* UPCGBaseTextureData::CreatePointData(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGBaseTextureData::CreatePointData);
	// TODO: this is a trivial implementation
	// A better sampler would allow to sample a fixed number of points in either direction
	// or based on a given texel size
	FBox2D LocalSurfaceBounds(FVector2D(-1.0f, -1.0f), FVector2D(1.0f, 1.0f));

	UPCGPointData* Data = NewObject<UPCGPointData>();
	Data->InitializeFromData(this);

	// Early out for invalid data
	if (!IsValid())
	{
		UE_LOG(LogPCG, Error, TEXT("Texture data does not have valid sizes - will return empty data"));
		return Data;
	}

	TArray<FPCGPoint>& Points = Data->GetMutablePoints();

	// Map target texel size to the current physical size of the texture data.
	const FVector::FReal XSize = 2.0 * Transform.GetScale3D().X;
	const FVector::FReal YSize = 2.0 * Transform.GetScale3D().Y;

	const int32 XCount = FMath::Floor(XSize / TexelSize);
	const int32 YCount = FMath::Floor(YSize / TexelSize);
	const int32 PointCount = XCount * YCount;

	if (PointCount <= 0)
	{
		UE_LOG(LogPCG, Warning, TEXT("Texture data has a texel size larger than its data - will return empty data"));
		return Data;
	}

	const FBox2D Surface(FVector2D(-1.0f, -1.0f), FVector2D(1.0f, 1.0f));

	FPCGAsync::AsyncPointProcessing(Context, PointCount, Points, [this, XCount, YCount, &Surface](int32 Index, FPCGPoint& OutPoint)
	{
		const int X = (Index % XCount);
		const int Y = (Index / YCount);

		// TODO: we should have a 0.5 bias here
		FVector2D LocalCoordinate((2.0 * X + 0.5) / XCount - 1.0, (2.0 * Y + 0.5) / YCount - 1.0);
		FLinearColor Color = FLinearColor(EForceInit::ForceInit);

		if (PCGTextureSampling::Sample<FLinearColor>(LocalCoordinate, Surface, this, Width, Height, Color, [this](int32 Index) { return ColorData[Index]; }))
		{
			const float Density = ((DensityFunction == EPCGTextureDensityFunction::Ignore) ? 1.0f : PCGTextureSampling::SampleFloatChannel(Color, ColorChannel));
#if WITH_EDITOR
			if (Density > 0 || bKeepZeroDensityPoints)
#else
			if (Density > 0)
#endif
			{
				FVector LocalPosition(LocalCoordinate, 0);
				OutPoint = FPCGPoint(FTransform(Transform.TransformPosition(LocalPosition)),
					Density,
					PCGHelpers::ComputeSeed(X, Y));

				OutPoint.SetExtents(FVector(TexelSize / 2.0));
				OutPoint.Color = Color;

				return true;
			}
		}

		return false;
	});

	return Data;
}

bool UPCGBaseTextureData::IsValid() const
{
	return Height > 0 && Width > 0;
}

void UPCGBaseTextureData::CopyBaseTextureData(UPCGBaseTextureData* NewTextureData) const
{
	CopyBaseSurfaceData(NewTextureData);

	NewTextureData->DensityFunction = DensityFunction;
	NewTextureData->ColorChannel = ColorChannel;
	NewTextureData->TexelSize = TexelSize;
	NewTextureData->bUseAdvancedTiling = bUseAdvancedTiling;
	NewTextureData->Tiling = Tiling;
	NewTextureData->CenterOffset = CenterOffset;
	NewTextureData->Rotation = Rotation;
	NewTextureData->bUseTileBounds = bUseTileBounds;
	NewTextureData->TileBounds = TileBounds;
	NewTextureData->ColorData = ColorData;
	NewTextureData->Bounds = Bounds;
	NewTextureData->Height = Height;
	NewTextureData->Width = Width;
}

void UPCGTextureData::Initialize(UTexture2D* InTexture, const FTransform& InTransform, const TFunction<void()>& PostInitializeCallback)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGTextureData::Initialize);

	Texture = InTexture;
	Transform = InTransform;
	Width = 0;
	Height = 0;

	Bounds = FBox(EForceInit::ForceInit);
	Bounds += FVector(-1.0f, -1.0f, 0.0f);
	Bounds += FVector(1.0f, 1.0f, 0.0f);
	Bounds = Bounds.TransformBy(Transform);

	if (!InTexture)
	{
		PostInitializeCallback();
		return;
	}

	if (!IsSupported(InTexture))
	{
		UE_LOG(LogPCG, Error, TEXT("PCGTextureData does not support one or more settings on '%s'"), *Texture->GetFName().ToString());

		PostInitializeCallback();
		return;
	}

#if WITH_EDITOR
	// Force a wait on any incomplete async texture compilation and caching operations
	Texture->FinishCachePlatformData();
#endif

	// TODO: Forcing a resource update is problematic. Clears cooked platform data and may break other usages of this texture
	// But it is necessary to force the texture memory to be present on the first dispatch
	Texture->UpdateResource();
	Texture->WaitForPendingInitOrStreaming();

	FTexturePlatformData* PlatformData = Texture->GetPlatformData();
	FTextureResource* TextureResource = InTexture->GetResource();

	if (PlatformData && TextureResource && TextureResource->TextureRHI)
	{
		FPCGTextureReadbackDispatchParams Params;
		Params.SourceTexture = TextureResource->TextureRHI;
		Params.SourceSampler = TextureResource->SamplerStateRHI;
		Params.SourceDimensions = FIntPoint(PlatformData->SizeX, PlatformData->SizeY);

		FPCGTextureReadbackInterface::Dispatch(Params, [this, PlatformData, PostInitializeCallback](void* OutBuffer, int32 ReadbackWidth, int32 ReadbackHeight)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UPCGTextureData::Initialize::DispatchCallback);

			const int32 PixelCount = ReadbackWidth * ReadbackHeight;

			if (PlatformData->SizeX * PlatformData->SizeY != PixelCount)
			{
				UE_LOG(LogPCG, Error,
					TEXT("PCGTextureData readback has different dimensions than the source texture '%s'. Expected (%d, %d), received (%d, %d)."),
					*Texture->GetFName().ToString(),
					PlatformData->SizeX, PlatformData->SizeY,
					ReadbackWidth, ReadbackHeight);
			}

			if (const FColor* FormattedImageData = reinterpret_cast<const FColor*>(OutBuffer))
			{
				Width = ReadbackWidth;
				Height = ReadbackHeight;
				ColorData.SetNum(PixelCount);

				for (int32 D = 0; D < PixelCount; ++D)
				{
					ColorData[D] = FormattedImageData[D].ReinterpretAsLinear();
				}
			}
			else
			{
				UE_LOG(LogPCG, Error, TEXT("PCGTextureData unable to get readback results from '%s'"), *Texture->GetFName().ToString());
			}

			PostInitializeCallback();
		});
	}
	else
	{
		UE_LOG(LogPCG, Error, TEXT("PCGTextureData failed to acquire texture resource for '%s'"), *Texture->GetFName().ToString());

		PostInitializeCallback();
	}
}

bool UPCGTextureData::IsSupported(UTexture2D* InTexture)
{
	// TODO: GPU readback seems to support all textures, but future implementations (such as UBitmap) may have
	// some limitations, so we will avoid removing this trivial function from the API for now
	return true;
}

UPCGSpatialData* UPCGTextureData::CopyInternal() const
{
	UPCGTextureData* NewTextureData = NewObject<UPCGTextureData>();

	CopyBaseTextureData(NewTextureData);

	NewTextureData->Texture = Texture;

	return NewTextureData;
}
