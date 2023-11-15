// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/SModularRigHierarchy.h"
#include "Widgets/Input/SComboButton.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "Editor/ControlRigEditor.h"
#include "BlueprintActionDatabase.h"
#include "BlueprintVariableNodeSpawner.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "K2Node_VariableGet.h"
#include "RigVMBlueprintUtils.h"
#include "ControlRigModularRigHierarchyCommands.h"
#include "ControlRigBlueprint.h"
#include "Graph/ControlRigGraph.h"
#include "Graph/ControlRigGraphNode.h"
#include "Graph/ControlRigGraphSchema.h"
#include "GraphEditorModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AnimationRuntime.h"
#include "ClassViewerFilter.h"
#include "PropertyCustomizationHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor/EditorEngine.h"
#include "HelperUtil.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "ControlRig.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformTime.h"
#include "Dialogs/Dialogs.h"
#include "IPersonaToolkit.h"
#include "SKismetInspector.h"
#include "Types/WidgetActiveTimerDelegate.h"
#include "Dialog/SCustomDialog.h"
#include "EditMode/ControlRigEditMode.h"
#include "ToolMenus.h"
#include "Editor/ControlRigContextMenuContext.h"
#include "Editor/SRigSpacePickerWidget.h"
#include "Settings/ControlRigSettings.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Styling/AppStyle.h"
#include "ControlRigSkeletalMeshComponent.h"
#include "Sequencer/ControlRigLayerInstance.h"
#include "Algo/MinElement.h"
#include "Algo/MaxElement.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Kismet2/SClassPickerDialog.h"
#include "RigVMFunctions/Math/RigVMMathLibrary.h"
#include "Preferences/PersonaOptions.h"

#define LOCTEXT_NAMESPACE "SModularRigHierarchy"

//////////////////////////////////////////////////////////////
/// FModuleRigHierarchyDragDropOp
///////////////////////////////////////////////////////////
TSharedRef<FModuleRigHierarchyDragDropOp> FModuleRigHierarchyDragDropOp::New(const TArray<FString>& InElements)
{
	TSharedRef<FModuleRigHierarchyDragDropOp> Operation = MakeShared<FModuleRigHierarchyDragDropOp>();
	Operation->Elements = InElements;
	Operation->Construct();
	return Operation;
}

TSharedPtr<SWidget> FModuleRigHierarchyDragDropOp::GetDefaultDecorator() const
{
	return SNew(SBorder)
		.Visibility(EVisibility::Visible)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		[
			SNew(STextBlock)
			.Text(FText::FromString(GetJoinedElementNames()))
			//.Font(FAppStyle::Get().GetFontStyle("FontAwesome.10"))
		];
}

FString FModuleRigHierarchyDragDropOp::GetJoinedElementNames() const
{
	TArray<FString> ElementNameStrings;
	for (const FString& Element: Elements)
	{
		ElementNameStrings.Add(Element);
	}
	return FString::Join(ElementNameStrings, TEXT(","));
}

///////////////////////////////////////////////////////////

const FName SModularRigHierarchy::ContextMenuName = TEXT("ControlRigEditor.ModularRigHierarchy.ContextMenu");

SModularRigHierarchy::~SModularRigHierarchy()
{
	const FControlRigEditor* Editor = ControlRigEditor.IsValid() ? ControlRigEditor.Pin().Get() : nullptr;
	OnEditorClose(Editor, ControlRigBlueprint.Get());
}

void SModularRigHierarchy::Construct(const FArguments& InArgs, TSharedRef<FControlRigEditor> InControlRigEditor)
{
	ControlRigEditor = InControlRigEditor;

	ControlRigBlueprint = ControlRigEditor.Pin()->GetControlRigBlueprint();

	ControlRigBlueprint->OnRefreshEditor().AddRaw(this, &SModularRigHierarchy::HandleRefreshEditorFromBlueprint);
	ControlRigBlueprint->OnSetObjectBeingDebugged().AddRaw(this, &SModularRigHierarchy::HandleSetObjectBeingDebugged);
	ControlRigBlueprint->OnModularRigPreCompiled().AddRaw(this, &SModularRigHierarchy::HandlePreCompileModularRigs);
	ControlRigBlueprint->OnModularRigCompiled().AddRaw(this, &SModularRigHierarchy::HandlePostCompileModularRigs);

	// for deleting, renaming, dragging
	CommandList = MakeShared<FUICommandList>();

	UEditorEngine* Editor = Cast<UEditorEngine>(GEngine);
	if (Editor != nullptr)
	{
		Editor->RegisterForUndo(this);
	}

	BindCommands();

	// setup all delegates for the rig hierarchy widget
	FModularRigTreeDelegates Delegates;
	Delegates.OnGetHierarchy = FOnGetModularRigTreeHierarchy::CreateSP(this, &SModularRigHierarchy::GetHierarchyForTreeView);
	Delegates.OnContextMenuOpening = FOnContextMenuOpening::CreateSP(this, &SModularRigHierarchy::CreateContextMenuWidget);
	Delegates.OnDragDetected = FOnDragDetected::CreateSP(this, &SModularRigHierarchy::OnDragDetected);
	Delegates.OnCanAcceptDrop = FOnModularRigTreeCanAcceptDrop::CreateSP(this, &SModularRigHierarchy::OnCanAcceptDrop);
	Delegates.OnAcceptDrop = FOnModularRigTreeAcceptDrop::CreateSP(this, &SModularRigHierarchy::OnAcceptDrop);
	Delegates.OnMouseButtonClick = FOnModularRigTreeMouseButtonClick::CreateSP(this, &SModularRigHierarchy::OnItemClicked);
	Delegates.OnMouseButtonDoubleClick = FOnModularRigTreeMouseButtonClick::CreateSP(this, &SModularRigHierarchy::OnItemDoubleClicked);
	Delegates.OnRequestDetailsInspection = FOnModularRigTreeRequestDetailsInspection::CreateSP(this, &SModularRigHierarchy::OnRequestDetailsInspection);
	Delegates.OnRenameElement = FOnModularRigTreeRenameElement::CreateSP(this, &SModularRigHierarchy::HandleRenameModule);
	Delegates.OnVerifyModuleNameChanged = FOnModularRigTreeVerifyElementNameChanged::CreateSP(this, &SModularRigHierarchy::HandleVerifyNameChanged);
	
	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(0.0f, 0.0f)
		[
			SNew(SBorder)
			.Padding(0.0f)
			.ShowEffectWhenDisabled(false)
			[
				SNew(SBorder)
				.Padding(2.0f)
				.BorderImage(FAppStyle::GetBrush("SCSEditor.TreePanel"))
				[
					SAssignNew(TreeView, SModularRigHierarchyTreeView)
					.RigTreeDelegates(Delegates)
					.AutoScrollEnabled(true)
				]
			]
		]
	];

	RefreshTreeView();

	if (ControlRigEditor.IsValid())
	{
		ControlRigEditor.Pin()->GetKeyDownDelegate().BindLambda([&](const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)->FReply {
			return OnKeyDown(MyGeometry, InKeyEvent);
		});
		ControlRigEditor.Pin()->OnGetViewportContextMenu().BindSP(this, &SModularRigHierarchy::GetContextMenu);
		ControlRigEditor.Pin()->OnViewportContextMenuCommands().BindSP(this, &SModularRigHierarchy::GetContextMenuCommands);
		ControlRigEditor.Pin()->OnEditorClosed().AddSP(this, &SModularRigHierarchy::OnEditorClose);
	}
	
	CreateContextMenu();
}

void SModularRigHierarchy::OnEditorClose(const FRigVMEditor* InEditor, URigVMBlueprint* InBlueprint)
{
	if (InEditor)
	{
		FControlRigEditor* Editor = (FControlRigEditor*)InEditor;  
		Editor->OnGetViewportContextMenu().Unbind();
		Editor->OnViewportContextMenuCommands().Unbind();
	}

	if (UControlRigBlueprint* BP = Cast<UControlRigBlueprint>(InBlueprint))
	{
		InBlueprint->OnRefreshEditor().RemoveAll(this);
		InBlueprint->OnSetObjectBeingDebugged().RemoveAll(this);
		BP->OnModularRigPreCompiled().RemoveAll(this);
		BP->OnModularRigCompiled().RemoveAll(this);
	}
	
	ControlRigEditor.Reset();
	ControlRigBlueprint.Reset();
}

void SModularRigHierarchy::BindCommands()
{
	// create new command
	const FControlRigModularHierarchyCommands& Commands = FControlRigModularHierarchyCommands::Get();

	CommandList->MapAction(Commands.AddModuleItem,
		FExecuteAction::CreateSP(this, &SModularRigHierarchy::HandleNewItem),
		FCanExecuteAction());

	CommandList->MapAction(Commands.RenameModuleItem,
		FExecuteAction::CreateSP(this, &SModularRigHierarchy::HandleRenameModule),
		FCanExecuteAction());

	CommandList->MapAction(Commands.DeleteModuleItem,
		FExecuteAction::CreateSP(this, &SModularRigHierarchy::HandleDeleteModules),
		FCanExecuteAction());
}

FReply SModularRigHierarchy::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList.IsValid() && CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SModularRigHierarchy::RefreshTreeView(bool bRebuildContent)
{
	const UModularRig* Hierarchy = GetHierarchy();
	bool bDummySuspensionFlag = false;
	bool* SuspensionFlagPtr = &bDummySuspensionFlag;
	if (ControlRigEditor.IsValid())
	{
		SuspensionFlagPtr = &ControlRigEditor.Pin()->GetSuspendDetailsPanelRefreshFlag();
	}
	TGuardValue<bool> SuspendDetailsPanelRefreshGuard(*SuspensionFlagPtr, true);

	TreeView->RefreshTreeView(bRebuildContent);
}

TArray<FString> SModularRigHierarchy::GetSelectedKeys() const
{
	TArray<TSharedPtr<FModularRigTreeElement>> SelectedItems = TreeView->GetSelectedItems();
	
	TArray<FString> SelectedKeys;
	for (const TSharedPtr<FModularRigTreeElement>& SelectedItem : SelectedItems)
	{
		if(!SelectedItem->Key.IsEmpty())
		{
			SelectedKeys.AddUnique(SelectedItem->Key);
		}
	}

	return SelectedKeys;
}


void SModularRigHierarchy::HandlePreCompileModularRigs(URigVMBlueprint* InBlueprint)
{
	ClearDetailPanel();
}

void SModularRigHierarchy::HandlePostCompileModularRigs(URigVMBlueprint* InBlueprint)
{
	RefreshTreeView();
	if (ControlRigEditor.IsValid())
	{
		TArray<TSharedPtr<FModularRigTreeElement>> SelectedElements;
		Algo::Transform(ControlRigEditor.Pin()->ModulesSelected, SelectedElements, [this](const FString& Path)
		{
			return TreeView->FindElement(Path);
		});
		TreeView->SetSelection(SelectedElements);
		ControlRigEditor.Pin()->RefreshDetailView();
	}
}

void SModularRigHierarchy::HandleRefreshEditorFromBlueprint(URigVMBlueprint* InBlueprint)
{
	RefreshTreeView();
}

void SModularRigHierarchy::HandleSetObjectBeingDebugged(UObject* InObject)
{
	if(ControlRigBeingDebuggedPtr.Get() == InObject)
	{
		return;
	}

	ControlRigBeingDebuggedPtr.Reset();
	
	if(UModularRig* ControlRig = Cast<UModularRig>(InObject))
	{
		ControlRigBeingDebuggedPtr = ControlRig;
	}

	RefreshTreeView();
}

TSharedPtr< SWidget > SModularRigHierarchy::CreateContextMenuWidget()
{
	UToolMenus* ToolMenus = UToolMenus::Get();

	if (UToolMenu* Menu = GetContextMenu())
	{
		return ToolMenus->GenerateWidget(Menu);
	}
	
	return SNullWidget::NullWidget;
}

void SModularRigHierarchy::OnItemClicked(TSharedPtr<FModularRigTreeElement> InItem)
{
	UModularRig* Rig = GetHierarchy();
	check(Rig);

	if (ControlRigEditor.IsValid() && InItem.IsValid())
	{
		ControlRigEditor.Pin()->SetDetailViewForRigModules({InItem->Key});
	}
}

void SModularRigHierarchy::OnItemDoubleClicked(TSharedPtr<FModularRigTreeElement> InItem)
{

}

void SModularRigHierarchy::CreateContextMenu()
{
	static bool bCreatedMenu = false;
	if(bCreatedMenu)
	{
		return;
	}
	bCreatedMenu = true;
	
	const FName MenuName = ContextMenuName;

	UToolMenus* ToolMenus = UToolMenus::Get();
	
	if (!ensure(ToolMenus))
	{
		return;
	}

	if (UToolMenu* Menu = ToolMenus->ExtendMenu(MenuName))
	{
		Menu->AddDynamicSection(NAME_None, FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
			{
				UControlRigContextMenuContext* MainContext = InMenu->FindContext<UControlRigContextMenuContext>();
				
				if (SModularRigHierarchy* RigHierarchyPanel = MainContext->GetModularRigHierarchyPanel())
				{
					const FControlRigModularHierarchyCommands& Commands = FControlRigModularHierarchyCommands::Get(); 
				
					FToolMenuSection& ElementsSection = InMenu->AddSection(TEXT("Elements"), LOCTEXT("ElementsHeader", "Elements"));
					ElementsSection.AddSubMenu(TEXT("New"), LOCTEXT("New", "New"), LOCTEXT("New_ToolTip", "Create New Elements"),
						FNewToolMenuDelegate::CreateLambda([Commands, RigHierarchyPanel](UToolMenu* InSubMenu)
						{
							FToolMenuSection& DefaultSection = InSubMenu->AddSection(NAME_None);
							DefaultSection.AddMenuEntry(Commands.AddModuleItem);
						})
					);
					ElementsSection.AddMenuEntry(Commands.RenameModuleItem);
					ElementsSection.AddMenuEntry(Commands.DeleteModuleItem);
				}
			})
		);
	}
}

UToolMenu* SModularRigHierarchy::GetContextMenu()
{
	const FName MenuName = ContextMenuName;
	UToolMenus* ToolMenus = UToolMenus::Get();

	if(!ensure(ToolMenus))
	{
		return nullptr;
	}

	// individual entries in this menu can access members of this context, particularly useful for editor scripting
	UControlRigContextMenuContext* ContextMenuContext = NewObject<UControlRigContextMenuContext>();
	FControlRigMenuSpecificContext MenuSpecificContext;
	MenuSpecificContext.ModularRigHierarchyPanel = SharedThis(this);
	ContextMenuContext->Init(ControlRigEditor, MenuSpecificContext);

	FToolMenuContext MenuContext(CommandList);
	MenuContext.AddObject(ContextMenuContext);

	UToolMenu* Menu = ToolMenus->GenerateMenu(MenuName, MenuContext);

	return Menu;
}

TSharedPtr<FUICommandList> SModularRigHierarchy::GetContextMenuCommands() const
{
	return CommandList;
}

bool SModularRigHierarchy::IsSingleSelected() const
{
	if(GetSelectedKeys().Num() == 1)
	{
		return true;
	}
	return false;
}

/** Filter class to show only RigModules. */
class FClassViewerRigModulesFilter : public IClassViewerFilter
{
public:
	FClassViewerRigModulesFilter()
		: AssetRegistry(FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get())
	{}
	
	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		if(InClass)
		{
			const bool bChildOfObjectClass = InClass->IsChildOf(UControlRig::StaticClass());
			const bool bMatchesFlags = !InClass->HasAnyClassFlags(CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated | CLASS_Abstract);
			const bool bNotNative = !InClass->IsNative();

			// Allow any class contained in the extra picker common classes array
			if (InInitOptions.ExtraPickerCommonClasses.Contains(InClass))
			{
				return true;
			}
			
			if (bChildOfObjectClass && bMatchesFlags && bNotNative)
			{
				const FAssetData AssetData(InClass);
				return MatchesFilter(AssetData);
			}
		}
		return false;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		const bool bChildOfObjectClass = InUnloadedClassData->IsChildOf(UControlRig::StaticClass());
		const bool bMatchesFlags = !InUnloadedClassData->HasAnyClassFlags(CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated | CLASS_Abstract);
		if (bChildOfObjectClass && bMatchesFlags)
		{
			const FString GeneratedClassPathString = InUnloadedClassData->GetClassPathName().ToString();
			const FString BlueprintPath = GeneratedClassPathString.LeftChop(2); // Chop off _C
			const FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(BlueprintPath));
			return MatchesFilter(AssetData);

		}
		return false;
	}

private:
	bool MatchesFilter(const FAssetData& AssetData)
	{
		static const UEnum* ControlTypeEnum = StaticEnum<EControlRigType>();
		const FString ControlRigTypeStr = AssetData.GetTagValueRef<FString>(TEXT("ControlRigType"));
		if (ControlRigTypeStr.IsEmpty())
		{
			return false;
		}

		const EControlRigType ControlRigType = (EControlRigType)(ControlTypeEnum->GetValueByName(*ControlRigTypeStr));
		return ControlRigType == EControlRigType::RigModule;
	}

	const IAssetRegistry& AssetRegistry;
};

/** Create Item */
void SModularRigHierarchy::HandleNewItem()
{
	if(!ControlRigEditor.IsValid())
	{
		return;
	}

	FString ParentPath;
	if (IsSingleSelected())
	{
		ParentPath = GetSelectedKeys()[0];
	}
	
	FClassViewerInitializationOptions Options;
	Options.bShowUnloadedBlueprints = true;
	Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;

	TSharedPtr<FClassViewerRigModulesFilter> ClassFilter = MakeShareable(new FClassViewerRigModulesFilter());
	Options.ClassFilters.Add(ClassFilter.ToSharedRef());
	Options.bShowNoneOption = false;
	
	UClass* ChosenClass;
	const FText TitleText = LOCTEXT("ModularRigHierarchy", "Pick Rig Module Class");
	const bool bPressedOk = SClassPickerDialog::PickClass(TitleText, Options, ChosenClass, UControlRig::StaticClass());
	if (bPressedOk)
	{
		HandleNewItem(ChosenClass, ParentPath);
	}
}

void SModularRigHierarchy::HandleNewItem(UClass* InClass, const FString &InParentPath)
{
	UControlRig* ControlRig = InClass->GetDefaultObject<UControlRig>();
	if (!ControlRig)
	{
		return;
	}

	FSlateApplication::Get().DismissAllMenus();
	
	if (ControlRigBlueprint.IsValid())
	{
		FString ClassName = InClass->GetName();
		ClassName.RemoveFromEnd(TEXT("_C"));
		FString PathName = InParentPath.IsEmpty() ? *ClassName : FString::Printf(TEXT("%s:%s"), *InParentPath, *ClassName);
		const FName Name = CreateUniqueName(*PathName);
		ControlRigBlueprint->GetModularRigController()->AddModule(Name, InClass, InParentPath);

		FString NewPathName = InParentPath.IsEmpty() ? *Name.ToString() : FString::Printf(TEXT("%s:%s"), *InParentPath, *Name.ToString());
		TSharedPtr<FModularRigTreeElement> Element = TreeView->FindElement(NewPathName);
		if (Element.IsValid())
		{
			TreeView->SetSelection({Element});
			TreeView->bRequestRenameSelected = true;
		}
	}
}

bool SModularRigHierarchy::CanRenameModule() const
{
	return IsSingleSelected();
}

void SModularRigHierarchy::HandleRenameModule()
{
	if(!ControlRigEditor.IsValid())
	{
		return;
	}

	if (!CanRenameModule())
	{
		return;
	}

	UModularRig* Rig = GetDefaultHierarchy();
	if (Rig)
	{
		FScopedTransaction Transaction(LOCTEXT("ModularRigHierarchyRenameSelected", "Rename selected module"));

		TArray<TSharedPtr<FModularRigTreeElement>> SelectedItems = TreeView->GetSelectedItems();
		if (SelectedItems.Num() == 1)
		{
			SelectedItems[0]->RequestRename();
		}
	}

	return;
}

FName SModularRigHierarchy::HandleRenameModule(const FString& InOldPath, const FName& InNewName)
{
	ClearDetailPanel();
	
	if (ControlRigBlueprint.IsValid())
	{
		FScopedTransaction Transaction(LOCTEXT("ModularRigHierarchyRename", "Rename Module"));

		UModularRigController* Controller = ControlRigBlueprint->GetModularRigController();
		check(Controller);

		FName ResultingName = NAME_None;
		if (Controller->RenameModule(InOldPath, InNewName))
		{
			return InNewName;
		}
	}

	return NAME_None;
}

bool SModularRigHierarchy::HandleVerifyNameChanged(const FString& InOldPath, const FName& InNewName, FText& OutErrorMessage)
{
	if (InNewName.IsNone())
	{
		return false;
	}
	
	if (ControlRigBlueprint.IsValid())
	{
		UModularRigController* Controller = ControlRigBlueprint->GetModularRigController();
		check(Controller);

		return Controller->CanRenameModule(InOldPath, InNewName, OutErrorMessage);
	}

	return false;
}

void SModularRigHierarchy::HandleDeleteModules()
{
	if(!ControlRigEditor.IsValid())
	{
		return;
	}

	UModularRig* Rig = GetDefaultHierarchy();
	if (Rig)
	{
		FScopedTransaction Transaction(LOCTEXT("ModularRigHierarchyDeleteSelected", "Delete selected modules"));

		TArray<TSharedPtr<FModularRigTreeElement>> SelectedItems = TreeView->GetSelectedItems();
		TArray<FString> SelectedPaths;
		Algo::Transform(SelectedItems, SelectedPaths, [](const TSharedPtr<FModularRigTreeElement>& Element)
		{
			return Element->Key;
		});
		HandleDeleteModules(SelectedPaths);
	}

	return;
}

void SModularRigHierarchy::HandleDeleteModules(const TArray<FString>& InPaths)
{
	ClearDetailPanel();
	
	if (ControlRigBlueprint.IsValid())
	{
		FScopedTransaction Transaction(LOCTEXT("ModularRigHierarchyDelete", "Delete Modules"));

		UModularRigController* Controller = ControlRigBlueprint->GetModularRigController();
		check(Controller);

		// Make sure we delete the modules from children to root
		TArray<FString> SortedPaths = Controller->Model->SortPaths(InPaths);
		Algo::Reverse(SortedPaths);
		for (const FString& Path : SortedPaths)
		{
			Controller->DeleteModule(Path);
		}
	}
}

void SModularRigHierarchy::HandleReparentModules(const TArray<FString>& InPaths, const FString& InParentPath)
{
	ClearDetailPanel();
	
	if (ControlRigBlueprint.IsValid())
	{
		FScopedTransaction Transaction(LOCTEXT("ModularRigHierarchyReparent", "Reparent Modules"));

		UModularRigController* Controller = ControlRigBlueprint->GetModularRigController();
		check(Controller);

		for (const FString& Path : InPaths)
		{
			Controller->ReparentModule(Path, InParentPath);
		}
	}
}

class SModularRigHierarchyPasteTransformsErrorPipe : public FOutputDevice
{
public:

	int32 NumErrors;

	SModularRigHierarchyPasteTransformsErrorPipe()
		: FOutputDevice()
		, NumErrors(0)
	{
	}

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
		UE_LOG(LogControlRig, Error, TEXT("Error importing transforms to Hierarchy: %s"), V);
		NumErrors++;
	}
};

UModularRig* SModularRigHierarchy::GetHierarchy() const
{
	if (ControlRigBlueprint.IsValid())
	{
		if (UControlRig* DebuggedRig = ControlRigBeingDebuggedPtr.Get())
		{
			return Cast<UModularRig>(DebuggedRig);
		}
	}
	if (ControlRigEditor.IsValid())
	{
		if (UControlRig* CurrentRig = ControlRigEditor.Pin()->GetControlRig())
		{
			return Cast<UModularRig>(CurrentRig);
		}
	}
	return nullptr;
}

UModularRig* SModularRigHierarchy::GetDefaultHierarchy() const
{
	if (ControlRigBlueprint.IsValid())
	{
		if (UControlRig* DebuggedRig = ControlRigBeingDebuggedPtr.Get())
		{
			return Cast<UModularRig>(DebuggedRig);
		}
	}
	return nullptr;
}


FName SModularRigHierarchy::CreateUniqueName(const FName& InBasePath) const
{
	return ControlRigBlueprint->GetModularRigController()->GetSafeNewName(InBasePath.ToString());
}

void SModularRigHierarchy::OnRequestDetailsInspection(const FString& InKey)
{
	if(!ControlRigEditor.IsValid())
	{
		return;
	}
	ControlRigEditor.Pin()->SetDetailViewForRigModules({InKey});
}

void SModularRigHierarchy::ClearDetailPanel() const
{
	if(ControlRigEditor.IsValid())
	{
		ControlRigEditor.Pin()->ClearDetailObject();
	}
}

void SModularRigHierarchy::PostRedo(bool bSuccess) 
{
	if (bSuccess)
	{
		RefreshTreeView();
	}
}

void SModularRigHierarchy::PostUndo(bool bSuccess) 
{
	if (bSuccess)
	{
		RefreshTreeView();
	}
}

FReply SModularRigHierarchy::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TArray<FString> DraggedElements = GetSelectedKeys();
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && DraggedElements.Num() > 0)
	{
		if (ControlRigEditor.IsValid())
		{
			TSharedRef<FModuleRigHierarchyDragDropOp> DragDropOp = FModuleRigHierarchyDragDropOp::New(MoveTemp(DraggedElements));
			return FReply::Handled().BeginDragDrop(DragDropOp);
		}
	}

	return FReply::Unhandled();
}

TOptional<EItemDropZone> SModularRigHierarchy::OnCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FModularRigTreeElement> TargetItem)
{
	const TOptional<EItemDropZone> InvalidDropZone;
	TOptional<EItemDropZone> ReturnDropZone = DropZone;

	TSharedPtr<FAssetDragDropOp> AssetDragDropOperation = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
	TSharedPtr<FModuleRigHierarchyDragDropOp> ModuleDragDropOperation = DragDropEvent.GetOperationAs<FModuleRigHierarchyDragDropOp>();
	if (AssetDragDropOperation)
	{
		for (const FAssetData& AssetData : AssetDragDropOperation->GetAssets())
		{
			static const UEnum* ControlTypeEnum = StaticEnum<EControlRigType>();
			const FString ControlRigTypeStr = AssetData.GetTagValueRef<FString>(TEXT("ControlRigType"));
			if (ControlRigTypeStr.IsEmpty())
			{
				ReturnDropZone.Reset();
				break;
			}

			const EControlRigType ControlRigType = (EControlRigType)(ControlTypeEnum->GetValueByName(*ControlRigTypeStr));
			if (ControlRigType != EControlRigType::RigModule)
			{
				ReturnDropZone.Reset();
				break;
			}
		}
	}
	else if(ModuleDragDropOperation)
	{
		// Accept this drop
	}
	else
	{
		ReturnDropZone.Reset();
	}

	return ReturnDropZone;
}

FReply SModularRigHierarchy::OnAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FModularRigTreeElement> TargetItem)
{
	const TSharedPtr<FModularRigTreeElement>* ItemAtMouse = TreeView->FindItemAtPosition(DragDropEvent.GetScreenSpacePosition());
	FString ParentPath;
	if (ItemAtMouse && ItemAtMouse->IsValid())
	{
		ParentPath = ItemAtMouse->Get()->Key;
	}

	TSharedPtr<FAssetDragDropOp> AssetDragDropOperation = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
	TSharedPtr<FModuleRigHierarchyDragDropOp> ModuleDragDropOperation = DragDropEvent.GetOperationAs<FModuleRigHierarchyDragDropOp>();
	if (AssetDragDropOperation)
	{
		for (const FAssetData& AssetData : AssetDragDropOperation->GetAssets())
		{
			static const UEnum* ControlTypeEnum = StaticEnum<EControlRigType>();
			const FString ControlRigTypeStr = AssetData.GetTagValueRef<FString>(TEXT("ControlRigType"));
			if (ControlRigTypeStr.IsEmpty())
			{
				continue;
			}

			const EControlRigType ControlRigType = (EControlRigType)(ControlTypeEnum->GetValueByName(*ControlRigTypeStr));
			if (ControlRigType != EControlRigType::RigModule)
			{
				continue;
			}

			UClass* AssetClass = AssetData.GetClass();
			if (!AssetClass->IsChildOf(UControlRigBlueprint::StaticClass()))
			{
				continue;
			}

			if(UControlRigBlueprint* AssetBlueprint = Cast<UControlRigBlueprint>(AssetData.GetAsset()))
			{
				HandleNewItem(AssetBlueprint->GetControlRigClass(), ParentPath);
			}
		}

		FReply::Handled();
	}
	else if(ModuleDragDropOperation)
	{
		const TArray<FString> Paths = ModuleDragDropOperation->GetElements();
		HandleReparentModules(Paths, ParentPath);
	}
	
	return FReply::Unhandled();
}

FReply SModularRigHierarchy::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	// only allow drops onto empty space of the widget (when there's no target item under the mouse)
	// when dropped onto an item SModularRigHierarchy::OnAcceptDrop will deal with the event
	const TSharedPtr<FModularRigTreeElement>* ItemAtMouse = TreeView->FindItemAtPosition(DragDropEvent.GetScreenSpacePosition());
	FString ParentPath;
	if (ItemAtMouse && ItemAtMouse->IsValid())
	{
		return SCompoundWidget::OnDrop(MyGeometry, DragDropEvent);
	}
	
	if (OnCanAcceptDrop(DragDropEvent, EItemDropZone::BelowItem, nullptr))
	{
		if (OnAcceptDrop(DragDropEvent, EItemDropZone::BelowItem, nullptr).IsEventHandled())
		{
			return FReply::Handled();
		}
	}
	return SCompoundWidget::OnDrop(MyGeometry, DragDropEvent);
}

#undef LOCTEXT_NAMESPACE

