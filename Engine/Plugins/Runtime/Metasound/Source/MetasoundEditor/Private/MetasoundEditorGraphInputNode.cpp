// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundEditorGraphInputNode.h"

#include "Algo/Count.h"
#include "Algo/Transform.h"
#include "AudioParameter.h"
#include "AudioParameterControllerInterface.h"
#include "EdGraph/EdGraphNode.h"
#include "GraphEditorSettings.h"
#include "MetasoundDataReference.h"
#include "MetasoundEditorGraph.h"
#include "MetasoundEditorGraphNode.h"
#include "MetasoundEditorGraphValidation.h"
#include "MetasoundEditorSettings.h"
#include "MetasoundFrontend.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendLiteral.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendSearchEngine.h"
#include "MetasoundPrimitives.h"
#include "Misc/Guid.h"
#include "Sound/SoundWave.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetasoundEditorGraphInputNode)

#define LOCTEXT_NAMESPACE "MetaSoundEditor"

void UMetasoundEditorGraphInputNode::CacheTitle()
{
	static FText InputDisplayTitle = LOCTEXT("InputNode_Title", "Input");
	CachedTitle = InputDisplayTitle;
}

const FMetasoundEditorGraphVertexNodeBreadcrumb& UMetasoundEditorGraphInputNode::GetBreadcrumb() const
{
	return Breadcrumb;
}

void UMetasoundEditorGraphInputNode::CacheBreadcrumb()
{
	using namespace Metasound::Frontend;

	Breadcrumb = { };

	// Take data from associated input as pasted graph may not be same as local graph
	// and associated input will not be copied with given node.  Need the following data
	// to associate or create new associated input.
	if (Input)
	{
		FConstNodeHandle NodeHandle = Input->GetConstNodeHandle();

		Breadcrumb.MemberName = NodeHandle->GetNodeName();
		Breadcrumb.ClassName = NodeHandle->GetClassMetadata().GetClassName();

		FConstOutputHandle OutputHandle = NodeHandle->GetConstOutputs().Last();
		Breadcrumb.AccessType = OutputHandle->GetVertexAccessType();
		Breadcrumb.DataType = OutputHandle->GetDataType();

		if (const UMetasoundEditorGraphMemberDefaultLiteral* Literal = Input->GetLiteral())
		{
			Breadcrumb.DefaultLiteral = Literal->GetDefault();
		}
	}
}

UMetasoundEditorGraphMember* UMetasoundEditorGraphInputNode::GetMember() const
{
	return Input;
}

FMetasoundFrontendClassName UMetasoundEditorGraphInputNode::GetClassName() const
{
	using namespace Metasound::Frontend;

	if (Input)
	{
		const FMetaSoundFrontendDocumentBuilder& Builder = Input->GetFrontendBuilderChecked();
		if (const FMetasoundFrontendNode* Node = Builder.FindNode(Input->NodeID))
		{
			if (const FMetasoundFrontendClass* Class = Builder.FindDependency(Node->ClassID))
			{
				return Class->Metadata.GetClassName();
			}
		}
	}

	return Breadcrumb.ClassName;
}

void UMetasoundEditorGraphInputNode::UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterControllerInterface>& InParameterInterface) const
{
	if (Input)
	{
		if (UMetasoundEditorGraphMemberDefaultLiteral* DefaultLiteral = Input->GetLiteral())
		{
			DefaultLiteral->UpdatePreviewInstance(InParameterName, InParameterInterface);
		}
	}
}

FGuid UMetasoundEditorGraphInputNode::GetNodeID() const
{
	return NodeID;
}

FLinearColor UMetasoundEditorGraphInputNode::GetNodeTitleColor() const
{
	if (const UMetasoundEditorSettings* EditorSettings = GetDefault<UMetasoundEditorSettings>())
	{
		return EditorSettings->InputNodeTitleColor;
	}

	return Super::GetNodeTitleColor();
}

FSlateIcon UMetasoundEditorGraphInputNode::GetNodeTitleIcon() const
{
	static const FName NativeIconName = "MetasoundEditor.Graph.Node.Class.Input";
	return FSlateIcon("MetaSoundStyle", NativeIconName);
}

void UMetasoundEditorGraphInputNode::GetPinHoverText(const UEdGraphPin& Pin, FString& OutHoverText) const
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	if (ensure(Pin.Direction == EGPD_Output)) // Should never display input pin for input node hover
	{
		if (ensure(Input))
		{
			FConstNodeHandle InputNode = Input->GetConstNodeHandle();
			OutHoverText = InputNode->GetDescription().ToString();
			if (ShowNodeDebugData())
			{
				FConstOutputHandle OutputHandle = FGraphBuilder::FindReroutedConstOutputHandleFromPin(&Pin);
				OutHoverText = FString::Format(TEXT("{0}\nVertex Name: {1}\nDataType: {2}\nID: {3}"),
				{
					OutHoverText,
					OutputHandle->GetName().ToString(),
					OutputHandle->GetDataType().ToString(),
					OutputHandle->GetID().ToString(),
				});
			}
		}
	}
}

void UMetasoundEditorGraphInputNode::ReconstructNode()
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	Super::ReconstructNode();
}

void UMetasoundEditorGraphInputNode::Validate(Metasound::Editor::FGraphNodeValidationResult& OutResult)
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	Super::Validate(OutResult);

	if (const UMetasoundEditorGraphVertex* Vertex = Cast<UMetasoundEditorGraphVertex>(GetMember()))
	{
		FMetasoundFrontendInterface InterfaceToValidate;
		if (Vertex->IsInterfaceMember(&InterfaceToValidate))
		{
			FText RequiredText;
			if (InterfaceToValidate.IsMemberOutputRequired(Vertex->GetMemberName(), RequiredText))
			{
				if (const FMetasoundFrontendNode* Node = GetFrontendNode())
				{
					const TArray<FMetasoundFrontendVertex>& Outputs = Node->Interface.Outputs;
					if (ensure(!Outputs.IsEmpty()))
					{
						const FMetaSoundFrontendDocumentBuilder& Builder = GetBuilderChecked().GetConstBuilder();
						if (!Builder.IsNodeOutputConnected(Node->GetID(), Outputs.Last().VertexID))
						{
							OutResult.SetMessage(EMessageSeverity::Warning, *RequiredText.ToString());
						}
					}
				}
			}
		}
	}
}

FText UMetasoundEditorGraphInputNode::GetTooltipText() const
{
	//If Constructor input
	if (Input && Input->GetVertexAccessType() == EMetasoundFrontendVertexAccessType::Value)
	{
		UMetasoundEditorGraph* Graph = CastChecked<UMetasoundEditorGraph>(GetGraph());
		if (Graph->IsPreviewing())
		{
			FText ToolTip = LOCTEXT("Metasound_ConstructorInputNodeDescription", "Editing constructor values is disabled while previewing.");
			return ToolTip;
		}
	}

	return Super::GetTooltipText();
}

bool UMetasoundEditorGraphInputNode::EnableInteractWidgets() const
{
	using namespace Metasound::Frontend;
	
	//If Constructor input
	if (Input && Input->GetVertexAccessType() == EMetasoundFrontendVertexAccessType::Value)
	{
		UMetasoundEditorGraph* Graph = CastChecked<UMetasoundEditorGraph>(GetGraph());
		return !Graph->IsPreviewing();
	}

	return true;
}
#undef LOCTEXT_NAMESPACE
