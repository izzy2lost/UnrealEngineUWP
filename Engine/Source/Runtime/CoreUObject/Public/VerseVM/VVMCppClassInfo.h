// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
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

struct FAbstractVisitorDispatch;
struct FMarkStackVisitorDispatch;
template <typename TVisitor>
struct TVisitorWrapper;
using FAbstractVisitor = TVisitorWrapper<FAbstractVisitorDispatch>;
using FMarkStackVisitor = TVisitorWrapper<FMarkStackVisitorDispatch>;

#define DECLARE_VISIT_REFERENCES(Api)                                             \
private:                                                                          \
	template <typename TVisitor>                                                  \
	FORCEINLINE void VisitReferencesImpl(TVisitor&);                              \
                                                                                  \
public:                                                                           \
	Api static void VisitReferences(::Verse::VCell*, ::Verse::FAbstractVisitor&); \
	Api static void VisitReferences(::Verse::VCell*, ::Verse::FMarkStackVisitor&);

// VVMAbstractVisitorInline.h and VVMMarkStackVisitor.h need to be included in a source to use DEFINE_VISIT_REFERENCES
#define DEFINE_VISIT_REFERENCES(ClassName)                                                     \
	void ClassName::VisitReferences(::Verse::VCell* This, ::Verse::FAbstractVisitor& Visitor)  \
	{                                                                                          \
		::Verse::FAbstractVisitorDispatch::FReferrerContext Context(Visitor, This);            \
		static_cast<ClassName*>(This)->VisitReferencesImpl(Visitor);                           \
	}                                                                                          \
	void ClassName::VisitReferences(::Verse::VCell* This, ::Verse::FMarkStackVisitor& Visitor) \
	{                                                                                          \
		static_cast<ClassName*>(This)->VisitReferencesImpl(Visitor);                           \
	}

#define DEFINE_VCPPCLASSINFO_IMPL(CellType, SuperClassInfoPtr, Name) \
	::Verse::VCppClassInfo CellType::StaticCppClassInfo = {          \
		(Name),                                                      \
		(SuperClassInfoPtr),                                         \
		CellType::VisitReferences,                                   \
		CellType::VisitReferences,                                   \
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
	void (*MarkReferencesImpl)(VCell* This, FMarkStackVisitor&);
	void (*VisitReferencesImpl)(VCell* This, FAbstractVisitor&);
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

	FORCEINLINE void VisitReferences(VCell* This, FMarkStackVisitor& Visitor)
	{
		MarkReferencesImpl(This, Visitor);
	}

	FORCEINLINE void VisitReferences(VCell* This, FAbstractVisitor& Visitor)
	{
		VisitReferencesImpl(This, Visitor);
	}

	COREUOBJECT_API FString DebugName() const;
};

} // namespace Verse
