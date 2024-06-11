// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Memory/MemoryFwd.h"
#include "Memory/MemoryView.h"
#include "PlainPropsCtti.h"
#include "PlainPropsDeclare.h"
#include "PlainPropsRead.h"
#include "PlainPropsTypes.h"

namespace PlainProps 
{

struct FBuiltRange;
struct FBuiltStruct;
class FIdIndexerBase;
struct FLoadBatch;
class FMemberBuilder;
struct FSchemaBatch;
class FScratchAllocator;
class FStructBinding;
class FRangeBinding;
struct FSaveContext;
struct FTypedRange;
class IItemRangeBinding;
template<class T> class TIdIndexer;

////////////////////////////////////////////////////////////////////////////////////////////////

enum class ELeafBindType : uint8 { Bool, IntS, IntU, Float, Hex, Enum, Unicode, BitfieldBool };

inline static constexpr ELeafBindType ToLeafBindType(ELeafType Type)
{
	return static_cast<ELeafBindType>(static_cast<uint8>(Type));
}

inline static constexpr ELeafType ToLeafType(ELeafBindType Type)
{
	return Type == ELeafBindType::BitfieldBool ? ELeafType::Bool : static_cast<ELeafType>(static_cast<uint8>(Type));
}

struct FArithmeticBindType
{
    EMemberKind		_ : 2;
	ELeafBindType	__ : 3;
	ELeafWidth		Width : 2;
};

struct FBitfieldBindType
{
    EMemberKind		_ : 2;
	ELeafBindType	__ : 3;
	uint8			Idx : 3;
};

union FLeafBindType
{
	constexpr explicit FLeafBindType(ELeafBindType ArithmeticType, ELeafWidth Width) : Arithmetic({EMemberKind::Leaf, ArithmeticType, Width}) {}
	constexpr explicit FLeafBindType(FUnpackedLeafType In) : Arithmetic({EMemberKind::Leaf, ToLeafBindType(In.Type), In.Width}) {}
	constexpr explicit FLeafBindType(FLeafType In) : FLeafBindType(FUnpackedLeafType(In)) {}
	constexpr explicit FLeafBindType(FBitfieldBindType In) : Bitfield({EMemberKind::Leaf, ELeafBindType::BitfieldBool, In.Idx}) {}
	constexpr explicit FLeafBindType(uint8 BitfieldIdx) : Bitfield({EMemberKind::Leaf, ELeafBindType::BitfieldBool, BitfieldIdx}) {}

	struct
	{
		EMemberKind			_ : 2;
		ELeafBindType		Type : 3;
	}						Bind;
	FArithmeticBindType		Arithmetic;
	FBitfieldBindType		Bitfield;
};

inline static constexpr FLeafType ToLeafType(FLeafBindType Leaf)
{
	if (Leaf.Bind.Type == ELeafBindType::BitfieldBool)
	{
		return { EMemberKind::Leaf, ELeafWidth::B8, ELeafType::Bool };
	}
	
	return { EMemberKind::Leaf,  Leaf.Arithmetic.Width, ToLeafType(Leaf.Bind.Type) };
}

struct FRangeBindType : FRangeType {};

struct FStructBindType : FStructType {};

union FMemberBindType
{
	constexpr explicit FMemberBindType(FLeafType In) : Leaf(In) {}
	constexpr explicit FMemberBindType(FUnpackedLeafType In) : Leaf(In) {}
	constexpr explicit FMemberBindType(FLeafBindType In) : Leaf(In) {}
	constexpr explicit FMemberBindType(FBitfieldBindType In) : Leaf(In) {}
	constexpr explicit FMemberBindType(FRangeType In) : Range(In) {}
	constexpr explicit FMemberBindType(ERangeSizeType MaxSize) : Range({EMemberKind::Range, MaxSize}) {}
	constexpr explicit FMemberBindType(FStructType In) : Struct(In) {}
	
	bool					IsLeaf() const		{ return Kind == EMemberKind::Leaf; }
	bool					IsRange() const		{ return Kind == EMemberKind::Range; }
	bool					IsStruct() const	{ return Kind == EMemberKind::Struct; }
	EMemberKind				GetKind() const		{ return Kind; }
	
	FLeafBindType			AsLeaf() const		{ check(IsLeaf());		return Leaf; }
	FRangeBindType			AsRange() const		{ check(IsRange());		return Range; }
	FStructBindType			AsStruct() const	{ check(IsStruct());	return Struct; }
	uint8					AsByte() const		{ return BitCast<uint8>(*this); }

	friend inline bool operator==(FMemberBindType A, FMemberBindType B) { return A.AsByte() == B.AsByte(); }
private:
    EMemberKind				Kind : 2;
	FLeafBindType			Leaf;
    FRangeBindType			Range;
    FStructBindType			Struct;
};

static_assert(sizeof(FMemberBindType) == 1);

////////////////////////////////////////////////////////////////////////////////////////////////

// Members are loaded in saved FStructSchema order, not current offset order unless upgrade layer reorders
struct FSchemaBinding
{
	uint16					NumMembers;
	uint16					NumInnerSchemas;
	uint16					NumInnerRanges;
	FMemberBindType			Members[0];

	const FMemberBindType*	GetInnerRangeTypes() const	{ return Members + NumMembers; }
	const uint32*			GetOffsets() const			{ return AlignPtr<uint32>(GetInnerRangeTypes() + NumInnerRanges); }
	const FSchemaId*		GetInnerSchemas() const		{ return AlignPtr<FSchemaId>(GetOffsets() + NumMembers); }
	const FRangeBinding*	GetRangeBindings() const	{ return AlignPtr<FRangeBinding>(GetInnerSchemas() + NumInnerSchemas); }
	uint32					CalculateSize() const;
	bool					HasSuper() const			{ return NumInnerSchemas > 0 && Members[0].IsStruct() && Members[0].AsStruct().IsSuper; }
};

////////////////////////////////////////////////////////////////////////////////////////////////

struct FUnpackedLeafBindType
{
	ELeafBindType			Type;
	union
	{
		ELeafWidth			Width;
		uint8				BitfieldIdx;
	};
	
	//constexpr FUnpackedLeafBindType(ELeafBindType InType, ELeafWidth InWidth) : Type(InType), Width(InWidth) {}
	constexpr FUnpackedLeafBindType(FLeafBindType In)
	: Type(In.Bind.Type)
	{
		if (Type == ELeafBindType::BitfieldBool)
		{
			Width = In.Arithmetic.Width;
		}
		else
		{
			BitfieldIdx = In.Bitfield.Idx;
		}
	}

//	constexpr bool operator==(FUnpackedLeafBindType O) const { return Type == O.Type && Width == O.Width; }
	FMemberBindType Pack() const
	{ 
		return FMemberBindType(Type == ELeafBindType::BitfieldBool ? FLeafBindType(BitfieldIdx) : FLeafBindType(Type, Width));
	}
};

// @pre Type != ELeafBindType::BitfieldBool
inline FUnpackedLeafType UnpackNonBitfield(FLeafBindType Packed)
{
	FUnpackedLeafBindType Unpacked(Packed);
	return { ToLeafType(Unpacked.Type), Unpacked.Width };
}

struct FLeafMemberBinding
{
	FUnpackedLeafBindType	Leaf;
	FOptionalEnumSchemaId	Enum;
	SIZE_T					Offset;
};

struct FRangeMemberBinding
{
	const FMemberBindType*	InnerTypes;
	const FRangeBinding*	RangeBindings;
	uint16					NumRanges; // At least 1, >1 for nested ranges
	FOptionalSchemaId		InnermostSchema;
	SIZE_T					Offset;
};

struct FStructMemberBinding
{
	FStructType				Type;
	FStructSchemaId			Id;
	SIZE_T					Offset;
};

// Iterates over member bindings
class FMemberVisitor
{
public:
	explicit FMemberVisitor(const FSchemaBinding& InSchema);

	bool						HasMore() const			{ return MemberIdx < NumMembers; }
	uint16						GetIndex() const		{ return MemberIdx; }
	
	EMemberKind					PeekKind() const;		// @pre HasMore()
	FMemberBindType				PeekType() const;		// @pre HasMore()
	uint32						PeekOffset() const;		// @pre HasMore()

	FLeafMemberBinding			GrabLeaf();				// @pre PeekKind() == EMemberKind::Leaf
	FRangeMemberBinding			GrabRange();			// @pre PeekKind() == EMemberKind::Range
	FStructMemberBinding		GrabStruct();			// @pre PeekKind() == EMemberKind::Struct
	void						SkipMember();

protected: // for unit tests
	const FSchemaBinding&		Schema;
	const uint16				NumMembers;
	uint16						MemberIdx = 0;
	uint16						InnerRangeIdx = 0;		// Types of [nested] ranges
	uint16						InnerSchemaIdx = 0;		// Types of static structs and enums

	using FMemberBindTypeRange = TConstArrayView<FMemberBindType>;

	uint64						GrabMemberOffset();
	FMemberBindTypeRange		GrabInnerTypes();
	FSchemaId					GrabInnerSchema();
	FStructSchemaId				GrabStructSchema(FStructType Type);
	FOptionalSchemaId			GrabRangeSchema(FMemberType InnermostType);
	FEnumSchemaId				GrabEnumSchema()			{ return static_cast<FEnumSchemaId&&>(GrabInnerSchema()); }
};

////////////////////////////////////////////////////////////////////////////////////////////////

enum ECustomLoadMethod { Construct, Assign };

class ICustomBinding
{
public:
	virtual ~ICustomBinding() {}
	virtual void				SaveStruct(FMemberBuilder& Dst, const void* Src, const void* Default, const FSaveContext& Ctx) = 0;
	virtual void				LoadStruct(void* Dst, FStructView Src, ECustomLoadMethod Method, const FLoadBatch& Batch) const = 0;
	virtual bool				DiffStruct(const void* StructA, const void* StructB) const = 0;
};

class FCustomBindings
{
public:
	UE_NONCOPYABLE(FCustomBindings);
	FCustomBindings(const FDebugIds& Dbg, const FCustomBindings* InBase = nullptr) : Base(InBase), Debug(Dbg) {}

	// @param Binding must outlive this or call DropStruct()
	PLAINPROPS_API void						BindStruct(FStructSchemaId Id, ICustomBinding& Binding);
	ICustomBinding*							FindStruct(FStructSchemaId Id)								{ return Find(Id); }
	const ICustomBinding*					FindStruct(FStructSchemaId Id) const						{ return Find(Id); }
	PLAINPROPS_API void						DropStruct(FStructSchemaId Id);

private:
	PLAINPROPS_API ICustomBinding* Find(FStructSchemaId Id) const;

	struct FEntry
	{
		FStructSchemaId	Id;
		ICustomBinding* Binding;
	};

	const FCustomBindings*					Base = nullptr;
	TArray<FEntry, TInlineAllocator<8>>		Entries;
	const FDebugIds&						Debug;
};

template<typename T>
struct TCustomBind{ using Type = void; };

template<typename T>
using CustomBind = typename TCustomBind<T>::Type;

////////////////////////////////////////////////////////////////////////////////////////////////

class FConstructionRequest
{
	void* const Range = nullptr;
	const uint64 Num = 0;
	uint64 Index = 0;

	friend FRangeLoader;
	FConstructionRequest(void* InRange, uint64 InNum) : Range(InRange), Num(InNum) {}
	
public:
	template<typename T>
	T& GetRange() const { return *reinterpret_cast<T*>(Range); }
	
	uint64 NumTotal() const { return Num; }
	uint64 NumMore() const { return Num - Index; }
	uint64 GetIndex() const { return Index; }
	bool IsFirstCall() const { return Index == 0;}
	bool IsFinalCall() const { return Index == Num;}
};

class FConstructedItems
{
public:
	// E.g. allow hash table to rehash after all items are loaded
	void RequestFinalCall() { bNeedFinalize = true; }

	void SetUnconstructed() { bUnconstructed = true; }

	// Non-contiguous items must be set individually
	template<typename ItemType>
	void Set(ItemType* Items, uint64 NumItems)
	{
		Set(Items, NumItems, sizeof(ItemType));
	}

	void Set(void* Items, uint64 NumItems, uint32 ItemSize)
	{
		check(NumItems == 0 || Items != Data);
		Data = reinterpret_cast<uint8*>(Items);
		Num = NumItems;
		Size = ItemSize;
	}

	template<typename ItemType>
	TArrayView<ItemType> Get()
	{ 
		return MakeArrayView(reinterpret_cast<ItemType*>(Data), Num);
	}

private:
	friend FRangeLoader;
	uint8*	Data = nullptr;
	uint64	Num = 0;			
	uint32	Size = 0;
	bool	bNeedFinalize = false;
	bool	bUnconstructed = false;

	uint64	NumBytes() const { return Num * Size; }
};

struct FLoadRangeContext
{
	FConstructionRequest	Request;		// Request to construct items to be loaded
	FConstructedItems		Items;			// Response from IItemRangeBinding
	uint64					Scratch[64];	// Scratch memory for IItemRangeBinding
};

// todo: switch to class
struct FGetItemsRequest
{
	template<typename T>
	const T& GetRange() const { return *reinterpret_cast<const T*>(Range); }

	bool IsFirstCall() const { return NumRead == 0;}

	const void* Range = nullptr;
	uint64 NumRead = 0;
};

struct FExistingItemSlice
{
	const void*		Data = nullptr;			
	uint64			Num = 0;

	const uint8* At(uint64 Idx, uint32 Stride) const
	{
		check(Idx < Num);
		return reinterpret_cast<const uint8*>(Data) + Idx*Stride;
	}
};

struct FExistingItems
{
	uint64				NumTotal = 0;
	uint32				Stride = 0;
	FExistingItemSlice	Slice;

	void SetPart(FExistingItemSlice Part)
	{
		NumTotal += Part.Num;
		Slice = Part;
	}

	void SetAll(FExistingItemSlice Whole, uint32 InStride)
	{
		NumTotal = Whole.Num;
		Stride = InStride;
		Slice = Whole;
	}

	template<typename ItemType>
	void SetAll(const ItemType* Items, uint64 NumItems)
	{
		SetAll(FExistingItemSlice{Items, NumItems}, sizeof(ItemType));
	}
};


// TODO: Consider API changes so that
//  - Leaf ranges can allocate data and fill it in directly
//  - Leaf ranges can allocate more capacity than is used
//  - Maybe separate Finalize function
//  - Continue hiding intermediate details (e.g. FBuiltRange) and allocator
struct FSaveRangeContext
{
	FGetItemsRequest		Request;		// Request to get items to be saved
	FExistingItems			Items;			// Response from IRangeBinding
	uint64					Scratch[8]; 	// Scratch memory for IRangeBinding
};

class alignas(16) IItemRangeBinding
{
public:
	virtual void ReadItems(FSaveRangeContext& Ctx) const = 0;
	virtual void MakeItems(FLoadRangeContext& Ctx) const = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////

// Possible save opt: Use paged linear allocator that only allocates on page exhaustion
class FLeafRangeAllocator
{
	FScratchAllocator&			Scratch;
	FBuiltRange*				Range = nullptr;
	const FUnpackedLeafType		Expected;

	void* Allocate(FUnpackedLeafType Type, uint64 Num);

public:
	FLeafRangeAllocator(FScratchAllocator& InScratch, FUnpackedLeafType InExpected) : Scratch(InScratch), Expected(InExpected) {}

	template<typename LeafType, typename SizeType>
	LeafType* AllocateRange(SizeType Num)
	{
		check(ReflectLeaf<LeafType> == Expected);
		return Num ? static_cast<LeafType*>(Allocate(ReflectLeaf<LeafType>, IntCastChecked<uint64>(Num))) : nullptr;
	}

	FBuiltRange* GetAllocatedRange() { return Range; }
};

class FLeafRangeLoadView
{
	const void*				Data;
	uint64					Num;
	FUnpackedLeafType		Leaf;

public:
	FLeafRangeLoadView(const void* InData, uint64 InNum, FUnpackedLeafType InLeaf)
	: Data(InData)
	, Num(InNum)
	, Leaf(InLeaf)
	{}
	
	// The returned ranges hide the internal representations so we can change format in the future, 
	// e.g. store zeroes or 1.0f in some compact fashion or even var int encodings
	template<typename LeafType>
	auto As() const
	{
		check(Leaf == ReflectLeaf<LeafType>);
		if constexpr (std::is_same_v<bool, LeafType>)
		{
			return FBoolRangeView(static_cast<const uint8*>(Data), Num);
		}
		else
		{
			return TRangeView<LeafType>(static_cast<const LeafType*>(Data), Num);
		}
	}
};

// Specialized binding for transcoding leaf ranges
class alignas(16) ILeafRangeBinding
{
public:
	virtual void	SaveLeaves(const void* Range, FLeafRangeAllocator& Out) const = 0;
	virtual void	LoadLeaves(void* Range, FLeafRangeLoadView Leaves) const = 0;
	virtual int64	DiffLeaves(const void* RangeA, const void* RangeB) const = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////

class FRangeBinding
{
	static constexpr uint64		SizeMask = 0b1111;
	static constexpr uint64		LeafMask = uint64(1) << FPlatformMemory::KernelAddressBit;
	static constexpr uint64		BindMask = ~(SizeMask | LeafMask);
	uint64						Handle;

public:
	PLAINPROPS_API FRangeBinding(const IItemRangeBinding& Binding, ERangeSizeType SizeType);
	PLAINPROPS_API FRangeBinding(const ILeafRangeBinding& Binding, ERangeSizeType SizeType);
	
	bool						IsLeafBinding() const		{ return !!(LeafMask & Handle); }
	const IItemRangeBinding&	AsItemBinding() const		{ check(!IsLeafBinding()); return *reinterpret_cast<IItemRangeBinding*>(Handle & BindMask); }
	const ILeafRangeBinding&	AsLeafBinding() const		{ check( IsLeafBinding()); return *reinterpret_cast<ILeafRangeBinding*>(Handle & BindMask); }
	ERangeSizeType				GetSizeType() const			{ return static_cast<ERangeSizeType>(Handle & SizeMask); }
};

template<typename T>
struct TRangeBind{ using Type = void; };

template<typename T>
using RangeBind = typename TRangeBind<T>::Type;

////////////////////////////////////////////////////////////////////////////////////////////////

struct FMemberBinding
{
	FMemberBinding() : InnermostType(FLeafBindType(ELeafBindType::Bool, ELeafWidth::B8)) {}

	uint64							Offset = 0;
	FMemberBindType					InnermostType;		// Always Leaf or Struct
	FOptionalSchemaId				InnermostSchema;	// Enum or struct schema
	TConstArrayView<FRangeBinding>	RangeBindings;		// Non-empty -> Range
};

class FSchemaBindings
{
public:
	UE_NONCOPYABLE(FSchemaBindings);
	explicit FSchemaBindings(const FDebugIds& In) : Debug(In) {}
	PLAINPROPS_API ~FSchemaBindings();

	PLAINPROPS_API void						BindStruct(FStructSchemaId Id, TConstArrayView<FMemberBinding> Schema);
	PLAINPROPS_API const FSchemaBinding&	GetStruct(FStructSchemaId Id) const;
	PLAINPROPS_API void						DropStruct(FStructSchemaId Id);

private:
	TArray<TUniquePtr<FSchemaBinding>>		Bindings;
	const FDebugIds&						Debug;
};

////////////////////////////////////////////////////////////////////////////////////////////////

template<typename Enum, typename Ids>
FEnumSchemaId IndexNativeEnum()
{
	static FEnumSchemaId Id = Ids::IndexEnum(CttiOf<Enum>::Name);
	return Id;
}

template<typename Struct, typename Ids>
FStructSchemaId IndexNativeStruct()
{
	static FStructSchemaId Id = Ids::IndexStruct(CttiOf<Struct>::Name);
	return Id;
}

template<typename Struct, typename Ids>
FOptionalStructSchemaId IndexOptionalNativeStruct()
{
	if constexpr (!std::is_void_v<Struct>)
	{
		return IndexNativeStruct<Struct, Ids>();
	}
	
	return NoId;
}

////////////////////////////////////////////////////////////////////////////////////////////////

template<typename Type, class Ids>
FMemberBindType BindMemberLeaf(FOptionalSchemaId& OutSchema)
{
	OutSchema = std::is_enum_v<Type> ? ToOptional(static_cast<FSchemaId>(IndexNativeEnum<Type, Ids>())) : NoId;
	return FMemberBindType(ReflectLeaf<Type>);
}

template<typename CustomBinding, class Runtime>
FStructSchemaId BindCustomStructOnce()
{
	struct FBinding : CustomBinding
	{
		using Type = typename CustomBinding::Type;
		using Ids = typename Runtime::Ids;

		FBinding()
		{
			FTypeId Name = Ids::IndexNativeType(CttiOf<Type>::Name);
			Id = Ids::IndexStruct(Name);
			CustomBinding::template InitIds<Ids>();
			Runtime::GetTypes().DeclareStruct(Id, Name, CustomBinding::MemberIds, CustomBinding::Occupancy);
			Runtime::GetCustoms().BindStruct(Id, *this);
		}

		~FBinding()
		{
			Runtime::GetCustoms().DropStruct(Id);
			Runtime::GetTypes().DropStruct(Id);
		}

		virtual void SaveStruct(FMemberBuilder& Dst, const void* Src, const void* Default, const FSaveContext& Ctx) override
		{
			CustomBinding::Save(Dst, *static_cast<const Type*>(Src), static_cast<const Type*>(Default), Ctx);
		}

		virtual void LoadStruct(void* Dst, FStructView Src, ECustomLoadMethod Method, const FLoadBatch& Batch) const override
		{
			CustomBinding::Load(*static_cast<Type*>(Dst), Src, Method, Batch);
		}

		virtual bool DiffStruct(const void* A, const void* B) const override
		{
			return CustomBinding::Diff(*static_cast<const Type*>(A), *static_cast<const Type*>(B));
		}

		FStructSchemaId Id;
	};

	static FBinding Binding;
	return Binding.Id;
}

template<class Type, class CustomBinding, class Runtime>
FMemberBindType BindMemberStruct(FOptionalSchemaId& OutSchema)
{
	if constexpr (std::is_void_v<CustomBinding>)
	{
		OutSchema = FOptionalSchemaId(IndexNativeStruct<Type, typename Runtime::Ids>());
	}
	else
	{
		OutSchema = FOptionalSchemaId(BindCustomStructOnce<CustomBinding, Runtime>());
	}

	return FMemberBindType(FStructType{EMemberKind::Struct, /* IsDynamic */ 0, /* IsSuper */ 0});
}

template<typename Type, class Ids>
FMemberBindType BindType(FOptionalSchemaId& OutSchema)
{
	if constexpr (std::is_arithmetic_v<Type> || std::is_enum_v<Type>)
	{
		OutSchema = std::is_enum_v<Type> ? ToOptional(static_cast<FSchemaId>(IndexNativeEnum<Type, Ids>())) : NoId;
		return FMemberBindType(ReflectLeaf<Type>);
	}
	else
	{
		OutSchema = FOptionalSchemaId(IndexNativeStruct<Type, Ids>());
		return FMemberBindType(FStructType{EMemberKind::Struct, /* IsDynamic */ 0, /* IsSuper */ 0});
	}
}

template<typename RangeBinding>
constexpr uint32 CountRangeBindings()
{
	if constexpr (std::is_void_v<RangeBinding>)
	{
		return 0;
	}
	else
	{
		using InnerBinding = RangeBind<typename RangeBinding::ItemType>;
		return 1 + CountRangeBindings<InnerBinding>();
	}
}

template<typename RangeBinding, uint32 NestLevel>
struct TInnerType
{
	using InnerType = typename RangeBinding::ItemType;
	using Type = TInnerType<RangeBind<InnerType>, NestLevel - 1>::Type;
};

template<typename RangeBinding>
struct TInnerType<RangeBinding, 1>
{
	using Type = typename RangeBinding::ItemType;
};

template<typename RangeBinding, uint32 N>
TConstArrayView<FRangeBinding> GetRangeBindings()
{
	static_assert(N != 0);

	struct FOnce
	{
		FOnce() : Binding(Instance, RangeSizeOf(typename RangeBinding::SizeType{})) {}
		RangeBinding Instance;
		FRangeBinding Binding;
	};

	if constexpr (N == 1)
	{
		static FOnce Static;
		return MakeArrayView(&Static.Binding, N);
	}
	else
	{
		using InnerType = typename RangeBinding::ItemType;
		using InnerRangeBinding = RangeBind<InnerType>;

		struct FNestedOnce : FOnce
		{
			FNestedOnce() 
			{
				FMemory::Memcpy(NestedBindings, GetRangeBindings<InnerRangeBinding, N - 1>().GetData(), sizeof(NestedBindings));
			}
			
			uint8 NestedBindings[sizeof(FRangeBinding) * (N - 1)] = {};
		};
		static_assert(std::is_trivially_destructible_v<FRangeBinding>);
		static_assert(offsetof(FNestedOnce, Binding) + sizeof(FRangeBinding) == offsetof(FNestedOnce, NestedBindings));	

		static FNestedOnce Static;
		return MakeArrayView(&Static.Binding, N);
	}
}

template<class Var, class Runtime>
FMemberBinding BindMember()
{
	using Ids = typename Runtime::Ids;
	using Type = Var::Type;

	FMemberBinding Out;
	Out.Offset = Var::Offset;
	if constexpr (std::is_arithmetic_v<Type> || std::is_enum_v<Type>)
	{
		Out.InnermostType = BindMemberLeaf<Type, Ids>(Out.InnermostSchema);
	}
	else
	{
		using CustomBinding = typename Runtime::template CustomBindings<Type>::Type;
		if constexpr (!std::is_void_v<CustomBinding>)
		{
			Out.InnermostType = BindMemberStruct<Type, CustomBinding, Runtime>(Out.InnermostSchema);
		}
		else
		{
			using RangeBinding = RangeBind<Type>;
			if constexpr (!std::is_void_v<RangeBinding>)
			{
				constexpr uint32 NumRangeBindings = CountRangeBindings<RangeBinding>();
				using InnermostType = typename TInnerType<RangeBinding, NumRangeBindings>::Type;

				Out.RangeBindings = GetRangeBindings<RangeBinding, NumRangeBindings>();
				Out.InnermostType = BindType<InnermostType, Ids>(Out.InnermostSchema);
			}
			else
			{
				Out.InnermostType = BindMemberStruct<Type, CustomBinding, Runtime>(Out.InnermostSchema);
			}
		}
	}
	
	return Out;
}

////////////////////////////////////////////////////////////////////////////////////////////////

template<class Ctti, class Ids>
FEnumSchemaId DeclareNativeEnum(FDeclarations& Out, EEnumMode Mode)
{
	using UnderlyingType = std::underlying_type_t<typename Ctti::Type>;

	FTypeId Type = Ids::IndexNativeType(Ctti::Name);
	FEnumSchemaId Id = Ids::IndexEnum(Type);
	FEnumerator Enumerators[Ctti::NumEnumerators];
	for (FEnumerator& Enumerator : Enumerators)
	{
		Enumerator.Name = Ids::IndexName(Ctti::Enumerators[&Enumerator - Enumerators].Name);
		Enumerator.Constant = static_cast<uint64>(static_cast<UnderlyingType>(Ctti::Enumerators[&Enumerator - Enumerators].Constant));
	}
	Out.DeclareEnum(Id, Type, Mode, LeafWidth<sizeof(UnderlyingType)>, Enumerators);

	return Id;
}

template<class Ctti, class Ids>
FStructSchemaId DeclareNativeStruct(FDeclarations& Out, EMemberPresence Occupancy)
{
	FTypeId Type = Ids::IndexNativeType(Ctti::Name);
	FStructSchemaId Id = Ids::IndexStruct(Type);
	FOptionalStructSchemaId SuperId = IndexOptionalNativeStruct<typename Ctti::Super, Ids>();
	FMemberId MemberIds[Ctti::NumVars];
	ForEachVar<Ctti>([&]<class Var>()
	{ 
		MemberIds[Var::Index] = Ids::IndexMember(Var::Name);
	});
	Out.DeclareStruct(Id, Type, MemberIds, Occupancy, SuperId);

	return Id;
}

template<class Ctti, class Runtime>
void BindNativeStruct(FSchemaBindings& Out, FStructSchemaId DeclaredId)
{
	FMemberBinding MemberBindings[Ctti::NumVars];
	ForEachVar<Ctti>([&]<class Var>()
	{ 
		MemberBindings[Var::Index] = BindMember<Var, Runtime>();
	});
	Out.BindStruct(DeclaredId, MemberBindings);
}

////////////////////////////////////////////////////////////////////////////////////////////////

struct FMemberBinder
{
	FMemberBinder(FSchemaBinding& InSchema)
	: Schema(InSchema)
	, MemberIt(Schema.Members)
	, RangeTypeIt(const_cast<FMemberBindType*>(Schema.GetInnerRangeTypes()))
	, OffsetIt(const_cast<uint32*>(Schema.GetOffsets()))
	, InnerSchemaIt(const_cast<FSchemaId*>(Schema.GetInnerSchemas()))
	, RangeBindingIt(const_cast<FRangeBinding*>(Schema.GetRangeBindings()))
	{}

	~FMemberBinder()
	{
		check(MemberIt == Schema.GetInnerRangeTypes());
		check(Align(RangeTypeIt, alignof(uint32)) == (const void*)Schema.GetOffsets());
		check(OffsetIt == (const void*)Schema.GetInnerSchemas());
		check(Align(InnerSchemaIt, alignof(FRangeBinding)) == (const void*)Schema.GetRangeBindings() || Schema.NumInnerRanges == 0);
		check(Schema.NumInnerRanges == RangeBindingIt - Schema.GetRangeBindings());
	}

	void AddMember(FMemberBindType Type, uint32 Offset)
	{
		*MemberIt++ = Type;
		*OffsetIt++ = Offset;
	}

	void AddRange(TConstArrayView<FRangeBinding> Ranges, FMemberBindType InnermostType, uint32 Offset)
	{
		AddMember(FMemberBindType(Ranges[0].GetSizeType()), Offset);

		for (FRangeBinding Range : Ranges.RightChop(1))
		{
			*RangeTypeIt++ = FMemberBindType(Range.GetSizeType());
		}
		*RangeTypeIt++ = InnermostType;

		FMemory::Memcpy(RangeBindingIt, Ranges.GetData(), Ranges.Num() * Ranges.GetTypeSize());
		RangeBindingIt += Ranges.Num();
	}
	
	void AddInnerSchema(FSchemaId InnermostSchema)
	{
		*InnerSchemaIt++ = InnermostSchema;
	}

	FSchemaBinding& Schema;
	FMemberBindType* MemberIt;
	FMemberBindType* RangeTypeIt;
	uint32* OffsetIt;
	FSchemaId* InnerSchemaIt;
	FRangeBinding* RangeBindingIt;
};

//////////////////////////////////////////////////////////////////////////

// Save -> load struct ids for ESchemaFormat::InMemoryNames
[[nodiscard]] PLAINPROPS_API TArray<FStructSchemaId> IndexInMemoryNames(const FSchemaBatch& Schemas,  FIdIndexerBase& Indexer);

// Save -> load ids for ESchemaFormat::StableNames
struct FIdBinding
{
	TConstArrayView<FNameId>			Names;
	TConstArrayView<FNestedScopeId>		NestedScopes;
	TConstArrayView<FParametricTypeId>	ParametricTypes;
	TConstArrayView<FSchemaId>			Schemas;

	FNameId								Remap(FNameId Old) const				{ return Names[Old.Idx]; }
	FMemberId							Remap(FMemberId Old) const				{ return { Remap(Old.Id) }; }
	FFlatScopeId						Remap(FFlatScopeId Old) const			{ return { Remap(Old.Name) }; }
	FNestedScopeId						Remap(FNestedScopeId Old) const			{ return NestedScopes[Old.Idx]; }
	FScopeId							Remap(FScopeId Old) const				{ return Old.IsNested() ? FScopeId(Remap(Old.AsNested())) : FScopeId(Remap(Old.AsFlat())); }
	FConcreteTypenameId					Remap(FConcreteTypenameId Old) const	{ return { Remap(Old.Id) }; }	
	FParametricTypeId					Remap(FParametricTypeId Old) const		{ return ParametricTypes[Old.Idx]; }
	FTypenameId							Remap(FTypenameId Old) const			{ return Old.IsConcrete() ? FTypenameId(Remap(Old.AsConcrete())) : FTypenameId(Remap(Old.AsParametric())); }
	FTypeId								Remap(FTypeId Old) const				{ return { Remap(Old.Scope), Remap(Old.Name) }; }

	TConstArrayView<FStructSchemaId>	GetStructIds(int32 NumStructs) const
	{
		// All saved struct schema ids are lower than enum schema ids
		check(NumStructs <= Schemas.Num());
		return MakeArrayView(static_cast<const FStructSchemaId*>(Schemas.GetData()), NumStructs);
	}
};

struct FIdTranslatorBase
{
	static uint32 CalculateTranslationSize(int32 NumSavedNames, const FSchemaBatch& Batch);
	static FIdBinding TranslateIds(FMutableMemoryView To, FIdIndexerBase& Indexer, TConstArrayView<FNameId> TranslatedNames, const FSchemaBatch& From);
};

// Maps saved ids -> runtime load ids for ESchemaFormat::StableNames
struct FIdTranslator : FIdTranslatorBase
{
	template<class NameType>
	FIdTranslator(TIdIndexer<NameType>& Indexer, TConstArrayView<NameType> SavedNames, const FSchemaBatch& Batch)
	{
		Allocator.SetNumUninitialized(CalculateTranslationSize(SavedNames.Num(), Batch));
		
		// Translate names
		TArrayView<FNameId> NewNames(reinterpret_cast<FNameId*>(&Allocator[0]), SavedNames.Num());
		FNameId* NameIt = &NewNames[0];
		for (const NameType& SavedName : SavedNames)
		{
			(*NameIt++) = Indexer.MakeName(SavedName);
		}

		FMutableMemoryView OtherIds(NameIt, Allocator.Num() - NewNames.Num() * sizeof(FNameId));
		Translation = TranslateIds(/* out */ OtherIds, Indexer, NewNames, Batch);
	}
	
	FIdBinding								Translation;
	TArray<uint8, TInlineAllocator<1024>>	Allocator;
};

FSchemaBatch*			CreateTranslatedSchemas(const FSchemaBatch& Schemas, FIdBinding NewIds);
void					DestroyTranslatedSchemas(const FSchemaBatch* Schemas);

} // namespace PlainProps