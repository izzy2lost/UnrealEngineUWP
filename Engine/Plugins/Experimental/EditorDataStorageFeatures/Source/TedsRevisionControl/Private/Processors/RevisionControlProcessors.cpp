// Copyright Epic Games, Inc. All Rights Reserved.

#include "Processors/RevisionControlProcessors.h"

#include "ISourceControlModule.h"
#include "SourceControlFileStatusMonitor.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"

#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementPackageColumns.h"
#include "Elements/Columns/TypedElementRevisionControlColumns.h"
#include "Elements/Columns/TypedElementSelectionColumns.h"
#include "Elements/Columns/TypedElementViewportColumns.h"
#include "Elements/Common/TypedElementDataStorageLog.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Elements/Interfaces/TypedElementQueryStorageInterfaces.h"

extern FAutoConsoleVariableRef CVarAutoPopulateState;

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
	ECVF_Default);

static bool gEnableOverlayNotAtHeadRevision = true;
TAutoConsoleVariable<bool> CVarEnableOverlayNotAtHeadRevision(
	TEXT("RevisionControl.Overlays.NotAtHeadRevision.Enable"),
	gEnableOverlayNotAtHeadRevision,
	TEXT("Enables overlays for files that are not at the latest revision."),
	ECVF_Default);

static bool gEnableOverlayCheckedOut = false;
TAutoConsoleVariable<bool> CVarEnableOverlayCheckedOut(
	TEXT("RevisionControl.Overlays.CheckedOut.Enable"),
	gEnableOverlayCheckedOut,
	TEXT("Enables overlays for files that are checked out by user."),
	ECVF_Default);

static bool gEnableOverlayOpenForAdd = false;
TAutoConsoleVariable<bool> CVarEnableOverlayOpenForAdd(
	TEXT("RevisionControl.Overlays.OpenForAdd.Enable"),
	gEnableOverlayOpenForAdd,
	TEXT("Enables overlays for files that are newly added."),
	ECVF_Default);

static int32 gOverlayAlpha = 20; // [0..100]
TAutoConsoleVariable<int32> CVarOverlayAlpha(
	TEXT("RevisionControl.Overlays.Alpha"),
	gOverlayAlpha,
	TEXT("Configures overlay opacity."),
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

static FColor DetermineOverlayColor(const TypedElementDataStorage::IQueryContext& ObjectContext, const TypedElementDataStorage::ICommonQueryContext& SCCContext, const FTypedElementUObjectColumn& Actor)
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
	bool bSelected = ObjectContext.HasColumn<FTypedElementSelectionColumn>();
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

void URevisionControlDataStorageFactory::RegisterTables(ITypedElementDataStorageInterface& DataStorage)
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

void URevisionControlDataStorageFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
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
				FetchUpdates = TypedElementInvalidQueryHandle;
			}
		}
	);

	CVarEnableOverlays->AsVariable()->OnChangedDelegate().AddLambda(
		[this, &DataStorage](IConsoleVariable* EnableOverlays)
		{
			if (EnableOverlays->GetBool())
			{
				DataStorage.UnregisterQuery(RemoveOverlays);
				RemoveOverlays = TypedElementInvalidQueryHandle;

				RegisterApplyOverlays(DataStorage);
			}
			else
			{
				DataStorage.UnregisterQuery(ApplyNewOverlays);
				ApplyNewOverlays = TypedElementInvalidQueryHandle;
				DataStorage.UnregisterQuery(ChangeOverlay);
				ChangeOverlay = TypedElementInvalidQueryHandle;

				DataStorage.UnregisterQuery(ApplyOverlaysObjectToSCC);
				ApplyOverlaysObjectToSCC = TypedElementInvalidQueryHandle;

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
}

void URevisionControlDataStorageFactory::RegisterFetchUpdates(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;
	
	FSourceControlFileStatusMonitor& FileStatusMonitor = ISourceControlModule::Get().GetSourceControlFileStatusMonitor();

	if (FetchUpdates == TypedElementDataStorage::InvalidQueryHandle)
	{
		FetchUpdates = DataStorage.RegisterQuery(
			Select(
				TEXT("Gather source control statuses for objects with unresolved package paths"),
				FObserver::OnAdd<FTypedElementPackageUnresolvedReference>()
				.ForceToGameThread(true),
				[this, &FileStatusMonitor](DSI::IQueryContext& Context, const FTypedElementPackageUnresolvedReference& UnresolvedReference)
				{
					static FSourceControlFileStatusMonitor::FOnSourceControlFileStatus EmptyDelegate{};
				
					FileStatusMonitor.StartMonitoringFile(
						reinterpret_cast<uintptr_t>(this),
						UnresolvedReference.PathOnDisk,
						EmptyDelegate
						
					);
				}
			)
			.Compile()
		);
	}
}

void URevisionControlDataStorageFactory::RegisterApplyOverlays(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace TypedElementDataStorage;
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

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
			.Compile()
		);
	}

	if (ChangeOverlay == InvalidQueryHandle)
	{
		ChangeOverlay = DataStorage.RegisterQuery(
			Select()
			.ReadOnly<FTypedElementUObjectColumn, FTypedElementPackageReference, FTypedElementViewportOverlayColorColumn>()
			.Where()
				.All<FTypedElementActorTag>()
			.Compile()
		);
	}

	if (FlushPackageUpdates == InvalidQueryHandle)
	{
		check(ApplyOverlaysObjectToSCC != InvalidQueryHandle && ApplyNewOverlays != InvalidQueryHandle && ChangeOverlay!= InvalidQueryHandle);
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
		
		FlushPackageUpdates = DataStorage.RegisterQuery(
			Select(
				TEXT("Consume collected package updates"),
				FProcessor(DSI::EQueryTickPhase::PrePhysics, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::Update))
				.ForceToGameThread(true),
				[](DSI::IQueryContext& Context, RowHandle Row, const FTypedElementPackageUpdateColumn& Update)
				{
					// Query:
					// For all actors without an overlay color column AND having a package reference:
					//   Determine if a color should be applied based on SCC status tags
					//   If so, add the OverlayColorColumn to the actor row
					Context.RunSubquery(EApplyNewOverlays, Update.ObjectRow, CreateSubqueryCallbackBinding(
						[&Context](RowHandle ObjectRow, const FTypedElementUObjectColumn& Actor, const FTypedElementPackageReference& PackageReference)
						{
							Context.RunSubquery(EApplyOverlaysObjectToSCC, PackageReference.Row, CreateSubqueryCallbackBinding(
								[&Context, &ObjectRow, &Actor](DSI::ISubqueryContext& SubQueryContext)
								{
									FColor Color = DetermineOverlayColor(Context, SubQueryContext, Actor);
									if (Color.Bits != 0)
									{
										Context.AddColumn<FTypedElementViewportOverlayColorColumn>(ObjectRow, { .OverlayColor = Color });
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
						[&Context](TypedElementRowHandle ObjectRow, const FTypedElementUObjectColumn& Actor, const FTypedElementPackageReference& PackageReference, const FTypedElementViewportOverlayColorColumn& OverlayColorColumn)
						{
							Context.RunSubquery(EApplyOverlaysObjectToSCC, PackageReference.Row, CreateSubqueryCallbackBinding(
								[&Context, &ObjectRow, &Actor, &OverlayColorColumn](DSI::ISubqueryContext& SubQueryContext)
								{
									FColor Color = DetermineOverlayColor(Context, SubQueryContext, Actor);
									if (Color.Bits == 0)
									{
										Context.RemoveColumns<FTypedElementViewportOverlayColorColumn>(ObjectRow);
									}
									else if (Color != OverlayColorColumn.OverlayColor)
									{
										// Remove and re-add to trigger the observer
										Context.RemoveColumns<FTypedElementViewportOverlayColorColumn>(ObjectRow);
										Context.AddColumn<FTypedElementViewportOverlayColorColumn>(ObjectRow, { .OverlayColor = Color });
									}
								})
							);
						}
					));
					Context.RemoveRow(Row);
				}
			)
			.DependsOn()
				.SubQuery(Subqueries)
			.Compile()
		);
	}
}

void URevisionControlDataStorageFactory::RegisterRemoveOverlays(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	if (RemoveOverlays == TypedElementInvalidQueryHandle)
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
				FProcessor(DSI::EQueryTickPhase::PrePhysics, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncExternalToDataStorage))
				.ForceToGameThread(true),
				[](DSI::IQueryContext& Context, TypedElementRowHandle ObjectRow, FTypedElementUObjectColumn& Actor, const FTypedElementViewportOverlayColorColumn& ViewportColor)
				{
					Context.RemoveColumns<FTypedElementViewportOverlayColorColumn>(ObjectRow);
				}
			)
			.Where()
				.All<FTypedElementActorTag>()
			.Compile()
		);
	}
}