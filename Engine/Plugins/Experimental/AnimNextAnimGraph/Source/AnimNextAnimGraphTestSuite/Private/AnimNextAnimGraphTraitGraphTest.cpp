// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextAnimGraphTraitGraphTest.h"

#include "AnimNextRuntimeTest.h"
#include "AnimNextTest.h"
#include "AssetToolsModule.h"
#include "Context.h"
#include "UncookedOnlyUtils.h"
#include "TraitCore/TraitRegistry.h"
#include "TraitInterfaces/IEvaluate.h"
#include "TraitInterfaces/IUpdate.h"
#include "Editor/Transactor.h"
#include "AnimNextExecuteContext.h"
#include "Module/AnimNextModule.h"
#include "Module/AnimNextModule_EditorData.h"
#include "Module/ModuleFactory.h"
#include "Graph/RigDecorator_AnimNextCppTrait.h"
#include "Graph/RigUnit_AnimNextGraphRoot.h"
#include "Graph/RigUnit_AnimNextTraitStack.h"
#include "Misc/AutomationTest.h"
#include "Param/ParamStack.h"
#include "Param/RigVMDispatch_GetParameter.h"
#include "RigVMFunctions/Math/RigVMFunction_MathInt.h"
#include "RigVMFunctions/Math/RigVMFunction_MathFloat.h"

#if WITH_DEV_AUTOMATION_TESTS

//****************************************************************************
// AnimNext Runtime Trait Graph Tests
//****************************************************************************

namespace UE::AnimNext
{
	struct FTestTrait final : FBaseTrait, IEvaluate, IUpdate
	{
		DECLARE_ANIM_TRAIT(FTestTrait, 0x41cecb7f, FBaseTrait)

		using FSharedData = FTestTraitSharedData;

		// IUpdate impl
		virtual void PostUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const override
		{
			IUpdate::PostUpdate(Context, Binding, TraitState);

			const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
			UE::AnimNext::FParamStack& ParamStack = UE::AnimNext::FParamStack::Get();

			int32& UpdateCount = ParamStack.GetMutableParam<int32>("/Engine/Transient.TestAnimNextGraph:UpdateCount");
			UpdateCount++;

			ParamStack.GetMutableParam<int32>("/Engine/Transient.TestAnimNextGraph:SomeInt32") = SharedData->SomeInt32;
			ParamStack.GetMutableParam<float>("/Engine/Transient.TestAnimNextGraph:SomeFloat") = SharedData->SomeFloat;

			ParamStack.GetMutableParam<int32>("/Engine/Transient.TestAnimNextGraph:SomeLatentInt32") = SharedData->GetSomeLatentInt32(Binding);				// MathAdd with constants, latent
			ParamStack.GetMutableParam<int32>("/Engine/Transient.TestAnimNextGraph:SomeOtherLatentInt32") = SharedData->GetSomeOtherLatentInt32(Binding);	// GetParameter, latent
			ParamStack.GetMutableParam<float>("/Engine/Transient.TestAnimNextGraph:SomeLatentFloat") = SharedData->GetSomeLatentFloat(Binding);				// Inline value, not latent
		}

		// IEvaluate impl
		virtual void PostEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const override
		{
			IEvaluate::PostEvaluate(Context, Binding);

			const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
			UE::AnimNext::FParamStack& ParamStack = UE::AnimNext::FParamStack::Get();
			
			int32& EvaluateCount = ParamStack.GetMutableParam<int32>("/Engine/Transient.TestAnimNextGraph:EvaluateCount");
			EvaluateCount++;
		}
	};

	// Trait implementation boilerplate
	#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IEvaluate) \
		GeneratorMacro(IUpdate) \

	GENERATE_ANIM_TRAIT_IMPLEMENTATION(FTestTrait, TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_EVENT_ENUMERATOR)
	#undef TRAIT_INTERFACE_ENUMERATOR

	// --- FTestBasicTrait ---

	struct FTestBasicTrait final : FBaseTrait
	{
		DECLARE_ANIM_TRAIT(FTestBasicTrait, 0x24ce4372, FBaseTrait)

		using FSharedData = FTestTraitSharedData;
	};

	GENERATE_ANIM_TRAIT_IMPLEMENTATION(FTestBasicTrait, NULL_ANIM_TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_EVENT_ENUMERATOR)
	#undef TRAIT_INTERFACE_ENUMERATOR
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextEditorTest_GraphAddTrait, "Animation.AnimNext.Editor.Graph.AddTrait", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextEditorTest_GraphAddTrait::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;

	{
		AUTO_REGISTER_ANIM_TRAIT(FTestTrait)

		FScopedClearNodeTemplateRegistry ScopedClearNodeTemplateRegistry;

		UFactory* GraphFactory = NewObject<UAnimNextModuleFactory>();
		UAnimNextModule* Module = CastChecked<UAnimNextModule>(GraphFactory->FactoryCreateNew(UAnimNextModule::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextGraph"), RF_Transient, nullptr, nullptr, NAME_None));
		UE_RETURN_ON_ERROR(Module != nullptr, "FAnimationAnimNextEditorTest_GraphAddTrait -> Failed to create module");

		UAnimNextModule_EditorData* EditorData = UncookedOnly::FUtils::GetEditorData(Module);
		UE_RETURN_ON_ERROR(EditorData != nullptr, "FAnimationAnimNextEditorTest_GraphAddTrait -> Failed to find module editor data");

		UE_RETURN_ON_ERROR(EditorData->AddAnimationGraph(FRigUnit_AnimNextGraphRoot::DefaultEntryPoint, false) != nullptr, "FAnimationAnimNextEditorTest_GraphAddTrait -> Failed to add animation graph");

		URigVMController* Controller = EditorData->GetRigVMClient()->GetController(EditorData->GetRigVMClient()->GetDefaultModel());
		UE_RETURN_ON_ERROR(Controller != nullptr, "FAnimationAnimNextEditorTest_GraphAddTrait -> Failed to get RigVM controller");

		// Create an empty trait stack node
		URigVMUnitNode* TraitStackNode = Controller->AddUnitNode(FRigUnit_AnimNextTraitStack::StaticStruct(), FRigVMStruct::ExecuteName, FVector2D(0.0f, 0.0f), FString(), false);
		UE_RETURN_ON_ERROR(TraitStackNode != nullptr, "FAnimationAnimNextRuntimeTest_GraphAddTrait -> Failed to create trait stack node");

		// Add a trait
		const UScriptStruct* CppTraitStruct = FRigDecorator_AnimNextCppDecorator::StaticStruct();
		UE_RETURN_ON_ERROR(CppTraitStruct != nullptr, "FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Failed to get find Cpp trait static struct");

		const FTrait* Trait = FTraitRegistry::Get().Find(FTestTrait::TraitUID);
		UE_RETURN_ON_ERROR(Trait != nullptr, "FAnimationAnimNextEditorTest_GraphAddTrait -> Failed to find test trait");

		UScriptStruct* ScriptStruct = Trait->GetTraitSharedDataStruct();
		UE_RETURN_ON_ERROR(ScriptStruct != nullptr, "FAnimationAnimNextEditorTest_GraphAddTrait -> Failed to find trait shared data struct");

		FString DefaultValue;
		{
			const FRigDecorator_AnimNextCppDecorator DefaultCppDecoratorStructInstance;
			FRigDecorator_AnimNextCppDecorator CppDecoratorStructInstance;
			CppDecoratorStructInstance.DecoratorSharedDataStruct = ScriptStruct;

			UE_RETURN_ON_ERROR(CppDecoratorStructInstance.CanBeAddedToNode(TraitStackNode, nullptr), "FAnimationAnimNextEditorTest_GraphAddTrait -> Trait cannot be added to trait stack node");

			const FProperty* Prop = FAnimNextCppDecoratorWrapper::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_STRING_CHECKED(FAnimNextCppDecoratorWrapper, CppDecorator));
			UE_RETURN_ON_ERROR(Prop != nullptr, "FAnimationAnimNextEditorTest_GraphAddTrait -> Failed to find wrapper property");

			Prop->ExportText_Direct(DefaultValue, &CppDecoratorStructInstance, &DefaultCppDecoratorStructInstance, nullptr, PPF_None);
		}

		FString DisplayNameMetadata;
		ScriptStruct->GetStringMetaDataHierarchical(FRigVMStruct::DisplayNameMetaName, &DisplayNameMetadata);
		const FString DisplayName = DisplayNameMetadata.IsEmpty() ? Trait->GetTraitName() : DisplayNameMetadata;

		const FName TraitName = Controller->AddTrait(
			TraitStackNode->GetFName(),
			*CppTraitStruct->GetPathName(),
			*DisplayName,
			DefaultValue, INDEX_NONE, true, true);
		UE_RETURN_ON_ERROR(TraitName == DisplayName, TEXT("FAnimationAnimNextEditorTest_GraphAddTrait -> Unexpected trait name"));

		URigVMPin* TraitPin = TraitStackNode->FindPin(*DisplayName);
		UE_RETURN_ON_ERROR(TraitPin != nullptr, TEXT("FAnimationAnimNextEditorTest_GraphAddTrait -> Failed to find trait pin"));

		// Our first pin is the hard coded output result, trait pins follow
		UE_RETURN_ON_ERROR(TraitStackNode->GetPins().Num() == 2, TEXT("FAnimationAnimNextEditorTest_GraphAddTrait -> Unexpected number of pins"));
		UE_RETURN_ON_ERROR(TraitPin->IsTraitPin() == true, TEXT("FAnimationAnimNextEditorTest_GraphAddTrait -> Unexpected pin type"));
		UE_RETURN_ON_ERROR(TraitPin->GetFName() == TraitName, TEXT("FAnimationAnimNextEditorTest_GraphAddTrait -> Unexpected pin name"));
		UE_RETURN_ON_ERROR(TraitPin->GetCPPTypeObject() == FRigDecorator_AnimNextCppDecorator::StaticStruct(), TEXT("FAnimationAnimNextEditorTest_GraphAddTrait -> Unexpected pin type"));

		// Our first sub-pin is the hard coded script struct member that parametrizes the trait, dynamic trait sub-pins follow
		UE_RETURN_ON_ERROR(TraitPin->GetSubPins().Num() == 6, TEXT("FAnimationAnimNextEditorTest_GraphAddTrait -> Unexpected trait sub pins"));

		// SomeInt32
		UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[1]->GetCPPType() == TEXT("int32"), TEXT("FAnimationAnimNextEditorTest_GraphAddTrait -> Unexpected trait pin type"));
		UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[1]->GetDefaultValue() == TEXT("3"), TEXT("FAnimationAnimNextEditorTest_GraphAddTrait -> Unexpected trait pin value"));
		UE_RETURN_ON_ERROR(!TraitPin->GetSubPins()[1]->IsLazy(), TEXT("FAnimationAnimNextEditorTest_GraphAddTrait -> Expected non-lazy trait pin"));

		// SomeFloat
		UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[2]->GetCPPType() == TEXT("float"), TEXT("FAnimationAnimNextEditorTest_GraphAddTrait -> Unexpected trait pin type"));
		UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[2]->GetDefaultValue() == TEXT("34.000000"), TEXT("FAnimationAnimNextEditorTest_GraphAddTrait -> Unexpected trait pin value"));
		UE_RETURN_ON_ERROR(!TraitPin->GetSubPins()[2]->IsLazy(), TEXT("FAnimationAnimNextEditorTest_GraphAddTrait -> Expected non-lazy trait pin"));

		// SomeLatentInt32
		UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[3]->GetCPPType() == TEXT("int32"), TEXT("FAnimationAnimNextEditorTest_GraphAddTrait -> Unexpected trait pin type"));
		UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[3]->GetDefaultValue() == TEXT("3"), TEXT("FAnimationAnimNextEditorTest_GraphAddTrait -> Unexpected trait pin value"));
		UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[3]->IsLazy(), TEXT("FAnimationAnimNextEditorTest_GraphAddTrait -> Expected lazy trait pin"));

		// SomeOtherLatentInt32
		UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[4]->GetCPPType() == TEXT("int32"), TEXT("FAnimationAnimNextEditorTest_GraphAddTrait -> Unexpected trait pin type"));
		UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[4]->GetDefaultValue() == TEXT("3"), TEXT("FAnimationAnimNextEditorTest_GraphAddTrait -> Unexpected trait pin value"));
		UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[4]->IsLazy(), TEXT("FAnimationAnimNextEditorTest_GraphAddTrait -> Expected lazy trait pin"));

		// SomeLatentFloat
		UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[5]->GetCPPType() == TEXT("float"), TEXT("FAnimationAnimNextEditorTest_GraphAddTrait -> Unexpected trait pin type"));
		UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[5]->GetDefaultValue() == TEXT("34.000000"), TEXT("FAnimationAnimNextEditorTest_GraphAddTrait -> Unexpected trait pin value"));
		UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[5]->IsLazy(), TEXT("FAnimationAnimNextEditorTest_GraphAddTrait -> Expected lazy trait pin"));
	}

	Tests::FUtils::CleanupAfterTests();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextEditorTest_GraphTraitOperations, "Animation.AnimNext.Editor.Graph.TraitOperations", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextEditorTest_GraphTraitOperations::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;

	{
		AUTO_REGISTER_ANIM_TRAIT(FTestTrait)
		AUTO_REGISTER_ANIM_TRAIT(FTestBasicTrait)

		FScopedClearNodeTemplateRegistry ScopedClearNodeTemplateRegistry;

		UFactory* GraphFactory = NewObject<UAnimNextModuleFactory>();
		UAnimNextModule* Module = CastChecked<UAnimNextModule>(GraphFactory->FactoryCreateNew(UAnimNextModule::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextGraph"), RF_Transient, nullptr, nullptr, NAME_None));
		UE_RETURN_ON_ERROR(Module != nullptr, "FAnimationAnimNextEditorTest_GraphTraitOperations -> Failed to create module");

		UAnimNextModule_EditorData* EditorData = UncookedOnly::FUtils::GetEditorData(Module);
		UE_RETURN_ON_ERROR(EditorData != nullptr, "FAnimationAnimNextEditorTest_GraphTraitOperations -> Failed to find module editor data");

		UE_RETURN_ON_ERROR(EditorData->AddAnimationGraph(FRigUnit_AnimNextGraphRoot::DefaultEntryPoint, false) != nullptr, "FAnimationAnimNextEditorTest_GraphTraitOperations -> Failed to add animation graph");

		UAnimNextModule_Controller* Controller = Cast<UAnimNextModule_Controller>(EditorData->GetRigVMClient()->GetController(EditorData->GetRigVMClient()->GetDefaultModel()));
		UE_RETURN_ON_ERROR(Controller != nullptr, "FAnimationAnimNextEditorTest_GraphTraitOperations -> Failed to get RigVM controller");

		// Create an empty trait stack node
		URigVMUnitNode* TraitStackNode = Controller->AddUnitNode(FRigUnit_AnimNextTraitStack::StaticStruct(), FRigVMStruct::ExecuteName, FVector2D(0.0f, 0.0f), FString(), false);
		UE_RETURN_ON_ERROR(TraitStackNode != nullptr, "FAnimationAnimNextEditorTest_GraphTraitOperations -> Failed to create trait stack node");

		const TArray<URigVMPin*>& NodePins = TraitStackNode->GetPins();

		FName TraitInstanceName = NAME_None;

		// --- Add a trait ---
		{
			const FTrait* Trait = FTraitRegistry::Get().Find(FTestTrait::TraitUID);
			UE_RETURN_ON_ERROR(Trait != nullptr, "FAnimationAnimNextEditorTest_GraphTraitOperations -> Failed to find test trait");

			const FName TraitTypeName = *Trait->GetTraitName();

			TraitInstanceName = Controller->AddTraitByName(TraitStackNode->GetFName(), TraitTypeName, INDEX_NONE);
			UE_RETURN_ON_ERROR(TraitInstanceName == TraitTypeName, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected Trait name"));

			URigVMPin* TraitPin = TraitStackNode->FindPin(TraitInstanceName.ToString());
			UE_RETURN_ON_ERROR(TraitPin != nullptr, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Failed to find Trait pin"));

			// Our first pin is the hard coded output result, trait pins follow
			UE_RETURN_ON_ERROR(NodePins.Num() == 2, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected number of pins"));
			UE_RETURN_ON_ERROR(TraitPin->IsTraitPin() == true, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected pin type"));
			UE_RETURN_ON_ERROR(TraitPin->GetFName() == TraitInstanceName, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected pin name"));
			UE_RETURN_ON_ERROR(TraitPin->GetCPPTypeObject() == FRigDecorator_AnimNextCppDecorator::StaticStruct(), TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected pin type"));

			// Our first sub-pin is the hard coded script struct member that parametrizes the trait, dynamic trait sub-pins follow
			UE_RETURN_ON_ERROR(TraitPin->GetSubPins().Num() == 6, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected trait sub pins"));

			// SomeInt32
			UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[1]->GetCPPType() == TEXT("int32"), TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected trait pin type"));
			UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[1]->GetDefaultValue() == TEXT("3"), TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected trait pin value"));
			UE_RETURN_ON_ERROR(!TraitPin->GetSubPins()[1]->IsLazy(), TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Expected non-lazy trait pin"));

			// SomeFloat
			UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[2]->GetCPPType() == TEXT("float"), TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected trait pin type"));
			UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[2]->GetDefaultValue() == TEXT("34.000000"), TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected trait pin value"));
			UE_RETURN_ON_ERROR(!TraitPin->GetSubPins()[2]->IsLazy(), TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Expected non-lazy trait pin"));

			// SomeLatentInt32
			UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[3]->GetCPPType() == TEXT("int32"), TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected trait pin type"));
			UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[3]->GetDefaultValue() == TEXT("3"), TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected trait pin value"));
			UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[3]->IsLazy(), TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Expected lazy trait pin"));

			// SomeOtherLatentInt32
			UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[4]->GetCPPType() == TEXT("int32"), TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected trait pin type"));
			UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[4]->GetDefaultValue() == TEXT("3"), TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected trait pin value"));
			UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[4]->IsLazy(), TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Expected lazy trait pin"));

			// SomeLatentFloat
			UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[5]->GetCPPType() == TEXT("float"), TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected trait pin type"));
			UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[5]->GetDefaultValue() == TEXT("34.000000"), TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected trait pin value"));
			UE_RETURN_ON_ERROR(TraitPin->GetSubPins()[5]->IsLazy(), TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Expected lazy trait pin"));
		}

		// --- Undo Add Trait ---
		{
			Controller->Undo();

			URigVMPin* TraitPin = TraitStackNode->FindPin(TraitInstanceName.ToString());
			UE_RETURN_ON_ERROR(TraitPin == nullptr, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Undo AddTrait failed, Trait pin is still present"));

			// Our first pin is the hard coded output result, trait pins follow
			const URigVMPin* FirstPin = NodePins[0];
			UE_RETURN_ON_ERROR(NodePins.Num() == 1, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected number of pins"));
			UE_RETURN_ON_ERROR(FirstPin->IsTraitPin() == false, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected pin type"));
		}

		// --- Redo Add Trait ---
		{
			Controller->Redo();

			URigVMPin* TraitPin = TraitStackNode->FindPin(TraitInstanceName.ToString());
			UE_RETURN_ON_ERROR(TraitPin != nullptr, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Redo AddTrait failed, can not find Trait pin"));

			// Our first pin is the hard coded output result, trait pins follow
			UE_RETURN_ON_ERROR(NodePins.Num() == 2, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected number of pins"));
			UE_RETURN_ON_ERROR(TraitPin->IsTraitPin() == true, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected pin type"));
			UE_RETURN_ON_ERROR(TraitPin->GetFName() == TraitInstanceName, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected pin name"));
			UE_RETURN_ON_ERROR(TraitPin->GetCPPTypeObject() == FRigDecorator_AnimNextCppDecorator::StaticStruct(), TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected pin type"));
		}

		// --- Remove the created trait ---
		{
			Controller->RemoveTraitByName(TraitStackNode->GetFName(), TraitInstanceName);

			// Our first pin is the hard coded output result, trait pins follow
			UE_RETURN_ON_ERROR(NodePins.Num() == 1, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected number of pins"));

			URigVMPin* DeletedTraitPin = TraitStackNode->FindPin(TraitInstanceName.ToString());
			UE_RETURN_ON_ERROR(DeletedTraitPin == nullptr, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Failed to remove Trait pin"));

			// Only the output result pin should be in the pin array
			const URigVMPin* FirstPin = NodePins[0];
			UE_RETURN_ON_ERROR(FirstPin->IsTraitPin() == false, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected pin type"));
			UE_RETURN_ON_ERROR(FirstPin->GetFName() != TraitInstanceName, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected pin name"));
		}

		// --- Undo Remove Trait ---
		{
			Controller->Undo();

			URigVMPin* TraitPin = TraitStackNode->FindPin(TraitInstanceName.ToString());
			UE_RETURN_ON_ERROR(TraitPin != nullptr, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Undo failed, unable to find Trait pin"));

			// Our first pin is the hard coded output result, trait pins follow
			UE_RETURN_ON_ERROR(NodePins.Num() == 2, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected number of pins"));
			UE_RETURN_ON_ERROR(TraitPin->IsTraitPin() == true, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected pin type"));
			UE_RETURN_ON_ERROR(TraitPin->GetFName() == TraitInstanceName, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected pin name"));
			UE_RETURN_ON_ERROR(TraitPin->GetCPPTypeObject() == FRigDecorator_AnimNextCppDecorator::StaticStruct(), TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected pin type"));
		}

		// --- Swap the FTestTrait with FTestBasicTrait ---
		{
			const FTrait* BasicTrait = FTraitRegistry::Get().Find(FTestBasicTrait::TraitUID);
			UE_RETURN_ON_ERROR(BasicTrait != nullptr, "FAnimationAnimNextEditorTest_GraphTraitOperations -> Failed to find test basic trait");

			const FName BasicTraitTypeName = *BasicTrait->GetTraitName();

			TraitInstanceName = Controller->SwapTraitByName(TraitStackNode->GetFName(), TraitInstanceName, 1, BasicTraitTypeName);
			UE_RETURN_ON_ERROR(TraitInstanceName == BasicTraitTypeName, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected Trait name"));

			URigVMPin* TraitPin = TraitStackNode->FindPin(TraitInstanceName.ToString());
			UE_RETURN_ON_ERROR(TraitPin != nullptr, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Failed to find FTestBasicTrait pin"));

			// Our first pin is the hard coded output result, trait pins follow
			UE_RETURN_ON_ERROR(NodePins.Num() == 2, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected number of pins"));
			UE_RETURN_ON_ERROR(TraitPin->IsTraitPin() == true, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected pin type"));
			UE_RETURN_ON_ERROR(TraitPin->GetFName() == TraitInstanceName, TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected pin name"));
			UE_RETURN_ON_ERROR(TraitPin->GetCPPTypeObject() == FRigDecorator_AnimNextCppDecorator::StaticStruct(), TEXT("FAnimationAnimNextEditorTest_GraphTraitOperations -> Unexpected pin type"));
		}
	}

	Tests::FUtils::CleanupAfterTests();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextRuntimeTest_GraphExecute, "Animation.AnimNext.Runtime.Graph.Execute", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextRuntimeTest_GraphExecute::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;

	{
		AUTO_REGISTER_ANIM_TRAIT(FTestTrait)

		FScopedClearNodeTemplateRegistry ScopedClearNodeTemplateRegistry;

		UFactory* GraphFactory = NewObject<UAnimNextModuleFactory>();
		UAnimNextModule* Module = CastChecked<UAnimNextModule>(GraphFactory->FactoryCreateNew(UAnimNextModule::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextGraph"), RF_Transient, nullptr, nullptr, NAME_None));
		UE_RETURN_ON_ERROR(Module != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecute -> Failed to create module");

		UAnimNextModule_EditorData* EditorData = UncookedOnly::FUtils::GetEditorData(Module);
		UE_RETURN_ON_ERROR(EditorData != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecute -> Failed to find module editor data");

		UE_RETURN_ON_ERROR(EditorData->AddAnimationGraph(FRigUnit_AnimNextGraphRoot::DefaultEntryPoint, false) != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecute -> Failed to add animation graph");

		URigVMController* Controller = EditorData->GetRigVMClient()->GetController(EditorData->GetRigVMClient()->GetDefaultModel());
		UE_RETURN_ON_ERROR(Controller != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecute -> Failed to get RigVM controller");

		// Find graph entry point
		URigVMNode* MainEntryPointNode = Controller->GetGraph()->FindNodeByName(FRigUnit_AnimNextGraphRoot::StaticStruct()->GetFName());
		UE_RETURN_ON_ERROR(MainEntryPointNode != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecute -> Failed to find main entry point node");

		URigVMPin* BeginExecutePin = MainEntryPointNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_AnimNextGraphRoot, Result));
		UE_RETURN_ON_ERROR(BeginExecutePin != nullptr && BeginExecutePin->GetDirection() == ERigVMPinDirection::Input, "FAnimationAnimNextRuntimeTest_GraphExecute -> Failed to create entry point");

		// Create an empty trait stack node
		URigVMUnitNode* DecoratorStackNode = Controller->AddUnitNode(FRigUnit_AnimNextTraitStack::StaticStruct(), FRigVMStruct::ExecuteName, FVector2D(0.0f, 0.0f), FString(), false);
		UE_RETURN_ON_ERROR(DecoratorStackNode != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecute -> Failed to create trait stack node");

		// Link our stack result to our entry point
		Controller->AddLink(DecoratorStackNode->GetPins()[0], MainEntryPointNode->GetPins()[0]);

		// Add a trait
		const UScriptStruct* CppDecoratorStruct = FRigDecorator_AnimNextCppDecorator::StaticStruct();
		UE_RETURN_ON_ERROR(CppDecoratorStruct != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecute -> Failed to get find Cpp trait static struct");

		const FTrait* Trait = FTraitRegistry::Get().Find(FTestTrait::TraitUID);
		UE_RETURN_ON_ERROR(Trait != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecute -> Failed to find test trait");

		UScriptStruct* ScriptStruct = Trait->GetTraitSharedDataStruct();
		UE_RETURN_ON_ERROR(ScriptStruct != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecute -> Failed to find trait shared data struct");

		FString DefaultValue;
		{
			const FRigDecorator_AnimNextCppDecorator DefaultCppDecoratorStructInstance;
			FRigDecorator_AnimNextCppDecorator CppDecoratorStructInstance;
			CppDecoratorStructInstance.DecoratorSharedDataStruct = ScriptStruct;

			UE_RETURN_ON_ERROR(CppDecoratorStructInstance.CanBeAddedToNode(DecoratorStackNode, nullptr), "FAnimationAnimNextRuntimeTest_GraphExecute -> Trait cannot be added to trait stack node");

			const FProperty* Prop = FAnimNextCppDecoratorWrapper::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_STRING_CHECKED(FAnimNextCppDecoratorWrapper, CppDecorator));
			UE_RETURN_ON_ERROR(Prop != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecute -> Failed to find wrapper property");

			Prop->ExportText_Direct(DefaultValue, &CppDecoratorStructInstance, &DefaultCppDecoratorStructInstance, nullptr, PPF_None);
		}

		FString DisplayNameMetadata;
		ScriptStruct->GetStringMetaDataHierarchical(FRigVMStruct::DisplayNameMetaName, &DisplayNameMetadata);
		const FString DisplayName = DisplayNameMetadata.IsEmpty() ? Trait->GetTraitName() : DisplayNameMetadata;

		const FName DecoratorName = Controller->AddTrait(
			DecoratorStackNode->GetFName(),
			*CppDecoratorStruct->GetPathName(),
			*DisplayName,
			DefaultValue, INDEX_NONE, true, true);
		UE_RETURN_ON_ERROR(DecoratorName == DisplayName, TEXT("FAnimationAnimNextRuntimeTest_GraphExecute -> Unexpected trait name"));

		URigVMPin* DecoratorPin = DecoratorStackNode->FindPin(*DisplayName);
		UE_RETURN_ON_ERROR(DecoratorPin != nullptr, TEXT("FAnimationAnimNextRuntimeTest_GraphExecute -> Failed to find trait pin"));

		// Set some values on our trait
		Controller->SetPinDefaultValue(DecoratorPin->GetSubPins()[1]->GetPinPath(), TEXT("78"));
		Controller->SetPinDefaultValue(DecoratorPin->GetSubPins()[2]->GetPinPath(), TEXT("142.33"));

		TSharedRef<FParamStack> ParamStack = MakeShared<FParamStack>();
		FParamStack::AttachToCurrentThread(ParamStack);

		FAnimNextGraphInstancePtr GraphInstance;
		Module->AllocateInstance(GraphInstance);

		FParamStack::FPushedLayerHandle LayerHandle = ParamStack->PushValues(
			"/Engine/Transient.TestAnimNextGraph:UpdateCount", (int32)0,
			"/Engine/Transient.TestAnimNextGraph:EvaluateCount", (int32)0,
			"/Engine/Transient.TestAnimNextGraph:SomeInt32", (int32)0,
			"/Engine/Transient.TestAnimNextGraph:SomeFloat", 0.0f,
			"/Engine/Transient.TestAnimNextGraph:SomeLatentInt32", (int32)0,
			"/Engine/Transient.TestAnimNextGraph:SomeOtherLatentInt32", (int32)0,
			"/Engine/Transient.TestAnimNextGraph:SomeLatentFloat", 0.0f
		);

		{
			UE::AnimNext::FTraitEventList InputEventList;
			UE::AnimNext::FTraitEventList OutputEventList;
			UE::AnimNext::UpdateGraph(GraphInstance, 1.0f / 30.0f, InputEventList, OutputEventList);
			(void)UE::AnimNext::EvaluateGraph(GraphInstance);
		}

		AddErrorIfFalse(ParamStack->GetParam<int32>("/Engine/Transient.TestAnimNextGraph:UpdateCount") == 1, "FAnimationAnimNextRuntimeTest_GraphExecute -> Unexpected update count");
		AddErrorIfFalse(ParamStack->GetParam<int32>("/Engine/Transient.TestAnimNextGraph:EvaluateCount") == 1, "FAnimationAnimNextRuntimeTest_GraphExecute -> Unexpected evaluate count");
		AddErrorIfFalse(ParamStack->GetParam<int32>("/Engine/Transient.TestAnimNextGraph:SomeInt32") == 78, "FAnimationAnimNextRuntimeTest_GraphExecute -> Unexpected SomeInt32 value");
		AddErrorIfFalse(ParamStack->GetParam<float>("/Engine/Transient.TestAnimNextGraph:SomeFloat") == 142.33f, "FAnimationAnimNextRuntimeTest_GraphExecute -> Unexpected SomeFloat value");
		AddErrorIfFalse(ParamStack->GetParam<int32>("/Engine/Transient.TestAnimNextGraph:SomeLatentInt32") == 3, "FAnimationAnimNextRuntimeTest_GraphExecute -> Unexpected SomeLatentInt32 value");
		AddErrorIfFalse(ParamStack->GetParam<int32>("/Engine/Transient.TestAnimNextGraph:SomeOtherLatentInt32") == 3, "FAnimationAnimNextRuntimeTest_GraphExecute -> Unexpected SomeOtherLatentInt32 value");
		AddErrorIfFalse(ParamStack->GetParam<float>("/Engine/Transient.TestAnimNextGraph:SomeLatentFloat") == 34.0f, "FAnimationAnimNextRuntimeTest_GraphExecute -> Unexpected SomeLatentFloat value");

		ParamStack->PopLayer(LayerHandle);
		GraphInstance.Release();

		FParamStack::DetachFromCurrentThread();
	}

	Tests::FUtils::CleanupAfterTests();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextRuntimeTest_GraphExecuteLatent, "Animation.AnimNext.Runtime.Graph.ExecuteLatent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextRuntimeTest_GraphExecuteLatent::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;

	{
		AUTO_REGISTER_ANIM_TRAIT(FTestTrait)
		FScopedClearNodeTemplateRegistry ScopedClearNodeTemplateRegistry;

		UFactory* GraphFactory = NewObject<UAnimNextModuleFactory>();
		UAnimNextModule* Module = CastChecked<UAnimNextModule>(GraphFactory->FactoryCreateNew(UAnimNextModule::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextGraph"), RF_Transient, nullptr, nullptr, NAME_None));
		UE_RETURN_ON_ERROR(Module != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to create module");

		UAnimNextModule_EditorData* EditorData = UncookedOnly::FUtils::GetEditorData(Module);
		UE_RETURN_ON_ERROR(EditorData != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to find module editor data");

		UE_RETURN_ON_ERROR(EditorData->AddAnimationGraph(FRigUnit_AnimNextGraphRoot::DefaultEntryPoint, false) != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to add animation graph");

		UAnimNextModule_Controller* Controller = Cast<UAnimNextModule_Controller>(EditorData->GetRigVMClient()->GetController(EditorData->GetRigVMClient()->GetDefaultModel()));
		UE_RETURN_ON_ERROR(Controller != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to get RigVM controller");

		// Find graph entry point
		URigVMNode* MainEntryPointNode = Controller->GetGraph()->FindNodeByName(FRigUnit_AnimNextGraphRoot::StaticStruct()->GetFName());
		UE_RETURN_ON_ERROR(MainEntryPointNode != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to find main entry point node");

		URigVMPin* BeginExecutePin = MainEntryPointNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_AnimNextGraphRoot, Result));
		UE_RETURN_ON_ERROR(BeginExecutePin != nullptr && BeginExecutePin->GetDirection() == ERigVMPinDirection::Input, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to create entry point");

		// Create an empty trait stack node
		URigVMUnitNode* DecoratorStackNode = Controller->AddUnitNode(FRigUnit_AnimNextTraitStack::StaticStruct(), FRigVMStruct::ExecuteName, FVector2D(0.0f, 0.0f), FString(), false);
		UE_RETURN_ON_ERROR(DecoratorStackNode != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to create trait stack node");

		// Link our stack result to our entry point
		Controller->AddLink(DecoratorStackNode->GetPins()[0], MainEntryPointNode->GetPins()[0]);

		// Add a trait
		const UScriptStruct* CppDecoratorStruct = FRigDecorator_AnimNextCppDecorator::StaticStruct();
		UE_RETURN_ON_ERROR(CppDecoratorStruct != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to get find Cpp trait static struct");

		const FTrait* Trait = FTraitRegistry::Get().Find(FTestTrait::TraitUID);
		UE_RETURN_ON_ERROR(Trait != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to find test trait");

		UScriptStruct* ScriptStruct = Trait->GetTraitSharedDataStruct();
		UE_RETURN_ON_ERROR(ScriptStruct != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to find trait shared data struct");

		FString DefaultValue;
		{
			const FRigDecorator_AnimNextCppDecorator DefaultCppDecoratorStructInstance;
			FRigDecorator_AnimNextCppDecorator CppDecoratorStructInstance;
			CppDecoratorStructInstance.DecoratorSharedDataStruct = ScriptStruct;

			UE_RETURN_ON_ERROR(CppDecoratorStructInstance.CanBeAddedToNode(DecoratorStackNode, nullptr), "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Trait cannot be added to trait stack node");

			const FProperty* Prop = FAnimNextCppDecoratorWrapper::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_STRING_CHECKED(FAnimNextCppDecoratorWrapper, CppDecorator));
			UE_RETURN_ON_ERROR(Prop != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to find wrapper property");

			Prop->ExportText_Direct(DefaultValue, &CppDecoratorStructInstance, &DefaultCppDecoratorStructInstance, nullptr, PPF_None);
		}

		FString DisplayNameMetadata;
		ScriptStruct->GetStringMetaDataHierarchical(FRigVMStruct::DisplayNameMetaName, &DisplayNameMetadata);
		const FString DisplayName = DisplayNameMetadata.IsEmpty() ? Trait->GetTraitName() : DisplayNameMetadata;

		const FName DecoratorName = Controller->AddTrait(
			DecoratorStackNode->GetFName(),
			*CppDecoratorStruct->GetPathName(),
			*DisplayName,
			DefaultValue, INDEX_NONE, true, true);
		UE_RETURN_ON_ERROR(DecoratorName == DisplayName, TEXT("FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Unexpected trait name"));

		// Set some values on our trait
		URigVMPin* DecoratorPin = DecoratorStackNode->FindPin(*DisplayName);
		UE_RETURN_ON_ERROR(DecoratorPin != nullptr, TEXT("FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to find trait pin"));

		Controller->SetPinDefaultValue(DecoratorPin->GetSubPins()[1]->GetPinPath(), TEXT("78"));		// SomeInt32
		Controller->SetPinDefaultValue(DecoratorPin->GetSubPins()[2]->GetPinPath(), TEXT("142.33"));	// SomeFloat
		Controller->SetPinDefaultValue(DecoratorPin->GetSubPins()[5]->GetPinPath(), TEXT("1123.31"));	// SomeLatentFloat, inline value on latent pin

		// Set some latent values on our trait
		{
			FRigVMFunction_MathIntAdd IntAdd;
			IntAdd.A = 10;
			IntAdd.B = 23;

			URigVMUnitNode* IntAddNode = Controller->AddUnitNodeWithDefaults(FRigVMFunction_MathIntAdd::StaticStruct(), FRigStructScope(IntAdd), FRigVMStruct::ExecuteName, FVector2D::ZeroVector, FString(), false);
			UE_RETURN_ON_ERROR(IntAddNode != nullptr, TEXT("FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to create Int add node"));

			Controller->AddLink(
				IntAddNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigVMFunction_MathIntAdd, Result)),
				DecoratorPin->GetSubPins()[3]);	// SomeLatentInt32
		}

		{
			URigVMNode* GetParameterNode = Controller->AddGetAnimNextParameterNode(FVector2D::ZeroVector, UncookedOnly::FUtils::GetQualifiedName(Module, "SomeSourceInt"), FAnimNextParamType::GetType<int32>());
			UE_RETURN_ON_ERROR(GetParameterNode != nullptr, TEXT("FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to create GetParameter node"));

			Controller->AddLink(
				GetParameterNode->FindPin(TEXT("Value")),
				DecoratorPin->GetSubPins()[4]);	// SomeOtherLatentInt32
		}

		TSharedRef<FParamStack> ParamStack = MakeShared<FParamStack>();
		FParamStack::AttachToCurrentThread(ParamStack);

		FAnimNextGraphInstancePtr GraphInstance;
		Module->AllocateInstance(GraphInstance);

		FParamStack::FPushedLayerHandle LayerHandle = ParamStack->PushValues(
			UncookedOnly::FUtils::GetQualifiedName(Module, "UpdateCount"), (int32)0,
			UncookedOnly::FUtils::GetQualifiedName(Module, "EvaluateCount"), (int32)0,
			UncookedOnly::FUtils::GetQualifiedName(Module, "SomeSourceInt"), (int32)1223,
			UncookedOnly::FUtils::GetQualifiedName(Module, "SomeInt32"), (int32)0,
			UncookedOnly::FUtils::GetQualifiedName(Module, "SomeFloat"), 0.0f,
			UncookedOnly::FUtils::GetQualifiedName(Module, "SomeLatentInt32"), (int32)0,
			UncookedOnly::FUtils::GetQualifiedName(Module, "SomeOtherLatentInt32"), (int32)0,
			UncookedOnly::FUtils::GetQualifiedName(Module, "SomeLatentFloat"), 0.0f
		);

		{
			UE::AnimNext::FTraitEventList InputEventList;
			UE::AnimNext::FTraitEventList OutputEventList;
			UE::AnimNext::UpdateGraph(GraphInstance, 1.0f / 30.0f, InputEventList, OutputEventList);
			(void)UE::AnimNext::EvaluateGraph(GraphInstance);
		}

		AddErrorIfFalse(ParamStack->GetParam<int32>(UncookedOnly::FUtils::GetQualifiedName(Module, "UpdateCount")) == 1, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Unexpected update count");
		AddErrorIfFalse(ParamStack->GetParam<int32>(UncookedOnly::FUtils::GetQualifiedName(Module, "EvaluateCount")) == 1, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Unexpected evaluate count");
		AddErrorIfFalse(ParamStack->GetParam<int32>(UncookedOnly::FUtils::GetQualifiedName(Module, "SomeInt32")) == 78, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Unexpected SomeInt32 value");
		AddErrorIfFalse(ParamStack->GetParam<float>(UncookedOnly::FUtils::GetQualifiedName(Module, "SomeFloat")) == 142.33f, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Unexpected SomeFloat value");
		AddErrorIfFalse(ParamStack->GetParam<int32>(UncookedOnly::FUtils::GetQualifiedName(Module, "SomeLatentInt32")) == 33, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Unexpected SomeLatentInt32 value");
		AddErrorIfFalse(ParamStack->GetParam<int32>(UncookedOnly::FUtils::GetQualifiedName(Module, "SomeOtherLatentInt32")) == 1223, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Unexpected SomeOtherLatentInt32 value");
		AddErrorIfFalse(ParamStack->GetParam<float>(UncookedOnly::FUtils::GetQualifiedName(Module, "SomeLatentFloat")) == 1123.31f, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Unexpected SomeLatentFloat value");

		ParamStack->PopLayer(LayerHandle);
		GraphInstance.Release();

		FParamStack::DetachFromCurrentThread();
	}

	Tests::FUtils::CleanupAfterTests();

	return true;
}

#endif
