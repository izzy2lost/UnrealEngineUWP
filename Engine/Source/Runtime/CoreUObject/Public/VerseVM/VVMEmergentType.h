// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "VVMCell.h"
#include "VVMEmergentTypeCreator.h"
#include "VVMGlobalHeapPtr.h"
#include "VVMType.h"
#include <new>

namespace Verse
{
struct VShape;

struct VEmergentType final : VCell
{
	COREUOBJECT_API static VCppClassInfo StaticCppClassInfo;

	// Don't mutate this. If you need to, make a new emergent type that points to your new shape instead.
	const VShape* const Shape = nullptr;
	TWriteBarrier<VType> Type;
	VCppClassInfo* CppClassInfo = nullptr;

	static VEmergentType* New(FAllocationContext Context, VType* Type, VCppClassInfo* CppClassInfo)
	{
		return new (Context.AllocateEmergentType(sizeof(VEmergentType))) VEmergentType(Context, VEmergentTypeCreator::EmergentTypeForEmergentType.Get(), Type, CppClassInfo);
	}

	static VEmergentType* New(FAllocationContext Context, const VShape* InShape, VType* Type, VCppClassInfo* CppClassInfo)
	{
		return new (Context.AllocateEmergentType(sizeof(VEmergentType))) VEmergentType(Context, InShape, VEmergentTypeCreator::EmergentTypeForEmergentType.Get(), Type, CppClassInfo);
	}

	static bool Equals(const VEmergentType& EmergentType, VType* Type, VCppClassInfo* CppClassInfo)
	{
		return EmergentType.Shape == nullptr && EmergentType.Type.Get() == Type && EmergentType.CppClassInfo == CppClassInfo;
	}

	static bool Equals(const VEmergentType& EmergentType, const VShape* InShape, VType* Type, VCppClassInfo* CppClassInfo)
	{
		return EmergentType.Shape == InShape && EmergentType.Type.Get() == Type && EmergentType.CppClassInfo == CppClassInfo;
	}

	COREUOBJECT_API static void MarkReferencedCellsImpl(VCell* This, FMarkStack&);

	friend uint32 GetTypeHash(const VEmergentType& EmergentType)
	{
		uint32 Hash = HashCombineFast(::GetTypeHash(EmergentType.Shape), ::GetTypeHash(EmergentType.Type.Get()->Tag));
		Hash = HashCombineFast(Hash, ::GetTypeHash(EmergentType.CppClassInfo));
		return Hash;
	}

private:
	friend class VEmergentTypeCreator;

	// Need this for the EmergentType of EmergentType.
	static VEmergentType* NewIncomplete(FAllocationContext Context, VCppClassInfo* CppClassInfo)
	{
		return new (Context.AllocateEmergentType(sizeof(VEmergentType))) VEmergentType(Context, CppClassInfo);
	}

	void SetEmergentType(FAccessContext Context, VEmergentType* EmergentType)
	{
		VCell::SetEmergentType(Context, EmergentType);
	}

	VEmergentType(FAllocationContext Context, VCppClassInfo* CppClassInfo)
		: VCell()
		, Shape(nullptr)
		, CppClassInfo(CppClassInfo)
	{
	}

	VEmergentType(FAllocationContext Context, VEmergentType* EmergentType, VType* T, VCppClassInfo* CppClassInfo)
		: VCell(Context, EmergentType)
		, Shape(nullptr)
		, Type(Context, T)
		, CppClassInfo(CppClassInfo)
	{
	}

	VEmergentType(FAllocationContext Context, const VShape* InShape, VEmergentType* EmergentType, VType* InType, VCppClassInfo* CppClassInfo)
		: VCell(Context, EmergentType)
		, Shape(InShape)
		, Type(Context, InType)
		, CppClassInfo(CppClassInfo)
	{
	}
};

}; // namespace Verse
