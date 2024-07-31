// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGElement.h"

#include "PCGContext.h"
#include "Compute/Elements/PCGCustomHLSL.h"

#include "Compute/PCGComputeGraph.h"
#include "Compute/PCGDataBinding.h"
#include "ComputeFramework/ComputeGraphInstance.h"

#include "UObject/StrongObjectPtr.h"

struct FPCGComputeGraphContext : public FPCGContext
{
public:
	virtual bool IsComputeContext() const override { return true; }

protected:
	virtual void AddExtraStructReferencedObjects(FReferenceCollector& Collector) override;

public:
	TStrongObjectPtr<UPCGDataBinding> DataBinding = nullptr;

	/** Data providers created from data interfaces and data bindings. */
	FComputeGraphInstance ComputeGraphInstance;

	/** Keep track of data providers that perform async operations and require multiple frames to complete. */
	TSet<UComputeDataProvider*> ProvidersRunningAsyncOperations;
	mutable FRWLock ProvidersRunningAsyncOperationsLock;

	/** Graph enqueued (scheduled for execution by GPU). */
	bool bGraphEnqueued = false;

	/** All async operations complete and results processed. */
	bool bAllAsyncOperationsDone = false;

	/** Graph executed successfully. */
	bool bExecutionSuccess = false;
};

/** Executes a CF graph. Created by the compiler when collapsing GPU nodes rather than by a settings/node. */
class FPCGComputeGraphElement : public IPCGElement
{
public:
	FPCGComputeGraphElement() {}

#if WITH_EDITOR
	//~Begin IPCGElement interface
	virtual bool IsComputeGraphElement() const override { return true; }
	//~End IPCGElement interface

	/** Return true if the elements are identical, used for change detection. */
	bool operator==(const FPCGComputeGraphElement& Other) const;
#endif

	TStrongObjectPtr<UPCGComputeGraph> Graph = nullptr;

protected:
	virtual FPCGContext* CreateContext() override { return new FPCGComputeGraphContext(); }
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
	virtual void PostExecuteInternal(FPCGContext* InContext) const override;
	virtual void AbortInternal(FPCGContext* InContext) const override;;

	// The calls to initialize the compute graph are not thread safe.
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

	// TODO - need to accumulate dependencies from compute graph nodes.
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

	void ResetAsyncOperations(FPCGContext* InContext) const;

#if WITH_EDITOR
	void LogCompilationMessages(FPCGComputeGraphContext* InContext) const;
#endif
};
