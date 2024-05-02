// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_TESTS

#include "Tests/TestHarnessAdapter.h"
#include "Containers/StringView.h"
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
#include "Templates/UnrealTemplate.h"

namespace PlainProps::UE::Test
{

static TIdIndexer<FName>	GNames;
static FDeclarations		GTypes;
static FStructBindings		GBindings;
static FStructBindings		GDeltaBindings;

struct FIds
{
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
	static void						DropStruct(FStructSchemaId Id)	{} // todo
};

struct FDeltaRuntime : FDefaultRuntime
{
	template<class T> using CustomBindings = TCustomDeltaBindings<T>;

	static FStructBindings&			GetBindings()					{ return GDeltaBindings; }
	static void						DropStruct(FStructSchemaId Id)	{} // todo,  drop from default too
};

////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, class Runtime>
struct TScopedStructBinding
{
	using Ids = typename Runtime::Ids;
	using Ctti = CttiOf<T>;
	//constexpr bool bDeltaBindMembers = NumCustomMembers<Ctti, FDeltaRuntime::CustomBindings>() > NumCustomMembers<Ctti, FDefaultRuntime::CustomBindings>();

	FStructSchemaId Id;

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
		Runtime::DropStruct(Id);
	}
};

template<typename T, class Runtime>
struct TScopedEnumBinding
{
	// todo
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
		FIdTranslator LoadIds(GNames, SavedNames, *SavedSchemas);
		FSchemaBatch* LoadSchemas = CreateTranslatedSchemas(*SavedSchemas, LoadIds.Translation);
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
		TConstArrayView<FStructSchemaId> LoadStructIds = LoadIds.Translation.GetStructIds(SavedSchemas->NumStructSchemas);
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
//enum EFlat { A = 1, B = 3 };
//enum EFlag { A = 0, B = 2 };
//
//UE_REFLECT_ENUM(EFlat, A, B);
//UE_REFLECT_ENUM(EFlag, A, B);
//
//struct FEnums
//{
//	EFlat Flat;
//	EFlag Flag;
//};
//
//UE_REFLECT_STRUCT(FEnums, Flat, Flag);
//
//struct Leaves
//{
//	uint16 U16;
//	int64 I64;
//	bool Bool;
//	char AsciiChar;
//	uint8 BitfieldBool : 1;
//	uint32 StaticU32s[3];
//};
//
//UE_REFLECT_STRUCT(Leaves, U16, I64, Bool, AsciiChar, BitfieldBool, StaticU32s);
//
//GTestBindings.Add(LeavesCtti);
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

	SECTION("TUniquePtr")
	{}

	SECTION("TArray")
	{}

	SECTION("FString")
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