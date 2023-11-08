// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureSourceDataUtils.h"

#if WITH_EDITOR

#include "ImageCoreUtils.h"
#include "Engine/Texture.h"
#include "HAL/UnrealMemory.h"
#include "EngineLogs.h"

namespace UE::TextureUtilitiesCommon::Experimental
{

namespace Private
{

	// resize so that the largest dimension is <= MaxSize
	bool ResizeTexture2D(UTexture* Texture, int32 MaxSize, const ITargetPlatform* TargetPlatform)
	{
		// We want to reduce the asset size so ignore the imported mip(s)
		const int32 MipIndex = 0;
		FImage SourceMip0;
		if (!Texture->Source.GetMipImage(SourceMip0, MipIndex))
		{
			return false;
		}

		int32 NumSlices = Texture->Source.GetNumSlices(); // == 1 or 6 for cubes

		const int32 LayerIndex = 0;
		bool MadeChanges;
		if ( ! Texture->DownsizeImageUsingTextureSettings(TargetPlatform, SourceMip0, MaxSize, LayerIndex, MadeChanges) )
		{
			// a critical error
			return false;
		}
		if ( ! MadeChanges )
		{
			return false;
		}
		
		Texture->PreEditChange(nullptr);

		FImage ResizedImage = MoveTemp(SourceMip0); // this is just a variable rename

		check( ResizedImage.NumSlices == NumSlices ); // slices are done one by one

		UE::Serialization::FEditorBulkData::FSharedBufferWithID ResizedImageBufferWithID = MakeSharedBufferFromArray(MoveTemp(ResizedImage.RawData));

		const int32 NumMips = 1;
		Texture->Source.Init(ResizedImage.SizeX
			, ResizedImage.SizeY
			, ResizedImage.NumSlices
			, NumMips
			, FImageCoreUtils::ConvertToTextureSourceFormat(ResizedImage.Format)
			, MoveTemp(ResizedImageBufferWithID));

		// if gamma was Pow22 it is now sRGB
		Texture->bUseLegacyGamma = false;

		// PostEditChange is called outside by our caller

		return true;
	}

	bool ResizeTextureSlicesOneByOne(UTexture* Texture, int32 MaxSize, const ITargetPlatform* TargetPlatform)
	{
		// @@ delete me; this function is not used; the normal ResizeTexture2D does the same thing

		// should check Source size vs MaxSize and early return here
		if ( Texture->Source.GetSizeX() <= MaxSize && Texture->Source.GetSizeY() <= MaxSize )
			return false;
				
		FImage ResizedImage;

		{
			check( Texture->Source.GetNumSlices() > 1 );

			TArray<FImage> ResizedSlices;
			ResizedSlices.Reserve(Texture->Source.GetNumSlices());

			ERawImageFormat::Type FormatUsed;
			EGammaSpace GammaSpaceUsed;
			bool MadeAnyChanges = false;

			{
				// We want to reduce the asset size so ignore the imported mip(s)
				TArray<FImage> Slices;
				Slices.Reserve(Texture->Source.GetNumSlices());

				{
					// We could probably avoid a copy here but for the simplicity of the code keep it for now.
					FImage SourceMip0;
					if (!Texture->Source.GetMipImage(SourceMip0, 0))
					{
						return false;
					}

					FormatUsed = SourceMip0.Format;
					GammaSpaceUsed = SourceMip0.GammaSpace;

					check( SourceMip0.NumSlices == Texture->Source.GetNumSlices() );

					for (int32 Index = 0; Index < SourceMip0.NumSlices; ++Index)
					{
						FImage& Slice = Slices.AddDefaulted_GetRef();
						FImageView SliceView = SourceMip0.GetSlice(Index);

						SliceView.CopyTo(Slice); // allocs new image in Slice
					}
				}

				check( Slices.Num() == Texture->Source.GetNumSlices() );

				for (FImage& Slice : Slices)
				{
					const int32 LayerIndex = 0;
					bool MadeChanges;
					if ( ! Texture->DownsizeImageUsingTextureSettings(TargetPlatform, Slice, MaxSize, LayerIndex, MadeChanges) )
					{
						// a critical error
						return false;
					}
					MadeAnyChanges = MadeAnyChanges || MadeChanges;
					
					FImage& ResizedSlice = ResizedSlices.AddDefaulted_GetRef();
					ResizedSlice = MoveTemp(Slice);
				}
			}

			check( ResizedSlices.Num() == Texture->Source.GetNumSlices() );
			
			if ( ! MadeAnyChanges )
			{
				return false;
			}

			// Move the resized slices into the resized image
			ResizedImage.Init(ResizedSlices[0].SizeX,ResizedSlices[0].SizeY, ResizedSlices.Num(), FormatUsed, GammaSpaceUsed);

			for (int32 Index = 0; Index < ResizedImage.NumSlices; ++Index)
			{
				FImageCore::CopyImage(ResizedSlices[Index], ResizedImage.GetSlice(Index));
			}
		}
		
		// Protect the code from an async build of the texture
		Texture->PreEditChange(nullptr);

		UE::Serialization::FEditorBulkData::FSharedBufferWithID ResizedImageBufferWithID = MakeSharedBufferFromArray(MoveTemp(ResizedImage.RawData));

		const int32 NumMips = 1;
		Texture->Source.Init(ResizedImage.SizeX
			, ResizedImage.SizeY
			, ResizedImage.NumSlices
			, NumMips
			, FImageCoreUtils::ConvertToTextureSourceFormat(ResizedImage.Format)
			, MoveTemp(ResizedImageBufferWithID));
			
		// if gamma was Pow22 it is now sRGB
		Texture->bUseLegacyGamma = false;

		return true;
	}

	bool ResizeTexture2DBlocked(UTexture* Texture, int32 MaxSize, const ITargetPlatform* TargetPlatform)
	{
		// note: does not support layers
		// MaxSize is applied to each block in the UDIM, not the total size

		/*
		FIntPoint LogicalSourceSize = Texture->Source.GetLogicalSize();
		double RatioX = double(MaxSize) / LogicalSourceSize.X;
		double RatioY = double(MaxSize) / LogicalSourceSize.Y;

		// early return if we're not shrinking
		if ( RatioX >= 1.0 && RatioY >= 1.0 )
		{
			return false;
		}
		*/

		TArray<FTextureSourceBlock > ResizedSourceBlocks;
		ResizedSourceBlocks.Reserve(Texture->Source.GetNumBlocks());
	
		TArray<FImage> ResizedBlocks;
		ResizedBlocks.Reserve(Texture->Source.GetNumBlocks());

		bool MadeAnyChanges = false;

		for (int32 BlockIndex = 0; BlockIndex < Texture->Source.GetNumBlocks(); ++BlockIndex)
		{
			// We want to reduce the asset size so ignore the imported mip(s)
			FImage SourceMip0;
			const int32 MipIndex = 0;
			const int32 LayerIndex = 0;
			if (!Texture->Source.GetMipImage(SourceMip0, BlockIndex, LayerIndex, MipIndex))
			{
				return false;
			}

			//int32 BlockMaxSize = FMath::RoundToInt32(FMath::Min(ResizedSourceBlock.SizeX * RatioX, ResizedSourceBlock.SizeY * RatioY));

			// each block is resized to MaxSize
			bool MadeChanges;
			if ( ! Texture->DownsizeImageUsingTextureSettings(TargetPlatform, SourceMip0, MaxSize, LayerIndex, MadeChanges) )
			{
				// critical error
				return false;
			}
			MadeAnyChanges = MadeAnyChanges || MadeChanges;

			FTextureSourceBlock& ResizedSourceBlock = ResizedSourceBlocks.AddDefaulted_GetRef();
			Texture->Source.GetBlock(BlockIndex, ResizedSourceBlock);
		
			FImage& ResizedBlock = ResizedBlocks.AddDefaulted_GetRef();
			ResizedBlock = MoveTemp(SourceMip0);
			
			ResizedSourceBlock.SizeX = ResizedBlock.SizeX;
			ResizedSourceBlock.SizeY = ResizedBlock.SizeY;
			ResizedSourceBlock.NumSlices = 1;
		}

		if ( ! MadeAnyChanges )
		{
			return false;
		}
		
		// Protect the code from an async build of the texture
		Texture->PreEditChange(nullptr);

		int64 SizeNeededInBytes = 0;
		for (const FImage& Block : ResizedBlocks)
		{
			SizeNeededInBytes += Block.RawData.Num(); // Block.GetImageSizeBytes()
		}
		FUniqueBuffer WriteImageBuffer = FUniqueBuffer::Alloc(SizeNeededInBytes);

		uint8* CurrentAddress = static_cast<uint8*>(WriteImageBuffer.GetData());
		for (FImage& Block : ResizedBlocks)
		{
			FMemory::Memcpy(CurrentAddress, static_cast<uint8*>(Block.RawData.GetData()), Block.RawData.Num());
			CurrentAddress += Block.RawData.Num();
		}

		UE::Serialization::FEditorBulkData::FSharedBufferWithID ResizedImageBufferWithID = WriteImageBuffer.MoveToShared();

		const ETextureSourceFormat SourceFormat = Texture->Source.GetFormat();
		int32 NumLayers = 1;
		Texture->Source.InitBlocked(
			&SourceFormat,
			ResizedSourceBlocks.GetData(),
			NumLayers,
			ResizedSourceBlocks.Num(),
			MoveTemp(ResizedImageBufferWithID)
		);

		// if gamma was Pow22 it is now sRGB
		Texture->bUseLegacyGamma = false;

		return true;
	}
}


bool DownsizeTextureSourceData(UTexture* Texture, int32 TargetSizeInGame, const ITargetPlatform* TargetPlatform)
{
	check( Texture->Source.IsValid() );

	// Check if we don't know how to resize that texture

	if ( Texture->Source.GetNumMips() > 1 && Texture->MipGenSettings == TMGS_LeaveExistingMips )
	{
		//	should not do this if MipGen == LeaveExisting ; or maybe warn?
		// return false;

		// go ahead and do it, but warn:

		UE_LOG(LogTexture,Warning,TEXT("DownsizeTextureSourceData: Texture has LeaveExistingMips ; they will be discarded! [%s]"),
			*Texture->GetFullName());
	}

	// we only support 1 layer currently
	if (Texture->Source.GetNumLayers() != 1)
	{
		return false;
	}

	if (!(Texture->Source.GetTextureClass() == ETextureClass::Cube || Texture->Source.GetTextureClass() == ETextureClass::TwoD))
	{
		return false;
	}

	if (Texture->Source.GetTextureClass() == ETextureClass::TwoD && Texture->Source.GetNumSlices() != 1)
	{
		return false;
	}

	if (Texture->Source.GetTextureClass() == ETextureClass::Cube)
	{
		if (Texture->Source.IsLongLatCubemap())
		{
			if (Texture->Source.GetNumSlices() != 1)
			{
				return false;
			}
		}
		else if (Texture->Source.GetNumSlices() != 6)
		{
			return false;
		}
	}

	int32 TargetSourceSize = TargetSizeInGame;
	if (Texture->Source.IsLongLatCubemap())
	{
		// The function return the max size of the generated cube from the source long lat
		// this should be kept in sync with the implementation details of ComputeLongLatCubemapExtents() or refactored
		TargetSourceSize = (1U << FMath::FloorLog2(TargetSizeInGame)) * 2;
	}

	FIntPoint SourceSize = Texture->Source.GetLogicalSize();
	if (SourceSize.X <= TargetSourceSize && SourceSize.Y <= TargetSourceSize)
	{
		return false;
	}

	if (Texture->Source.GetTextureClass() == ETextureClass::TwoD)
	{
		if (Texture->Source.GetNumBlocks() == 1)
		{
			return Private::ResizeTexture2D(Texture, TargetSourceSize, TargetPlatform);
		}
		else
		{
			// UDIM(s)
			return Private::ResizeTexture2DBlocked(Texture, TargetSourceSize, TargetPlatform);
		}
	}
	else if (Texture->Source.GetTextureClass() == ETextureClass::Cube)
	{
		if (Texture->Source.IsLongLatCubemap())
		{
			return Private::ResizeTexture2D(Texture, TargetSourceSize, TargetPlatform);
		}
		else
		{
			//return Private::ResizeTextureSlicesOneByOne(Texture, TargetSourceSize, TargetPlatform);
			return Private::ResizeTexture2D(Texture, TargetSourceSize, TargetPlatform);
		}
	}
	// could do GetTextureClass == Array ?
	// other classes unsupported
	
	return false;
}

bool DownsizeTexureSourceDataNearRenderingSize(UTexture* Texture, const ITargetPlatform* TargetPlatform)
{
	if ( ! Texture->Source.IsValid() )
	{
		return false;
	}

	int32 BeforeSizeX;
	int32 BeforeSizeY;
	Texture->GetBuiltTextureSize(TargetPlatform, BeforeSizeX, BeforeSizeY);

	int32 TargetSize = FMath::Max(BeforeSizeX, BeforeSizeY);
	if (DownsizeTextureSourceData(Texture, TargetSize, TargetPlatform))
	{
		Texture->LODBias = 0;
		Texture->PostEditChange();
		
		// check that GetBuiltTextureSize was preserved :
		int32 AfterSizeX;
		int32 AfterSizeY;
		Texture->GetBuiltTextureSize(TargetPlatform, AfterSizeX, AfterSizeY);

		if ( BeforeSizeX != AfterSizeX ||
			 BeforeSizeY != AfterSizeY )
		{
			UE_LOG(LogTexture,Warning,TEXT("DownsizeTexureSourceDataNearRenderingSize failed to preserve built size; was: %dx%d now: %dx%d on [%s]"),
				BeforeSizeX,BeforeSizeY,
				AfterSizeX,AfterSizeY,
				*Texture->GetFullName());
		}

		return true;
	}
	//	PreEditChange may have been called even if DownsizeTextureSourceData return false
	//	we don't PostEditChange here
	//	that's okay but not great

	return false;
}

} // End namespace UE::TextureUtilitiesCommon::Experimental

#endif //WITH_EDITOR
