// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "Containers/Map.h"
#include "VVMCell.h"
#include "VVMUTF8String.h"
#include "VVMWriteBarrier.h"

namespace Verse
{
struct FAccessContext;
struct VUniqueString;

template <Verse::VCppClassInfo* ClassInfo>
struct TGlobalTrivialEmergentTypePtr;

#define VERSE_ENUM_FIELDTYPES(Decl)                                                                                             \
	/*                                                                                                                          \
	 * Whether or not the field's data is stored in the object.                                                                 \
	 * i.e. `c := class { X:int = 0 }` versus `c := class { X:int }`.                                                           \
	 */                                                                                                                         \
	Decl(Offset)      /* Same as `Offset`, but also indicates that this is a mutable field. i.e. `c := class { var X:int }` */  \
		Decl(Mutable) /*                                                                                                        \
					   * Shapes have the ability to store constants in them.                                                    \
					   * This will be the case in instances such as when we have fields that                                    \
					   * point to some sort of global entry, or if the field is initialized with a constant value.              \
					   * Other examples include fields that refer to functions/lambdas.                                         \
					   *                                                                                                        \
					   * Example: `a := class{ F:int = 0, G:int = 0}; A:a = a{F:= 100}` where `F` is a constant value of `100`. \
					   *                                                                                                        \
					   * For `A.F`, that would just point to the constant value directly.                                       \
					   *                                                                                                        \
					   * Another example: `a := class{ B(X:int, Y:int):int = X + Y }; A:a = a{}` where `B` is a constant proc.  \
					   * `A.B` just points to the function directly.                                                            \
					   */                                                                                                       \
		Decl(Constant)

enum class EFieldType : int8
{
#define VERSE_VISIT_FIELDTYPE(Name) Name,
	VERSE_ENUM_FIELDTYPES(VERSE_VISIT_FIELDTYPE)
#undef VERSE_VISIT_FIELDTYPE
};

/// Represents a series of entries for fields that are used to instantiate classes in the VM.
struct VFields : VCell
{
	/// Represents a field entry on a given shape.
	struct VEntry
	{
		/// The zero-based offset for the given entry that can be used to index into the object.
		uint64 Index;

		/// This can either be a constant value for the given entry, or, in conjunction with the index, be
		/// the "default value" for the offset-based entry.
		TWriteBarrier<VValue> Constant;
		EFieldType Type;

		~VEntry() = default;

		// Must have a copy/move constructor in order to be used with `TMap` as the value type.
		VEntry(const VEntry& Other);
		VEntry(VEntry&& Other);
		VEntry(const uint64 InIndex, const bool bIsMutable);
		VEntry(FAccessContext Context, VValue InConstant, const EFieldType FieldType);
		VEntry(FAccessContext Context, VValue InConstant);

		inline bool operator==(const VEntry& Other) const;
	};

	/// We're providing this in order to be able to lookup into the fields map without having to
	/// construct a write barrier around the unique string representing the field name each time.
	struct FFieldsMapKeyFuncs : TDefaultMapKeyFuncs<TWriteBarrier<VUniqueString>, VEntry, /*bInAllowDuplicateKeys*/ false>
	{
		static bool Matches(KeyInitType A, KeyInitType B);
		static bool Matches(KeyInitType A, const VUniqueString& B);
		static uint32 GetKeyHash(KeyInitType Key);
		static uint32 GetKeyHash(const VUniqueString& Key);
	};

	using FieldsMap = TMap<TWriteBarrier<VUniqueString>, VEntry, FDefaultSetAllocator, FFieldsMapKeyFuncs>;

	COREUOBJECT_API static VCppClassInfo StaticCppClassInfo;
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;
	DECLARE_VISIT_REFERENCES(COREUOBJECT_API);

	static VFields& New(FAllocationContext Context, FieldsMap&& InFields);

	FieldsMap& GetFields();

private:
	VFields(FAllocationContext Context, FieldsMap&& InFields);
	~VFields() = default;

	/// Overridden because we want to ensure that the `TMap` of offsets above gets de-allocated
	/// once the shape object lifetime ends. Otherwise it would not get its destructor called.
	static void RunDestructorImpl(VCell* This);

	template <typename TVisitor>
	static void VisitFields(FieldsMap&, TVisitor&);

	FieldsMap Fields;

	friend struct VShape;
};

/// Maps fully qualified names to offsets/constants.
struct VShape : VCell
{
	COREUOBJECT_API static VCppClassInfo StaticCppClassInfo;
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;
	DECLARE_VISIT_REFERENCES(COREUOBJECT_API);

	/// Creates a new shape. Note that indices for offset-based fields will be discarded and the fields given re-ordered
	/// indices as part of the new shape created.
	static VShape* New(FAllocationContext Context, VFields::FieldsMap&& InFields);

	const VFields::VEntry* GetField(FAllocationContext Context, const VUniqueString& Name) const;

	uint64 GetNumFields() const;

	uint64 GetNumIndexedFields() const;

	bool operator==(const VShape& Other) const;

	friend uint32 GetTypeHash(const VShape& Shape);

private:
	VShape(FAllocationContext Context, VFields::FieldsMap&& InFields);

	~VShape() = default;

	const VFields::FieldsMap& GetFields() const;

	/// Mapping of the field names to their data in the layout.
	/// This should not be mutated after initialization; if this needs to be modified, you
	/// should create a new shape and emergent type instead.
	/// We can't mark this map as `const` because we need to be able to mark the entries and names to hold strong references to them.
	VFields::FieldsMap Fields;

	/// Overridden because we want to ensure that the `TMap` of offsets above gets de-allocated
	/// once the shape object lifetime ends. Otherwise it would not get its destructor called.
	static void RunDestructorImpl(VCell* This);

	uint64 NumIndexedFields;

	friend struct VClass;
	friend struct VObject;
};

} // namespace Verse
