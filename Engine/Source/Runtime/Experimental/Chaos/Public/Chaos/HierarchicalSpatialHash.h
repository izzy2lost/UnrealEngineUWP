// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Box.h"
#include "Chaos/Vector.h"
#include "Containers/Map.h"
#include "Chaos/Framework/Parallel.h"
#include "Math/VectorRegister.h"

#include <atomic>

namespace Chaos
{
struct FHierarchicalSpatialHashCellIdx
{
	TVec3<int32> Cell;
	int32 Lod;

	bool operator==(const FHierarchicalSpatialHashCellIdx& OtherIdx) const
	{
		return Cell[0] == OtherIdx.Cell[0] && Cell[1] == OtherIdx.Cell[1] && Cell[2] == OtherIdx.Cell[2] && Lod == OtherIdx.Lod;
	}
	
	template<typename T>
	static int32 LodForCellSize(T CellSize)
	{
		// want Ceil( log2(CellSize) )
		if (CellSize <= (T).5)
		{
			return -(int32)FMath::FloorLog2((uint32)FMath::FloorToInt32((T)1 / CellSize));
		}
		else
		{
			return (int32)FMath::CeilLogTwo((uint32)FMath::CeilToInt32(CellSize));
		}
	}

	template<typename T>
	static int32 LodForAABB(const TAABB<T, 3>& Box)
	{
		const T LongestSideLen = Box.Extents().Max();
		return LodForCellSize(LongestSideLen);
	}

	template<typename T>
	static T CellSizeForLod(const int32 Lod)
	{
		if (Lod >= 0)
		{
			return (T)(1ULL << Lod);
		}
		return (T)1 / (T)(1ULL << (-Lod));
	}

	template<typename T>
	static TVec3<int32> CellIdxForPoint(const TVec3<T>& Point, const T CellSize)
	{
		return { FMath::FloorToInt32(Point[0] / CellSize), FMath::FloorToInt32(Point[1] / CellSize), FMath::FloorToInt32(Point[2] / CellSize) };
	}
};

inline uint32 GetTypeHash(const Chaos::FHierarchicalSpatialHashCellIdx& Idx)
{
	// xorHash
	int32 Hash = Idx.Cell[0] * 73856093;
	Hash = Hash ^ (Idx.Cell[1] * 19349663);
	Hash = Hash ^ (Idx.Cell[2] * 83492791);
	Hash = Hash ^ (Idx.Lod * 67867979);
	return (uint32)Hash;
}

template<typename PayloadType>
class TSpatialHashGridBase
{
protected:	
	struct FPayloadAndBounds
	{
		PayloadType Payload;
		int32 BoundsLookupIdx;
	};

	// This hashmap is designed to be fast at just what we need to do to build all at once in exactly the order it is done in Initialize and then query. 
	// Based off of Core/HashTable.h
	class FHashMap
	{
	public:
		void Reset()
		{
			Hash.Reset();
			Elements.Reset();
		}

		void PreallocateElementsForConcurrentAdd(int32 MaxNumElements, int32 HashSize)
		{
			HashSize = FMath::RoundUpToPowerOfTwo(HashSize);
			Hash.SetNumUninitialized(HashSize);
			for (std::atomic<int32>& H : Hash)
			{
				H.store(INDEX_NONE, std::memory_order_relaxed);
			}
			Elements.SetNumUninitialized(MaxNumElements);
			ConcurrentElementAddIdx.store(0);
		}

		void ConcurrentAddElementRange(const TVec3<int32>& MinCellIdx, const TVec3<int32>& MaxCellIdx, int32 Lod, const FPayloadAndBounds& Value)
		{
			const int32 NumElementsToAdd = (MaxCellIdx[0] - MinCellIdx[0] + 1) * (MaxCellIdx[1] - MinCellIdx[1] + 1) * (MaxCellIdx[2] - MinCellIdx[2] + 1);

			int32 IndexToWrite = ConcurrentElementAddIdx.fetch_add(NumElementsToAdd, std::memory_order_relaxed);

			for (int32 XIdx = MinCellIdx[0]; XIdx <= MaxCellIdx[0]; ++XIdx)
			{
				for (int32 YIdx = MinCellIdx[1]; YIdx <= MaxCellIdx[1]; ++YIdx)
				{
					for (int32 ZIdx = MinCellIdx[2]; ZIdx <= MaxCellIdx[2]; ++ZIdx)
					{
						Elements[IndexToWrite].Key = FHierarchicalSpatialHashCellIdx{ {XIdx, YIdx, ZIdx}, Lod };
						Elements[IndexToWrite].Value = Value;
						const uint32 HashKey = GetTypeHash(Elements[IndexToWrite].Key) & (Hash.Num() - 1);
						Elements[IndexToWrite].NextIndex = Hash[HashKey].exchange(IndexToWrite);
						++IndexToWrite;
					}
				}
			}
		}

		int32 ConcurrentAddElement(const TVec3<int32>& CellIdx, int32 Lod, const FPayloadAndBounds& Value)
		{
			const int32 IndexToWrite = ConcurrentElementAddIdx.fetch_add(1, std::memory_order_relaxed);
			Elements[IndexToWrite].Key = FHierarchicalSpatialHashCellIdx{ CellIdx, Lod };
			Elements[IndexToWrite].Value = Value;
			const uint32 HashKey = GetTypeHash(Elements[IndexToWrite].Key) & (Hash.Num() - 1);
			Elements[IndexToWrite].NextIndex = Hash[HashKey].exchange(IndexToWrite);
			return IndexToWrite;
		}

		void ShrinkElementsAfterConcurrentAdd()
		{
			constexpr bool bAllowShrinking = false;
			Elements.SetNum(ConcurrentElementAddIdx.load(), bAllowShrinking);
		}

		int32 First(const FHierarchicalSpatialHashCellIdx& Key) const
		{
			checkSlow(FMath::IsPowerOfTwo(Hash.Num()));
			const uint32 HashKey = GetTypeHash(Key) & (Hash.Num() - 1);
			return Hash[HashKey].load(std::memory_order_relaxed);
		}

		int32 Next(int32 Index) const
		{
			return Elements[Index].NextIndex;
		}

		static bool IsValid(int32 Index)
		{
			return Index != INDEX_NONE;
		}

		bool KeyMatches(int32 Index, const FHierarchicalSpatialHashCellIdx& Key) const
		{
			return Elements[Index].Key == Key;
		}

		const FPayloadAndBounds& GetValue(int32 Index) const
		{
			return Elements[Index].Value;
		}

		const FHierarchicalSpatialHashCellIdx& GetKey(int32 Index) const
		{
			return Elements[Index].Key;
		}

	private:

		TArray<std::atomic<int32>> Hash;
		struct alignas(32) FElement
		{
			FHierarchicalSpatialHashCellIdx Key;
			FPayloadAndBounds Value;
			int32 NextIndex;
		};

		TArray<FElement> Elements;
		std::atomic<int32> ConcurrentElementAddIdx;
	};

	FHashMap HashMap;

	void Reset()
	{
		HashMap.Reset();
	}
};

// Currently this assumes a 3 dimensional grid. 
template<typename PayloadType, typename T>
class THierarchicalSpatialHash : public TSpatialHashGridBase<PayloadType>
{
	typedef TSpatialHashGridBase<PayloadType> Base;
	using typename Base::FPayloadAndBounds;
	using typename Base::FHashMap;
	using Base::HashMap;

	class FVectorAABB
	{
	public:

		FVectorAABB() = default;

		explicit FVectorAABB(const TAABB<T, 3>& BBox)
			: Min(MakeVectorRegister(BBox.Min()[0], BBox.Min()[1], BBox.Min()[2], (T)0.))
			, Max(MakeVectorRegister(BBox.Max()[0], BBox.Max()[1], BBox.Max()[2], (T)0.))
		{}

		bool Intersects(const FVectorAABB& Other) const
		{
			if (VectorAnyLesserThan(Other.Max, Min) || VectorAnyGreaterThan(Other.Min, Max))
			{
				return false;
			}
			return true;
		}

		bool Intersects(const TVectorRegisterType<T>& Point) const
		{
			if (VectorAnyLesserThan(Point, Min) || VectorAnyGreaterThan(Point, Max))
			{
				return false;
			}
			return true;
		}

	private:

		TVectorRegisterType<T> Min;
		TVectorRegisterType<T> Max;
	};

public:

	void Reset()
	{
		Base::Reset();
		UsedLods.Reset();
	}

	template<typename ParticlesType>
	void Initialize(const ParticlesType& Particles, const T MinLodSize = (T)0)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(HierarchicalSpatialHash_Init);
		Reset();

		Bounds.SetNumUninitialized(Particles.Num());
		HashMap.PreallocateElementsForConcurrentAdd(Particles.Num() * 8, Particles.Num() * 2);

		const int32 MinAllowableLod = MinLodSize > (T)0 ? FHierarchicalSpatialHashCellIdx::LodForCellSize(MinLodSize) : std::numeric_limits<int32>::min();

		// For most common UsedLods, store whether or not the Lod is used in an array without a lock.
		// For uncommon Lods, use a critical section to store directly into the UsedLods map.
		constexpr int32 MaxPreAllocatedUsedLodValue = 10; // i.e., a single element 2^10 = 1024 across
		constexpr int32 MinPreAllocatedUsedLodValue = -10; // i.e., a single element 1 / (2^10) ~= 0.001 across.
		TStaticArray<bool, MaxPreAllocatedUsedLodValue - MinPreAllocatedUsedLodValue + 1> UsedLodsArray;
		memset(UsedLodsArray.GetData(), 0, UsedLodsArray.Num() * sizeof(bool));
		FCriticalSection UsedLodsCriticalSection;

		PhysicsParallelFor(Particles.Num(),
			[this, &Particles, MinAllowableLod, MaxPreAllocatedUsedLodValue, MinPreAllocatedUsedLodValue, &UsedLodsArray, &UsedLodsCriticalSection](int32 ParticleIdx)
			{
				const auto& Particle = Particles[ParticleIdx];
				const TAABB<T, 3> ParticleBounds = Particle.BoundingBox();

				const int32 Lod = FMath::Max(MinAllowableLod, FHierarchicalSpatialHashCellIdx::LodForAABB(ParticleBounds));
				const T CellSize = FHierarchicalSpatialHashCellIdx::CellSizeForLod<T>(Lod);

				const PayloadType Payload = Particle.template GetPayload<PayloadType>(ParticleIdx);
				Bounds[ParticleIdx] = FVectorAABB(ParticleBounds);

				const TVec3<int32> MinCellIdx = FHierarchicalSpatialHashCellIdx::CellIdxForPoint(ParticleBounds.Min(), CellSize);
				const TVec3<int32> MaxCellIdx = FHierarchicalSpatialHashCellIdx::CellIdxForPoint(ParticleBounds.Max(), CellSize);
				const FPayloadAndBounds Value = { Payload, ParticleIdx };
				HashMap.ConcurrentAddElementRange(MinCellIdx, MaxCellIdx, Lod, Value);

				if (Lod >= MinPreAllocatedUsedLodValue && Lod <= MaxPreAllocatedUsedLodValue)
				{
					UsedLodsArray[Lod - MinPreAllocatedUsedLodValue] = true;
				}
				else
				{
					UsedLodsCriticalSection.Lock();
					UsedLods.Add(Lod, CellSize);
					UsedLodsCriticalSection.Unlock();
				}
			}
		);

		HashMap.ShrinkElementsAfterConcurrentAdd();

		for (int32 Lod = MinPreAllocatedUsedLodValue; Lod <= MaxPreAllocatedUsedLodValue; ++Lod)
		{
			if (UsedLodsArray[Lod - MinPreAllocatedUsedLodValue])
			{
				UsedLods.Add(Lod, FHierarchicalSpatialHashCellIdx::CellSizeForLod<T>(Lod));
			}
		}
	}

	TArray<PayloadType> FindAllIntersections(const TAABB<T, 3>& LookupBounds) const 
	{
		TArray<PayloadType> Result;
		FVectorAABB LookupBoundsVec(LookupBounds);
		for (const typename TMap<int32, T>::ElementType& UsedLod : UsedLods)
		{
			const int32 Lod = UsedLod.Key;
			const T CellSize = UsedLod.Value;

			const TVec3<int32> MinCellIdx = FHierarchicalSpatialHashCellIdx::CellIdxForPoint(LookupBounds.Min(), CellSize);
			const TVec3<int32> MaxCellIdx = FHierarchicalSpatialHashCellIdx::CellIdxForPoint(LookupBounds.Max(), CellSize);

			for (int32 XIdx = MinCellIdx[0]; XIdx <= MaxCellIdx[0]; ++XIdx)
			{
				for (int32 YIdx = MinCellIdx[1]; YIdx <= MaxCellIdx[1]; ++YIdx)
				{
					for (int32 ZIdx = MinCellIdx[2]; ZIdx <= MaxCellIdx[2]; ++ZIdx)
					{
						const FHierarchicalSpatialHashCellIdx Key{ {XIdx, YIdx, ZIdx}, Lod };
						for (int32 HashIndex = HashMap.First(Key); HashMap.IsValid(HashIndex); HashIndex = HashMap.Next(HashIndex))
						{
							if (HashMap.KeyMatches(HashIndex, Key))
							{
								const FPayloadAndBounds Value = HashMap.GetValue(HashIndex);
								const FVectorAABB& PayloadBounds = Bounds[Value.BoundsLookupIdx];

								if (PayloadBounds.Intersects(LookupBoundsVec))
								{
									// Typical cloth examples do not find a lot of unique results. 
									// In testing, the overhead to do this search is lower than using a TSet and converting the result to an array
									Result.AddUnique(Value.Payload);
								}
							}
						}
					}
				}
			}
		}
		return Result;
	}

	TArray<PayloadType> FindAllIntersections(const TVec3<T>& Point) const
	{
		TVectorRegisterType<T> PointVec(MakeVectorRegister(Point.X, Point.Y, Point.Z, (T)0));

		TArray<PayloadType> Result;
		for (const typename TMap<int32, T>::ElementType& UsedLod : UsedLods)
		{
			const int32 Lod = UsedLod.Key;
			const T CellSize = UsedLod.Value;

			const TVec3<int32> CellIdx = FHierarchicalSpatialHashCellIdx::CellIdxForPoint(Point, CellSize);
			const FHierarchicalSpatialHashCellIdx Key{ CellIdx, Lod };
			for (int32 HashIndex = HashMap.First(Key); HashMap.IsValid(HashIndex); HashIndex = HashMap.Next(HashIndex))
			{
				if (HashMap.KeyMatches(HashIndex, Key))
				{
					const FPayloadAndBounds Value = HashMap.GetValue(HashIndex);
					const FVectorAABB& PayloadBounds = Bounds[Value.BoundsLookupIdx];

					if (PayloadBounds.Intersects(PointVec))
					{
						// Typical cloth examples do not find a lot of unique results. 
						// In testing, the overhead to do this search is lower than using a TSet and converting the result to an array
						Result.AddUnique(Value.Payload);
					}
				}
			}
		}
		return Result;
	}

private:
	TMap<int32, T> UsedLods;
	TArray<FVectorAABB> Bounds;
};

template<typename PayloadType, typename T>
class TSpatialHashGridPoints : public TSpatialHashGridBase<PayloadType>
{
	typedef TSpatialHashGridBase<PayloadType> Base;
	using typename Base::FPayloadAndBounds;
	using typename Base::FHashMap;
	using Base::HashMap;

public:
	TSpatialHashGridPoints(const T InCellSize)
		: Base()
		, CellSize(InCellSize)
	{}

	void Reset()
	{
		Base::Reset();
		ParticleIndexToElement.Reset();
	}

	template<typename ParticlesType>
	void InitializePoints(const ParticlesType& Particles)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SpatialHashGridConstantBounds_Init);
		Reset();

		ParticleIndexToElement.SetNum(Particles.Num());
		HashMap.PreallocateElementsForConcurrentAdd(Particles.Num(), Particles.Num());
		PhysicsParallelFor(Particles.Num(),
			[this, &Particles](int32 ParticleIdx)
		{
			const auto& Particle = Particles[ParticleIdx];
			const PayloadType Payload = Particle.template GetPayload<PayloadType>(ParticleIdx);
			const TVec3<int32> CellIdx = FHierarchicalSpatialHashCellIdx::CellIdxForPoint(Particle.X(), CellSize);
			const FPayloadAndBounds Value = { Payload, ParticleIdx };
			const int32 ElementIdx = HashMap.ConcurrentAddElement(CellIdx, INDEX_NONE, Value);
			ParticleIndexToElement[ParticleIdx] = ElementIdx;
		}
		);

		HashMap.ShrinkElementsAfterConcurrentAdd();
	}

	TArray<TVec2<PayloadType>> FindAllSelfProximities(int32 CellRadius, int32 MaxNumExpectedConnections, TFunctionRef<bool(const int32 Payload0, const int32 Payload1)> NarrowTest)
	{
		check(CellRadius >= 0);

		// Resize using a RWLock. Start with MaxNumExpectedConnections
		TArray<TVec2<PayloadType>> Result;
		Result.SetNum(MaxNumExpectedConnections);
		Result.SetNum(Result.Max()); // Remove all slack.
		std::atomic<int32> ResultIndex(0);
		std::atomic<int32> CurrentResultNum(Result.Num()); // Arrays aren't thread-safe, so use this to track size instead.
		FRWLock ResultResizeRWLock;

		auto AddResult = [this, &NarrowTest, &Result, &ResultIndex, &CurrentResultNum, &ResultResizeRWLock]
		(const TVec2<PayloadType>& Proximity)
		{
			if (!NarrowTest(Proximity[0], Proximity[1]))
			{
				return;
			}
			const int32 NewResultIndex = ResultIndex.fetch_add(1);
			if (NewResultIndex < CurrentResultNum.load())
			{
				ResultResizeRWLock.ReadLock();
				Result[NewResultIndex] = Proximity;
				ResultResizeRWLock.ReadUnlock();
			}
			else
			{
				ResultResizeRWLock.WriteLock();
				if (NewResultIndex >= CurrentResultNum.load())
				{
					// Use array's allocator to decide how much slack to create
					Result.SetNum(NewResultIndex+1);
					Result.SetNum(Result.Max());
					CurrentResultNum.store(Result.Num());
				}
				Result[NewResultIndex] = Proximity;
				ResultResizeRWLock.WriteUnlock();
			}
		};

		auto AddResultsInCell = [this,&AddResult](PayloadType ThisPayload, const FHierarchicalSpatialHashCellIdx& Cell)
		{
			for (int32 HashIndex = HashMap.First(Cell); HashMap.IsValid(HashIndex); HashIndex = HashMap.Next(HashIndex))
			{
				if (HashMap.KeyMatches(HashIndex, Cell))
				{
					AddResult(TVec2<PayloadType>(ThisPayload, HashMap.GetValue(HashIndex).Payload));
				}
			}
		};

		PhysicsParallelFor(ParticleIndexToElement.Num(),
			[this, &AddResult, &AddResultsInCell, CellRadius](int32 ParticleIdx)
		{
			const int32 ThisElementIdx = ParticleIndexToElement[ParticleIdx];
			const int32 ThisPayload = HashMap.GetValue(ThisElementIdx).Payload;
			const FHierarchicalSpatialHashCellIdx& ThisKey = HashMap.GetKey(ThisElementIdx);

			// All neighbors in our cell after us in the list
			int32 OtherElementIdx = HashMap.Next(ThisElementIdx);
			for (; HashMap.IsValid(OtherElementIdx); OtherElementIdx = HashMap.Next(OtherElementIdx))
			{
				if (HashMap.KeyMatches(OtherElementIdx, ThisKey))
				{
					AddResult(TVec2<PayloadType>(ThisPayload, HashMap.GetValue(OtherElementIdx).Payload));
				}
			}

			// Iterate over cells within CellRadius away. Only look "forward". Particles behind us will find us by looking forward.
			// I dunno... there's probably a better way to do this.....
			for (int32 ZIndex = 1; ZIndex <= CellRadius; ++ZIndex)
			{
				AddResultsInCell(ThisPayload, FHierarchicalSpatialHashCellIdx{ ThisKey.Cell + TVec3<int32>(0,0,ZIndex), INDEX_NONE });
			}

			for (int32 YIndex = 1; YIndex <= CellRadius; ++YIndex)
			{
				for (int32 ZIndex = -CellRadius; ZIndex <= CellRadius; ++ZIndex)
				{
					AddResultsInCell(ThisPayload, FHierarchicalSpatialHashCellIdx{ ThisKey.Cell + TVec3<int32>(0,YIndex,ZIndex), INDEX_NONE });
				}
			}

			for (int32 XIndex = 1; XIndex <= CellRadius; ++XIndex)
			{
				for (int32 YIndex = -CellRadius; YIndex <= CellRadius; ++YIndex)
				{
					for (int32 ZIndex = -CellRadius; ZIndex <= CellRadius; ++ZIndex)
					{
						AddResultsInCell(ThisPayload, FHierarchicalSpatialHashCellIdx{ ThisKey.Cell + TVec3<int32>(XIndex,YIndex,ZIndex), INDEX_NONE });
					}
				}
			}
		}
		);

		// Shrink Result array to actual number of found proximities
		const int32 ResultNum = ResultIndex.load();
		Result.SetNum(ResultNum, /*bAllowShrinking*/ true);
		return Result;
	}
private:
	const T CellSize;

	// Particle Index is dense array index passed in at build time. 
	TArray<int32> ParticleIndexToElement;
};
}

