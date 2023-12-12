// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "AnimNextTest.h"
#include "UncookedOnlyUtils.h"
#include "Param/AnimNextParameterBlock_EditorData.h"
#include "Misc/AutomationTest.h"
#include "Param/ParameterBlockFactory.h"
#include "Param/AnimNextParameterBlock.h"
#include "Param/AnimNextParameterBlockParameter.h"
#include "Animation/AnimSequence.h"
#if WITH_EDITOR
#include "ScopedTransaction.h"
#include "Editor.h"
#include "IPythonScriptPlugin.h"
#endif

// AnimNext Editor Tests

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextParametersEditorTest_Block, "Animation.AnimNext.Parameters.Editor.Block", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextParametersEditorTest_Block::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;

	const TStrongObjectPtr<UFactory> BlockFactory(NewObject<UAnimNextParameterBlockFactory>());
	UAnimNextParameterBlock* Block = Cast<UAnimNextParameterBlock>(BlockFactory->FactoryCreateNew(UAnimNextParameterBlock::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextParameterBlock"), RF_Transient, nullptr, nullptr, NAME_None));
	if(Block == nullptr)
	{
		AddError(TEXT("Could not create parameter block."));
		return false;
	}

	UAnimNextParameterBlock_EditorData* EditorData = UncookedOnly::FUtils::GetEditorData(Block);
	if(EditorData == nullptr)
	{
		AddError(TEXT("Parameter block has no editor data."));
		return false;
	}

	static FName TestParameterName = TEXT("TestParam");
	
	// AddParameter
	UAnimNextParameterBlockParameter* Parameter = nullptr;
	{
		FScopedTransaction Transaction(FText::GetEmpty());		
		Parameter = EditorData->AddParameter(TestParameterName, FAnimNextParamType::GetType<bool>());

		if (AddErrorIfFalse(Parameter != nullptr, TEXT("Could not create new parameter in block.")))
		{
			AddErrorIfFalse(Parameter->GetParamType() == FAnimNextParamType::GetType<bool>(), TEXT("Incorrect parameter type found"));			
		}
	}

	AddExpectedError(TEXT("UAnimNextParameterBlock_EditorData::AddParameter: A parameter already exists for the supplied parameter name."));
	AddErrorIfFalse(EditorData->AddParameter(TestParameterName, FAnimNextParamType::GetType<bool>()) == nullptr, TEXT("Expected duplicate parameter name argument to fail"));

	GEditor->UndoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 0, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 0)."), EditorData->Entries.Num()));

	GEditor->RedoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 1, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 1)."), EditorData->Entries.Num()));

	// Failure cases
	AddExpectedError(TEXT("UAnimNextParameterBlock_EditorData::AddParameter: Invalid parameter name supplied."));
	AddErrorIfFalse(EditorData->AddParameter(NAME_None, FAnimNextParamType::GetType<bool>()) == nullptr, TEXT("Expected invalid argument to fail"));

	auto TestParameterType = [this, EditorData](FAnimNextParamType InType)
	{
		UAnimNextParameterBlockParameter* TypedParameter = EditorData->AddParameter(TEXT("TestParam0"), InType);
		const bool bValidParameter = TypedParameter != nullptr;
		if (bValidParameter && AddErrorIfFalse(bValidParameter, FString::Printf(TEXT("Could not create new parameter of type %s in block."), *InType.ToString())))
		{
			AddErrorIfFalse(TypedParameter->GetParamType() == InType, TEXT("Incorrect parameter type found"));
			EditorData->RemoveEntry(TypedParameter);
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
	AddErrorIfFalse(EditorData->Entries.Num() == 1, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 1)."), EditorData->Entries.Num()));

	GEditor->RedoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 0, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 0)."), EditorData->Entries.Num()));

	GEditor->UndoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 1, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 1)."), EditorData->Entries.Num()));

	// FindEntry
	AddErrorIfFalse(EditorData->FindEntry(TestParameterName) != nullptr, TEXT("Could not find entry in block."));
	GEditor->UndoTransaction();

	// Add graph
	UAnimNextParameterBlockGraph* Graph = nullptr;
	{
		FScopedTransaction Transaction(FText::GetEmpty());
		Graph = EditorData->AddGraph(TEXT("TestGraph"));
		AddErrorIfFalse(Graph != nullptr, TEXT("Could not create new graph in block."));
	}

	GEditor->UndoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 0, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 0)."), EditorData->Entries.Num()));

	GEditor->RedoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 1, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 1)."), EditorData->Entries.Num()));
	GEditor->UndoTransaction();

	Tests::FUtils::CleanupAfterTests();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextParametersEditorTest_Python, "Animation.AnimNext.Parameters.Editor.Python", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextParametersEditorTest_Python::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;

	const TCHAR* Script = TEXT(
		"asset_tools = unreal.AssetToolsHelpers.get_asset_tools()\n"
		"block = unreal.AssetTools.create_asset(asset_tools, asset_name = \"TestBlock\", package_path = \"/Game/\", asset_class = unreal.AnimNextParameterBlock, factory = unreal.AnimNextParameterBlockFactory())\n"
		"block.add_parameter(name = \"TestParam\", value_type = unreal.PropertyBagPropertyType.BOOL, container_type = unreal.PropertyBagContainerType.NONE)\n"
		"block.add_graph(name = \"TestGraph\")\n"
		"unreal.EditorAssetLibrary.delete_loaded_asset(block)\n"
	);

	IPythonScriptPlugin::Get()->ExecPythonCommand(Script);

	Tests::FUtils::CleanupAfterTests();

	return true;
}

#endif	// WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR