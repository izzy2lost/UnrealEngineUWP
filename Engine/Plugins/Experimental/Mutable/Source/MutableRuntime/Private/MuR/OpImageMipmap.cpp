// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/ImagePrivate.h"
#include "MuR/SystemPrivate.h"
#include "Async/ParallelFor.h"
#include "HAL/UnrealMemory.h"

namespace
{

bool bEnableCompressedMipGenerationMemoryOptimizations = true;
static FAutoConsoleVariableRef CVarEnableCompressedMipGenerationMemoryOptimizations (
	TEXT("mutable.EnableCompressedMipGenerationMemoryOptimizations"),
	bEnableCompressedMipGenerationMemoryOptimizations,
	TEXT("If set to true, enables memory optimizations for mip generation on compressed images."),
	ECVF_Default);
}

namespace mu
{


namespace OpImageMipmap_Detail
{

	template<int32 PIXEL_SIZE>
	inline void GenerateMipmapUint8Unfiltered(
		int mips,
		const uint8* pSource, uint8* Dest,
		FIntVector2 sourceSize)
	{
		for (; mips >= 0; --mips)
		{
			FIntVector2 destSize = FIntVector2(FMath::DivideAndRoundUp(sourceSize.X, 2), FMath::DivideAndRoundUp(sourceSize.Y, 2));

			for (int32 y = 0; y < destSize.Y; ++y)
			{
				for (int32 x = 0; x < destSize.X; ++x)
				{
					for (int32 c = 0; c < PIXEL_SIZE; ++c)
					{
						Dest[(y * destSize.X + x) * PIXEL_SIZE + c] =
							pSource[((y << 1) * sourceSize.X + (x << 1)) * PIXEL_SIZE + c];
					}
				}
			}

			sourceSize = destSize;
			pSource = Dest;
			Dest = Dest + destSize.X * destSize.Y * PIXEL_SIZE;
		}
	}

	template<int32 PIXEL_SIZE>
	inline void GenerateMipmapsUint8SimpleAverage(
		int mips,
		const uint8* pSource, uint8* Dest,
		FIntVector2 sourceSize)
	{
		
		const uint8* pMipSource = pSource;
		uint8* pMipDest = Dest;

		FIntVector2 destSize = sourceSize;

		for (int m = 0; m < mips; ++m)
		{
			check(destSize[0] > 1 || destSize[1] > 1);

			sourceSize = destSize;

			int fullColumns = destSize[0] / 2;
			bool strayColumn = (destSize[0] % 2) != 0;
			int fullRows = destSize[1] / 2;
			bool strayRow = (destSize[1] % 2) != 0;

			destSize[0] = FMath::DivideAndRoundUp(destSize[0], 2);
			destSize[1] = FMath::DivideAndRoundUp(destSize[1], 2);

			int sourceStride = sourceSize[0] * PIXEL_SIZE;
			int destStride = destSize[0] * PIXEL_SIZE;

			const auto ProcessRow = [
				pMipDest, pMipSource, fullColumns, strayColumn, sourceStride, destStride
			] (uint32 y)
			{
				const uint8* pSourceRow0 = pMipSource + 2 * y * sourceStride;
				const uint8* pSourceRow1 = pSourceRow0 + sourceStride;
				uint8* pDestRow = pMipDest + y * destStride;

				for (int x = 0; x < fullColumns; ++x)
				{
					if constexpr (PIXEL_SIZE == 4)
					{
						//const uint64 Row0Bits = *reinterpret_cast<const uint64*>(pSourceRow0);
						//const uint64 Row1Bits = *reinterpret_cast<const uint64*>(pSourceRow1);

						// Use memcpy to avoid any possible but improbable UB. memcpy should be optimized away by the compiler.
						uint64 Row0Bits; 
						uint64 Row1Bits;

						FMemory::Memcpy(&Row0Bits, pSourceRow0, sizeof(uint64));
						FMemory::Memcpy(&Row1Bits, pSourceRow1, sizeof(uint64));
						
						const uint64 XorRow0Row1Bits = Row0Bits ^ Row1Bits;

						// Average of 2 unsigned integers without overflow extended to work on multiple bytes.
						constexpr uint64 ShiftMask = 0xFEFEFEFEFEFEFEFE;
						const uint64 ErrorCorrection = XorRow0Row1Bits & 0x0101010101010101;
						const uint64 AvgLowBits = (Row0Bits & Row1Bits) + ((XorRow0Row1Bits & ShiftMask) >> 1) + ErrorCorrection;
						const uint64 AvgHighBits = AvgLowBits >> 32;
						const uint32 Result = (AvgLowBits & AvgHighBits) + (((AvgLowBits ^ AvgHighBits) & ShiftMask) >> 1);
					
						//*reinterpret_cast<uint32*>(pDestRow) = Result;
						FMemory::Memcpy(pDestRow, &Result, sizeof(uint32));
					}
					else
					{
						for (int32 C = 0; C < PIXEL_SIZE; ++C)
						{
							int32 PixelSum = pSourceRow0[C] + pSourceRow0[PIXEL_SIZE + C] + pSourceRow1[C] + pSourceRow1[PIXEL_SIZE + C];
							pDestRow[C] = (uint8)(PixelSum >> 2);
						}
					}

					pSourceRow0 += 2 * PIXEL_SIZE;
					pSourceRow1 += 2 * PIXEL_SIZE;
					pDestRow += PIXEL_SIZE;
				}

				if (strayColumn)
				{
					if constexpr (PIXEL_SIZE == 4)
					{	
						//const uint32 Row0Bits = *reinterpret_cast<const uint32*>(pSourceRow0);
						//const uint32 Row1Bits = *reinterpret_cast<const uint32*>(pSourceRow1);

						uint32 Row0Bits; 
						uint32 Row1Bits;

						FMemory::Memcpy(&Row0Bits, pSourceRow0, sizeof(uint32));
						FMemory::Memcpy(&Row1Bits, pSourceRow1, sizeof(uint32));

						// Average of 2 unsigned integers without overflow extended to work on multiple bytes.
						constexpr uint32 ShiftMask = 0xFEFEFEFE;
						const uint32 Result = (Row0Bits & Row1Bits) + (((Row0Bits ^ Row1Bits) & ShiftMask) >> 1);

						//*reinterpret_cast<uint32*>(pDestRow) = Result;
						FMemory::Memcpy(pDestRow, &Result, sizeof(uint32));
					}
					else
					{
						for (int32 C = 0; C < PIXEL_SIZE; ++C)
						{
							int32 PixelSum = pSourceRow0[C] + pSourceRow1[C];
							pDestRow[C] = (uint8)(PixelSum >> 1);
						}
					}
				}
			};

			constexpr int PixelConcurrencyThreshold = 0xffff;
			if (destSize[0] * destSize[1] < PixelConcurrencyThreshold)
			{
				for (int y = 0; y < fullRows; ++y)
				{
					ProcessRow(y);
				}
			}
			else
			{
				ParallelFor(fullRows, ProcessRow);
			}

			if (strayRow)
			{
				const uint8* pSourceRow0 = pMipSource + 2 * fullRows * sourceStride;
				const uint8* pSourceRow1 = pSourceRow0 + sourceStride;
				uint8* pDestRow = pMipDest + fullRows * destStride;

				for (int x = 0; x < fullColumns; ++x)
				{
					if constexpr (PIXEL_SIZE == 4)
					{
						//const uint32 Col0Bits = *reinterpret_cast<const uint32*>(pSourceRow0);
						//const uint32 Col1Bits = *reinterpret_cast<const uint32*>(pSourceRow0 + 4);

						uint32 Col0Bits; 
						uint32 Col1Bits;

						FMemory::Memcpy(&Col0Bits, pSourceRow0, sizeof(uint32));
						FMemory::Memcpy(&Col1Bits, pSourceRow0 + 4, sizeof(uint32));

						// Average of 2 unsigned integers without overflow extended to work on multiple bytes. 
						// In this case we use the ceil variant to be consistent with the method used for 4 pixel average. 
						constexpr uint32 ShiftMask = 0xFEFEFEFE;
						const uint32 Result = (Col0Bits & Col1Bits) + (((Col0Bits ^ Col1Bits) & ShiftMask) >> 1);

						//*reinterpret_cast<uint32*>(pDestRow) = Result;
						FMemory::Memcpy(pDestRow, &Result, sizeof(uint32));
					}
					else
					{
						for (int32 C = 0; C < PIXEL_SIZE; ++C)
						{
							int32 p = pSourceRow0[C] + pSourceRow0[PIXEL_SIZE + C];
							pDestRow[C] = (uint8)(p >> 1);
						}
					}

					pSourceRow0 += 2 * PIXEL_SIZE;
					pDestRow += PIXEL_SIZE;
				}

				if (strayColumn)
				{
					if constexpr (PIXEL_SIZE == 4)
					{
						//*reinterpret_cast<uint32*>(pDestRow) = *reinterpret_cast<const uint32*>(pSourceRow0);
						FMemory::Memcpy(pDestRow, pSourceRow0, 4);
					}
					else
					{
						for (int32 C = 0; C < PIXEL_SIZE; ++C)
						{
							pDestRow[C] = pSourceRow0[C];
						}
					}
				}
			}

			// Reset the source pointer for the next mip, to use the dest that we have just
			// generated.
			pMipSource = pMipDest;
			pMipDest += destSize[0] * destSize[1] * PIXEL_SIZE;
		}
	}

	// Generate next mip decompressed form block compressed image. 
	template<int32 NumChannels, EMipmapFilterType Filter>
	inline void GenerateNextMipBlockCompressed(
		const uint8* Src, uint8* Dest, FIntVector2 SrcSize, EImageFormat SrcFormat, EImageFormat DestFormat)
	{
		MUTABLE_CPUPROFILER_SCOPE(GenerateNextMipBlockCompressed);

		const FImageFormatData& DestFormatData = GetImageFormatData(DestFormat);
		const FImageFormatData& SrcFormatData = GetImageFormatData(SrcFormat);

		check(NumChannels == DestFormatData.Channels);
		check(DestFormatData.PixelsPerBlockX == 1 && DestFormatData.PixelsPerBlockY == 1);

		const int32 DestChannelCount = DestFormatData.Channels; 
		const FIntVector2 PixelsPerBlock = FIntVector2(SrcFormatData.PixelsPerBlockX, SrcFormatData.PixelsPerBlockY);
		const int32 BlockSizeInBytes = SrcFormatData.BytesPerBlock;

		const FIntVector2 DestSize = FIntVector2(
				FMath::DivideAndRoundUp(SrcSize.X, 2),
				FMath::DivideAndRoundUp(SrcSize.Y, 2));

		const FIntVector2 NumBlocks = FIntVector2(
				FMath::DivideAndRoundUp(SrcSize.X, PixelsPerBlock.X),
				FMath::DivideAndRoundUp(SrcSize.Y, PixelsPerBlock.Y));

		constexpr int32 BatchSizeInBlocksX = 1 << 5;
		constexpr int32 BatchSizeInBlocksY = 1 << 4;

		FIntVector2 NumBatches = FIntVector2(
				FMath::DivideAndRoundUp(NumBlocks.X, BatchSizeInBlocksX),
				FMath::DivideAndRoundUp(NumBlocks.Y, BatchSizeInBlocksY));


		// Limit the parallel job num based on actual num workers. Here we cannot rely on ParallelFor
		// balancing the load as we need to allocate memory for every job. Make sure there is always 1 job.
		// TODO: Consider balancing work on using a 2D grid.
		const int32 MaxParallelJobs = FMath::Max(1, FMath::Min(int32(LowLevelTasks::FScheduler::Get().GetNumWorkers()), 8));
		
		constexpr int32 MinRowBatchesPerJob = 1;

		const int32 NumRowBatchesPerJob = 
			 FMath::Min(NumBatches.Y, FMath::Max(MinRowBatchesPerJob, FMath::DivideAndRoundUp(NumBatches.Y, MaxParallelJobs)));

		const int32 NumParallelJobs = FMath::DivideAndRoundUp(NumBatches.Y, NumRowBatchesPerJob); 

		// Use the tracking allocator policy on the image counter, this will not count for preventing memory peaks 
		// but will show if it happens. This allocation should be small enough so it is not a problem to get over-budget
		// by this amount. 
		TArray<uint8, FDefaultMemoryTrackingAllocator<MemoryCounters::FImageMemoryCounter>> StagingMemory;

		const miro::FImageSize StagingSize = miro::FImageSize(
				uint16(BatchSizeInBlocksX*PixelsPerBlock.X), 
				uint16(BatchSizeInBlocksY*PixelsPerBlock.Y));
		
		// Allocate extra memory so the mip computation can work on all possible pixels sizes. 
		// Also add some extra padding so different threads do not share cache lines.
		const int32 PerJobStagingBytes = StagingSize.X*StagingSize.Y*NumChannels + 8 + 64;
		
		StagingMemory.SetNum(PerJobStagingBytes*NumParallelJobs);
		uint8 * const StagingMemoryData = StagingMemory.GetData();

		miro::SubImageDecompression::FuncRefType DecompressionFunc = SelectDecompressionFunction(DestFormat, SrcFormat);

		auto ProcessJob = 
			[
				NumParallelJobs, NumRowBatchesPerJob, 
				StagingMemoryData, PerJobStagingBytes, 
				NumBatches, NumBlocks, PixelsPerBlock, BlockSizeInBytes, DecompressionFunc,
				Src, SrcSize, Dest, DestSize
			](int32 JobId)
		{
			const int32 JobRowBegin = JobId*NumRowBatchesPerJob;
			const int32 JobRowEnd   = FMath::Min(JobRowBegin + NumRowBatchesPerJob, NumBatches.Y);
			uint8 * const JobStagingMemoryData = StagingMemoryData + JobId*PerJobStagingBytes;

			for (int32 BatchY = JobRowBegin; BatchY < JobRowEnd; ++BatchY)
			{
				for (int32 BatchX = 0; BatchX < NumBatches.X; ++BatchX)
				{
					const FIntVector2 BatchBeginInBlocks = FIntVector2(BatchX*BatchSizeInBlocksX, BatchY*BatchSizeInBlocksY);
					const FIntVector2 BatchEndInBlocks = FIntVector2(
							FMath::Min(BatchBeginInBlocks.X + BatchSizeInBlocksX, NumBlocks.X),
							FMath::Min(BatchBeginInBlocks.Y + BatchSizeInBlocksY, NumBlocks.Y));

					const uint8* const SrcBatchData = Src + (BatchBeginInBlocks.Y * NumBlocks.X + BatchBeginInBlocks.X)*BlockSizeInBytes;

					// Assume the decompressed size is always multiple of the block size. Trim unused bytes when copying to 
					// the final destination.
					const FIntVector2 BatchDecSizeInPixels = FIntVector2(
							(BatchEndInBlocks.X - BatchBeginInBlocks.X)*PixelsPerBlock.X,
							(BatchEndInBlocks.Y - BatchBeginInBlocks.Y)*PixelsPerBlock.Y);

					const miro::FImageSize FromSize = miro::FImageSize(uint16(SrcSize.X), uint16(SrcSize.Y)); 
					const miro::FImageSize SubSize  = miro::FImageSize(uint16(BatchDecSizeInPixels.X), uint16(BatchDecSizeInPixels.Y));
					DecompressionFunc(FromSize, SubSize, SubSize, SrcBatchData, JobStagingMemoryData);

					const FIntVector2 BatchOutBeginInPixels = FIntVector2(
							(BatchBeginInBlocks.X*PixelsPerBlock.X) >> 1, 
							(BatchBeginInBlocks.Y*PixelsPerBlock.Y) >> 1);

					const FIntVector2 BatchOutEndInPixels = FIntVector2(
							FMath::Min(BatchOutBeginInPixels.X + ((BatchSizeInBlocksX*PixelsPerBlock.X) >> 1), DestSize.X), 
							FMath::Min(BatchOutBeginInPixels.Y + ((BatchSizeInBlocksY*PixelsPerBlock.Y) >> 1), DestSize.Y));
		
					// Generate partial next mip to dest.
					// This works for all pixel sizes because we have preallocated more memory than needed.
					for (int32 Y = BatchOutBeginInPixels.Y; Y < BatchOutEndInPixels.Y; ++Y)
					{
						for (int32 X = BatchOutBeginInPixels.X; X < BatchOutEndInPixels.X; ++X)
						{
							uint8* const DestPixel = Dest + (Y*DestSize.X + X) * NumChannels;

							const FIntVector2 Row0Offset = FIntVector2(
									(X - BatchOutBeginInPixels.X) << 1, (Y - BatchOutBeginInPixels.Y) << 1);

							uint8 const * const SrcRow0 = JobStagingMemoryData + (Row0Offset.Y*BatchDecSizeInPixels.X + Row0Offset.X) * NumChannels;

							if constexpr (Filter == EMipmapFilterType::MFT_SimpleAverage)
							{
								// Use memcpy to avoid any possible but improbable UB. memcpy should be optimized away by the compiler.
								uint64 Row0Bits; 
								FMemory::Memcpy(&Row0Bits, SrcRow0, sizeof(uint64));
								
								uint8 const * const SrcRow1 = JobStagingMemoryData + 
										(FMath::Min(Row0Offset.Y + 1, BatchDecSizeInPixels.Y - 1)*BatchDecSizeInPixels.X + Row0Offset.X) * NumChannels;

								uint64 Row1Bits;
								FMemory::Memcpy(&Row1Bits, SrcRow1, sizeof(uint64));
								
								const bool bOutOfBounds = Row0Offset.X + 1 >= BatchDecSizeInPixels.X;
								
								constexpr uint64 ShiftMask = 0xFEFEFEFEFEFEFEFE;
								
								const uint64 XorRow0Row1Bits = Row0Bits ^ Row1Bits;
								const uint64 ErrorCorrection = XorRow0Row1Bits & 0x0101010101010101;
								
								// Average of 2 unsigned integers without overflow extended to work on multiple bytes.
								const uint64 AvgLowBits = (Row0Bits & Row1Bits) + ((XorRow0Row1Bits & ShiftMask) >> 1) + ErrorCorrection;
								const uint64 AvgHighBits = bOutOfBounds ? AvgLowBits : (AvgLowBits >> NumChannels*8);
								const uint32 Result = (AvgLowBits & AvgHighBits) + (((AvgLowBits ^ AvgHighBits) & ShiftMask) >> 1);
								
								FMemory::Memcpy(DestPixel, &Result, NumChannels);
							}
							else // constexpr Filter == EMipmapFilterType::MFT_Unfiltered
							{
								FMemory::Memcpy(DestPixel, SrcRow0, NumChannels);	
							}
							static_assert(
								Filter == EMipmapFilterType::MFT_SimpleAverage || 
								Filter == EMipmapFilterType::MFT_Unfiltered);
						}
					}
				}
			}
		};

		if (NumParallelJobs == 1)
		{
			ProcessJob(0);
		}
		else if (NumParallelJobs > 1)
		{	
			ParallelFor(NumParallelJobs, ProcessJob);
		}
	}
} // namespace OpImageMipmap_Detail


	/** Generate the mipmaps for byte-based images of whatever number of channels.
	* \param mips number of additional levels to build from the source.
	*/
	template<int32 PIXEL_SIZE>
	inline void GenerateMipmapsUint8(int32 mips,
		const uint8* pSource, uint8* Dest,
		FIntVector2 sourceSize,
		const FMipmapGenerationSettings& settings)
	{
		using namespace OpImageMipmap_Detail;

		switch (settings.m_filterType)
		{
		case EMipmapFilterType::MFT_SimpleAverage:
		{
			GenerateMipmapsUint8SimpleAverage<PIXEL_SIZE>(mips, pSource, Dest, sourceSize);
			break;
		}
		case EMipmapFilterType::MFT_Unfiltered:
		{
			GenerateMipmapUint8Unfiltered<PIXEL_SIZE>(mips, pSource, Dest, sourceSize);
			break;
		}
		default:
		{
			check(false);
			break;
		}
		}
	}

	/** Generate the mipmaps for Block Comporessed images of whatever number of channels.
	 *  The result is a non compressed image of the next mip with its tail.
	* \param mips number of additional levels to build from the source.
	*/
	template<int32 PixelSize>
	inline void GenerateMipmapsBlockCompressed(int32 Mips,
		const uint8* SourceData, uint8* DestData,
		FIntVector2 SourceSize, EImageFormat SrcFormat, EImageFormat DestFormat,
		const FMipmapGenerationSettings& Settings)
	{
		using namespace OpImageMipmap_Detail;

		switch (Settings.m_filterType)
		{
		case EMipmapFilterType::MFT_SimpleAverage:
		{
			GenerateNextMipBlockCompressed<PixelSize, EMipmapFilterType::MFT_SimpleAverage>(
				SourceData, DestData, SourceSize, SrcFormat, DestFormat);

			const FIntVector2 CurrentMipSize = FIntVector2(
				FMath::DivideAndRoundUp(SourceSize[0], 2),
				FMath::DivideAndRoundUp(SourceSize[1], 2));

			if (CurrentMipSize.X > 1 || CurrentMipSize.Y > 1)
			{
				uint8* CurrentMipData = DestData;
				uint8* NextMipData = CurrentMipData + CurrentMipSize.X*CurrentMipSize.Y*PixelSize;
				GenerateMipmapsUint8SimpleAverage<PixelSize>(Mips - 1, CurrentMipData, NextMipData, CurrentMipSize);
			}

			break;
		}
		case EMipmapFilterType::MFT_Unfiltered:
		{
			GenerateNextMipBlockCompressed<PixelSize, EMipmapFilterType::MFT_Unfiltered>(
					SourceData, DestData, SourceSize, SrcFormat, DestFormat);

			const FIntVector2 CurrentMipSize = FIntVector2(
					FMath::DivideAndRoundUp(SourceSize[0], 2), 
					FMath::DivideAndRoundUp(SourceSize[1], 2));

			if (CurrentMipSize.X > 1 || CurrentMipSize.Y > 1)
			{
				uint8* CurrentMipData = DestData;
				uint8* NextMipData = CurrentMipData + CurrentMipSize.Y*CurrentMipSize.X*PixelSize;
				GenerateMipmapUint8Unfiltered<PixelSize>(Mips + 1, CurrentMipData, NextMipData, CurrentMipSize);
			}
			break;
		}
		default:
		{
			check(false);
			break;
		}
		}
	}


    //---------------------------------------------------------------------------------------------
    void FImageOperator::ImageMipmap_PrepareScratch(Image* Dest, const Image* Base, int32 LevelCount, FScratchImageMipmap& Scratch )
    {
        int StartLevel = Base->GetLODCount() - 1;

        check(Dest->GetLODCount() == LevelCount);
        check(Dest->GetSizeX() == Base->GetSizeX());
        check(Dest->GetSizeY() == Base->GetSizeY());
        check(Dest->GetFormat() == Base->GetFormat());

		EImageFormat BaseFormat = Base->GetFormat();
		if (mu::IsCompressedFormat(BaseFormat))
		{
			// Is it a block format?
			if (mu::GetImageFormatData(BaseFormat).PixelsPerBlockX > 1)
			{
				if (!bEnableCompressedMipGenerationMemoryOptimizations)
				{
					// Uncompress the last mip that we already have
					FIntVector2 UncompressedSize = Base->CalculateMipSize(StartLevel);
					Scratch.Uncompressed = CreateImage(
						(uint16)UncompressedSize[0], (uint16)UncompressedSize[1],
						1,
						EImageFormat::IF_RGBA_UBYTE, EInitializationType::NotInitialized);
				}

				FIntVector2 UncompressedMipsSize = Base->CalculateMipSize(StartLevel + 1);
				// Generate the mipmaps from there on
				Scratch.UncompressedMips = CreateImage(
					(uint16)UncompressedMipsSize[0], (uint16)UncompressedMipsSize[1],
					FMath::Max(1, LevelCount - StartLevel - 1),
					EImageFormat::IF_RGBA_UBYTE, EInitializationType::NotInitialized);

				// Compress the mipmapped image
				Scratch.CompressedMips = CreateImage(
					(uint16)UncompressedMipsSize[0], (uint16)UncompressedMipsSize[1],
					Scratch.UncompressedMips->GetLODCount(),
					Base->GetFormat(), EInitializationType::NotInitialized);
			}
			else
			{
				// It's probably an RLE compressed format

				// Uncompress the last mip that we already have
				FIntVector2 UncompressedSize = Base->CalculateMipSize(StartLevel);
				Scratch.Uncompressed = CreateImage(
					(uint16)UncompressedSize[0], (uint16)UncompressedSize[1],
					1,
					EImageFormat::IF_L_UBYTE, EInitializationType::NotInitialized);


				FIntVector2 UncompressedMipsSize = Base->CalculateMipSize(StartLevel + 1);
				// Generate the mipmaps from there on
				Scratch.UncompressedMips = CreateImage(
					(uint16)UncompressedMipsSize[0], (uint16)UncompressedMipsSize[1],
					FMath::Max(1, LevelCount - StartLevel - 1),
					EImageFormat::IF_L_UBYTE, EInitializationType::NotInitialized);


				// Compress the mipmapped image
				Scratch.CompressedMips = CreateImage(
					(uint16)UncompressedMipsSize[0], (uint16)UncompressedMipsSize[1],
					Scratch.UncompressedMips->GetLODCount(),
					Base->GetFormat(), EInitializationType::NotInitialized);

				// Preallocate ample memory for the compressed data
				uint32 TotalMemory = Scratch.UncompressedMips->GetDataSize();
				Scratch.CompressedMips->m_data.SetNumUninitialized(TotalMemory);

				// Preallocate ample memory for the destination data
				TotalMemory = Base->GetDataSize() + Scratch.UncompressedMips->GetDataSize();
				Dest->m_data.SetNumUninitialized(TotalMemory);
			}
		}
    }


	void FImageOperator::ImageMipmap_ReleaseScratch(FScratchImageMipmap& Scratch)
	{
		ReleaseImage(Scratch.Uncompressed);
		ReleaseImage(Scratch.UncompressedMips);
		ReleaseImage(Scratch.CompressedMips);
	}


	void FImageOperator::ImageMipmap(FScratchImageMipmap& Scratch, int32 CompressionQuality, Image* Dest, const Image* Base,
		int32 LevelCount,
		const FMipmapGenerationSettings& Settings, bool bGenerateOnlyTail)
	{
		int32 StartLevel = Base->GetLODCount() - 1;

		check(!(Base->m_flags & Image::IF_CANNOT_BE_SCALED));


		if (!bGenerateOnlyTail)
		{
			check(Dest->GetLODCount() == LevelCount);
			check(Dest->GetSizeX() == Base->GetSizeX());
			check(Dest->GetSizeY() == Base->GetSizeY());
		}
		else
		{
			check(Dest->GetLODCount() + Base->GetLODCount() == LevelCount);

			check(
				[&]() -> bool
				{
					const FIntVector2 BaseImageNextMipSize = Base->CalculateMipSize(StartLevel + 1);
					return BaseImageNextMipSize.X == Dest->GetSizeX() && BaseImageNextMipSize.Y == Dest->GetSizeY();
				}());
		}

		check(Dest->GetFormat() == Base->GetFormat());

		// Calculate the data size, since the base could have more data allocated.
		int32 BaseMipDataSize = 0;
		if (!bGenerateOnlyTail)
		{
			BaseMipDataSize = Base->GetMipsDataSize();
			check(Dest->GetDataSize() >= BaseMipDataSize);
			FMemory::Memcpy(Dest->GetData(), Base->GetData(), BaseMipDataSize);
		}

		int mipsToBuild = LevelCount - StartLevel - 1;
		if (!mipsToBuild)
		{
			return;
		}

		const uint8* pSourceBuf = !bGenerateOnlyTail ? Dest->GetMipData(StartLevel) : Base->GetMipData(StartLevel);

		uint8* pDestBuf = !bGenerateOnlyTail ? Dest->GetMipData(StartLevel + 1) : Dest->GetMipData(0);

		FIntVector2 SourceSize = Base->CalculateMipSize(StartLevel);

		EImageFormat BaseFormat = Base->GetFormat();
		const bool bIsBlockCompressedFormat = mu::IsBlockCompressedFormat(BaseFormat);
		const bool bIsCompressedFormat = mu::IsCompressedFormat(BaseFormat);
		
		check(!bIsBlockCompressedFormat || bIsCompressedFormat);

		if (bIsBlockCompressedFormat && bEnableCompressedMipGenerationMemoryOptimizations)
		{
			const EImageFormat DestFormat = Scratch.UncompressedMips->GetFormat();
			uint8* DestData = Scratch.UncompressedMips->GetData();
			switch (DestFormat)
			{
			case EImageFormat::IF_L_UBYTE:
			{
				constexpr int32 PixelSize = 1;
				GenerateMipmapsBlockCompressed<PixelSize>(
						LevelCount - StartLevel - 1, pSourceBuf, DestData, SourceSize, BaseFormat, DestFormat, Settings);
				break;
			}
			case EImageFormat::IF_RGB_UBYTE:
			{
				constexpr int32 PixelSize = 3;
				GenerateMipmapsBlockCompressed<PixelSize>(
						LevelCount - StartLevel - 1, pSourceBuf, DestData, SourceSize, BaseFormat, DestFormat, Settings);
				break;
			}
			case EImageFormat::IF_RGBA_UBYTE:
			{
				constexpr int32 PixelSize = 4;
				GenerateMipmapsBlockCompressed<PixelSize>(
						LevelCount - StartLevel - 1, pSourceBuf, DestData, SourceSize, BaseFormat, DestFormat, Settings);
				break;
			}
			default: check(false);
			}

			bool bSuccess = false;
			constexpr int32 OnlyLod = -1;
			ImagePixelFormat(bSuccess, CompressionQuality, Scratch.CompressedMips.get(), Scratch.UncompressedMips.get(), OnlyLod); 
			check(bSuccess);
			
			FMemory::Memcpy(pDestBuf, Scratch.CompressedMips->GetData(), Scratch.CompressedMips->GetDataSize());
		}
		else if (bIsCompressedFormat)
		{
			// Bad case.
			// Uncompress the last mip that we already have
			bool bSuccess = false;
			ImagePixelFormat(bSuccess, CompressionQuality, Scratch.Uncompressed.get(), Base, StartLevel);
			check(bSuccess);

			// Generate the mipmaps from there on

			constexpr bool bGenerateOnlyTailForCompressed = true;
			ImageMipmap(Scratch, CompressionQuality, Scratch.UncompressedMips.get(),
				Scratch.Uncompressed.get(), LevelCount - StartLevel, Settings, bGenerateOnlyTailForCompressed);

			// Compress the mipmapped image
			bSuccess = false;
			ImagePixelFormat(bSuccess, CompressionQuality, Scratch.CompressedMips.get(), Scratch.UncompressedMips.get());
			int32 ExcessDataSize = FMath::Max(2, Scratch.CompressedMips->GetDataSize());
			while (!bSuccess)
			{
				// Bad case: this should almost never happen.
				MUTABLE_CPUPROFILER_SCOPE(Mipmap_Recompression_OutOfSpace);

				Scratch.CompressedMips->m_data.SetNumUninitialized(ExcessDataSize);
				bSuccess = false;
				ImagePixelFormat(bSuccess, CompressionQuality, Scratch.CompressedMips.get(), Scratch.UncompressedMips.get());
				ExcessDataSize *= 4;
			}

			check(!bGenerateOnlyTail || BaseMipDataSize == 0);
			const int32 FinalDestSize = BaseMipDataSize + Scratch.CompressedMips->GetDataSize();

			if (FinalDestSize > Dest->GetDataSize())
			{
				// Bad case: this should almost never happen.
				Dest->m_data.SetNumUninitialized(FinalDestSize);
				pDestBuf = !bGenerateOnlyTail ? Dest->GetMipData(StartLevel + 1) : Dest->GetMipData(0);
			}

			FMemory::Memcpy(pDestBuf, Scratch.CompressedMips->GetData(), Scratch.CompressedMips->GetDataSize());
		}
		else
		{
			switch (Base->GetFormat())
			{
			case EImageFormat::IF_L_UBYTE:
				GenerateMipmapsUint8<1>(LevelCount - StartLevel - 1, pSourceBuf, pDestBuf, SourceSize, Settings);
				break;

			case EImageFormat::IF_RGB_UBYTE:
				GenerateMipmapsUint8<3>(LevelCount - StartLevel - 1, pSourceBuf, pDestBuf, SourceSize, Settings);
				break;

			case EImageFormat::IF_BGRA_UBYTE:
			case EImageFormat::IF_RGBA_UBYTE:
				GenerateMipmapsUint8<4>(LevelCount - StartLevel - 1, pSourceBuf, pDestBuf, SourceSize, Settings);
				break;

			default:
				checkf(false, TEXT("Format not implemented in mipmap generation."));
			}
		}
	}

	void FImageOperator::ImageMipmap(int32 CompressionQuality, Image* Dest, const Image* Base,
		int32 LevelCount,
		const FMipmapGenerationSettings& Settings, bool bGenerateOnlyTail)
	{
		FScratchImageMipmap Scratch;
		ImageMipmap_PrepareScratch(Dest, Base, LevelCount, Scratch);

		ImageMipmap(Scratch, CompressionQuality, Dest, Base, LevelCount, Settings, bGenerateOnlyTail);

		ImageMipmap_ReleaseScratch(Scratch);
	}


	/** Update all the mipmaps in the image from the data in the base one. 
	* Only the mipmaps already existing in the image are updated.
	*/
	void ImageMipmapInPlace(int32 InImageCompressionQuality, Image* InBase, const FMipmapGenerationSettings& InSettings)
	{
		int32 StartLevel = 0;
		int32 LevelCount = InBase->GetLODCount();

		check(!(InBase->m_flags & Image::IF_CANNOT_BE_SCALED));

		int32 MipsToBuild = InBase->GetLODCount()-1;
		if (!MipsToBuild)
		{
			return;
		}

		const uint8* pSourceBuf = InBase->GetMipData(StartLevel);
		uint8* pDestBuf = InBase->GetMipData(StartLevel + 1);

		FIntVector2 sourceSize = InBase->CalculateMipSize(StartLevel);

		switch (InBase->GetFormat())
		{
		case EImageFormat::IF_L_UBYTE:
			GenerateMipmapsUint8<1>(MipsToBuild, pSourceBuf, pDestBuf, sourceSize, InSettings);
			break;

		case EImageFormat::IF_RGB_UBYTE:
			GenerateMipmapsUint8<3>(MipsToBuild, pSourceBuf, pDestBuf, sourceSize, InSettings);
			break;

		case EImageFormat::IF_BGRA_UBYTE:
		case EImageFormat::IF_RGBA_UBYTE:
			GenerateMipmapsUint8<4>(MipsToBuild, pSourceBuf, pDestBuf, sourceSize, InSettings);
			break;

		default:
			checkf(false, TEXT("Format not implemented in mipmap generation."));
		}
	}

}
