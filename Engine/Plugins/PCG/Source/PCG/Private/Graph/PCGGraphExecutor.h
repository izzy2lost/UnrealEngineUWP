// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGContext.h"
#include "PCGElement.h"
#include "PCGPin.h"
#include "PCGSubsystem.h"
#include "Graph/PCGGraphCache.h"
#include "Graph/PCGGraphCompiler.h"
#include "Graph/PCGPinDependencyExpression.h"
#include "Graph/PCGStackContext.h"

#include "Misc/SpinLock.h"
#include "UObject/GCObject.h"
#include "Tasks/Task.h"
#include "Templates/UniquePtr.h"

#if WITH_EDITOR
#include "Editor/IPCGEditorProgressNotification.h"
#include "WorldPartition/WorldPartitionHandle.h" // Needed for FWorldPartitionReference
#endif

class UPCGGraph;
class UPCGNode;
class UPCGComponent;
class FPCGGraphCompiler;
struct FPCGStack;
class FPCGStackContext;
class FTextFormat;

namespace PCGGraphExecutor
{
	extern PCG_API TAutoConsoleVariable<float> CVarTimePerFrame;
	extern PCG_API TAutoConsoleVariable<bool> CVarGraphMultithreading;

#if WITH_EDITOR
	extern PCG_API TAutoConsoleVariable<float> CVarEditorTimePerFrame;
#endif
}

struct FPCGGraphTaskInput
{
	FPCGGraphTaskInput(FPCGTaskId InTaskId, const TOptional<FPCGPinProperties>& InUpstreamPin = NoPin, const TOptional<FPCGPinProperties>& InDownstreamPin = NoPin, bool bInProvideData = true)
		: TaskId(InTaskId)
		, UpstreamPin(InUpstreamPin)
		, DownstreamPin(InDownstreamPin)
		, bProvideData(bInProvideData)
	{
	}

#if WITH_EDITOR
	bool operator==(const FPCGGraphTaskInput& Other) const;
#endif

	FPCGTaskId TaskId;

	/** The upstream output pin from which the input data comes. */
	TOptional<FPCGPinProperties> UpstreamPin;

	/** The input pin on the task element. */
	TOptional<FPCGPinProperties> DownstreamPin;

	/** Whether the input provides any data. For the post execute task, only the output node will provide data. */
	bool bProvideData;

	static inline const TOptional<FPCGPinProperties> NoPin = TOptional<FPCGPinProperties>();
};

struct FPCGGraphTask
{
#if WITH_EDITOR
	/** Approximate equivalence. Does not deeply check node settings, nor does it do a deep comparison of the element. */
	bool IsApproximatelyEqual(const FPCGGraphTask& Other) const;

	/** Because we might not already have a context, but still want to attach some logs to the node, use this utility function */
	void LogVisual(ELogVerbosity::Type InVerbosity, const FText& InMessage) const;
#endif

	const FPCGStack* GetStack() const;

	TArray<FPCGGraphTaskInput> Inputs;
	const UPCGNode* Node = nullptr;
	TWeakObjectPtr<UPCGComponent> SourceComponent = nullptr;
	FPCGElementPtr Element; // Added to have tasks that aren't node-bound
	FPCGContext* Context = nullptr;
	FPCGTaskId NodeId = InvalidPCGTaskId;
	FPCGTaskId CompiledTaskId = InvalidPCGTaskId; // the task id as it exists when compiled
	FPCGTaskId ParentId = InvalidPCGTaskId; // represents the parent sub object graph task, if we were called from one

	/** Conjunction of disjunctions of pin IDs that are required to be active for this task to be active.
	* Example - keep task if: UpstreamPin0Active && (UpstreamPin1Active || UpstreamPin2Active)
	*/
	FPCGPinDependencyExpression PinDependency;

	int32 StackIndex = INDEX_NONE;
	TSharedPtr<const FPCGStackContext> StackContext;

	// Whether SetupTask has been called on this task
	bool bHasDoneSetup = false;
	// BuildTaskInput will initialize this Collection which will later be used by PrepareForExecute
	FPCGDataCollection TaskInput;
	// CombineParams call might have created AsyncObjects
	TSet<TObjectPtr<UObject>> CombineParamsAsyncObjects;

	// Whether PrepareForExecute as been called on this task
	bool bHasDonePrepareForExecute = false;

#if WITH_EDITOR
	// Can be true when we want to have debug display on a task but have taken the results from the cache
	bool bIsBypassed = false;
#endif
};

struct FPCGGraphScheduleTask
{
	TArray<FPCGGraphTask> Tasks;
	TWeakObjectPtr<UPCGComponent> SourceComponent = nullptr;
	int32 FirstTaskIndex = 0;
	int32 LastTaskIndex = 0;
	bool bHasAbortCallbacks = false;
};

struct FPCGGraphActiveTask : TSharedFromThis<FPCGGraphActiveTask>
{
	FPCGGraphActiveTask() = default;
	virtual ~FPCGGraphActiveTask();

	FPCGGraphActiveTask(const FPCGGraphActiveTask&) = delete;
	FPCGGraphActiveTask& operator=(const FPCGGraphActiveTask&) = delete;

	FPCGGraphActiveTask(FPCGGraphActiveTask&&) = delete;
	FPCGGraphActiveTask& operator=(FPCGGraphActiveTask&&) = delete;

	void StartExecuting();
	void StopExecuting();

	TArray<FPCGGraphTaskInput> Inputs;
	FPCGElementPtr Element;
	TUniquePtr<FPCGContext> Context;
	FPCGTaskId NodeId = InvalidPCGTaskId;
	std::atomic<bool> bWasCancelled = false;
#if WITH_EDITOR
	bool bIsBypassed = false;
#endif
	int32 StackIndex = INDEX_NONE;
	TSharedPtr<const FPCGStackContext> StackContext;
		
	// Those members need to be modified under the FPCGGraphExecutor::LiveTasksLock (unless we are running the old executor path)
	UE::Tasks::TTask<bool> ExecutingTask;
	bool bIsExecutingTask = false;

	// Used to know if task should be in ActiveTasks or ActiveTasksGameThreadOnly
	bool bIsGameThreadOnly = false;
	// TaskIndex inside ActiveTasks/ActiveTasksGameThreadOnly/SleepingTasks for fast removal
	int32 TaskIndex = INDEX_NONE;

	static int32 NumExecuting;
	TArray<TObjectPtr<const UObject>> ExecutingReferences;
};

class FPCGGraphExecutor : public FGCObject
{
public:
	// Default constructor used by unittests
	FPCGGraphExecutor();
	FPCGGraphExecutor(UWorld* InWorld);
	~FPCGGraphExecutor();

	/** Compile (and cache) a graph for later use. This call is threadsafe */
	void Compile(UPCGGraph* InGraph);

	/** Schedules the execution of a given graph with specified inputs. This call is threadsafe */
	FPCGTaskId Schedule(UPCGComponent* InComponent, const TArray<FPCGTaskId>& TaskDependency = TArray<FPCGTaskId>(), const FPCGStack* InFromStack = nullptr);

	/** Schedules the execution of a given graph with specified inputs. This call is threadsafe */
	FPCGTaskId Schedule(
		UPCGGraph* Graph,
		UPCGComponent* InSourceComponent,
		FPCGElementPtr PreGraphElement,
		FPCGElementPtr InputElement,
		const TArray<FPCGTaskId>& TaskDependency,
		const FPCGStack* InFromStack,
		bool bAllowHierarchicalGeneration);

	/** Cancels all tasks originating from the given component */
	TArray<UPCGComponent*> Cancel(UPCGComponent* InComponent);
	
	/** Cancels all tasks running a given graph */
	TArray<UPCGComponent*> Cancel(UPCGGraph* InGraph);

	/** Cancels all tasks */
	TArray<UPCGComponent*> CancelAll();

	/** Returns true if any task is scheduled or executing for the given graph. */
	bool IsGraphCurrentlyExecuting(UPCGGraph* InGraph);

	/** Returns true if any task is scheduled or executing for any graph */
	bool IsAnyGraphCurrentlyExecuting() const;
	
	// Back compatibility function. Use ScheduleGenericWithContext
	FPCGTaskId ScheduleGeneric(TFunction<bool()> InOperation, UPCGComponent* InSourceComponent, const TArray<FPCGTaskId>& TaskExecutionDependencies);
	FPCGTaskId ScheduleGeneric(TFunction<bool()> InOperation, TFunction<void()> InAbortOperation, UPCGComponent* InSourceComponent, const TArray<FPCGTaskId>& TaskExecutionDependencies);

	/** General job scheduling
	*  @param InOperation:               Callback that takes a Context as argument and returns true if the task is done, false otherwise
	*  @param InSourceComponent:         PCG component associated with this task. Can be null.
	*  @param TaskExecutionDependencies: Task will wait on these tasks to execute and won't take their output data as input.
	*  @param TaskDataDependencies:      Task will wait on these tasks to execute and will take their output data as input.
	*/
	FPCGTaskId ScheduleGenericWithContext(TFunction<bool(FPCGContext*)> InOperation, UPCGComponent* InSourceComponent, const TArray<FPCGTaskId>& TaskExecutionDependencies, const TArray<FPCGTaskId>& TaskDataDependencies);

	/** General job scheduling
	*  @param InOperation:               Callback that takes a Context as argument and returns true if the task is done, false otherwise
	*  @param InAbortOperation:          Callback that is called if the task is aborted (cancelled) before fully executed.
	*  @param InSourceComponent:         PCG component associated with this task. Can be null.
	*  @param TaskExecutionDependencies: Task will wait on these tasks to execute and won't take their output data as input.
	*  @param TaskDataDependencies:      Task will wait on these tasks to execute and will take their output data as input.
	*/
	FPCGTaskId ScheduleGenericWithContext(TFunction<bool(FPCGContext*)> InOperation, TFunction<void(FPCGContext*)> InAbortOperation, UPCGComponent* InSourceComponent, const TArray<FPCGTaskId>& TaskExecutionDependencies, const TArray<FPCGTaskId>& TaskDataDependencies);

	/** Gets data in the output results. Returns false if data is not ready. */
	bool GetOutputData(FPCGTaskId InTaskId, FPCGDataCollection& OutData);

	/** Clear output data */
	void ClearOutputData(FPCGTaskId InTaskId);

	/** Accessor so PCG tools (e.g. profiler) can easily decode graph task ids **/
	FPCGGraphCompiler& GetCompiler() { return GraphCompiler; }

	/** Accessor so PCG tools (e.g. profiler) can easily decode graph task ids **/
	const FPCGGraphCompiler& GetCompiler() const { return GraphCompiler; }

#if WITH_EDITOR
	FPCGTaskId ScheduleDebugWithTaskCallback(UPCGComponent* InComponent, TFunction<void(FPCGTaskId, const UPCGNode*, const FPCGDataCollection&)> TaskCompleteCallback);

	/** Notify compiler that graph has changed so it'll be removed from the cache */
	void NotifyGraphChanged(UPCGGraph* InGraph, EPCGChangeType ChangeType);

	/** Returns the number of entries currently in the cache for InElement. */
	uint32 GetGraphCacheEntryCount(IPCGElement* InElement) const { return GraphCache.GetGraphCacheEntryCount(InElement); }
#endif

	/** "Tick" of the graph executor. This call is NOT THREADSAFE */
	void Execute();

	/** Expose cache so it can be dirtied */
	FPCGGraphCache& GetCache() { return GraphCache; }

	/** True if graph cache debugging is enabled. */
	bool IsGraphCacheDebuggingEnabled() const { return GraphCache.IsDebuggingEnabled(); }

	//~Begin FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FPCGGraphExecutor"); }
	//~End FGCObject interface

private:
	void ExecuteV1();
	void ExecuteV2();
	double GetTickBudgetInSeconds() const;
		
	struct FCachedResult
	{
		FPCGTaskId TaskId = InvalidPCGTaskId;
		FPCGDataCollection Output;
		const FPCGStack* Stack = nullptr;
		const UPCGNode* Node = nullptr;
		bool bDoDynamicTaskCulling = false;
		bool bIsPostGraphTask = false;		
		bool bIsBypassed = false;
	};
		
	void PostTaskExecute(TSharedPtr<FPCGGraphActiveTask> ActiveTask, bool bIsDone);
	bool ProcessScheduledTasks();
	void ExecuteTasksEnded();
	bool ExecuteScheduling(double EndTime, TSharedPtr<FPCGGraphActiveTask>* OutMainThreadTask = nullptr, bool bForceCheckSleepingTasks = false);

	TSet<UPCGComponent*> Cancel(TFunctionRef<bool(TWeakObjectPtr<UPCGComponent>)> CancelFilter);
	void ClearAllTasks();
	void QueueNextTasks(FPCGTaskId FinishedTask);
	TArray<FCachedResult*> QueueNextTasksInternal(FPCGTaskId FinishedTask);

	bool CancelNextTasks(FPCGTaskId CancelledTask, TSet<UPCGComponent*>& OutCancelledComponents);
	void RemoveTaskFromInputSuccessors(FPCGTaskId CancelledTask, const TArray<FPCGGraphTaskInput>& CancelledTaskInputs);
	void RemoveTaskFromInputSuccessorsNoLock(FPCGTaskId CancelledTask, const TArray<FPCGGraphTaskInput>& CancelledTaskInputs);
	
	/** Called from QueueNextTasks/ProcessScheduledTasks will try and setup/prepare task for execution */
	void OnTaskInputsReady(FPCGGraphTask& Task, TArray<FCachedResult*>& OutCachedResults, bool bIsInGameThread);

	/** SetupTask will call BuildTaskInput and assign a IPCGElement to the FPCGGraphTask */
	bool SetupTask(FPCGGraphTask& Task, TArray<FPCGTaskId>& ResultsToMarkAsRead);
	void BuildTaskInput(FPCGGraphTask& Task, TArray<FPCGTaskId>& ResultsToMarkAsRead);
	/** Will check the cache for existing result or create and initialize the FPCGContext to the task */
	void PrepareForExecute(FPCGGraphTask& Task, FCachedResult*& OutCachedResult, bool bLiveTasksLockAlreadyLocked);

	/** Store cache results and Queue next tasks */
	void ProcessCachedResults(TArray<FCachedResult*> CachedResults);
	TArray<FPCGTaskId> ProcessCachedResultsInternal(TArray<FCachedResult*> CachedResults);

	/** Combine all param data into one on the Params pin, if any.*/
	void CombineParams(FPCGGraphTask& Task);
	void StoreResults(FPCGTaskId InTaskId, const FPCGDataCollection& InTaskOutput, bool bNeedsManualClear);
	void ClearResults();
	void MarkInputResults(TArrayView<const FPCGTaskId> InInputResults);

	/** If the completed task has one or more deactivated pins, delete any downstream tasks that are inactive as a result. */
	void CullInactiveDownstreamNodes(FPCGTaskId CompletedTaskId, uint64 InInactiveOutputPinBitmask);

	/** Builds an array of all deactivated unique pin IDs. */
	static void GetPinIdsToDeactivate(FPCGTaskId TaskId, uint64 InactiveOutputPinBitmask, TArray<FPCGPinId>& InOutPinIds);

	FPCGElementPtr GetFetchInputElement();

	void LogTaskState() const;

	int32 GetNonScheduledRemainingTaskCount() const;

#if WITH_EDITOR
	/** Notify the component that the given pins were deactivated during execution. */
	void SendInactivePinNotification(const UPCGNode* InNode, const FPCGStack* InStack, uint64 InactiveOutputPinBitmask);

	void UpdateGenerationNotification();
	void ReleaseGenerationNotification();
	void OnNotificationCancel();
	static FTextFormat GetNotificationTextFormat();
#endif

	/** Graph compiler that turns a graph into tasks */
	FPCGGraphCompiler GraphCompiler;

	/** Graph results cache */
	FPCGGraphCache GraphCache;

	/** Input fetch element, stored here so we have only one */
	FPCGElementPtr FetchInputElement;

	/** 
	 * Define a Lock level for future reference. Rule is when we have a lock, we can't lock a lower or equal level lock to prevent deadlocks.
	 * Example: When locking LiveTasksLock we can't lock ScheduleLock or CollectGCReferenceTasksLock 
	 */

	/** Lock level - 1 (top most lock) */
	UE::FSpinLock ScheduleLock;
	TArray<FPCGGraphScheduleTask> ScheduledTasks;

	/** Lock level 2 */
	mutable UE::FSpinLock TasksLock;
	TMap<FPCGTaskId, FPCGGraphTask> Tasks;
	TMap<FPCGTaskId, TSet<FPCGTaskId>> TaskSuccessors;

	/** Lock level 3 */
	UE::FSpinLock LiveTasksLock;
	TArray<FPCGGraphTask> ReadyTasks;
	TArray<TSharedPtr<FPCGGraphActiveTask>> ActiveTasks;
	TArray<TSharedPtr<FPCGGraphActiveTask>> ActiveTasksGameThreadOnly;
	TArray<TSharedPtr<FPCGGraphActiveTask>> SleepingTasks;
	bool bNeedToCheckSleepingTasks = false;

	/** Lock level 3 */
	UE::FSpinLock CollectGCReferenceTasksLock;
	TSet<TSharedPtr<FPCGGraphActiveTask>> CollectGCReferenceTasks;
			
	/** Map of node instances to their output, could be cleared once execution is done */
	/** Note: this should at some point unload based on loaded/unloaded proxies, otherwise memory cost will be unbounded */
	struct FOutputDataInfo
	{
		FPCGDataCollection DataCollection;
		// Controls whether the results will be expunged from the OutputData as soon as the successor count reaches 0 or not.
		bool bNeedsManualClear = false;
		// Successor count, updated after a successor is done executing (MarkInputResults).
		int32 RemainingSuccessorCount = 0;
		// Culled
		bool bCulled = false;
	};

	/** Lock level 4 */
	UE::FSpinLock CachingResultsLock;
	// Used to keep GC references to in flight caching results (not yet stored to output and might not be in cache anymore)
	TMap<FPCGTaskId, TUniquePtr<FCachedResult>> CollectGCCachingResults;

	/** Lock level 4 */
	UE::FSpinLock TaskOutputsLock;
	TMap<FPCGTaskId, FOutputDataInfo> TaskOutputs;
	
	/** Monotonically increasing id. Should be reset once all tasks are executed, should be protected by the ScheduleLock */
	FPCGTaskId NextTaskId = 0;
	
	std::atomic<bool> bNeedToExecuteTasksEnded = false;

	/** Runtime information */
	int32 CurrentlyUsedThreads = 0;

#if WITH_EDITOR
	TWeakPtr<IPCGEditorProgressNotification> GenerationProgressNotification;
	double GenerationProgressNotificationStartTime = 0.0;
	int32 GenerationProgressLastTaskNum = 0;
#endif

	TObjectPtr<UWorld> World = nullptr;

	// Temporary while the 2 schedulers exist
	enum class EExecuteVersion : uint8
	{
		None,
		V1,
		V2
	};
	EExecuteVersion ExecuteVersion = EExecuteVersion::None;

	EExecuteVersion GetExecuteVersion() const;

	// Handler that we can use as a Weak ptr to determine if the Executor is still valid
	class FGameThreadHandler : public TSharedFromThis<FGameThreadHandler>
	{
	public:
		FGameThreadHandler(FPCGGraphExecutor* InExecutor)
			: Executor(InExecutor) { }

		FPCGGraphExecutor* GetExecutor() { return Executor; }

	private:
		FPCGGraphExecutor* Executor = nullptr;
	};

	TSharedPtr<FGameThreadHandler> GameThreadHandler;
};

class FPCGFetchInputElement : public IPCGElement
{
public:
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool IsPassthrough(const UPCGSettings* InSettings) const override { return true; }
};

class FPCGGenericElement : public IPCGElement
{
public:
	using FContextAllocator = TFunction<FPCGContext*(const FPCGDataCollection&, TWeakObjectPtr<UPCGComponent>, const UPCGNode*)>;

	FPCGGenericElement(
		TFunction<bool(FPCGContext*)> InOperation,
		const FContextAllocator& InContextAllocator = (FContextAllocator)[](const FPCGDataCollection&, TWeakObjectPtr<UPCGComponent>, const UPCGNode*)
	{
		return new FPCGContext();
	});

	FPCGGenericElement(
		TFunction<bool(FPCGContext*)> InOperation,
		TFunction<void(FPCGContext*)> InAbortOperation,
		const FContextAllocator& InContextAllocator = (FContextAllocator)[](const FPCGDataCollection&, TWeakObjectPtr<UPCGComponent>, const UPCGNode*)
	{
		return new FPCGContext();
	});
	
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;

	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	// Important note: generic elements must always be run on the main thread
	// as most of these will impact the editor in some way (loading, unloading, saving)
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual void AbortInternal(FPCGContext* Context) const override;
	virtual bool IsCancellable() const override { return false; }

#if WITH_EDITOR
	virtual bool ShouldLog() const override { return false; }
#endif

private:
	TFunction<bool(FPCGContext*)> Operation;
	TFunction<void(FPCGContext*)> AbortOperation;

	/** Creates a context object for this element. */
	FContextAllocator ContextAllocator;
};

/** Context for linkage element which marshalls data across hierarchical generation grids. */
struct FPCGGridLinkageContext : public FPCGContext
{
	/** If we require data from a component that is not generated, we schedule it once to see if we can get the data later. */
	bool bScheduledGraph = false;
};

namespace PCGGraphExecutor
{
	/** Marshals data across grid sizes at execution time. */
	class FPCGGridLinkageElement : public FPCGGenericElement
	{
	public:
		FPCGGridLinkageElement(TFunction<bool(FPCGContext*)> InOperation, const FContextAllocator& InContextAllocator, EPCGHiGenGrid InFromGrid, EPCGHiGenGrid InToGrid, const FString& InResourceKey)
			: FPCGGenericElement(InOperation, InContextAllocator)
			, FromGrid(InFromGrid)
			, ToGrid(InToGrid)
			, ResourceKey(InResourceKey)
		{
		}

#if WITH_EDITOR
		//~Begin IPCGElement interface
		virtual bool IsGridLinkage() const override { return true; }
		//~End IPCGElement interface

		/** Return true if the grid sizes & path match. */
		bool operator==(const FPCGGridLinkageElement& Other) const;
#endif

	private:
		// These values are stored here so that we can compare two grid linkage elements for equivalence.
		EPCGHiGenGrid FromGrid = EPCGHiGenGrid::Uninitialized;
		EPCGHiGenGrid ToGrid = EPCGHiGenGrid::Uninitialized;
		FString ResourceKey;
	};

	/** Compares InFromGrid and InToGrid and performs data storage/retrieval as necessary to marshal data across execution grids. */
	bool ExecuteGridLinkage(
		EPCGHiGenGrid InGenerationGrid,
		EPCGHiGenGrid InFromGrid,
		EPCGHiGenGrid InToGrid,
		const FString& InResourceKey,
		const FName& InOutputPinLabel,
		const UPCGNode* InDownstreamNode,
		FPCGGridLinkageContext* InContext);
}
