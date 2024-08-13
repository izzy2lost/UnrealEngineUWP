// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/System.h"

#include "MuR/Settings.h"
#include "MuR/Operations.h"
#include "MuR/Image.h"
#include "MuR/MutableString.h"
#include "MuR/ModelPrivate.h"
#include "MuR/Mesh.h"
#include "MuR/Instance.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/MutableTrace.h"

#include "Templates/UnrealTypeTraits.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#include "HAL/Thread.h"
#include "HAL/PlatformTLS.h"
#endif

namespace mu::MemoryCounters
{
	struct MUTABLERUNTIME_API FInternalMemoryCounter
	{
		alignas(8) static inline std::atomic<SSIZE_T> Counter {0};
	};
}

namespace mu
{
	inline uint32 GetResourceIDRoot(FResourceID Id)
	{
		return uint32(Id >> 32);
	}

	class ExtensionDataStreamer;

	// Call the tick of the LLM system (we do this to simulate a frame since the LLM system is not entirelly designed to run over a program)
	inline void UpdateLLMStats()
	{
		// This code will only be compiled (and ran) if the global definition to enable LLM tracking is set to 1 for the host program
		// Ex : 			GlobalDefinitions.Add("LLM_ENABLED_IN_CONFIG=1");
#if ENABLE_LOW_LEVEL_MEM_TRACKER && IS_PROGRAM
		FLowLevelMemTracker& MemTracker = FLowLevelMemTracker::Get();
		if (MemTracker.IsEnabled())
		{
			MemTracker.UpdateStatsPerFrame();
		}
#endif
	}

	constexpr uint64 AllParametersMask = TNumericLimits<uint64>::Max();

	enum class EMeshExecutionOptions : uint8
	{
		None               = 0,
		LoadGeometryData   = 1 << 0,
		LoadPoseData       = 1 << 1,
		LoadComponentsData = 1 << 2,

		AllFlags 	       = 0xFF,
	};

	ENUM_CLASS_FLAGS(EMeshExecutionOptions);

	/** ExecutinIndex stores the location inside all ranges of the execution of a specific
	* operation. The first integer on each pair is the dimension/range index in the program
	* array of ranges, and the second integer is the value inside that range.
	* The vector order is undefined.
	*/
	class ExecutionIndex : public  TArray<TPair<int32, int32>>
	{
	public:
		//! Set or add a value to the index
		void SetFromModelRangeIndex(uint16 RangeIndex, int32 RangeValue)
		{
			SizeType Index = IndexOfByPredicate([=](const ElementType& v) { return v.Key >= RangeIndex; });
			if (Index != INDEX_NONE && (*this)[Index].Key == RangeIndex)
			{
				// Update
				(*this)[Index].Value = RangeValue;
			}
			else
			{
				// Add new
				Push(ElementType(RangeIndex, RangeValue));
			}
		}

		//! Get the value of the index from the range index in the model.
		int32 GetFromModelRangeIndex(int32 ModelRangeIndex) const
		{
			for (const TPair<int32, int32>& e : *this)
			{
				if (e.Key == ModelRangeIndex)
				{
					return e.Value;
				}
			}
			return 0;
		}
	};


	/** This structure stores the data about an ongoing mutable operation that needs to be executed. */
	struct FScheduledOp
	{
		inline FScheduledOp()
		{
			Stage = 0;
			Type = EType::Full;
		}

		inline FScheduledOp(OP::ADDRESS InAt, const FScheduledOp& InOpTemplate, uint8 InStage = 0, uint32 InCustomState = 0)
		{
			check(InStage < 120);
			At = InAt;
			ExecutionOptions = InOpTemplate.ExecutionOptions;
			ExecutionIndex = InOpTemplate.ExecutionIndex;
			Stage = InStage;
			CustomState = InCustomState;
			Type = InOpTemplate.Type;
		}

		static inline FScheduledOp FromOpAndOptions(OP::ADDRESS InAt, const FScheduledOp& InOpTemplate, uint8 InExecutionOptions)
		{
			FScheduledOp r;
			r.At = InAt;
			r.ExecutionOptions = InExecutionOptions;
			r.ExecutionIndex = InOpTemplate.ExecutionIndex;
			r.Stage = 0;
			r.CustomState = InOpTemplate.CustomState;
			r.Type = InOpTemplate.Type;
			return r;
		}

		//! Address of the operation
		OP::ADDRESS At = 0;

		//! Additional custom state data that the operation can store. This is usually used to pass information
		//! between execution stages of an operation.
		uint32 CustomState = 0;

		//! Index of the operation execution: This is used for iteration of different ranges.
		//! It is an index into the CodeRunner::GetMemory()::m_rangeIndex vector.
		//! executionIndex 0 is always used for empty ExecutionIndex, which is the most common
		//! one.
		uint16 ExecutionIndex = 0;

		//! Additional execution options. Set externally to this op, it usually alters the result.
		//! For example, this is used to keep track of the mipmaps to skip in image operations.
		uint8 ExecutionOptions = 0;

		//! Internal stage of the operation.
		//! Stage 0 is usually scheduling of children, and 1 is execution. Some instructions
		//! may have more steges to schedule children that are optional for execution, etc.
		uint8 Stage : 6;

		//! Type of calculation we are requesting for this operation.
		enum class EType : uint8
		{
			// Execute the operation to calculate the full result
			Full,

			// Execute the operation to obtain the descriptor of an image.
			ImageDesc,

			// Execute the operation to generate a mesh components. Skeletons, referenced ids...
			MeshComp,
		};

		EType Type : 2;
	};

	/** A cache address is the operation plus the context of execution(iteration indices, etc...). */
	struct FCacheAddress
	{
		/** The meaning of all these fields is the same than the FScheduledOp struct. */
		OP::ADDRESS At = 0;
		uint16 ExecutionIndex = 0;
		uint8 ExecutionOptions = 0;
		FScheduledOp::EType Type = FScheduledOp::EType::Full;

		FCacheAddress() 
		{
		}

		FCacheAddress(OP::ADDRESS InAt, uint16 InExecutionIndex, uint8 InExecutionOptions, FScheduledOp::EType InType = FScheduledOp::EType::Full)
			: At(InAt), ExecutionIndex(InExecutionIndex), ExecutionOptions(InExecutionOptions), Type(InType)
		{
		}

		FCacheAddress(OP::ADDRESS InAt, const FScheduledOp& Item)
			: At(InAt), ExecutionIndex(Item.ExecutionIndex), ExecutionOptions(Item.ExecutionOptions), Type(Item.Type)
		{
		}

		FCacheAddress(const FScheduledOp& Item)
			: At(Item.At), ExecutionIndex(Item.ExecutionIndex), ExecutionOptions(Item.ExecutionOptions), Type(Item.Type)
		{
		}
	};

	static_assert(std::has_unique_object_representations_v<FCacheAddress>);
	static_assert(sizeof(FCacheAddress) == sizeof(uint64));

	inline bool operator==(const FCacheAddress& A, const FCacheAddress& B)
	{
		return BitCast<uint64>(A) == BitCast<uint64>(B); 
	}

	inline uint32 GetTypeHash(const FCacheAddress& CacheAddress)
	{
		return ::GetTypeHash(BitCast<uint64>(CacheAddress));
	}

	inline uint32 GetTypeHash(const Ptr<const Resource>& Value)
	{
		return ::GetTypeHash(Value.get());
	}


	//! Container that stores data per executable code operation (indexed by address and execution
	//! index).
	template<class DataType>
	class CodeContainer
	{
		using MemoryCounter = MemoryCounters::FInternalMemoryCounter;
		using ArrayDataContainerType = TArray<DataType, FDefaultMemoryTrackingAllocator<MemoryCounter>>;
		using MapDataContainerType = TMap<FCacheAddress, DataType, FDefaultMemoryTrackingSetAllocator<MemoryCounter>>;

	public:
		void Resize(int32 NewSize)
		{
			Index0.SetNumZeroed(NewSize);
		}

		uint32 CodeSize() const
		{
			return uint32(Index0.Num());
		}

		void Clear()
		{
			Index0.Empty();
			OtherIndex.Empty();
		}

		inline void Erase(const FCacheAddress& At)
		{
			if (At.Type == FScheduledOp::EType::Full && At.ExecutionIndex == 0 && At.ExecutionOptions == 0)
			{
				Index0[At.At] = nullptr;
			}
			else
			{
				OtherIndex.Remove(At);
			}
		}

		inline DataType Get(const FCacheAddress& At) const
		{
			if (At.Type == FScheduledOp::EType::Full && At.ExecutionIndex == 0 && At.ExecutionOptions == 0)
			{
				if (At.At < OP::ADDRESS(Index0.Num()))
				{
					return Index0[At.At];
				}
			}
			else
			{
				const DataType* Found = OtherIndex.Find(At);
				if (Found)
				{
					return *Found;
				}
			}

			return DataType{};
		}

		inline DataType* GetPtr(const FCacheAddress& At)
		{
			if (At.Type == FScheduledOp::EType::Full && At.ExecutionIndex == 0 && At.ExecutionOptions == 0)
			{
				if (At.At < OP::ADDRESS(Index0.Num()))
				{
					return &Index0[At.At];
				}
			}
			else
			{
				return &OtherIndex.FindOrAdd(At);
			}

			return nullptr;
		}

		inline const DataType* GetPtr(const FCacheAddress& At) const
		{
			if (At.Type == FScheduledOp::EType::Full && At.ExecutionIndex == 0 && At.ExecutionOptions == 0)
			{
				if (At.At < uint32(Index0.Num()))
				{
					return &Index0[At.At];
				}
			}
			else
			{
				return OtherIndex.Find(At);
			}

			return nullptr;
		}

		inline DataType& operator[](const FCacheAddress& At)
		{
			if (At.Type == FScheduledOp::EType::Full && At.ExecutionIndex == 0 && At.ExecutionOptions == 0)
			{
				return Index0[At.At];
			}
			else
			{
				return OtherIndex.FindOrAdd(At);
			}
		}

		inline const DataType& operator[](const FCacheAddress& At) const
		{
			if (At.Type == FScheduledOp::EType::Full && At.ExecutionIndex == 0 && At.ExecutionOptions == 0)
			{
				return Index0[At.At];
			}
			else
			{
				return OtherIndex[At];
			}
		}

		struct TIterator
		{
		private:
			friend class CodeContainer<DataType>;

			const CodeContainer<DataType>* Container;
			typename CodeContainer<DataType>::ArrayDataContainerType::TIterator Iter0;
			typename CodeContainer<DataType>::MapDataContainerType::TIterator Iter1;

			TIterator(CodeContainer<DataType>* InContainer)
				: Container(InContainer)
				, Iter0(InContainer->Index0.CreateIterator())
				, Iter1(InContainer->OtherIndex.CreateIterator())
			{
			}

		public:

			inline void operator++(int)
			{
				if (Iter0)
				{
					++Iter0;
				}
				else
				{
					++Iter1;
				}
			}

			TIterator operator++()
			{
				if (Iter0)
				{
					++Iter0;
				}
				else
				{
					++Iter1;
				}

				return *this;
			}

			inline bool operator != (const TIterator& Other) const
			{
				return Iter0 != Other.Iter0 || Iter1 != Other.Iter1;
			}

			inline DataType& operator*()
			{
				if (Iter0)
				{
					return *Iter0;
				}
				else
				{
					return Iter1->Value;
				}
			}

			inline FCacheAddress GetAddress() const
			{
				if (Iter0)
				{
					return {uint32(Iter0.GetIndex()), 0, 0};
				}
				else
				{
					return Iter1->Key;
				}
			}

			inline bool IsValid() const
			{
				return bool(Iter0) || bool(Iter1);
			}
		};

		inline TIterator Begin()
		{
			TIterator Iter(this);
			return Iter;
		}

		inline int32 GetAllocatedSize() const
		{
			return Index0.GetAllocatedSize() + OtherIndex.GetAllocatedSize();
		}
	private:
		// For index 0
		ArrayDataContainerType Index0;

		// For index > 0
		MapDataContainerType OtherIndex;
	};


	/** Interface for storage of data while Mutable code is being executed. */
	class FProgramCache
	{
	public:

		using AllocType = FDefaultMemoryTrackingAllocator<MemoryCounters::FInternalMemoryCounter>;

		template<class Type, class Alloc = AllocType>
		using TMemoryTrackedArray = TArray<Type, Alloc>;
		
		TMemoryTrackedArray<ExecutionIndex, TInlineAllocator<4, AllocType>> UsedRangeIndices;

		/** Runtime data for each program op. */
		struct FOpExecutionData
		{
			uint16 OpHitCount;

			/** Enabled if the op descriptor has been calculated. */
			uint8 IsDescCacheValid : 1;
			uint8 IsValueValid     : 1;
			uint8 IsCacheLocked    : 1;

			/** The operation DATA_TYPE. */
			uint8 DataType;

			/** 
			 * The position in the type-specific array where the result data is stored. 
			 * 0 means index not valid.
			 * For small types like bool, int and float, the actual value is the index, instead of having their own array.
			 */
			union
			{
				int32 DataTypeIndex;
				float ScalarResult;
			};
		};

		/** Cached resources while the program is executing. 
		* first value of the pair:
		* 0 : value not valid (not set).
		* 1 : valid, not worth freeing for memory
		* 2 : valid, worth freeing
		*/
		CodeContainer<FOpExecutionData> OpExecutionData;

		template<class ResourceType>
		struct TResourceResult
		{
			static_assert(TIsDerivedFrom<ResourceType, Resource>::Value);

			FCacheAddress OpAddress;
			Ptr<const ResourceType> Value = nullptr;
		};
		static_assert(sizeof(TResourceResult<Image>) == 16);
		static_assert(sizeof(TResourceResult<Mesh>) == 16);

		/** */

		TMemoryTrackedArray<FVector4f> ColorResults;
		TMemoryTrackedArray<TResourceResult<Image>> ImageResults;
		TMemoryTrackedArray<TResourceResult<Mesh>> MeshResults;
		TMemoryTrackedArray<Ptr<const Layout>> LayoutResults;
		TMemoryTrackedArray<Ptr<const Instance>> InstanceResults;
		TMemoryTrackedArray<FProjector> ProjectorResults;
		TMemoryTrackedArray<Ptr<const String>> StringResults;
		TMemoryTrackedArray<Ptr<const ExtensionData>> ExtensionDataResults;

		/** */
		inline const ExecutionIndex& GetRangeIndex(uint32 Index)
		{
			// Make sure we have the default element.
			if (UsedRangeIndices.IsEmpty())
			{
				UsedRangeIndices.Push(ExecutionIndex());
			}

			check(Index < uint32(UsedRangeIndices.Num()));
			return UsedRangeIndices[Index];
		}

		//!
		inline uint32 GetRangeIndexIndex(const ExecutionIndex& RangeIndex)
		{
			if (RangeIndex.IsEmpty())
			{
				return 0;
			}

			// Make sure we have the default element.
			if (UsedRangeIndices.IsEmpty())
			{
				UsedRangeIndices.Push(ExecutionIndex());
			}

			// Look for or add the new element.
			int32 ElemIndex = UsedRangeIndices.Find(RangeIndex);
			if (ElemIndex != INDEX_NONE)
			{
				return ElemIndex;
			}

			UsedRangeIndices.Push(RangeIndex);
			return uint32(UsedRangeIndices.Num()) - 1;
		}

		void Init(uint32 Size)
		{
			// This clear would prevent live update cache reusal
			//OpExecutionData.Clear();

			OpExecutionData.Resize(Size);

			if (ColorResults.IsEmpty())
			{
				// Insert dafault/null values
				ColorResults.Add(FVector4f());
				ImageResults.Emplace();
				LayoutResults.Add(nullptr);
				MeshResults.Emplace();
				InstanceResults.Add(nullptr);
				ProjectorResults.Add(FProjector());
				StringResults.Add(nullptr);
				ExtensionDataResults.Add(nullptr);
			}
		}

		void SetUnused(FOpExecutionData& Data)
		{
			// Only clear datatypes that have results that use relevant amounts of memory			
			uint32 DataTypeIndex = Data.DataTypeIndex;
			Data.IsValueValid = false;

			if (DataTypeIndex)
			{
				check(Data.DataType < DATATYPE::DT_COUNT);
				switch ((DATATYPE)Data.DataType)
				{
				case DATATYPE::DT_IMAGE:					
					ImageResults[DataTypeIndex].Value = nullptr;
					break;

				case DATATYPE::DT_MESH:
					MeshResults[DataTypeIndex].Value = nullptr;
					break;

				case DATATYPE::DT_INSTANCE:
					InstanceResults[DataTypeIndex] = nullptr;
					break;

				case DATATYPE::DT_EXTENSION_DATA:
					ExtensionDataResults[DataTypeIndex] = nullptr;
					break;

				default:
					break;
				}
			}
		}


		bool IsValid(FCacheAddress At) const
		{
			if ((At.At == 0) || (At.At >= OP::ADDRESS(OpExecutionData.CodeSize())))
			{
				return false;
			}

			const FOpExecutionData* Data = OpExecutionData.GetPtr(At);
			if (!Data)
			{
				return false;
			}

			// Is it a desc data query?
			if (At.Type == FScheduledOp::EType::ImageDesc)
			{
				return Data->IsDescCacheValid;
			}

			// It's a full data query.
			return Data->IsValueValid != 0;
		}


		/** */
		void CheckHitCountsCleared()
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			//MUTABLE_CPUPROFILER_SCOPE(CheckHitCountsCleared);

			//int32 IncorrectCount = 0;
			//CodeContainer<int32>::TIterator Iter = OpHitCount.begin();
			//for (; Iter.IsValid(); ++it)
			//{
			//	int32 Count = *it;
			//	if (Count>0 && Count < UE_MUTABLE_CACHE_COUNT_LIMIT)
			//	{
			//		// We don't manage the hitcounts of small types that don't use much memory
			//		if (m_resources[it.get_address()].Key==2)
			//		{
			//			// Op hitcount should have reached 0, otherwise, it means we requested an operation but never read 
			//			// the result.
			//			++IncorrectCount;
			//		}
			//	}
			//}

			//if (IncorrectCount > 0)
			//{
			//	UE_LOG(LogMutableCore, Log, TEXT("The op-hit-count didn't hit 0 for %5d operations. This may mean that too much memory is cached."), IncorrectCount);
			//	//check(false);
			//}
#endif
		}


		void Clear()
		{
			MUTABLE_CPUPROFILER_SCOPE(ProgramCacheClear);

			uint32 CodeSize = OpExecutionData.CodeSize();
			OpExecutionData.Clear();
			OpExecutionData.Resize(CodeSize);
		}


		void ClearDescCache()
		{
			MUTABLE_CPUPROFILER_SCOPE(ProgramDescCacheClear);

			CodeContainer<FProgramCache::FOpExecutionData>::TIterator Iter = OpExecutionData.Begin();
			for (; Iter.IsValid(); ++Iter)
			{
				FProgramCache::FOpExecutionData& Data = *Iter;
				Data.IsDescCacheValid = false;
			}
		}


		bool GetBool(FCacheAddress At)
		{
			if (!At.At)
			{
				return false;
			}

			const FOpExecutionData* Data = OpExecutionData.GetPtr(At);
			if (!Data)
			{
				return false;
			}

			check(Data->DataType == DATATYPE::DT_BOOL);
			return Data->DataTypeIndex != 0;
		}

		float GetScalar(FCacheAddress At)
		{
			if (!At.At)
			{
				return 0.0f;
			}

			const FOpExecutionData* Data = OpExecutionData.GetPtr(At);
			if (!Data) 
			{
				return 0.0f;
			}

			check(Data->DataType == DATATYPE::DT_SCALAR);
			return Data->ScalarResult;
		}

		int32 GetInt(FCacheAddress At)
		{
			if (!At.At) 
			{
				return 0;
			}

			const FOpExecutionData* Data = OpExecutionData.GetPtr(At);
			if (!Data) 
			{
				return 0;
			}

			check(Data->DataType == DATATYPE::DT_INT);
			return Data->DataTypeIndex;
		}

		FVector4f GetColour(FCacheAddress At)
		{
			if (!At.At)
			{
				return FVector4f();
			}

			const FOpExecutionData* Data = OpExecutionData.GetPtr(At);
			if (!Data) 
			{
				return FVector4f();
			}

			check(Data->DataType == DATATYPE::DT_COLOUR);
			return ColorResults[Data->DataTypeIndex];
		}

		FProjector GetProjector(FCacheAddress At)
		{
			if (!At.At)
			{
				return FProjector();
			}

			const FOpExecutionData* Data = OpExecutionData.GetPtr(At);

			if (!Data) 
			{
				return FProjector();
			}

			check(Data->DataType == DATATYPE::DT_PROJECTOR);
			return ProjectorResults[Data->DataTypeIndex];
		}

		Ptr<const Instance> GetInstance(FCacheAddress At)
		{
			if (!At.At) 
			{
				return nullptr;
			}

			FOpExecutionData* Data = OpExecutionData.GetPtr(At);
			if (!Data)
			{
				return nullptr;
			}

			check(Data->DataType == DATATYPE::DT_INSTANCE);
			Ptr<const Instance> Result = InstanceResults[Data->DataTypeIndex];

			// We need to decrease the hit-count even if the result is null.
			check(Data->OpHitCount > 0);

			--Data->OpHitCount;
			if (Data->OpHitCount == 0 && !Data->IsCacheLocked)
			{
				SetUnused(*Data);
			}

			return Result;
		}


		Ptr<const Image> GetImage(FCacheAddress At, bool& bIsLastReference)
		{
			bIsLastReference = false;

			if (!At.At) 
			{
				return nullptr;
			}

			if (At.At >= OpExecutionData.CodeSize())
			{
				return nullptr;
			}

			FOpExecutionData* Data = OpExecutionData.GetPtr(At);
			if (!Data) 
			{
				return nullptr;
			}

			check(Data->DataType == DATATYPE::DT_IMAGE);
			Ptr<const Image> Result = ImageResults[Data->DataTypeIndex].Value;

			// We need to decrease the hit-count even if the result is null.
			check(Data->OpHitCount > 0);

			--Data->OpHitCount;
			if (Data->OpHitCount == 0 && !Data->IsCacheLocked)
			{
				SetUnused(*Data);
				bIsLastReference = true;
			}

			return Result;
		}


		Ptr<const Mesh> GetMesh(FCacheAddress At, bool& bIsLastReference)
		{
			bIsLastReference = false;

			if (!At.At)
			{
				return nullptr;
			}

			if (At.At >= OpExecutionData.CodeSize())
			{
				return nullptr;
			}

			FOpExecutionData* Data = OpExecutionData.GetPtr(At);
			if (!Data)
			{
				return nullptr;
			}

			// Chache lock forcing is only done at base address, without options, index or type. See SetForceCached().
			FOpExecutionData& ForceLockCacheData = OpExecutionData[FCacheAddress(At.At, 0, 0)];

			check(Data->DataType == DATATYPE::DT_MESH);
			Ptr<const Mesh> Result = MeshResults[Data->DataTypeIndex].Value;

			// We need to decrease the hit-count even if the result is null.
			check(Data->OpHitCount > 0);

			--Data->OpHitCount;
			if (Data->OpHitCount == 0 && !Data->IsCacheLocked && !ForceLockCacheData.IsCacheLocked)
			{
				SetUnused(*Data);
				bIsLastReference = true;
			}

			return Result;
		}


		Ptr<const Layout> GetLayout(FCacheAddress At)
		{
			if (!At.At) 
			{
				return nullptr;
			}

			FOpExecutionData* Data = OpExecutionData.GetPtr(At);
			if (!Data) 
			{
				return nullptr;
			}

			check(Data->DataType == DATATYPE::DT_LAYOUT);
			return LayoutResults[Data->DataTypeIndex];
		}


		Ptr<const String> GetString(FCacheAddress At)
		{
			if (!At.At) 
			{
				return nullptr;
			}

			FOpExecutionData* Data = OpExecutionData.GetPtr(At);
			if (!Data) 
			{
				return nullptr;
			}

			check(Data->DataType == DATATYPE::DT_STRING);
			return StringResults[Data->DataTypeIndex];
		}


		Ptr<const ExtensionData> GetExtensionData(FCacheAddress At)
		{
			if (!At.At) 
			{
				return nullptr;
			}

			FOpExecutionData* Data = OpExecutionData.GetPtr(At);
			if (!Data) 
			{
				return nullptr;
			}

			check(Data->DataType == DATATYPE::DT_EXTENSION_DATA);
			return ExtensionDataResults[Data->DataTypeIndex];
		}

		void SetValidDesc(FCacheAddress At)
		{
			check(At.Type == FScheduledOp::EType::ImageDesc);
			check(At.At < OpExecutionData.CodeSize());
			FOpExecutionData* Data = OpExecutionData.GetPtr(At);
			Data->IsDescCacheValid = true;
		}

		void SetBool(FCacheAddress At, bool Value)
		{
			check(At.At < OpExecutionData.CodeSize());
			FOpExecutionData* Data = OpExecutionData.GetPtr(At);
			check(Data->DataType == DATATYPE::DT_BOOL || Data->DataType == DATATYPE::DT_NONE);
			Data->DataType = DATATYPE::DT_BOOL;
			Data->DataTypeIndex = Value;
			Data->IsValueValid = true;
		}

		void SetInt(FCacheAddress At, int32 Value)
		{
			check(At.At < OpExecutionData.CodeSize());
			FOpExecutionData* Data = OpExecutionData.GetPtr(At);
			check(Data->DataType == DATATYPE::DT_INT || Data->DataType == DATATYPE::DT_NONE);
			Data->DataType = DATATYPE::DT_INT;
			Data->DataTypeIndex = Value;
			Data->IsValueValid = true;
		}

		void SetScalar(FCacheAddress At, float Value)
		{
			check(At.At < OpExecutionData.CodeSize());
			FOpExecutionData* Data = OpExecutionData.GetPtr(At);
			check(Data->DataType == DATATYPE::DT_SCALAR || Data->DataType == DATATYPE::DT_NONE);
			Data->DataType = DATATYPE::DT_SCALAR;
			Data->ScalarResult = Value;
			Data->IsValueValid = true;
		}

		void SetColour(FCacheAddress At, const FVector4f& Value)
		{
			check(At.At < OpExecutionData.CodeSize());
			FOpExecutionData* Data = OpExecutionData.GetPtr(At);
			check(Data->DataType == DATATYPE::DT_COLOUR || Data->DataType == DATATYPE::DT_NONE);
			Data->DataType = DATATYPE::DT_COLOUR;
			Data->IsValueValid = true;

			if (!Data->DataTypeIndex)
			{
				Data->DataTypeIndex = ColorResults.Num();
				ColorResults.Add(Value);
			}
			else
			{
				ColorResults[Data->DataTypeIndex] = Value;
			}
			check(Data->DataTypeIndex != 0);
		}

		void SetProjector(FCacheAddress At, const FProjector& Value)
		{
			check(At.At < OpExecutionData.CodeSize());
			FOpExecutionData* Data = OpExecutionData.GetPtr(At);
			check(Data->DataType == DATATYPE::DT_PROJECTOR || Data->DataType == DATATYPE::DT_NONE);
			Data->DataType = DATATYPE::DT_PROJECTOR;
			Data->IsValueValid = true;

			if (!Data->DataTypeIndex)
			{
				Data->DataTypeIndex = ProjectorResults.Num();
				ProjectorResults.Add(Value);
			}
			else
			{
				ProjectorResults[Data->DataTypeIndex] = Value;
			}
			check(Data->DataTypeIndex != 0);
		}

		void SetInstance(FCacheAddress At, Ptr<const Instance> Value)
		{
			check(At.At < OpExecutionData.CodeSize());
			FOpExecutionData* Data = OpExecutionData.GetPtr(At);
			check(Data->DataType == DATATYPE::DT_INSTANCE || Data->DataType == DATATYPE::DT_NONE);
			Data->DataType = DATATYPE::DT_INSTANCE;
			Data->IsValueValid = true;

			if (!Data->DataTypeIndex)
			{
				Data->DataTypeIndex = InstanceResults.Num();
				InstanceResults.Add(Value);
			}
			else
			{
				InstanceResults[Data->DataTypeIndex] = Value;
			}
			check(Data->DataTypeIndex != 0);
		}

		void SetExtensionData(FCacheAddress At, Ptr<const ExtensionData> Value)
		{
			check(At.At < OpExecutionData.CodeSize());
			FOpExecutionData* Data = OpExecutionData.GetPtr(At);
			check(Data->DataType == DATATYPE::DT_EXTENSION_DATA || Data->DataType == DATATYPE::DT_NONE);
			Data->DataType = DATATYPE::DT_EXTENSION_DATA;
			Data->IsValueValid = true;

			if (!Data->DataTypeIndex)
			{
				Data->DataTypeIndex = ExtensionDataResults.Num();
				ExtensionDataResults.Add(Value);
			}
			else
			{
				ExtensionDataResults[Data->DataTypeIndex] = Value;
			}
			check(Data->DataTypeIndex != 0);
		}

		void SetImage(FCacheAddress At, Ptr<const Image> Value)
		{
			check(At.At < OpExecutionData.CodeSize());
			FOpExecutionData* Data = OpExecutionData.GetPtr(At);
			check(Data->DataType == DATATYPE::DT_IMAGE || Data->DataType == DATATYPE::DT_NONE);
			Data->DataType = DATATYPE::DT_IMAGE;
			Data->IsValueValid = true;

			if (!Data->DataTypeIndex)
			{
				Data->DataTypeIndex = ImageResults.Num();
				ImageResults.Add(TResourceResult<Image>{At, Value});
			}
			else
			{
				ImageResults[Data->DataTypeIndex].Value = Value;
			}
			check(Data->DataTypeIndex != 0);

			mu::UpdateLLMStats();
		}

		void SetMesh(FCacheAddress At, Ptr<const Mesh> Value)
		{
			check(At.At < OpExecutionData.CodeSize());

			FOpExecutionData* Data = OpExecutionData.GetPtr(At);

			check(Data->DataType == DATATYPE::DT_MESH || Data->DataType == DATATYPE::DT_NONE);
			Data->DataType = DATATYPE::DT_MESH;
			Data->IsValueValid = true;

			if (!Data->DataTypeIndex)
			{
				Data->DataTypeIndex = MeshResults.Num();
				MeshResults.Add(TResourceResult<Mesh>{At, Value});
			}
			else
			{
				MeshResults[Data->DataTypeIndex].Value = Value;
			}
			check(Data->DataTypeIndex != 0);

			mu::UpdateLLMStats();
		}

		void SetLayout(FCacheAddress At, Ptr<const Layout> Value)
		{
			check(At.At < OpExecutionData.CodeSize());
			FOpExecutionData* Data = OpExecutionData.GetPtr(At);
			check(Data->DataType == DATATYPE::DT_LAYOUT || Data->DataType == DATATYPE::DT_NONE);
			Data->DataType = DATATYPE::DT_LAYOUT;
			Data->IsValueValid = true;

			if (!Data->DataTypeIndex)
			{
				Data->DataTypeIndex = LayoutResults.Num();
				LayoutResults.Add(Value);
			}
			else
			{
				LayoutResults[Data->DataTypeIndex] = Value;
			}
			check(Data->DataTypeIndex != 0);

			mu::UpdateLLMStats();
		}

		void SetString(FCacheAddress At, Ptr<const String> Value)
		{
			check(At.At < OpExecutionData.CodeSize());
			FOpExecutionData* Data = OpExecutionData.GetPtr(At);
			check(Data->DataType == DATATYPE::DT_STRING || Data->DataType == DATATYPE::DT_NONE);
			Data->DataType = DATATYPE::DT_STRING;
			Data->IsValueValid = true;

			if (!Data->DataTypeIndex)
			{
				Data->DataTypeIndex = StringResults.Num();
				StringResults.Add(Value);
			}
			else
			{
				StringResults[Data->DataTypeIndex] = Value;
			}
			check(Data->DataTypeIndex != 0);
		}


		inline void IncreaseHitCount(FCacheAddress At)
		{
			// Don't count hits for instruction 0, which is always null. It is usually already
			// check that At is not 0, and then it is not requested, generating a stray non-zero count
			// at its position.
			if (At.At)
			{
				check(At.At < OpExecutionData.CodeSize());
				FOpExecutionData& Data = OpExecutionData[At];
				Data.OpHitCount++;
			}
		}

		inline void SetForceCached(OP::ADDRESS At)
		{
			// \TODO: It only locks at,0,0
			if (At)
			{
				check(At < OpExecutionData.CodeSize());
				FOpExecutionData& Data = OpExecutionData[FCacheAddress(At, 0, 0)];
				Data.IsCacheLocked = true;
			}
		}
	};

	/** 
	 * Data for an instance that is currently being processed in the mutable system. This means it is
	 * between a BeginUpdate and EndUpdate, or during an "atomic" operation (like generate a single resource).
	 */
	struct FLiveInstance
	{
		Instance::ID InstanceID;
		int32 State = 0;
		Ptr<const Instance> Instance;
		TSharedPtr<const Model> Model;

		Ptr<Parameters> OldParameters;

		/** Mask of the parameters that have changed since the last update.
		* Every bit represents a state parameter.
		*/
		uint64 UpdatedParameters = 0;

		/** Cached data for the generation of this instance. */
		TSharedPtr<FProgramCache> Cache;

		~FLiveInstance()
		{
			// Manually done to trace mem deallocations
			MUTABLE_CPUPROFILER_SCOPE(LiveInstanceDestructor);
			Cache = nullptr;
			OldParameters = nullptr;
			Instance = nullptr;
			Model = nullptr;
		}
	};


    /** Struct to manage all the memory allocated for resources used during mutable operation. */
    struct FWorkingMemoryManager
    {	
		using MemoryCounter = MemoryCounters::FInternalMemoryCounter;

		template<class Type>
		using TMemoryTrackedArray = TArray<Type, FDefaultMemoryTrackingAllocator<MemoryCounter>>;

		template<class KeyType, class ValueType>
		using TMemoryTrackedMap = TMap<KeyType, ValueType, FDefaultMemoryTrackingSetAllocator<MemoryCounter>>;

		/** Cached traking for streamed model data for one model. */
        struct FModelCacheEntry
        {
            /** Model who's data is being tracked. */
            TWeakPtr<const Model> Model;

            /** For each model rom, the last time its streamed data was used. */
            TMemoryTrackedArray<TPair<uint64, uint64>> RomWeights;

			/** Count of pending operations for every rom index. */
			TMemoryTrackedArray<uint16> PendingOpsPerRom;
        };

		//! Management of generated resources
		//! @{

		//! This is used to uniquely identify a generated resource like meshes or images.
		struct FGeneratedResourceData
		{
			/** Model for this resource. */
			TWeakPtr<const Model> Model;

			//! The id assigned to the generated resource.
			FResourceID Id;

			//! The last request operation for this resource
			uint32 LastRequestId;

			//! An opaque blob with the values of the relevant parameters
			TMemoryTrackedArray<uint8> ParameterValuesBlob;
		};

		//! The last id generated for a resource
		uint32 LastResourceKeyId = 0;

		//! The last id generated for a resource request. This is used to check the
		//! relevancy of the resources when flushing the cache
		uint32 LastResourceResquestId = 0;

		//! Cached ids for returned assets
		//! This is non-persistent runtime data
		TMemoryTrackedArray<FGeneratedResourceData> GeneratedResources;

		/** */
		FResourceID GetResourceKey(const TSharedPtr<const Model>&, const Parameters*, uint32 ParamListIndex, OP::ADDRESS RootAt);

		//! @}

		/** Maximum working memory that mutable should be using. */
		int64 BudgetBytes = 0;

		/** Maximum excess memory reached suring the current operation. */
		int64 BudgetExcessBytes = 0;

		/** Maximum number of resource keys that will be stored for resource reusal. */
		int32 MaxGeneratedResourceCacheSize = 1024;


		/** This value is used to track the order of loading of roms. */
        uint64 RomTick = 0;

		/** Control info for the per-model cache of streamed data. */
        TMemoryTrackedArray<FModelCacheEntry> CachePerModel;

		/** Data for each mutable instance that is being updated. */
		TMemoryTrackedArray<FLiveInstance> LiveInstances;

		/** Temporary reference to the memory of the current instance being updated. Only valid during a mutable "atomic" operation, like a BeginUpdate or a GetImage. */
		TSharedPtr<FProgramCache> CurrentInstanceCache;

		/** Resources that have been used in the past, but haven't been deallocated because they still fitted the memory budget and they could be reused. */
		TMemoryTrackedArray<Ptr<Image>> PooledImages;

		/** List of intermediate resources that are not soterd anywhere yet. They are still locally referenced by code. */
		TMemoryTrackedArray<Ptr<const Image>> TempImages;
		TMemoryTrackedArray<Ptr<const Mesh>> TempMeshes;

		/** List of resources that are currently in any cache position, and the number of positions they are in. */
		TMemoryTrackedMap<Ptr<const Resource>, int32> CacheResources;

		/** Given a mutable model, find or create its rom cache. */
		FModelCacheEntry* FindModelCache(const Model*);
		FModelCacheEntry& FindOrAddModelCache(const TSharedPtr<const Model>&);

        /** Make sure the working memory is below the internal budget, even counting with the passed additional memory. 
		* An optional function can be passed to "block" the unload of certain roms of data.
		* Return true if it succeeded, false otherwise.
		*/
		bool EnsureBudgetBelow(uint64 AdditionalMemory);
		
		/** Return true if the memory budget is 90% full. */
		bool IsMemoryBudgetFull() const;

		/** Calculate the current usage of memory as used to calculate the budget. */
		int64 GetCurrentMemoryBytes() const;

        /** Register that a specific rom has been requested and update the heuristics to keep it in memory. */
        void MarkRomUsed( int32 RomIndex, const TSharedPtr<const Model>& );

		/** */
		[[nodiscard]] Ptr<Image> CreateImage(uint32 SizeX, uint32 SizeY, uint32 Lods, EImageFormat Format, EInitializationType Init)
		{
			CheckRunnerThread();

			uint32 DataSize = Image::CalculateDataSize(SizeX, SizeY, Lods, Format);

			// Look for an unused image in the pool that can be reused
			int32 PooledImageCount = PooledImages.Num();
			for (int32 Index = 0; DataSize > 0 && Index < PooledImageCount; ++Index)
			{
				Ptr<Image>& Candidate = PooledImages[Index];
				if (Candidate->GetFormat() == Format
					&& Candidate->GetSizeX() == SizeX
					&& Candidate->GetSizeY() == SizeY
					&& Candidate->GetLODCount() == Lods
					)
				{
					Ptr<Image> Result = Candidate;
					PooledImages.RemoveAtSwap(Index);
					
					if (Init == EInitializationType::Black)
					{
						Result->InitToBlack();
					}
					else
					{
						Result->m_flags = 0;
						Result->RelevancyMinY = 0;
						Result->RelevancyMaxY = 0;
					}
					return Result;
				}
			}

			// Make room in the budget
			EnsureBudgetBelow(DataSize);

			// Create it
			Ptr<Image> Result = new Image(SizeX, SizeY, Lods, Format, Init);

			TempImages.Add(Result);
			return Result;
		}

		/** Ref will be nulled and relesed in any case. */
		[[nodiscard]] Ptr<Image> CloneOrTakeOver(Ptr<const Image>& Resource)
		{
			CheckRunnerThread();

			TempImages.RemoveSingle(Resource);
			
			check(!TempImages.Contains(Resource));
			check(!PooledImages.Contains(Resource));

			Ptr<Image> Result;
			if (!Resource->IsUnique())
			{
				// TODO: try to grab from the pool

				uint32 DataSize = Resource->GetDataSize();
				EnsureBudgetBelow(DataSize);

				Result = Resource->Clone();
				Release(Resource);
			}
			else
			{
				Result = const_cast<Image*>(Resource.get());
				Resource = nullptr;
			}

			return Result;
		}

		/** */
		void Release(Ptr<const Image>& Resource)
		{
			CheckRunnerThread();

			if (!Resource)
			{
				return;
			}

			const int32 ResourceDataSize = Resource->GetDataSize();
			TempImages.RemoveSingle(Resource);

			check(!TempImages.Contains(Resource));
			check(!PooledImages.Contains(Resource));

			if (IsBudgetTemp(Resource))
			{
				// Check if we are exceeding the budget
				bool bInBudget = EnsureBudgetBelow(ResourceDataSize);
				if (bInBudget)
				{
					PooledImages.Add(const_cast<Image*>(Resource.get()));
				}
			}
			else
			{
				// Check if we are exceeding the budget
				EnsureBudgetBelow(0);
			}

			Resource = nullptr;
		}

		/** */
		void Release(Ptr<Image>& Resource)
		{
			CheckRunnerThread();

			if (!Resource)
			{
				return;
			}

			const int32 ResourceDataSize = Resource->GetDataSize();
			TempImages.RemoveSingle(Resource);

			check(!TempImages.Contains(Resource));
			check(!PooledImages.Contains(Resource));

			if (IsBudgetTemp(Resource))
			{
				// Check if we are exceeding the budget
				bool bInBudget = EnsureBudgetBelow(ResourceDataSize);
				if (bInBudget)
				{
					PooledImages.Add(Resource.get());
				}
			}
			else
			{
				// Check if we are exceeding the budget
				EnsureBudgetBelow(0);
			}

			Resource = nullptr;
		}

		[[nodiscard]] Ptr<Mesh> CreateMesh(int32 BudgetReserveSize)
		{
			CheckRunnerThread();

			EnsureBudgetBelow(BudgetReserveSize);

			Ptr<Mesh> Result = new Mesh();

			TempMeshes.Add(Result);

			return Result;
		}

		[[nodiscard]] Ptr<Mesh> CloneOrTakeOver(Ptr<const Mesh>& Resource)
		{
			CheckRunnerThread();

			const int32 ResourceDataSize = Resource->GetDataSize();
			TempMeshes.RemoveSingle(Resource);

			Ptr<Mesh> Result;
			if (!Resource->IsUnique())
			{
				Result = CreateMesh(ResourceDataSize); 
				Result->CopyFrom(*Resource);
				Release(Resource);
			}
			else
			{
				Result = Ptr<Mesh>(const_cast<Mesh*>(Resource.get()));
				Resource = nullptr;
			}

			return Result;
		}

		void Release(Ptr<const Mesh>& Resource)
		{
			CheckRunnerThread();

			if (!Resource)
			{
				return;
			}

			TempMeshes.RemoveSingle(Resource);
			check(!TempMeshes.Contains(Resource));

			EnsureBudgetBelow(0);

			Resource = nullptr;
		}

		void Release(Ptr<Mesh>& Resource)
		{
			Ptr<const Mesh> ConstPtr = Resource;

			Release(ConstPtr);

			Resource = nullptr;
		}

		/** */
		[[nodiscard]] Ptr<const Mesh> LoadMesh(const FCacheAddress& From, bool bTakeOwnership = false)
		{
			bool bIsLastReference = false;
			Ptr<const Mesh> Result = CurrentInstanceCache->GetMesh(From, bIsLastReference);
			if (!Result)
			{
				return nullptr;
			}

			// If we retrieved the last reference to this resource in "From" cache position (it could still be in other cache positions as well)
			if (bIsLastReference)
			{
				int32* CountPtr = CacheResources.Find(Result);
				check(CountPtr);
				*CountPtr = (*CountPtr) - 1;
				if (!*CountPtr)
				{
					CacheResources.FindAndRemoveChecked(Result);
				}
			}

			if (!bTakeOwnership && Result->IsUnique())
			{
				TempMeshes.Add(Result);
			}

			return Result;
		}

		/** */
		[[nodiscard]] Ptr<const Image> LoadImage(const FCacheAddress& From, bool bTakeOwnership = false)
		{
			bool bIsLastReference = false;
			Ptr<const Image> Result = CurrentInstanceCache->GetImage(From, bIsLastReference);
			if (!Result)
			{
				return nullptr;
			}

			// If we retrieved the last reference to this resource in "From" cache position (it could still be in other cache positions as well)
			if (bIsLastReference)
			{
				int32* CountPtr = CacheResources.Find(Result);
				check(CountPtr);
				*CountPtr = (*CountPtr) - 1;
				if (!*CountPtr)
				{
					CacheResources.FindAndRemoveChecked(Result);
				}
			}


			if (!bTakeOwnership && Result->IsUnique())
			{
				TempImages.Add(Result);
			}

			return Result;
		}

		/** */
		void StoreImage(const FCacheAddress& To, Ptr<const Image> Resource)
		{
			if (Resource)
			{
				int32 ResourceDataSize = Resource->GetDataSize();
				TempImages.RemoveSingle(Resource);

				check(!TempImages.Contains(Resource));

				int32& Count = CacheResources.FindOrAdd(Resource, 0);
				++Count;
			}

			CurrentInstanceCache->SetImage(To, Resource);
		}

		void StoreMesh(const FCacheAddress& To, Ptr<const Mesh> Resource)
		{
			if (Resource)
			{
				//Resource->CheckIntegrity();

				TempMeshes.RemoveSingle(Resource);

				int32& Count = CacheResources.FindOrAdd(Resource, 0);
				++Count;
			}

			CurrentInstanceCache->SetMesh(To, Resource);
		}

		/** Return true if the resource is not in any cache (0,1,rom). */
		bool IsBudgetTemp(const Ptr<const Resource>& Resource)
		{
			if (!Resource)
			{
				return false;
			}

			bool bIsTemp = Resource->IsUnique();
			return bIsTemp;
		}

		int32 GetPooledBytes() const
		{
			int32 Result = 0;
			for (const Ptr<Image>& Value : PooledImages)
			{
				Result += Value->GetDataSize();
			}

			return Result;
		}

		int32 GetTempBytes() const
		{
			int32 Result = 0;
			for (const Ptr<const Image>& Value : TempImages)
			{
				Result += Value->GetDataSize();
			}

			for (const Ptr<const Mesh>& Value : TempMeshes)
			{
				Result += Value->GetDataSize();
			}

			return Result;
		}


		int32 GetRomBytes() const
		{
			int32 Result = 0;

			TArray<const Model*> Models;
			for (const FLiveInstance& Instance : LiveInstances)
			{
				Models.AddUnique(Instance.Model.Get());
			}

			// Data stored per-model, but related to instance construction
			for (const Model* Model : Models)
			{
				// Count streamable and currently-loaded resources
				const FProgram& Program = Model->GetPrivate()->m_program;
				for (const TPair<int32, Ptr<const Image>>& Rom : Program.ConstantImageLODs)
				{
					if (Rom.Value && Rom.Key >= 0)
					{
						Result += Rom.Value->GetDataSize();
					}
				}
				for (const TPair<int32, Ptr<const Mesh>>& Rom : Program.ConstantMeshData)
				{
					if (Rom.Value && Rom.Key >= 0)
					{
						Result += Rom.Value->GetDataSize();
					}
				}
			}

			return Result;
		}


		int32 GetTrackedCacheBytes() const
		{
			int32 Result = 0;
			for (const TTuple<Ptr<const Resource>,int32>& It : CacheResources)
			{
				Result += It.Key->GetDataSize();
			}

			return Result;
		}

		/** Calculate the amount of bytes in data cached in the level 0 and 1 cache in all live instances. */
		int32 GetCacheBytes() const
		{
			int32 Result = 0;
			TSet<const Resource*> Cache0Unique;
			TSet<const Resource*> Cache1Unique;

			for (const FLiveInstance& Instance : LiveInstances)
			{
				CodeContainer<FProgramCache::FOpExecutionData>::TIterator Iter = Instance.Cache->OpExecutionData.Begin();
				for (; Iter.IsValid(); ++Iter)
				{
					FProgramCache::FOpExecutionData& Data = *Iter;

					if (!Data.DataTypeIndex)
					{
						continue;
					}

					TSet<const Resource*>* TargetSet = &Cache0Unique;
					if (Data.IsCacheLocked)
					{
						TargetSet = &Cache1Unique;
					}

					const Resource* Value = nullptr;
					switch (Data.DataType)
					{
					case DATATYPE::DT_IMAGE:
						Value = Instance.Cache->ImageResults[Data.DataTypeIndex].Value.get();
						if (Value)
						{
							TargetSet->Add(Value);
						}
						break;

					case DATATYPE::DT_MESH:
						Value = Instance.Cache->MeshResults[Data.DataTypeIndex].Value.get();
						if (Value)
						{
							TargetSet->Add(Value);
						}
						break;

					default:
						break;
					}
				}
			}

			// Merge
			Cache0Unique.Append(Cache1Unique);

			// Count
			for (const Resource* Value : Cache0Unique)
			{
				Result += Value->GetDataSize();
			}

			return Result;
		}

		/** Remove all intermediate data (big and small) from the memory except for the one that has been explicitely
		* marked as state cache.
		*/
		void ClearCacheLayer0()
		{
			check(CurrentInstanceCache);

			MUTABLE_CPUPROFILER_SCOPE(ClearLayer0);

			CodeContainer<FProgramCache::FOpExecutionData>::TIterator Iter = CurrentInstanceCache->OpExecutionData.Begin();
			for (; Iter.IsValid(); ++Iter)
			{
				FProgramCache::FOpExecutionData& Data = *Iter;
				
				if (!Data.DataTypeIndex || Data.IsCacheLocked)
				{
					continue;
				}

				const Resource* Value = nullptr;

				switch (Data.DataType)
				{
				case DATATYPE::DT_IMAGE:
					Value = CurrentInstanceCache->ImageResults[Data.DataTypeIndex].Value.get();
					if (Value)
					{
						CacheResources.Remove(Value);
						CurrentInstanceCache->ImageResults[Data.DataTypeIndex].Value = nullptr;
					}
					break;

				case DATATYPE::DT_MESH:
					Value = CurrentInstanceCache->MeshResults[Data.DataTypeIndex].Value.get();
					if (Value)
					{
						CacheResources.Remove(Value);
						CurrentInstanceCache->MeshResults[Data.DataTypeIndex].Value = nullptr;
					}
					break;

				case DATATYPE::DT_LAYOUT:
					Value = CurrentInstanceCache->LayoutResults[Data.DataTypeIndex].get();
					CurrentInstanceCache->LayoutResults[Data.DataTypeIndex] = nullptr;
					break;

				case DATATYPE::DT_INSTANCE:
					Value = CurrentInstanceCache->InstanceResults[Data.DataTypeIndex].get();
					CurrentInstanceCache->InstanceResults[Data.DataTypeIndex] = nullptr;
					break;

				default:
					break;
				}

				Data.OpHitCount = 0;
				Data.IsValueValid = false;
			}
		}


		/** Remove all intermediate data (big and small) from the memory including the one that has been explicitely
		* marked as state cache.
		*/
		void ClearCacheLayer1()
		{
			MUTABLE_CPUPROFILER_SCOPE(ClearLayer1);

			CodeContainer<FProgramCache::FOpExecutionData>::TIterator Iter = CurrentInstanceCache->OpExecutionData.Begin();
			for (; Iter.IsValid(); ++Iter)
			{
				FProgramCache::FOpExecutionData& Data = *Iter;

				if (!Data.DataTypeIndex)
				{
					continue;
				}

				const Resource* Value = nullptr;

				switch (Data.DataType)
				{
				case DATATYPE::DT_IMAGE:
					Value = CurrentInstanceCache->ImageResults[Data.DataTypeIndex].Value.get();
					CacheResources.Remove(Value);
					CurrentInstanceCache->ImageResults[Data.DataTypeIndex].Value = nullptr;
					break;

				case DATATYPE::DT_MESH:
					Value = CurrentInstanceCache->MeshResults[Data.DataTypeIndex].Value.get();
					CacheResources.Remove(Value);
					CurrentInstanceCache->MeshResults[Data.DataTypeIndex].Value = nullptr;
					break;

				case DATATYPE::DT_LAYOUT:
					Value = CurrentInstanceCache->LayoutResults[Data.DataTypeIndex].get();
					CurrentInstanceCache->LayoutResults[Data.DataTypeIndex] = nullptr;
					break;

				case DATATYPE::DT_INSTANCE:
					Value = CurrentInstanceCache->InstanceResults[Data.DataTypeIndex].get();
					CurrentInstanceCache->InstanceResults[Data.DataTypeIndex] = nullptr;
					break;

				default:
					break;
				}

				Data.OpHitCount = 0;
				Data.IsValueValid = false;
			}
		}


		void LogWorkingMemory(const class CodeRunner* CurrentRunner) const;


#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		static constexpr uint64 InvalidRunnerId = 0;
		std::atomic<uint64> DebugCurrentRunnerId {1};
		
		/** Temp variable with the ID of the thread running code, for debugging. */
		uint32 DebugRunnerThreadID = FThread::InvalidThreadId;
		uint64 DebugRunnerID = InvalidRunnerId;
#endif

		/** This is a development-only check to make sure calls to resource management happen in the correct thread. */
		FORCEINLINE void BeginRunnerThread()
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			// If this check fails it means have not set up correctly all the paths to debug threading for resource management.
			check(DebugRunnerThreadID == FThread::InvalidThreadId);
			check(DebugRunnerID == InvalidRunnerId)

			DebugRunnerThreadID = FPlatformTLS::GetCurrentThreadId();
			DebugRunnerID = ++DebugCurrentRunnerId;
#endif
		}

		/** This is a development-only check to make sure calls to resource management happen in the correct thread. */
		FORCEINLINE void ResetRunnerThread()
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			// If this check fails it means have not set up correctly all the paths to debug threading for resource management.
			check(DebugRunnerThreadID == FThread::InvalidThreadId);
			check(DebugRunnerID != InvalidRunnerId)

			DebugRunnerThreadID = FPlatformTLS::GetCurrentThreadId();
#endif
		}

		/** This is a development-only check to make sure calls to resource management happen in the correct thread. */
		FORCEINLINE void InvalidateRunnerThread()
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			// If this check fails it means have not set up correctly all the paths to debug threading for resource management.
			check(DebugRunnerThreadID != FThread::InvalidThreadId);
			check(DebugRunnerID != InvalidRunnerId)

			DebugRunnerThreadID = FThread::InvalidThreadId;
#endif
		}

		/** This is a development-only check to make sure calls to resource management happen in the correct thread. */
		FORCEINLINE void CheckRunnerThread()
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			// If this check fails it means have not set up correctly all the paths to debug threading for resource management.
			check(DebugRunnerThreadID != FThread::InvalidThreadId);
			check(DebugRunnerID != InvalidRunnerId);

			// If this check fails it means we are doing resource management from a thread that is not the CodeRunner::RunCode
			// thread, and this is not allowed.
			check(DebugRunnerThreadID == FPlatformTLS::GetCurrentThreadId());
			check(DebugRunnerID == DebugCurrentRunnerId);
#endif
		}


		/** This is a development-only check to make sure calls to resource management happen in the correct thread. */
		FORCEINLINE void EndRunnerThread()
		{
			CurrentInstanceCache->CheckHitCountsCleared();

			// If this check fails it means some operation is not correctly handling resource management and didn't release
			// a resource it created.
			// This should be reported and reviewed, but it is not fatal. Some unnecessary memory may be used temporarily.
			ensure(TempImages.Num() == 0);
			ensure(TempMeshes.Num() == 0);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			// If this check fails it means have not set up correctly all the paths to debug threading for resource management.
			check(DebugRunnerThreadID != FThread::InvalidThreadId);
			check(DebugRunnerID != InvalidRunnerId);

			DebugRunnerThreadID = FThread::InvalidThreadId;
			DebugRunnerID = InvalidRunnerId;
#endif
		}

	};


	/** */
    class System::Private
    {
		friend class System;
    public:

        Private( Ptr<Settings>, const TSharedPtr<ExtensionDataStreamer>& );
        ~Private();

        //-----------------------------------------------------------------------------------------
        //! Own interface
        //-----------------------------------------------------------------------------------------

		/** This method can be used to internally prepare for code execution. */
		MUTABLERUNTIME_API void BeginBuild(const TSharedPtr<const Model>&);
		MUTABLERUNTIME_API void EndBuild();

		MUTABLERUNTIME_API bool BuildBool(const TSharedPtr<const Model>&, const Parameters*, OP::ADDRESS);
		MUTABLERUNTIME_API int32 BuildInt(const TSharedPtr<const Model>&, const Parameters*, OP::ADDRESS);
		MUTABLERUNTIME_API float BuildScalar(const TSharedPtr<const Model>&, const Parameters*, OP::ADDRESS);
		MUTABLERUNTIME_API FVector4f BuildColour(const TSharedPtr<const Model>&, const Parameters*, OP::ADDRESS);
		MUTABLERUNTIME_API Ptr<const String> BuildString(const TSharedPtr<const Model>&, const Parameters*, OP::ADDRESS);
		MUTABLERUNTIME_API Ptr<const Image> BuildImage(const TSharedPtr<const Model>&, const Parameters*, OP::ADDRESS, int32 MipsToSkip, int32 LOD);
		MUTABLERUNTIME_API Ptr<const Mesh> BuildMesh(const TSharedPtr<const Model>&, const Parameters*, OP::ADDRESS, uint8 ExecutionOptions = 0);
		MUTABLERUNTIME_API Ptr<const Instance> BuildInstance(const TSharedPtr<const Model>&, const Parameters*, OP::ADDRESS);
		MUTABLERUNTIME_API Ptr<const Layout> BuildLayout(const TSharedPtr<const Model>&, const Parameters*, OP::ADDRESS);
    	MUTABLERUNTIME_API FProjector BuildProjector(const TSharedPtr<const Model>&, const Parameters*, OP::ADDRESS);

		ExtensionDataStreamer* GetExtensionDataStreamer() const { return ExtensionDataStreamer.Get(); }

        //!
        Ptr<const Settings> Settings;

        //! Data streaming interface, if any.
		TSharedPtr<ModelReader> StreamInterface;

		TSharedPtr<ExternalResourceProvider> ExternalResourceProvider;

		/** Counter used to generate unique IDs for every new instance created in the system. */
        Instance::ID LastInstanceID = 0;

	private:
		/** This flag is turned on when a streaming error or similar happens. Results are not usable.
		* This should only happen in-editor.
		*/
		bool bUnrecoverableError = false;

	public:
		/** If this is set, it will be tried first instead of the internal formatting function. */
		FImageOperator::FImagePixelFormatFunc ImagePixelFormatOverride;

		/** */
		FWorkingMemoryManager WorkingMemoryManager;

		/** The pointer returned by this function is only valid for the duration of the current mutable operation. */
		inline FLiveInstance* FindLiveInstance(Instance::ID id);

        //!
        bool CheckUpdatedParameters( const FLiveInstance*, const Ptr<const Parameters>&, uint64& OutUpdatedParameters );


		void RunCode(const TSharedPtr<const Model>&, const Parameters*, OP::ADDRESS at, uint32 LODs = System::AllLODs, uint8 ExecutionOptions = 0, int32 LOD = 0);

		//!
		void PrepareCache(const Model*, int32 State);

	private:

		/** Owned by this system. */
		TSharedPtr<ExtensionDataStreamer> ExtensionDataStreamer = nullptr;
    };

}
