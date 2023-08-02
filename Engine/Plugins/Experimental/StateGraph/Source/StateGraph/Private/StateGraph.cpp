// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateGraph.h"

#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogStateGraph);

class FStateGraphModule : public IModuleInterface
{
};

IMPLEMENT_MODULE(FStateGraphModule, StateGraph);

namespace UE {

// FStateGraphNode

FStateGraphNode::FStateGraphNode(FName InName) :
	Name(InName)
{
	UE_LOG_STATEGRAPH(VeryVerbose, TEXT("[%s.%s] Created node"), *GetStateGraphName().ToString(), *Name.ToString());
}

FStateGraphNode::~FStateGraphNode()
{
	UE_LOG_STATEGRAPH(VeryVerbose, TEXT("[%s.%s] Destroyed node"), *GetStateGraphName().ToString(), *Name.ToString());
}

const TCHAR* FStateGraphNode::GetStatusName(EStatus Status)
{
	switch (Status)
	{
		case EStatus::NotStarted: return TEXT("NotStarted");
		case EStatus::Blocked: return TEXT("Blocked");
		case EStatus::Started: return TEXT("Started");
		case EStatus::Completed: return TEXT("Completed");
		case EStatus::TimedOut: return TEXT("TimedOut");
		default: checkNoEntry(); return TEXT("Unknown");
	}
}

void FStateGraphNode::SetTimeout(double InTimeout)
{
	Timeout = InTimeout;
}

double FStateGraphNode::GetDuration() const
{
	if (StartTime == 0.f)
	{
		return 0.f;
	}

	return (CompletedTime == 0.f ? FPlatformTime::Seconds() : CompletedTime) - StartTime;
}

bool FStateGraphNode::CheckDependencies() const
{
	FStateGraphPtr StateGraphPtr(StateGraphWeakPtr.Pin());
	if (!StateGraphPtr.IsValid())
	{
		UE_LOG_STATEGRAPH(Warning, TEXT("[%s] Node checked with invalid state graph"), *Name.ToString());
		return false;
	}

	for (FName Dependency : Dependencies)
	{
		FStateGraphNodeRef* DependencyNode = StateGraphPtr->GetNodeRef(Dependency);
		if (!DependencyNode || (*DependencyNode)->GetStatus() != FStateGraphNode::EStatus::Completed)
		{
			return false;
		}
	}

	return true;
}

void FStateGraphNode::Complete()
{
	if (Status == EStatus::Completed)
	{
		UE_LOG_STATEGRAPH(Warning, TEXT("[%s.%s] Node already completed"), *GetStateGraphName().ToString(), *Name.ToString());
		return;
	}

	CompletedTime = FPlatformTime::Seconds();
	UE_LOG_STATEGRAPH(Log, TEXT("[%s.%s] Completed node (Duration=%.6f)"), *GetStateGraphName().ToString(), *Name.ToString(), CompletedTime - StartTime);

	// Keep a reference to check if the node is destroyed during external functions.
	FStateGraphNodeWeakPtr StateGraphNodeWeakPtr(AsWeak());

	SetStatus(EStatus::Completed);

	if (!StateGraphNodeWeakPtr.IsValid())
	{
		return;
	}

	// Run state graph again since this completion may have fulfilled dependencies to start new nodes.
	FStateGraphPtr StateGraphPtr = StateGraphWeakPtr.Pin();
	if (StateGraphPtr.IsValid())
	{
		if (StateGraphPtr->GetStatus() != FStateGraph::EStatus::Paused)
		{
			StateGraphPtr->Run();
		}
	}
	else
	{
		UE_LOG_STATEGRAPH(Warning, TEXT("[%s] Node completed with invalid state graph"), *Name.ToString());
	}
}

void FStateGraphNode::Reset()
{
	UE_LOG_STATEGRAPH(Verbose, TEXT("[%s.%s] Resetting node"), *GetStateGraphName().ToString(), *Name.ToString());
	StartTime = 0.f;
	CompletedTime = 0.f;
	SetStatus(EStatus::NotStarted);
}

FName FStateGraphNode::GetStateGraphName() const
{
	return StateGraphWeakPtr.IsValid() ? StateGraphWeakPtr.Pin()->GetName() : NAME_None;
}

void FStateGraphNode::UpdateConfig()
{
	GConfig->GetDouble(*ConfigSectionName, TEXT("Timeout"), Timeout, GEngineIni);
}

void FStateGraphNode::SetStatus(EStatus NewStatus)
{
	if (Status != NewStatus)
	{
		const EStatus OldStatus = Status;
		Status = NewStatus;
		FStateGraphPtr StateGraphPtr = StateGraphWeakPtr.Pin();
		if (StateGraphPtr.IsValid())
		{
			StateGraphPtr->OnNodeStatusChanged.Broadcast(*this, OldStatus, NewStatus);
		}
	}
}

// FStateGraphNodeFunction

FStateGraphNodeFunction::FStateGraphNodeFunction(FName InName, const FStateGraphNodeFunctionStart& InStartFunction) :
	FStateGraphNode(InName),
	StartFunction(InStartFunction)
{}

bool FStateGraphNodeFunction::CheckDependencies() const
{
	if (!StartFunction.IsBound())
	{
		UE_LOG_STATEGRAPH(Warning, TEXT("[%s.%s] Function node start not bound"), *GetStateGraphName().ToString(), *GetName().ToString());
		return false;
	}

	return FStateGraphNode::CheckDependencies();
}

void FStateGraphNodeFunction::Start()
{
	// CheckDependencies verified the function is bound right before calling this in StateGraph->Run().
	check(StartFunction.IsBound());

	FStateGraphNodeWeakPtr WeakPtr(AsWeak());
	StartFunction.Execute([WeakPtr, StateGraphName = GetStateGraphName(), NodeName = GetName()]()
	{
		if (WeakPtr.IsValid())
		{
			WeakPtr.Pin()->Complete();
		}
		else
		{
			UE_LOG_STATEGRAPH(Warning, TEXT("[%s.%s] Function node completed after node was destroyed"), *StateGraphName.ToString(), *NodeName.ToString());
		}
	});
}

// FStateGraph

FStateGraph::FStateGraph(FName InName) :
	Name(InName),
	ConfigSectionName(FString::Printf(TEXT("StateGraph.%s"), *InName.ToString()))
{
	UE_LOG_STATEGRAPH(VeryVerbose, TEXT("[%s] Created state graph"), *Name.ToString());
}

FStateGraph::~FStateGraph()
{
	UE_LOG_STATEGRAPH(VeryVerbose, TEXT("[%s] Destroyed state graph"), *Name.ToString());
	FCoreDelegates::TSOnConfigSectionsChanged().Remove(ConfigSectionsChangedDelegate);
}

void FStateGraph::Initialize()
{
	if (!ConfigSectionsChangedDelegate.IsValid())
	{
		ConfigSectionsChangedDelegate = FCoreDelegates::TSOnConfigSectionsChanged().AddSP(this, &FStateGraph::OnConfigSectionsChanged);
	}

	UpdateConfig();
}

const TCHAR* FStateGraph::GetStatusName(EStatus Status)
{
	switch (Status)
	{
		case EStatus::NotStarted: return TEXT("NotStarted");
		case EStatus::Running: return TEXT("Running");
		case EStatus::Waiting: return TEXT("Waiting");
		case EStatus::Blocked: return TEXT("Blocked");
		case EStatus::Completed: return TEXT("Completed");
		case EStatus::Paused: return TEXT("Paused");
		case EStatus::TimedOut: return TEXT("TimedOut");
		default: checkNoEntry(); return TEXT("Unknown");
	}
}

void FStateGraph::SetTimeout(double InTimeout)
{
	Timeout = InTimeout;
}

double FStateGraph::GetDuration() const
{
	if (StartTime == 0.f)
	{
		return 0.f;
	}

	return (CompletedTime == 0.f ? FPlatformTime::Seconds() : CompletedTime) - StartTime;
}

bool FStateGraph::AddNode(const FStateGraphNodeRef& Node)
{
	if (Node->StateGraphWeakPtr.IsValid())
	{
		UE_LOG_STATEGRAPH(Warning, TEXT("[%s.%s] Node already associated with a state graph"), *Name.ToString(), *Node->GetName().ToString());
		return false;
	}

	if (Nodes.Contains(Node->GetName()))
	{
		UE_LOG_STATEGRAPH(Warning, TEXT("[%s.%s] Node with the same name already exists"), *Name.ToString(), *Node->GetName().ToString());
		return false;
	}

	UE_LOG_STATEGRAPH(Verbose, TEXT("[%s.%s] Adding node"), *Name.ToString(), *Node->GetName().ToString());
	Node->StateGraphWeakPtr = AsWeak();
	Node->ConfigSectionName = FString::Printf(TEXT("%s.%s"), *ConfigSectionName, *Node->GetName().ToString());
	Nodes.Add(Node->GetName(), Node);
	Node->UpdateConfig();
	return true;
}

bool FStateGraph::RemoveNode(FName NodeName)
{
	FStateGraphNodeRef* Node = GetNodeRef(NodeName);
	if (!Node)
	{
		UE_LOG_STATEGRAPH(Warning, TEXT("[%s.%s] Failed to remove node"), *Name.ToString(), *NodeName.ToString());
		return false;
	}

	UE_LOG_STATEGRAPH(Verbose, TEXT("[%s.%s] Removing node"), *Name.ToString(), *NodeName.ToString());
	(*Node)->StateGraphWeakPtr.Reset();

	// Keep a reference to check if the state graph is destroyed during external functions.
	FStateGraphWeakPtr StateGraphWeakPtr(AsWeak());
	(*Node)->Removed();
	if (StateGraphWeakPtr.IsValid())
	{
		Nodes.Remove(NodeName);
	}

	return true;
}

void FStateGraph::RemoveAllNodes()
{
	// Keep a reference to check if the state graph is destroyed during external functions.
	FStateGraphWeakPtr StateGraphWeakPtr(AsWeak());

	// Copy names since node map may be modified during loop.
	TArray<FName> NodeNames;
	Nodes.GenerateKeyArray(NodeNames);
	for (FName NodeName : NodeNames)
	{
		RemoveNode(NodeName);
		if (!StateGraphWeakPtr.IsValid())
		{
			return;
		}
	}
}

void FStateGraph::Run()
{
	const double Now = FPlatformTime::Seconds();

	if (Status == EStatus::NotStarted)
	{
		StartTime = Now;
	}

	// Keep a reference to check if the state graph is destroyed during external functions.
	FStateGraphWeakPtr StateGraphWeakPtr(AsWeak());

	SetStatus(EStatus::Running);

	if (!StateGraphWeakPtr.IsValid())
	{
		return;
	}

	if (bRunning)
	{
		bRunAgain = true;
		return;
	}

	FTSTicker::GetCoreTicker().RemoveTicker(TimeoutTicker);

	double NextTimeout = 0.f;
	if (Timeout > 0.f)
	{
		NextTimeout = (StartTime + Timeout) - Now;
		if (NextTimeout <= 0.f)
		{
			UE_LOG_STATEGRAPH(Log, TEXT("[%s] State graph timed out (Duration=%.6f)"), *Name.ToString(), Now - StartTime);
			SetStatus(EStatus::TimedOut);
			return;
		}
	}

	bRunning = true;

	UE_LOG_STATEGRAPH(Verbose, TEXT("[%s] Starting run loop"), *Name.ToString());

	uint32 Blocked = 0;
	uint32 Started = 0;
	uint32 Running = 0;
	uint32 Completed = 0;
	uint32 Removed = 0;
	uint32 TimedOut = 0;

	// Copy names since node map may be modified during loop.
	TArray<FName> NodeNames;
	Nodes.GenerateKeyArray(NodeNames);
	for (FName NodeName : NodeNames)
	{
		if (Status != EStatus::Running)
		{
			// State graph was reset or paused during last node Start().
			break;
		}

		FStateGraphNodeRef* Node = GetNodeRef(NodeName);
		if (!Node)
		{
			++Removed;
			continue;
		}

		switch ((*Node)->Status)
		{
		case FStateGraphNode::EStatus::NotStarted:
		case FStateGraphNode::EStatus::Blocked:
			if (!(*Node)->CheckDependencies())
			{
				if (!StateGraphWeakPtr.IsValid())
				{
					return;
				}

				// Get node again in case CheckDependencies() removed the node.
				Node = GetNodeRef(NodeName);
				if (Node)
				{
					(*Node)->SetStatus(FStateGraphNode::EStatus::Blocked);

					if (!StateGraphWeakPtr.IsValid())
					{
						return;
					}

					++Blocked;
				}
				else
				{
					++Removed;
				}

				break;
			}

			UE_LOG_STATEGRAPH(Log, TEXT("[%s.%s] Starting node"), *Name.ToString(), *NodeName.ToString());
			(*Node)->SetStatus(FStateGraphNode::EStatus::Started);

			if (!StateGraphWeakPtr.IsValid())
			{
				return;
			}

			Node = GetNodeRef(NodeName);
			if (!Node)
			{
				++Removed;
				break;
			}

			(*Node)->StartTime = Now;
			(*Node)->Start();

			if (!StateGraphWeakPtr.IsValid())
			{
				return;
			}

			// Get node again in case Start() removed the node.
			Node = GetNodeRef(NodeName);
			if (Node)
			{
				if ((*Node)->Status == FStateGraphNode::EStatus::Completed)
				{
					// Start() called Complete() before returning.
					++Completed;
				}
				else
				{
					++Started;
				}
			}
			else
			{
				++Removed;
			}

			if (!Node || (*Node)->Status != FStateGraphNode::EStatus::Started)
			{
				break;
			}

			// Fall through to checkout for timeout.

		case FStateGraphNode::EStatus::Started:
			if ((*Node)->Timeout > 0.f)
			{
				const double NodeTimeout = ((*Node)->StartTime + (*Node)->Timeout) - Now;
				if (NodeTimeout <= 0.f)
				{
					++TimedOut;
					UE_LOG_STATEGRAPH(Log, TEXT("[%s.%s] Node timed out (Duration=%.6f)"), *Name.ToString(), *NodeName.ToString(), Now - (*Node)->StartTime);
					(*Node)->SetStatus(FStateGraphNode::EStatus::TimedOut);

					if (!StateGraphWeakPtr.IsValid())
					{
						return;
					}

					Node = GetNodeRef(NodeName);
					if (Node)
					{
						(*Node)->TimedOut();
						if (!StateGraphWeakPtr.IsValid())
						{
							return;
						}
					}

					break;
				}
				
				if (NextTimeout == 0.f || NodeTimeout < NextTimeout)
				{
					NextTimeout = NodeTimeout;
				}
			}

			++Running;
			break;

		case FStateGraphNode::EStatus::Completed:
			++Completed;
			break;

		case FStateGraphNode::EStatus::TimedOut:
			++TimedOut;
			break;

		default:
			UE_LOG_STATEGRAPH(Error, TEXT("[%s.%s] Unknown state"), *Name.ToString(), *NodeName.ToString());
			checkNoEntry();
			break;
		}
	}

	bRunning = false;
	UE_LOG_STATEGRAPH(Verbose, TEXT("[%s] Duration=%.6f Blocked=%d Started=%d Running=%d Completed=%d Removed=%d TimedOut=%d"),
		*Name.ToString(), Now - StartTime, Blocked, Started, Running, Completed, Removed, TimedOut);

	if (bRunAgain)
	{
		bRunAgain = false;
		LogDebugInfo();
		Run();
		return;
	}

	if (Status != EStatus::Running)
	{
		// State graph was reset or paused during loop, don't change Status.
	}
	else if (Started == 0 && Running == 0)
	{
		if (Blocked == 0 && TimedOut == 0)
		{
			CompletedTime = Now;
			UE_LOG_STATEGRAPH(Verbose, TEXT("[%s] Completed (Duration=%.6f)"), *Name.ToString(), CompletedTime - StartTime);
			SetStatus(EStatus::Completed);
		}
		else
		{
			UE_LOG_STATEGRAPH(Warning, TEXT("[%s] Blocked on %d nodes, timed out %d nodes"), *Name.ToString(), Blocked, TimedOut);
			LogDebugInfo(true);
			SetStatus(EStatus::Blocked);
		}
	}
	else
	{
		UE_LOG_STATEGRAPH(VeryVerbose, TEXT("[%s] Waiting on %d nodes"), *Name.ToString(), Started + Running);
		SetStatus(EStatus::Waiting);
	}

	if (!StateGraphWeakPtr.IsValid())
	{
		return;
	}

	if ((Status == EStatus::Blocked || Status == EStatus::Waiting) && NextTimeout > 0.f)
	{
		TimeoutTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSPLambda(this, [this](float DeltaTime) {
			Run();
			return false;
		}), NextTimeout);
	}

	LogDebugInfo();
}

void FStateGraph::Reset()
{
	// Keep a reference to check if the state graph is destroyed during external functions.
	FStateGraphWeakPtr StateGraphWeakPtr(AsWeak());

	UE_LOG_STATEGRAPH(Verbose, TEXT("[%s] Resetting state graph"), *Name.ToString());
	StartTime = 0.f;
	CompletedTime = 0.f;
	bRunAgain = false;
	SetStatus(EStatus::NotStarted);

	if (!StateGraphWeakPtr.IsValid())
	{
		return;
	}

	// Copy names since node map may be modified during loop.
	TArray<FName> NodeNames;
	Nodes.GenerateKeyArray(NodeNames);
	for (FName NodeName : NodeNames)
	{
		if (FStateGraphNodeRef* Node = GetNodeRef(NodeName))
		{
			(*Node)->Reset();
			if (!StateGraphWeakPtr.IsValid())
			{
				return;
			}
		}
	}
}

void FStateGraph::Pause()
{
	UE_LOG_STATEGRAPH(Verbose, TEXT("[%s] Pausing state graph"), *Name.ToString());
	bRunAgain = false;
	SetStatus(EStatus::Paused);
}

void FStateGraph::LogDebugInfo(bool bWarning)
{
	if (!bWarning && !UE_LOG_ACTIVE(LogStateGraph, VeryVerbose))
	{
		return;
	}

	FString Message = FString::Printf(TEXT("[%s] Status=%s Nodes=%d Duration=%.6f"), *Name.ToString(), GetStatusName(), Nodes.Num(), GetDuration());

	if (bWarning)
	{
		UE_LOG_STATEGRAPH(Warning, TEXT("%s"), *Message);
	}
	else
	{
		UE_LOG_STATEGRAPH(VeryVerbose, TEXT("%s"), *Message);
	}

	for (auto Node : Nodes)
	{
		TArray<FString> Missing;
		TArray<FString> Blocked;
		TArray<FString> Completed;
		TArray<FString> TimedOut;

		for (FName Dependency : Node.Value->Dependencies)
		{
			FStateGraphNodeRef* DependencyNode = GetNodeRef(Dependency);
			if (DependencyNode)
			{
				if ((*DependencyNode)->Status == FStateGraphNode::EStatus::Completed)
				{
					Completed.Add(Dependency.ToString());
				}
				else if ((*DependencyNode)->Status == FStateGraphNode::EStatus::Blocked)
				{
					Blocked.Add(Dependency.ToString());
				}
				else
				{
					TimedOut.Add(Dependency.ToString());
				}
			}
			else
			{
				Missing.Add(Dependency.ToString());
			}
		}

		TArray<FString> Dependencies;
		if (Missing.Num())
		{
			Dependencies.Add(TEXT("Missing=") + FString::Join(Missing, TEXT(",")));
		}
		if (Blocked.Num())
		{
			Dependencies.Add(TEXT("Blocked=") + FString::Join(Blocked, TEXT(",")));
		}
		if (Completed.Num())
		{
			Dependencies.Add(TEXT("Completed=") + FString::Join(Completed, TEXT(",")));
		}
		if (TimedOut.Num())
		{
			Dependencies.Add(TEXT("TimedOut=") + FString::Join(TimedOut, TEXT(",")));
		}
		if (Dependencies.Num() == 0)
		{
			Dependencies.Add(TEXT("None"));
		}

		Message = FString::Printf(TEXT("[%s.%s] Status=%s Duration=%.6f Dependencies(%s)"),
			*Name.ToString(), *Node.Key.ToString(), Node.Value->GetStatusName(), Node.Value->GetDuration(), *FString::Join(Dependencies, TEXT(" ")));

		if (bWarning)
		{
			UE_LOG_STATEGRAPH(Warning, TEXT("%s"), *Message);
		}
		else
		{
			UE_LOG_STATEGRAPH(VeryVerbose, TEXT("%s"), *Message);
		}
	}
}

void FStateGraph::SetStatus(EStatus NewStatus)
{
	if (Status != NewStatus)
	{
		const EStatus OldStatus = Status;
		Status = NewStatus;
		OnStatusChanged.Broadcast(*this, OldStatus, NewStatus);
	}
}

void FStateGraph::UpdateConfig()
{
	GConfig->GetDouble(*ConfigSectionName, TEXT("Timeout"), Timeout, GEngineIni);
}

void FStateGraph::OnConfigSectionsChanged(const FString& IniFilename, const TSet<FString>& SectionNames)
{
	if (IniFilename != GEngineIni)
	{
		return;
	}

	// Keep a reference to check if the state graph is destroyed during external functions.
	FStateGraphWeakPtr StateGraphWeakPtr(AsWeak());

	for (const FString& SectionName : SectionNames)
	{
		// Assume all node section names start with the state graph section name.
		if (!SectionName.StartsWith(ConfigSectionName))
		{
			continue;
		}

		if (SectionName.Len() == ConfigSectionName.Len())
		{
			UpdateConfig();
			continue;
		}

		// Copy names since node map may be modified during loop.
		TArray<FName> NodeNames;
		Nodes.GenerateKeyArray(NodeNames);
		for (FName NodeName : NodeNames)
		{
			FStateGraphNodeRef* Node = GetNodeRef(NodeName);
			if (Node)
			{
				if (SectionName == (*Node)->ConfigSectionName)
				{
					(*Node)->UpdateConfig();
					if (!StateGraphWeakPtr.IsValid())
					{
						return;
					}
				}
			}
		}
	}
}

} // UE
