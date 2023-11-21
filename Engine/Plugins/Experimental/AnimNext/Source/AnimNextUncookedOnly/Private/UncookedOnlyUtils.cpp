// Copyright Epic Games, Inc. All Rights Reserved.

#include "UncookedOnlyUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
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
#include "Param/RigUnit_AnimNextParameterBeginExecution.h"
#include "Param/RigVMDispatch_GetParameter.h"
#include "Param/RigVMDispatch_SetLayerParameter.h"
#include "Param/AnimNextParameterBlockEntry.h"
#include "Param/IAnimNextParameterBlockParameterInterface.h"
#include "DecoratorBase/DecoratorReader.h"
#include "DecoratorBase/DecoratorWriter.h"
#include "DecoratorBase/NodeTemplate.h"
#include "DecoratorBase/NodeTemplateBuilder.h"
#include "DecoratorBase/NodeTemplateRegistry.h"
#include "DecoratorBase/NodeDescription.h"
#include "DecoratorBase/NodeInstance.h"
#include "DecoratorBase/DecoratorRegistry.h"
#include "DecoratorBase/Decorator.h"
#include "Graph/RigUnit_AnimNextBeginExecution.h"
#include "Param/AnimNextParameterLibrary.h"
#include "Param/Params.h"
#include "Serialization/MemoryReader.h"
#include "RigVMRuntimeDataRegistry.h"

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

	TArray<FDecoratorStackMapping> CollectDecoratorStacks(const URigVMGraph* VMGraph, UAnimNextGraph_Controller* VMController)
	{
		const TArray<URigVMNode*>& VMNodes = VMGraph->GetNodes();
		const URigVMUnitNode* VMRootNode = FindRootNode(VMNodes);

		TArray<FDecoratorStackMapping> DecoratorStackNodes;

		if (VMRootNode == nullptr)
		{
			// Root node wasn't found, add it, we'll need it to compile
			VMRootNode = VMController->AddUnitNode(FRigUnit_AnimNextGraphRoot::StaticStruct(), FRigUnit_AnimNextGraphRoot::EventName, FVector2D(0.0f, 0.0f), FString(), false);
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

		if (DecoratorStackNodes.IsEmpty())
		{
			// If the graph is empty, add a dummy node that just pushes a reference pose
			URigVMUnitNode* VMNode = VMController->AddUnitNode(FRigUnit_AnimNextDecoratorStack::StaticStruct(), FRigVMStruct::ExecuteName, FVector2D(0.0f, 0.0f), FString(), false);

			const UScriptStruct* CppDecoratorStruct = FRigDecorator_AnimNextCppDecorator::StaticStruct();

			FString DefaultValue;
			{
				const UE::AnimNext::FDecoratorUID ReferencePoseDecoratorUID(0xc03d6afc);	// Decorator header is private, reference by UID directly
				const FDecorator* Decorator = FDecoratorRegistry::Get().Find(ReferencePoseDecoratorUID);
				check(Decorator != nullptr);

				const FRigDecorator_AnimNextCppDecorator DefaultCppDecoratorStructInstance;
				FRigDecorator_AnimNextCppDecorator CppDecoratorStructInstance;
				CppDecoratorStructInstance.DecoratorSharedDataStruct = Decorator->GetDecoratorSharedDataStruct();

				const FProperty* Prop = FAnimNextCppDecoratorWrapper::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_STRING_CHECKED(FAnimNextCppDecoratorWrapper, CppDecorator));
				check(Prop != nullptr);

				Prop->ExportText_Direct(DefaultValue, &CppDecoratorStructInstance, &DefaultCppDecoratorStructInstance, nullptr, PPF_None);
			}

			VMController->AddDecorator(VMNode->GetFName(), *CppDecoratorStruct->GetPathName(), TEXT("ReferencePose"), DefaultValue, INDEX_NONE, false, false);

			FDecoratorStackMapping Mapping(VMNode);
			ForEachDecoratorInStack(VMNode,
				[&Mapping](const URigVMNode* DecoratorStackNode, const URigVMPin* DecoratorPin, const FDecorator* Decorator)
				{
					Mapping.DecoratorEntries.Add(FDecoratorEntryMapping(DecoratorStackNode, DecoratorPin, Decorator));
				});

			DecoratorStackNodes.Add(MoveTemp(Mapping));
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

	FAnimNextGraphEvaluatorExecuteDefinition GetGraphEvaluatorExecuteMethod(const FRigVMPinInfoArray& LatentPins)
	{
		const uint32 LatentPinListHash = GetTypeHash(LatentPins);
		if (const FAnimNextGraphEvaluatorExecuteDefinition* ExecuteDefinition = FRigUnit_AnimNextGraphEvaluator::FindExecuteMethod(LatentPinListHash))
		{
			return *ExecuteDefinition;
		}

		const FRigVMRegistry& Registry = FRigVMRegistry::Get();

		// Generate a new method for this argument list
		FAnimNextGraphEvaluatorExecuteDefinition ExecuteDefinition;
		ExecuteDefinition.Hash = LatentPinListHash;
		ExecuteDefinition.MethodName = FString::Printf(TEXT("Execute_%X"), LatentPinListHash);
		ExecuteDefinition.Arguments.Reserve(LatentPins.Num());

		for (const FRigVMPinInfo& Pin : LatentPins)
		{
			const FRigVMTemplateArgumentType& TypeArg = Registry.GetType(Pin.TypeIndex);

			FAnimNextGraphEvaluatorExecuteArgument Argument;
			Argument.Name = Pin.Name.ToString();
			Argument.CPPType = TypeArg.CPPType.ToString();

			ExecuteDefinition.Arguments.Add(Argument);
		}

		FRigUnit_AnimNextGraphEvaluator::RegisterExecuteMethod(ExecuteDefinition);

		return ExecuteDefinition;
	}
}

void FUtils::Compile(UAnimNextGraph* InGraph)
{
	check(InGraph);

	UAnimNextGraph_EditorData* EditorData = GetEditorData(InGraph);

	if (EditorData->bIsCompiling)
	{
		return;
	}

	TGuardValue<bool> CompilingGuard(EditorData->bIsCompiling, true);

	// Before we re-compile a graph, we need to release and live instances since we need the metadata we are about to replace
	// to call decorator destructors etc
	const TArray<FAnimNextGraphInstance*> PreviousLiveGraphInstances = InGraph->ReleaseAllInstances();

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
	TArray<Private::FDecoratorStackMapping> DecoratorStackNodes = Private::CollectDecoratorStacks(VMTempGraph, TempController);
	check(!DecoratorStackNodes.IsEmpty());

	// Add our runtime shim root node
	URigVMUnitNode* TempShimRootNode = TempController->AddUnitNode(FRigUnit_AnimNextShimRoot::StaticStruct(), FRigVMStruct::ExecuteName, FVector2D::ZeroVector, FString(), false);

	// Add our graph evaluator node
	TMap<FName, URigVMPin*> LatentPinMapping;
	const FRigVMPinInfoArray LatentPins = Private::CollectLatentPins(DecoratorStackNodes, LatentPinMapping);

	// We need a unique method name to match our unique argument list
	const FAnimNextGraphEvaluatorExecuteDefinition ExecuteDefinition = Private::GetGraphEvaluatorExecuteMethod(LatentPins);

	URigVMUnitNode* GraphEvaluatorNode = TempController->AddUnitNodeWithPins(FRigUnit_AnimNextGraphEvaluator::StaticStruct(), LatentPins, *ExecuteDefinition.MethodName, FVector2D::ZeroVector, FString(), false);

	// Link our shim and evaluator nodes together using the execution context
	TempController->AddLink(
		TempShimRootNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_AnimNextShimRoot, ExecuteContext)),
		GraphEvaluatorNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_AnimNextGraphEvaluator, ExecuteContext)),
		false);

	// Link our latent pins
	for (const FRigVMPinInfo& LatentPin : LatentPins)
	{
		TempController->AddLink(
			LatentPinMapping[LatentPin.Name],
			GraphEvaluatorNode->FindPin(LatentPin.Name.ToString()),
			false);
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
	InGraph->ExecuteDefinition = ExecuteDefinition;
	InGraph->SharedDataArchiveBuffer = DecoratorWriter.GetGraphSharedData();
	InGraph->GraphReferencedObjects = DecoratorWriter.GetGraphReferencedObjects();
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
	Compiler->Compile(Settings, { VMTempGraph }, TempController, InGraph->VM, InGraph->ExtendedExecuteContext, InGraph->GetRigVMExternalVariables(), & EditorData->PinToOperandMap);

	// Initialize right away, in packaged builds we initialize during PostLoad
	InGraph->VM->Initialize(InGraph->ExtendedExecuteContext);

	if (EditorData->bErrorsDuringCompilation)
	{
		if(Settings.SurpressErrors)
		{
			Settings.Reportf(EMessageSeverity::Info, InGraph,TEXT("Compilation Errors may be suppressed for AnimNext Interface Graph: %s. See VM Compile Settings for more Details"), *InGraph->GetName());
		}
	}

	EditorData->bVMRecompilationRequired = false;
	if(InGraph->VM)
	{
		EditorData->RigVMCompiledEvent.Broadcast(InGraph, InGraph->VM, InGraph->ExtendedExecuteContext);
	}

	VMClient->RemoveController(VMTempGraph);

	// Now that the graph has been re-compiled, re-allocate the previous live instances
	for (FAnimNextGraphInstance* GraphInstance : PreviousLiveGraphInstances)
	{
		InGraph->AllocateInstance(*GraphInstance);
	}

#if WITH_EDITOR
//	RefreshBreakpoints(EditorData);
#endif
}

void FUtils::RecreateVM(UAnimNextGraph* InGraph)
{
	InGraph->VM = NewObject<URigVM>(InGraph, TEXT("VM"), RF_NoFlags);
	InGraph->VM->Reset(InGraph->ExtendedExecuteContext);
	InGraph->RigVM = InGraph->VM; // Local serialization
}

UAnimNextGraph_EditorData* FUtils::GetEditorData(const UAnimNextGraph* InAnimNextGraph)
{
	check(InAnimNextGraph);
	
	return CastChecked<UAnimNextGraph_EditorData>(InAnimNextGraph->EditorData);
}

UAnimNextGraph* FUtils::GetGraph(const UAnimNextGraph_EditorData* InEditorData)
{
	check(InEditorData);

	return CastChecked<UAnimNextGraph>(InEditorData->GetOuter());
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
	FRigVMExtendedExecuteContext& CDOContext = InParameterBlock->GetRigVMExtendedExecuteContext();
	const FRigVMCompileSettings Settings = (EditorData->bCompileInDebugMode) ? FRigVMCompileSettings::Fast(EditorData->VMCompileSettings.GetExecuteContextStruct()) : EditorData->VMCompileSettings;
	URigVMController* RootController = EditorData->GetRigVMClient()->GetOrCreateController(EditorData->GetRigVMClient()->GetDefaultModel());
	Compiler->Compile(Settings, EditorData->GetRigVMClient()->GetAllModels(false, false), RootController, InParameterBlock->VM, CDOContext, InParameterBlock->GetExternalVariables(), &EditorData->PinToOperandMap);

	InParameterBlock->VM->Initialize(CDOContext);
	InParameterBlock->GenerateUserDefinedDependenciesData(CDOContext);

	// Notable difference with vanilla RigVM host behavior - we init the VM here at the moment as we only have one 'instance'
	InParameterBlock->InitializeVM(FRigUnit_AnimNextBeginExecution::EventName);

	if (EditorData->bErrorsDuringCompilation)
	{
		if(Settings.SurpressErrors)
		{
			Settings.Reportf(EMessageSeverity::Info, InParameterBlock,
				TEXT("Compilation Errors may be suppressed for ControlRigBlueprint: %s. See VM Compile Setting in Class Settings for more Details"), *InParameterBlock->GetName());
		}
		EditorData->bVMRecompilationRequired = false;
		if(InParameterBlock->VM)
		{
			EditorData->RigVMCompiledEvent.Broadcast(InParameterBlock, InParameterBlock->VM, InParameterBlock->GetRigVMExtendedExecuteContext());
		}
		return;
	}

//	InitializeArchetypeInstances();

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

	TArray<FPropertyBagPropertyDesc> PropertyDescs;
	PropertyDescs.Reserve(EditorData->Entries.Num());
	
	// Gather all parameters in this block
	for(const UAnimNextParameterBlockEntry* Entry : EditorData->Entries)
	{
		if(const IAnimNextParameterBlockParameterInterface* Binding = Cast<IAnimNextParameterBlockParameterInterface>(Entry))
		{
			if(const UAnimNextParameter* Parameter = Binding->GetParameter())
			{
				const FAnimNextParamType& Type = Binding->GetParamType();
				PropertyDescs.Emplace(Parameter->GetFName(), Type.GetContainerType(), Type.GetValueType(), Type.GetValueTypeObject());
			}
		}
	}

	if(PropertyDescs.Num() > 0)
	{
		// find any existing IDs for old properties with name-matching
		for(FPropertyBagPropertyDesc& NewDesc : PropertyDescs)
		{
			if(InParameterBlock->PropertyBag.GetPropertyBagStruct())
			{
				for(const FPropertyBagPropertyDesc& ExistingDesc : InParameterBlock->PropertyBag.GetPropertyBagStruct()->GetPropertyDescs())
				{
					if(ExistingDesc.Name == NewDesc.Name)
					{
						NewDesc.ID = ExistingDesc.ID;
						break;
					}
				}
			}
		}

		// Create new property bag and migrate
		const UPropertyBag* NewBagStruct = UPropertyBag::GetOrCreateFromDescs(PropertyDescs);
		InParameterBlock->PropertyBag.MigrateToNewBagStruct(NewBagStruct);
	}
	else
	{
		InParameterBlock->PropertyBag.Reset();
	}

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
	InParameterBlock->VM = NewObject<URigVM>(InParameterBlock, TEXT("VM"), RF_NoFlags);
	InParameterBlock->VM->Reset(InParameterBlock->GetRigVMExtendedExecuteContext());
	InParameterBlock->RigVM = InParameterBlock->VM; // Local serialization
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

FInstancedPropertyBag* FUtils::GetPropertyBag(UAnimNextParameterBlock* ReferencedBlock)
{
	FInstancedPropertyBag* InstancedPropertyBag =&ReferencedBlock->PropertyBag;

	return InstancedPropertyBag;
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
		CPPTypeString = RigVMTypeUtils::CPPTypeFromObject(Cast<UClass>(InParamType.ValueTypeObject.Get()), RigVMTypeUtils::EClassArgType::AsObject);
		ArgType.CPPTypeObject = const_cast<UObject*>(InParamType.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::SoftObject:
		ensureMsgf(false, TEXT("Unhandled value type %d"), InParamType.ValueType);
		break;
	case EPropertyBagPropertyType::Class:
		CPPTypeString = RigVMTypeUtils::CPPTypeFromObject(Cast<UClass>(InParamType.ValueTypeObject.Get()), RigVMTypeUtils::EClassArgType::AsClass);
		ArgType.CPPTypeObject = const_cast<UObject*>(InParamType.ValueTypeObject.Get());
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

void FUtils::SetupAnimGraph(URigVMController* InController)
{
	// Clear the graph
	InController->RemoveNodes(InController->GetGraph()->GetNodes());

	// Add root node
	URigVMUnitNode* MainEntryPointNode = InController->AddUnitNode(FRigUnit_AnimNextGraphRoot::StaticStruct(), FRigUnit_AnimNextGraphRoot::EventName, FVector2D(-400.0f, 0.0f), FString(), false);
	URigVMPin* BeginExecutePin = MainEntryPointNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_AnimNextGraphRoot, Result));
	check(BeginExecutePin);
	check(BeginExecutePin->GetDirection() == ERigVMPinDirection::Input);
}

void FUtils::SetupParameterGraph(URigVMController* InController)
{
	// Clear the graph
	InController->RemoveNodes(InController->GetGraph()->GetNodes());

	// Add entry point
	InController->AddUnitNode(FRigUnit_AnimNextParameterBeginExecution::StaticStruct(), FRigVMStruct::ExecuteName, FVector2D(-200.0f, 0.0f), FString(), false);
}

void FUtils::SetupBindingGraph(URigVMController* InController, FName InParameterName, const FAnimNextParamType& InParamType)
{
	SetupBindingGraphForLiteral(InController, InParameterName, InParamType);
}

void FUtils::SetupBindingGraphForLiteral(URigVMController* InController, FName InParameterName, const FAnimNextParamType& InParamType)
{
	FRigVMTemplateArgumentType ArgType = GetRigVMArgTypeFromParamType(InParamType);
	TRigVMTypeIndex TypeIndex = FRigVMRegistry::Get().GetTypeIndex(ArgType);

	// Clear the graph
	InController->RemoveNodes(InController->GetGraph()->GetNodes());

	// Add new nodes for a simple literal binding
	URigVMUnitNode* EntryPointNode = InController->AddUnitNode(FRigUnit_AnimNextParameterBeginExecution::StaticStruct(), FRigVMStruct::ExecuteName, FVector2D(-200.0f, 0.0f), FString(), false);

	const FName FactoryName = FRigVMDispatch_SetLayerParameter().GetFactoryName();
	FRigVMDispatch_SetLayerParameter* Factory = static_cast<FRigVMDispatch_SetLayerParameter*>(FRigVMRegistry::Get().FindDispatchFactory(FactoryName));
	URigVMTemplateNode* SetParameterNode = InController->AddTemplateNode(Factory->GetTemplate()->GetNotation(), FVector2D(200.0f, 0.0f));
	InController->SetPinDefaultValue(SetParameterNode->FindPin(FRigVMDispatch_SetLayerParameter::ParameterName.ToString())->GetPinPath(), InParameterName.ToString());
	InController->ResolveWildCardPin(SetParameterNode->FindPin(FRigVMDispatch_SetLayerParameter::ValueName.ToString()), TypeIndex);

	InController->AddLink(EntryPointNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_AnimNextParameterBeginExecution, ExecuteContext)), SetParameterNode->FindPin(FRigVMDispatch_SetLayerParameter::ExecuteContextName.ToString()));
}

FText FUtils::GetParameterDisplayNameText(FName InParameterName)
{
	FString NameAsString = InParameterName.ToString();
	NameAsString.ReplaceCharInline(TEXT('_'), TEXT('.'));
	return FText::FromString(NameAsString);
}

bool FUtils::GetExportedParametersForLibrary(const FAssetData& InLibraryAsset, FAnimNextParameterLibraryAssetRegistryExports& OutExports)
{
	const FString TagValue = InLibraryAsset.GetTagValueRef<FString>(UAnimNextParameterLibrary::ExportsAssetRegistryTag);
	return FAnimNextParameterLibraryAssetRegistryExports::StaticStruct()->ImportText(*TagValue, &OutExports, nullptr, PPF_None, nullptr, FAnimNextParameterLibraryAssetRegistryExports::StaticStruct()->GetName()) != nullptr;
}

FAnimNextParamType FUtils::GetParameterTypeFromName(FName InName)
{
	// Check built-in params first as they are cheaper
	if(const FParamDefinition* FoundDefinition = FParams::FindBuiltInParameter(InName))
	{
		return FoundDefinition->Type;
	}

	// Query the asset registry for other params
	IAssetRegistry& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	FARFilter ARFilter;
	ARFilter.ClassPaths = { UAnimNextParameterLibrary::StaticClass()->GetClassPathName() };

	TArray<FAssetData> LibraryAssets;
	AssetRegistry.GetAssets(ARFilter, LibraryAssets);

	for(const FAssetData& LibraryAsset : LibraryAssets)
	{
		FAnimNextParameterLibraryAssetRegistryExports Exports;
		GetExportedParametersForLibrary(LibraryAsset, Exports);
		for(const FAnimNextParameterLibraryAssetRegistryExportEntry& Export : Exports.Parameters)
		{
			if(Export.Name == InName)
			{
				return Export.Type;
			}
		}
	}

	return FAnimNextParamType();
}

}
