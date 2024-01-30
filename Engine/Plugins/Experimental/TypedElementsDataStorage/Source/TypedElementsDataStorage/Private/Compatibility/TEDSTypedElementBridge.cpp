// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compatibility/TEDSTypedElementBridge.h"

#include "MassActorSubsystem.h"
#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "Elements/Framework/TypedElementList.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Framework/TypedElementRegistry.h"

namespace UTEDSTypedElementBridge_Private
{
	bool bBridgeEnabled = false;
	TYPEDELEMENTSDATASTORAGE_API FAutoConsoleVariableRef CVarBridgeEnabled(
		TEXT("TEDS.TyepedElementBridge.Enable"),
		bBridgeEnabled,
		TEXT("Automatically populated TEDS with TypedElementHandles"));
}

uint8 UTEDSTypedElementBridge::GetOrder() const
{
	return 110;
}

void UTEDSTypedElementBridge::PreRegister(ITypedElementDataStorageInterface& DataStorage)
{
	Super::PreRegister(DataStorage);
	DebugEnabledDelegateHandle = UTEDSTypedElementBridge_Private::CVarBridgeEnabled->OnChangedDelegate().AddUObject(this, &UTEDSTypedElementBridge::HandleOnEnabled);
}

void UTEDSTypedElementBridge::PreShutdown(ITypedElementDataStorageInterface& DataStorage)
{
	UTEDSTypedElementBridge_Private::CVarBridgeEnabled->OnChangedDelegate().Remove(DebugEnabledDelegateHandle);
	DebugEnabledDelegateHandle.Reset();
	CleanupTypedElementColumns(DataStorage);

	Super::PreShutdown(DataStorage);
}

void UTEDSTypedElementBridge::RegisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
	Super::RegisterQueries(DataStorage);

	if (IsEnabled())
	{
		RegisterQuery_NewUObject(DataStorage);
	}
}

void UTEDSTypedElementBridge::RegisterQuery_NewUObject(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	if (!ensureMsgf(QueryHandle == TypedElementInvalidQueryHandle, TEXT("Query already registered")))
	{
		return;
	}

	// TODO Need a two-step initialize since the FObserver::Add gets called before FTypedElementUObjectColumn is
	// ever populated.

	QueryHandle = DataStorage.RegisterQuery(
		Select(
		TEXT("Watch for new UObjects"),
			FObserver::OnAdd<FTypedElementUObjectColumn>(),
			[](DSI::IQueryContext& Context, TypedElementRowHandle Row, const FTypedElementUObjectColumn& Object)
			{
				Context.AddColumns<FPendingPopulateTypedElementColumn>(Row);
			}
		)
		.Compile());
	
	RemoveTypedElementRowHandleQuery = DataStorage.RegisterQuery(
		Select()
			.ReadOnly<FTEDSTypedElementColumn>()
		.Compile());
	
	RemovePendingTypedElementRowHandleQuery = DataStorage.RegisterQuery(
			Select().Where().All<FPendingPopulateTypedElementColumn>()
		.Compile());
}

void UTEDSTypedElementBridge::UnregisterQuery_NewUObject(ITypedElementDataStorageInterface& DataStorage)
{
	DataStorage.UnregisterQuery(QueryHandle);
	QueryHandle = TypedElementInvalidQueryHandle;
}

FOnTEDSTypedElementBridgeEnable& UTEDSTypedElementBridge::OnEnabled()
{
	static FOnTEDSTypedElementBridgeEnable OnBridgeEnabled;
	return OnBridgeEnabled;
}

bool UTEDSTypedElementBridge::IsEnabled()
{
	return UTEDSTypedElementBridge_Private::CVarBridgeEnabled->GetBool();
}

void UTEDSTypedElementBridge::CleanupTypedElementColumns(ITypedElementDataStorageInterface& DataStorage)
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
		
		DataStorage.BatchAddRemoveColumns(TConstArrayView<TypedElementRowHandle>(Handles), {}, {FTEDSTypedElementColumn::StaticStruct()});
	}

	// Remove any columns on row indicating they are pending population of their TEv1 columnn
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
		
		DataStorage.BatchAddRemoveColumns(TConstArrayView<TypedElementRowHandle>(Handles), {}, {FPendingPopulateTypedElementColumn::StaticStruct()});
	}
}

void UTEDSTypedElementBridge::HandleOnEnabled(IConsoleVariable* CVar)
{
	ITypedElementDataStorageInterface* DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage();
	bool bIsEnabled = CVar->GetBool();

	if (bIsEnabled)
	{
		DataStorage->UnregisterQuery(QueryHandle);
		QueryHandle = TypedElementInvalidQueryHandle;
		
		RegisterQuery_NewUObject(*DataStorage);
		UTEDSTypedElementBridge::OnEnabled().Broadcast(bIsEnabled);
	}
	else
	{
		UTEDSTypedElementBridge::OnEnabled().Broadcast(bIsEnabled);
		UnregisterQuery_NewUObject(*DataStorage);
		CleanupTypedElementColumns(*DataStorage);
	}
}
