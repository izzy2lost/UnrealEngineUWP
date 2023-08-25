// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "HAL/Platform.h"
#include "Templates/Function.h"

class FString;

namespace Verse
{
struct FMarkStack;
struct VCell;
struct FRunningContext;
struct VValue;

#define DEFINE_VCPPCLASSINFO_IMPL(CellType, SuperClassInfoPtr, Name) \
	::Verse::VCppClassInfo CellType::StaticCppClassInfo = {          \
		(Name),                                                      \
		(SuperClassInfoPtr),                                         \
		CellType::MarkReferencedCellsImpl,                           \
		CellType::ConductCensusImpl,                                 \
		CellType::RunDestructorImpl,                                 \
		CellType::EqualImpl,                                         \
		CellType::GetTypeHashImpl}

#define DEFINE_VCPPCLASSINFO(CellType, SuperCellType, Name) \
	DEFINE_VCPPCLASSINFO_IMPL(CellType, &SuperCellType::StaticCppClassInfo, Name)

// C++ information; this is where the "vtable" goes.
struct VCppClassInfo
{
	const TCHAR* Name;
	VCppClassInfo* SuperClass;
	void (*MarkReferencedCells)(VCell* This, FMarkStack&);
	void (*ConductCensus)(VCell* This);
	void (*RunDestructor)(VCell* This);
	bool (*Equal)(FRunningContext Context, VCell* This, VCell* Other, TFunction<void(VValue, VValue)> HandlePlaceholder);
	uint32 (*GetTypeHash)(VCell* This);

	bool IsA(const VCppClassInfo* Other) const
	{
		for (const VCppClassInfo* Current = this; Current; Current = Current->SuperClass)
		{
			if (Current == Other)
			{
				return true;
			}
		}
		return false;
	}

	COREUOBJECT_API FString DebugName() const;
};

} // namespace Verse
