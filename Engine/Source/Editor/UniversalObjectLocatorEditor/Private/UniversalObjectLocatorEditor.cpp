// Copyright Epic Games, Inc. All Rights Reserved.

#include "UniversalObjectLocatorFragmentEditor.h"
#include "UniversalObjectLocator.h"

namespace UE::UniversalObjectLocator
{
	UClass* ILocatorFragmentEditor::ResolveClass(const FUniversalObjectLocatorFragment& InFragment, UObject* InContext) const
	{
		const FResolveParams ResolveParams(InContext);
		UObject* Object = InFragment.Resolve(ResolveParams).SyncGet().Object;
		return Object != nullptr ? Object->GetClass() : nullptr;
	}
}