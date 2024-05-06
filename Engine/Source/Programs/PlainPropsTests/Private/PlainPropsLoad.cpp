// Copyright Epic Games, Inc. All Rights Reserved.

#include  "PlainPropsLoad.h"
#include  "PlainPropsBind.h"
#include  "PlainPropsInternalFormat.h"
#include  "PlainPropsInternalRead.h"
#include "Misc/Optional.h"
#include <type_traits>

namespace PlainProps
{

struct FLoadStructMemcpy
{
	uint32			Size;
	uint32			Offset;
};

// Describes how to load a saved struct into the matching in-memory representation
class FLoadStructPlan
{
public:
	FLoadStructPlan() = default;

	explicit FLoadStructPlan(FLoadStructMemcpy Memcpy)
	: Handle((uint64(Memcpy.Size) << 32) | (uint64(Memcpy.Offset) << 2) | MemcpyMask)
	{
		check(Memcpy.Offset == AsMemcpy().Offset && Memcpy.Size == AsMemcpy().Size);
	}

	explicit FLoadStructPlan(const ICustomStructBinding& Custom)
	: Handle(uint64(&Custom) | CustomMask)
	{
		check(&Custom == &AsCustom());
	}

	// @param OffsetWidth Usage unimplemented, store size and offsets as 8/16/32/64-bit 
	explicit FLoadStructPlan(const FStructSchemaBinding& Schema, ELeafWidth OffsetWidth, bool bSparse)
	: Handle(uint64(&Schema) | (uint64(OffsetWidth) << 1) | (uint64(bSparse) << KernelAddressBit) | SchemaMask)
	{
		check(&Schema == &AsSchema());
		check(IsSparseSchema() == bSparse);
	}

	bool						IsSchema() const		{ return (Handle & SchemaMask) == SchemaMask; }
	bool						IsSparseSchema() const	{ return (Handle & SparseSchemaMask) == SparseSchemaMask; }
	bool						IsMemcpy() const		{ return (Handle & LoMask) == MemcpyMask; }
	bool						IsCustom() const		{ return (Handle & LoMask) == CustomMask; }
	FLoadStructMemcpy			AsMemcpy() const		{ check(IsMemcpy()); return { static_cast<uint32>(Handle >> 32), static_cast<uint32>(Handle) >> 1 }; }
	const ICustomStructBinding&	AsCustom() const		{ check(IsCustom()); return *AsPtr<ICustomStructBinding>(); }
	const FStructSchemaBinding&	AsSchema() const		{ check(IsSchema()); return *AsPtr<FStructSchemaBinding>(); }

private:
	// This bit is always zero in user mode addresses and most likely won't be used by current or future
	// CPU features like ARM's PAC / Top-Byte Ignore or Intel's Linear Address Masking / 5-Level Paging
#if defined(__x86_64__) || defined(_M_X64)
	static constexpr uint32 KernelAddressBit = 63;
#elif defined(__aarch64__) || defined(_M_ARM64)
	static constexpr uint32 KernelAddressBit = 55;
#else
	#error Unsupported architecture, please declare which address bit distinguish user space from kernel space
#endif
	// todo handle WASM, copy updated KernelAddressBit from AssetDataTagMap.h

	
	static constexpr uint64 SparseMask			= uint64(1) << KernelAddressBit;
	static constexpr uint64 PtrMask				= ~(SparseMask | 0b111);
	static constexpr uint64 LoMask				= 0b11;
	static constexpr uint64 MemcpyMask			= 0b00;
	static constexpr uint64 CustomMask			= 0b10;
	static constexpr uint64 SchemaMask			= 0b01;
	static constexpr uint64 SparseSchemaMask	= SchemaMask | SparseMask;
	
	template<typename T>
	const T* AsPtr() const
	{
		check(Handle & PtrMask);
		return reinterpret_cast<T*>(Handle & PtrMask);
	}

	uint64						Handle = 0;
};

////////////////////////////////////////////////////////////////////////////

static uint16 CountEnums(const FStructSchema& Schema)
{
	uint16 Num = 0;
	for (FMemberType Member : MakeArrayView(Schema.Footer, Schema.NumMembers))
	{
		Num += IsEnum(Member);
	}
	return Num;
}

static uint16 CountEnums(const FStructSchemaBinding& Schema)
{
	uint16 Num = 0;
	for (FMemberBindType Member : MakeArrayView(Schema.Members, Schema.NumMembers))
	{
		Num += (Member.IsLeaf() && Member.AsLeaf().Bind.Type == ELeafBindType::Enum);
	}
	return Num;
}

static uint16 CountStaticStructs(const FStructSchemaBinding& Schema)
{
	uint16 Num = 0;
	for (FMemberBindType Member : MakeArrayView(Schema.Members, Schema.NumMembers))
	{
		Num += (Member.IsStruct() && !Member.AsStruct().IsDynamic);
	}
	return Num;
}

static bool HasDifferentSupers(const FStructSchema& From, const FStructSchemaBinding& To, TConstArrayView<FStructSchemaId> ToStructIds)
{
	if (From.Inheritance == ESuper::No)
	{
		return To.HasSuper();
	}
	else if (To.HasSuper())
	{
		FStructSchemaId FromSuper = ToStructIds[From.GetSuperSchema().Get().Idx];
		FStructSchemaId ToSuper = static_cast<FStructSchemaId>(To.GetInnerSchemas()[0]);
		return FromSuper == ToSuper;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////

struct FLoadBatch
{
	FReadBatchId			ReadId; // Needed to access schemas for custom struct loading
	uint32					NumPlans;
	FLoadStructPlan			Plans[0];

	FLoadStructPlan			operator[](FStructSchemaId Id) const { check(Id.Idx < NumPlans); return Plans[Id.Idx]; }
};

using SubsetByteArray = TArray<uint8, TInlineAllocator<1024>>;

static void CopyMemberBinding(FLeafMemberBinding From, FMemberBinder& To)
{
	// Skip enum schema
	To.AddMember(From.Leaf.Pack(), static_cast<uint32>(From.Offset));
}

static void CopyMemberBinding(FStructMemberBinding From, FMemberBinder& To)
{
	To.AddMember(FMemberBindType(From.Type), static_cast<uint32>(From.Offset));
	To.AddInnerSchema(From.Id);
}

static void CopyMemberBinding(FRangeMemberBinding From, FMemberBinder& To)
{
	FMemberBindType InnermostType = From.InnerTypes[From.NumRanges - 1];
	To.AddRange(MakeArrayView(From.RangeBindings, From.NumRanges), InnermostType, static_cast<uint32>(From.Offset));
	To.AddOptionalInnerSchema(From.InnermostSchema);
}

static void CopyMemberBinding(/* in-out */ FMemberVisitor& From, FMemberBinder& To)
{
	switch (From.PeekKind())
	{
		case EMemberKind::Leaf:		CopyMemberBinding(From.GrabLeaf(), To);		break;
		case EMemberKind::Range:	CopyMemberBinding(From.GrabRange(), To);	break;
		case EMemberKind::Struct:	CopyMemberBinding(From.GrabStruct(), To);	break;
		default:					check(false);								break;
	}
}

static void CreateSubsetLoadSchema(const FStructSchema& From, const FStructSchemaBinding& To, TConstArrayView<FMemberId> ToNames,  uint16 NumEnums, SubsetByteArray& Out)
{
	check(To.NumMembers == ToNames.Num());
	check(To.NumMembers >= From.NumMembers);

	int32 OutPos = Out.Num();
	FStructSchemaBinding Header = { From.NumMembers, From.NumInnerSchemas - NumEnums, From.NumRangeTypes };
	Out.AddUninitialized(Header.CalculateSize());
	FStructSchemaBinding* Schema = new (&Out[OutPos]) FStructSchemaBinding {Header};
	
	FMemberVisitor ToIt(To);
	FMemberBinder Footer(*Schema);
	for (FMemberId FromName : From.GetMemberNames())
	{
		while (FromName != ToNames[ToIt.GetIndex()])
		{
			ToIt.SkipMember();
		}

		CopyMemberBinding(/* in-out */ ToIt, /* out */ Footer);	
	}
}

[[nodiscard]] static FLoadStructPlan MakeSchemaLoadPlan(const FStructSchema& From, const FStructSchemaBinding& To, TConstArrayView<FMemberId> ToMemberIds, TConstArrayView<FStructSchemaId> ToStructIds, SubsetByteArray& OutSubsetSchemas)
{
	uint16 NumEnums = CountEnums(From);
	if (From.NumMembers < To.NumMembers || NumEnums || HasDifferentSupers(From, To, ToStructIds))
	{
		CreateSubsetLoadSchema(From, To, ToMemberIds, NumEnums, /* out */ OutSubsetSchemas);
	}
	else // Reuse To bindings
	{
		check(From.NumMembers == To.NumMembers);
		check(From.NumInnerSchemas == To.NumInnerSchemas);
		check(From.NumRangeTypes == To.NumInnerRanges);
	}

	// Pointer to created subset load schema will be remapped later
	return FLoadStructPlan(To, ELeafWidth::B32, !From.IsDense);
}

[[nodiscard]] static TOptional<FLoadStructMemcpy> TryMakeMemcpyPlan(const FStructSchema& From, const FStructSchemaBinding& To, TConstArrayView<FMemberId> ToMemberIds)
{
	// todo
	return NullOpt;
}

[[nodiscard]] static FLoadStructPlan MakeLoadPlan(const FStructSchema& From, const FStructSchemaBinding& To, TConstArrayView<FMemberId> ToMemberIds, TConstArrayView<FStructSchemaId> ToStructIds, SubsetByteArray& OutSubsetSchemas)
{
	TOptional<FLoadStructMemcpy> Memcpy = TryMakeMemcpyPlan(From, To, ToMemberIds);
	return Memcpy ? FLoadStructPlan(Memcpy.GetValue()) : MakeSchemaLoadPlan(From, To, ToMemberIds, ToStructIds, OutSubsetSchemas);
}

FLoadBatch* CreateLoadPlans(FReadBatchId ReadId, const FDeclarations& Declarations, const FStructBindings& Bindings, TConstArrayView<FStructSchemaId> RuntimeIds)
{
	check(NumStructSchemas(ReadId) == RuntimeIds.Num());

	// Temporary data structures
	const uint32 NumPlans = RuntimeIds.Num();
	TArray<FLoadStructPlan, TInlineAllocator<256>> Plans;
	TArray<uint32, TInlineAllocator<256>> SubsetSchemaSizes;
	SubsetByteArray SubsetSchemaData;
	Plans.SetNumUninitialized(NumPlans);
	SubsetSchemaSizes.SetNumUninitialized(NumPlans);

	// Create plans
	for (FStructSchemaId SavedId = { 0 }; SavedId.Idx < NumPlans; ++SavedId.Idx)
	{
		FStructSchemaId RuntimeId = RuntimeIds[SavedId.Idx];
		FStructBinding Binding = Bindings.Get(RuntimeId);
		int32 SubsetSchemaOffset = SubsetSchemaData.Num();
		if (Binding.IsCustom())
		{
			Plans[SavedId.Idx] = FLoadStructPlan(Binding.AsCustom()) ;
		}
		else
		{
			const FStructSchema& From = ResolveStructSchema(ReadId, SavedId);
			const FStructSchemaBinding& To = Binding.AsSchema();
			// Possible optimization - some simple memcpy cases doesn't need to resolve the declaration
			TConstArrayView<FMemberId> ToMemberIds = Declarations.Get(RuntimeId).GetMemberOrder();
			Plans[SavedId.Idx] = MakeLoadPlan(From, To, ToMemberIds, RuntimeIds, /* out */ SubsetSchemaData);	
		}
		
		SubsetSchemaSizes[SavedId.Idx] = SubsetSchemaData.Num() - SubsetSchemaOffset;
	}
	
	// Allocate load batch, copy plans and subset schemas, and fixup subset schema plans
	SIZE_T Bytes = sizeof(FLoadBatch) + sizeof(FLoadStructPlan) * Plans.Num() + SubsetSchemaData.Num();
	FLoadBatch Header = { ReadId, NumPlans };
	FLoadBatch* Out = new (FMemory::Malloc(Bytes)) FLoadBatch{Header};
	FMemory::Memcpy(Out->Plans, Plans.GetData(), sizeof(FLoadStructPlan) * Plans.Num());
	if (SubsetSchemaData.Num() > 0)
	{
		uint8* OutSubsetData = reinterpret_cast<uint8*>(Out->Plans + Plans.Num());
		FMemory::Memcpy(OutSubsetData, SubsetSchemaData.GetData(), SubsetSchemaData.Num());
		
		// Update plans with actual subset schema pointers
		const uint8* It = OutSubsetData;
		for (uint32 Idx = 0; Idx < NumPlans; ++Idx)
		{
			if (int32 Size = SubsetSchemaSizes[Idx])
			{
				check(IsAligned(Size, alignof(FStructSchemaBinding)));
				check(Plans[Idx].IsSchema());
				bool bSparse = Plans[Idx].IsSparseSchema();
				Out->Plans[Idx] = FLoadStructPlan(*reinterpret_cast<const FStructSchemaBinding*>(It), ELeafWidth::B32, bSparse);
				It += Size;
			}
		}
		check(It == OutSubsetData + SubsetSchemaData.Num());
	}

	return Out;
}

void DestroyLoadPlans(FLoadBatch* Batch)
{
	FMemory::Free(Batch);
}

////////////////////////////////////////////////////////////////////////////

FORCEINLINE static void SetBit(uint8& Out, uint8 Idx, bool bValue)
{
	uint8 Mask = IntCastChecked<uint8>(1 << Idx);
	if (bValue)
	{
		Out |= Mask;
	}
	else
	{
		Out &= Mask;
	}	
}

struct FLoadRangePlan
{
	ERangeSizeType MaxSize;
	FOptionalStructSchemaId InnermostStruct;
	TConstArrayView<FMemberBindType> InnerTypes;
	const FRangeBinding* Bindings = nullptr;

	FLoadRangePlan Tail() const
	{
		return { InnerTypes[0].AsRange().MaxSize, InnermostStruct, InnerTypes.RightChop(1), Bindings + 1 };
	}
};

static FMemberBindType ToBindType(FMemberType Member)
{
	switch (Member.GetKind())
	{
	case EMemberKind::Leaf:		return FMemberBindType(Member.AsLeaf());
	case EMemberKind::Range:	return FMemberBindType(Member.AsRange());
	default:					return FMemberBindType(Member.AsStruct());	
	}
}

class FRangeLoader
{
public:
	static void LoadNestedRange(uint8* Member, FMemoryView Values, const FLoadBatch& Batch, const FLoadRangePlan& Range)
	{
		FByteReader ByteIt(Values);
		FBitCacheReader BitIt;
		LoadRange(Member, ByteIt, BitIt, Batch, Range);
	}

	static void LoadRangeView(uint8* Member, FRangeView Src, ERangeSizeType MaxSize, TConstArrayView<FRangeBinding> Bindings, const FLoadBatch& Batch)
	{
		TArray<FMemberBindType, TFixedAllocator<16>> InnerTypes;
		InnerTypes.Add(ToBindType(Src.Schema.ItemType));
		for (const FMemberType* It = Src.Schema.NestedItemTypes; It; It = It->IsRange() ? (It + 1) : nullptr)
		{
			InnerTypes.Add(ToBindType(*It));
		}
		check(Bindings.Num() == InnerTypes.Num());
		
		FOptionalStructSchemaId StructSchema = InnerTypes.Last().IsStruct() ? static_cast<FOptionalStructSchemaId>(Src.Schema.InnermostSchema) : NoId;
		
		FLoadRangePlan Plan = { MaxSize, StructSchema, InnerTypes, Bindings.GetData() };
		
		FByteReader ByteIt(Src.Values);
		FBitCacheReader BitIt;
		LoadRange(Member, ByteIt, BitIt, Batch, Plan);
	}

	static void LoadRange(uint8* Member, FByteReader& ByteIt, FBitCacheReader& BitIt, const FLoadBatch& Batch, const FLoadRangePlan& Range)
	{
		FMemberBindType InnerType = Range.InnerTypes[0];
		const IRangeBinding& Binding = Range.Bindings[0].GetBinding();

		if (uint64 Num = GrabRangeNum(Range.MaxSize, ByteIt, BitIt))
		{
			switch (InnerType.GetKind())
			{
				case EMemberKind::Leaf:		LoadRangeValues(Member, Num, Binding, ByteIt, Batch, InnerType.AsLeaf()); break;
				case EMemberKind::Range:	LoadRangeValues(Member, Num, Binding, ByteIt, Batch, Range.Tail()); break;
				case EMemberKind::Struct:	LoadRangeValues(Member, Num, Binding, ByteIt, Batch, Range.InnermostStruct.Get()); break;
			}
		}
		else
		{
			FLoadRangeContext NoItemsCtx{.Request = {Member, 0}};
			(Binding.MakeItems)(NoItemsCtx);
		}
	}

	template<class SchemaType>
	static void LoadRangeValues(uint8* Member, uint64 Num, const IRangeBinding& Binding, FByteReader& ByteIt, const FLoadBatch& Batch, SchemaType&& Schema)
	{
		FLoadRangeContext Ctx{.Request = {Member, Num}};
		
		while (Ctx.Request.Index < Num)
		{
			(Binding.MakeItems)(Ctx);
			CopyRangeValues(Ctx.Items, ByteIt, Batch, Schema);
			Ctx.Request.Index += Ctx.Items.Num;
		}
		
		if (Ctx.Items.bNeedFinalize)
		{
			(Binding.MakeItems)(Ctx);
		}
	}
	static void CopyRangeValues(const FConstructedItems& Items, FByteReader& ByteIt, const FLoadBatch& Batch, FLeafBindType Leaf)
	{
		switch (Leaf.Bind.Type)
		{
		case ELeafBindType::Bool:
		{
			check(Items.Size == sizeof(bool));
			FBoolRangeView Bits(ByteIt.GrabBytes(Align(Items.Num, 8)/8), Items.Num);
			uint8* It = Items.Data;
			for (bool bBit : Bits)
			{
				reinterpret_cast<bool&>(*It++) = bBit;
			}

			break;
		}
		default:
			checkf(Leaf.Bind.Type != ELeafBindType::BitfieldBool, TEXT("Loading to bitfield array is unsupported"));
			check(Items.Size == SizeOf(Leaf.Arithmetic.Width));
			FMemory::Memcpy(Items.Data, ByteIt.GrabBytes(Items.NumBytes()), Items.NumBytes());
			break;
		}
	}
	
	static void CopyRangeValues(const FConstructedItems& Items, FByteReader& ByteIt, const FLoadBatch& Batch, FStructSchemaId Id)
	{
		uint64 ItemSize = Items.Size;
		for (uint8* It = Items.Data, *End = It + Items.NumBytes(); It != End; It += ItemSize)
		{
			PlainProps::LoadStruct(It, FByteReader(ByteIt.GrabSkippableSlice()), Id, Batch);
		}
	}
	
	static void CopyRangeValues(const FConstructedItems& Items, FByteReader& ByteIt, const FLoadBatch& Batch, const FLoadRangePlan& Plan)
	{
		uint64 ItemSize = Items.Size;
		for (uint8* It = Items.Data, *End = It + Items.NumBytes(); It != End; It += ItemSize)
		{
			LoadNestedRange(It, ByteIt.GrabSkippableSlice(), Batch, Plan);	
		}
	}
};

////////////////////////////////////////////////////////////////////////////

template<bool bSparse, typename OffsetType>
class TMemberLoader : public FRangeLoader
{
public:
	TMemberLoader(FByteReader Values, const FStructSchemaBinding& Schema, const FLoadBatch& InBatch)
	: Types(Schema.Members, Schema.NumMembers)
	, Offsets(Schema.GetOffsets())
	, InnerStructSchemas(static_cast<const FStructSchemaId*>(Schema.GetInnerSchemas()), Schema.NumInnerSchemas)
	, InnerRangeTypes(Schema.GetInnerRangeTypes(), Schema.NumInnerRanges)
	, RangeBindings(Schema.GetRangeBindings())
	, Batch(InBatch)
	, ByteIt(Values)
	{
		checkf(CountStaticStructs(Schema) == Schema.NumInnerSchemas, TEXT("Expects Schema stripped from load-irrelevant enum schema ids, see CreateSubsetLoadSchema(). "
			"# schemas/structs/enums: %d/%d/%d"), Schema.NumInnerSchemas, CountStaticStructs(Schema), CountEnums(Schema));
	}

	void Load(uint8* Struct)
	{
		SkipMissingSparseMembers();
	
		while (MemberIdx < Types.Num())
		{
			LoadMember(Struct);
			++MemberIdx;
			SkipMissingSparseMembers();
		}
	}

private:
	const TConstArrayView<FMemberBindType>		Types;
	const OffsetType* const						Offsets;
	const TConstArrayView<FStructSchemaId>		InnerStructSchemas;
	const TConstArrayView<FMemberBindType>		InnerRangeTypes;
	const FRangeBinding*						RangeBindings;
	const FLoadBatch&							Batch;
	
	FByteReader ByteIt;
	FBitCacheReader BitIt;
	int32 MemberIdx = 0;
	int32 InnerRangeIdx = 0;
	int32 InnerStructIdx = 0;	

	void SkipMissingSparseMembers()
	{
		// Make code changes in FMemberReader::SkipMissingSparseMembers() too
		while (bSparse && MemberIdx < Types.Num() && BitIt.GrabNext(ByteIt))
		{
			FMemberBindType Type = Types[MemberIdx];
			FMemberBindType InnermostType = Type.IsRange() ? GrabInnerRanges(Type.AsRange()).InnerTypes.Last() : Type;
			if (InnermostType.IsStruct())
			{
				(void)GrabInnerStruct(InnermostType.AsStruct());
			}
			++MemberIdx;
		}
	}

	void LoadMember(uint8* Struct)
	{
		FMemberBindType Type = Types[MemberIdx];
		uint8* Member = Struct + Offsets[MemberIdx];

		switch (Type.GetKind())
		{
			case EMemberKind::Leaf:		LoadMemberLeaf(Member, Type.AsLeaf()); break;
			case EMemberKind::Range:	LoadMemberRange(Member, GrabInnerRanges(Type.AsRange())); break;
			case EMemberKind::Struct:	LoadMemberStruct(Member, GrabInnerStruct(Type.AsStruct())); break;
		}
	}

	FStructSchemaId GrabInnerStruct(FStructBindType Type)
	{
		return Type.IsDynamic ? FStructSchemaId { ByteIt.Grab<uint32>() } : InnerStructSchemas[InnerStructIdx++];
	}

	FLoadRangePlan GrabInnerRanges(FRangeBindType Type)
	{
		const FRangeBinding* Bindings = RangeBindings + InnerRangeIdx;
		TConstArrayView<FMemberBindType> InnerTypes = GrabInnerRangeTypes(InnerRangeTypes, /* in-out */ InnerRangeIdx);	
		FOptionalStructSchemaId InnermostStruct = InnerTypes.Last().IsStruct() ? ToOptional(GrabInnerStruct(InnerTypes.Last().AsStruct())) : NoId;
		return { Type.MaxSize, InnermostStruct, InnerTypes, Bindings };
	}
	
	void LoadMemberLeaf(uint8* Member, FLeafBindType Leaf)
	{
		switch (Leaf.Bind.Type)
		{
			case ELeafBindType::Bool:
				reinterpret_cast<bool&>(*Member) = BitIt.GrabNext(ByteIt);
				break;
			case ELeafBindType::BitfieldBool:
				SetBit(*Member, Leaf.Bitfield.Idx, BitIt.GrabNext(ByteIt));
				break;
			default:
				switch (Leaf.Arithmetic.Width)
				{
					case ELeafWidth::B8:	FMemory::Memcpy(Member, ByteIt.GrabBytes(1), 1); break;
					case ELeafWidth::B16:	FMemory::Memcpy(Member, ByteIt.GrabBytes(2), 2); break;
					case ELeafWidth::B32:	FMemory::Memcpy(Member, ByteIt.GrabBytes(4), 4); break;
					case ELeafWidth::B64:	FMemory::Memcpy(Member, ByteIt.GrabBytes(8), 8); break;
				}
				break;
		}
	}

	void LoadMemberStruct(uint8* Member, FStructSchemaId Id)
	{
		PlainProps::LoadStruct(Member, FByteReader(ByteIt.GrabSkippableSlice()), Id, Batch);
	}

	void LoadMemberRange(uint8* Member, FLoadRangePlan&& Range)
	{
		LoadRange(Member, ByteIt, BitIt, Batch, Range);
	}
};

////////////////////////////////////////////////////////////////////////////

void LoadStruct(uint8* Dst, FByteReader Src, FStructSchemaId Id, const FLoadBatch& Batch)
{
	FLoadStructPlan Plan = Batch[Id];
	if (Plan.IsSchema())
	{
		if (Plan.IsSparseSchema())
		{
			TMemberLoader< true, uint32>(Src, Plan.AsSchema(), Batch).Load(Dst);
		}
		else
		{
			TMemberLoader<false, uint32>(Src, Plan.AsSchema(), Batch).Load(Dst);
		}
	}
	else if (Plan.IsMemcpy())
	{
		Src.CheckSize(Plan.AsMemcpy().Size);
		FMemory::Memcpy(Dst + Plan.AsMemcpy().Offset, Src.Peek(), Plan.AsMemcpy().Size);
	}
	else
	{
		FStructSchemaHandle ReadSchema{Id, Batch.ReadId};
		Plan.AsCustom().LoadStruct(Dst, { ReadSchema, Src }, ECustomLoadMethod::Assign, Batch);
	}
}

void LoadStruct(uint8* Dst, FStructView Src, const FLoadBatch& Batch)
{
	LoadStruct(Dst, Src.Values, Src.Schema.Id, Batch);
}

void ConstructAndLoadStruct(uint8* Dst, FByteReader Src, FStructSchemaId Id, const FLoadBatch& Batch)
{
	FLoadStructPlan Plan = Batch[Id];
	checkf(!Plan.IsSchema(), TEXT("Non-default constructible types requires ICustomStructBinding or in rare cases memcpying"));

	if (Plan.IsMemcpy())
	{
		Src.CheckSize(Plan.AsMemcpy().Size);
		FMemory::Memcpy(Dst + Plan.AsMemcpy().Offset, Src.Peek(), Plan.AsMemcpy().Size);
	}
	else
	{
		FStructSchemaHandle ReadSchema{Id, Batch.ReadId};
		Plan.AsCustom().LoadStruct(Dst, { ReadSchema, Src }, ECustomLoadMethod::Construct, Batch);
	}
}

void LoadRange(uint8* Dst, FRangeView Src, ERangeSizeType MaxSize, TConstArrayView<FRangeBinding> Bindings, const FLoadBatch& Batch)
{
	FRangeLoader::LoadRangeView(Dst, Src, MaxSize, Bindings, Batch);
}


} // namespace PlainProps