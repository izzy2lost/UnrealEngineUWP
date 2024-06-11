// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "AnimNextTest.h"
#include "UncookedOnlyUtils.h"
#include "Misc/AutomationTest.h"
#include "Animation/AnimSequence.h"
#include "Module/AnimNextModule.h"
#include "Module/AnimNextModule_Parameter.h"
#include "Module/AnimNextModule_EditorData.h"
#include "Graph/AnimNextModule_AnimationGraph.h"
#include "Module/AnimNextModule_EventGraph.h"
#include "Module/ModuleFactory.h"
#include "Param/RigVMDispatch_GetParameter.h"
#if WITH_EDITOR
#include "ScopedTransaction.h"
#include "Editor.h"
#include "IPythonScriptPlugin.h"
#endif

// AnimNext Editor Tests

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

namespace UE::AnimNext::Tests
{

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEditor_Parameters, "Animation.AnimNext.Editor.Parameters", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEditor_Parameters::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;

	ON_SCOPE_EXIT{ FUtils::CleanupAfterTests(); };

	const TStrongObjectPtr<UFactory> GraphFactory(NewObject<UAnimNextModuleFactory>());
	UAnimNextModule* Module = Cast<UAnimNextModule>(GraphFactory->FactoryCreateNew(UAnimNextModule::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextGraph"), RF_Transient, nullptr, nullptr, NAME_None));
	if(Module == nullptr)
	{
		AddError(TEXT("Could not create module."));
		return false;
	}

	UAnimNextModule_EditorData* EditorData = UncookedOnly::FUtils::GetEditorData(Module);
	if(EditorData == nullptr)
	{
		AddError(TEXT("Module has no editor data."));
		return false;
	}

	static FName TestParameterName = TEXT("TestParam");
	
	// AddParameter
	UAnimNextModule_Parameter* Parameter = nullptr;
	{
		FScopedTransaction Transaction(FText::GetEmpty());
		Parameter = EditorData->AddParameter(TestParameterName, FAnimNextParamType::GetType<bool>());

		if (AddErrorIfFalse(Parameter != nullptr, TEXT("Could not create new parameter in graph.")))
		{
			AddErrorIfFalse(Parameter->GetParamType() == FAnimNextParamType::GetType<bool>(), TEXT("Incorrect parameter type found"));
		}
	}

	AddExpectedError(TEXT("UAnimNextModule_EditorData::AddParameter: A parameter already exists for the supplied parameter name."));
	AddErrorIfFalse(EditorData->AddParameter(TestParameterName, FAnimNextParamType::GetType<bool>()) == nullptr, TEXT("Expected duplicate parameter name argument to fail"));

	GEditor->UndoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 0, FString::Printf(TEXT("Unexpected entry count found in graph (Have %d, expected 0)."), EditorData->Entries.Num()));

	GEditor->RedoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 1, FString::Printf(TEXT("Unexpected entry count found in graph (Have %d, expected 1)."), EditorData->Entries.Num()));

	// Failure cases
	AddExpectedError(TEXT("UAnimNextModule_EditorData::AddParameter: Invalid parameter name supplied."));
	AddErrorIfFalse(EditorData->AddParameter(NAME_None, FAnimNextParamType::GetType<bool>()) == nullptr, TEXT("Expected invalid argument to fail"));

	auto TestParameterType = [this, EditorData](FAnimNextParamType InType, bool bInRemove = true)
	{
		UAnimNextModule_Parameter* TypedParameter = EditorData->AddParameter(TEXT("TestParam0"), InType);
		const bool bValidParameter = TypedParameter != nullptr;
		if (bValidParameter && AddErrorIfFalse(bValidParameter, FString::Printf(TEXT("Could not create new parameter of type %s in graph."), *InType.ToString())))
		{
			AddErrorIfFalse(TypedParameter->GetParamType() == InType, TEXT("Incorrect parameter type found"));
			if(bInRemove)
			{
				EditorData->RemoveEntry(TypedParameter);
			}
		}
	};

	// Various types
	TestParameterType(FAnimNextParamType::GetType<bool>());
	TestParameterType(FAnimNextParamType::GetType<uint8>());
	TestParameterType(FAnimNextParamType::GetType<int32>());
	TestParameterType(FAnimNextParamType::GetType<int64>());
	TestParameterType(FAnimNextParamType::GetType<float>());
	TestParameterType(FAnimNextParamType::GetType<double>());
	TestParameterType(FAnimNextParamType::GetType<FName>());
	TestParameterType(FAnimNextParamType::GetType<FString>());
	TestParameterType(FAnimNextParamType::GetType<FText>());
	TestParameterType(FAnimNextParamType::GetType<EPropertyBagPropertyType>());
	TestParameterType(FAnimNextParamType::GetType<FVector>());
	TestParameterType(FAnimNextParamType::GetType<FQuat>());
	TestParameterType(FAnimNextParamType::GetType<FTransform>());
	TestParameterType(FAnimNextParamType::GetType<TObjectPtr<UObject>>());
	TestParameterType(FAnimNextParamType::GetType<TObjectPtr<UAnimSequence>>());
	TestParameterType(FAnimNextParamType::GetType<TArray<float>>());
	TestParameterType(FAnimNextParamType::GetType<TArray<TObjectPtr<UAnimSequence>>>());

	// RemoveEntry
	{
		FScopedTransaction Transaction(FText::GetEmpty());
		AddErrorIfFalse(EditorData->RemoveEntry(Parameter), TEXT("Failed to remove entry."));
	}

	GEditor->UndoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 1, FString::Printf(TEXT("Unexpected entry count found in graph (Have %d, expected 1)."), EditorData->Entries.Num()));

	GEditor->RedoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 0, FString::Printf(TEXT("Unexpected entry count found in graph (Have %d, expected 0)."), EditorData->Entries.Num()));

	GEditor->UndoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 1, FString::Printf(TEXT("Unexpected entry count found in graph (Have %d, expected 1)."), EditorData->Entries.Num()));

	// FindEntry
	AddErrorIfFalse(EditorData->FindEntry(TestParameterName) != nullptr, TEXT("Could not find entry in graph."));
	GEditor->UndoTransaction();

	// Add graph
	{
		FScopedTransaction Transaction(FText::GetEmpty());
		UAnimNextModule_EventGraph* EventGraph = EditorData->AddEventGraph(TEXT("TestGraph"));
		AddErrorIfFalse(EventGraph != nullptr, TEXT("Could not create new event graph in graph."));
	}

	GEditor->UndoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 0, FString::Printf(TEXT("Unexpected entry count found in graph (Have %d, expected 0)."), EditorData->Entries.Num()));

	GEditor->RedoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 1, FString::Printf(TEXT("Unexpected entry count found in graph (Have %d, expected 1)."), EditorData->Entries.Num()));
	GEditor->UndoTransaction();

	// Add graph and add parameter getters and setters to it, testing compilation
	{
		TestParameterType(FAnimNextParamType::GetType<bool>(), false);

		IAnimNextRigVMParameterInterface* ParameterEntry = CastChecked<IAnimNextRigVMParameterInterface>(EditorData->FindEntry("TestParam0"));
		UE_RETURN_ON_ERROR(ParameterEntry != nullptr, TEXT("Could not find new parameter entry."));

		UAnimNextModule_EventGraph* EventGraph = EditorData->AddEventGraph(TEXT("TestGraph1"));
		UE_RETURN_ON_ERROR(EventGraph != nullptr, TEXT("Could not create new event graph in graph."));

		URigVMGraph* RigVMGraph = EventGraph->GetRigVMGraph();
		UE_RETURN_ON_ERROR(RigVMGraph->GetNodes().Num() == 1, TEXT("Unexpected number of nodes in new event graph."));

		URigVMNode* EventNode = RigVMGraph->GetNodes()[0];
		check(EventNode);
		URigVMPin* ExecutePin = EventNode->FindPin("ExecuteContext");
		UE_RETURN_ON_ERROR(ExecutePin != nullptr, TEXT("Could find initial execute pin."));

		UAnimNextModule_Controller* Controller = Cast<UAnimNextModule_Controller>(EditorData->GetController(EventGraph->GetRigVMGraph()));
		URigVMNode* GetParameterNode = Controller->AddGetAnimNextGraphParameterNode(FVector2D::ZeroVector, ParameterEntry->GetParamName(), FAnimNextParamType::GetType<bool>());
		UE_RETURN_ON_ERROR(GetParameterNode != nullptr, TEXT("Could not add GetParameter node."));

		URigVMNode* SetParameterNode = Controller->AddSetAnimNextGraphParameterNode(FVector2D::ZeroVector, ParameterEntry->GetParamName(), FAnimNextParamType::GetType<bool>());
		UE_RETURN_ON_ERROR(SetParameterNode != nullptr, TEXT("Could not add SetParameter node."));

		UE_RETURN_ON_ERROR(Controller->AddLink(ExecutePin, SetParameterNode->FindPin("ExecuteContext")), TEXT("Could not link SetParameter node."));

		const FString ValuePin = FRigVMDispatch_GetParameter::ValueName.ToString(); 
		UE_RETURN_ON_ERROR(Controller->AddLink(GetParameterNode->FindPin(ValuePin), SetParameterNode->FindPin(ValuePin)), TEXT("Could not link value pins."));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEditor_Parameters_Python, "Animation.AnimNext.Editor.PythonParameters", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEditor_Parameters_Python::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;

	const TCHAR* Script = TEXT(
		"asset_tools = unreal.AssetToolsHelpers.get_asset_tools()\n"
		"module = unreal.AssetTools.create_asset(asset_tools, asset_name = \"TestModule\", package_path = \"/Game/\", asset_class = unreal.AnimNextModule, factory = unreal.AnimNextModuleFactory())\n"
		"module.add_parameter(name = \"TestParam\", value_type = unreal.PropertyBagPropertyType.BOOL, container_type = unreal.PropertyBagContainerType.NONE)\n"
		"module.add_event_graph(name = \"TestEventGraph\")\n"
		"module.add_animation_graph(name = \"TestAnimationGraph\")\n"
		"unreal.EditorAssetLibrary.delete_loaded_asset(module)\n"
	);

	IPythonScriptPlugin::Get()->ExecPythonCommand(Script);

	FUtils::CleanupAfterTests();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEditor_Graph, "Animation.AnimNext.Editor.AnimationGraph", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEditor_Graph::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;

	const TStrongObjectPtr<UFactory> GraphFactory(NewObject<UAnimNextModuleFactory>());
	UAnimNextModule* Graph = Cast<UAnimNextModule>(GraphFactory->FactoryCreateNew(UAnimNextModule::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextGraph"), RF_Transient, nullptr, nullptr, NAME_None));
	if(Graph == nullptr)
	{
		AddError(TEXT("Could not create graph."));
		return false;
	}

	UAnimNextModule_EditorData* EditorData = UncookedOnly::FUtils::GetEditorData(Graph);
	if(EditorData == nullptr)
	{
		AddError(TEXT("Graph has no editor data."));
		return false;
	}

	// Add graph
	UAnimNextModule_AnimationGraph* GraphEntry = nullptr;
	{
		FScopedTransaction Transaction(FText::GetEmpty());
		GraphEntry = EditorData->AddAnimationGraph(TEXT("TestGraph"));
		AddErrorIfFalse(Graph != nullptr, TEXT("Could not create new animation graph in asset."));
	}

	GEditor->UndoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 0, FString::Printf(TEXT("Unexpected entry count found in graph asset (Have %d, expected 0)."), EditorData->Entries.Num()));

	GEditor->RedoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 1, FString::Printf(TEXT("Unexpected entry count found in graph asset (Have %d, expected 1)."), EditorData->Entries.Num()));

	// RemoveEntry
	{
		FScopedTransaction Transaction(FText::GetEmpty());
		AddErrorIfFalse(EditorData->RemoveEntry(GraphEntry), TEXT("Failed to remove entry."));
	}

	GEditor->UndoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 1, FString::Printf(TEXT("Unexpected entry count found in graph asset (Have %d, expected 1)."), EditorData->Entries.Num()));

	GEditor->RedoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 0, FString::Printf(TEXT("Unexpected entry count found in graph asset (Have %d, expected 0)."), EditorData->Entries.Num()));

	GEditor->UndoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 1, FString::Printf(TEXT("Unexpected entry count found in graph asset (Have %d, expected 1)."), EditorData->Entries.Num()));

	// FindEntry
	AddErrorIfFalse(EditorData->FindEntry(TEXT("TestGraph")) != nullptr, TEXT("Could not find entry in asset."));
	GEditor->UndoTransaction();

	FUtils::CleanupAfterTests();

	return true;
}

}

#endif	// WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR