// Copyright Epic Games, Inc. All Rights Reserved.

#include "SynchronizedRequestUtils.h"

#include "ConcertLogGlobal.h"
#include "Replication/Client/ReplicationClientManager.h"
#include "Replication/Submission/Data/AuthoritySubmission.h"
#include "Replication/Submission/Data/StreamSubmission.h"
#include "Replication/Submission/ISubmissionOperation.h"

#include "Algo/AllOf.h"
#include "Containers/UnrealString.h"

namespace UE::MultiUserClient
{
	namespace ParallelSubmission::Private
	{
		static void RemoveInvalidClientEntries(const FReplicationClientManager& ClientManager, TMap<FGuid, FSubmissionParams> ParallelOperations)
		{
			for (auto It = ParallelOperations.CreateIterator(); It; ++It)
			{
				const TPair<FGuid, FSubmissionParams>& Operation =*It;
				if (!ensure(ClientManager.FindClient(Operation.Key)))
				{
					It.RemoveCurrent();
				}
			}
		}
		
		static void LogStreamChangeError(const FGuid& ClientId, const FSubmitStreamChangesResponse& Response)
		{
			if (Response.ErrorCode == EStreamSubmissionErrorCode::Timeout)
			{
				UE_LOG(LogConcert, Warning, TEXT("Remote stream change to client %s timed out."), *ClientId.ToString());
			}
			else if (Response.SubmissionInfo && Response.SubmissionInfo->Response.IsFailure())
			{
				FStringOutputDevice OutputDevice;
				Response.SubmissionInfo->Response.LogErrors(OutputDevice);
				UE_LOG(LogConcert, Warning, TEXT("Remote stream change to client %s failed: %s"), *ClientId.ToString(), *OutputDevice);
			}
		}

		class FSyncOperation;

		/** Adapter for FSubmissionQueue. Handles submitting for one client and notifying FSyncOperation if completions. */
		class FDeferredSubmitter
			: public FNoncopyable
			, public IDeferredSubmitter
		{
			FSyncOperation& Owner;
			FReplicationClientManager& ClientManager;
			FSubmissionQueue& SubmissionQueue;
			const FGuid ClientId;
			const FSubmissionParams SubmissionParams;
			
		public:

			FDeferredSubmitter(
				FSyncOperation& InOwner,
				FReplicationClientManager& InClientManager,
				FSubmissionQueue& InSubmissionQueue,
				const FGuid& InClientId,
				FSubmissionParams&& InSubmissionParams
				)
				: Owner(InOwner)
				, ClientManager(InClientManager)
				, SubmissionQueue(InSubmissionQueue)
				, ClientId(InClientId)
				, SubmissionParams(MoveTemp(InSubmissionParams))
			{}

			virtual ~FDeferredSubmitter() override
			{
				checkf(IsInGameThread(), TEXT("You must destroy IParallelSubmissionOperation on the game thread"));
				if (FReplicationClient* Client = ClientManager.FindClient(ClientId))
				{
					Client->GetSubmissionQueue().Dequeue_GameThread(*this);
				}
			}

			void EnqueueOrSubmit()
			{
				SubmissionQueue.SubmitNowOrEnqueue_GameThread(*this);
			}
			
			const FGuid& GetClientId() const { return ClientId; }

			virtual void PerformSubmission_GameThread(ISubmissionWorkflow& Workflow) override;
		};

		/** Starts all submissions and synchronizes their concurrent completion. */
		class FSyncOperation : public IParallelSubmissionOperation, public TSharedFromThis<FSyncOperation>
		{
		public:
			
			FSyncOperation(FReplicationClientManager& ClientManager, TMap<FGuid, FSubmissionParams>&& Operations)
			{
				const int32 NumOperations = Operations.Num();
				IntermediateResults.StreamResponses.Reserve(NumOperations);
				IntermediateResults.AuthorityResponses.Reserve(NumOperations);
				SubOperations.Reserve(NumOperations);

				// Prepare the intermediate slots in advance to handle concurrent writing
				for (TPair<FGuid, FSubmissionParams>& Pair : Operations)
				{
					// Allocate slots
					const FGuid& ClientId = Pair.Key;
					IntermediateResults.StreamResponses.Add(ClientId);
					IntermediateResults.AuthorityResponses.Add(ClientId);

					FReplicationClient* Client = ClientManager.FindClient(ClientId);
					checkf(Client, TEXT("Was supposed to have been validated ExecuteParallelStreamChanges."))
					SubOperations.Emplace(*this, ClientManager, ClientId, MoveTemp(Pair.Value), Client->GetSubmissionQueue());
				}
			}

			virtual ~FSyncOperation() override
			{
				if (!bPromiseWasSet)
				{
					bPromiseWasSet = true;
					constexpr bool bWasCancelled = true;
					Promise.EmplaceValue(FParallelExecutionResult{ bWasCancelled });
				}
			}

			void StartSubmissions()
			{
				// FDeferredSubmitter needs access to SharedThis(), so it must be called outside of constructor.
				for (FPendingOperation& PendingOperation : SubOperations)
				{
					FDeferredSubmitter& Submitter = PendingOperation.Submitter;
					Submitter.EnqueueOrSubmit();
				}
			}

			//~ Begin IParallelSubmissionOperation Interface
			virtual TFuture<FParallelExecutionResult> GetFuture() override { return Promise.GetFuture(); }
			//~ End IParallelSubmissionOperation Interface

			void OnCompleteStream(const FGuid& ClientId, FSubmitStreamChangesResponse&& Response)
			{
				LogStreamChangeError(ClientId, Response);
				
				--FindOperationByClient(ClientId)->NumOperationsLeft;
				IntermediateResults.StreamResponses[ClientId] = MoveTemp(Response);

				FinishIfDone();
			}

			void OnCompleteAuthority(const FGuid& ClientId, FSubmitAuthorityChangesResponse&& Response)
			{
				--FindOperationByClient(ClientId)->NumOperationsLeft;
				IntermediateResults.AuthorityResponses[ClientId] = MoveTemp(Response);
				
				const bool bIsFailure = Response.ErrorCode == EAuthoritySubmissionResponseErrorCode::Timeout || (Response.Response && !Response.Response->RejectedObjects.IsEmpty());
				UE_CLOG(bIsFailure, LogConcert, Warning, TEXT("Remote authority change to client %s failed"), *ClientId.ToString());
				FinishIfDone();
			}

			void OnSubmissionFailure(const FGuid& ClientId)
			{
				FindOperationByClient(ClientId)->NumOperationsLeft = 0;
				FinishIfDone();
			}

		private:
			
			TPromise<FParallelExecutionResult> Promise;
			bool bPromiseWasSet = false;

			struct FPendingOperation
			{
				/** Handles the submission. Unregisters from the client queue (if we're destroyed early). */
				FDeferredSubmitter Submitter;
			
				/** Used to determine when the operation is done. */
				int32 NumOperationsLeft = 2; // Stream + Authority = 2
				
				FPendingOperation(
					FSyncOperation& InOwner,
					FReplicationClientManager& InClientManager,
					const FGuid& InClientId,
					FSubmissionParams&& InSubmissionParams,
					FSubmissionQueue& InSubmissionQueue
					)
					: Submitter(InOwner, InClientManager, InSubmissionQueue, InClientId, MoveTemp(InSubmissionParams))
				{}

				bool IsDone() const { return NumOperationsLeft == 0; }
			};
			/**
			 * Each latent tasks has its own, pre-allocated slot which effectively handles potentially concurrent writes.
			 *
			 * Initially every task value is initialized to 2: one for each operation (stream, authority).
			 * Every time a sub-operation finishes, the value decrements.
			 * Once all values have reached 0, that means Operations can be Emplace()'ed with IntermediateResults.
			 */
			TArray<FPendingOperation> SubOperations;

			/** Holds intermediate results as they come in. */
			FParallelExecutionResult IntermediateResults;

			void FinishIfDone()
			{
				const bool bAllDone = Algo::AllOf(SubOperations, [](const FPendingOperation& OperationInfo)
				{
					return OperationInfo.IsDone();
				});

				if (bAllDone && ensure(!bPromiseWasSet))
				{
					bPromiseWasSet = true;
					Promise.EmplaceValue(MoveTemp(IntermediateResults));
				}
			}

			FPendingOperation* FindOperationByClient(const FGuid& ClientId)
			{
				return SubOperations.FindByPredicate([&ClientId](const FPendingOperation& Operation)
				{
					return Operation.Submitter.GetClientId() == ClientId;
				});
			}
		};

		void FDeferredSubmitter::PerformSubmission_GameThread(ISubmissionWorkflow& Workflow)
		{
			const TSharedPtr<ISubmissionOperation> SubmissionOperation = Workflow.SubmitChanges(SubmissionParams);
			if (ensure(SubmissionOperation))
			{
				SubmissionOperation->OnCompleteStreamChangesFuture()
					.Next([this, WeakOwner = Owner.AsWeak()](FSubmitStreamChangesResponse&& Response)
					{
						if (const TSharedPtr<FSyncOperation> SyncOperation = WeakOwner.Pin())
						{
							SyncOperation->OnCompleteStream(ClientId, MoveTemp(Response));
						}
					});
				SubmissionOperation->OnCompleteAuthorityChangeFuture()
					.Next([this, WeakOwner = Owner.AsWeak()](FSubmitAuthorityChangesResponse&& Response)
					{
						if (const TSharedPtr<FSyncOperation> SyncOperation = WeakOwner.Pin())
						{
							SyncOperation->OnCompleteAuthority(ClientId, MoveTemp(Response));
						}
					});
			}
			else
			{
				Owner.OnSubmissionFailure(ClientId);
			}
		}
	}
	
	TSharedPtr<IParallelSubmissionOperation> ExecuteParallelStreamChanges(FReplicationClientManager& ClientManager, TMap<FGuid, FSubmissionParams> ParallelOperations)
	{
		ParallelSubmission::Private::RemoveInvalidClientEntries(ClientManager, ParallelOperations);
		if (!ensure(!ParallelOperations.IsEmpty()))
		{
			return nullptr;
		}

		using namespace ParallelSubmission::Private;
		TSharedRef<FSyncOperation> Operation = MakeShared<FSyncOperation>(ClientManager, MoveTemp(ParallelOperations));
		Operation->StartSubmissions();
		return Operation;
	}

	TSharedPtr<IParallelSubmissionOperation> ExecuteParallelStreamChanges(FReplicationClientManager& ClientManager, TMap<FGuid, FConcertReplication_ChangeStream_Request> ParallelOperations)
	{
		if (!ensure(!ParallelOperations.IsEmpty()))
		{
			return nullptr;
		}
		
		TMap<FGuid, FSubmissionParams> Transformed;
		Transformed.Reserve(ParallelOperations.Num()); // check()s if Num() == 0
		for (TPair<FGuid, FConcertReplication_ChangeStream_Request>& RequestPair : ParallelOperations)
		{
			Transformed.Emplace(RequestPair.Key, FSubmissionParams{ MoveTemp(RequestPair.Value) });
		}
		
		return ExecuteParallelStreamChanges(ClientManager, MoveTemp(Transformed));
	}
}
