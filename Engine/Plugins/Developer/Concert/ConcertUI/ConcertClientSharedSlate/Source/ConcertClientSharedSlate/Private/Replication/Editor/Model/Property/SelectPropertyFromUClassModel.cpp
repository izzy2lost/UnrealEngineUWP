// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Editor/Model/Property/SelectPropertyFromUClassModel.h"

#include "Replication/Editor/Model/Property/ConcertSyncCoreReplicatedPropertySource.h"

#define LOCTEXT_NAMESPACE "FSelectPropertyFromUClassModel"

namespace UE::ConcertClientSharedSlate
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