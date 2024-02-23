// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/SMutableGraphViewer.h"

#include "DesktopPlatformModule.h"
#include "EditorDirectories.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Views/TableViewMetadata.h"
#include "IDesktopPlatform.h"
#include "Misc/Paths.h"
#include "MuCOE/CustomizableObjectCompileRunnable.h"
#include "MuCOE/CustomizableObjectEditorStyle.h"
#include "MuCOE/SMutableCodeViewer.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Input/SNumericDropDown.h"
#include "Widgets/Views/STreeView.h"
#include "ScopedTransaction.h"
#include "MuT/NodeObjectGroup.h"
#include "MuT/NodeObjectNew.h"
#include "MuT/NodeSurfaceEdit.h"
#include "MuT/NodeSurfaceSwitch.h"
#include "MuT/NodeSurfaceVariation.h"
#include "MuT/Streams.h"

// This is necessary because of problems with rtti information in other platforms. In any case, this part of the debugger is only useful in the standard editor.
#if PLATFORM_WINDOWS
#include "MuT/NodeObjectNewPrivate.h"
#include "MuT/NodeObjectGroupPrivate.h"
#include "MuT/NodeSurfaceNewPrivate.h"
#include "MuT/NodeSurfaceEditPrivate.h"
#include "MuT/NodeSurfaceSwitchPrivate.h"
#include "MuT/NodeSurfaceVariationPrivate.h"
#endif

class FExtender;
class FReferenceCollector;
class FUICommandList;
class ITableRow;
class SWidget;
struct FGeometry;
struct FSlateBrush;

#define LOCTEXT_NAMESPACE "SMutableDebugger"


// \todo: multi-column tree
namespace MutableGraphTreeViewColumns
{
	static const FName Name("Name");
};


class SMutableGraphTreeRow : public STableRow<TSharedPtr<FMutableGraphTreeElement>>
{
public:

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<FMutableGraphTreeElement>& InRowItem)
	{
		RowItem = InRowItem;

		const char* TypeName = RowItem->MutableNode->GetType()->m_strName;

		FString LabelString = RowItem->Prefix.IsEmpty() 
			? StringCast<TCHAR>(TypeName).Get() 
			: FString::Printf( TEXT("%s : %s"), *RowItem->Prefix, StringCast<TCHAR>(TypeName).Get() );

		FText MainLabel = FText::FromString(LabelString);
		if (RowItem->DuplicatedOf)
		{
			MainLabel = FText::FromString( FString::Printf(TEXT("%s (Duplicated)"), StringCast<TCHAR>(TypeName).Get()));
		}

		this->ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SExpanderArrow, SharedThis(this))
				.ShouldDrawWires(true)
			]

			+ SHorizontalBox::Slot()
			[
				SNew(STextBlock)
				.Text(MainLabel)
			]
		];

		STableRow< TSharedPtr<FMutableGraphTreeElement> >::ConstructInternal(
			STableRow::FArguments()
			//.Style(FAppStyle::Get(), "DetailsView.TreeView.TableRow")
			.ShowSelection(true)
			, InOwnerTableView
		);

	}


private:

	TSharedPtr<FMutableGraphTreeElement> RowItem;
};


void SMutableGraphViewer::AddReferencedObjects(FReferenceCollector& Collector)
{
	// Add UObjects here if we own any at some point
	//Collector.AddReferencedObject(CustomizableObject);
}


FString SMutableGraphViewer::GetReferencerName() const
{
	return TEXT("SMutableGraphViewer");
}


void SMutableGraphViewer::Construct(const FArguments& InArgs, const mu::NodePtr& InRootNode)
{
	DataTag = InArgs._DataTag;
	ReferencedRuntimeTextures = InArgs._ReferencedRuntimeTextures;
	ReferencedCompileTextures = InArgs._ReferencedCompileTextures;
	RootNode = InRootNode;

	FToolBarBuilder ToolbarBuilder(TSharedPtr<const FUICommandList>(), FMultiBoxCustomization::None, TSharedPtr<FExtender>(), true);
	ToolbarBuilder.SetLabelVisibility(EVisibility::Visible);
	ToolbarBuilder.SetStyle(&FAppStyle::Get(), "SlimToolBar");

	// Export
	ToolbarBuilder.AddToolBarButton(
		FUIAction(
			FExecuteAction::CreateLambda([InRootNode]()
				{
					TArray<FString> SaveFilenames;
					IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
					bool bSave = false;
					if (!DesktopPlatform) return;

					FString LastExportPath = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT);
					FString FileTypes = TEXT("Mutable source data files|*.mutable_source|All files|*.*");
					bSave = DesktopPlatform->SaveFileDialog(
						FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
						TEXT("Export Mutable object"),
						*LastExportPath,
						TEXT("exported.mutable_source"),
						*FileTypes,
						EFileDialogFlags::None,
						SaveFilenames
					);

					if (!bSave) return;

					// Dump source model to a file.
					FString SaveFileName = FString(SaveFilenames[0]);
					mu::OutputFileStream stream(SaveFileName);
					stream.Write(MUTABLE_SOURCE_MODEL_FILETAG, 4);
					mu::OutputArchive arch(&stream);
					mu::Node::Serialise(InRootNode.get(), arch);
					stream.Flush();

					FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_EXPORT, SaveFileName);
				})
			),
			NAME_None,
			LOCTEXT("ExportMutableGraph", "Export"),
			LOCTEXT("ExportMutableGraphTooltip", "Export a debug mutable graph file."),
			FSlateIcon(),
			EUserInterfaceActionType::Button
		);
		
	ToolbarBuilder.EndSection();

	ToolbarBuilder.AddWidget(SNew(STextBlock).Text(MakeAttributeLambda([this]() { return FText::FromString(DataTag); })));

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		[
			ToolbarBuilder.MakeWidget()
		]
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Fill)
		[
			SNew(SSplitter)
			.Orientation(EOrientation::Orient_Horizontal)
			+ SSplitter::Slot()
			.Value(0.25f)
			[
				SNew(SBorder)
				.BorderImage(UE_MUTABLE_GET_BRUSH("ToolPanel.GroupBorder"))
				.Padding(FMargin(4.0f, 4.0f))
				[
					SAssignNew(TreeView, STreeView<TSharedPtr<FMutableGraphTreeElement>>)
					.TreeItemsSource(&RootNodes)
					.OnGenerateRow(this,&SMutableGraphViewer::GenerateRowForNodeTree)
					.OnGetChildren(this, &SMutableGraphViewer::GetChildrenForInfo)
					.OnSetExpansionRecursive(this, &SMutableGraphViewer::TreeExpandRecursive)
					.OnContextMenuOpening(this, &SMutableGraphViewer::OnTreeContextMenuOpening)
					.SelectionMode(ESelectionMode::Single)
					.HeaderRow
					(
						SNew(SHeaderRow)
						+ SHeaderRow::Column(MutableGraphTreeViewColumns::Name)
						.FillWidth(25.f)
						.DefaultLabel(LOCTEXT("Node Name", "Node Name"))
					)
				]
			]
			+ SSplitter::Slot()
			.Value(0.75f)
			[
				SNew(SBorder)
				.BorderImage(UE_MUTABLE_GET_BRUSH("ToolPanel.GroupBorder"))
				.Padding(FMargin(4.0f, 4.0f))
				//[
				//	SubjectsTreeView->AsShared()
				//]
			]
		]
	];
	
	RebuildTree();
}


void SMutableGraphViewer::RebuildTree()
{
	RootNodes.Reset();
	ItemCache.Reset();
	MainItemPerNode.Reset();

	RootNodes.Add(MakeShareable(new FMutableGraphTreeElement(RootNode)));
	TreeView->RequestTreeRefresh();
	TreeExpandUnique();
}


TSharedRef<ITableRow> SMutableGraphViewer::GenerateRowForNodeTree(TSharedPtr<FMutableGraphTreeElement> InTreeNode, const TSharedRef<STableViewBase>& InOwnerTable)
{
	TSharedRef<SMutableGraphTreeRow> Row = SNew(SMutableGraphTreeRow, InOwnerTable, InTreeNode);
	return Row;
}

void SMutableGraphViewer::GetChildrenForInfo(TSharedPtr<FMutableGraphTreeElement> InInfo, TArray<TSharedPtr<FMutableGraphTreeElement>>& OutChildren)
{
// This is necessary because of problems with rtti information in other platforms. In any case, this part of the debugger is only useful in the standard editor.
#if PLATFORM_WINDOWS
	if (!InInfo->MutableNode)
	{
		return;
	}

	// If this is a duplicated of another row, don't provide its children.
	if (InInfo->DuplicatedOf)
	{
		return;
	}

	mu::Node* ParentNode = InInfo->MutableNode.get();
	uint32 InputIndex = 0;

	auto AddChildFunc = [this, ParentNode, &InputIndex, &OutChildren](mu::Node* ChildNode, const FString& Prefix)
	{
		if (ChildNode)
		{
			FItemCacheKey Key = { ParentNode, ChildNode, InputIndex };
			TSharedPtr<FMutableGraphTreeElement>* CachedItem = ItemCache.Find(Key);

			if (CachedItem)
			{
				OutChildren.Add(*CachedItem);
			}
			else
			{
				TSharedPtr<FMutableGraphTreeElement>* MainItemPtr = MainItemPerNode.Find(ChildNode);
				TSharedPtr<FMutableGraphTreeElement> Item = MakeShareable(new FMutableGraphTreeElement(ChildNode, MainItemPtr, Prefix));
				OutChildren.Add(Item);
				ItemCache.Add(Key, Item);

				if (!MainItemPtr)
				{
					MainItemPerNode.Add(ChildNode, Item);
				}
			}
		}
		++InputIndex;
	};

	if (ParentNode->GetType() == mu::NodeObjectNew::GetStaticType())
	{
		mu::NodeObjectNew* ObjectNew = reinterpret_cast<mu::NodeObjectNew*>(ParentNode);
		mu::NodeObjectNew::Private* Private = ObjectNew->GetPrivate();
		for (int32 l = 0; l < Private->m_lods.Num(); ++l)
		{
			AddChildFunc(Private->m_lods[l].get(), TEXT("LOD") );
		}

		for (int32 l = 0; l < Private->m_children.Num(); ++l)
		{
			AddChildFunc(Private->m_children[l].get(), TEXT("CHILD"));
		}
	}

	else if (ParentNode->GetType() == mu::NodeObjectGroup::GetStaticType())
	{
		mu::NodeObjectGroup* ObjectGroup = reinterpret_cast<mu::NodeObjectGroup*>(ParentNode);
		mu::NodeObjectGroup::Private* Private = ObjectGroup->GetPrivate();
		for (int32 l = 0; l < Private->m_children.Num(); ++l)
		{
			AddChildFunc(Private->m_children[l].get(), TEXT("CHILD"));
		}
	}

	else if (ParentNode->GetType() == mu::NodeSurfaceNew::GetStaticType())
	{
		mu::NodeSurfaceNew* SurfaceNew = reinterpret_cast<mu::NodeSurfaceNew*>(ParentNode);
		mu::NodeSurfaceNew::Private* Private = SurfaceNew->GetPrivate();
		for (int32 l = 0; l < Private->m_meshes.Num(); ++l)
		{
			AddChildFunc(Private->m_meshes[l].m_pMesh.get(), TEXT("MESH"));
		}

		for (int32 l = 0; l < Private->m_images.Num(); ++l)
		{
			AddChildFunc(Private->m_images[l].m_pImage.get(), FString::Printf(TEXT("IMAGE [%s]"), *Private->m_images[l].m_name));
		}

		for (int32 l = 0; l < Private->m_vectors.Num(); ++l)
		{
			AddChildFunc(Private->m_vectors[l].m_pVector.get(), FString::Printf(TEXT("VECTOR [%s]"), *Private->m_vectors[l].m_name));
		}

		for (int32 l = 0; l < Private->m_scalars.Num(); ++l)
		{
			AddChildFunc(Private->m_scalars[l].m_pScalar.get(), FString::Printf(TEXT("SCALAR [%s]"), *Private->m_scalars[l].m_name));
		}
	}

	else if (ParentNode->GetType() == mu::NodeSurfaceEdit::GetStaticType())
	{
		mu::NodeSurfaceEdit* SurfaceEdit = reinterpret_cast<mu::NodeSurfaceEdit*>(ParentNode);
		mu::NodeSurfaceEdit::Private* Private = SurfaceEdit->GetPrivate();
		AddChildFunc(Private->m_pMesh.get(), TEXT("MESH"));
		AddChildFunc(Private->m_pMorph.get(), TEXT("MORPH"));
		AddChildFunc(Private->m_pFactor.get(), TEXT("MORPH_FACTOR"));

		for (int32 l = 0; l < Private->m_textures.Num(); ++l)
		{
			AddChildFunc(Private->m_textures[l].m_pExtend.get(), FString::Printf(TEXT("EXTEND [%d]"), l));
			AddChildFunc(Private->m_textures[l].m_pPatch.get(), FString::Printf(TEXT("PATCH [%d]"), l));
		}
	}

	else if (ParentNode->GetType() == mu::NodeSurfaceSwitch::GetStaticType())
	{
		mu::NodeSurfaceSwitch* SurfaceSwitch = reinterpret_cast<mu::NodeSurfaceSwitch*>(ParentNode);
		mu::NodeSurfaceSwitch::Private* Private = SurfaceSwitch->GetPrivate();
		AddChildFunc(Private->Parameter.get(), TEXT("PARAM"));

		for (int32 l = 0; l < Private->Options.Num(); ++l)
		{
			AddChildFunc(Private->Options[l].get(), FString::Printf(TEXT("OPTION [%d]"), l));
		}
	}

	else if (ParentNode->GetType() == mu::NodeSurfaceVariation::GetStaticType())
	{
		mu::NodeSurfaceVariation* SurfaceVar = reinterpret_cast<mu::NodeSurfaceVariation*>(ParentNode);
		mu::NodeSurfaceVariation::Private* Private = SurfaceVar->GetPrivate();
		for (int32 l = 0; l < Private->m_defaultSurfaces.Num(); ++l)
		{
			AddChildFunc(Private->m_defaultSurfaces[l].get(), FString::Printf(TEXT("DEF SURF [%d]"), l));
		}
		for (int32 l = 0; l < Private->m_defaultModifiers.Num(); ++l)
		{
			AddChildFunc(Private->m_defaultModifiers[l].get(), FString::Printf(TEXT("DEF MOD [%d]"), l));
		}

		for (int32 v = 0; v < Private->m_variations.Num(); ++v)
		{
			const mu::NodeSurfaceVariation::Private::FVariation Var = Private->m_variations[v];
			for (int32 l = 0; l < Var.m_surfaces.Num(); ++l)
			{
				AddChildFunc(Var.m_surfaces[l].get(), FString::Printf(TEXT("VAR [%s] SURF [%d]"), *Var.m_tag, l));
			}
			for (int32 l = 0; l < Var.m_modifiers.Num(); ++l)
			{
				AddChildFunc(Var.m_modifiers[l].get(), FString::Printf(TEXT("VAR [%s] MOD [%d]"), *Var.m_tag, l));
			}
		}
	}

	else
	{
		// This node type has not been implemented, so its children won't be added to the tree.
		ensure(false);
	}
#endif
}


TSharedPtr<SWidget> SMutableGraphViewer::OnTreeContextMenuOpening()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Graph_Expand_Instance", "Expand Instance-Level Operations"),
		LOCTEXT("Graph_Expand_Instance_Tooltip", "Expands all the operations in the tree that are instance operations (not images, meshes, booleans, etc.)."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SMutableGraphViewer::TreeExpandUnique)
			//, FCanExecuteAction::CreateSP(this, &SMutableCodeViewer::HasAnyItemInPalette)
		)
	);

	return MenuBuilder.MakeWidget();
}


void SMutableGraphViewer::TreeExpandRecursive(TSharedPtr<FMutableGraphTreeElement> InInfo, bool bExpand)
{
	if (bExpand)
	{
		TreeExpandUnique();
	}
}


void SMutableGraphViewer::TreeExpandUnique()
{
	TArray<TSharedPtr<FMutableGraphTreeElement>> Pending = RootNodes;

	TSet<TSharedPtr<FMutableGraphTreeElement>> Processed;

	TArray<TSharedPtr<FMutableGraphTreeElement>> Children;

	while (!Pending.IsEmpty())
	{
		TSharedPtr<FMutableGraphTreeElement> Item = Pending.Pop();
		TreeView->SetItemExpansion(Item, true);

		Children.SetNum(0);
		GetChildrenForInfo(Item, Children);
		Pending.Append(Children);
	}
}


FReply SMutableGraphViewer::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (TSharedPtr<FExternalDragOperation> DragDropOp = DragDropEvent.GetOperationAs<FExternalDragOperation>())
	{
		if (DragDropOp->HasFiles())
		{
			// For now, only allow a single file.
			const TArray<FString>& Files = DragDropOp->GetFiles();
			if (Files.Num() == 1)
			{
				const FString DraggedFileExtension = FPaths::GetExtension(Files[0], true);
				if (DraggedFileExtension == TEXT(".mutable_source"))
				{
					// Dump source model to a file.
					mu::InputFileStream stream(Files[0]);

					char MutableSourceTag[4] = {};
					stream.Read(MutableSourceTag, 4);

					if (!FMemory::Memcmp(MutableSourceTag, MUTABLE_SOURCE_MODEL_FILETAG, 4))
					{
						return FReply::Handled();
					}

					return FReply::Unhandled();
				}
			}
		}
	}

	return FReply::Unhandled();
}


FReply SMutableGraphViewer::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	SCompoundWidget::OnDrop(MyGeometry, DragDropEvent);

	if (TSharedPtr<FExternalDragOperation> DragDropOp = DragDropEvent.GetOperationAs<FExternalDragOperation>())
	{
		if (DragDropOp->HasFiles())
		{
			// For now, only allow a single file.
			const TArray<FString>& Files = DragDropOp->GetFiles();
			if (Files.Num() == 1)
			{
				const FString DraggedFileExtension = FPaths::GetExtension(Files[0], true);
				if (DraggedFileExtension == TEXT(".mutable_source"))
				{
					// Dump source model to a file.
					mu::InputFileStream stream(Files[0]);

					char MutableSourceTag[4] = {};
					stream.Read(MutableSourceTag, 4);

					if (!FMemory::Memcmp(MutableSourceTag, MUTABLE_SOURCE_MODEL_FILETAG, 4))
					{
						mu::InputArchive arch(&stream);
						RootNode = mu::Node::StaticUnserialise( arch );
						DataTag = FString("dropped-file ") + FPaths::GetCleanFilename(Files[0]);
						RebuildTree();

						return FReply::Handled();
					}

					return FReply::Unhandled();
				}
			}
		}

		return FReply::Unhandled();
	}

	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE 
