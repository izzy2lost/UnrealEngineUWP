// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRig.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "Units/Execution/RigUnit_InverseExecution.h"
#include "Units/Execution/RigUnit_PrepareForExecution.h"
#include "Units/Execution/RigUnit_InteractionExecution.h"
#include "ControlRigObjectBinding.h"
#include "Rigs/RigHierarchyController.h"
#include "ControlRigComponent.h"

#define LOCTEXT_NAMESPACE "ControlRig"

UControlRig::UControlRig(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UControlRig::Execute_Internal(const FName& InEventName)
{
	if(IsRigModule())
	{
		FString ConnectorWarning;
		if(!AllConnectorsAreResolved(&ConnectorWarning))
		{
#if WITH_EDITOR
			LogOnce(EMessageSeverity::Warning, INDEX_NONE, ConnectorWarning);
#endif
			return false;
		}
	}
	
	if (VM)
	{
		FRigVMExtendedExecuteContext& Context = GetRigVMExtendedExecuteContext();

		static constexpr TCHAR InvalidatedVMFormat[] = TEXT("%s: Invalidated VM - aborting execution.");
		if(VM->IsNativized())
		{
			if(!IsValidLowLevel() ||
				!VM->IsValidLowLevel())
			{
				UE_LOG(LogControlRig, Warning, InvalidatedVMFormat, *GetClass()->GetName());
				return false;
			}
		}
		else
		{
			// sanity check the validity of the VM to ensure stability.
			if(!VM->IsContextValidForExecution(Context)
				|| !IsValidLowLevel()
				|| !VM->IsValidLowLevel()
			)
			{
				UE_LOG(LogControlRig, Warning, InvalidatedVMFormat, *GetClass()->GetName());
				return false;
			}
		}
		
#if UE_RIGVM_PROFILE_EXECUTE_UNITS_NUM
		const uint64 StartCycles = FPlatformTime::Cycles64();
		if(ProfilingRunsLeft <= 0)
		{
			ProfilingRunsLeft = UE_RIGVM_PROFILE_EXECUTE_UNITS_NUM;
			AccumulatedCycles = 0;
		}
#endif
		
		const bool bUseDebuggingSnapshots = !VM->IsNativized();
		
#if WITH_EDITOR
		if(bUseDebuggingSnapshots)
		{
			if(URigVM* SnapShotVM = GetSnapshotVM(false)) // don't create it for normal runs
			{
				const bool bIsEventFirstInQueue = !EventQueueToRun.IsEmpty() && EventQueueToRun[0] == InEventName; 
				const bool bIsEventLastInQueue = !EventQueueToRun.IsEmpty() && EventQueueToRun.Last() == InEventName;

				if (GetHaltedAtBreakpoint().IsValid())
				{
					if(bIsEventFirstInQueue)
					{
						CopyVMMemory(GetRigVMExtendedExecuteContext(), GetSnapshotContext());
					}
				}
				else if(bIsEventLastInQueue)
				{
					CopyVMMemory(GetSnapshotContext(), GetRigVMExtendedExecuteContext());
				}
			}
		}
#endif

		URigHierarchy* Hierarchy = GetHierarchy();
#if WITH_EDITOR

		bool bRecordTransformsAtRuntime = true;
		if(const UObject* Outer = GetOuter())
		{
			if(Outer->IsA<UControlRigComponent>())
			{
				bRecordTransformsAtRuntime = false;
			}
		}
		TGuardValue<bool> RecordTransformsPerInstructionGuard(Hierarchy->bRecordTransformsAtRuntime, bRecordTransformsAtRuntime);
		
		if(Hierarchy->bRecordTransformsAtRuntime)
		{
			Hierarchy->ReadTransformsAtRuntime.Reset();
			Hierarchy->WrittenTransformsAtRuntime.Reset();
		}
		
#endif
		FRigHierarchyExecuteContextBracket HierarchyContextGuard(Hierarchy, &Context);

		// setup the module information
		FControlRigExecuteContext& PublicContext = Context.GetPublicDataSafe<FControlRigExecuteContext>();
		FControlRigExecuteContextRigModuleGuard RigModuleGuard(PublicContext, this);
		FRigHierarchyRedirectorGuard ElementRedirectorGuard(this);

		const bool bSuccess = VM->ExecuteVM(Context, InEventName) != ERigVMExecuteResult::Failed;

#if UE_RIGVM_PROFILE_EXECUTE_UNITS_NUM
		const uint64 EndCycles = FPlatformTime::Cycles64();
		const uint64 Cycles = EndCycles - StartCycles;
		AccumulatedCycles += Cycles;
		ProfilingRunsLeft--;
		if(ProfilingRunsLeft == 0)
		{
			const double Milliseconds = FPlatformTime::ToMilliseconds64(AccumulatedCycles);
			UE_LOG(LogControlRig, Display, TEXT("%s: %d runs took %.03lfms."), *GetClass()->GetName(), UE_RIGVM_PROFILE_EXECUTE_UNITS_NUM, Milliseconds);
		}
#endif

		return bSuccess;
	}
	return false;
}

#if WITH_EDITOR
void UControlRig::SetFirstEntryEventInEventQueue(FRigVMExtendedExecuteContext& Context, const FName& InFirstEventName)
{
	VM->SetFirstEntryEventInEventQueue(Context, NAME_None);
}
#endif

#undef LOCTEXT_NAMESPACE