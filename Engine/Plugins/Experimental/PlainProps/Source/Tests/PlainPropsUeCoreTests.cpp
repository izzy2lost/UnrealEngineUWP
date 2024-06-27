// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_TESTS

#include "PlainPropsBuildSchema.h"
#include "PlainPropsCtti.h"
#include "PlainPropsIndex.h"
#include "PlainPropsInternalBuild.h"
#include "PlainPropsInternalFormat.h"
#include "PlainPropsLoad.h"
#include "PlainPropsRead.h"
#include "PlainPropsSave.h"
#include "PlainPropsWrite.h"
#include "PlainPropsUeCoreBindings.h"
#include "Algo/Compare.h"
#include "Containers/StringView.h"
#include "Containers/Map.h"
#include "Math/Transform.h"
#include "Templates/UnrealTemplate.h"
#include "Tests/TestHarnessAdapter.h"

namespace PlainProps::UE::Test
{

static TIdIndexer<FName>	GNames;
static FDeclarations		GTypes(/* debug */ GNames);
static FSchemaBindings		GSchemas(/* debug */ GNames);
static FCustomBindings		GCustoms(/* debug */ GNames);
static FCustomBindings		GDeltaCustoms(/* debug */ GNames, /* base*/ &GCustoms);

struct FIds
{
	static FNameId			IndexName(FAnsiStringView Name)				{ return GNames.MakeName(FName(Name)); }
	static FMemberId		IndexMember(FAnsiStringView Name)			{ return GNames.NameMember(FName(Name)); }
	static FTypenameId		IndexTypename(FAnsiStringView Name)			{ return GNames.MakeTypename(FName(Name)); }
	static FScopeId			IndexNativeScope()							{ return GNames.MakeScope(FName(UE_MODULE_NAME)); }
	static FTypeId			IndexNativeType(FAnsiStringView Typename)	{ return {IndexNativeScope(), IndexTypename(Typename)}; }
	static FEnumSchemaId	IndexEnum(FTypeId Type)						{ return GNames.IndexEnum(Type); }
	static FEnumSchemaId	IndexEnum(FAnsiStringView Name)				{ return IndexEnum(IndexNativeType(Name)); }
	static FStructSchemaId	IndexStruct(FTypeId Type)					{ return GNames.IndexStruct(Type); }
	static FStructSchemaId	IndexStruct(FAnsiStringView Name)			{ return IndexStruct(IndexNativeType(Name)); }
	static FIdIndexerBase&	GetIndexer()								{ return GNames; }
	static const FDebugIds& GetDebug()									{ return GNames; }
};

////////////////////////////////////////////////////////////////////////////////////////////////

struct FDefaultRuntime
{
	using Ids = FIds;
	template<class T> using CustomBindings = TCustomBind<T>;

	static FDeclarations&			GetTypes()			{ return GTypes; }
	static FSchemaBindings&			GetSchemas()		{ return GSchemas; }
	static FCustomBindings&			GetCustoms()		{ return GCustoms; }
};


////////////////////////////////////////////////////////////////////////////////////////////////

template<class T>
struct TCustomDeltaBind
{
	using Type = void;
};

template <typename T, typename KeyFuncs, typename SetAllocator>
struct TCustomDeltaBind<TSet<T, KeyFuncs, SetAllocator>>
{
	using Type = UE::TSetDeltaBinding<T, KeyFuncs, SetAllocator>;
};

//template <typename K, typename V, typename SetAllocator, typename KeyFuncs>
//struct TCustomDeltaBind<TMap<K, V, SetAllocator, KeyFuncs>>
//{
//	using Type = UE::TMapDeltaBinding<K, V, SetAllocator, KeyFuncs>;
//};

struct FDeltaRuntime : FDefaultRuntime
{
	template<class T> using CustomBindings = TCustomDeltaBind<T>;

	static FCustomBindings&			GetCustoms()		{ return GDeltaCustoms; }
};

////////////////////////////////////////////////////////////////////////////////////////////////

template<typename Enum, EEnumMode Mode, class Runtime = FDefaultRuntime>
struct TScopedEnumDeclaration
{
	using Ids = typename Runtime::Ids;
	using Ctti = CttiOf<Enum>;

	FEnumSchemaId Id;
	TScopedEnumDeclaration() : Id(DeclareNativeEnum<Ctti, Ids>(Runtime::GetTypes(), Mode)) {}
	~TScopedEnumDeclaration() { Runtime::GetTypes().DropEnum(Id); }
};

template<class T, EMemberPresence Occupancy = EMemberPresence::AllowSparse, class Runtime = FDefaultRuntime>
struct TScopedStructDeclaration
{
	using Ids = typename Runtime::Ids;

	FStructSchemaId Id;

	TScopedStructDeclaration()
	: Id(DeclareNativeStruct<CttiOf<T>, Ids>(Runtime::GetTypes(), Occupancy))
	{}

	~TScopedStructDeclaration()
	{
		Runtime::GetTypes().DropStruct(Id);
	}

	const FStructDeclaration& Get() const
	{
		return Runtime::GetTypes().Get(Id);
	}
};

template<class T, EMemberPresence Occupancy = EMemberPresence::AllowSparse, class Runtime = FDefaultRuntime>
struct TScopedStructBinding : TScopedStructDeclaration<T, Occupancy, Runtime>
{
	using TScopedStructDeclaration<T, Occupancy, Runtime>::Id;

	TScopedStructBinding()
	{
		BindNativeStruct<CttiOf<T>, Runtime>(Runtime::GetSchemas(), Id);
	}

	~TScopedStructBinding()
	{
		Runtime::GetSchemas().DropStruct(Id);
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////

// Maybe replace with macro, e.g. PP_DECLARE_CUSTOM_DENSE_STRUCT(FIds, FName, void, Idx)
struct FNameDeclaration 
{
	FStructSchemaId		Id;
	FMemberId			Idx;

	FNameDeclaration(FTypeId Type = FIds::IndexNativeType(CttiOf<FName>::Name))
	: Id(FIds::IndexStruct(Type))
	, Idx(FIds::IndexMember("Idx"))
	{
		GTypes.DeclareStruct(Id, Type, MakeArrayView(&Idx, 1), EMemberPresence::RequireAll);
	}

	~FNameDeclaration()
	{
		GTypes.DropStruct(Id);
	}
};

struct FTestCustomBinding : ICustomBinding
{
	virtual FStructSchemaId GetId() const = 0;
};

struct FNameBinding : FTestCustomBinding
{
	virtual void SaveCustom(FMemberBuilder& Dst, const void* Src, const void*, const FSaveContext& Ctx) override
	{
		FSetElementId Idx = Names.Add(*static_cast<const FName*>(Src));
		Dst.Add(Declaration.Idx, Idx.AsInteger());
	}

	virtual void LoadCustom(void* Dst, FStructView Src, ECustomLoadMethod, const FLoadBatch&) const override
	{
		FSetElementId Idx = FSetElementId::FromInteger(FMemberReader(Src).GrabLeaf().AsS32());
		*static_cast<FName*>(Dst) = Names.Get(Idx);
	}

	virtual bool DiffCustom(const void* StructA, const void* StructB) const override
	{
		FName A = *static_cast<const FName*>(StructA);
		FName B = *static_cast<const FName*>(StructB);
		return A.IsEqual(B, ENameCase::CaseSensitive);
	}

	virtual FStructSchemaId GetId() const override { return Declaration.Id; }

	FNameDeclaration	Declaration;
	TSet<FName>			Names;
};

////////////////////////////////////////////////////////////////////////////////////////////////

class FBatchSaver
{
public:
	FBatchSaver();

	template<class T>
	void						Save(T&& Object);
	template<class T>
	bool						SaveDelta(const T& Object, const T& Default);

	TArray64<uint8>				Write() const;

private:
	using IdBuiltStructPair = TPair<FStructSchemaId, FBuiltStructPtr>;
	TArray<IdBuiltStructPair>	SavedObjects;
	FNameBinding				SavedNames;
	FCustomBindings				Customs;
	mutable FScratchAllocator	Scratch;
};

FBatchSaver::FBatchSaver()
: Customs(/* debug */ GNames, &GCustoms)
{
	Customs.BindStruct(SavedNames.Declaration.Id, SavedNames);
}

template<class T>
void FBatchSaver::Save(T&& Object) 
{
	FStructSchemaId Id = IndexStruct<std::remove_reference_t<T>, FIds>();
	SavedObjects.Emplace(Id, SaveStruct(&Object, Id, {GTypes, GSchemas, Customs, Scratch}));
}

template<class T>
bool FBatchSaver::SaveDelta(const T& Object, const T& Default) 
{
	FStructSchemaId Id = IndexStruct<std::remove_reference_t<T>, FIds>();
	if (FBuiltStructPtr Delta = SaveStructDelta(&Object, &Default, Id, {GTypes, GSchemas, Customs, Scratch}))
	{
		SavedObjects.Emplace(Id, MoveTemp(Delta));
		return true;
	}
	return false;
}

template<typename ArrayType>
void WriteNumAndArray(TArray64<uint8>& Out, const ArrayType& Items)
{
	WriteU32(Out, IntCastChecked<uint32>(Items.Num()));
	WriteArray(Out, Items);
}

template<typename T>
TConstArrayView<T> GrabNumAndArray(/* in-out */ FByteReader& It)
{
	uint32 Num = It.Grab<uint32>();
	return MakeArrayView(reinterpret_cast<const T*>(It.GrabBytes(Num * sizeof(T))), Num);
}

inline constexpr uint32 Magics[] = { 0xFEEDF00D, 0xABCD1234, 0xDADADAAA, 0x99887766, 0xF0F1F2F3 , 0x00112233};

TArray64<uint8> FBatchSaver::Write() const
{
	// Build partial schemas
	FSchemasBuilder SchemaBuilders(GTypes, Scratch);
	for (const IdBuiltStructPair& Object : SavedObjects)
	{
		SchemaBuilders.NoteStructAndMembers(Object.Key, *Object.Value);
	}
	FBuiltSchemas Schemas = SchemaBuilders.Build(); 

	// Filter out declared but unused names and ids
	FWriter Writer(GNames, Schemas, ESchemaFormat::StableNames);
	TArray<FName> UsedNames;
	for (uint32 Idx = 0, Num = GNames.NumNames(); Idx < Num; ++Idx)
	{
		if (Writer.Uses({Idx}))
		{
			UsedNames.Add(GNames.ResolveName({Idx}));
		}
	}
	
	// Write ids. Just copying in-memory FNames, a stable format might use SaveNameBatch().
	TArray64<uint8> Out;
	WriteU32(Out, Magics[0]);
	WriteNumAndArray(Out, TArrayView<const FName, int32>(UsedNames));

	// Write schemas
	WriteU32(Out, Magics[1]);
	WriteAlignmentPadding<uint32>(Out);
	TArray64<uint8> Tmp;
	Writer.WriteSchemas(/* Out */ Tmp);
	WriteNumAndArray(Out, TArrayView<const uint8, int64>(Tmp));
	Tmp.Reset();

	// Write objects
	WriteU32(Out, Magics[2]);
	for (const TPair<FStructSchemaId, FBuiltStructPtr>& Object : SavedObjects)
	{
		WriteU32(/* out */ Tmp, Magics[3]);
		WriteU32(/* out */ Tmp, Writer.GetWriteId(Object.Key).Get().Idx);
		Writer.WriteMembers(/* out */ Tmp, Object.Key, *Object.Value);
		WriteSkippableSlice(Out, Tmp);
		Tmp.Reset();
	}

	// Write object terminator
	WriteSkippableSlice(Out, TConstArrayView64<uint8>());
	WriteU32(Out, Magics[4]);
	
	// Write names
	WriteNumAndArray(Out, SavedNames.Names.Array());
	WriteU32(Out, Magics[5]);
		
	return Out;
}

////////////////////////////////////////////////////////////////////////////////////////////////

class FBatchLoader
{
public:
	FBatchLoader(FMemoryView Data)
	: Customs(/* debug */ GNames, &GCustoms)
	{
		// Read ids
		FByteReader It(Data);
		CHECK(It.Grab<uint32>() == Magics[0]);
		Ids = GrabNumAndArray<FName>(It);
		CHECK(Ids.Num() != 0);
		
		// Read schemas
		CHECK(It.Grab<uint32>() == Magics[1]);
		It.SkipAlignmentPadding<uint32>();
		uint32 SchemasSize = It.Grab<uint32>();
		const FSchemaBatch* SavedSchemas = ValidateSchemas(It.GrabSlice(SchemasSize));
		CHECK(It.Grab<uint32>() == Magics[2]);
		
		// Bind saved ids to runtime ids, make new schemas with new ids and mount them
		FIdTranslator RuntimeIds(GNames, Ids, *SavedSchemas);
		FSchemaBatch* LoadSchemas = CreateTranslatedSchemas(*SavedSchemas, RuntimeIds.Translation);
		FReadBatchId Batch = MountReadSchemas(LoadSchemas);

		// Read objects
		while (uint64 NumBytes = It.GrabVarIntU())
		{	
			FByteReader ObjIt(It.GrabSlice(NumBytes));
			CHECK(ObjIt.Grab<uint32>() == Magics[3]);
			FStructSchemaId Schema = { ObjIt.Grab<uint32>() };
			Objects.Add({ { Schema, Batch }, ObjIt });
		}
		
		CHECK(It.Grab<uint32>() == Magics[4]);
		CHECK(!Objects.IsEmpty());

		// Read names and bind custom loader
		Names.Names.Append(GrabNumAndArray<FName>(It));
		Customs.BindStruct(Names.Declaration.Id, Names);
		CHECK(It.Grab<uint32>() == Magics[5]);

		// Finally create load plans
		TConstArrayView<FStructSchemaId> LoadStructIds = RuntimeIds.Translation.GetStructIds(SavedSchemas->NumStructSchemas);
		Plans = CreateLoadPlans(Batch, GTypes, Customs, GSchemas, LoadStructIds);
	}

	~FBatchLoader()
	{
		CHECK(LoadIdx == Objects.Num()); // Test should load all saved objects
		Plans.Reset();
		const FSchemaBatch* LoadSchemas = UnmountReadSchemas(Objects[0].Schema.Batch);
		DestroyTranslatedSchemas(LoadSchemas);
	}

	template<class T>
	T Load()
	{
		T Out;
		LoadInto(Out);
		return MoveTemp(Out);
	}

	template<class T>
	void LoadInto(T& Out)
	{
		FStructView In = Objects[LoadIdx++];
		LoadStruct(reinterpret_cast<uint8*>(&Out), In, *Plans);
	}

private:
	TConstArrayView<FName>		Ids;
	FNameBinding				Names;
	FCustomBindings				Customs;
	FLoadBatchPtr				Plans;
	TArray<FStructView>			Objects;
	int32						LoadIdx = 0;
};


static void Run(void (*Save)(FBatchSaver&), void (*Load)(FBatchLoader&))
{
	TArray64<uint8> Data;
	{
		FBatchSaver Batch;
		Save(Batch);
		Data = Batch.Write();
	}

	FBatchLoader Batch(MakeMemoryView(Data));
	Load(Batch);
}

//////////////////////////////////////////////////////////////////////////

//struct FNestedStructs
//{
//	FPt Point;
//	FPt StaticPoints[2];
//};
//UE_REFLECT_STRUCT(FNestedStructs, Point, StaticPoints);
//
//struct FLeafRanges
//{
//	TArray<int32> IntArray;
//	TOptional<uint8> MaybeByte;
//	TUniquePtr<float> FloatPtr;
//	TSet<uint16> ShortSet;
//	TSparseArray<bool> SparseBools;
//};
//
//struct FSuper
//{
//	uint16 Pad;
//	bool A;
//};
//UE_REFLECT_STRUCT(FSuper, A);
//
//struct FSub : FSuper
//{
//	bool B;
//	uint32 Pad;
//};
//UE_REFLECT_SUBSTRUCT(FSub, FSuper, B);
//
//
//struct FSubs
//{
//	TArray<FSub> Subs;
//};

struct FInt { int32 X; };
PP_REFLECT_STRUCT(PlainProps::UE::Test, FInt, void, X);
static bool operator==(FInt A, FInt B) { return A.X == B.X; }
inline uint32 GetTypeHash(FInt I) { return ::GetTypeHash(I.X); }

enum class EFlat1 : uint8 { A = 1, B = 3 };
enum class EFlat2 : uint8 { A, B };
enum class EFlag1 : uint8 { A = 2, B = 8, AB = 10 };
enum class EFlag2 : uint8 { A = 1, B = 2, AB = 3 };
PP_REFLECT_ENUM(PlainProps::UE::Test, EFlat1, A, B);
PP_REFLECT_ENUM(PlainProps::UE::Test, EFlat2, A, B);
PP_REFLECT_ENUM(PlainProps::UE::Test, EFlag1, A, B);
PP_REFLECT_ENUM(PlainProps::UE::Test, EFlag2, A, B);

struct FEnums
{
	EFlat1 Flat1;
	EFlat2 Flat2;
	EFlag1 Flag1;
	EFlag2 Flag2;

	friend bool operator==(FEnums, FEnums) = default;
};
PP_REFLECT_STRUCT(PlainProps::UE::Test, FEnums, void, Flat1, Flat2, Flag1, Flag2);

struct FLeafArrays
{
	TArray<bool> Bits;
	TArray<int>	Bobs;

	bool operator==(const FLeafArrays&) const = default;
};
PP_REFLECT_STRUCT(PlainProps::UE::Test, FLeafArrays, void, Bits, Bobs);

struct FComplexArrays
{
	TArray<char> Str;
	TArray<EFlat1> Enums;
	TArray<FLeafArrays> Misc;
	TArray<TArray<EFlat1>> Nested;

	bool operator==(const FComplexArrays&) const = default;
};
PP_REFLECT_STRUCT(PlainProps::UE::Test, FComplexArrays, void, Str, Enums, Misc, Nested);

struct FNames
{
	FName Name;
	TArray<FName> Names;

	bool operator==(const FNames&) const = default;
};
PP_REFLECT_STRUCT(PlainProps::UE::Test, FNames, void, Name, Names);

struct FStr
{
	FString S;
};
PP_REFLECT_STRUCT(PlainProps::UE::Test, FStr, void, S);

struct FNDC
{
	int X = -1;
	explicit FNDC(int I) : X(I) {}
	friend bool operator==(FNDC A, FNDC B) = default;
};
PP_REFLECT_STRUCT(PlainProps::UE::Test, FNDC, void, X);

struct FSets
{
	TSet<char> Leaves;
	TSet<TArray<uint8>> Ranges;
	TSet<FInt> Structs;
};
PP_REFLECT_STRUCT(PlainProps::UE::Test, FSets, void, Leaves, Ranges, Structs);

inline bool operator==(const FSets& A, const FSets& B)
{
	return LegacyCompareEqual(A.Leaves, B.Leaves) && LegacyCompareEqual(A.Ranges, B.Ranges) && LegacyCompareEqual(A.Structs, B.Structs);
}

struct FMaps
{
	TMap<bool, bool> Leaves;
	TMap<int, TArray<char>> Ranges;
	TMap<FInt, FNDC> Structs;
};
PP_REFLECT_STRUCT(PlainProps::UE::Test, FMaps, void, Leaves, Ranges, Structs);

inline bool operator==(const FMaps& A, const FMaps& B)
{
	return LegacyCompareEqual(A.Leaves, B.Leaves) && LegacyCompareEqual(A.Ranges, B.Ranges) && LegacyCompareEqual(A.Structs, B.Structs);
}

//////////////////////////////////////////////////////////////////////////

struct FUniquePtrs
{
	TUniquePtr<bool> Bit;
	TUniquePtr<FInt> Struct;
	TUniquePtr<TUniquePtr<int>> IntPtr;
	TArray<TUniquePtr<double>> Doubles;
};
PP_REFLECT_STRUCT(PlainProps::UE::Test, FUniquePtrs, void, Bit, Struct, IntPtr, Doubles);

template<typename T>
bool SameValue(const TUniquePtr<T>& A, const TUniquePtr<T>& B)
{
	return !A == !B && (!A || *A == *B); 
}

static bool operator==(const FUniquePtrs& A, const FUniquePtrs& B) 
{ 
	return	SameValue(A.Bit, B.Bit) &&
			SameValue(A.Struct, B.Struct) &&
			!A.IntPtr == !B.IntPtr && (!A.IntPtr || SameValue(*A.IntPtr, *B.IntPtr)) && 
			Algo::Compare(A.Doubles, B.Doubles, [](auto& X, auto& Y) { return SameValue(X, Y); });
}

template<typename T>
TUniquePtr<T> MakeOne(T&& Value)
{
	return MakeUnique<T>(MoveTemp(Value));
}

template<typename T>
TArray<TUniquePtr<T>> MakeTwo(T&& A, T&& B)
{
	TArray<TUniquePtr<T>> Out;
	Out.Add(MakeOne(MoveTemp(A)));
	Out.Add(MakeOne(MoveTemp(B)));
	return Out;
}

//////////////////////////////////////////////////////////////////////////

struct FNDCIntrusive : FNDC
{
	FNDCIntrusive() : FNDC(-1) {}
	explicit FNDCIntrusive(FIntrusiveUnsetOptionalState) : FNDC(-1) {}
	explicit FNDCIntrusive(int I) : FNDC(I) {}
	friend bool operator==(FNDCIntrusive, FNDCIntrusive) = default;
	bool operator==(FIntrusiveUnsetOptionalState) const { return X == -1;}
};
PP_REFLECT_STRUCT(PlainProps::UE::Test, FNDCIntrusive, void, X);

struct FOpts
{
	TOptional<bool> Bit;
	TOptional<FNDC> NDC;
	TOptional<FNDCIntrusive> NDCI;

	friend bool operator==(const FOpts&, const FOpts&) = default;
};
PP_REFLECT_STRUCT(PlainProps::UE::Test, FOpts, void, Bit, NDC, NDCI);

//////////////////////////////////////////////////////////////////////////

struct FDelta
{
	bool		A = true;
	float		B = 1.0;
	FInt		C = { 2 };
	TArray<int> D;
	FString		E = "!";

	friend bool operator==(const FDelta& A, const FDelta& B) = default;
};
PP_REFLECT_STRUCT(PlainProps::UE::Test, FDelta, void, A, B, C, D, E);

//////////////////////////////////////////////////////////////////////////
//
//struct FObject
//{
//	virtual ~FObject() {}
//	char Rtti = '\0';
//	int Id = 0;
//};
//
//struct FObjectReferenceBinding : ICustomBinding
//{
//	using Type = FObject*;
//
//	static constexpr EMemberPresence Occupancy = EMemberPresence::RequireAll;
//
//	static TConstArrayView<FMemberId> GetMemberIds()
//	{
//		static FMemberId Ids[] = {Ids::IndexMember("Type"), Ids::IndexMember("Id")};
//		return MakeArrayView(Ids, 2);
//	}
//
//	virtual void SaveStruct(FMemberBuilder& Dst, const void* Src, const void*, const FSaveContext&) const override
//	{
//		if (const FObject* Object = reinterpret_cast<const FObject*>(Src))
//		{
//			Dst.AddLeaf(GetMemberIds()[0], Object->Rtti);
//			Dst.AddLeaf(GetMemberIds()[1], Object->Id);
//		}
//	}
//
//	virtual void LoadStruct(void* Dst, FStructView Src, ECustomLoadMethod, const FLoadBatch&) const override
//	{
//		FMemberReader Members(Src);
//		check(Members.PeekName() == GetMemberIds()[0]);
//		char Rtti = static_cast<char>(Members.GrabLeaf().AsChar8());
//		check(Members.PeekName() == GetMemberIds()[1]);
//		int Id = Members.GrabLeaf().AsS32();
//		check(!Members.HasMore());
//
//	}
//}
//
//PP_REFLECT_STRUCT(PlainProps::UE::Test, FObject, void, Rtti, Sibling);
//
//struct FObjectX : public FObject
//{
//	FObjectX() { Rtti = 'x'; }
//	FObject* Sibling = nullptr;
//};
//PP_REFLECT_STRUCT(PlainProps::UE::Test, FObjectX, FObject, X);
//
//struct FObjectY : public FObject
//{
//	FObjectY() { Rtti = 'y'; }
//	FObject* Sibling = nullptr;
//};
//PP_REFLECT_STRUCT(PlainProps::UE::Test, FObjectY, FObject, X);
//
//struct FOwner
//{
//	TArray<TUniquePtr<FObject>> Objects;
//};
//PP_REFLECT_STRUCT(PlainProps::UE::Test, FOwner, void, Objects);

//////////////////////////////////////////////////////////////////////////

TEST_CASE_NAMED(FPlainPropsUeCoreTest, "System::Core::Serialization::PlainProps::UE::Core", "[Core][PlainProps][SmokeFilter]")
{
	SECTION("Basic")
	{
		TScopedStructBinding<FInt> Int;
		Run([](FBatchSaver& Batch)
			{
				Batch.Save(FInt{1234});
			}, 
			[](FBatchLoader& Batch)
			{
				CHECK(Batch.Load<FInt>().X == 1234);
			});
	}
	
	SECTION("Enum")
	{
		TScopedEnumDeclaration<EFlat1, EEnumMode::Flat> Flat1;
		TScopedEnumDeclaration<EFlat2, EEnumMode::Flat> Flat2;
		TScopedEnumDeclaration<EFlag1, EEnumMode::Flag> Flag1;
		TScopedEnumDeclaration<EFlag2, EEnumMode::Flag> Flag2;
		TScopedStructBinding<FEnums> Int;
		Run([](FBatchSaver& Batch)
			{
				Batch.Save(FEnums{EFlat1::A, EFlat2::A, EFlag1::A, EFlag2::A});
				Batch.Save(FEnums{EFlat1::A, EFlat2::A, EFlag1::B, EFlag2::B});
				Batch.Save(FEnums{EFlat1::B, EFlat2::B, EFlag1::A, EFlag2::A});
				Batch.Save(FEnums{EFlat1::B, EFlat2::B, EFlag1::B, EFlag2::B});
				Batch.Save(FEnums{EFlat1::B, EFlat2::B, EFlag1::AB, EFlag2::AB});
			}, 
			[](FBatchLoader& Batch)
			{
				CHECK(Batch.Load<FEnums>() == FEnums{EFlat1::A, EFlat2::A, EFlag1::A, EFlag2::A});
				CHECK(Batch.Load<FEnums>() == FEnums{EFlat1::A, EFlat2::A, EFlag1::B, EFlag2::B});
				CHECK(Batch.Load<FEnums>() == FEnums{EFlat1::B, EFlat2::B, EFlag1::A, EFlag2::A});
				CHECK(Batch.Load<FEnums>() == FEnums{EFlat1::B, EFlat2::B, EFlag1::B, EFlag2::B});
				CHECK(Batch.Load<FEnums>() == FEnums{EFlat1::B, EFlat2::B, EFlag1::AB, EFlag2::AB});
			});
	}

	SECTION("TArray")
	{
		TScopedStructBinding<FLeafArrays> LeafArrays;
		Run([](FBatchSaver& Batch)
			{
				Batch.Save(FLeafArrays{{}, {}});
				Batch.Save(FLeafArrays{{false}, {1, 2}});
				Batch.Save(FLeafArrays{{true, false}, {3, 4, 5}});
				Batch.Save(FLeafArrays{{true,true,true,true,true,true,true,true,false,true}});
			}, 
			[](FBatchLoader& Batch)
			{
				CHECK(Batch.Load<FLeafArrays>() == FLeafArrays{{}, {}});
				CHECK(Batch.Load<FLeafArrays>() == FLeafArrays{{false}, {1, 2}});
				CHECK(Batch.Load<FLeafArrays>() == FLeafArrays{{true, false}, {3, 4, 5}});
				CHECK(Batch.Load<FLeafArrays>() == FLeafArrays{{true,true,true,true,true,true,true,true,false,true}});
			});
	}

	SECTION("Nesting")
	{
		TScopedEnumDeclaration<EFlat1, EEnumMode::Flat> Flat1;
		TScopedStructBinding<FLeafArrays> LeafArrays;
		TScopedStructBinding<FComplexArrays> ComplexArrays;
		Run([](FBatchSaver& Batch)
			{
				Batch.Save(FComplexArrays{});
				Batch.Save(FComplexArrays{{'a', 'b'}, {EFlat1::A}, {{}, {{true}, {2}}}, {{EFlat1::B}, {}} });
			}, 
			[](FBatchLoader& Batch)
			{
				CHECK(Batch.Load<FComplexArrays>() == FComplexArrays{});
				CHECK(Batch.Load<FComplexArrays>() == FComplexArrays{{'a', 'b'}, {EFlat1::A}, {{}, {{true}, {2}}}, {{EFlat1::B}, {}} });
			});
	}

	SECTION("TUniquePtr")
	{
		TScopedStructBinding<FInt> Int;
		TScopedStructBinding<FUniquePtrs> UniquePtrs;
		Run([](FBatchSaver& Batch)
			{
				Batch.Save(FUniquePtrs{});
				Batch.Save(FUniquePtrs{MakeOne(true), MakeOne(FInt{3}), MakeOne(MakeOne(2)), MakeTwo(1.0, 2.0)});
			}, 
			[](FBatchLoader& Batch)
			{
				CHECK(Batch.Load<FUniquePtrs>() == FUniquePtrs{});
				CHECK(Batch.Load<FUniquePtrs>() == FUniquePtrs{MakeOne(true), MakeOne(FInt{3}), MakeOne(MakeOne(2)), MakeTwo(1.0, 2.0)});
			});
	}

	SECTION("TOptional")
	{
		TScopedStructBinding<FNDC> NDC;
		TScopedStructBinding<FNDCIntrusive> NDCI;
		TScopedStructBinding<FOpts> Opts;
		Run([](FBatchSaver& Batch)
			{
				Batch.Save(FOpts{});
				Batch.Save(FOpts{{true}, {FNDC{2}}, {FNDCIntrusive{3}}});
				Batch.Save(FOpts{{true}, {FNDC{2}}, {FNDCIntrusive{3}}});
			}, 
			[](FBatchLoader& Batch)
			{
				CHECK(Batch.Load<FOpts>() == FOpts{});
				CHECK(Batch.Load<FOpts>() == FOpts{{true}, {FNDC{2}}, {FNDCIntrusive{3}}});
				FOpts AlreadySet = {{false}, {FNDC{0}}, {FNDCIntrusive{1}}};
				Batch.LoadInto(AlreadySet);
				CHECK(AlreadySet == FOpts{{true}, {FNDC{2}}, {FNDCIntrusive{3}}});
			});
	}

	SECTION("FName")
	{
		TScopedStructBinding<FNames> Names;
		Run([](FBatchSaver& Batch)
			{
				Batch.Save(FNames{ FName("A"), {FName("Y"), FName("A")} });
			}, 
			[](FBatchLoader& Batch)
			{
				CHECK(Batch.Load<FNames>() == FNames{ FName("A"), {FName("Y"), FName("A")}});
			});
	}

	SECTION("FString")
	{
		TScopedStructBinding<FStr> Str;
		Run([](FBatchSaver& Batch)
			{
				Batch.Save(FStr{});
				Batch.Save(FStr{"ABC"});
				if constexpr (sizeof(TCHAR) > 1)
				{
					Batch.Save(FStr{TEXT("\x7FF")});
					Batch.Save(FStr{TEXT("\x3300")});
					Batch.Save(FStr{TEXT("\xFE30")});
					Batch.Save(FStr{TEXT("\xD83D\xDC69")});
				}
			}, 
			[](FBatchLoader& Batch)
			{
				CHECK(Batch.Load<FStr>().S.IsEmpty());
				CHECK(Batch.Load<FStr>().S == "ABC");
				if constexpr (sizeof(TCHAR) > 1)
				{
					CHECK(Batch.Load<FStr>().S == TEXT("\x7FF"));
					CHECK(Batch.Load<FStr>().S == TEXT("\x3300"));
					CHECK(Batch.Load<FStr>().S == TEXT("\xFE30"));
					CHECK(Batch.Load<FStr>().S == TEXT("\xD83D\xDC69"));
				}
			});	
	}
		
	SECTION("TSet")
	{
		TScopedStructBinding<FInt> Int;
		TScopedStructBinding<FSets> Sets;
		Run([](FBatchSaver& Batch)
			{
				Batch.Save(FSets{{'H','i'}, {{uint8(10)}, {}}, {{123}}});
				
				// Test order preservation
				Batch.Save(FSets{{'a','b'}});
				Batch.Save(FSets{{'b','a'}});

				// Test non-compact set
				FSets Sparse = FSets{{'w','z','a','p','?','!'}};
				Sparse.Leaves.Remove('w');
				Sparse.Leaves.Remove('p');
				Sparse.Leaves.Remove('!');
				Batch.Save(Sparse);
			}, 
			[](FBatchLoader& Batch)
			{
				CHECK(Batch.Load<FSets>() == FSets{{'H','i'}, {{uint8(10)}, {}}, {{123}}});
				CHECK(FSets{{'a','b'}} != FSets{{'b','a'}});
				CHECK(Batch.Load<FSets>() == FSets{{'a','b'}});
				CHECK(Batch.Load<FSets>() == FSets{{'b','a'}});
				CHECK(Batch.Load<FSets>() == FSets{{'z','a','?'}});
			});
	}

	SECTION("TMap")
	{
		TScopedStructBinding<FInt> Int;
		TScopedStructBinding<FNDC> NDC;
		TScopedStructBinding<FMaps> Maps;
		TScopedStructBinding<TPair<bool, bool>> BoolBoolPair;
		TScopedStructBinding<TPair<int, TArray<char>>> IntStringPair;
		TScopedStructBinding<TPair<FInt, FNDC>> IntNDCPair;
		
		Run([](FBatchSaver& Batch)
			{
				Batch.Save(FMaps{});
				Batch.Save(FMaps{{{true, true}, {false, false}}, {{5, {'h', 'i'}}}, {{{7}, FNDC{8}}}});
			}, 
			[](FBatchLoader& Batch)
			{
				CHECK(Batch.Load<FMaps>() == FMaps{});
				CHECK(Batch.Load<FMaps>() == FMaps{{{true, true}, {false, false}}, {{5, {'h', 'i'}}}, {{{7}, FNDC{8}}}});
			});
	}

	SECTION("Delta")
	{
		TScopedStructBinding<FInt> Int;
		TScopedStructBinding<FDelta> Delta;
		Run([](FBatchSaver& Batch)
			{
				FDelta Zero = {false, 0, {}, {}, {}};
				CHECK(!Batch.SaveDelta(FInt{123},FInt{123}));
				CHECK(!Batch.SaveDelta(FDelta{},FDelta{}));
				CHECK(!Batch.SaveDelta(Zero, Zero));
					
				Batch.SaveDelta({}, Zero);
				Batch.SaveDelta(Zero, {});
				Batch.SaveDelta(FDelta{.B = 123}, {});
				Batch.SaveDelta(FDelta{.C = {321}}, {});
				Batch.SaveDelta(FDelta{.D = {0}}, {});
				Batch.SaveDelta(FDelta{.E = "!!"}, {});
			}, 
			[](FBatchLoader& Batch)
			{
				FDelta Zero = {false, 0, {}, {}, {}};
				FDelta DefaultOnZero = Zero;
				Batch.LoadInto(DefaultOnZero);
				CHECK(DefaultOnZero == FDelta{});
				CHECK(Batch.Load<FDelta>() == Zero);
				CHECK(Batch.Load<FDelta>() == FDelta{.B = 123});
				CHECK(Batch.Load<FDelta>() == FDelta{.C = {321}});
				CHECK(Batch.Load<FDelta>() == FDelta{.D = {0}});
				CHECK(Batch.Load<FDelta>() == FDelta{.E = "!!"});
			});
	}

	SECTION("TSetDelta")
	{
		TScopedStructBinding<FInt> Int;
		//TScopedStructBinding<FSets, EMemberPresence::AllowSparse, FDeltaRuntime> Sets;
	}

	SECTION("Transform")
	{
		TScopedStructBinding<FVector, EMemberPresence::RequireAll> Vector;
		TScopedStructBinding<FQuat, EMemberPresence::RequireAll> Quat;
		BindCustomStructOnce<FTransformBinding, FDefaultRuntime>();

		Run([](FBatchSaver& Batch)
			{
				CHECK(!Batch.SaveDelta(FTransform(),FTransform()));
				CHECK(!Batch.SaveDelta(FTransform(FVector::UnitY()), FTransform(FVector::UnitY())));

				Batch.Save(FTransform());

				// This should only save translation
				Batch.SaveDelta(FTransform(FVector::UnitY()), FTransform());
			}, 
			[](FBatchLoader& Batch)
			{
				CHECK(Batch.Load<FTransform>().Equals(FTransform(), 0.0));

				FTransform TranslateY(				FQuat(1, 2, 3, 4), FVector(5, 5, 5), FVector(6, 7, 8));
				Batch.LoadInto(TranslateY);
				CHECK(TranslateY.Equals(FTransform(	FQuat(1, 2, 3, 4), FVector::UnitY(), FVector(6, 7, 8)), 0.0));
			});
	}

	SECTION("Reference")
	{}
}

} // namespace PlainProps::UE::Test

#endif // WITH_TESTS