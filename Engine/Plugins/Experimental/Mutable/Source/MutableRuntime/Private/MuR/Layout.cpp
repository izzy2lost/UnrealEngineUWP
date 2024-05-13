// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuR/Layout.h"

#include "HAL/LowLevelMemTracker.h"
#include "Math/IntPoint.h"
#include "MuR/MutableMath.h"
#include "MuR/SerialisationPrivate.h"


namespace mu {

	//---------------------------------------------------------------------------------------------
	Layout::Layout()
	{
	}


	//---------------------------------------------------------------------------------------------
	void Layout::Serialise( const Layout* p, OutputArchive& arch )
	{
		arch << *p;
	}


	//---------------------------------------------------------------------------------------------
	LayoutPtr Layout::StaticUnserialise( InputArchive& arch )
	{
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
		LayoutPtr pResult = new Layout();
		arch >> *pResult;
		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	LayoutPtr Layout::Clone() const
	{
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
		LayoutPtr pResult = new Layout();		
		pResult->Size = Size;
		pResult->MaxSize = MaxSize;
		pResult->Blocks = Blocks;
		pResult->Strategy = Strategy;
		pResult->FirstLODToIgnoreWarnings = FirstLODToIgnoreWarnings;
		pResult->ReductionMethod = ReductionMethod;
		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	bool Layout::operator==( const Layout& o ) const
	{
		return (Size == o.Size) &&
			(MaxSize == o.MaxSize) &&
			(Blocks == o.Blocks) &&
			(Strategy == o.Strategy) &&
			// maybe this is not needed
			(FirstLODToIgnoreWarnings == o.FirstLODToIgnoreWarnings) &&
			(ReductionMethod==o.ReductionMethod);
	}


	//---------------------------------------------------------------------------------------------
	int32 Layout::GetDataSize() const
	{
		return sizeof(Layout) + Blocks.GetAllocatedSize();
	}


	//---------------------------------------------------------------------------------------------
	FIntPoint Layout::GetGridSize() const
	{
		return FIntPoint(Size[0], Size[1]);
	}


	//---------------------------------------------------------------------------------------------
	void Layout::SetGridSize( int32 sizeX, int32 sizeY )
	{
		check( sizeX>=0 && sizeY>=0 );
		Size[0] = (uint16)sizeX;
		Size[1] = (uint16)sizeY;
	}


	//---------------------------------------------------------------------------------------------
	void Layout::GetMaxGridSize(int32* SizeX, int32* SizeY) const
	{
		check(SizeX && SizeY);

		if (SizeX && SizeY)
		{
			*SizeX = MaxSize[0];
			*SizeY = MaxSize[1];
		}
	}


	//---------------------------------------------------------------------------------------------
	void Layout::SetMaxGridSize(int32 sizeX, int32 sizeY)
	{
		check(sizeX >= 0 && sizeY >= 0);
		MaxSize[0] = (uint16)sizeX;
		MaxSize[1] = (uint16)sizeY;
	}


	//---------------------------------------------------------------------------------------------
	int32 Layout::GetBlockCount() const
	{
		return Blocks.Num();
	}


	//---------------------------------------------------------------------------------------------
	void Layout::SetBlockCount( int32 n )
	{
		check( n>=0 );
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
		Blocks.SetNum( n );
	}


	//---------------------------------------------------------------------------------------------
	void Layout::GetBlock( int32 index, uint16* pMinX, uint16* pMinY, uint16* pSizeX, uint16* pSizeY ) const
	{
		check( index >=0 && index < Blocks.Num() );
		check( pMinX && pMinY && pSizeX && pSizeY );

		if (pMinX && pMinY && pSizeX && pSizeY)
		{
			*pMinX = Blocks[index].Min[0];
			*pMinY = Blocks[index].Min[1];
			*pSizeX = Blocks[index].Size[0];
			*pSizeY = Blocks[index].Size[1];
		}
	}


	//---------------------------------------------------------------------------------------------
	void Layout::GetBlockOptions(int index, int& pPriority, bool& bReduceBothAxes, bool& bReduceByTwo) const
	{
		check(index >= 0 && index < Blocks.Num());

		pPriority = Blocks[index].Priority;
		bReduceBothAxes = Blocks[index].bReduceBothAxes;
		bReduceByTwo = Blocks[index].bReduceByTwo;
	}


	//---------------------------------------------------------------------------------------------
    void Layout::SetBlock( int index, int minx, int miny, int sizex, int sizey )
	{
		check( index >=0 && index < Blocks.Num() );

		// Keeps the id
		Blocks[index].Min = UE::Math::TIntVector2<uint16>((uint16)minx, (uint16)miny);
		Blocks[index].Size = UE::Math::TIntVector2<uint16>((uint16)sizex, (uint16)sizey);
	}


	//---------------------------------------------------------------------------------------------
	void Layout::SetBlockOptions(int index, int priority, bool bReduceBothAxes, bool bReduceByTwo)
	{
		check(index >= 0 && index < Blocks.Num());

		Blocks[index].Priority = priority;
		Blocks[index].bReduceBothAxes = bReduceBothAxes;
		Blocks[index].bReduceByTwo = bReduceByTwo;
	}


	//---------------------------------------------------------------------------------------------
	void Layout::SetLayoutPackingStrategy(EPackStrategy _strategy)
	{
		Strategy = _strategy;
	}


	//---------------------------------------------------------------------------------------------
	EPackStrategy Layout::GetLayoutPackingStrategy() const
	{
		return Strategy;
	}


	//---------------------------------------------------------------------------------------------
	void Layout::Serialise(OutputArchive& arch) const
	{
		uint32 ver = 7;
		arch << ver;

		arch << Size;
		arch << Blocks;

		arch << MaxSize;
		arch << uint32(Strategy);
		arch << FirstLODToIgnoreWarnings;
		arch << uint32(ReductionMethod);
	}

	
	//---------------------------------------------------------------------------------------------
	void Layout::Unserialise(InputArchive& arch)
	{
		uint32 ver;
		arch >> ver;
		check(ver <= 7);

		arch >> Size;
		arch >> Blocks;
		arch >> MaxSize;

		uint32 temp;
		arch >> temp;
		Strategy = EPackStrategy(temp);

		arch >> FirstLODToIgnoreWarnings;

		arch >> temp;
		ReductionMethod = EReductionMethod(temp);
	}


	//---------------------------------------------------------------------------------------------
	bool Layout::IsSimilar(const Layout& o) const
	{
		if (Size != o.Size || MaxSize != o.MaxSize ||
			Blocks.Num() != o.Blocks.Num() || Strategy != o.Strategy)
			return false;

		for (int32 i = 0; i < Blocks.Num(); ++i)
		{
			if (!Blocks[i].IsSimilar(o.Blocks[i])) return false;
		}

		return true;

	}


	//---------------------------------------------------------------------------------------------
	int32 Layout::FindBlock(uint64 Id) const
	{
		for (int32 i = 0; i < Blocks.Num(); ++i)
		{
			if (Blocks[i].Id == Id)
			{
				return i;
			}
		}

		return -1;
	}


	//---------------------------------------------------------------------------------------------
	bool Layout::IsSingleBlockAndFull() const
	{
		if (Blocks.Num() == 1
			&& Blocks[0].Min == UE::Math::TIntVector2<uint16>(0, 0)
			&& Blocks[0].Size == Size)
		{
			return true;
		}
		return false;
	}


	//---------------------------------------------------------------------------------------------
	void Layout::SetIgnoreLODWarnings(int32 LOD)
	{
		FirstLODToIgnoreWarnings = LOD;
	}


	//---------------------------------------------------------------------------------------------
	int32 Layout::GetIgnoreLODWarnings()
	{
		return FirstLODToIgnoreWarnings;
	}


	//---------------------------------------------------------------------------------------------
	void Layout::SetBlockReductionMethod(EReductionMethod Method)
	{
		ReductionMethod = Method;
	}


	//---------------------------------------------------------------------------------------------
	EReductionMethod Layout::GetBlockReductionMethod() const
	{
		return ReductionMethod;
	}

	
	//---------------------------------------------------------------------------------------------
	void Layout::FBlock::Serialise(OutputArchive& arch) const
	{
		arch << Min;
		arch << Size;
		arch << Id;
		arch << Priority;
		arch << bReduceBothAxes;
		arch << bReduceByTwo;
	}


	//---------------------------------------------------------------------------------------------
	void Layout::FBlock::Unserialise(InputArchive& arch)
	{
		arch >> Min;
		arch >> Size;
		arch >> Id;
		arch >> Priority;
		arch >> bReduceBothAxes;
		arch >> bReduceByTwo;
	}

}

