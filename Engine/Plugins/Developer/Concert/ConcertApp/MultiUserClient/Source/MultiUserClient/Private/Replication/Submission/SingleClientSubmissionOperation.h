// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISubmissionOperation.h"
#include "Replication/Stream/IClientStreamSynchronizer.h"

#include "Templates/UnrealTemplate.h"

namespace UE::MultiUserClient
{
	/** Keeps a promise for each relevant event. Handles cancelling them when destroyed. */
	class FSingleClientSubmissionOperation
		: public ISubmissionOperation
		, public FNoncopyable
	{
	public:

		FSingleClientSubmissionOperation(bool bInModifiesStreams)
			: bModifiesStreams(bInModifiesStreams)
		{}
		
		virtual ~FSingleClientSubmissionOperation() override
		{
			if (!bStreamPromiseWasSet)
			{
				StreamChangesPromise.EmplaceValue(FSubmitStreamChangesResponse{ EStreamSubmissionErrorCode::Cancelled });
			}
			if (!bAuthorityRequestPromiseWasSet)
			{
				AuthorityChangeRequestPromise.EmplaceValue(FSubmitAuthorityChangesRequest{ EAuthoritySubmissionRequestErrorCode::Cancelled });
			}
			if (!bAuthorityResponsePromiseWasSet)
			{
				AuthorityChangeResponsePromise.EmplaceValue(FSubmitAuthorityChangesResponse{ EAuthoritySubmissionResponseErrorCode::Cancelled });
			}
			if (!bCompleteOperationPromiseWasSet)
			{
				CompleteOperationPromise.EmplaceValue(ESubmissionOperationCompletedCode::Cancelled);
			}
		}

		void EmplaceStreamPromise(FSubmitStreamChangesResponse Result)
		{
			bStreamPromiseWasSet = true;
			StreamChangesPromise.EmplaceValue(MoveTemp(Result));
		}
		void EmplaceAuthorityRequestPromise(FSubmitAuthorityChangesRequest Result)
		{
			bAuthorityRequestPromiseWasSet = true;
			AuthorityChangeRequestPromise.EmplaceValue(MoveTemp(Result));
		}
		void EmplaceAuthorityResponsePromise(FSubmitAuthorityChangesResponse Result)
		{
			bAuthorityResponsePromiseWasSet = true;
			AuthorityChangeResponsePromise.EmplaceValue(MoveTemp(Result));
		}
		void EmplaceCompleteOperationPromise(ESubmissionOperationCompletedCode Result)
		{
			bCompleteOperationPromiseWasSet = true;
			CompleteOperationPromise.EmplaceValue(Result);
		}

		bool HasSetStreamPromise() const { return bStreamPromiseWasSet; }
		bool HasSetAuthorityRequestPromise() const { return bAuthorityRequestPromiseWasSet; }
		bool HasSetAuthorityResponsePromise() const { return bAuthorityResponsePromiseWasSet; }
		bool HasSetCompleteOperationPromise() const { return bCompleteOperationPromiseWasSet; }
		
		//~ Begin ISubmissionOperation Interface
		virtual bool IsModifyingStreams() const override { return bModifiesStreams; }
		virtual TFuture<FSubmitStreamChangesResponse> OnCompleteStreamChangesFuture_AnyThread() override { return StreamChangesPromise.GetFuture();  }
		virtual TFuture<FSubmitAuthorityChangesRequest> OnRequestAuthorityChangeFuture_AnyThread() override { return AuthorityChangeRequestPromise.GetFuture(); }
		virtual TFuture<FSubmitAuthorityChangesResponse> OnCompleteAuthorityChangeFuture_AnyThread() override { return AuthorityChangeResponsePromise.GetFuture(); }
		virtual TFuture<ESubmissionOperationCompletedCode> OnCompletedOperation_AnyThread() override { return CompleteOperationPromise.GetFuture(); }
		//~ End ISubmissionOperation Interface

	private:

		const bool bModifiesStreams;

		bool bStreamPromiseWasSet = false;
		bool bAuthorityRequestPromiseWasSet = false;
		bool bAuthorityResponsePromiseWasSet = false;
		bool bCompleteOperationPromiseWasSet = false;
		
		// All fulfilled by the owning FSingleClientSubmissionWorkflow
		TPromise<FSubmitStreamChangesResponse> StreamChangesPromise;
		TPromise<FSubmitAuthorityChangesRequest> AuthorityChangeRequestPromise;
		TPromise<FSubmitAuthorityChangesResponse> AuthorityChangeResponsePromise;
		TPromise<ESubmissionOperationCompletedCode> CompleteOperationPromise;
	};
}
