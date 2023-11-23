// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UncookedOnlyUtils.h"
#include "Param/AnimNextParameterBlock_EditorData.h"
#include "Misc/AutomationTest.h"
#include "Param/ParameterBlockFactory.h"
#include "Param/AnimNextParameterBlock.h"
#include "Param/AnimNextParameterBlockParameter.h"
#include "Param/AnimNextParameterBlockBinding.h"
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
	{
		FScopedTransaction Transaction(FText::GetEmpty());		
		UAnimNextParameterBlockParameter* Parameter = EditorData->AddParameter(TestParameterName, FAnimNextParamType::GetType<bool>());

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
	
	GEditor->UndoTransaction();
	
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

	// AddBinding
	static FName TestBindingName = TEXT("TestParamBinding");
	UAnimNextParameterBlockBinding* Binding = nullptr;
	{
		FScopedTransaction Transaction(FText::GetEmpty());
		Binding = EditorData->AddBinding(TestBindingName, FAnimNextParamType::GetType<int32>());
		AddErrorIfFalse(Binding != nullptr, TEXT("Could not create new binding in block."));
	}

	GEditor->UndoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 0, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 0)."), EditorData->Entries.Num()));

	GEditor->RedoTransaction();

	// RemoveAllBindings
	{
		FScopedTransaction Transaction(FText::GetEmpty());
		AddErrorIfFalse(EditorData->RemoveAllBindings(TestBindingName), TEXT("Failed to remove binding."));
	}

	GEditor->UndoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 1, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 1)."), EditorData->Entries.Num()));

	GEditor->RedoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 0, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 0)."), EditorData->Entries.Num()));

	GEditor->UndoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 1, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 1)."), EditorData->Entries.Num()));

	// RemoveEntry
	{
		FScopedTransaction Transaction(FText::GetEmpty());
		AddErrorIfFalse(EditorData->RemoveEntry(Binding), TEXT("Failed to remove entry."));
	}

	GEditor->UndoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 1, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 1)."), EditorData->Entries.Num()));

	GEditor->RedoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 0, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 0)."), EditorData->Entries.Num()));

	GEditor->UndoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 1, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 1)."), EditorData->Entries.Num()));

	// FindBinding
	AddErrorIfFalse(EditorData->FindBinding(TestBindingName) != nullptr, TEXT("Could not find binding in block."));

	const UAnimNextParameterBlock* OtherBlock = Cast<UAnimNextParameterBlock>(BlockFactory->FactoryCreateNew(UAnimNextParameterBlock::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextParameterBlock2"), RF_Transient, nullptr, nullptr, NAME_None));
	if(OtherBlock == nullptr)
	{
		AddError(TEXT("Could not create additional parameter block."));
		return false;
	}
	
	UAnimNextParameterBlock_EditorData* OtherEditorData = UncookedOnly::FUtils::GetEditorData(OtherBlock);
	if(OtherEditorData == nullptr)
	{
		AddError(TEXT("Additional parameter block has no editor data."));
		return false;
	}

	// AddBindingReference
	{
		FScopedTransaction Transaction(FText::GetEmpty());
		UAnimNextParameterBlockBindingReference* Ref = OtherEditorData->AddBindingReference(TestBindingName, Block);
		AddErrorIfFalse(Ref != nullptr, TEXT("Could not create new binding reference in block."));
	}

	GEditor->UndoTransaction();
	AddErrorIfFalse(OtherEditorData->Entries.Num() == 0, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 0)."), OtherEditorData->Entries.Num()));

	GEditor->RedoTransaction();
	AddErrorIfFalse(OtherEditorData->Entries.Num() == 1, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 1)."), OtherEditorData->Entries.Num()));

	// FindBinding
	AddErrorIfFalse(OtherEditorData->FindBinding(TestBindingName) != nullptr, TEXT("Could not find binding refernce in block."));

	GEditor->UndoTransaction();
	GEditor->UndoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 0, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 0)."), EditorData->Entries.Num()));

	GEditor->RedoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 1, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 1)."), EditorData->Entries.Num()));
	GEditor->UndoTransaction();
	
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

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextParametersEditorTest_Python, "Animation.AnimNext.Parameters.Editor.Python", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextParametersEditorTest_Python::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;

	const TCHAR* Script = TEXT(
		"asset_tools = unreal.AssetToolsHelpers.get_asset_tools()\n"
		"block = unreal.AssetTools.create_asset(asset_tools, asset_name = \"TestBlock\", package_path = \"/Game/\", asset_class = unreal.AnimNextParameterBlock, factory = unreal.AnimNextParameterBlockFactory())\n"
		"other_block = unreal.AssetTools.create_asset(asset_tools, asset_name = \"TestBlock1\", package_path = \"/Game/\", asset_class = unreal.AnimNextParameterBlock, factory = unreal.AnimNextParameterBlockFactory())\n"
		"block.add_parameter(name = \"TestParam\", value_type = unreal.PropertyBagPropertyType.BOOL, container_type = unreal.PropertyBagContainerType.NONE)\n"
		"block.add_binding(name = \"TestParamBinding\", value_type = unreal.PropertyBagPropertyType.BOOL, container_type = unreal.PropertyBagContainerType.NONE)\n"
		"other_block.add_binding_reference(name = \"TestParamBinding\", referenced_block = block)\n"
		"block.add_graph(name = \"TestGraph\")\n"
		"unreal.EditorAssetLibrary.delete_loaded_asset(block)\n"
		"unreal.EditorAssetLibrary.delete_loaded_asset(other_block)\n"
	);

	IPythonScriptPlugin::Get()->ExecPythonCommand(Script);

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	return true;
}

#endif	// WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR