// Copyright Epic Games, Inc. All Rights Reserved.

#include "RevisionControlProcessors.h"

#include "ISourceControlModule.h"
#include "SourceControlFileStatusMonitor.h"
#include "HAL/IConsoleManager.h"

#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementPackageColumns.h"
#include "Elements/Columns/TypedElementRevisionControlColumns.h"
#include "Elements/Columns/TypedElementSelectionColumns.h"
#include "Elements/Columns/TypedElementViewportColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Framework/TypedElementRegistry.h"

#include "MassActorSubsystem.h"

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

static int32 gOverlayAlpha = 217;
TAutoConsoleVariable<int32> CVarOverlayAlpha(
	TEXT("SourceControl.Overlays.Alpha"),
	gOverlayAlpha,
	TEXT("Configures overlay opacity."),
	ECVF_Default);

static FColor DetermineOverlayColor(const TypedElementDataStorage::IQueryContext& ObjectContext, const TypedElementDataStorage::ICommonQueryContext& SCCContext)
{
	check(IsInGameThread());

	bool bSelected = ObjectContext.HasColumn<FTypedElementSelectionColumn>();
	if (!bSelected)
	{
		// Check if the package is outdated because there is a newer version available.
		if (SCCContext.HasColumn<FSCCNotCurrentTag>())
		{
			if (CVarEnableOverlayNotAtHeadRevision.GetValueOnGameThread())
			{
				// Yellow.
				return FColor(255, 255, 61, CVarOverlayAlpha.GetValueOnGameThread());
			}
		}

		// Check if the package is locked by someone else.
		if (SCCContext.HasColumn<FSCCExternallyLockedColumn>())
		{
			if (CVarEnableOverlayCheckedOutByOtherUser.GetValueOnGameThread())
			{
				// Red.
				return FColor(255, 64, 64, CVarOverlayAlpha.GetValueOnGameThread());
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
						// Green.
						return FColor(134, 194, 74, CVarOverlayAlpha.GetValueOnGameThread());
					}
				}
			}
		}

		// Check if the package is locked by self.
		if (SCCContext.HasColumn<FSCCLockedTag>())
		{
			if (CVarEnableOverlayCheckedOut.GetValueOnGameThread())
			{
				// Blue.
				return FColor(0, 112, 224, CVarOverlayAlpha.GetValueOnGameThread());
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
			}
		}
	);

	CVarEnableOverlays->AsVariable()->OnChangedDelegate().AddLambda(
		[this, &DataStorage](IConsoleVariable* EnableOverlays)
		{
			if (EnableOverlays->GetBool())
			{
				RegisterApplyOverlays(DataStorage);
			}
			else
			{
				DataStorage.UnregisterQuery(ApplyOverlays);
				DataStorage.UnregisterQuery(ApplyOverlaysObjectToSCC);
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
}

void UTypedElementRevisionControlFactory::RegisterFetchUpdates(ITypedElementDataStorageInterface& DataStorage) const
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;
	
	FSourceControlFileStatusMonitor& FileStatusMonitor = ISourceControlModule::Get().GetSourceControlFileStatusMonitor();

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

void UTypedElementRevisionControlFactory::RegisterApplyOverlays(ITypedElementDataStorageInterface& DataStorage) const
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	ApplyOverlaysObjectToSCC = DataStorage.RegisterQuery(
		Select()
			.ReadOnly<FTypedElementPackagePathColumn>()
			.ReadOnly<FSCCStatusColumn, FSCCExternallyLockedColumn>(EOptional::Yes)
		.Compile());

	ApplyOverlays = DataStorage.RegisterQuery(
		Select(
			TEXT("Change selection outline colors based on SCC status"),
			// This is in PrePhysics because the outline->actor query is in DuringPhysics and contexts don't flush changes between tick groups
			FProcessor(DSI::EQueryTickPhase::PrePhysics, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncExternalToDataStorage))
				.ForceToGameThread(true),
			[](DSI::IQueryContext& Context, TypedElementRowHandle ObjectRow, const FTypedElementPackageReference& PackageReference)
			{
				Context.RunSubquery(0, PackageReference.Row, CreateSubqueryCallbackBinding(
					[&Context, &ObjectRow](DSI::ISubqueryContext& SubQueryContext)
					{
						FColor Color = DetermineOverlayColor(Context, SubQueryContext);
						if (Color.Bits != 0)
						{
							Context.AddColumn<FTypedElementViewportOverlayColorColumn>(ObjectRow, { .OverlayColor = Color });
						}
						else
						{
							Context.RemoveColumns<FTypedElementViewportOverlayColorColumn>(ObjectRow);
						}
						Context.AddColumns<FTypedElementSyncBackToWorldTag>(ObjectRow);
					})
				);
			}
		)
		.DependsOn()
			.SubQuery(ApplyOverlaysObjectToSCC)
		.Compile()
	);
}