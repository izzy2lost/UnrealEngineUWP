// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCore/RigVMGraphFunctionDefinition.h"
#include "RigVMCore/RigVMGraphFunctionHost.h"
#include "RigVMStringUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMGraphFunctionDefinition)

FRigVMExternalVariable FRigVMGraphFunctionArgument::GetExternalVariable() const
{
	FRigVMExternalVariable Variable;
	Variable.Name = Name;
	Variable.bIsArray = bIsArray;
	Variable.TypeName = CPPType;

	if(IsCPPTypeObjectValid())
	{
		Variable.TypeObject = CPPTypeObject.Get();
	}

	return Variable;
}

bool FRigVMGraphFunctionArgument::IsCPPTypeObjectValid() const
{
	if(!CPPTypeObject.IsValid())
	{
		// this is potentially a user defined struct or user defined enum
		// so we have to try to load it.
		(void)CPPTypeObject.LoadSynchronous();
	}
	return CPPTypeObject.IsValid();
}

bool FRigVMGraphFunctionArgument::IsExecuteContext() const
{
	if(IsCPPTypeObjectValid())
	{
		if(const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(CPPTypeObject.Get()))
		{
			if(ScriptStruct->IsChildOf(FRigVMExecuteContext::StaticStruct()))
			{
				return true;
			}
		}
	}
	return false;
}

bool FRigVMGraphFunctionHeader::IsMutable() const
{
	for(const FRigVMGraphFunctionArgument& Arg : Arguments)
	{
		if(Arg.IsCPPTypeObjectValid())
		{
			if(UScriptStruct* Struct = Cast<UScriptStruct>(Arg.CPPTypeObject.Get()))
			{
				if(Struct->IsChildOf(FRigVMExecuteContext::StaticStruct()))
				{
					return true;
				}
			}
		}
	}
	return false;
}

IRigVMGraphFunctionHost* FRigVMGraphFunctionHeader::GetFunctionHost(bool bLoadIfNecessary) const
{
	UObject* HostObj = LibraryPointer.HostObject.ResolveObject();
	if (!HostObj && bLoadIfNecessary)
	{
		HostObj = LibraryPointer.HostObject.TryLoad();
	}
	if (HostObj)
	{
		return Cast<IRigVMGraphFunctionHost>(HostObj);
	}
	return nullptr;
}

FRigVMGraphFunctionData* FRigVMGraphFunctionHeader::GetFunctionData(bool bLoadIfNecessary) const
{
	if (IRigVMGraphFunctionHost* Host = GetFunctionHost(bLoadIfNecessary))
	{
		return Host->GetRigVMGraphFunctionStore()->FindFunction(LibraryPointer);
	}
	return nullptr;
}

void FRigVMGraphFunctionHeader::PostDuplicateHost(const FString& InOldPathName, const FString& InNewPathName)
{
	const FString OldPathName = InOldPathName + TEXT(":");
	const FString NewPathName = InNewPathName + TEXT(":");

	auto ReplacePathName = [InOldPathName, InNewPathName, OldPathName, NewPathName](FString& InOutObjectPath)
	{
		if(InOutObjectPath.Equals(InOldPathName, ESearchCase::CaseSensitive))
		{
			InOutObjectPath = InNewPathName;
		}
		else if(InOutObjectPath.StartsWith(OldPathName, ESearchCase::CaseSensitive))
		{
			InOutObjectPath = NewPathName + InOutObjectPath.Mid(OldPathName.Len());
		}
	};
	
	auto ReplaceSoftPathName = [InOldPathName, InNewPathName, OldPathName, NewPathName, ReplacePathName](FSoftObjectPath& InOutObjectPath)
	{
		FString PathName = InOutObjectPath.ToString();
		ReplacePathName(PathName);
		InOutObjectPath = FSoftObjectPath(PathName);
	};

	ReplacePathName(LibraryPointer.LibraryNodePath);
	ReplaceSoftPathName(LibraryPointer.HostObject);
	for (TPair<FRigVMGraphFunctionIdentifier, uint32>& Pair : Dependencies)
	{
		ReplacePathName(Pair.Key.LibraryNodePath);
		ReplaceSoftPathName(Pair.Key.HostObject);
	}
}

FRigVMGraphFunctionHeader FRigVMGraphFunctionHeader::FindGraphFunctionHeader(const FString& InHostPath, const FName& InFunctionName, bool* bOutIsPublic, FString* OutErrorMessage)
{
	if(const FRigVMGraphFunctionData* FunctionData = FRigVMGraphFunctionData::FindFunctionData(InHostPath, InFunctionName, bOutIsPublic, OutErrorMessage))
	{
		return FunctionData->Header;
	}
	return FRigVMGraphFunctionHeader(); 
}

FRigVMGraphFunctionHeader FRigVMGraphFunctionHeader::FindGraphFunctionHeader(const FRigVMGraphFunctionIdentifier& InIdentifier, bool* bOutIsPublic, FString* OutErrorMessage)
{
	if(const FRigVMGraphFunctionData* FunctionData = FRigVMGraphFunctionData::FindFunctionData(InIdentifier, bOutIsPublic, OutErrorMessage))
	{
		return FunctionData->Header;
	}
	return FRigVMGraphFunctionHeader(); 
}

bool FRigVMGraphFunctionData::IsMutable() const
{
	return Header.IsMutable();
}

void FRigVMGraphFunctionData::PostDuplicateHost(const FString& InOldHostPathName, const FString& InNewHostPathName)
{
	Header.PostDuplicateHost(InOldHostPathName, InNewHostPathName);
}

FRigVMGraphFunctionData* FRigVMGraphFunctionData::FindFunctionData(const FString& InHostPath, const FName& InFunctionName, bool* bOutIsPublic, FString* OutErrorMessage)
{
	FRigVMGraphFunctionHeader InvalidHeader;

	UObject* HostObject = StaticLoadObject(UObject::StaticClass(), NULL, *InHostPath, NULL, LOAD_None, NULL);
	if (!HostObject)
	{
		if(OutErrorMessage)
		{
			*OutErrorMessage = FString::Printf(TEXT("Failed to load the Host object %s."), *InHostPath);
		}
		return nullptr;
	}

	IRigVMGraphFunctionHost* FunctionHost = Cast<IRigVMGraphFunctionHost>(HostObject);
	if (!FunctionHost)
	{
		if(OutErrorMessage)
		{
			*OutErrorMessage = TEXT("Host object is not a IRigVMGraphFunctionHost.");
		}
		return nullptr;
	}

	FRigVMGraphFunctionStore* FunctionStore = FunctionHost->GetRigVMGraphFunctionStore();
	if (!FunctionStore)
	{
		if(OutErrorMessage)
		{
			*OutErrorMessage = TEXT("Host object does not contain a function store.");
		}
		return nullptr;
	}

	FRigVMGraphFunctionData* Data = FunctionStore->FindFunctionByName(InFunctionName, bOutIsPublic);
	if (!Data)
	{
		if(OutErrorMessage)
		{
			*OutErrorMessage = FString::Printf(TEXT("Function %s not found in host %s."), *InFunctionName.ToString(), *InHostPath);
		}
		return nullptr;
	}

	return Data;
}

FRigVMGraphFunctionData* FRigVMGraphFunctionData::FindFunctionData(const FRigVMGraphFunctionIdentifier& InIdentifier, bool* bOutIsPublic, FString* OutErrorMessage)
{
	IRigVMGraphFunctionHost* FunctionHost = nullptr;
	if (UObject* FunctionHostObj = InIdentifier.HostObject.TryLoad())
	{
		FunctionHost = Cast<IRigVMGraphFunctionHost>(FunctionHostObj);									
	}
	else
	{
		if(OutErrorMessage)
		{
			*OutErrorMessage = FString::Printf(TEXT("Failed to load the Host object %s."), *InIdentifier.HostObject.ToString());
		}
		return nullptr;
	}

	if (!FunctionHost)
	{
		if(OutErrorMessage)
		{
			*OutErrorMessage = TEXT("Host object is not a IRigVMGraphFunctionHost.");
		}
		return nullptr;
	}

	FRigVMGraphFunctionStore* FunctionStore = FunctionHost->GetRigVMGraphFunctionStore();
	if (!FunctionStore)
	{
		if(OutErrorMessage)
		{
			*OutErrorMessage = TEXT("Host object does not contain a function store.");
		}
		return nullptr;
	}

	if(FRigVMGraphFunctionData* FunctionData = FunctionStore->FindFunction(InIdentifier, bOutIsPublic))
	{
		return FunctionData;
	}

	if(OutErrorMessage)
	{
		*OutErrorMessage = FString::Printf(TEXT("Function %s not found in host %s."), *InIdentifier.GetFunctionName(), *InIdentifier.HostObject.ToString());
	}
	return nullptr;
}


FString FRigVMGraphFunctionData::GetArgumentNameFromPinHash(const FString& InPinHash)
{
	FString Left, PinPath, PinName;
	if(RigVMStringUtils::SplitNodePathAtEnd(InPinHash, Left, PinPath))
	{
		Left.Reset();
		if(RigVMStringUtils::SplitPinPathAtStart(PinPath, Left, PinName))
		{
			if(Left.Equals(EntryString, ESearchCase::CaseSensitive) ||
				Left.Equals(ReturnString, ESearchCase::CaseSensitive))
			{
				return PinName;
			}
		}
	}
	return FString();
}

FRigVMOperand FRigVMGraphFunctionData::GetOperandForArgument(const FName& InArgumentName) const
{
	const FString InArgumentNameString = InArgumentName.ToString();
	for(const TPair<FString, FRigVMOperand>& Pair : CompilationData.Operands)
	{
		const FString ArgumentName = GetArgumentNameFromPinHash(Pair.Key);
		if(!ArgumentName.IsEmpty())
		{
			if(ArgumentName.Equals(InArgumentNameString, ESearchCase::CaseSensitive))
			{
				return Pair.Value;
			}
		}
	}
	return FRigVMOperand();
}

bool FRigVMGraphFunctionData::IsAnyOperandSharedAcrossArguments() const
{
	TSet<FRigVMOperand> UsedOperands;
	UsedOperands.Reserve(Header.Arguments.Num());
	for(const FRigVMGraphFunctionArgument& Argument : Header.Arguments)
	{
		if(Argument.IsExecuteContext())
		{
			continue;
		}
		
		const FRigVMOperand Operand = GetOperandForArgument(Argument.Name);
		if(!Operand.IsValid())
		{
			continue;
		}
		
		if(UsedOperands.Contains(Operand))
		{
			return true;
		}
		UsedOperands.Add(Operand);
	}
	return false;
}

bool FRigVMGraphFunctionData::PatchSharedArgumentOperandsIfRequired()
{
	// we are doing this to avoid output arguments of a function to share
	// memory. each output argument needs its own memory for the node
	// referencing the function to rely on.
	if(!IsAnyOperandSharedAcrossArguments())
	{
		return false;
	}

	// we'll keep doing this until there is no work left since
	// we need to shift all operand indices every time we change anything.
	bool bWorkLeft = true;
	while(bWorkLeft)
	{
		bWorkLeft = false;
		
		// create a list of arguments / operands to update
		TArray<TTuple<FName, FRigVMOperand>> ArgumentOperands;
		TMap<FRigVMOperand, TArray<FName>> OperandToArguments;
		ArgumentOperands.Reserve(Header.Arguments.Num());
		for(const FRigVMGraphFunctionArgument& Argument : Header.Arguments)
		{
			if(Argument.IsExecuteContext())
			{
				continue;
			}

			const FRigVMOperand Operand = GetOperandForArgument(Argument.Name);
			if(Operand.IsValid())
			{
				ArgumentOperands.Emplace(Argument.Name, Operand);
				OperandToArguments.FindOrAdd(Operand).Add(Argument.Name);
			}
		}

		// step 1: inject the properties and operands necessary to reflect the expected layout
		FRigVMOperand SourceOperand, TargetOperand;
		FString SourcePinPath, TargetPinPath;

		int32 ArgumentIndex = -1;
		for(int32 Index = 0; Index < Header.Arguments.Num(); Index++)
		{
			const FRigVMGraphFunctionArgument& Argument = Header.Arguments[Index];
			if(Argument.IsExecuteContext())
			{
				continue;
			}
			ArgumentIndex++;
			
			SourceOperand = GetOperandForArgument(Argument.Name);
			if(!SourceOperand.IsValid())
			{
				SourceOperand = FRigVMOperand();
				continue;
			}
			
			const TArray<FName>& ArgumentsSharingOperand = OperandToArguments.FindChecked(SourceOperand);
			if(ArgumentsSharingOperand.Num() == 1 || ArgumentsSharingOperand[0].IsEqual(Argument.Name, ENameCase::CaseSensitive))
			{
				continue;
			}

			check(SourceOperand.GetMemoryType() == ERigVMMemoryType::Work);

			// clone the property
			FRigVMFunctionCompilationPropertyDescription PropertyDescription = CompilationData.WorkPropertyDescriptions[SourceOperand.GetRegisterIndex()];

			for(const TPair<FString,FRigVMOperand>& Pair : CompilationData.Operands)
			{
				if(Pair.Value == SourceOperand)
				{
					SourcePinPath = Pair.Key;
					break;
				}
			}
			check(!SourcePinPath.IsEmpty());
			FString CompleteNodePath, NodePathPrefix, NodeName, PinName;
			verify(RigVMStringUtils::SplitPinPathAtEnd(SourcePinPath, CompleteNodePath, PinName));
			verify(RigVMStringUtils::SplitNodePathAtEnd(CompleteNodePath, NodePathPrefix, NodeName));
			if(Argument.Direction == ERigVMPinDirection::Input || Argument.Direction == ERigVMPinDirection::IO)
			{
				static const FString NewNodeName = TEXT("Entry");
				CompleteNodePath = RigVMStringUtils::JoinNodePath(NodePathPrefix, NewNodeName);
			}
			else
			{
				static const FString NewNodeName = TEXT("Return");
				CompleteNodePath = RigVMStringUtils::JoinNodePath(NodePathPrefix, NewNodeName);
			}
			TargetPinPath = RigVMStringUtils::JoinPinPath(CompleteNodePath, Argument.Name.ToString());
			PropertyDescription.Name = FRigVMPropertyDescription::SanitizeName(*TargetPinPath);

			int32 TargetIndex = ArgumentIndex;
			if(CompilationData.WorkPropertyDescriptions.IsValidIndex(ArgumentIndex))
			{
				TargetIndex = CompilationData.WorkPropertyDescriptions.Insert(PropertyDescription, ArgumentIndex);
			}
			else
			{
				TargetIndex = CompilationData.WorkPropertyDescriptions.Add(PropertyDescription);
			}

			TargetOperand = FRigVMOperand(SourceOperand.GetMemoryType(), TargetIndex, SourceOperand.GetRegisterOffset());
			bWorkLeft = true;
			break;
		}

		if(!bWorkLeft)
		{
			return true;
		}

		auto UpdateOperand = [TargetOperand](FRigVMOperand& Operand)
		{
			if(Operand.GetMemoryType() == TargetOperand.GetMemoryType())
			{
				if(Operand.GetRegisterIndex() >= TargetOperand.GetRegisterIndex())
				{
					Operand = FRigVMOperand(Operand.GetMemoryType(), Operand.GetRegisterIndex() + 1, Operand.GetRegisterOffset());
				}
			}
		};

		// step 2: update the property paths
		for(FRigVMFunctionCompilationPropertyPath& PropertyPath : CompilationData.WorkPropertyPathDescriptions)
		{
			if(PropertyPath.PropertyIndex != INDEX_NONE)
			{
				if(PropertyPath.PropertyIndex >= TargetOperand.GetRegisterIndex())
				{
					PropertyPath.PropertyIndex++;
				}
			}
		}

		// step 3: update the operands map
		for(TPair<FString, FRigVMOperand>& Pair : CompilationData.Operands)
		{
			UpdateOperand(Pair.Value);
		}
		CompilationData.Operands.FindOrAdd(TargetPinPath) = TargetOperand;

		// step 4: update the operands in the bytecode itself
		const FRigVMInstructionArray Instructions = CompilationData.ByteCode.GetInstructions();
		for(const FRigVMInstruction& Instruction : Instructions)
		{
			FRigVMOperandArray Operands = CompilationData.ByteCode.GetOperandsForOp(Instruction);
			for(int32 Index = 0; Index < Operands.Num(); Index++)
			{
				FRigVMOperand* Operand = const_cast<FRigVMOperand*>(&Operands[Index]);
				UpdateOperand(*Operand);
			}
		}

		// step 5: add copy operations at the end of the byte code
		CompilationData.ByteCode.AddCopyOp(SourceOperand, TargetOperand);
	}
	
	return true;
}
