// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Editor/Model/PropertySource/SelectPropertyFromUClassModel.h"

#include "Replication/Editor/Model/Property/IPropertySourceModel.h"
#include "Replication/Editor/Model/PropertySource/ConcertSyncCoreReplicatedPropertySource.h"

#include "ConcertLogGlobal.h"

#define LOCTEXT_NAMESPACE "FSelectPropertyFromUClassModel"

namespace UE::ConcertClientSharedSlate
{
	void FSelectPropertyFromUClassModel::ProcessPropertySource(
		const ConcertSharedSlate::FPropertySourceContext& Context,
		TFunctionRef<void(const ConcertSharedSlate::IPropertySourceModel& Model)> Processor
		) const
	{
		UClass* LoadedClass = Context.Class.TryLoadClass<UObject>();
		
		FConcertSyncCoreReplicatedPropertySource UClassIteratorSource;
		UClassIteratorSource.SetClass(LoadedClass ? LoadedClass : nullptr);
		UE_CLOG(!LoadedClass, LogConcert, Warning, TEXT("Could not resolve class %s. Properties will not be available."), *Context.Class.ToString());
		Processor(UClassIteratorSource);
	}
}

#undef LOCTEXT_NAMESPACE