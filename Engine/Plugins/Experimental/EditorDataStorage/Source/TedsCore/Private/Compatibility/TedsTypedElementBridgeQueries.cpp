// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compatibility/TedsTypedElementBridgeQueries.h"

#include "Compatibility/TedsTypedElementBridge.h"
#include "Compatibility/Columns/TypedElement.h"
#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "Elements/Framework/TypedElementList.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "HAL/IConsoleManager.h"

namespace UE::Editor::DataStorage::Compatibility::Private
{
	bool bBridgeEnabled = false;
	FAutoConsoleVariableRef CVarBridgeEnabled(
		TEXT("TEDS.TypedElementBridge.Enable"),
		bBridgeEnabled,
		TEXT("Automatically populated TEDS with TypedElementHandles"));
} // namespace UE::Editor::DataStorage::Compatibility::Private

uint8 UTypedElementBridgeDataStorageFactory::GetOrder() const
{
	return 110;
}

void UTypedElementBridgeDataStorageFactory::PreRegister(ITypedElementDataStorageInterface& DataStorage)
{
	Super::PreRegister(DataStorage);
	DebugEnabledDelegateHandle = UE::Editor::DataStorage::Compatibility::Private::CVarBridgeEnabled->OnChangedDelegate().AddUObject(this, &UTypedElementBridgeDataStorageFactory::HandleOnEnabled);
}

void UTypedElementBridgeDataStorageFactory::PreShutdown(ITypedElementDataStorageInterface& DataStorage)
{
	UE::Editor::DataStorage::Compatibility::Private::CVarBridgeEnabled->OnChangedDelegate().Remove(DebugEnabledDelegateHandle);
	DebugEnabledDelegateHandle.Reset();
	CleanupTypedElementColumns(DataStorage);

	Super::PreShutdown(DataStorage);
}

void UTypedElementBridgeDataStorageFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
	Super::RegisterQueries(DataStorage);

	if (IsEnabled())
	{
		RegisterQuery_NewUObject(DataStorage);
	}
}

bool UTypedElementBridgeDataStorageFactory::IsEnabled()
{
	return UE::Editor::DataStorage::Compatibility::Private::CVarBridgeEnabled->GetBool();
}

void UTypedElementBridgeDataStorageFactory::RegisterQuery_NewUObject(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;
	
	RemoveTypedElementRowHandleQuery = DataStorage.RegisterQuery(
		Select()
			.ReadOnly<UE::Editor::DataStorage::Compatibility::FTypedElementColumn>()
		.Compile());
}

void UTypedElementBridgeDataStorageFactory::UnregisterQuery_NewUObject(ITypedElementDataStorageInterface& DataStorage)
{
}

void UTypedElementBridgeDataStorageFactory::CleanupTypedElementColumns(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace TypedElementQueryBuilder;
	using namespace TypedElementDataStorage;

	// Remove any TEv1 handles
	{
		TArray<TypedElementRowHandle> Handles;
		using namespace TypedElementQueryBuilder;
		DataStorage.RunQuery(
			RemoveTypedElementRowHandleQuery,
			CreateDirectQueryCallbackBinding(
				[&Handles](ITypedElementDataStorageInterface::IDirectQueryContext& Context)
				{
					Handles.Append(Context.GetRowHandles());
				}));
		
		DataStorage.BatchAddRemoveColumns(TConstArrayView<TypedElementRowHandle>(Handles), {}, {UE::Editor::DataStorage::Compatibility::FTypedElementColumn::StaticStruct()});
	}
}

void UTypedElementBridgeDataStorageFactory::HandleOnEnabled(IConsoleVariable* CVar)
{
	ITypedElementDataStorageInterface* DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage();
	bool bIsEnabled = CVar->GetBool();

	if (bIsEnabled)
	{
		RegisterQuery_NewUObject(*DataStorage);
		UE::Editor::DataStorage::Compatibility::OnTypedElementBridgeEnabled().Broadcast(bIsEnabled);
	}
	else
	{
		UE::Editor::DataStorage::Compatibility::OnTypedElementBridgeEnabled().Broadcast(bIsEnabled);
		UnregisterQuery_NewUObject(*DataStorage);
		CleanupTypedElementColumns(*DataStorage);
	}
}
