// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Future.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "Misc/Optional.h"
#include "Templates/UnrealTemplate.h"

struct FConcertClientInfo;
class IConcertSyncClient;
class UMultiUserReplicationSessionPreset;
enum class EMultiUserClientPresetLoadMode : uint8;

namespace UE::ConcertSyncClient::Replication { struct FRemoteEditEvent; }

namespace UE::MultiUserClient
{
	class FReplicationClientManager;

	enum class EReplaceSessionContentErrorCode : uint8
	{
		/** Request completed successfully. */
		Success,
		
		/** Request cancelled because FPresetManager was destroyed - probably because the use left the session during the request. */
		Cancelled,
		/** Another locally initiated operation is already in progress. */
		InProgress,
		
		/** Request timed out */
		Timeout,
		/** The feature is not enabled (i.e. EConcertSyncSessionFlags::ShouldEnableRemoteEditing or EConcertSyncSessionFlags::ShouldAllowGlobalMuting were not set on the server).  */
		FeatureDisabled,
		/** Server rejected the change because it was not valid */
		Rejected
	};
	
	/** Result of FPresetManager::ReplaceSessionContentWithPreset. */
	struct FReplaceSessionContentResult
	{
		EReplaceSessionContentErrorCode ErrorCode;

		FReplaceSessionContentResult(EReplaceSessionContentErrorCode ErrorCode = EReplaceSessionContentErrorCode::Success)
			: ErrorCode(ErrorCode)
		{}

		bool IsSuccess() const { return ErrorCode == EReplaceSessionContentErrorCode::Success; }
	};

	enum class EApplyPresetFlags : uint8
	{
		None,
		/** If set clients that were not in the session when the preset was created will get their content reset, too. */
		ClearUnreferencedClients = 1 << 0
	};
	ENUM_CLASS_FLAGS(EApplyPresetFlags);

	enum class EFilterResult : uint8 { Include, Exclude };
	DECLARE_DELEGATE_RetVal_OneParam(EFilterResult, FFilterClientForPreset, const FConcertClientInfo&)
	/** Optionsl for saving a preset */
	struct FSavePresetOptions
	{
		/** Filter that decides whether a client should be included in the preset. */
		FFilterClientForPreset ClientFilterDelegate; 
	};

	enum class ECanSaveResult : uint8
	{
		/** Yes, a preset can be saved. */
		Yes,
		/** There are no clients to save for */
		NoClients
	};
	
	/**
	 * Implements all logic for managing presets in the MU session: saving and loading presets.
	 * The UI directly interfaces with this class.
	 */
	class FPresetManager : public FNoncopyable
	{
	public:
		
		FPresetManager(const IConcertSyncClient& SyncClient UE_LIFETIMEBOUND, const FReplicationClientManager& ClientManager UE_LIFETIMEBOUND);
		~FPresetManager();

		/** @return Whether any preset is currently being applied. */
		bool IsPresetChangeInProgress() const { return InProgressSessionReplacementOp.IsValid(); }
		
		/** Applies Preset to all clients in the session. */
		TFuture<FReplaceSessionContentResult> ReplaceSessionContentWithPreset(const UMultiUserReplicationSessionPreset& Preset, EApplyPresetFlags Flags = EApplyPresetFlags::None);

		/** @return Whether a preset can be saved (i.e. at least one client is included). */
		ECanSaveResult CanSavePreset(const FSavePresetOptions& Options = {}) const;
		/**
		 * Exports the current session content to a preset, asks the user where to save it, then saves it.
		 * @return A future that finishes when saving has completed.
		 */
		UMultiUserReplicationSessionPreset* ExportToPresetAndSaveAs(const FSavePresetOptions& Options = {});

	private:

		/** Used to get display information of clients in the session. */
		const IConcertSyncClient& SyncClient;
		/** Used to get the clients' replication content. */
		const FReplicationClientManager& ClientManager;

		/** Non-null for as long as the ReplaceSessionContentWithPreset network request takes. */
		TSharedPtr<TPromise<FReplaceSessionContentResult>> InProgressSessionReplacementOp;

		/** Exports the current session content to a preset. */
		UMultiUserReplicationSessionPreset* ExportToPreset(const FSavePresetOptions& Options) const;

		/** Called when the local client receives a remote edit. */
		void OnPostRemoteEditApplied(const ConcertSyncClient::Replication::FRemoteEditEvent&) const;
	};
}
