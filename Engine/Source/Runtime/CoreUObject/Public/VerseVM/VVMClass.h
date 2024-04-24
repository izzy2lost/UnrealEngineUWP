// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Templates/SharedPointer.h"
#include "VerseVM/VVMArray.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMProcedure.h"
#include "VerseVM/VVMPropertyType.h"
#include "VerseVM/VVMShape.h"
#include "VerseVM/VVMType.h"

class UObject;
class UVerseVMClass;
class FVerseVMEngineEnvironment;

namespace Verse
{
struct FAbstractVisitor;
struct VObject;
struct VProcedure;
struct VPackage;
struct VUniqueString;

/// This provides a custom comparison that allows us to do pointer-based compares of each unique string set, rather than hash-based comparisons.
struct FEmergentTypesCacheKeyFuncs : TDefaultMapKeyFuncs<TWriteBarrier<VUniqueStringSet>, TWriteBarrier<VEmergentType>, /*bInAllowDuplicateKeys*/ false>
{
public:
	static bool Matches(KeyInitType A, KeyInitType B);
	static bool Matches(KeyInitType A, const VUniqueStringSet& B);
	static uint32 GetKeyHash(KeyInitType Key);
	static uint32 GetKeyHash(const VUniqueStringSet& Key);
};

/// A sequence of fields and blocks in a class body.
/// May represent either a single class, or the flattened combination of a subclass and its superclasses.
struct VConstructor : VCell
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VCell);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	struct VEntry
	{
		/// When non-null, the name of this field. When null, this entry represents a block.
		TWriteBarrier<VUniqueString> Name;

		/// When non-null, the verse compiler has provided type information about the entry
		TWriteBarrier<VPropertyType> PropertyType;

		/// When bDynamic, a VProcedure for a default initializer or block, or nothing for an uninitialized field.
		/// Otherwise, a constant VValue for a default field value (which may be a VProcedure for functions, which bind Self lazily).
		TWriteBarrier<VValue> Value;
		bool bDynamic;

		static VEntry Constant(FAllocationContext Context, FUtf8StringView InField, VValue InValue, VPropertyType* InPropertyType = nullptr)
		{
			return Constant(Context, VUniqueString::New(Context, InField), InValue, InPropertyType);
		}

		static VEntry Constant(FAllocationContext Context, VUniqueString& InField, VValue InValue, VPropertyType* InPropertyType = nullptr)
		{
			return VEntry{
				{Context,        InField},
				{Context, InPropertyType},
				{Context,        InValue},
				false
            };
		}

		static VEntry Field(FAllocationContext Context, FUtf8StringView InField, VPropertyType* InPropertyType = nullptr)
		{
			return Field(Context, VUniqueString::New(Context, InField), InPropertyType);
		}

		static VEntry Field(FAllocationContext Context, VUniqueString& InField, VPropertyType* InPropertyType = nullptr)
		{
			return {
				{Context, InField},
				{Context, InPropertyType},
				{},
				true
            };
		}

		static VEntry FieldInitializer(FAllocationContext Context, FUtf8StringView InField, VProcedure& InCode, VPropertyType* InPropertyType = nullptr)
		{
			return FieldInitializer(Context, VUniqueString::New(Context, InField), InCode, InPropertyType);
		}

		static VEntry FieldInitializer(FAllocationContext Context, VUniqueString& InField, VProcedure& InCode, VPropertyType* InPropertyType = nullptr)
		{
			return {
				{Context,        InField},
				{Context, InPropertyType},
				{Context, VValue(InCode)},
				true
            };
		}

		static VEntry Block(FAllocationContext Context, VProcedure& Code)
		{
			return {
				{},
				{},
				{Context, VValue(Code)},
				true
            };
		}

		VProcedure* Initializer() const
		{
			if (bDynamic && Value.Get())
			{
				return &Value.Get().StaticCast<VProcedure>();
			}
			else
			{
				return nullptr;
			}
		}
	};

	const uint32 NumEntries;
	VEntry Entries[];

	static VConstructor& New(FAllocationContext Context, const TArray<VEntry>& InEntries)
	{
		size_t NumBytes = offsetof(VConstructor, Entries) + InEntries.Num() * sizeof(Entries[0]);
		return *new (Context.AllocateFastCell(NumBytes)) VConstructor(Context, InEntries);
	}

	COREUOBJECT_API void ToStringImpl(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter);

	static void SerializeImpl(VConstructor*& This, FAllocationContext Context, FAbstractVisitor& Visitor);

private:
	static VConstructor& NewUninitialized(FAllocationContext Context, uint32 InNumEntries)
	{
		size_t NumBytes = offsetof(VConstructor, Entries) + InNumEntries * sizeof(Entries[0]);
		return *new (Context.AllocateFastCell(NumBytes)) VConstructor(Context, InNumEntries);
	}

	VConstructor(FAllocationContext Context, const TArray<VEntry>& InEntries)
		: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
		, NumEntries(InEntries.Num())
	{
		for (uint32 Index = 0; Index < NumEntries; ++Index)
		{
			new (&Entries[Index]) VEntry(InEntries[Index]);
		}
	}

	VConstructor(FAllocationContext Context, uint32 InNumEntries)
		: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
		, NumEntries(InNumEntries)
	{
	}
};

struct VClass : VType
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VType);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	enum class EKind : uint8
	{
		Class,
		Struct,
		Interface
	};

	FUtf8StringView GetName() const { return ClassName.Get() != nullptr ? ClassName->AsStringView() : FUtf8StringView(); }
	FUtf8StringView GetUEMangledName() const { return UEMangledName.Get() != nullptr ? UEMangledName->AsStringView() : FUtf8StringView(); }
	COREUOBJECT_API FUtf8StringView ExtractClassName() const;
	VPackage* GetScope() const { return Scope.Get(); }
	EKind GetKind() const { return Kind; }
	bool IsStruct() const { return GetKind() == EKind::Struct; }
	bool IsNative() const { return bNative; }

	/// Allocate a new VObject. Also returns a sequence of VProcedures to invoke to finish the object's construction.
	/// `ArchetypeValues` should match the order of IDs in `ArchetypeFields`.
	COREUOBJECT_API VObject& NewVObject(FAllocationContext Context, VUniqueStringSet& ArchetypeFields, const TArray<VValue>& ArchetypeValues, TArray<VProcedure*>& OutInitializers);

	/// Allocate a new UObject. Also returns a sequence of VProcedures to invoke to finish the object's construction.
	/// `ArchetypeValues` should match the order of IDs in `ArchetypeFields`.
	COREUOBJECT_API UObject* NewUObject(FAllocationContext Context, VUniqueStringSet& ArchetypeFields, const TArray<VValue>& ArchetypeValues, TArray<VProcedure*>& OutInitializers);

private:
	// Helper to find initializer procedures after archetype fields have been set on an object
	void GatherInitializers(VUniqueStringSet& ArchetypeFields, TArray<VProcedure*>& OutInitializers);

public:
	/// Vends an emergent type based on requested fields to override in the class archetype instantiation.
	COREUOBJECT_API VEmergentType& GetOrCreateEmergentTypeForArchetype(FAllocationContext Context, VUniqueStringSet& ArchetypeFieldNames, VCppClassInfo* CppClassInfo);

	COREUOBJECT_API UClass* GetOrCreateUClass(FAllocationContext Context);

	/// Creates an associated UClass for this VClass
	COREUOBJECT_API UClass* CreateUClass(FAllocationContext Context);

	/**
	 * Creates a new class.
	 *
	 * @param Scope         Containing package or null.
	 * @param Name          Name or null.
	 * @param UEMangledName Name to be used when creating the UE version of the class or the UE package.  Can be null.
	 * @param Kind          Class, Struct or Interface.
	 * @param Inherited     An array of base classes in order of inheritance.
	 * @param Constructor   The sequence of fields and blocks in the class body.
	 */
	COREUOBJECT_API static VClass& New(FAllocationContext Context, VPackage* Scope, VArray* Name, VArray* UEMangledName, EKind Kind, bool bNative, const TArray<VClass*>& Inherited, VConstructor& Constructor, UClass* ImportClass);

private:
	VClass(FAllocationContext Context, VPackage* InScope, VArray* InName, VArray* InUEMangledName, EKind InKind, bool bInNative, const TArray<VClass*>& InInherited, VConstructor& InConstructor, UClass* InImportClass);

	/// Append to `Entries` those elements of `Base` which are not already overridden, indicated by `Fields`.
	COREUOBJECT_API static void Extend(TSet<VUniqueString*>& Fields, TArray<VConstructor::VEntry>& Entries, const VConstructor& Base);

	bool SubsumesImpl(FRunningContext, VValue);

	TWriteBarrier<VArray> ClassName;
	TWriteBarrier<VArray> UEMangledName;

	/// The package this class is in
	TWriteBarrier<VPackage> Scope;

	// TODO: (yiliang.siew) This should be a weak map when we can support it in the GC. https://jira.it.epicgames.com/browse/SOL-5312
	/// This is a cache that allows for fast vending of emergent types based on the fields being overridden.
	TMap<TWriteBarrier<VUniqueStringSet>, TWriteBarrier<VEmergentType>, FDefaultSetAllocator, FEmergentTypesCacheKeyFuncs> EmergentTypesCache;

	/// The combined sequence of initializers and blocks in this class and its superclasses, in execution order.
	/// Actual object construction may further override some elements of this sequence.
	TWriteBarrier<VConstructor> Constructor;

	/// An associated UClass allows this VClass to create UObject instances
	TWriteBarrier<VValue> AssociatedUClass;

	// Stored here to share alignment space with NumInherited
	EKind Kind;
	bool bNative;

	// Super classes and interfaces. The single superclass is always first.
	uint32 NumInherited;
	TWriteBarrier<VClass> Inherited[];

	friend class ::FVerseVMEngineEnvironment;
};
};     // namespace Verse
#endif // WITH_VERSE_VM
