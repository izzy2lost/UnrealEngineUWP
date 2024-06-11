// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextEdGraphNode.h"

#include "TraitCore/TraitHandle.h"
#include "Graph/RigUnit_AnimNextTraitStack.h"
#include "Graph/RigDecorator_AnimNextCppTrait.h"
#include "RigVMModel/RigVMController.h"
#include "TraitCore/TraitRegistry.h"
#include "ToolMenu.h"

#define LOCTEXT_NAMESPACE "AnimNextEdGraphNode"

void UAnimNextEdGraphNode::GetNodeContextMenuActions(UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const
{
	URigVMEdGraphNode::GetNodeContextMenuActions(Menu, Context);

	if (IsTraitStack())
	{
		FToolMenuSection& Section = Menu->AddSection("AnimNextTraitNodeActions", LOCTEXT("AnimNextTraitNodeActionsMenuHeader", "Anim Next Trait Actions"));

		UAnimNextEdGraphNode* NonConstThis = const_cast<UAnimNextEdGraphNode*>(this);

		Section.AddSubMenu(
			TEXT("AddTraitMenu"),
			LOCTEXT("AddTraitMenu", "Add Trait"),
			LOCTEXT("AddTraitMenuTooltip", "Add the chosen trait to currently selected node"),
			FNewToolMenuDelegate::CreateUObject(NonConstThis, &UAnimNextEdGraphNode::BuildAddTraitContextMenu));
	}
}

void UAnimNextEdGraphNode::ConfigurePin(UEdGraphPin* EdGraphPin, const URigVMPin* ModelPin) const
{
	Super::ConfigurePin(EdGraphPin, ModelPin);

	// Trait handles always remain as a RigVM input pins so that we can still link things to them even if they are hidden
	// We handle visibility for those explicitly here
	const bool bIsInputPin = ModelPin->GetDirection() == ERigVMPinDirection::Input;
	const bool bIsTraitHandle = ModelPin->GetCPPTypeObject() == FAnimNextTraitHandle::StaticStruct();
	if (bIsInputPin && bIsTraitHandle)
	{
		if (const URigVMPin* DecoratorPin = ModelPin->GetParentPin())
		{
			if (DecoratorPin->IsTraitPin())
			{
				check(DecoratorPin->GetScriptStruct() == FRigDecorator_AnimNextCppDecorator::StaticStruct());

				TSharedPtr<FStructOnScope> DecoratorScope = DecoratorPin->GetTraitInstance();
				const FRigDecorator_AnimNextCppDecorator* VMDecorator = (const FRigDecorator_AnimNextCppDecorator*)DecoratorScope->GetStructMemory();

				const UScriptStruct* TraitStruct = VMDecorator->GetTraitSharedDataStruct();
				check(TraitStruct != nullptr);

				const FProperty* PinProperty = TraitStruct->FindPropertyByName(ModelPin->GetFName());
				EdGraphPin->bHidden = PinProperty->HasMetaData(FRigVMStruct::HiddenMetaName);
			}
		}
	}
}

bool UAnimNextEdGraphNode::IsTraitStack() const
{
	if (const URigVMUnitNode* VMNode = Cast<URigVMUnitNode>(GetModelNode()))
	{
		const UScriptStruct* ScriptStruct = VMNode->GetScriptStruct();
		return ScriptStruct == FRigUnit_AnimNextTraitStack::StaticStruct();
	}

	return false;
}

void UAnimNextEdGraphNode::BuildAddTraitContextMenu(UToolMenu* SubMenu)
{
	using namespace UE::AnimNext;

	const FTraitRegistry& TraitRegistry = FTraitRegistry::Get();
	TArray<const FTrait*> Traits = TraitRegistry.GetTraits();

	URigVMController* VMController = GetController();
	URigVMNode* VMNode = GetModelNode();

	const UScriptStruct* CppDecoratorStruct = FRigDecorator_AnimNextCppDecorator::StaticStruct();

	for (const FTrait* Trait : Traits)
	{
		UScriptStruct* ScriptStruct = Trait->GetTraitSharedDataStruct();

		FString DefaultValue;
		{
			const FRigDecorator_AnimNextCppDecorator DefaultCppDecoratorStructInstance;
			FRigDecorator_AnimNextCppDecorator CppDecoratorStructInstance;
			CppDecoratorStructInstance.DecoratorSharedDataStruct = ScriptStruct;

			if (!CppDecoratorStructInstance.CanBeAddedToNode(VMNode, nullptr))
			{
				continue;	// This trait isn't supported on this node
			}

			const FProperty* Prop = FAnimNextCppDecoratorWrapper::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_STRING_CHECKED(FAnimNextCppDecoratorWrapper, CppDecorator));
			Prop->ExportText_Direct(DefaultValue, &CppDecoratorStructInstance, &DefaultCppDecoratorStructInstance, nullptr, PPF_SerializedAsImportText);
		}

		FString DisplayNameMetadata;
		ScriptStruct->GetStringMetaDataHierarchical(FRigVMStruct::DisplayNameMetaName, &DisplayNameMetadata);
		const FString DisplayName = DisplayNameMetadata.IsEmpty() ? Trait->GetTraitName() : DisplayNameMetadata;

		const FText ToolTip = ScriptStruct->GetToolTipText();

		FToolMenuEntry TraitEntry = FToolMenuEntry::InitMenuEntry(
			*Trait->GetTraitName(),
			FText::FromString(DisplayName),
			ToolTip,
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda(
				[this, Trait, VMController, VMNode, CppDecoratorStruct, DefaultValue, DisplayName]()
				{
					VMController->AddTrait(
						VMNode->GetFName(),
						*CppDecoratorStruct->GetPathName(),
						*DisplayName,
						DefaultValue, INDEX_NONE, true, true);
				})
			)
		);

		SubMenu->AddMenuEntry(NAME_None, TraitEntry);
	}
}

#undef LOCTEXT_NAMESPACE
