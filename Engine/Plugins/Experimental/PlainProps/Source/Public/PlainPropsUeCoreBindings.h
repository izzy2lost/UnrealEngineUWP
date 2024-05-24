// Copyright Epic Games, Inc. All Rights Reserved.
#if 0
#pragma once

#include "Containers/Set.h"
#include "PlainPropsBind.h"
#include "PlainPropsLoad.h"
#include "PlainPropsRead.h"
#include "PlainPropsIndex.h"
#include "UObject/NameTypes.h"

namespace PlainProps
{
	template<>
	void AppendString(FString& Out, const FName& Name) { Name.AppendString(Out); }
}

// Todo: PP_NAME_STRUCT(, FName);
struct FName_Ctti { static constexpr char Name[] = "FName"; };
FName_Ctti CttiOfPtr(FName*);

namespace PlainProps::UE
{

//class FReflection
//{
//	TIdIndexer<FName>	Names;
//	FRuntime			Types;
//
//public:
//	TIdIndexer<FName>&		GetIds() { return Names; }

	//template<typename Ctti>
	//FStructSchemaId			BindStruct();

	//template<typename Ctti>
	//FStructSchemaId			BindStructInterlaced(TConstArrayView<FMemberBinding> NonCttiMembers);
	//FStructSchemaId			BindStruct(FStructSchemaId Id, const ICustomBinding& Custom);
	//FStructSchemaId			BindStruct(FTypeId Type, FOptionalSchemaId Super, TConstArrayView<FNamedMemberBinding> Members, EMemberPresence Occupancy);
	//void					DropStruct(FStructSchemaId Id) { Types.DropStruct(Id); }

	//template<typename Ctti>
	//FEnumSchemaId			BindEnum();
	//FEnumSchemaId			BindEnum(FTypeId Type, EEnumMode Mode, ELeafWidth Width, TConstArrayView<FEnumerator> Enumerators);
	//void					DropEnum(FEnumSchemaId Id) { Types.DropEnum(Id); }
//};
//
//PLAINPROPS_API FReflection GReflection;
//
//struct FIds
//{
//	static FMemberId		IndexMember(FAnsiStringView Name)			{ return GReflection.GetIds().NameMember(FName(Name)); }
//	static FTypenameId		IndexTypename(FAnsiStringView Name)			{ return GReflection.GetIds().MakeTypename(FName(Name)); }
//	static FScopeId			IndexCurrentModule()						{ return GReflection.GetIds().MakeScope(FName(UE_MODULE_NAME)); }
//	static FTypeId			IndexNativeType(FAnsiStringView Typename)	{ return {IndexCurrentModule(), IndexTypename(Typename)}; }
//	static FEnumSchemaId	IndexEnum(FTypeId Name)						;
//	static FEnumSchemaId	IndexEnum(FAnsiStringView Name)				;
//	static FStructSchemaId	IndexStruct(FTypeId Name)					;
//	static FStructSchemaId	IndexStruct(FAnsiStringView Name)			;
//	
//};

// todo: use generic cached instance template?
//template<class Ids>
//FScopeId GetModuleScope()
//{
//	static FScopeId Id = Ids::IndexScope(UE_MODULE_NAME);
//	return Id;
//}

//template<class Ctti>
//class TBindRtti
//{
//	FSchemaId Id;
//public:
//	TBindRtti() : Id(BindRtti<Ctti, FIds>(GReflection.GetTypes()))
//	{}
//
//	~TBindRtti()
//	{
//		if constexpr (std::is_enum_v<Ctti::Type>)
//		{
//			GReflection.DropEnum(static_cast<FEnumSchemaId>(Id));
//		}
//		else
//		{
//			GReflection.DropStruct(static_cast<FStructSchemaId>(Id));
//		}
//	}
//};

} // namespace PlainProps::UE

//#define UEPP_BIND_STRUCT(T) 
	
//////////////////////////////////////////////////////////////////////////
// Below container bindings should be moved to some suitable header
//////////////////////////////////////////////////////////////////////////

#include "Containers/Array.h"
#include "Containers/Set.h"
#include "Templates/UniquePtr.h"

namespace PlainProps::UE
{

template <typename T>
struct TArrayBinding : public IItemRangeBinding
{
	using SizeType = int32;
	using ItemType = T;

	virtual void MakeItems(FLoadRangeContext& Ctx) const override
	{
		TArray<T>& Array = Ctx.Request.GetRange<TArray<T>>();
		if constexpr (std::is_default_constructible_v<T>)
		{
			Array.SetNum(Ctx.Request.NumTotal());
		}
		else
		{
			Array.SetNumUninitialized(Ctx.Request.NumTotal());
			Ctx.Items.SetUnconstructed();
		}
		
		Ctx.Items.Set(Array.GetData(), Ctx.Request.NumTotal());
	}

	virtual void ReadItems(FSaveRangeContext& Ctx) const override
	{
		const TArray<T>& Array = Ctx.Request.GetRange<TArray<T>>();
		Ctx.Items.SetAll(Array.GetData(), static_cast<uint64>(Array.Num()));
	}
};

//////////////////////////////////////////////////////////////////////////

struct FStringBinding : public ILeafRangeBinding
{
	using SizeType = int32;
	using ItemType = char8_t;

	virtual void SaveLeaves(const void* Range, FLeafRangeAllocator& Out) const override
	{
		const TArray<TCHAR>& Src = static_cast<const FString*>(Range)->GetCharArray();
		int32 SrcLen = Src.Num() - 1;
		if (SrcLen <= 0)
		{
		}
		else if constexpr (sizeof(TCHAR) == sizeof(char8_t))
		{
			char8_t* Utf8 = Out.AllocateRange<char8_t>(SrcLen);
			FMemory::Memcpy(Utf8, Src.GetData(), SrcLen);
		}
		else
		{
			int32 Utf8Len = FPlatformString::ConvertedLength<UTF8CHAR>(Src.GetData(), SrcLen);
			char8_t* Utf8 = Out.AllocateRange<char8_t>(Utf8Len);
			UTF8CHAR* Utf8End = FPlatformString::Convert(reinterpret_cast<UTF8CHAR*>(Utf8), Utf8Len, Src.GetData(), SrcLen);	
			check((char8_t*)Utf8End - Utf8 == Utf8Len);
		}
	}

	virtual void LoadLeaves(void* Range, FLeafRangeLoadView Items) const override
	{
		TArray<TCHAR>& Dst = static_cast<FString*>(Range)->GetCharArray();
		TRangeView<char8_t> Utf8 = Items.As<char8_t>();
		const UTF8CHAR* Src = reinterpret_cast<const UTF8CHAR*>(Utf8.begin());
		int32 SrcLen = static_cast<int32>(Utf8.Num());
		if (SrcLen == 0)
		{
			Dst.Reset();
		}
		else if constexpr (sizeof(TCHAR) == sizeof(char8_t))
		{
			Dst.SetNum(SrcLen + 1);
			FMemory::Memcpy(Dst.GetData(), Src, SrcLen);
			Dst[SrcLen] = '\0';	
		}
		else
		{
			int32 DstLen = FPlatformString::ConvertedLength<TCHAR>(Src, SrcLen);
			Dst.SetNum(DstLen + 1);
			TCHAR* DstEnd = FPlatformString::Convert(Dst.GetData(), DstLen, Src, SrcLen);
			check(DstEnd - Dst.GetData() == DstLen);
			*DstEnd = '\0';
		}
	}

	virtual int64 DiffLeaves(const void* RangeA, const void* RangeB) const override
	{
		const FString& A = *static_cast<const FString*>(RangeA);
		const FString& B = *static_cast<const FString*>(RangeB);
		int32 ALen = A.Len();
		int32 BLen = B.Len();

		if (int32 LenDiff = ALen - BLen)
		{
			return LenDiff;
		}

		// Case-sensitive comparison
		return ALen ? FMemory::Memcmp(A.GetCharArray().GetData(), B.GetCharArray().GetData(), ALen * sizeof(TCHAR)) : 0;
	}
};

//////////////////////////////////////////////////////////////////////////

template <typename T>
struct TUniquePtrBinding : public IItemRangeBinding
{
	using SizeType = bool;
	using ItemType = T;

	virtual void MakeItems(FLoadRangeContext& Ctx) const override
	{
		TUniquePtr<T>& Ptr = Ctx.Request.GetRange<TUniquePtr<T>>();
		
		if (Ctx.Request.NumTotal() == 0)
		{
			Ptr.Reset();
			return;
		}
		
		if (!Ptr)
		{
			if constexpr (std::is_default_constructible_v<T>)
			{
				Ptr.Reset(new T);
			}
			else
			{
				Ptr.Reset(reinterpret_cast<T*>(FMemory::Malloc(sizeof(T), alignof(T))));
				Ctx.Items.SetUnconstructed();
			}
		}
		
		Ctx.Items.Set(Ptr.Get(), 1);
	}

	virtual void ReadItems(FSaveRangeContext& Ctx) const override
	{
		const TUniquePtr<T>& Ptr = Ctx.Request.GetRange<TUniquePtr<T>>();
		Ctx.Items.SetAll(Ptr.Get(), Ptr ? 1 : 0);
	}

};

//////////////////////////////////////////////////////////////////////////

template <typename T>
struct TSetBinding : public IItemRangeBinding
{
	using SizeType = int32;
	using ItemType = T;

	virtual void MakeItems(FLoadRangeContext& Ctx) const override
	{
		const TSet<T>& Set = *Ctx.Request.GetRange<TSet<T>>();
		SizeType Num = static_cast<SizeType>(Ctx.Request.NumTotal());

		static constexpr bool bAllocate = sizeof(T) > sizeof(FLoadRangeContext::Scratch);
		static constexpr uint64 MaxItems = bAllocate ? sizeof(FLoadRangeContext::Scratch) / sizeof(T) : 1;
		
		if (Ctx.Request.IsFirstCall())
		{
			Set.Reset();

			if (uint64 NumRequested = Ctx.Request.NumTotal())
			{
				Set.Reserve(NumRequested);

				// Create temporary buffer
				uint64 NumTmp = FMath::Min(MaxItems, NumRequested);
				void* Tmp = bAllocate ? FMemory::Malloc(sizeof(T)) : Ctx.Scratch;
				Ctx.Items.Set(Tmp, NumTmp, sizeof(T));
				if constexpr (std::is_default_constructible_v<T>)
				{
					for (T* It = static_cast<T*>(Tmp), End = It + NumTmp; It != End; ++It)
					{
						::new (It) T;
					}
				}
				else
				{
					Ctx.Items.SetUnconstructed();
				}

				Ctx.Items.RequestFinalCall();
			}
		}
		else
		{
			// Add items that have been loaded
			TArrayView<T> Tmp = Ctx.Items.Get<T>();
			for (T& Item : Tmp)
			{
				Set.Emplace(MoveTemp(Item));
			}

			if (Ctx.Request.IsFinalCall())
			{
				// Destroy and free temporaries
				uint64 NumTmp = FMath::Min(MaxItems, Ctx.Request.NumTotal());
				for (T& Item : MakeArrayView(Tmp.GetData(), NumTmp))
				{
					Item.~T();
				}
				if constexpr (bAllocate)
				{
					FMemory::Free(Tmp.GetData());
				}	
			}
			else
			{
				Ctx.Items.Set(Tmp.Slice(0, FMath::Min(static_cast<uint64>(Tmp.Num()), Ctx.Request.NumMore())));
				check(Ctx.Items.Get<T>().Num());
			}
		}
	}

	virtual void ReadItems(FSaveRangeContext& Ctx) const override
	{
		static_assert(offsetof(TSetElement<T>, Value) == 0);
		const TSparseArray<TSetElement<T>>& Elems = *Ctx.Request.GetRange<TSparseArray<TSetElement<T>>>();

		if (int32 NumRead = static_cast<int32>(Ctx.Items.NumTotal))
		{
			// Continue partial response
			const TSetElement<T>* NextElem = static_cast<const TSetElement<T>*>(Ctx.Items.Slice) + Ctx.Items.Slice.Num + /* skip known invalid */ 1;
			Ctx.Items.SetPart(GetContiguousSlice(Elems.PointerToIndex(NextElem), Elems));
		}
		else if (Elems.IsCompact())
		{
			Ctx.Items.SetAll(Elems.GetData(), Elems.Num());
		}
		else
		{
			// Start partial response
			Ctx.Items.Stride = sizeof(TSetElement<T>);
			Ctx.Items.SetPart(GetContiguousSlice(0, Elems));
		}
	}

	FExistingItemSlice GetContiguousSlice(int32 Idx, const TSparseArray<TSetElement<T>>& Elems)
	{
		int32 Num = 1;
		for (;!Elems.IsValidIndex(Idx); ++Idx) {}
		for (; Elems.IsValidIndex(Idx + Num); ++Num) {}
		return { &Elems[Idx], Num };
	}
};

//////////////////////////////////////////////////////////////////////////
struct FSetOps
{
	union
	{
		struct
		{
			FMemberId Add;
			FMemberId Del;
			FMemberId Set;
		};

		FMemberId All[3];
	};
	
	template<class Ids>	
	static const FSetOps& Get()
	{
		static FSetOps Ops = {Ids::IndexMember("Add"), Ids::IndexMember("Del"), Ids::IndexMember("Set")};
		return Ops;
	}
};

template <class Ids, typename T>
struct TSetDeltaBinding : public ICustomBinding
{
	using Type = TSet<T>;
	static constexpr EMemberPresence Occupancy = EMemberPresence::AllowSparse;
	
	static TConstArrayView<FMemberId> GetMemberIds() { return FSetOps::Get<Ids>().All; }

	virtual void SaveStruct(FMemberBuilder& Dst, const void* Src, const void* Default, const FDebugIds& Debug) override;

	virtual void LoadStruct(void* Dst, FStructView Src, ECustomLoadMethod Method, const FLoadBatch& Batch) const override
	{
		Type& Out = *static_cast<Type*>(Dst);
		FMemberReader Members(Src);

		if (Method == ECustomLoadMethod::Construct)
		{
			::new (Dst) Type;
		}
				
		if (!Members.HasMore())
		{
			return;
		}

		FSetOps Ops = FSetOps::Get<Ids>();
		FMemberId Name = Members.PeekName().Get();
		FRangeView Items = Members.GrabRange();
		int32 NumItems = static_cast<int32>(Items.Num());
		if (Name == Ops.Set)
		{
			Out.Empty(NumItems);
			AddItems(Out, Items);
		}
		else if (Name == Ops.Add)
		{
			Out.Reserve(Out.Num() + NumItems);
			AddItems(Out, Items);
		}
		else if (Name == Ops.Del)
		{
			RemoveItems(Out, NumItems);
		
			if (Members.HasMore())
			{
				check(Members.PeekName() == Ops.Add);
				Items = Members.GrabRange();
				Out.Reserve(Out.Num() + static_cast<int32>(Items.Num()));
				AddItems(Out, Items);
			}
		}
		
		check(!Members.HasMore());
	}

	virtual bool DiffStruct(const void* StructA, const void* StructB) const override
	{
		const Type& A = *static_cast<const Type*>(StructA);
		const Type& B = *static_cast<const Type*>(StructB);
		if (A.Num() != B.Num())
		{
			return false;
		}

		for (const T& AKey : A)
		{
			if (!B.Contains(AKey))
			{
				return false;
			}
		}

		return true;
	}

	void AddItems(Type& Out, FRangeView Items, const FLoadBatch& Batch)
	{
		check(!Items.IsEmpty());

		if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>)
		{
			for (T Item : Items.AsLeaves().As<T>())
			{
				Out.Add(Item);
			}
		}
		else if (Items.IsStructRange())
		{
			FStructRangeView Structs = Items.AsStructs();
			for (FStructView Item : Structs)
			{
				if constexpr (std::is_default_constructible_v<T>())
				{
					T Tmp;
					PlainProps::LoadStruct(&Tmp, Item, Batch);	
					Out.Emplace(MoveTemp(Item));
				}
				else
				{
					alignas(T) uint8 Buffer[sizeof(T)];
					PlainProps::ConstructAndLoadStruct(Buffer, Item, Batch);
					T* Tmp = reinterpret_cast<T*>(Buffer);
					Out.Emplace(MoveTemp(*Tmp));
					Tmp->~T();
				}
			}
		}
		else if constexpr (std::is_default_constructible_v<T>())
		{
			using Binding = RangeBind<T>;
			const IItemRangeBinding* Bindings[] = {};// ... generate somehow ... };
			T Tmp;
			for (FRangeView Item : Items.AsRanges())
			{
				LoadRange(&Tmp, Item, Bindings, Batch);
				Out.Emplace(MoveTemp(*Tmp));
			}
		}
		else
		{
			check(Items.IsNestedRange());
			checkf(false, TEXT("Ranges must be default-constructible"));
		}
	}
};

//	template <typename T, typename VariantType>
//	struct TVariantConstructFromMember
//	{
//		/** Default construct the type and load it from the FArchive */
//		static void Construct(VariantType& Dst, FMemberReader Src)
//		{
//			if constexpr (std::is_arithmetic_v<T>)
//			{
//				Dst.template Emplace<T>(Src.GrabLeaf().As<T>());
//			}
//			else constexpr 
//
//		}
//	};
//
//template <typename... Ts>
//struct TVariantBinding : public ICustomBinding
//{
//	using VariantType = TVariant<Ts...>;
//
//	const FStructDeclaration& Declaration;
//
//	static constexpr void(*Loaders[])(FMemberReader&, VariantType&) = { &TLoader<Ts, VariantType>::Load... };
//
//	virtual void LoadStruct(void* Dst, FStructView Src, ECustomLoadMethod Method, const FLoadBatch& Batch) const override
//	{
//		VariantType& Variant = *reinterpret_cast<VariantType*>(Dst);
//
//		if (Method == ECustomLoadMethod::Assign)
//		{
//			Variant.~VariantType();
//		}
//
//		FMemberReader Members(Src);
//		const FMemberId* DeclaredName = Algo::Find(Declaration.Names, Members.PeekName());
//		check(DeclaredName);
//		int64 Idx = DeclaredName - &Declaration.Names[0];
//
//		check(TypeIndex < UE_ARRAY_COUNT(Loaders));
//		Loaders[TypeIndex](Ar, OutVariant);		
//		
//	}
//
//	template<typename T>
//	void Load(TVariant<Ts...>& Dst, FCustomMemberLoader& Src, ECustomLoadMethod Method)
//	{
//
//		
//		{
//			Dst.Emplace(MoveTemp(*reinterpret_cast<T*>(Temp)));
//		}
//		else
//		{
//			new (Dst) TVariant<Ts...>(MoveTemp(*reinterpret_cast<T*>(Temp)));
//		}
//	}
//
//	virtual TUniquePtr<FBuiltStruct>	SaveStruct(const void* Src, const FDebugIds& Debug) const override
//	{
//		...
//	}
//};

}

namespace PlainProps
{
	template<typename T>
	struct TRangeBind<TArray<T>>
	{
		using Type = UE::TArrayBinding<T>;
	};

	template<>
	struct TRangeBind<FString>
	{
		using Type = UE::FStringBinding;
	};

	template<typename T>
	struct TRangeBind<TUniquePtr<T>>
	{
		using Type = UE::TUniquePtrBinding<T>;
	};

	template<typename T>
	struct TRangeBind<TSet<T>>
	{
		using Type = UE::TSetBinding<T>;
	};
}
#endif