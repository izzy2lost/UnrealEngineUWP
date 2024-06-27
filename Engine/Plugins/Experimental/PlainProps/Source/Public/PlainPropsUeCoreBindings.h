// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Set.h"
#include "Math/Quat.h"
#include "Math/Vector.h"
#include "Math/Transform.h"
#include "Misc/Optional.h"
#include "PlainPropsBind.h"
#include "PlainPropsLoad.h"
#include "PlainPropsRead.h"
#include "PlainPropsIndex.h"
#include "UObject/NameTypes.h"


PP_NAME_STRUCT(, FName);
PP_NAME_STRUCT_TEMPLATE(, TSet);
PP_REFLECT_STRUCT_TEMPLATE(, TTuple, void, Key, Value); // Todo handle TTuple and higher arities

namespace UE::Math
{
PP_REFLECT_STRUCT(, FVector, void, X, Y, Z);
PP_REFLECT_STRUCT(, FVector4, void, X, Y, Z, W);
PP_REFLECT_STRUCT(, FQuat, void, X, Y, Z, W);
PP_NAME_STRUCT(, FTransform);
}

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

template <typename T, class Allocator>
struct TArrayBinding : public IItemRangeBinding
{
	using SizeType = int32;
	using ItemType = T;
	using ArrayType = TArray<T, Allocator>;

	virtual void MakeItems(FLoadRangeContext& Ctx) const override
	{
		ArrayType& Array = Ctx.Request.GetRange<ArrayType>();
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
		const ArrayType& Array = Ctx.Request.GetRange<ArrayType>();
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
struct TOptionalBinding : public IItemRangeBinding
{
	using SizeType = bool;
	using ItemType = T;

	virtual void MakeItems(FLoadRangeContext& Ctx) const override
	{
		TOptional<T>& Opt = Ctx.Request.GetRange<TOptional<T>>();
		
		if (Ctx.Request.NumTotal() == 0)
		{
			Opt.Reset();
		}
		else if constexpr (std::is_default_constructible_v<T>)
		{
			if (!Opt)
			{
				Opt.Emplace();
			}
			Ctx.Items.Set(reinterpret_cast<T*>(&Opt), 1);
		}
		else if (Opt)
		{
			Ctx.Items.Set(reinterpret_cast<T*>(&Opt), 1);
		}
		else
		{
			if (Ctx.Request.IsFirstCall())
			{
				Ctx.Items.SetUnconstructed();
				Ctx.Items.RequestFinalCall();
				Ctx.Items.Set(reinterpret_cast<T*>(&Opt), 1);	
			}
			else
			{
				// Move-construct from self reference
				Opt.Emplace(reinterpret_cast<T&&>(Opt));
			}
		}
	}

	virtual void ReadItems(FSaveRangeContext& Ctx) const override
	{
		const TOptional<T>& Opt = Ctx.Request.GetRange<TOptional<T>>();
		check(!Opt || reinterpret_cast<const T*>(&Opt) == &Opt.GetValue());
		Ctx.Items.SetAll(reinterpret_cast<const T*>(Opt ? &Opt : nullptr), Opt ? 1 : 0);
	}
};

//////////////////////////////////////////////////////////////////////////

template <typename T, typename KeyFuncs, typename SetAllocator>
struct TSetBinding : public IItemRangeBinding
{
	using SizeType = int32;
	using ItemType = T;
	using SetType = TSet<T, KeyFuncs, SetAllocator>;

	virtual void MakeItems(FLoadRangeContext& Ctx) const override
	{
		SetType& Set = Ctx.Request.GetRange<SetType>();
		SizeType Num = static_cast<SizeType>(Ctx.Request.NumTotal());

		static constexpr bool bAllocate = sizeof(T) > sizeof(FLoadRangeContext::Scratch);
		static constexpr uint64 MaxItems = bAllocate ? 1 : sizeof(FLoadRangeContext::Scratch) / SIZE_T(sizeof(T));
		
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
					for (T* It = static_cast<T*>(Tmp), *End = It + NumTmp; It != End; ++It)
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
				Ctx.Items.Set(Tmp.GetData(), FMath::Min(static_cast<uint64>(Tmp.Num()), Ctx.Request.NumMore()));
				check(Ctx.Items.Get<T>().Num());
			}
		}
	}

	virtual void ReadItems(FSaveRangeContext& Ctx) const override
	{
		static_assert(offsetof(TSetElement<T>, Value) == 0);
		const TSparseArray<TSetElement<T>>& Elems = Ctx.Request.GetRange<TSparseArray<TSetElement<T>>>();

		if (FExistingItemSlice LastRead = Ctx.Items.Slice)
		{
			// Continue partial response
			const TSetElement<T>* NextElem = static_cast<const TSetElement<T>*>(LastRead.Data) + LastRead.Num + /* skip known invalid */ 1;
			Ctx.Items.Slice = GetContiguousSlice(Elems.PointerToIndex(NextElem), Elems);
		}
		else if (Elems.IsCompact())
		{
			int32 Num = Elems.Num();
			Ctx.Items.SetAll(Num ? &Elems[0] : nullptr, Num);
		}
		else
		{
			// Start partial response
			Ctx.Items.NumTotal = Elems.Num();
			Ctx.Items.Stride = sizeof(TSetElement<T>);
			Ctx.Items.Slice = GetContiguousSlice(0, Elems);
		}
	}

	static FExistingItemSlice GetContiguousSlice(int32 Idx, const TSparseArray<TSetElement<T>>& Elems)
	{
		int32 Num = 1;
		for (;!Elems.IsValidIndex(Idx); ++Idx) {}
		for (; Elems.IsValidIndex(Idx + Num); ++Num) {}
		return { &Elems[Idx], static_cast<uint64>(Num) };
	}
};

//////////////////////////////////////////////////////////////////////////

template <typename K, typename V, typename SetAllocator, typename KeyFuncs>
struct TMapBinding : public TSetBinding<TPair<K, V>, KeyFuncs, SetAllocator>
{};

//////////////////////////////////////////////////////////////////////////

//TODO: macroify, e.g PP_CUSTOM_BIND(PLAINPROPS_API, FTransform, Transform, Translate, Rotate, Scale)
struct FTransformBinding : ICustomBinding
{
	using Type = FTransform;
	inline static constexpr EMemberPresence Occupancy = EMemberPresence::AllowSparse;
	enum class EMember : uint8 { Translate, Rotate, Scale };
	FMemberId MemberIds[3];
	FStructSchemaId VectorId;
	FStructSchemaId QuatId;

	template<typename Ids>
	void InitIds(/*const FDeclarations& Declared*/) 
	{
		MemberIds[(uint8)EMember::Translate] = Ids::IndexMember("Translate");
		MemberIds[(uint8)EMember::Rotate] = Ids::IndexMember("Rotate");
		MemberIds[(uint8)EMember::Scale] = Ids::IndexMember("Scale");

		VectorId = IndexStruct<FVector, Ids>();
		QuatId = IndexStruct<FQuat, Ids>();
	}

	PLAINPROPS_API void	Save(FMemberBuilder& Dst, const FTransform& Src, const FTransform* Default, const FSaveContext& Context) const;
	PLAINPROPS_API void	Load(FTransform& Dst, FStructView Src, ECustomLoadMethod Method, const FLoadBatch& Batch) const;
	inline static bool	Diff(const FTransform& A, const FTransform& B) { return !A.Equals(B, 0.0); }
};

//////////////////////////////////////////////////////////////////////////

struct FSetDeltaOps
{
	union
	{
		FMemberId MemberIds[3] = {};
		struct { FMemberId Add, Del, Set; } Ops;
	};
	
	template<class Ids>	
	void InitIds()
	{
		static FSetDeltaOps Cache = {.MemberIds = {Ids::IndexMember("Add"), Ids::IndexMember("Del"), Ids::IndexMember("Set")}};
		*this = Cache;
	}
};

template <typename T, typename KeyFuncs, typename SetAllocator>
struct TSetDeltaBinding : ICustomBinding, FSetDeltaOps
{
	using Type = TSet<T, KeyFuncs, SetAllocator>;
	static constexpr EMemberPresence Occupancy = EMemberPresence::AllowSparse;

	void Save(FMemberBuilder& Dst, const Type& Src, const Type* Default, const FSaveContext& Context) const
	{
		if (Default)
		{
			if (Default->IsEmpty())
			{
				// Todo: Add everything
			}
			else
			{
				// TODO: Range builder for missing items, iterate over defaults elements and check existence in Src
				// Dst.AddRange(Ops.Del, MissingItems) if non-empty;

				// TODO: Range builder for added items, iterate over defaults elements and check existence in Src
			}
		}
		else
		{
			// Dst.AddRange(Ops.Set, );
		}
	}

	inline void Load(Type& Dst, FStructView Src, ECustomLoadMethod Method, const FLoadBatch& Batch) const
	{
		FMemberReader Members(Src);

		if (Method == ECustomLoadMethod::Construct)
		{
			::new (&Dst) Type;
		}
				
		if (!Members.HasMore())
		{
			return;
		}

		FMemberId Name = Members.PeekName().Get();
		FRangeView Items = Members.GrabRange();
		int32 NumItems = static_cast<int32>(Items.Num());
		if (Name == Ops.Set)
		{
			Dst.Empty(NumItems);
			AddItems(Dst, Items, Batch);
		}
		else if (Name == Ops.Add)
		{
			Dst.Reserve(Dst.Num() + NumItems);
			AddItems(Dst, Items, Batch);
		}
		else if (Name == Ops.Del)
		{
			RemoveItems(Dst, Items);
		
			if (Members.HasMore())
			{
				check(Members.PeekName() == Ops.Add);
				Items = Members.GrabRange();
				Dst.Reserve(Dst.Num() + static_cast<int32>(Items.Num()));
				AddItems(Dst, Items, Batch);
			}
		}
		
		check(!Members.HasMore());
	}

	inline static bool Diff(const Type& A, const Type& B)
	{
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

	static void AddItems(Type& Out, FRangeView Items, const FLoadBatch& Batch)
	{
		check(!Items.IsEmpty());

		if constexpr (LeafType<T>)
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
				if constexpr (std::is_default_constructible_v<T>)
				{
					T Tmp;
					PlainProps::LoadStruct(&Tmp, Item, Batch);	
					Out.Emplace(MoveTemp(Tmp));
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
		else // Nested range
		{
			using RangeBinding = RangeBind<T>;
			if constexpr (std::is_default_constructible_v<T> && !std::is_void_v<RangeBinding>)
			{
				static constexpr ERangeSizeType MaxSize = RangeSizeOf(typename RangeBinding::SizeType{});
				//const IItemRangeBinding* Bindings[] = {};// TODO ... generate somehow ... ;
				T Tmp;
				for (FRangeView Item : Items.AsRanges())
				{
				//	LoadRange(&Tmp, Item, MaxSize, Bindings, Batch);
				//	Out.Emplace(MoveTemp(*Tmp));
				}
			}
			else
			{
				check(Items.IsNestedRange());
				checkf(std::is_default_constructible_v<T>, TEXT("Ranges must be default-constructible"));
				checkf(!std::is_void_v<RangeBinding>, TEXT("Inner range type unbound (no TRangeBind specialization)"));
			}
		}
	}

	static void RemoveItems(Type& Out, FRangeView Items)
	{
		// TODO copy paste from AddItems or template AddItems -> ApplyItems<EOp::Add/Del>
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
//struct TVariantBinding : ICustomBinding
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
//	virtual FBuiltStructPtr	SaveStruct(const void* Src, const FDebugIds& Debug) const override
//	{
//		...
//	}
//};

}

namespace PlainProps
{

template<>
PLAINPROPS_API void AppendString(FString& Out, const FName& Name);

template<typename T, typename Allocator>
struct TRangeBind<TArray<T, Allocator>>
{
	using Type = UE::TArrayBinding<T, Allocator>;
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

template <typename T, typename KeyFuncs, typename SetAllocator>
struct TRangeBind<TSet<T, KeyFuncs, SetAllocator>>
{
	using Type = UE::TSetBinding<T, KeyFuncs, SetAllocator>;
};

template <typename K, typename V, typename SetAllocator, typename KeyFuncs>
struct TRangeBind<TMap<K, V, SetAllocator, KeyFuncs>>
{
	using Type = UE::TMapBinding<K, V, SetAllocator, KeyFuncs>;
};

template<typename T>
struct TRangeBind<TOptional<T>>
{
	using Type = UE::TOptionalBinding<T>;
};


template<>
struct TCustomBind<FTransform>
{
	using Type = UE::FTransformBinding;
};

} // namespace PlainProps