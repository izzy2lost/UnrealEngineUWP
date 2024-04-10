// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Containers/StringView.h"
#include "VVMAux.h"
#include "VVMEmergentTypeCreator.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMHeap.h"
#include "VerseVM/VVMLog.h"

namespace Verse
{

struct VInt;
struct FOpResult;

struct VArrayBase : VHeapValue
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VHeapValue);

protected:
	// Our Aux memory buffer is typed as void so we can accomodate all the types listed in ::EArrayType
	TWriteBarrier<TAux<void>> Values;
	uint32 NumValues;

	void SetArrayType(EArrayType ArrayType)
	{
		Misc3 &= ~(static_cast<uint8_t>(GetArrayType())); // Clear any existing type
		Misc3 |= static_cast<uint8_t>(ArrayType);
	}

	static EArrayType DetermineArrayType(VValue Value)
	{
		if (Value.IsInt32())
		{
			return EArrayType::Int32;
		}
		if (Value.IsChar())
		{
			return EArrayType::Char8;
		}
		if (Value.IsChar32())
		{
			return EArrayType::Char32;
		}
		return EArrayType::VValue;
	}

	static EArrayType DetermineCombinedType(EArrayType A, EArrayType B)
	{
		return A == B ? A : EArrayType::VValue;
	}

	static size_t ByteLength(EArrayType ArrayType)
	{
		switch (ArrayType)
		{
			case EArrayType::None:
				return 0; // Empty-Untyped VMutableArray
			case EArrayType::VValue:
				return sizeof(TWriteBarrier<VValue>);
			case EArrayType::Int32:
				return sizeof(int32);
			case EArrayType::Char8:
				return sizeof(uint8);
			case EArrayType::Char32:
				return sizeof(uint32);
			default:
				V_DIE("Unhandled EArrayType encountered!");
		}
	}

	VArrayBase(FAllocationContext Context, uint32 InNumValues, EArrayType ArrayType, VEmergentType* Type)
		: VHeapValue(Context, Type)
		, NumValues(InNumValues)
	{
		SetIsDeeplyMutable();
		if (ArrayType != EArrayType::None)
		{
			AllocateBuffer(Context, ArrayType, NumValues);
		}
	}

	VArrayBase(FAllocationContext Context, std::initializer_list<VValue> InitList, VEmergentType* Type)
		: VHeapValue(Context, Type)
		, NumValues(InitList.size())
	{
		SetIsDeeplyMutable();
		if (NumValues)
		{
			AllocateBuffer(Context, DetermineArrayType(*InitList.begin()), NumValues);
			uint32 Index = 0;
			for (const VValue& Value : InitList)
			{
				SetValue(Context, Index++, Value, &NumValues);
			}
		}
	}

	template <typename InitIndexFunc>
	VArrayBase(FAllocationContext Context, uint32 InNumValues, InitIndexFunc&& InitFunc, VEmergentType* Type)
		: VHeapValue(Context, Type)
		, NumValues(InNumValues)
	{
		SetIsDeeplyMutable();
		if (NumValues)
		{
			AllocateBuffer(Context, DetermineArrayType(InitFunc(0)), NumValues);
			for (uint32 Index = 0; Index < NumValues; ++Index)
			{
				SetValue(Context, Index, InitFunc(Index), &NumValues);
			}
		}
	}

	VArrayBase(FAllocationContext Context, FUtf8StringView String, VEmergentType* Type)
		: VHeapValue(Context, Type)
		, NumValues(String.Len())
	{
		SetIsDeeplyMutable();
		AllocateBuffer(Context, EArrayType::Char8, NumValues);
		FMemory::Memcpy(GetData(), String.GetData(), String.Len());
	}

	void AllocateBuffer(FAllocationContext Context, EArrayType ArrayType, uint32 Capacity)
	{
		checkSlow(!GetData());
		SetArrayType(ArrayType);

		if (IsString())
		{
			// If we are UTF8, add +1 to capacity and set a null-terminator
			TAux<void> NewValues = TAux<void>(Context.AllocateAuxCell(ByteLength(ArrayType) * (Capacity + 1)));
			Values.Set(Context, NewValues);
			SetChar(Num(), static_cast<UTF8CHAR>(0));
		}
		else
		{
			TAux<void> NewValues = TAux<void>(Context.AllocateAuxCell(ByteLength(ArrayType) * Capacity));
			Values.Set(Context, NewValues);
		}
	}

	void SetNullTerminator()
	{
		SetChar(Num(), static_cast<UTF8CHAR>(0));
	}

	void ConvertDataToVValues(FAllocationContext Context, const uint32* Capacity);

	template <typename T>
	static void Serialize(T*& This, FAllocationContext Context, FAbstractVisitor& Visitor);

public:
	uint32 Num() const { return NumValues; }
	bool IsInBounds(uint32 Index) const;
	bool IsInBounds(const VInt& Index, const uint32 Bounds) const;
	VValue GetValue(uint32 Index);

	/// Capacity parameter is required for handling when a re-allocation to VValues takes place during SetValue from a VMutableArray.
	void SetValue(FAllocationContext Context, uint32 Index, VValue Value, const uint32* Capacity = nullptr);
	void SetVValue(FAllocationContext Context, uint32 Index, VValue Value)
	{
		checkSlow(GetArrayType() == EArrayType::VValue);
		new (&BitCast<TAux<TWriteBarrier<VValue>>>(Values.Get())[Index]) TWriteBarrier<VValue>(Context, Value);
	}
	void SetInt32(uint32 Index, int32 Value)
	{
		checkSlow(GetArrayType() == EArrayType::Int32);
		new (&BitCast<TAux<int32>>(Values.Get())[Index]) int32(Value);
	}
	void SetChar(uint32 Index, uint8 Value)
	{
		checkSlow(GetArrayType() == EArrayType::Char8);
		new (&BitCast<TAux<uint8>>(Values.Get())[Index]) uint8(Value);
	}
	void SetChar32(uint32 Index, uint32 Value)
	{
		checkSlow(GetArrayType() == EArrayType::Char32);
		new (&BitCast<TAux<uint32>>(Values.Get())[Index]) uint32(Value);
	}

	void* GetData() { return Values.Get().GetPtr(); };
	const void* GetData() const { return Values.Get().GetPtr(); };
	template <typename T>
	T* GetData() { return BitCast<TAux<T>>(Values.Get()).GetPtr(); }
	template <typename T>
	const T* GetData() const { return BitCast<TAux<T>>(Values.Get()).GetPtr(); }

	size_t ByteLength()
	{
		return Num() * ByteLength(GetArrayType());
	}

	bool IsString() const
	{
		return GetArrayType() == EArrayType::Char8;
	}

	FString AsString() const
	{
		if (IsString())
		{
			return FString(GetData<UTF8CHAR>());
		}
		V_DIE("Array is not UTF8!");
		return FString();
	}

	FUtf8StringView AsStringView() const
	{
		if (IsString())
		{
			return FUtf8StringView(GetData<UTF8CHAR>());
		}
		V_DIE("Array is not UTF8!");
		return FUtf8StringView();
	}

	bool Equals(const FUtf8StringView String) const
	{
		if (IsString())
		{
			return AsStringView().Equals(String);
		}
		return false;
	}

	COREUOBJECT_API bool EqualImpl(FRunningContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder);

	COREUOBJECT_API VValue MeltImpl(FRunningContext Context);

	COREUOBJECT_API uint32 GetTypeHashImpl();

	COREUOBJECT_API void ToStringImpl(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter);

	// C++ ranged-based iteration
	class FConstIterator
	{
		union
		{
			const TWriteBarrier<VValue>* Barrier;
			const int32* Int32;
			const uint8* Char8;
			const uint32* Char32;
			const void* None;
		};
		EArrayType ArrayType;

	public:
		FORCEINLINE VValue operator*() const
		{
			switch (ArrayType)
			{
				case EArrayType::VValue:
					return Barrier->Get();
				case EArrayType::Int32:
					return VValue::FromInt32(*Int32);
				case EArrayType::Char8:
					return VValue::Char(*Char8);
				case EArrayType::Char32:
					return VValue::Char32(*Char32);
				default:
					V_DIE("Unhandled EArrayType encountered!");
			}
		}

		// Don't need to worry about the data-type here as we are just doing pointer comparison
		FORCEINLINE bool operator==(const FConstIterator& Rhs) const { return Barrier == Rhs.Barrier; }
		FORCEINLINE bool operator!=(const FConstIterator& Rhs) const { return Barrier != Rhs.Barrier; }

		FORCEINLINE FConstIterator& operator++()
		{
			switch (ArrayType)
			{
				case EArrayType::VValue:
					++Barrier;
					break;
				case EArrayType::Int32:
					++Int32;
					break;
				case EArrayType::Char8:
					++Char8;
					break;
				case EArrayType::Char32:
					++Char32;
					break;
				default:
					V_DIE("Unhandled EArrayType encountered!");
			}
			return *this;
		}

	private:
		friend struct VArrayBase;
		FORCEINLINE FConstIterator(const TWriteBarrier<VValue>* InCurrentValue)
			: Barrier(InCurrentValue)
			, ArrayType(EArrayType::VValue) {}
		FORCEINLINE FConstIterator(const int32* InCurrentValue)
			: Int32(InCurrentValue)
			, ArrayType(EArrayType::Int32) {}
		FORCEINLINE FConstIterator(const uint8* InCurrentValue)
			: Char8(InCurrentValue)
			, ArrayType(EArrayType::Char8) {}
		FORCEINLINE FConstIterator(const uint32* InCurrentValue)
			: Char32(InCurrentValue)
			, ArrayType(EArrayType::Char32) {}
		FORCEINLINE FConstIterator(const void* InCurrentValue)
			: None(InCurrentValue)
			, ArrayType(EArrayType::None) {}
	};

	COREUOBJECT_API FConstIterator begin() const;
	COREUOBJECT_API FConstIterator end() const;
};

} // namespace Verse
#endif // WITH_VERSE_VM
