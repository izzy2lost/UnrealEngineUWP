// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "Containers/Map.h"
#include "Templates/TypeHash.h"
#include "VVMCell.h"
#include "VVMUnreachable.h"
#include "VVMWriteBarrier.h"

namespace Verse
{
struct FAccessContext;
struct VUniqueString;

template <Verse::VCppClassInfo* ClassInfo>
struct TGlobalTrivialEmergentTypePtr;

enum class EFieldType : int8
{
	/// Whether or not the field's data is stored in the object.
	/// i.e. `c := class { X:int = 0}` versus `c := class {var X:int = 0}`
	Offset,

	/*
	 * Shapes have the ability to store constants in them.
	 * This will be the case in instances such as when we have fields that
	 * point to some sort of global entry, or if the field is initialized with a constant value.
	 * Other examples include fields that refer to functions/lambdas.
	 *
	 * Example: `a := class{ F:int = 0, G:int = 0}; A:a = a{F:= 100}` where `F` is a constant value of `100`.
	 *
	 * For `A.F`, that would just point to the constant value directly.
	 *
	 * Another example: `a := class{ B(X:int, Y:int):int = X + Y }; A:a = a{}` where `B` is a constant proc.
	 * `A.B` just points to the function directly.
	 */
	Constant
};

/// Maps fully qualified names to offsets.
struct VShape : VCell
{
	// TODO: (yiliang.siew) This will waste space for each field entry. What could be a better solution?
	// Could use `TVariant` instead, but would have to make `TWriteBarrier` work with `TVariant`, which is annoying to deal with right now.
	// Could also look into doing something fancy with `TMap` to use the unused key bits to store extra metadata.
	/// A field entry on a given shape.
	struct VEntry
	{
		union
		{
			uint64 Index;
			TWriteBarrier<VValue> Constant;
		};
		EFieldType Type;

		~VEntry() = default;

		// Must have a copy/move constructor in order to be used with `TMap` as the value type.
		VEntry(const VEntry& Other);
		VEntry(VEntry&& Other);
		VEntry(const uint64 InIndex);
		VEntry(FAccessContext Context, VValue InConstant);

		inline bool operator==(const VEntry& Other) const
		{
			if (Type != Other.Type)
			{
				return false;
			}
			switch (Type)
			{
				case EFieldType::Offset:
					return Index == Other.Index;
				case EFieldType::Constant:
					return Constant == Other.Constant;
				default:
					VERSE_UNREACHABLE();
			}
		}
	};
	using FieldsMap = TMap<TWriteBarrier<VUniqueString>, VEntry>;

	COREUOBJECT_API static VCppClassInfo StaticCppClassInfo;
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	/// Overridden to allow for marking the weak references to the strings in the field name mapping.
	COREUOBJECT_API static void MarkReferencedCellsImpl(VCell* This, FMarkStack&);

	static VShape* New(FAllocationContext Context, FieldsMap&& InFields);

	const VEntry* GetField(FAllocationContext Context, VUniqueString& Name) const;

	uint64 GetNumFields() const;

	bool operator==(const VShape& Other) const;

	friend uint32 GetTypeHash(const VShape& Shape);

private:
	VShape(FAllocationContext Context, FieldsMap&& InFields);

	~VShape() = default;

	/// Mapping of the field names to their data in the layout.
	/// This should not be mutated after initialization; if this needs to be modified, you
	/// should create a new shape and emergent type instead.
	/// We can't mark this map as `const` because we need to be able to mark the `VFieldDefinition` values during GC.
	FieldsMap Fields;

	/// Overridden because we want to ensure that the `TMap` of offsets above gets de-allocated
	/// once the shape object lifetime ends. Otherwise it would not get its destructor called.
	static void RunDestructorImpl(VCell* This);
};
} // namespace Verse
