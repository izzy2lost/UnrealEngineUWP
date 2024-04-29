// Copyright Epic Games, Inc. All Rights Reserved.

#include "RevisionControlProcessors.h"

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
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Framework/TypedElementRegistry.h"

extern TYPEDELEMENTSDATASTORAGE_API FAutoConsoleVariableRef CVarAutoPopulateState;

static bool gEnableOverlays = false;
TAutoConsoleVariable<bool> CVarEnableOverlays(
	TEXT("SourceControl.Overlays.Enable"),
	gEnableOverlays,
	TEXT("Enables overlays."),
	ECVF_Default);

static bool gEnableOverlayCheckedOutByOtherUser = true;
TAutoConsoleVariable<bool> CVarEnableOverlayCheckedOutByOtherUser(
	TEXT("SourceControl.Overlays.CheckedOutByOtherUser.Enable"),
	gEnableOverlayCheckedOutByOtherUser,
	TEXT("Enables overlays for files that are checked out by another user."),
	ECVF_Default);

static bool gEnableOverlayNotAtHeadRevision = true;
TAutoConsoleVariable<bool> CVarEnableOverlayNotAtHeadRevision(
	TEXT("SourceControl.Overlays.NotAtHeadRevision.Enable"),
	gEnableOverlayNotAtHeadRevision,
	TEXT("Enables overlays for files that are not at the latest revision."),
	ECVF_Default);

static bool gEnableOverlayCheckedOut = false;
TAutoConsoleVariable<bool> CVarEnableOverlayCheckedOut(
	TEXT("SourceControl.Overlays.CheckedOut.Enable"),
	gEnableOverlayCheckedOut,
	TEXT("Enables overlays for files that are checked out by user."),
	ECVF_Default);

static bool gEnableOverlayOpenForAdd = false;
TAutoConsoleVariable<bool> CVarEnableOverlayOpenForAdd(
	TEXT("SourceControl.Overlays.OpenForAdd.Enable"),
	gEnableOverlayOpenForAdd,
	TEXT("Enables overlays for files that are newly added."),
	ECVF_Default);

static int32 gOverlayAlpha = 20; // [0..100]
TAutoConsoleVariable<int32> CVarOverlayAlpha(
	TEXT("SourceControl.Overlays.Alpha"),
	gOverlayAlpha,
	TEXT("Configures overlay opacity."),
	ECVF_Default);

static FColor DetermineOverlayColor(const TypedElementDataStorage::IQueryContext& ObjectContext, const TypedElementDataStorage::ICommonQueryContext& SCCContext, const FTypedElementUObjectColumn& Actor)
{
	check(IsInGameThread());

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

void UTypedElementRevisionControlFactory::RegisterTables(ITypedElementDataStorageInterface& DataStorage)
{
	DataStorage.RegisterTable(
		TTypedElementColumnTypeList<
			FTypedElementPackagePathColumn, FTypedElementPackageLoadedPathColumn,
			FSCCRevisionIdColumn, FSCCExternalRevisionIdColumn>(),
		FName("Editor_RevisionControlTable"));
}

void UTypedElementRevisionControlFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage)
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
				DataStorage.UnregisterQuery(ApplyOverlays);
				ApplyOverlays = TypedElementInvalidQueryHandle;

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

void UTypedElementRevisionControlFactory::RegisterFetchUpdates(ITypedElementDataStorageInterface& DataStorage) const
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;
	
	FSourceControlFileStatusMonitor& FileStatusMonitor = ISourceControlModule::Get().GetSourceControlFileStatusMonitor();

	if (FetchUpdates == TypedElementInvalidQueryHandle)
	{
		FetchUpdates = DataStorage.RegisterQuery(
			Select(
				TEXT("Gather source control statuses for objects with unresolved package paths"),
				FProcessor(DSI::EQueryTickPhase::DuringPhysics, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncExternalToDataStorage))
					.ForceToGameThread(true),
				[this, &FileStatusMonitor](DSI::IQueryContext& Context, const FTypedElementPackageUnresolvedReference* InUnresolvedReferences)
				{
					TConstArrayView<TypedElementDataStorage::RowHandle> RowHandles = Context.GetRowHandles();
					TConstArrayView<FTypedElementPackageUnresolvedReference, int64> UnresolvedReferences { InUnresolvedReferences, Context.GetRowCount() };

					for (int64 UnresolvedReferenceIndex = 0; UnresolvedReferenceIndex < UnresolvedReferences.Num(); ++UnresolvedReferenceIndex)
					{
						const FTypedElementPackageUnresolvedReference& UnresolvedReference = UnresolvedReferences[UnresolvedReferenceIndex];
						if (UnresolvedReference.Index == 0)
						{
							Context.RemoveColumns<FTypedElementPackageUnresolvedReference>(RowHandles[UnresolvedReferenceIndex]);
							return;
						}
						static FSourceControlFileStatusMonitor::FOnSourceControlFileStatus EmptyDelegate{};
					
						FileStatusMonitor.StartMonitoringFile(
							reinterpret_cast<uintptr_t>(this),
							UnresolvedReference.PathOnDisk,
							EmptyDelegate
						);
					}
				}
			)
			.Compile()
		);
	}
}

void UTypedElementRevisionControlFactory::RegisterApplyOverlays(ITypedElementDataStorageInterface& DataStorage) const
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	if (ApplyOverlaysObjectToSCC == TypedElementInvalidQueryHandle)
	{
		ApplyOverlaysObjectToSCC = DataStorage.RegisterQuery(
			Select()
				.ReadOnly<FTypedElementPackagePathColumn>()
				.ReadOnly<FSCCStatusColumn, FSCCExternallyLockedColumn>(EOptional::Yes)
			.Compile());
		}

	if (ApplyOverlays == TypedElementInvalidQueryHandle)
	{
		ApplyOverlays = DataStorage.RegisterQuery(
			Select(
				TEXT("Change selection overlay colors based on SCC status"),
				// This is in PrePhysics because the overlay->actor query is in DuringPhysics and contexts don't flush changes between tick groups
				FProcessor(DSI::EQueryTickPhase::PrePhysics, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncExternalToDataStorage))
					.ForceToGameThread(true),
				[](DSI::IQueryContext& Context, TypedElementRowHandle ObjectRow, const FTypedElementUObjectColumn& Actor, const FTypedElementPackageReference& PackageReference)
				{
					Context.RemoveColumns<FTypedElementViewportOverlayColorColumn>(ObjectRow);
					Context.RunSubquery(0, PackageReference.Row, CreateSubqueryCallbackBinding(
						[&Context, &ObjectRow, &Actor](DSI::ISubqueryContext& SubQueryContext)
						{
							FColor Color = DetermineOverlayColor(Context, SubQueryContext, Actor);
							if (Color.Bits != 0)
							{
								Context.AddColumn<FTypedElementViewportOverlayColorColumn>(ObjectRow, { .OverlayColor = Color });
							}
						})
					);
					Context.AddColumns<FTypedElementSyncBackToWorldTag>(ObjectRow);
				}
			)
			.Where()
				.All<FTypedElementActorTag>()
			.DependsOn()
				.SubQuery(ApplyOverlaysObjectToSCC)
			.Compile()
		);
	}
}

void UTypedElementRevisionControlFactory::RegisterRemoveOverlays(ITypedElementDataStorageInterface& DataStorage) const
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	if (RemoveOverlays == TypedElementInvalidQueryHandle)
	{
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