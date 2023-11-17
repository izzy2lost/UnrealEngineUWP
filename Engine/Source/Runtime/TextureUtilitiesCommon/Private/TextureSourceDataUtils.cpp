// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureSourceDataUtils.h"

#if WITH_EDITOR

#include "ImageCoreUtils.h"
#include "Engine/Texture.h"
#include "HAL/UnrealMemory.h"
#include "EngineLogs.h"
#include "TextureImportSettings.h"

namespace UE::TextureUtilitiesCommon::Experimental
{

namespace Private
{

	// resize so that the largest dimension is <= MaxSize
	static bool ResizeTexture2D(UTexture* Texture, int32 MaxSize, const ITargetPlatform* TargetPlatform)
	{
		check( Texture->Source.GetNumLayers() == 1 );
		const int32 LayerIndex = 0;

		// We want to reduce the asset size so ignore the imported mip(s) (??)
		const int32 MipIndex = 0;
		FImage SourceMip0;
		if (!Texture->Source.GetMipImage(SourceMip0, MipIndex))
		{
			UE_LOG(LogTexture,Error,TEXT("ResizeTexture2D: Texture GetMipImage failed [%s]"),
				*Texture->GetFullName());
			return false;
		}

		int32 NumSlices = Texture->Source.GetNumSlices(); // == 1 or 6 for cubes

		bool MadeChanges;
		if ( ! Texture->DownsizeImageUsingTextureSettings(TargetPlatform, SourceMip0, MaxSize, LayerIndex, MadeChanges) )
		{
			UE_LOG(LogTexture,Error,TEXT("ResizeTexture2D: Texture DownsizeImageUsingTextureSettings failed [%s]"),
				*Texture->GetFullName());
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
	
	// concatenate all the image payloads into one bulkdata, eg. for mips or blocks
	static UE::Serialization::FEditorBulkData::FSharedBufferWithID MakeSharedBufferForImageDatas(const TArray<FImage> & InImages)
	{
		int64 SizeNeededInBytes = 0;
		for (const FImage& Im : InImages)
		{
			check( Im.RawData.Num() == Im.GetImageSizeBytes() );
			SizeNeededInBytes += Im.RawData.Num(); 
		}
		FUniqueBuffer WriteImageBuffer = FUniqueBuffer::Alloc(SizeNeededInBytes);

		uint8* CurrentAddress = static_cast<uint8*>(WriteImageBuffer.GetData());
		for (const FImage& Im : InImages)
		{
			FMemory::Memcpy(CurrentAddress, Im.RawData.GetData(), Im.RawData.Num());
			CurrentAddress += Im.RawData.Num();
		}

		return WriteImageBuffer.MoveToShared();
	}

	static bool ResizeTexture2DBlocked(UTexture* Texture, int32 TotalMaxSize, const ITargetPlatform* TargetPlatform)
	{
		// does not support layers
		check( Texture->Source.GetNumLayers() == 1 );
		const int32 NumLayers = 1;
		const int32 LayerIndex = 0;

		// MaxSize is applied to the total UDIM size

		FIntPoint LogicalSourceSize = Texture->Source.GetLogicalSize();
		check( LogicalSourceSize.X > TotalMaxSize || LogicalSourceSize.Y > TotalMaxSize );

		double ResizeRatio = double(TotalMaxSize) / FMath::Max(LogicalSourceSize.X,LogicalSourceSize.Y);
		check( ResizeRatio < 1.0 );

		TArray<FTextureSourceBlock> ResizedSourceBlocks;
		ResizedSourceBlocks.Reserve(Texture->Source.GetNumBlocks());
	
		TArray<FImage> ResizedBlocks;
		ResizedBlocks.Reserve(Texture->Source.GetNumBlocks());

		bool MadeAnyChanges = false;

		for (int32 BlockIndex = 0; BlockIndex < Texture->Source.GetNumBlocks(); ++BlockIndex)
		{
			// We want to reduce the asset size so ignore the imported mip(s)
			FImage SourceMip0;
			const int32 MipIndex = 0;
			if (!Texture->Source.GetMipImage(SourceMip0, BlockIndex, LayerIndex, MipIndex))
			{
				UE_LOG(LogTexture,Error,TEXT("ResizeTexture2DBlocked: Texture GetMipImage failed [%s]"),
					*Texture->GetFullName());
				return false;
			}

			int32 NewSizeX = FMath::RoundToInt32( ResizeRatio * SourceMip0.SizeX );
			int32 NewSizeY = FMath::RoundToInt32( ResizeRatio * SourceMip0.SizeY );
			int32 BlockMaxSize = FMath::Max(NewSizeX,NewSizeY);

			bool MadeChanges;
			if ( ! Texture->DownsizeImageUsingTextureSettings(TargetPlatform, SourceMip0, BlockMaxSize, LayerIndex, MadeChanges) )
			{
				// critical error
				UE_LOG(LogTexture,Error,TEXT("ResizeTexture2DBlocked: Texture DownsizeImageUsingTextureSettings failed [%s]"),
					*Texture->GetFullName());
				return false;
			}
			MadeAnyChanges = MadeAnyChanges || MadeChanges;

			FTextureSourceBlock& ResizedSourceBlock = ResizedSourceBlocks.AddDefaulted_GetRef();
			Texture->Source.GetBlock(BlockIndex, ResizedSourceBlock);
		
			FImage& ResizedBlock = ResizedBlocks.AddDefaulted_GetRef();
			ResizedBlock = MoveTemp(SourceMip0);
			
			ResizedSourceBlock.SizeX = ResizedBlock.SizeX;
			ResizedSourceBlock.SizeY = ResizedBlock.SizeY;
			ResizedSourceBlock.NumMips = 1;
		}

		if ( ! MadeAnyChanges )
		{
			return false;
		}
		
		// Protect the code from an async build of the texture
		Texture->PreEditChange(nullptr);

		UE::Serialization::FEditorBulkData::FSharedBufferWithID ResizedImageBufferWithID = MakeSharedBufferForImageDatas(ResizedBlocks);

		const ETextureSourceFormat SourceFormat = Texture->Source.GetFormat();
		Texture->Source.InitBlocked(
			&SourceFormat, // array of formats per layer
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


TEXTUREUTILITIESCOMMON_API bool DownsizeTextureSourceData(UTexture* Texture, int32 TargetSourceSize, const ITargetPlatform* TargetPlatform)
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
		// array, cubearray, volume, not supported
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

	FIntPoint SourceSize = Texture->Source.GetLogicalSize();
	if (SourceSize.X <= TargetSourceSize && SourceSize.Y <= TargetSourceSize)
	{
		return false;
	}

	if (Texture->Source.GetNumBlocks() == 1)
	{
		return Private::ResizeTexture2D(Texture, TargetSourceSize, TargetPlatform);
	}
	else
	{
		// UDIM VT
		return Private::ResizeTexture2DBlocked(Texture, TargetSourceSize, TargetPlatform);
	}
		
	return false;
}

TEXTUREUTILITIESCOMMON_API bool DownsizeTextureSourceDataNearRenderingSize(UTexture* Texture, const ITargetPlatform* TargetPlatform)
{
	if ( ! Texture->Source.IsValid() )
	{
		return false;
	}

	int32 BeforeSizeX;
	int32 BeforeSizeY;
	Texture->GetBuiltTextureSize(TargetPlatform, BeforeSizeX, BeforeSizeY);

	int32 TargetSizeInGame = FMath::Max(BeforeSizeX, BeforeSizeY);
	
	int32 TargetSourceSize = TargetSizeInGame;
	if (Texture->Source.IsLongLatCubemap())
	{
		// The function return the max size of the generated cube from the source long lat
		// this should be kept in sync with the implementation details of ComputeLongLatCubemapExtents() or refactored
		TargetSourceSize = (1U << FMath::FloorLog2(TargetSizeInGame)) * 2;
	}

	if (DownsizeTextureSourceData(Texture, TargetSourceSize, TargetPlatform))
	{
		Texture->LODBias = 0;
		
		// this counts as a reimport :
		UE::TextureUtilitiesCommon::ApplyDefaultsForNewlyImportedTextures(Texture,true);

		Texture->PostEditChange();
		
		// check that GetBuiltTextureSize was preserved :
		int32 AfterSizeX;
		int32 AfterSizeY;
		Texture->GetBuiltTextureSize(TargetPlatform, AfterSizeX, AfterSizeY);

		if ( BeforeSizeX != AfterSizeX ||
			 BeforeSizeY != AfterSizeY )
		{
			UE_LOG(LogTexture,Warning,TEXT("DownsizeTextureSourceDataNearRenderingSize failed to preserve built size; was: %dx%d now: %dx%d on [%s]"),
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


TEXTUREUTILITIESCOMMON_API bool ChangeTextureSourceFormat(UTexture* Texture, ETextureSourceFormat NewFormat)
{
	if ( ! Texture->Source.IsValid() )
	{
		return false;
	}

	// we only support 1 layer currently
	if (Texture->Source.GetNumLayers() != 1)
	{
		return false;
	}
	
	ETextureSourceFormat OldFormat = Texture->Source.GetFormat(0);
	if ( OldFormat == NewFormat )
	{
		return false;
	}

	ERawImageFormat::Type NewRIF = FImageCoreUtils::ConvertToRawImageFormat(NewFormat);
	EGammaSpace NewGamma = ( Texture->SRGB && ERawImageFormat::GetFormatNeedsGammaSpace(NewRIF) ) ? EGammaSpace::sRGB : EGammaSpace::Linear;

	if ( Texture->Source.GetNumBlocks() == 1 && Texture->Source.GetNumMips() == 1 )
	{
		const int32 MipIndex = 0;
		FImage SourceMip;
		if (!Texture->Source.GetMipImage(SourceMip, MipIndex))
		{
			UE_LOG(LogTexture,Error,TEXT("ChangeTextureSourceFormat: Texture GetMipImage failed [%s]"),
				*Texture->GetFullName());
			return false;
		}

		FImage NewMip;
		SourceMip.CopyTo(NewMip,NewRIF,NewGamma);
		
		UE::Serialization::FEditorBulkData::FSharedBufferWithID ResizedImageBufferWithID = MakeSharedBufferFromArray(MoveTemp(NewMip.RawData));
		
		Texture->PreEditChange(nullptr);

		const int32 NumMips = 1;
		Texture->Source.Init(NewMip.SizeX,NewMip.SizeY,NewMip.NumSlices
			, NumMips
			, NewFormat
			, MoveTemp(ResizedImageBufferWithID));
	}
	else // blocks and/or mips
	{
		// all blocks of a UDIM have the same format; Layers do not
		int32 NumLayers = 1;
		int32 LayerIndex = 0;

		int32 NumBlocks = Texture->Source.GetNumBlocks();
		check( NumBlocks >= 1 );

		TArray<FTextureSourceBlock> NewBlocks;
		NewBlocks.Reserve(NumBlocks);
	
		TArray<FImage> NewImages;
		NewImages.Reserve(NumBlocks*16); // *16 for mips

		for (int32 BlockIndex = 0; BlockIndex < NumBlocks; ++BlockIndex)
		{
			FTextureSourceBlock Block;
			Texture->Source.GetBlock(BlockIndex,Block);

			// NewBlocks has sizes, they don't change
			NewBlocks.Add(Block);

			for(int32 MipIndex=0; MipIndex < Block.NumMips;MipIndex++)
			{
				FImage SourceMip;
				if ( ! Texture->Source.GetMipImage(SourceMip, BlockIndex, LayerIndex, MipIndex) )
				{
					UE_LOG(LogTexture,Error,TEXT("ChangeTextureSourceFormat: Texture GetMipImage failed [%s]"),
						*Texture->GetFullName());

					return false;
				}
				
				FImage & NewMip = NewImages.AddDefaulted_GetRef();

				SourceMip.CopyTo(NewMip,NewRIF,NewGamma);
			}
		}

		UE::Serialization::FEditorBulkData::FSharedBufferWithID ResizedImageBufferWithID = Private::MakeSharedBufferForImageDatas(NewImages);
		
		Texture->PreEditChange(nullptr);

		Texture->Source.InitBlocked(
			&NewFormat, // array of formats per layer
			NewBlocks.GetData(),
			NumLayers,
			NewBlocks.Num(),
			MoveTemp(ResizedImageBufferWithID)
		);
	}

	// if gamma was Pow22 it is now sRGB
	Texture->bUseLegacyGamma = false;
	
	// this counts as a reimport :
	UE::TextureUtilitiesCommon::ApplyDefaultsForNewlyImportedTextures(Texture,true);

	Texture->PostEditChange();

	check( Texture->Source.GetGammaSpace(0) == NewGamma );

	return true;
}

} // End namespace UE::TextureUtilitiesCommon::Experimental

#endif //WITH_EDITOR
