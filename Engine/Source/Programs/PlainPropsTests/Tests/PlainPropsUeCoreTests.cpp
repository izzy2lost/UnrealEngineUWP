// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_TESTS

#include  "PlainPropsBuildSchema.h"
#include  "PlainPropsCtti.h"
#include  "PlainPropsIndex.h"
#include  "PlainPropsInternalBuild.h"
#include  "PlainPropsInternalFormat.h"
#include  "PlainPropsLoad.h"
#include  "PlainPropsRead.h"
#include  "PlainPropsSave.h"
#include  "PlainPropsWrite.h"
#include  "PlainPropsUeCoreBindings.h"
#include "Tests/TestHarnessAdapter.h"
#include "Containers/StringView.h"
#include "Containers/Map.h"
#include "Templates/UnrealTemplate.h"

namespace PlainProps::UE::Test
{

static TIdIndexer<FName>	GNames;
static FDeclarations		GTypes;
static FStructBindings		GBindings;
static FStructBindings		GDeltaBindings;

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
	static const FDebugIds& GetDebug()									{ return GNames; }
};

////////////////////////////////////////////////////////////////////////////////////////////////

template<class T>
struct TCustomBindings
{
	using Type = void;
};

template<class T>
struct TCustomDeltaBindings : TCustomBindings<T>
{};

template<typename T>
struct TCustomDeltaBindings<TSet<T>>
{
	using Type = PlainProps::UE::TSetDeltaBinding<FIds, T>;
};

////////////////////////////////////////////////////////////////////////////////////////////////

struct FDefaultRuntime
{
	using Ids = FIds;
	template<class T> using CustomBindings = TCustomBindings<T>;

	static FDeclarations&			GetDeclarations()				{ return GTypes; }
	static FStructBindings&			GetBindings()					{ return GBindings; }
};

struct FDeltaRuntime : FDefaultRuntime
{
	template<class T> using CustomBindings = TCustomDeltaBindings<T>;

	static FStructBindings&			GetBindings()					{ return GDeltaBindings; }
};

////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, class Runtime>
struct TScopedStructBinding
{
	using Ids = typename Runtime::Ids;
	using Ctti = CttiOf<T>;
	//constexpr bool bDeltaBindMembers = NumCustomMembers<Ctti, FDeltaRuntime::CustomBindings>() > NumCustomMembers<Ctti, FDefaultRuntime::CustomBindings>();

	TScopedStructBinding(EMemberPresence Occupancy = EMemberPresence::AllowSparse)
	: Id(DeclareNativeStruct<Ctti, Ids>(Runtime::GetDeclarations(), Occupancy))
	{
		BindNativeStruct<Ctti, Runtime>(Runtime::GetBindings(), Id);
		//if constexpr (bDeltaBindMembers)
		//{
		//	BindNativeStruct<Ctti, FDeltaRuntime>(Runtime::GetBindings());	
		//}
	}

	~TScopedStructBinding()
	{
		Runtime::GetBindings().DropStruct(Id);
		Runtime::GetDeclarations().DropStruct(Id);
	}

	FStructSchemaId Id;
};

template<typename Enum, EEnumMode Mode, class Runtime>
struct TScopedEnumBinding
{
	using Ids = typename Runtime::Ids;
	using Ctti = CttiOf<Enum>;

	FEnumSchemaId Id;
	TScopedEnumBinding() : Id(DeclareNativeEnum<Ctti, Ids>(Runtime::GetDeclarations(), Mode)) {}
	~TScopedEnumBinding() { Runtime::GetDeclarations().DropEnum(Id); }
};

//template<typename T, class Runtime>
//struct TScopedBinding : std::conditional_t<std::is_enum_v<T>, TScopedEnumBinding<T, Runtime>, TScopedStructBinding<T, Runtime>>
//{
//	
//};

//template<class T>
//struct TScopedStructDeclaration
//{
//	FStructSchemaId Id;
//
//	TScopedStructDeclaration(EMemberPresence Occupancy)
//	: Id(DeclareNativeStruct<CttiOf<T>>(FDefaultRuntime::GetDeclarations()), Occupancy)
//	{}
//
//	~TScopedStructDeclaration()
//	{
//		FDeltaRuntime::DropStruct(Id);
//	}
//};
////////////////////////////////////////////////////////////////////////////////////////////////

inline constexpr uint32 Magics[] = { 0xFEEDF00D, 0xABCD1234, 0xDADADAAA, 0x99887766, 0xF0F1F2F3 };

class FBatchSaver
{
public:
	explicit FBatchSaver(const FStructBindings& InBindings) : Bindings(InBindings) {}

	template<class T>
	void						Save(T&& Object) { Save(IndexNativeStruct<T, FIds>(), &Object); }
	TArray64<uint8>				Write() const;

private:
	void						Save(FStructSchemaId Id, const void* Object); 

	using IdBuiltStructPair = TPair<FStructSchemaId, TUniquePtr<FBuiltStruct>>;
	TArray<IdBuiltStructPair>	SavedObjects;
	const FStructBindings&		Bindings;
};

void FBatchSaver::Save(FStructSchemaId Id, const void* Object)
{
	SavedObjects.Emplace(Id, SaveStruct(reinterpret_cast<const uint8*>(Object), Id, {GTypes, GBindings, /* debug */ GNames}));
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

TArray64<uint8> FBatchSaver::Write() const
{
	// Build partial schemas
	FSchemasBuilder SchemaBuilders(GTypes.GetStructs(), GTypes.GetEnums(), /* debug */ GNames);
	for (const IdBuiltStructPair& Object : SavedObjects)
	{
		SchemaBuilders.NoteMembers(Object.Key, *Object.Value);
	}
	FBuiltSchemas Schemas = SchemaBuilders.Build(); 

	// Filter out declared but unused names and ids
	FWriter Writer(GNames, Schemas);
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
	for (const TPair<FStructSchemaId, TUniquePtr<FBuiltStruct>>& Object : SavedObjects)
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
		
	return Out;
}

////////////////////////////////////////////////////////////////////////////////////////////////

class FBatchLoader
{
public:
	FBatchLoader(const FStructBindings& Bindings, FMemoryView Data)
	{
		// Read ids
		FByteReader It(Data);
		CHECK(It.Grab<uint32>() == Magics[0]);
		SavedNames = GrabNumAndArray<FName>(It);
		CHECK(SavedNames.Num() != 0);
		
		// Read schemas
		CHECK(It.Grab<uint32>() == Magics[1]);
		It.SkipAlignmentPadding<uint32>();
		uint32 SchemasSize = It.Grab<uint32>();
		const FSchemaBatch* SavedSchemas = ValidateSchemas(It.GrabSlice(SchemasSize));
		CHECK(It.Grab<uint32>() == Magics[2]);
		
		// Bind saved ids to runtime ids, make new schemas with new ids and mount them
		FIdTranslator RuntimeIds(GNames, SavedNames, *SavedSchemas);
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

		// Finally create load plans
		TConstArrayView<FStructSchemaId> LoadStructIds = RuntimeIds.Translation.GetStructIds(SavedSchemas->NumStructSchemas);
		Plans = CreateLoadPlans(Batch, GTypes, Bindings, LoadStructIds);
	}

	~FBatchLoader()
	{
		CHECK(LoadIdx == Objects.Num()); // Test should load all saved objects
		DestroyLoadPlans(Plans);
		const FSchemaBatch* LoadSchemas = UnmountReadSchemas(Objects[0].Schema.Batch);
		DestroyTranslatedSchemas(LoadSchemas);
	}

	template<class T>
	T Load()
	{
		T Out;
		FStructView In = Objects[LoadIdx++];
		LoadStruct(reinterpret_cast<uint8*>(&Out), In, *Plans);
		return MoveTemp(Out);
	}

private:
	TConstArrayView<FName> SavedNames;
	FLoadBatch* Plans;
	TArray<FStructView> Objects;
	int32 LoadIdx = 0;
};


static void SaveAndLoad(const FStructBindings& Bindings, void (*Save)(FBatchSaver&), void (*Load)(FBatchLoader&))
{
	TArray64<uint8> Data;
	{
		FBatchSaver Batch(Bindings);
		Save(Batch);
		Data = Batch.Write();
	}

	FBatchLoader Batch(Bindings, MakeMemoryView(Data));
	Load(Batch);
}

template<class Runtime>
static void TestSaveAndLoad(void (*Save)(FBatchSaver&), void (*Load)(FBatchLoader&))
{
	SaveAndLoad(Runtime::GetBindings(), Save, Load);
}



//// Tests that everything was visited
//struct FTestMemberVisitor : public FMemberVisitor
//{
//	using FMemberVisitor::FMemberVisitor;
//	
//	~FMemberVisitor()
//	{
//		CHECK(MemberIdx == NumMembers); // Must read all members
//		CHECK(RangeTypeIdx == NumRangeTypes); // Must read all ranges
//#if DO_CHECK
//		CHECK(InnerSchemaIdx == NumInnerSchemas); // Must read all schema ids
//#endif
//	}
//};
//
//template<typename OutType, typename InType>
//TArray<OutType> MakeArray(const InType& Items)
//{
//	TArray<OutType> Out;
//	Out.Reserve(IntCastChecked<int32>(Items.Num()));
//	for (const auto& Item : Items)
//	{
//		Out.Emplace(Item);
//	}
//	return Out;
//}
//
//
//
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

//////////////////////////////////////////////////////////////////////////

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

	bool operator==(FEnums O) const { return Flat1 == O.Flat1 && Flat2 == O.Flat2 && Flag1 == O.Flag1 && Flag2 == O.Flag2; }
};
PP_REFLECT_STRUCT(PlainProps::UE::Test, FEnums, void, Flat1, Flat2, Flag1, Flag2);

//////////////////////////////////////////////////////////////////////////

//struct FLeaves
//{
//	uint16 U16 = 0;
//	int64 I64 = 0;
//	bool Bool = false;
//	char AsciiChar = 0;
////	uint8 BitfieldBool : 1;
//	uint32 StaticU32s[3] = {};
//	double Double = 0.0;
//};
//PP_REFLECT_STRUCT(PlainProps::UE::Test, FLeaves, void, U16, I64, Bool, AsciiChar, StaticU32s, Double);

//////////////////////////////////////////////////////////////////////////

struct FLeafArrays
{
	TArray<bool> Bits;
	TArray<int>	Bobs;

	bool operator==(const FLeafArrays& O) const { return Bits == O.Bits && Bobs == O.Bobs; }
};
PP_REFLECT_STRUCT(PlainProps::UE::Test, FLeafArrays, void, Bits, Bobs);

struct FComplexArrays
{
	TArray<char> Str;
	TArray<EFlat1> Enums;
	TArray<FLeafArrays> Misc;
	TArray<TArray<EFlat1>> Nested;

	bool operator==(const FComplexArrays& O) const { return Str == O.Str && Enums == O.Enums && Misc == O.Misc && Nested == O.Nested; }
};
PP_REFLECT_STRUCT(PlainProps::UE::Test, FComplexArrays, void, Str, Enums, Misc, Nested);

//////////////////////////////////////////////////////////////////////////

TEST_CASE_NAMED(FPlainPropsUeCoreTest, "System::Core::Serialization::PlainProps::UE::Core", "[Core][PlainProps][SmokeFilter]")
{
	SECTION("Basic")
	{
		TScopedStructBinding<FInt, FDefaultRuntime> Int;
		TestSaveAndLoad<FDefaultRuntime>(
			[](FBatchSaver& Batch)
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
		TScopedEnumBinding<EFlat1, EEnumMode::Flat, FDefaultRuntime> Flat1;
		TScopedEnumBinding<EFlat2, EEnumMode::Flat, FDefaultRuntime> Flat2;
		TScopedEnumBinding<EFlag1, EEnumMode::Flag, FDefaultRuntime> Flag1;
		TScopedEnumBinding<EFlag2, EEnumMode::Flag, FDefaultRuntime> Flag2;
		TScopedStructBinding<FEnums, FDefaultRuntime> Int;
		TestSaveAndLoad<FDefaultRuntime>(
			[](FBatchSaver& Batch)
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

	SECTION("TArrayBasic")
	{
		TScopedStructBinding<FLeafArrays, FDefaultRuntime> LeafArrays;
		TestSaveAndLoad<FDefaultRuntime>(
			[](FBatchSaver& Batch)
			{
				Batch.Save(FLeafArrays{{}, {}});
				Batch.Save(FLeafArrays{{false}, {1, 2}});
				Batch.Save(FLeafArrays{{true, false}, {3, 4, 5}});
			}, 
			[](FBatchLoader& Batch)
			{
				CHECK(Batch.Load<FLeafArrays>() == FLeafArrays{{}, {}});
				CHECK(Batch.Load<FLeafArrays>() == FLeafArrays{{false}, {1, 2}});
				CHECK(Batch.Load<FLeafArrays>() == FLeafArrays{{true, false}, {3, 4, 5}});
			});
	}

	SECTION("TArrayComplex")
	{}

	SECTION("FString")
	{}

	SECTION("TUniquePtr")
	{}
		
	SECTION("TSet")
	{}
	
	SECTION("FName")
	{}

	SECTION("FakeReference")
	{}

	SECTION("NestedContainer")
	{}
	
	SECTION("TSetDelta")
	{}

	//SECTION("LeafOptional")
	//{}
	//
	//SECTION("LeafSmartPtr")
	//{}

	//SECTION("LeafSetWhole")
	//{}

	//SECTION("LeafSparseArrayAppends")
	//{}

	//SECTION("LeafSetOps")
	//{}
	//
	//SECTION("SparseStructArray")
	//{}

	//SECTION("DenseStructArray")
	//{}
	//
	//SECTION("SubStructArray")
	//{}

	//SECTION("NestedLeafArray")
	//{}

	//SECTION("NestedStructArray")
	//{}

	//SECTION("StructToSubStructMapOps")
	//{}
}

} // namespace PlainProps::UE::Test
#endif // WITH_TESTS