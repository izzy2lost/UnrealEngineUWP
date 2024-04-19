// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundEditorGraphCommentNode.h"

#include "Internationalization/Internationalization.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "Layout/SlateRect.h"
#include "MetasoundBuilderSubsystem.h"
#include "MetasoundUObjectRegistry.h"
#include "Styling/AppStyle.h"
#include "Templates/Casts.h"
#include "UObject/UnrealType.h"

class UEdGraphPin;

#define LOCTEXT_NAMESPACE "MetasoundEditor"


FMetasoundAssetBase& UMetasoundEditorGraphCommentNode::GetAssetChecked()
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	UObject* Outermost = GetOutermostObject();
	check(Outermost);

	FMetasoundAssetBase* MetaSound = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(Outermost);
	check(MetaSound);
	return *MetaSound;
}

void UMetasoundEditorGraphCommentNode::ConvertToFrontendComment(const UEdGraphNode_Comment& InEdNode, FMetaSoundFrontendGraphComment& OutComment)
{
	OutComment.bColorBubble = InEdNode.bColorCommentBubble != 0;
	OutComment.Color = InEdNode.CommentColor;
	OutComment.Comment = InEdNode.NodeComment;
	OutComment.Depth = InEdNode.CommentDepth;
	OutComment.FontSize = InEdNode.FontSize;
	OutComment.MoveMode = InEdNode.MoveMode == ECommentBoxMode::GroupMovement ? EMetaSoundFrontendGraphCommentMoveMode::GroupMovement : EMetaSoundFrontendGraphCommentMoveMode::NoGroupMovement;
	OutComment.Position = FVector2D(InEdNode.NodePosX, InEdNode.NodePosY);
	OutComment.Size = FVector2D(InEdNode.NodeWidth, InEdNode.NodeHeight);
}

void UMetasoundEditorGraphCommentNode::ConvertFromFrontendComment(const FMetaSoundFrontendGraphComment& InComment, UEdGraphNode_Comment& OutEdNode)
{
	OutEdNode.bColorCommentBubble = (uint32)InComment.bColorBubble;
	OutEdNode.CommentColor = InComment.Color;
	OutEdNode.NodeComment = InComment.Comment;
	OutEdNode.CommentDepth = InComment.Depth;
	OutEdNode.FontSize = InComment.FontSize;
	OutEdNode.MoveMode = InComment.MoveMode == EMetaSoundFrontendGraphCommentMoveMode::GroupMovement ? ECommentBoxMode::GroupMovement : ECommentBoxMode::NoGroupMovement;
	OutEdNode.NodePosX = InComment.Position.X;
	OutEdNode.NodePosY = InComment.Position.Y;
	OutEdNode.NodeWidth = InComment.Size.X;
	OutEdNode.NodeHeight = InComment.Size.Y;
}

void UMetasoundEditorGraphCommentNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// TODO: Check property name and only change that which changed
	FMetaSoundFrontendGraphComment Comment;
	ConvertToFrontendComment(*this, Comment);

	UMetaSoundBuilderBase& Builder = UMetaSoundBuilderSubsystem::GetChecked().AttachBuilderToAssetChecked(*GetOutermostObject());
	Builder.FindOrAddGraphComment(CommentID) = MoveTemp(Comment);
}

void UMetasoundEditorGraphCommentNode::PostPlacedNewNode()
{
	Super::PostPlacedNewNode();

	UMetaSoundBuilderBase& Builder = UMetaSoundBuilderSubsystem::GetChecked().AttachBuilderToAssetChecked(*GetOutermostObject());

	// If the frontend version predates this new node, sync this node's comment to it...
	if (const FMetaSoundFrontendGraphComment* Comment = Builder.FindGraphComment(CommentID))
	{
		NodeComment = Comment->Comment;
	}

	// otherwise, initialize the frontend version with the ed graph's default comment.
	else
	{
		Builder.FindOrAddGraphComment(CommentID).Comment = NodeComment;
	}
}

void UMetasoundEditorGraphCommentNode::ResizeNode(const FVector2D& NewSize)
{
	Super::ResizeNode(NewSize);
	if (bCanResizeNode) 
	{
		UMetaSoundBuilderBase& Builder = UMetaSoundBuilderSubsystem::GetChecked().AttachBuilderToAssetChecked(*GetOutermostObject());
		Builder.FindOrAddGraphComment(CommentID).Size = FVector2D(NodeWidth, NodeHeight);
	}
}

FGuid UMetasoundEditorGraphCommentNode::GetCommentID() const
{
	return CommentID;
}

void UMetasoundEditorGraphCommentNode::SetBounds(const class FSlateRect& Rect)
{
	Super::SetBounds(Rect);

	UMetaSoundBuilderBase& Builder = UMetaSoundBuilderSubsystem::GetChecked().AttachBuilderToAssetChecked(*GetOutermostObject());
	FMetaSoundFrontendGraphComment& FrontendComment = Builder.FindOrAddGraphComment(CommentID);
	FrontendComment.Position = FVector2D(NodePosX, NodePosY);
	FrontendComment.Size = FVector2D(NodeWidth, NodeHeight);
}

void UMetasoundEditorGraphCommentNode::OnRenameNode(const FString& NewName)
{
	Super::OnRenameNode(NewName);

	UMetaSoundBuilderBase& Builder = UMetaSoundBuilderSubsystem::GetChecked().AttachBuilderToAssetChecked(*GetOutermostObject());
	Builder.FindOrAddGraphComment(CommentID).Comment = NewName;
}
#undef LOCTEXT_NAMESPACE
