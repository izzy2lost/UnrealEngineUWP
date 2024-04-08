// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMUClass.h"

FORCEINLINE_DEBUGGABLE FVRestValueProperty* UVerseVMClass::GetPropertyForField(Verse::FAllocationContext Context, Verse::VUniqueString& FieldName) const
{
	using namespace Verse;

	const VShape::VEntry* Field = Shape->GetField(Context, FieldName);
	if (!Field)
	{
		V_DIE("Field: %s was not found!", *FieldName.AsString());
	}
	checkSlow(Field->Type == EFieldType::FProperty);
	return Field->Property;
}

#endif // WITH_VERSE_VM
