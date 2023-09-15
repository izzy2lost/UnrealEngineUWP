// Copyright Epic Games, Inc. All Rights Reserved.

#include "UncookedOnlyUtils.h"
#include "Graph/AnimNextGraph.h"
#include "Graph/AnimNextGraph_Controller.h"
#include "Graph/AnimNextGraph_EditorData.h"
#include "Graph/RigUnit_AnimNextDecoratorStack.h"
#include "Graph/RigUnit_AnimNextGraphRoot.h"
#include "Graph/RigUnit_AnimNextGraphEvaluator.h"
#include "Graph/RigUnit_AnimNextShimRoot.h"
#include "Graph/AnimNextExecuteContext.h"
#include "Param/AnimNextParameter.h"
#include "Param/AnimNextParameterBlock.h"
#include "Param/AnimNextParameterBlock_EditorData.h"
#include "Param/AnimNextParameterBlock_EdGraph.h"
#include "Graph/RigDecorator_AnimNextCppDecorator.h"
#include "Graph/RigUnit_AnimNextBeginExecution.h"
#include "Param/RigVMDispatch_SetParameter.h"
#include "Param/AnimNextParameterBlockEntry.h"
#include "Param/IAnimNextParameterBlockBindingInterface.h"
#include "DecoratorBase/DecoratorReader.h"
#include "DecoratorBase/DecoratorWriter.h"
#include "DecoratorBase/NodeTemplate.h"
#include "DecoratorBase/NodeTemplateBuilder.h"
#include "DecoratorBase/NodeTemplateRegistry.h"
#include "DecoratorBase/NodeDescription.h"
#include "DecoratorBase/NodeInstance.h"
#include "DecoratorBase/DecoratorRegistry.h"
#include "DecoratorBase/Decorator.h"
#include "Serialization/MemoryReader.h"

#include "RigVMCompiler/RigVMCompiler.h"
#include "RigVMCore/RigVM.h"

namespace UE::AnimNext::UncookedOnly
{
namespace Private
{
	struct FDecoratorEntryMapping
	{
		const URigVMNode* DecoratorStackNode;
		const URigVMPin* DecoratorEntryPin;
		const FDecorator* Decorator;

		FDecoratorEntryMapping(const URigVMNode* InDecoratorStackNode, const URigVMPin* InDecoratorEntryPin, const FDecorator* InDecorator)
			: DecoratorStackNode(InDecoratorStackNode)
			, DecoratorEntryPin(InDecoratorEntryPin)
			, Decorator(InDecorator)
		{}
	};

	struct FDecoratorStackMapping
	{
		const URigVMNode* DecoratorStackNode;
		TArray<FDecoratorEntryMapping> DecoratorEntries;

		FNodeHandle DecoratorStackNodeHandle;

		explicit FDecoratorStackMapping(const URigVMNode* InDecoratorStackNode)
			: DecoratorStackNode(InDecoratorStackNode)
		{}
	};

	template<typename DecoratorAction>
	void ForEachDecoratorInStack(const URigVMNode* DecoratorStackNode, const DecoratorAction& Action)
	{
		const TArray<URigVMPin*>& Pins = DecoratorStackNode->GetPins();
		for (URigVMPin* Pin : Pins)
		{
			if (!Pin->IsDecoratorPin())
			{
				continue;	// Not a decorator pin
			}

			if (Pin->GetScriptStruct() == FRigDecorator_AnimNextCppDecorator::StaticStruct())
			{
				TSharedPtr<FStructOnScope> DecoratorScope = Pin->GetDecoratorInstance();
				FRigDecorator_AnimNextCppDecorator* VMDecorator = (FRigDecorator_AnimNextCppDecorator*)DecoratorScope->GetStructMemory();

				if (const FDecorator* Decorator = VMDecorator->GetDecorator())
				{
					Action(DecoratorStackNode, Pin, Decorator);
				}
			}
		}
	}

	TArray<FDecoratorUID> GetDecoratorUIDs(const URigVMNode* DecoratorStackNode)
	{
		TArray<FDecoratorUID> Decorators;

		ForEachDecoratorInStack(DecoratorStackNode,
			[&Decorators](const URigVMNode* DecoratorStackNode, const URigVMPin* DecoratorPin, const FDecorator* Decorator)
			{
				Decorators.Add(Decorator->GetDecoratorUID());
			});

		return Decorators;
	}

	FNodeHandle RegisterDecoratorNodeTemplate(FDecoratorWriter& DecoratorWriter, const URigVMNode* DecoratorStackNode)
	{
		const TArray<FDecoratorUID> DecoratorUIDs = GetDecoratorUIDs(DecoratorStackNode);

		TArray<uint8> NodeTemplateBuffer;
		const FNodeTemplate* NodeTemplate = FNodeTemplateBuilder::BuildNodeTemplate(DecoratorUIDs, NodeTemplateBuffer);

		return DecoratorWriter.RegisterNode(*NodeTemplate);
	}

	FString GetDecoratorProperty(const FDecoratorStackMapping& DecoratorStack, uint32 DecoratorIndex, const FString& PropertyName, const TArray<FDecoratorStackMapping>& DecoratorStackNodes)
	{
		const TArray<URigVMPin*>& Pins = DecoratorStack.DecoratorEntries[DecoratorIndex].DecoratorEntryPin->GetSubPins();
		for (const URigVMPin* Pin : Pins)
		{
			if (Pin->GetDirection() != ERigVMPinDirection::Input)
			{
				continue;	// We only look for input pins
			}

			if (Pin->GetName() == PropertyName)
			{
				if (Pin->GetCPPTypeObject() == FAnimNextDecoratorHandle::StaticStruct())
				{
					// Decorator handle pins don't have a value, just an optional link
					const TArray<URigVMLink*>& PinLinks = Pin->GetLinks();
					if (PinLinks.Num() != 0)
					{
						// Something is connected to us, find the corresponding node handle so that we can encode it as our property value
						check(PinLinks.Num() == 1);

						const URigVMNode* SourceNode = PinLinks[0]->GetSourceNode();

						FNodeHandle SourceNodeHandle;
						int32 SourceDecoratorIndex = INDEX_NONE;

						const FDecoratorStackMapping* SourceDecoratorStack = DecoratorStackNodes.FindByPredicate([SourceNode](const FDecoratorStackMapping& Mapping) { return Mapping.DecoratorStackNode == SourceNode; });
						if (SourceDecoratorStack != nullptr)
						{
							SourceNodeHandle = SourceDecoratorStack->DecoratorStackNodeHandle;
							SourceDecoratorIndex = 0;	// We always bind to the first decorator index since we only allow one base decorator per stack for now
						}

						if (SourceNodeHandle.IsValid())
						{
							check(SourceDecoratorIndex != INDEX_NONE);

							const FAnimNextDecoratorHandle DecoratorHandle(SourceNodeHandle, SourceDecoratorIndex);
							const FAnimNextDecoratorHandle DefaultDecoratorHandle;

							// We need an instance of a decorator handle property to be able to serialize it into text, grab it from the root
							const FProperty* Property = FRigUnit_AnimNextGraphRoot::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_AnimNextGraphRoot, Result));

							FString PropertyValue;
							Property->ExportText_Direct(PropertyValue, &DecoratorHandle, &DefaultDecoratorHandle, nullptr, PPF_None);

							return PropertyValue;
						}
					}

					// This handle pin isn't connected
					return FString();
				}

				// A regular property pin
				return Pin->GetDefaultValue();
			}
		}

		// Unknown property
		return FString();
	}

	bool IsDecoratorPropertyLatent(const FDecoratorStackMapping& DecoratorStack, uint32 DecoratorIndex, const FString& PropertyName, const TArray<FDecoratorStackMapping>& DecoratorStackNodes)
	{
		const TArray<URigVMPin*>& Pins = DecoratorStack.DecoratorEntries[DecoratorIndex].DecoratorEntryPin->GetSubPins();
		for (const URigVMPin* Pin : Pins)
		{
			if (Pin->GetDirection() != ERigVMPinDirection::Input)
			{
				continue;	// We only look for input pins
			}

			if (Pin->GetName() == PropertyName)
			{
				// Lazy pins are latent if they are connected to something
				return Pin->IsLazy() && !Pin->GetLinks().IsEmpty();
			}
		}

		// Unknown property
		return false;
	}

	void WriteDecoratorProperties(FDecoratorWriter& DecoratorWriter, const FDecoratorStackMapping& Mapping, const TArray<FDecoratorStackMapping>& DecoratorStackNodes)
	{
		DecoratorWriter.WriteNode(Mapping.DecoratorStackNodeHandle,
			[&Mapping, &DecoratorStackNodes](uint32 DecoratorIndex, const FString& PropertyName)
			{
				return GetDecoratorProperty(Mapping, DecoratorIndex, PropertyName, DecoratorStackNodes);
			},
			[&Mapping, &DecoratorStackNodes](uint32 DecoratorIndex, const FString& PropertyName)
			{
				return IsDecoratorPropertyLatent(Mapping, DecoratorIndex, PropertyName, DecoratorStackNodes);
			});
	}

	const URigVMUnitNode* FindRootNode(const TArray<URigVMNode*>& VMNodes)
	{
		for (const URigVMNode* VMNode : VMNodes)
		{
			if (const URigVMUnitNode* VMUnitNode = Cast<URigVMUnitNode>(VMNode))
			{
				const UScriptStruct* ScriptStruct = VMUnitNode->GetScriptStruct();
				if (ScriptStruct == FRigUnit_AnimNextGraphRoot::StaticStruct())
				{
					return VMUnitNode;
				}
			}
		}

		return nullptr;
	}

	TArray<FDecoratorStackMapping> CollectDecoratorStacks(const URigVMGraph* VMGraph)
	{
		const TArray<URigVMNode*>& VMNodes = VMGraph->GetNodes();
		const URigVMUnitNode* VMRootNode = FindRootNode(VMNodes);

		TArray<FDecoratorStackMapping> DecoratorStackNodes;

		if (VMRootNode == nullptr)
		{
			return DecoratorStackNodes;
		}

		TArray<const URigVMNode*> NodesToVisit;
		NodesToVisit.Add(VMRootNode);

		while (NodesToVisit.Num() != 0)
		{
			const URigVMNode* VMNode = NodesToVisit[0];
			NodesToVisit.RemoveAt(0);

			if (const URigVMUnitNode* VMUnitNode = Cast<URigVMUnitNode>(VMNode))
			{
				const UScriptStruct* ScriptStruct = VMUnitNode->GetScriptStruct();
				if (ScriptStruct == FRigUnit_AnimNextDecoratorStack::StaticStruct())
				{
					FDecoratorStackMapping Mapping(VMNode);
					ForEachDecoratorInStack(VMNode,
						[&Mapping](const URigVMNode* DecoratorStackNode, const URigVMPin* DecoratorPin, const FDecorator* Decorator)
						{
							Mapping.DecoratorEntries.Add(FDecoratorEntryMapping(DecoratorStackNode, DecoratorPin, Decorator));
						});

					DecoratorStackNodes.Add(MoveTemp(Mapping));
				}
			}

			const TArray<URigVMNode*> SourceNodes = VMNode->GetLinkedSourceNodes();
			NodesToVisit.Append(SourceNodes);
		}

		return DecoratorStackNodes;
	}

	FRigVMPinInfoArray CollectLatentPins(const TArray<FDecoratorStackMapping>& DecoratorStackNodes, TMap<FName, URigVMPin*>& LatentPinMapping)
	{
		const FRigVMRegistry& Registry = FRigVMRegistry::Get();

		FRigVMPinInfoArray LatentPins;

		for (const FDecoratorStackMapping& DecoratorStack : DecoratorStackNodes)
		{
			for (const FDecoratorEntryMapping& DecoratorEntry : DecoratorStack.DecoratorEntries)
			{
				for (URigVMPin* Pin : DecoratorEntry.DecoratorEntryPin->GetSubPins())
				{
					if (Pin->IsLazy() && !Pin->GetLinks().IsEmpty())
					{
						// This pin has something linked to it, it is a latent pin
						const FName LatentPinName(TEXT("LatentPin"), LatentPins.Num());	// Create unique latent pin names

						FRigVMPinInfo PinInfo;
						PinInfo.Name = LatentPinName;
						PinInfo.TypeIndex = Pin->GetTypeIndex();

						// All our programmatic pins are lazy inputs
						PinInfo.Direction = ERigVMPinDirection::Input;
						PinInfo.bIsLazy = true;

						LatentPins.Pins.Emplace(PinInfo);

						const TArray<URigVMLink*>& PinLinks = Pin->GetLinks();
						check(PinLinks.Num() == 1);

						LatentPinMapping.Add(LatentPinName, PinLinks[0]->GetSourcePin());
					}
				}
			}
		}

		return LatentPins;
	}

	TArray<FRigVMFunctionArgument> GetGraphEvaluatorFunctionArguments(const FRigVMPinInfoArray& LatentPins)
	{
		const FRigVMRegistry& Registry = FRigVMRegistry::Get();

		TArray<FRigVMFunctionArgument> Arguments;
		Arguments.Reserve(LatentPins.Num());

		for (const FRigVMPinInfo& Pin : LatentPins)
		{
			const FRigVMTemplateArgumentType& TypeArg = Registry.GetType(Pin.TypeIndex);

			Arguments.Add(FRigVMFunctionArgument(Pin.Name.ToString(), TypeArg.CPPType.ToString(), ERigVMFunctionArgumentDirection::Input));
		}

		return Arguments;
	}

	FString GetGraphEvaluatorMethodName(const FRigVMPinInfoArray& LatentPins)
	{
		static TMap<uint32, FString> GraphEvaluatorMethodNameCache;

		const uint32 LatentPinListHash = GetTypeHash(LatentPins);
		if (const FString* MethodName = GraphEvaluatorMethodNameCache.Find(LatentPinListHash))
		{
			return *MethodName;
		}

		// Generate a new method for this argument list
		const FString MethodName = FString::Printf(TEXT("Execute_%X"), LatentPinListHash);
		const FString FullExecuteMethodName = FString::Printf(TEXT("FRigUnit_AnimNextGraphEvaluator::%s"), *MethodName);

		const TArray<FRigVMFunctionArgument> GraphEvaluatorArguments = GetGraphEvaluatorFunctionArguments(LatentPins);
		FRigVMRegistry::Get().Register(*FullExecuteMethodName, &FRigUnit_AnimNextGraphEvaluator::StaticExecute, FRigUnit_AnimNextGraphEvaluator::StaticStruct(), GraphEvaluatorArguments);

		// Cache our result
		GraphEvaluatorMethodNameCache.Add(LatentPinListHash, MethodName);

		return MethodName;
	}
}

void FUtils::Compile(UAnimNextGraph* InGraph)
{
	check(InGraph);
	
	UAnimNextGraph_EditorData* EditorData = GetEditorData(InGraph);
	
	if(EditorData->bIsCompiling)
	{
		return;
	}
	
	TGuardValue<bool> CompilingGuard(EditorData->bIsCompiling, true);
	
	EditorData->bErrorsDuringCompilation = false;

	EditorData->RigGraphDisplaySettings.MinMicroSeconds = EditorData->RigGraphDisplaySettings.LastMinMicroSeconds = DBL_MAX;
	EditorData->RigGraphDisplaySettings.MaxMicroSeconds = EditorData->RigGraphDisplaySettings.LastMaxMicroSeconds = (double)INDEX_NONE;
	
	TGuardValue<bool> ReentrantGuardSelf(EditorData->bSuspendModelNotificationsForSelf, true);
	TGuardValue<bool> ReentrantGuardOthers(EditorData->bSuspendModelNotificationsForOthers, true);

	RecreateVM(InGraph);

	InGraph->VMRuntimeSettings = EditorData->VMRuntimeSettings;

	EditorData->CompileLog.Messages.Reset();
	EditorData->CompileLog.NumErrors = EditorData->CompileLog.NumWarnings = 0;

	// We use a temporary graph model to build our final graph that we'll compile
	FRigVMClient* VMClient = EditorData->GetRigVMClient();
	URigVMGraph* VMRootGraph = VMClient->GetDefaultModel();
	URigVMGraph* VMTempGraph = CastChecked<URigVMGraph>(StaticDuplicateObject(VMRootGraph, GetTransientPackage(), VMClient->GetUniqueName(TEXT("TempRigVMGraph"))));

	UAnimNextGraph_Controller* TempController = CastChecked<UAnimNextGraph_Controller>(VMClient->GetOrCreateController(VMTempGraph));

	// Gather our decorator stacks
	TArray<Private::FDecoratorStackMapping> DecoratorStackNodes = Private::CollectDecoratorStacks(VMTempGraph);

	// Add our runtime shim root node
	URigVMUnitNode* TempShimRootNode = TempController->AddUnitNode(FRigUnit_AnimNextShimRoot::StaticStruct(), FRigVMStruct::ExecuteName, FVector2D::ZeroVector, FString(), false);

	// Add our graph evaluator node
	TMap<FName, URigVMPin*> LatentPinMapping;
	const FRigVMPinInfoArray LatentPins = Private::CollectLatentPins(DecoratorStackNodes, LatentPinMapping);

	// We need a unique method name to match our unique argument list
	const FString ExecuteMethodName = Private::GetGraphEvaluatorMethodName(LatentPins);

	URigVMUnitNode* GraphEvaluatorNode = TempController->AddUnitNodeWithPins(FRigUnit_AnimNextGraphEvaluator::StaticStruct(), LatentPins, *ExecuteMethodName, FVector2D::ZeroVector, FString(), false);

	// Link our shim and evaluator nodes together using the execution context
	TempController->AddLink(
		TempShimRootNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_AnimNextShimRoot, ExecuteContext)),
		GraphEvaluatorNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_AnimNextGraphEvaluator, ExecuteContext)));

	// Link our latent pins
	for (const FRigVMPinInfo& LatentPin : LatentPins)
	{
		TempController->AddLink(
			LatentPinMapping[LatentPin.Name],
			GraphEvaluatorNode->FindPin(LatentPin.Name.ToString()));
	}

	FDecoratorWriter DecoratorWriter;

	// Iterate over every decorator stack and register our node templates
	for (Private::FDecoratorStackMapping& NodeMapping : DecoratorStackNodes)
	{
		NodeMapping.DecoratorStackNodeHandle = Private::RegisterDecoratorNodeTemplate(DecoratorWriter, NodeMapping.DecoratorStackNode);
	}

	// Write our node shared data
	DecoratorWriter.BeginNodeWriting();

	for (const Private::FDecoratorStackMapping& NodeMapping : DecoratorStackNodes)
	{
		Private::WriteDecoratorProperties(DecoratorWriter, NodeMapping, DecoratorStackNodes);
	}

	DecoratorWriter.EndNodeWriting();

	// Find our root node handle, if we have any stack nodes, the first one is our root stack
	FAnimNextEntryPointHandle RootDecoratorHandle;
	if (DecoratorStackNodes.Num() != 0)
	{
		RootDecoratorHandle = FAnimNextEntryPointHandle(DecoratorStackNodes[0].DecoratorStackNodeHandle);
	}

	// Cache our compiled metadata
	InGraph->SharedDataArchiveBuffer = DecoratorWriter.GetGraphSharedData();
	InGraph->RootDecoratorHandle = RootDecoratorHandle;

	// Populate our runtime metadata
	InGraph->LoadFromArchiveBuffer(InGraph->SharedDataArchiveBuffer);

	// Remove our old root node
	if (URigVMNode* const* RootNode = VMTempGraph->GetNodes().FindByPredicate(
		[](URigVMNode* Node)
		{
			if (URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(Node))
			{
				return UnitNode->GetScriptStruct() == FRigUnit_AnimNextGraphRoot::StaticStruct();
			}

			return false;
		}))
	{
		TempController->RemoveNode(*RootNode, false, false);
	}

	URigVMCompiler* Compiler = URigVMCompiler::StaticClass()->GetDefaultObject<URigVMCompiler>();
	EditorData->VMCompileSettings.SetExecuteContextStruct(EditorData->RigVMClient.GetExecuteContextStruct());
	const FRigVMCompileSettings Settings = (EditorData->bCompileInDebugMode) ? FRigVMCompileSettings::Fast(EditorData->VMCompileSettings.GetExecuteContextStruct()) : EditorData->VMCompileSettings;
	Compiler->Compile(Settings, { VMTempGraph }, TempController, InGraph->RigVM, InGraph->ExtendedExecuteContext, InGraph->GetRigVMExternalVariables(), & EditorData->PinToOperandMap);

	// Initialize right away, in packaged builds we initialize during PostLoad
	InGraph->RigVM->Initialize(InGraph->ExtendedExecuteContext, InGraph->RigVM->GetLocalMemoryArray(InGraph->ExtendedExecuteContext));

	if (EditorData->bErrorsDuringCompilation)
	{
		if(Settings.SurpressErrors)
		{
			Settings.Reportf(EMessageSeverity::Info, InGraph,TEXT("Compilation Errors may be suppressed for AnimNext Interface Graph: %s. See VM Compile Settings for more Details"), *InGraph->GetName());
		}
	}

	EditorData->bVMRecompilationRequired = false;
	if(InGraph->RigVM)
	{
		EditorData->VMCompiledEvent.Broadcast(InGraph, InGraph->RigVM, InGraph->ExtendedExecuteContext);
	}

	VMClient->RemoveController(VMTempGraph);

#if WITH_EDITOR
//	RefreshBreakpoints(EditorData);
#endif
}

void FUtils::RecreateVM(UAnimNextGraph* InGraph)
{
	InGraph->RigVM = NewObject<URigVM>(InGraph, TEXT("VM"), RF_NoFlags);

	// Cooked platforms will load these pointers from disk
	if (!FPlatformProperties::RequiresCookedData())
	{
		InGraph->RigVM->CreateMemoryByType(InGraph->ExtendedExecuteContext, ERigVMMemoryType::Work);
		InGraph->RigVM->CreateMemoryByType(InGraph->ExtendedExecuteContext, ERigVMMemoryType::Literal);
		InGraph->RigVM->CreateMemoryByType(InGraph->ExtendedExecuteContext, ERigVMMemoryType::Debug);
	}

	InGraph->RigVM->Reset(InGraph->ExtendedExecuteContext);
}

UAnimNextGraph_EditorData* FUtils::GetEditorData(const UAnimNextGraph* InAnimNextGraph)
{
	check(InAnimNextGraph);
	
	return CastChecked<UAnimNextGraph_EditorData>(InAnimNextGraph->EditorData);
}

FParamTypeHandle FUtils::GetParameterHandleFromPin(const FEdGraphPinType& InPinType)
{
	FAnimNextParamType::EValueType ValueType = FAnimNextParamType::EValueType::None;
	FAnimNextParamType::EContainerType ContainerType = FAnimNextParamType::EContainerType::None;
	UObject* ValueTypeObject = nullptr;

	if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
	{
		ValueType = FAnimNextParamType::EValueType::Bool;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Byte)
	{
		ValueType = FAnimNextParamType::EValueType::Byte;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Int)
	{
		ValueType = FAnimNextParamType::EValueType::Int32;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Int64)
	{
		ValueType = FAnimNextParamType::EValueType::Int64;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Real)
	{
		if (InPinType.PinSubCategory == UEdGraphSchema_K2::PC_Float)
		{
			ValueType = FAnimNextParamType::EValueType::Float;
		}
		else if (InPinType.PinSubCategory == UEdGraphSchema_K2::PC_Double)
		{
			ValueType = FAnimNextParamType::EValueType::Double;
		}
		else
		{
			ensure(false);	// Reals should be either floats or doubles
		}
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Float)
	{
		ValueType = FAnimNextParamType::EValueType::Float;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Double)
	{
		ValueType = FAnimNextParamType::EValueType::Double;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Name)
	{
		ValueType = FAnimNextParamType::EValueType::Name;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_String)
	{
		ValueType = FAnimNextParamType::EValueType::String;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Text)
	{
		ValueType = FAnimNextParamType::EValueType::Text;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Enum)
	{
		ValueType = FAnimNextParamType::EValueType::Enum;
		ValueTypeObject = InPinType.PinSubCategoryObject.Get();
		ensure(ValueTypeObject);
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
	{
		ValueType = FAnimNextParamType::EValueType::Struct;
		ValueTypeObject = Cast<UScriptStruct>(InPinType.PinSubCategoryObject.Get());
		ensure(ValueTypeObject);
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Object)
	{
		ValueType = FAnimNextParamType::EValueType::Object;
		ValueTypeObject = Cast<UClass>(InPinType.PinSubCategoryObject.Get());
		ensure(ValueTypeObject);
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_SoftObject)
	{
		ValueType = FAnimNextParamType::EValueType::SoftObject;
		ValueTypeObject = Cast<UClass>(InPinType.PinSubCategoryObject.Get());
		ensure(ValueTypeObject);
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_SoftClass)
	{
		ValueType = FAnimNextParamType::EValueType::SoftClass;
		ValueTypeObject = Cast<UClass>(InPinType.PinSubCategoryObject.Get());
		ensure(ValueTypeObject);
	}

	if(InPinType.ContainerType == EPinContainerType::Array)
	{
		ContainerType = FAnimNextParamType::EContainerType::Array;
	}
	else if(InPinType.ContainerType == EPinContainerType::Set)
	{
		ensureMsgf(false, TEXT("Set pins are not yet supported"));
	}
	if(InPinType.ContainerType == EPinContainerType::Map)
	{
		ensureMsgf(false, TEXT("Map pins are not yet supported"));
	}
	
	return FAnimNextParamType(ValueType, ContainerType, ValueTypeObject).GetHandle();
}

void FUtils::CompileVM(UAnimNextParameterBlock* InParameterBlock)
{
	check(InParameterBlock);

	UAnimNextParameterBlock_EditorData* EditorData = FUtils::GetEditorData(InParameterBlock);
	if(EditorData->bIsCompiling)
	{
		return;
	}

	TGuardValue<bool> CompilingGuard(EditorData->bIsCompiling, true);

	EditorData->bErrorsDuringCompilation = false;

	EditorData->RigGraphDisplaySettings.MinMicroSeconds = EditorData->RigGraphDisplaySettings.LastMinMicroSeconds = DBL_MAX;
	EditorData->RigGraphDisplaySettings.MaxMicroSeconds = EditorData->RigGraphDisplaySettings.LastMaxMicroSeconds = (double)INDEX_NONE;
	
	TGuardValue<bool> ReentrantGuardSelf(EditorData->bSuspendModelNotificationsForSelf, true);
	TGuardValue<bool> ReentrantGuardOthers(EditorData->bSuspendModelNotificationsForOthers, true);

	RecreateVM(InParameterBlock);

	InParameterBlock->VMRuntimeSettings = EditorData->VMRuntimeSettings;

	EditorData->CompileLog.Messages.Reset();
	EditorData->CompileLog.NumErrors = EditorData->CompileLog.NumWarnings = 0;

	URigVMCompiler* Compiler = URigVMCompiler::StaticClass()->GetDefaultObject<URigVMCompiler>();
	EditorData->VMCompileSettings.SetExecuteContextStruct(EditorData->RigVMClient.GetExecuteContextStruct());
	const FRigVMCompileSettings Settings = (EditorData->bCompileInDebugMode) ? FRigVMCompileSettings::Fast(EditorData->VMCompileSettings.GetExecuteContextStruct()) : EditorData->VMCompileSettings;
	URigVMController* RootController = EditorData->GetRigVMClient()->GetOrCreateController(EditorData->GetRigVMClient()->GetDefaultModel());
	Compiler->Compile(Settings, EditorData->GetRigVMClient()->GetAllModels(false, false), RootController, InParameterBlock->RigVM, InParameterBlock->ExtendedExecuteContext, InParameterBlock->GetRigVMExternalVariables(), &EditorData->PinToOperandMap);

	if (EditorData->bErrorsDuringCompilation)
	{
		if(Settings.SurpressErrors)
		{
			Settings.Reportf(EMessageSeverity::Info, InParameterBlock, TEXT("Compilation Errors may be suppressed for AnimNext Interface Graph: %s. See VM Compile Settings for more Details"), *InParameterBlock->GetName());
		}
	}

	EditorData->bVMRecompilationRequired = false;
	if(InParameterBlock->RigVM)
	{
		EditorData->RigVMCompiledEvent.Broadcast(InParameterBlock, InParameterBlock->RigVM, InParameterBlock->ExtendedExecuteContext);
	}

#if WITH_EDITOR
//	RefreshBreakpoints(EditorData);
#endif
}

void FUtils::CompileStruct(UAnimNextParameterBlock* InParameterBlock)
{
	check(InParameterBlock);

	UAnimNextParameterBlock_EditorData* EditorData = GetEditorData(InParameterBlock);
	if(EditorData->bIsCompiling)
	{
		return;
	}

	TGuardValue<bool> CompilingGuard(EditorData->bIsCompiling, true);

	FInstancedPropertyBag& PropertyBag = InParameterBlock->PropertyBag;
	PropertyBag.Reset();

	TArray<FPropertyBagPropertyDesc> PropertyDescs;
	PropertyDescs.Reserve(EditorData->Entries.Num());
	
	// Gather all properties in this block
	for(const UAnimNextParameterBlockEntry* Entry : EditorData->Entries)
	{
		if(const IAnimNextParameterBlockBindingInterface* Binding = Cast<IAnimNextParameterBlockBindingInterface>(Entry))
		{
			if(const UAnimNextParameter* Parameter = Binding->GetParameter())
			{
				const FAnimNextParamType& Type = Binding->GetParamType();
				PropertyDescs.Emplace(Parameter->GetFName(), Type.GetContainerType(), Type.GetValueType(), Type.GetValueTypeObject());
			}
		}
	}

	// Bulk add to the bag
	PropertyBag.AddProperties(PropertyDescs);

	// TODO: Now copy over defaults for those properties that need it (literals)

	EditorData->bStructRecompilationRequired = false;
}

void FUtils::Compile(UAnimNextParameterBlock* InParameterBlock)
{
	check(InParameterBlock);

	CompileStruct(InParameterBlock);
	CompileVM(InParameterBlock);
}

void FUtils::RecreateVM(UAnimNextParameterBlock* InParameterBlock)
{
	InParameterBlock->RigVM = NewObject<URigVM>(InParameterBlock, TEXT("VM"), RF_NoFlags);

	// Cooked platforms will load these pointers from disk
	if (!FPlatformProperties::RequiresCookedData())
	{
		// We dont support ERigVMMemoryType::Work memory as we dont operate on an instance
	//	InParameterBlock->RigVM->GetMemoryByType(ERigVMMemoryType::Work, true);
		InParameterBlock->RigVM->CreateMemoryByType(InParameterBlock->ExtendedExecuteContext, ERigVMMemoryType::Literal);
		InParameterBlock->RigVM->CreateMemoryByType(InParameterBlock->ExtendedExecuteContext, ERigVMMemoryType::Debug);
	}

	InParameterBlock->RigVM->Reset(InParameterBlock->ExtendedExecuteContext);
}

UAnimNextParameterBlock_EditorData* FUtils::GetEditorData(const UAnimNextParameterBlock* InParameterBlock)
{
	check(InParameterBlock);

	return CastChecked<UAnimNextParameterBlock_EditorData>(InParameterBlock->EditorData);
}

UAnimNextParameterBlock* FUtils::GetBlock(const UAnimNextParameterBlock_EditorData* InEditorData)
{
	check(InEditorData);

	return CastChecked<UAnimNextParameterBlock>(InEditorData->GetOuter());
}

FParamTypeHandle FUtils::GetParamTypeHandleFromPinType(const FEdGraphPinType& InPinType)
{
	return GetParamTypeFromPinType(InPinType).GetHandle();
}

FAnimNextParamType FUtils::GetParamTypeFromPinType(const FEdGraphPinType& InPinType)
{
	FAnimNextParamType::EValueType ValueType = FAnimNextParamType::EValueType::None;
	FAnimNextParamType::EContainerType ContainerType = FAnimNextParamType::EContainerType::None;
	UObject* ValueTypeObject = nullptr;

	if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
	{
		ValueType = FAnimNextParamType::EValueType::Bool;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Byte)
	{
		ValueType = FAnimNextParamType::EValueType::Byte;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Int)
	{
		ValueType = FAnimNextParamType::EValueType::Int32;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Int64)
	{
		ValueType = FAnimNextParamType::EValueType::Int64;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Real)
	{
		if (InPinType.PinSubCategory == UEdGraphSchema_K2::PC_Float)
		{
			ValueType = FAnimNextParamType::EValueType::Float;
		}
		else if (InPinType.PinSubCategory == UEdGraphSchema_K2::PC_Double)
		{
			ValueType = FAnimNextParamType::EValueType::Double;
		}
		else
		{
			ensure(false);	// Reals should be either floats or doubles
		}
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Float)
	{
		ValueType = FAnimNextParamType::EValueType::Float;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Double)
	{
		ValueType = FAnimNextParamType::EValueType::Double;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Name)
	{
		ValueType = FAnimNextParamType::EValueType::Name;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_String)
	{
		ValueType = FAnimNextParamType::EValueType::String;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Text)
	{
		ValueType = FAnimNextParamType::EValueType::Text;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Enum)
	{
		ValueType = FAnimNextParamType::EValueType::Enum;
		ValueTypeObject = InPinType.PinSubCategoryObject.Get();
		ensure(ValueTypeObject);
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
	{
		ValueType = FAnimNextParamType::EValueType::Struct;
		ValueTypeObject = Cast<UScriptStruct>(InPinType.PinSubCategoryObject.Get());
		ensure(ValueTypeObject);
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Object)
	{
		ValueType = FAnimNextParamType::EValueType::Object;
		ValueTypeObject = Cast<UClass>(InPinType.PinSubCategoryObject.Get());
		ensure(ValueTypeObject);
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_SoftObject)
	{
		ValueType = FAnimNextParamType::EValueType::SoftObject;
		ValueTypeObject = Cast<UClass>(InPinType.PinSubCategoryObject.Get());
		ensure(ValueTypeObject);
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_SoftClass)
	{
		ValueType = FAnimNextParamType::EValueType::SoftClass;
		ValueTypeObject = Cast<UClass>(InPinType.PinSubCategoryObject.Get());
		ensure(ValueTypeObject);
	}

	if(InPinType.ContainerType == EPinContainerType::Array)
	{
		ContainerType = FAnimNextParamType::EContainerType::Array;
	}
	else if(InPinType.ContainerType == EPinContainerType::Set)
	{
		ensureMsgf(false, TEXT("Set pins are not yet supported"));
	}
	if(InPinType.ContainerType == EPinContainerType::Map)
	{
		ensureMsgf(false, TEXT("Map pins are not yet supported"));
	}
	
	return FAnimNextParamType(ValueType, ContainerType, ValueTypeObject);
}

FEdGraphPinType FUtils::GetPinTypeFromParamTypeHandle(const FParamTypeHandle& InParamTypeHandle)
{
	return GetPinTypeFromParamType(InParamTypeHandle.GetType());
}

FEdGraphPinType FUtils::GetPinTypeFromParamType(const FAnimNextParamType& InParamType)
{
	FEdGraphPinType PinType;
	PinType.PinSubCategory = NAME_None;

	// Container type
	switch (InParamType.ContainerType)
	{
	case FAnimNextParamType::EContainerType::Array:
		PinType.ContainerType = EPinContainerType::Array;
		break;
	default:
		PinType.ContainerType = EPinContainerType::None;
	}

	// Value type
	switch (InParamType.ValueType)
	{
	case EPropertyBagPropertyType::Bool:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		break;
	case EPropertyBagPropertyType::Byte:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
		break;
	case EPropertyBagPropertyType::Int32:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
		break;
	case EPropertyBagPropertyType::Int64:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
		break;
	case EPropertyBagPropertyType::Float:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
		break;
	case EPropertyBagPropertyType::Double:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
		break;
	case EPropertyBagPropertyType::Name:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
		break;
	case EPropertyBagPropertyType::String:
		PinType.PinCategory = UEdGraphSchema_K2::PC_String;
		break;
	case EPropertyBagPropertyType::Text:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Text;
		break;
	case EPropertyBagPropertyType::Enum:
		// @todo: some pin coloring is not correct due to this (byte-as-enum vs enum). 
		PinType.PinCategory = UEdGraphSchema_K2::PC_Enum;
		PinType.PinSubCategoryObject = const_cast<UObject*>(InParamType.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::Struct:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = const_cast<UObject*>(InParamType.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::Object:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
		PinType.PinSubCategoryObject = const_cast<UObject*>(InParamType.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::SoftObject:
		PinType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
		PinType.PinSubCategoryObject = const_cast<UObject*>(InParamType.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::Class:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Class;
		PinType.PinSubCategoryObject = const_cast<UObject*>(InParamType.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::SoftClass:
		PinType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
		PinType.PinSubCategoryObject = const_cast<UObject*>(InParamType.ValueTypeObject.Get());
		break;
	default:
		ensureMsgf(false, TEXT("Unhandled value type %d"), InParamType.ValueType);
		break;
	}

	return PinType;
}

FRigVMTemplateArgumentType FUtils::GetRigVMArgTypeFromParamTypeHandle(const FParamTypeHandle& InParamTypeHandle)
{
	return GetRigVMArgTypeFromParamType(InParamTypeHandle.GetType());
}

FRigVMTemplateArgumentType FUtils::GetRigVMArgTypeFromParamType(const FAnimNextParamType& InParamType)
{
	FRigVMTemplateArgumentType ArgType;

	FString CPPTypeString;

	// Value type
	switch (InParamType.ValueType)
	{
	case EPropertyBagPropertyType::Bool:
		CPPTypeString = RigVMTypeUtils::BoolType;
		break;
	case EPropertyBagPropertyType::Byte:
		CPPTypeString = RigVMTypeUtils::UInt8Type;
		break;
	case EPropertyBagPropertyType::Int32:
		CPPTypeString = RigVMTypeUtils::UInt32Type;
		break;
	case EPropertyBagPropertyType::Int64:
		ensureMsgf(false, TEXT("Unhandled value type %d"), InParamType.ValueType);
		break;
	case EPropertyBagPropertyType::Float:
		CPPTypeString = RigVMTypeUtils::FloatType;
		break;
	case EPropertyBagPropertyType::Double:
		CPPTypeString = RigVMTypeUtils::DoubleType;
		break;
	case EPropertyBagPropertyType::Name:
		CPPTypeString = RigVMTypeUtils::FNameType;
		break;
	case EPropertyBagPropertyType::String:
		CPPTypeString = RigVMTypeUtils::FStringType;
		break;
	case EPropertyBagPropertyType::Text:
		ensureMsgf(false, TEXT("Unhandled value type %d"), InParamType.ValueType);
		break;
	case EPropertyBagPropertyType::Enum:
		CPPTypeString = RigVMTypeUtils::CPPTypeFromEnum(Cast<UEnum>(InParamType.ValueTypeObject.Get()));
		ArgType.CPPTypeObject = const_cast<UObject*>(InParamType.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::Struct:
		CPPTypeString = RigVMTypeUtils::GetUniqueStructTypeName(Cast<UScriptStruct>(InParamType.ValueTypeObject.Get()));
		ArgType.CPPTypeObject = const_cast<UObject*>(InParamType.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::Object:
		CPPTypeString = RigVMTypeUtils::CPPTypeFromObject(Cast<UClass>(InParamType.ValueTypeObject.Get()));
		ArgType.CPPTypeObject = const_cast<UObject*>(InParamType.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::SoftObject:
		ensureMsgf(false, TEXT("Unhandled value type %d"), InParamType.ValueType);
		break;
	case EPropertyBagPropertyType::Class:
		ensureMsgf(false, TEXT("Unhandled value type %d"), InParamType.ValueType);
		break;
	case EPropertyBagPropertyType::SoftClass:
		ensureMsgf(false, TEXT("Unhandled value type %d"), InParamType.ValueType);
		break;
	default:
		ensureMsgf(false, TEXT("Unhandled value type %d"), InParamType.ValueType);
		break;
	}

	// Container type
	switch (InParamType.ContainerType)
	{
	case FAnimNextParamType::EContainerType::None:
		break;
	case FAnimNextParamType::EContainerType::Array:
		CPPTypeString = FString::Printf(RigVMTypeUtils::TArrayTemplate, *CPPTypeString);
		break;
	default:
		ensureMsgf(false, TEXT("Unhandled container type %d"), InParamType.ContainerType);
		break;
	}

	ArgType.CPPType = *CPPTypeString;

	return ArgType;
}

void FUtils::SetupBindingGraphForLiteral(URigVMController* InController, FName InParameterName, const FAnimNextParamType& InParamType)
{
	FRigVMTemplateArgumentType ArgType = GetRigVMArgTypeFromParamType(InParamType);
	TRigVMTypeIndex TypeIndex = FRigVMRegistry::Get().GetTypeIndex(ArgType);

	// Clear the graph
	InController->RemoveNodes(InController->GetGraph()->GetNodes());

	// Add new nodes for a simple literal binding
	URigVMUnitNode* EntryPointNode = InController->AddUnitNode(FRigUnit_AnimNextBeginExecution::StaticStruct(), FRigVMStruct::ExecuteName, FVector2D(-200.0f, 0.0f), FString(), false);

	const FName FactoryName = FRigVMDispatch_SetParameter().GetFactoryName();
	FRigVMDispatch_SetParameter* Factory = static_cast<FRigVMDispatch_SetParameter*>(FRigVMRegistry::Get().FindDispatchFactory(FactoryName));
	URigVMTemplateNode* SetParameterNode = InController->AddTemplateNode(Factory->GetTemplate()->GetNotation(), FVector2D(200.0f, 0.0f));
	InController->SetPinDefaultValue(SetParameterNode->FindPin(FRigVMDispatch_SetParameter::ParameterName.ToString())->GetPinPath(), InParameterName.ToString());
	InController->ResolveWildCardPin(SetParameterNode->FindPin(FRigVMDispatch_SetParameter::ValueName.ToString()), TypeIndex);

	InController->AddLink(EntryPointNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_AnimNextBeginExecution, ExecuteContext)), SetParameterNode->FindPin(FRigVMDispatch_SetParameter::ExecuteContextName.ToString()));
}

FText FUtils::GetParameterDisplayNameText(FName InParameterName)
{
	FString NameAsString = InParameterName.ToString();
	NameAsString.ReplaceCharInline(TEXT('_'), TEXT('.'));
	return FText::FromString(NameAsString);
}

}
