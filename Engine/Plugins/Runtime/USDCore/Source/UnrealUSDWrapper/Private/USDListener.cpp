// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDListener.h"

#include "USDLog.h"
#include "USDMemory.h"

#include "UsdWrappers/ForwardDeclarations.h"
#include "UsdWrappers/SdfLayer.h"
#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdStage.h"
#include "UsdWrappers/VtValue.h"

#if USE_USD_SDK

#include "USDIncludesStart.h"
#include "pxr/base/tf/weakBase.h"
#include "pxr/usd/sdf/changeList.h"
#include "pxr/usd/sdf/notice.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/usd/notice.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usdGeom/tokens.h"
#include "USDIncludesEnd.h"

#endif	  // USE_USD_SDK

namespace UsdToUnreal
{
#if USE_USD_SDK
	void ConvertSdfChangeListEntry(
		const pxr::SdfPath& ChangedObject,
		const pxr::SdfChangeList::Entry& Entry,
		UsdUtils::FSdfChangeListEntry& OutConverted
	)
	{
		using namespace pxr;

		// For most changes we'll only get one of these, but sometimes multiple changes are fired in sequence
		// (e.g. if you change framesPerSecond, it will send a notice for it but also for the matching, updated timeCodesPerSecond)
		for (const std::pair<TfToken, std::pair<VtValue, VtValue>>& AttributeChange : Entry.infoChanged)	// Note: infoChanged here is just a naming
																											// conflict, there's no "resyncChanged"
		{
			UsdUtils::FAttributeChange& ConvertedAttributeChange = OutConverted.AttributeChanges.Emplace_GetRef();

			const FString FieldToken = ANSI_TO_TCHAR(AttributeChange.first.GetString().c_str());

			// For regular properties (most common case) FullFieldPath will already carry the property and FieldToken will be "default",
			// "timeSamples", "variability", etc.
			if (ChangedObject.IsPropertyPath())
			{
				ConvertedAttributeChange.PropertyName = ANSI_TO_TCHAR(ChangedObject.GetName().c_str());
				ConvertedAttributeChange.Field = FieldToken;
			}
			// For stage properties it seems like USD likes to send just "/" as the FullFieldPath, and the actual property name in FieldToken (e.g.
			// "metersPerUnit", "timeCodesPerSecond", etc.) We send "default" here for consistency
			else if (ChangedObject.IsAbsoluteRootPath() || FieldToken == FString(TEXT("kind")))
			{
				ConvertedAttributeChange.PropertyName = FieldToken;
				ConvertedAttributeChange.Field = TEXT("default");
			}

			ConvertedAttributeChange.OldValue = UE::FVtValue{AttributeChange.second.first};
			ConvertedAttributeChange.NewValue = UE::FVtValue{AttributeChange.second.second};
		}

		// Some notices (like creating/removing a property) don't have any actual infoChanged entries, so we need to create a fake one in here in
		// order to convey the PropertyName, if applicable
		if (Entry.infoChanged.size() == 0
			&& (Entry.flags.didAddProperty || Entry.flags.didAddPropertyWithOnlyRequiredFields || Entry.flags.didRemoveProperty
				|| Entry.flags.didRemovePropertyWithOnlyRequiredFields || Entry.flags.didChangeAttributeTimeSamples))
		{
			UsdUtils::FAttributeChange& ConvertedAttributeChange = OutConverted.AttributeChanges.Emplace_GetRef();
			ConvertedAttributeChange.Field = Entry.flags.didChangeAttributeTimeSamples ? TEXT("timeSamples") : TEXT("default");

			if (ChangedObject.IsPropertyPath())
			{
				ConvertedAttributeChange.PropertyName = ANSI_TO_TCHAR(ChangedObject.GetName().c_str());
			}
		}

		// These should be packed just the same, but just in case we do member by member here instead of memcopying it over
		UsdUtils::FPrimChangeFlags& ConvertedFlags = OutConverted.Flags;
		ConvertedFlags.bDidChangeIdentifier = Entry.flags.didChangeIdentifier;
		ConvertedFlags.bDidChangeResolvedPath = Entry.flags.didChangeResolvedPath;
		ConvertedFlags.bDidReplaceContent = Entry.flags.didReplaceContent;
		ConvertedFlags.bDidReloadContent = Entry.flags.didReloadContent;
		ConvertedFlags.bDidReorderChildren = Entry.flags.didReorderChildren;
		ConvertedFlags.bDidReorderProperties = Entry.flags.didReorderProperties;
		ConvertedFlags.bDidRename = Entry.flags.didRename;
		ConvertedFlags.bDidChangePrimVariantSets = Entry.flags.didChangePrimVariantSets;
		ConvertedFlags.bDidChangePrimInheritPaths = Entry.flags.didChangePrimInheritPaths;
		ConvertedFlags.bDidChangePrimSpecializes = Entry.flags.didChangePrimSpecializes;
		ConvertedFlags.bDidChangePrimReferences = Entry.flags.didChangePrimReferences;
		ConvertedFlags.bDidChangeAttributeTimeSamples = Entry.flags.didChangeAttributeTimeSamples;
		ConvertedFlags.bDidChangeAttributeConnection = Entry.flags.didChangeAttributeConnection;
		ConvertedFlags.bDidChangeRelationshipTargets = Entry.flags.didChangeRelationshipTargets;
		ConvertedFlags.bDidAddTarget = Entry.flags.didAddTarget;
		ConvertedFlags.bDidRemoveTarget = Entry.flags.didRemoveTarget;
		ConvertedFlags.bDidAddInertPrim = Entry.flags.didAddInertPrim;
		ConvertedFlags.bDidAddNonInertPrim = Entry.flags.didAddNonInertPrim;
		ConvertedFlags.bDidRemoveInertPrim = Entry.flags.didRemoveInertPrim;
		ConvertedFlags.bDidRemoveNonInertPrim = Entry.flags.didRemoveNonInertPrim;
		ConvertedFlags.bDidAddPropertyWithOnlyRequiredFields = Entry.flags.didAddPropertyWithOnlyRequiredFields;
		ConvertedFlags.bDidAddProperty = Entry.flags.didAddProperty;
		ConvertedFlags.bDidRemovePropertyWithOnlyRequiredFields = Entry.flags.didRemovePropertyWithOnlyRequiredFields;
		ConvertedFlags.bDidRemoveProperty = Entry.flags.didRemoveProperty;

		static_assert(
			static_cast<int>(UsdUtils::ESubLayerChangeType::SubLayerAdded) == static_cast<int>(pxr::SdfChangeList::SubLayerChangeType::SubLayerAdded),
			"Enum values changed!"
		);
		static_assert(
			static_cast<int>(UsdUtils::ESubLayerChangeType::SubLayerOffset)
				== static_cast<int>(pxr::SdfChangeList::SubLayerChangeType::SubLayerOffset),
			"Enum values changed!"
		);
		static_assert(
			static_cast<int>(UsdUtils::ESubLayerChangeType::SubLayerRemoved)
				== static_cast<int>(pxr::SdfChangeList::SubLayerChangeType::SubLayerRemoved),
			"Enum values changed!"
		);
		for (const std::pair<std::string, pxr::SdfChangeList::SubLayerChangeType>& SubLayerChange : Entry.subLayerChanges)
		{
			OutConverted.SubLayerChanges.Add(TPair<FString, UsdUtils::ESubLayerChangeType>(
				ANSI_TO_TCHAR(SubLayerChange.first.c_str()),
				static_cast<UsdUtils::ESubLayerChangeType>(SubLayerChange.second)
			));
		}

		OutConverted.OldPath = ANSI_TO_TCHAR(Entry.oldPath.GetString().c_str());
		OutConverted.OldIdentifier = ANSI_TO_TCHAR(Entry.oldIdentifier.c_str());
	}

	bool ConvertPathRange(const pxr::UsdNotice::ObjectsChanged::PathRange& PathRange, UsdUtils::FObjectChangesByPath& OutChanges)
	{
		using namespace pxr;

		FScopedUsdAllocs UsdAllocs;

		OutChanges.Reset();

		for (UsdNotice::ObjectsChanged::PathRange::const_iterator It = PathRange.begin(); It != PathRange.end(); ++It)
		{
			const FString PrimPath = ANSI_TO_TCHAR(It->GetAbsoluteRootOrPrimPath().GetAsString().c_str());

			if (pxr::UsdPrim::IsPathInPrototype(UE::FSdfPath(*PrimPath)))
			{
				continue;
			}

			// Something like "/Root/Prim.some_field", or "/"
			const FString FullFieldPath = ANSI_TO_TCHAR(It->GetAsString().c_str());

			TArray<UsdUtils::FSdfChangeListEntry>& ConvertedChanges = OutChanges.FindOrAdd(PrimPath);

			// Changes may be empty, but we should still pass along this overall notice because sending a root
			// resync notice with no actual change item inside is how USD signals that a layer has been added/removed/resynced
			const std::vector<const SdfChangeList::Entry*>& Changes = It.base()->second;
			for (const SdfChangeList::Entry* Entry : Changes)
			{
				UsdUtils::FSdfChangeListEntry& ConvertedEntry = ConvertedChanges.Emplace_GetRef();
				if (Entry)
				{
					ConvertSdfChangeListEntry(*It, *Entry, ConvertedEntry);
				}
			}
		}

		return true;
	}

	bool ConvertObjectsChangedNotice(
		const pxr::UsdNotice::ObjectsChanged& InNotice,
		UsdUtils::FObjectChangesByPath& OutInfoChanges,
		UsdUtils::FObjectChangesByPath& OutResyncChanges
	)
	{
		ConvertPathRange(InNotice.GetChangedInfoOnlyPaths(), OutInfoChanges);

		// If we have a root path reload, just stop right here: We will have to reload everything anyway.
		// This is handy because otherwise on full reloads USD will emit an info change with bDidReloadContent=true
		// for most prims on the stage (this could be e.g. tens of thousands of entries, which our downstream code
		// would uselessly process, sort, serialize, etc.).
		const static FString RootPath = UE::FSdfPath::AbsoluteRootPath().GetString();
		if (TArray<UsdUtils::FSdfChangeListEntry>* RootInfoChange = OutInfoChanges.Find(RootPath))
		{
			for (const UsdUtils::FSdfChangeListEntry& Entry : *RootInfoChange)
			{
				if (Entry.Flags.bDidReloadContent)
				{
					OutInfoChanges = {};
					OutResyncChanges = {};
					OutResyncChanges.Add(RootPath, *RootInfoChange);
					return true;
				}
			}
		}

		ConvertPathRange(InNotice.GetResyncedPaths(), OutResyncChanges);

		// Upgrade targetted reload notices into resyncs (this should now only happen when reloading a reference/payload, as reloading
		// any layer on the local layer stack emits a change for the root path)
		bool bReloadedContent = false;
		for (TPair<FString, TArray<UsdUtils::FSdfChangeListEntry>>& InfoPair : OutInfoChanges)
		{
			const FString& PrimPath = InfoPair.Key;
			TArray<UsdUtils::FSdfChangeListEntry>* AnalogueResyncChanges = nullptr;

			for (TArray<UsdUtils::FSdfChangeListEntry>::TIterator ChangeIt = InfoPair.Value.CreateIterator(); ChangeIt; ++ChangeIt)
			{
				bool bUpgrade = false;

				// Upgrade info changes with content reloads into resync changes
				if (ChangeIt->Flags.bDidReloadContent)
				{
					bUpgrade = true;
					bReloadedContent = true;
				}

				// Upgrade visibility changes to resyncs because in case of mesh collapsing having one of the collapsed meshes go visible/invisible
				// should cause the regeneration of the collapsed asset. This is a bit expensive, but the asset cache will be used so its not as if
				// the mesh will be completely regenerated however
				if (!bUpgrade)
				{
					for (UsdUtils::FAttributeChange& Change : ChangeIt->AttributeChanges)
					{
						if (Change.PropertyName == ANSI_TO_TCHAR(pxr::UsdGeomTokens->visibility.GetString().c_str()))
						{
							bUpgrade = true;
							break;
						}
					}
				}

				if (bUpgrade)
				{
					if (!AnalogueResyncChanges)
					{
						AnalogueResyncChanges = &OutResyncChanges.FindOrAdd(PrimPath);
					}

					AnalogueResyncChanges->Add(*ChangeIt);
					ChangeIt.RemoveCurrent();
				}
			}
		}

		// For now, dump info changes when handling the notices about reloading layers.
		//
		// When we reload any layer, USD will emit some notices about attributes/prims that changed, and also generic notices about those prims
		// and their ancestors having reloaded their content (via the bDidReloadContent flag).
		// We'll upgrade those latter notices to resyncs (above), so that we regenerate the assets/components for those prims, but we cannot use the
		// former info changes about attributes/prims that changed (and how they changed) at all just yet, because that doesn't carry with them the
		// respective edit target information.
		//
		// For example imagine that a prim is only authored in a sublayer/referenced layer, is modified on disk, and the stage is reloaded.
		// We'll get the notice about what modification took place, but the stage's edit target will be the root layer. If we were to track those
		// changes, whenever we undid them we'd author the old values of the prim directly on the stage's root layer, which is definitely not what
		// we want.
		//
		// TODO: Remove this and use this information in order to have a "selective resync", updating only the changed prims/attributes's
		// assets/components. It *could* be possible, but it will be complex as it will involve deducing the right edit target via composition arcs,
		// and whether new attribute/prim specs where created within this transaction, etc.
		if (bReloadedContent)
		{
			OutInfoChanges = {};
		}

		return true;
	}
#endif	  // USE_USD_SDK
}	 // namespace UsdToUnreal

class FUsdListenerImpl
#if USE_USD_SDK
	: public pxr::TfWeakBase
#endif	  // #if USE_USD_SDK
{
public:
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FUsdListenerImpl() = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	virtual ~FUsdListenerImpl();

	FUsdListener::FOnStageEditTargetChanged OnStageEditTargetChanged;
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FUsdListener::FOnLayersChanged OnLayersChanged;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	FUsdListener::FOnSdfLayersChanged OnSdfLayersChanged;
	FUsdListener::FOnSdfLayerDirtinessChanged OnSdfLayerDirtinessChanged;
	FUsdListener::FOnObjectsChanged OnObjectsChanged;

	FThreadSafeCounter IsBlocked;

#if USE_USD_SDK
	FUsdListenerImpl(const pxr::UsdStageRefPtr& Stage);

	void Register(const pxr::UsdStageRefPtr& Stage);

protected:
	void HandleObjectsChangedNotice(const pxr::UsdNotice::ObjectsChanged& Notice, const pxr::UsdStageWeakPtr& Sender);
	void HandleStageEditTargetChangedNotice(const pxr::UsdNotice::StageEditTargetChanged& Notice, const pxr::UsdStageWeakPtr& Sender);
	void HandleLayersChangedNotice(const pxr::SdfNotice::LayersDidChange& Notice);
	void HandleLayerDirtinessChangedNotice(const pxr::SdfNotice::LayerDirtinessChanged& Notice);

private:
	pxr::TfNotice::Key RegisteredObjectsChangedKey;
	pxr::TfNotice::Key RegisteredStageEditTargetChangedKey;
	pxr::TfNotice::Key RegisteredSdfLayersChangedKey;
	pxr::TfNotice::Key RegisteredSdfLayerDirtinessChangedKey;
#endif	  // #if USE_USD_SDK
};

FUsdListener::FUsdListener()
	: Impl(MakeUnique<FUsdListenerImpl>())
{
}

FUsdListener::FUsdListener(const UE::FUsdStage& Stage)
	: FUsdListener()
{
	Register(Stage);
}

FUsdListener::~FUsdListener() = default;

void FUsdListener::Register(const UE::FUsdStage& Stage)
{
#if USE_USD_SDK
	Impl->Register(Stage);
#endif	  // #if USE_USD_SDK
}

void FUsdListener::Block()
{
	Impl->IsBlocked.Increment();
}

void FUsdListener::Unblock()
{
	Impl->IsBlocked.Decrement();
}

bool FUsdListener::IsBlocked() const
{
	return Impl->IsBlocked.GetValue() > 0;
}

FUsdListener::FOnStageEditTargetChanged& FUsdListener::GetOnStageEditTargetChanged()
{
	return Impl->OnStageEditTargetChanged;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FUsdListener::FOnLayersChanged& FUsdListener::GetOnLayersChanged()
{
	return Impl->OnLayersChanged;
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

FUsdListener::FOnSdfLayersChanged& FUsdListener::GetOnSdfLayersChanged()
{
	return Impl->OnSdfLayersChanged;
}

FUsdListener::FOnSdfLayerDirtinessChanged& FUsdListener::GetOnSdfLayerDirtinessChanged()
{
	return Impl->OnSdfLayerDirtinessChanged;
}

FUsdListener::FOnObjectsChanged& FUsdListener::GetOnObjectsChanged()
{
	return Impl->OnObjectsChanged;
}

#if USE_USD_SDK
void FUsdListenerImpl::Register(const pxr::UsdStageRefPtr& Stage)
{
	FScopedUsdAllocs UsdAllocs;

	if (RegisteredObjectsChangedKey.IsValid())
	{
		pxr::TfNotice::Revoke(RegisteredObjectsChangedKey);
	}
	RegisteredObjectsChangedKey = pxr::TfNotice::Register(
		pxr::TfWeakPtr<FUsdListenerImpl>(this),
		&FUsdListenerImpl::HandleObjectsChangedNotice,
		Stage
	);

	if (RegisteredStageEditTargetChangedKey.IsValid())
	{
		pxr::TfNotice::Revoke(RegisteredStageEditTargetChangedKey);
	}
	RegisteredStageEditTargetChangedKey = pxr::TfNotice::Register(
		pxr::TfWeakPtr<FUsdListenerImpl>(this),
		&FUsdListenerImpl::HandleStageEditTargetChangedNotice,
		Stage
	);

	if (RegisteredSdfLayersChangedKey.IsValid())
	{
		pxr::TfNotice::Revoke(RegisteredSdfLayersChangedKey);
	}
	RegisteredSdfLayersChangedKey = pxr::TfNotice::Register(pxr::TfWeakPtr<FUsdListenerImpl>(this), &FUsdListenerImpl::HandleLayersChangedNotice);

	if (RegisteredSdfLayerDirtinessChangedKey.IsValid())
	{
		pxr::TfNotice::Revoke(RegisteredSdfLayerDirtinessChangedKey);
	}
	RegisteredSdfLayerDirtinessChangedKey = pxr::TfNotice::Register(
		pxr::TfWeakPtr<FUsdListenerImpl>(this),
		&FUsdListenerImpl::HandleLayerDirtinessChangedNotice
	);
}
#endif	  // #if USE_USD_SDK

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FUsdListenerImpl::~FUsdListenerImpl()
{
#if USE_USD_SDK
	FScopedUsdAllocs UsdAllocs;
	pxr::TfNotice::Revoke(RegisteredObjectsChangedKey);
	pxr::TfNotice::Revoke(RegisteredStageEditTargetChangedKey);
	pxr::TfNotice::Revoke(RegisteredSdfLayersChangedKey);
	pxr::TfNotice::Revoke(RegisteredSdfLayerDirtinessChangedKey);
#endif	  // #if USE_USD_SDK
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

#if USE_USD_SDK
void FUsdListenerImpl::HandleObjectsChangedNotice(const pxr::UsdNotice::ObjectsChanged& Notice, const pxr::UsdStageWeakPtr& Sender)
{
	if (!OnObjectsChanged.IsBound())
	{
		return;
	}

	if (IsBlocked.GetValue() > 0)
	{
		return;
	}

	UsdUtils::FObjectChangesByPath InfoChanges;
	UsdUtils::FObjectChangesByPath ResyncChanges;
	UsdToUnreal::ConvertObjectsChangedNotice(Notice, InfoChanges, ResyncChanges);
	if (InfoChanges.Num() > 0 || ResyncChanges.Num() > 0)
	{
		FScopedUnrealAllocs UnrealAllocs;
		OnObjectsChanged.Broadcast(InfoChanges, ResyncChanges);
	}
}

void FUsdListenerImpl::HandleStageEditTargetChangedNotice(const pxr::UsdNotice::StageEditTargetChanged& Notice, const pxr::UsdStageWeakPtr& Sender)
{
	FScopedUnrealAllocs UnrealAllocs;
	OnStageEditTargetChanged.Broadcast();
}

void FUsdListenerImpl::HandleLayersChangedNotice(const pxr::SdfNotice::LayersDidChange& Notice)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if ((!OnLayersChanged.IsBound() && !OnSdfLayersChanged.IsBound()) || IsBlocked.GetValue() > 0)
	{
		return;
	}

	TArray<FString> LayersNames;
	UsdUtils::FLayerToSdfChangeList ConvertedLayerToChangeList;
	{
		FScopedUnrealAllocs Allocs;

		const std::vector<std::pair<pxr::SdfLayerHandle, pxr::SdfChangeList>>& UsdChangeLists = Notice.GetChangeListVec();
		ConvertedLayerToChangeList.Reserve(UsdChangeLists.size());

		for (const std::pair<pxr::SdfLayerHandle, pxr::SdfChangeList>& ChangeVecItem : UsdChangeLists)
		{
			const pxr::SdfLayerHandle& Layer = ChangeVecItem.first;
			const pxr::SdfChangeList::EntryList& ChangeList = ChangeVecItem.second.GetEntryList();

			TPair<UE::FSdfLayerWeak, UsdUtils::FSdfChangeList>& ConvertedChangeList = ConvertedLayerToChangeList.Emplace_GetRef();
			ConvertedChangeList.Key = UE::FSdfLayerWeak{Layer};
			ConvertedChangeList.Value.Reserve(ChangeList.size());

			for (const std::pair<pxr::SdfPath, pxr::SdfChangeList::Entry>& Change : ChangeList)
			{
				TPair<UE::FSdfPath, UsdUtils::FSdfChangeListEntry>& ConvertedChange = ConvertedChangeList.Value.Emplace_GetRef();
				ConvertedChange.Key = UE::FSdfPath{Change.first};
				UsdToUnreal::ConvertSdfChangeListEntry(Change.first, Change.second, ConvertedChange.Value);

				const pxr::SdfChangeList::Entry::_Flags& Flags = Change.second.flags;
				if (Flags.didReloadContent)
				{
					LayersNames.Add(ANSI_TO_TCHAR(ChangeVecItem.first->GetIdentifier().c_str()));
				}
			}
		}
	}

	FScopedUnrealAllocs UnrealAllocs;
	OnLayersChanged.Broadcast(LayersNames);
	OnSdfLayersChanged.Broadcast(ConvertedLayerToChangeList);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FUsdListenerImpl::HandleLayerDirtinessChangedNotice(const pxr::SdfNotice::LayerDirtinessChanged& Notice)
{
	if (!OnSdfLayerDirtinessChanged.IsBound() || IsBlocked.GetValue() > 0)
	{
		return;
	}

	FScopedUnrealAllocs UnrealAllocs;

	OnSdfLayerDirtinessChanged.Broadcast();
}
#endif	  // #if USE_USD_SDK

FScopedBlockNotices::FScopedBlockNotices(FUsdListener& InListener)
	: Listener(InListener)
{
	Listener.Block();
}

FScopedBlockNotices::~FScopedBlockNotices()
{
	Listener.Unblock();
}
