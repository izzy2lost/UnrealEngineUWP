// Copyright Epic Games, Inc. All Rights Reserved.

#include "Processors/RevisionControlProcessors.h"

#include "ISourceControlModule.h"
#include "SourceControlFileStatusMonitor.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"

#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementRevisionControlColumns.h"
#include "Elements/Columns/TypedElementSelectionColumns.h"
#include "Elements/Columns/TypedElementViewportColumns.h"
#include "Elements/Common/EditorDataStorageFeatures.h"
#include "Elements/Common/TypedElementDataStorageLog.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Interfaces/TypedElementQueryStorageInterfaces.h"

namespace UE::Editor::RevisionControl::Private
{
	extern FAutoConsoleVariableRef CVarAutoPopulateState;

	// Update the overlay color for all rows with InColumn
	void UpdateSCCOverlayStates(UScriptStruct* InColumn)
	{
		using namespace UE::Editor::DataStorage;
		IEditorDataStorageProvider* DataStorage = GetMutableDataStorageFeature<IEditorDataStorageProvider>(StorageFeatureName);

		if(const URevisionControlDataStorageFactory* Factory = DataStorage->FindFactory<URevisionControlDataStorageFactory>())
		{
			Factory->UpdateOverlaysForSCCState(DataStorage, InColumn);
		}
	}

	// Update all currently existing overlay colors
	void UpdateOverlayColors()
	{
		using namespace UE::Editor::DataStorage;
		IEditorDataStorageProvider* DataStorage = GetMutableDataStorageFeature<IEditorDataStorageProvider>(StorageFeatureName);

		if(const URevisionControlDataStorageFactory* Factory = DataStorage->FindFactory<URevisionControlDataStorageFactory>())
		{
			Factory->UpdateOverlayColors(DataStorage);
		}
	}
	
	

	static bool gEnableOverlays = false;
	TAutoConsoleVariable<bool> CVarEnableOverlays(
		TEXT("RevisionControl.Overlays.Enable"),
		gEnableOverlays,
		TEXT("Enables overlays."),
		ECVF_Default);

	static bool gEnableOverlayCheckedOutByOtherUser = true;
	TAutoConsoleVariable<bool> CVarEnableOverlayCheckedOutByOtherUser(
		TEXT("RevisionControl.Overlays.CheckedOutByOtherUser.Enable"),
		gEnableOverlayCheckedOutByOtherUser,
		TEXT("Enables overlays for files that are checked out by another user."),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable*)
		{
			// StaticStruct fails if the module containing the column isn't loaded yet.
			// There's no SCC rows to update on startup so simply skipping the call is fine
			if (FModuleManager::Get().IsModuleLoaded("TypedElementFramework"))
			{
				UpdateSCCOverlayStates(FSCCExternallyLockedColumn::StaticStruct());
			}
		}),
		ECVF_Default);

	static bool gEnableOverlayNotAtHeadRevision = true;
	TAutoConsoleVariable<bool> CVarEnableOverlayNotAtHeadRevision(
		TEXT("RevisionControl.Overlays.NotAtHeadRevision.Enable"),
		gEnableOverlayNotAtHeadRevision,
		TEXT("Enables overlays for files that are not at the latest revision."),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable*)
		{
			// StaticStruct fails if the module containing the column isn't loaded yet.
			// There's no SCC rows to update on startup so simply skipping the call is fine
			if (FModuleManager::Get().IsModuleLoaded("TypedElementFramework"))
			{
				UpdateSCCOverlayStates(FSCCNotCurrentTag::StaticStruct());
			}
		}),
		ECVF_Default);

	static bool gEnableOverlayCheckedOut = false;
	TAutoConsoleVariable<bool> CVarEnableOverlayCheckedOut(
		TEXT("RevisionControl.Overlays.CheckedOut.Enable"),
		gEnableOverlayCheckedOut,
		TEXT("Enables overlays for files that are checked out by user."),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable*)
		{
			// StaticStruct fails if the module containing the column isn't loaded yet.
			// There's no SCC rows to update on startup so simply skipping the call is fine
			if (FModuleManager::Get().IsModuleLoaded("TypedElementFramework"))
			{
				UpdateSCCOverlayStates(FSCCLockedTag::StaticStruct());
			}
		}),
		ECVF_Default);

	static bool gEnableOverlayOpenForAdd = false;
	TAutoConsoleVariable<bool> CVarEnableOverlayOpenForAdd(
		TEXT("RevisionControl.Overlays.OpenForAdd.Enable"),
		gEnableOverlayOpenForAdd,
		TEXT("Enables overlays for files that are newly added."),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable*)
		{
			// StaticStruct fails if the module containing the column isn't loaded yet.
			// There's no SCC rows to update on startup so simply skipping the call is fine
			if (FModuleManager::Get().IsModuleLoaded("TypedElementFramework"))
			{
				UpdateSCCOverlayStates(FSCCStatusColumn::StaticStruct());
			}
		}),
		ECVF_Default);

	static int32 gOverlayAlpha = 20; // [0..100]
	TAutoConsoleVariable<int32> CVarOverlayAlpha(
		TEXT("RevisionControl.Overlays.Alpha"),
		gOverlayAlpha,
		TEXT("Configures overlay opacity."),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable*)
		{
			// StaticStruct fails if the module containing the column isn't loaded yet.
			// There's no SCC rows to update on startup so simply skipping the call is fine
			if (FModuleManager::Get().IsModuleLoaded("TypedElementFramework"))
			{
				UpdateOverlayColors();
			}
		}),
		ECVF_Default);

	#if UE_BUILD_SHIPPING
	#define ENABLE_OVERLAY_DEBUG 0
	#else
	#define ENABLE_OVERLAY_DEBUG 1
	#endif

	#if ENABLE_OVERLAY_DEBUG
	static int32 gDefaultDebugForceColorOnAllValue = false; // [0..100]
	TAutoConsoleVariable<int32> CVarDebugForceColorOnAll(
		TEXT("RevisionControl.Overlays.Debug.ForceColorOnAll"),
		gDefaultDebugForceColorOnAllValue,
		TEXT("Debug to force overlay color on everything. 1 = Red, 2 = Green, 3 = Blue, 4 = White. 0 = off  ."),
		ECVF_Default);
	#endif

	using namespace UE::Editor::DataStorage;
	static FColor DetermineOverlayColor(const ICommonQueryContext& SCCContext, const FTypedElementUObjectColumn& Actor, bool bSelected)
	{
		check(IsInGameThread());

	#if ENABLE_OVERLAY_DEBUG
		if (int32 Force = CVarDebugForceColorOnAll.GetValueOnGameThread())
		{
			int32 Alpha = FMath::Lerp<float>(0.f, 255.f, gOverlayAlpha / 100.f);
			switch(Force)
			{
			case 1:
				return FColor(255, 0, 0, Alpha);
			case 2:
				return FColor(0, 255, 0, Alpha);
			case 3:
				return FColor(0, 0, 255, Alpha);
			case 4:
				return FColor(255, 255, 255, Alpha);
			// Do normal determination for higher than 4
			}
		}
	#endif

		bool bExternal = Actor.Object.IsValid() ? Cast<AActor>(Actor.Object)->IsPackageExternal() : false;
		bool bIgnored = !bExternal;
		if (!bIgnored && !bSelected)
		{
			// Convert CVar value from [0..100] to [0..255] range.
			int32 Alpha = FMath::Lerp<float>(0.f, 255.f, CVarOverlayAlpha.GetValueOnGameThread() / 100.f);

			// Check if the package is outdated because there is a newer version available.
			if (SCCContext.HasColumn<FSCCNotCurrentTag>())
			{
				if (CVarEnableOverlayNotAtHeadRevision.GetValueOnGameThread())
				{
					// Yellow.
					return FColor(225, 255, 61, Alpha);
				}
			}

			// Check if the package is locked by someone else.
			if (SCCContext.HasColumn<FSCCExternallyLockedColumn>())
			{
				if (CVarEnableOverlayCheckedOutByOtherUser.GetValueOnGameThread())
				{
					// Red.
					return FColor(239, 53, 53, Alpha);
				}
			}

			// Check if the package is added locally.
			if (SCCContext.HasColumn<FSCCStatusColumn>())
			{
				if (CVarEnableOverlayOpenForAdd.GetValueOnGameThread())
				{
					if (const FSCCStatusColumn* StatusColumn = SCCContext.GetColumn<FSCCStatusColumn>())
					{
						if (StatusColumn->Modification == ESCCModification::Added)
						{
							// Blue.
							return FColor(0, 112, 224, Alpha);
						}
					}
				}
			}

			// Check if the package is locked by self.
			if (SCCContext.HasColumn<FSCCLockedTag>())
			{
				if (CVarEnableOverlayCheckedOut.GetValueOnGameThread())
				{
					// Green.
					return FColor(31, 228, 75, Alpha);
				}
			}
		}

		return FColor(ForceInitToZero);
	}
} // namespace UE::Editor::RevisionControl::Private

void URevisionControlDataStorageFactory::RegisterTables(IEditorDataStorageProvider& DataStorage)
{
	DataStorage.RegisterTable(
		TTypedElementColumnTypeList<
			FTypedElementPackagePathColumn, FTypedElementPackageLoadedPathColumn,
			FSCCRevisionIdColumn, FSCCExternalRevisionIdColumn>(),
		FName("Editor_RevisionControlTable"));

	DataStorage.RegisterTable(
		TTypedElementColumnTypeList<FTypedElementPackageUpdateColumn>(),
		FName("Editor_PackageUpdateTable"));
}

void URevisionControlDataStorageFactory::RegisterQueries(IEditorDataStorageProvider& DataStorage)
{
	using namespace UE::Editor::DataStorage;
	using namespace UE::Editor::RevisionControl::Private;
	
	CVarAutoPopulateState->AsVariable()->OnChangedDelegate().AddLambda(
		[this, &DataStorage](IConsoleVariable* AutoPopulate)
		{
			if (AutoPopulate->GetBool())
			{
				RegisterFetchUpdates(DataStorage);
			}
			else
			{
				DataStorage.UnregisterQuery(FetchUpdates);
				FetchUpdates = InvalidQueryHandle;
			}
		}
	);

	CVarEnableOverlays->AsVariable()->OnChangedDelegate().AddLambda(
		[this, &DataStorage](IConsoleVariable* EnableOverlays)
		{
			if (EnableOverlays->GetBool())
			{
				DataStorage.UnregisterQuery(RemoveOverlays);
				RemoveOverlays = InvalidQueryHandle;

				RegisterApplyOverlays(DataStorage);
			}
			else
			{
				DataStorage.UnregisterQuery(ApplyNewOverlays);
				ApplyNewOverlays = InvalidQueryHandle;
				DataStorage.UnregisterQuery(ChangeOverlay);
				ChangeOverlay = InvalidQueryHandle;

				DataStorage.UnregisterQuery(ApplyOverlaysObjectToSCC);
				ApplyOverlaysObjectToSCC = InvalidQueryHandle;

				DataStorage.UnregisterQuery(SelectionAdded);
				SelectionAdded = InvalidQueryHandle;

				DataStorage.UnregisterQuery(SelectionRemoved);
				SelectionRemoved = InvalidQueryHandle;

				DataStorage.UnregisterQuery(FlushPackageUpdates);
				FlushPackageUpdates = InvalidQueryHandle;

				RegisterRemoveOverlays(DataStorage);
			}
		}
	);
	
	if (CVarAutoPopulateState->GetBool())
	{
		RegisterFetchUpdates(DataStorage);
	}

	if (CVarEnableOverlays->GetBool())
	{
		RegisterApplyOverlays(DataStorage);
	}
	else
	{
		RegisterRemoveOverlays(DataStorage);
	}
	
	RegisterGeneralQueries(DataStorage);
}

void URevisionControlDataStorageFactory::UpdateOverlaysForSCCState(IEditorDataStorageProvider* DataStorage, const UScriptStruct* Column) const
{
	using namespace UE::Editor::DataStorage::Queries;

	// The column provided must be present in our map
	const QueryHandle* QueryForColumn = GeneralQueriesMap.Find(Column);
	if(!QueryForColumn)
	{
		return;
	}

	// Collect all SCC and Object rows to request a package update
	TArray<FTypedElementPackageUpdateColumn> PackagesToUpdate;

	DirectQueryCallback RowCollector = CreateDirectQueryCallbackBinding(
		[&PackagesToUpdate, DataStorage] (const IDirectQueryContext& Context, const FTypedElementPackageReference* PackageReference)
		{
			uint32 RowCount = Context.GetRowCount();
			TConstArrayView<RowHandle> RowHandles = Context.GetRowHandles();

			for(uint32 RowIndex = 0; RowIndex < RowCount; ++RowIndex)
			{
				if(DataStorage->IsRowAvailable(PackageReference[RowIndex].Row))
				{
					// We are querying the SCC rows so the package row is the one present in the callback and the object
					// row is the one present in FTypedElementPackageReference
 					PackagesToUpdate.Add(FTypedElementPackageUpdateColumn{.ObjectRow = PackageReference[RowIndex].Row, .PackageRow = RowHandles[RowIndex]});
				}
			}
		});

	DataStorage->RunQuery(*QueryForColumn, RowCollector);
	RequestPackageUpdates(DataStorage, PackagesToUpdate);
}

void URevisionControlDataStorageFactory::UpdateOverlayColors(IEditorDataStorageProvider* DataStorage) const
{
	using namespace UE::Editor::DataStorage::Queries;

	TArray<FTypedElementPackageUpdateColumn> PackagesToUpdate;

	DirectQueryCallback RowCollector = CreateDirectQueryCallbackBinding(
		[&PackagesToUpdate] (const IDirectQueryContext& Context, const FTypedElementPackageReference* PackageReference)
		{
			uint32 RowCount = Context.GetRowCount();
			TConstArrayView<RowHandle> RowHandles = Context.GetRowHandles();

			for(uint32 RowIndex = 0; RowIndex < RowCount; ++RowIndex)
			{
				if(PackageReference[RowIndex].Row != InvalidRowHandle)
				{
					// The viewport color column is on the actor rows, so the row handle in the callback is the Object Row and the row handle
					// in FTypedElementPackageReference is the SCC row
					PackagesToUpdate.Add(FTypedElementPackageUpdateColumn{.ObjectRow = RowHandles[RowIndex], .PackageRow = PackageReference[RowIndex].Row});
				}
			}
		});

	DataStorage->RunQuery(FetchOverlayColors, RowCollector);
	RequestPackageUpdates(DataStorage, PackagesToUpdate);

}

void URevisionControlDataStorageFactory::RegisterFetchUpdates(IEditorDataStorageProvider& DataStorage)
{
	using namespace UE::Editor::DataStorage::Queries;
	
	FSourceControlFileStatusMonitor& FileStatusMonitor = ISourceControlModule::Get().GetSourceControlFileStatusMonitor();

	if (FetchUpdates == InvalidQueryHandle)
	{
		FetchUpdates = DataStorage.RegisterQuery(
			Select(
				TEXT("Gather source control statuses for objects with unresolved package paths"),
				FObserver::OnAdd<FTypedElementPackageUnresolvedReference>()
					.SetExecutionMode(EExecutionMode::GameThread),
				[this, &FileStatusMonitor](IQueryContext& Context, const FTypedElementPackageUnresolvedReference& UnresolvedReference)
				{
					static FSourceControlFileStatusMonitor::FOnSourceControlFileStatus EmptyDelegate{};
				
					FileStatusMonitor.StartMonitoringFile(
						reinterpret_cast<uintptr_t>(this),
						UnresolvedReference.PathOnDisk,
						EmptyDelegate
						
					);
				})
			.Compile());
	}
}

void URevisionControlDataStorageFactory::RegisterApplyOverlays(IEditorDataStorageProvider& DataStorage)
{
	using namespace UE::Editor::DataStorage::Queries;
	using namespace UE::Editor::RevisionControl::Private;
	
	if (ApplyOverlaysObjectToSCC == InvalidQueryHandle)
	{
		ApplyOverlaysObjectToSCC = DataStorage.RegisterQuery(
			Select()
				.ReadOnly<FTypedElementPackagePathColumn>()
				.ReadOnly<FSCCStatusColumn>(EOptional::Yes)
			.Compile());
	}

	if (ApplyNewOverlays == InvalidQueryHandle)
	{
		ApplyNewOverlays = DataStorage.RegisterQuery(
			Select()
			.ReadOnly<FTypedElementUObjectColumn, FTypedElementPackageReference>()
			.Where()
				.All<FTypedElementActorTag>()
				.None<FTypedElementViewportOverlayColorColumn>()
			.Compile());
	}

	if (ChangeOverlay == InvalidQueryHandle)
	{
		ChangeOverlay = DataStorage.RegisterQuery(
			Select()
			.ReadOnly<FTypedElementUObjectColumn, FTypedElementPackageReference, FTypedElementViewportOverlayColorColumn>()
			.Where()
				.All<FTypedElementActorTag>()
			.Compile());
	}

	enum EFlushPackageUpdatesSubqueries
	{
		EApplyOverlaysObjectToSCC,
		EApplyNewOverlays,
		EChangeOverlay,
			
		Num
	};
	TArray<RowHandle> Subqueries;
	Subqueries.AddUninitialized(EFlushPackageUpdatesSubqueries::Num);
	Subqueries[EApplyOverlaysObjectToSCC] = ApplyOverlaysObjectToSCC;
	Subqueries[EApplyNewOverlays] = ApplyNewOverlays;
	Subqueries[EChangeOverlay] = ChangeOverlay;

	if (FlushPackageUpdates == InvalidQueryHandle)
	{
		check(ApplyOverlaysObjectToSCC != InvalidQueryHandle && ApplyNewOverlays != InvalidQueryHandle && ChangeOverlay!= InvalidQueryHandle);
		
		FlushPackageUpdates = DataStorage.RegisterQuery(
			Select(
				TEXT("Consume collected package updates"),
				FProcessor(EQueryTickPhase::PrePhysics, DataStorage.GetQueryTickGroupName(EQueryTickGroups::Update))
					.SetExecutionMode(EExecutionMode::GameThread),
				[](IQueryContext& Context, RowHandle Row, const FTypedElementPackageUpdateColumn& Update)
				{
					// Query:
					// For all actors without an overlay color column AND having a package reference:
					//   Determine if a color should be applied based on SCC status tags
					//   If so, add the OverlayColorColumn to the actor row
					Context.RunSubquery(EApplyNewOverlays, Update.ObjectRow, CreateSubqueryCallbackBinding(
						[&Context](ISubqueryContext& ActorQueryContext, RowHandle ObjectRow, const FTypedElementUObjectColumn& Actor, const FTypedElementPackageReference& PackageReference)
						{
							Context.RunSubquery(EApplyOverlaysObjectToSCC, PackageReference.Row, CreateSubqueryCallbackBinding(
								[&ActorQueryContext, &ObjectRow, &Actor](ISubqueryContext& SubQueryContext)
								{
									FColor Color = DetermineOverlayColor(SubQueryContext, Actor, ActorQueryContext.HasColumn<FTypedElementSelectionColumn>());
									if (Color.Bits != 0)
									{
										ActorQueryContext.AddColumn<FTypedElementViewportOverlayColorColumn>(ObjectRow, { .OverlayColor = Color });
									}
								})
							);
						}
					));

					// Query:
					// For all actors WITH an overlay color column AND having a package reference:
					//   Re-check the color that should be applied based on SCC status tags
					//   If the color has changed, remove and re-add the OverlayColorColumn to the actor row
					//
					// Note: Remove and re-add will trigger observer in TypedElementActorViewportProcessors to SetOverlayColor on the primitive components
					Context.RunSubquery(EChangeOverlay, Update.ObjectRow, CreateSubqueryCallbackBinding(
						[&Context](ISubqueryContext& ActorQueryContext, RowHandle ObjectRow, const FTypedElementUObjectColumn& Actor, const FTypedElementPackageReference& PackageReference, const FTypedElementViewportOverlayColorColumn& OverlayColorColumn)
						{
							Context.RunSubquery(EApplyOverlaysObjectToSCC, PackageReference.Row, CreateSubqueryCallbackBinding(
								[&ActorQueryContext, &ObjectRow, &Actor, &OverlayColorColumn](ISubqueryContext& SubQueryContext)
								{
									FColor Color = DetermineOverlayColor(SubQueryContext, Actor, ActorQueryContext.HasColumn<FTypedElementSelectionColumn>());
									if (Color.Bits == 0)
									{
										ActorQueryContext.RemoveColumns<FTypedElementViewportOverlayColorColumn>(ObjectRow);
									}
									else if (Color != OverlayColorColumn.OverlayColor)
									{
										// Remove and re-add to trigger the observer
										ActorQueryContext.RemoveColumns<FTypedElementViewportOverlayColorColumn>(ObjectRow);
										ActorQueryContext.AddColumn<FTypedElementViewportOverlayColorColumn>(ObjectRow, { .OverlayColor = Color });
									}
								})
							);
						}
					));
					Context.RemoveRow(Row);
				})
			.DependsOn()
				.SubQuery(Subqueries)
			.Compile());
	}

	SelectionAdded = DataStorage.RegisterQuery(
					Select(
					TEXT("Update Overlay on Selection"),
					FObserver::OnAdd<FTypedElementSelectionColumn>(),
					[this](IQueryContext& Context, RowHandle RowHandle, const FTypedElementSelectionColumn& SelectionColumn)
					{
						// We only care about the level editor's selection set for now. When the selection column is made dynamic
						// we can directly query for it
						if(SelectionColumn.SelectionSet.IsNone())
						{
							// Since we know DetermineOverlayColor() removes the overlay when the row is selected we can directly
							// remove the column here to skip the need to do all the checks in there. But if that logic ever
							// changes we will have to update it here too
							Context.RemoveColumns<FTypedElementViewportOverlayColorColumn>(RowHandle);
						}
					})
					.Where()
						.All<FTypedElementActorTag, FTypedElementViewportOverlayColorColumn, FTypedElementPackageReference>()
					.Compile());
	
	SelectionRemoved = DataStorage.RegisterQuery(
					Select(
					TEXT("Update Overlay on Deselection"),
					FObserver::OnRemove<FTypedElementSelectionColumn>(),
					[this](IQueryContext& ActorQueryContext, RowHandle ObjectRow, const FTypedElementSelectionColumn& SelectionColumn,
						const FTypedElementPackageReference& PackageReference, const FTypedElementUObjectColumn& Actor)
					{
						// We only care about the level editor's selection set for now. When the selection column is made dynamic
						// we can directly query for it
						if(SelectionColumn.SelectionSet.IsNone())
						{
							// When an item is deselected, add the viewport overlay color column to it if applicable
							ActorQueryContext.RunSubquery(EApplyOverlaysObjectToSCC, PackageReference.Row, CreateSubqueryCallbackBinding(
								[&ActorQueryContext, &ObjectRow, &Actor](ISubqueryContext& SubQueryContext)
								{
									// We manually set the selection as false because the item is being deslected, but the row still has the selection column
									// when the observer is fired
									bool bSelected = false;
									FColor Color = DetermineOverlayColor(SubQueryContext, Actor, bSelected);
									if (Color.Bits != 0)
									{
										ActorQueryContext.AddColumn<FTypedElementViewportOverlayColorColumn>(ObjectRow, { .OverlayColor = Color });
									}
								})
							);
						}
					})
					.Where()
						.All<FTypedElementActorTag>()
					.DependsOn()
						.SubQuery(Subqueries)
					.Compile());
}

void URevisionControlDataStorageFactory::RegisterRemoveOverlays(IEditorDataStorageProvider& DataStorage)
{
	using namespace UE::Editor::DataStorage::Queries;
	
	if (RemoveOverlays == InvalidQueryHandle)
	{
		// Query:
		// For all actors WITH an overlay color column AND having a package reference:
		//   Remove the overlay color column
		//
		// This query is used to clean up the color columns if the overlay feature is disabled dynamically
		RemoveOverlays = DataStorage.RegisterQuery(
			Select(
				TEXT("Remove selection overlay colors"),
				// This is in PrePhysics because the overlay->actor query is in DuringPhysics and contexts don't flush changes between tick groups
				FProcessor(EQueryTickPhase::PrePhysics, DataStorage.GetQueryTickGroupName(EQueryTickGroups::SyncExternalToDataStorage))
					.SetExecutionMode(EExecutionMode::GameThread),
				[](IQueryContext& Context, RowHandle ObjectRow, FTypedElementUObjectColumn& Actor, const FTypedElementViewportOverlayColorColumn& ViewportColor)
				{
					Context.RemoveColumns<FTypedElementViewportOverlayColorColumn>(ObjectRow);
				})
			.Where()
				.All<FTypedElementActorTag>()
			.Compile());
	}
}

void URevisionControlDataStorageFactory::RegisterGeneralQueries(IEditorDataStorageProvider& DataStorage)
{
	using namespace UE::Editor::DataStorage::Queries;

	// SCC Columns we want to update the overlay state for
	const TArray<const UScriptStruct*> Columns{ FSCCNotCurrentTag::StaticStruct(), FSCCExternallyLockedColumn::StaticStruct(),
		FSCCStatusColumn::StaticStruct(), FSCCLockedTag::StaticStruct() };

	for(const UScriptStruct* Column : Columns)
	{
		QueryHandle Query = DataStorage.RegisterQuery(
			Select()
				.ReadOnly<FTypedElementPackageReference>()
			.Where()
				.All(Column)
			.Compile());

		GeneralQueriesMap.Add(Column, Query);
	}

	if(FetchOverlayColors == InvalidQueryHandle)
	{
		FetchOverlayColors = DataStorage.RegisterQuery(
		Select()
			.ReadOnly<FTypedElementPackageReference>()
		.Where()
			.All<FTypedElementViewportOverlayColorColumn, FTypedElementActorTag>()
		.Compile());
	}
}

void URevisionControlDataStorageFactory::RequestPackageUpdates(IEditorDataStorageProvider* DataStorage, TArray<FTypedElementPackageUpdateColumn>& Packages) const
{
	UE::Editor::DataStorage::TableHandle Table = DataStorage->FindTable(FName("Editor_PackageUpdateTable"));

	for(FTypedElementPackageUpdateColumn& PackageUpdate : Packages)
	{
		UE::Editor::DataStorage::RowHandle UpdateRow = DataStorage->AddRow(Table);
		DataStorage->AddColumn(UpdateRow, MoveTemp(PackageUpdate));
	}
}
