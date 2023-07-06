// Copyright Epic Games, Inc. All Rights Reserved.

#include "SelectPropertyFromUClassModel.h"

#include "ConcertSyncCoreReplicatedPropertySource.h"

#define LOCTEXT_NAMESPACE "FSelectPropertyFromUClassModel"

namespace UE::MultiUserReplicationEditor
{
	FSelectPropertyFromUClassModel::FSelectPropertyFromUClassModel()
		: UClassIteratorSource(MakeShared<FConcertSyncCoreReplicatedPropertySource>())
	{}

	TSharedRef<IPropertySourceModel> FSelectPropertyFromUClassModel::GetPropertySource(const FSoftClassPath& Class) const
	{
		UClass* LoadedClass = Class.TryLoadClass<UObject>();
		UClassIteratorSource->SetClass(LoadedClass);
		return UClassIteratorSource;
	}
}

#undef LOCTEXT_NAMESPACE