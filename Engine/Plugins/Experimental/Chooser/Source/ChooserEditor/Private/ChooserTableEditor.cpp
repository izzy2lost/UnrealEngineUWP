// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChooserTableEditor.h"

#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SComboButton.h"
#include "SAssetDropTarget.h"
#include "SClassViewer.h"
#include "StructViewerModule.h"
#include "SourceCodeNavigation.h"
#include "Chooser.h"
#include "ChooserEditorStyle.h"
#include "ChooserFindProperties.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "ClassViewerFilter.h"
#include "IPropertyAccessEditor.h"
#include "LandscapeRender.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "ObjectChooser_Asset.h"
#include "ObjectChooser_Class.h"
#include "ObjectChooserClassFilter.h"
#include "ObjectChooserWidgetFactories.h"
#include "GraphEditorSettings.h"
#include "IDetailCustomization.h"
#include "PersonaModule.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "Widgets/Layout/SSeparator.h"
#include "ChooserTableEditorCommands.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "Widgets/Input/STextEntryPopup.h"
#include "Widgets/Layout/SScrollBox.h"

#define LOCTEXT_NAMESPACE "ChooserEditor"

namespace UE::ChooserEditor
{

const FName FChooserTableEditor::ToolkitFName( TEXT( "ChooserTableEditor" ) );
const FName FChooserTableEditor::PropertiesTabId( TEXT( "ChooserEditor_Properties" ) );
const FName FChooserTableEditor::FindReplaceTabId( TEXT( "ChooserEditor_FindReplace" ) );
const FName FChooserTableEditor::TableTabId( TEXT( "ChooserEditor_Table" ) );

void FChooserTableEditor::PushChooserTableToEdit(UChooserTable* Chooser)
{
	BreadcrumbTrail->PushCrumb(FText::FromString(Chooser->GetName()), Chooser);
	RefreshAll();
}
	
void FChooserTableEditor::PopChooserTableToEdit()
{
	if (BreadcrumbTrail->HasCrumbs())
	{
		BreadcrumbTrail->PopCrumb();
		RefreshAll();
	}
}
	
void FChooserTableEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_ChooserTableEditor", "Chooser Table Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner( PropertiesTabId, FOnSpawnTab::CreateSP(this, &FChooserTableEditor::SpawnPropertiesTab) )
		.SetDisplayName( LOCTEXT("PropertiesTab", "Details") )
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon("EditorStyle", "LevelEditor.Tabs.Details"));
		
	InTabManager->RegisterTabSpawner( TableTabId, FOnSpawnTab::CreateSP(this, &FChooserTableEditor::SpawnTableTab) )
		.SetDisplayName( LOCTEXT("TableTab", "Chooser Table") )
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon("ChooserEditorStyle", "ChooserEditor.ChooserTableIconSmall"));

	InTabManager->RegisterTabSpawner( FindReplaceTabId, FOnSpawnTab::CreateSP(this, &FChooserTableEditor::SpawnFindReplaceTab) )
		.SetDisplayName( LOCTEXT("FindReplaceTab", "Find/Replace") )
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Find"));
}
	
void FChooserTableEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner( TableTabId );
	InTabManager->UnregisterTabSpawner( PropertiesTabId );
	InTabManager->UnregisterTabSpawner( FindReplaceTabId );
}

const FName FChooserTableEditor::ChooserEditorAppIdentifier( TEXT( "ChooserEditorApp" ) );

FChooserTableEditor::~FChooserTableEditor()
{
	if (SelectedColumn)
	{
		SelectedColumn->ClearFlags(RF_Standalone);
	}
	for (UObject* SelectedRow : SelectedRows)
	{
		SelectedRow->ClearFlags(RF_Standalone);
	}
	
	FCoreUObjectDelegates::OnObjectsReplaced.RemoveAll(this);

	DetailsView.Reset();
}


FName FChooserTableEditor::EditorName = "ChooserTableEditor";
	
FName FChooserTableEditor::GetEditorName() const
{
	return EditorName;
}

void FChooserTableEditor::MakeDebugTargetMenu(UToolMenu* InToolMenu) 
{
	static FName SectionName = "Select Debug Target";
		
	InToolMenu->AddMenuEntry(
			SectionName,
			FToolMenuEntry::InitMenuEntry(
				"None",
				LOCTEXT("None", "None"),
				LOCTEXT("None Tooltip", "Clear selected debug target"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([this]()
					{
						UChooserTable* Chooser = GetRootChooser();
						Chooser->ResetDebugTarget();
						if (Chooser->bEnableDebugTesting)
						{
							Chooser->bEnableDebugTesting = false;
							Chooser->bDebugTestValuesValid = false;
							UpdateTableColumns();
						}
					}),
					FCanExecuteAction()
				)
			));
	
	InToolMenu->AddMenuEntry(
			SectionName,
			FToolMenuEntry::InitMenuEntry(
				"Manual",
				LOCTEXT("Manual Testing", "Manual Testing"),
				LOCTEXT("Manual Tooltip", "Test the chooser by manually entering values for each column"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([this]()
					{
						UChooserTable* Chooser = GetRootChooser();
						Chooser->ResetDebugTarget();
						if (!Chooser->bEnableDebugTesting)
						{
							Chooser->bEnableDebugTesting = true;
							Chooser->bDebugTestValuesValid = true;
							UpdateTableColumns();
						}
					}),
					FCanExecuteAction()
				)
			));

	const UChooserTable* Chooser = GetChooser();

	Chooser->IterateRecentContextObjects([this, InToolMenu](const UObject* Object)
		{
			InToolMenu->AddMenuEntry(
						SectionName,
						FToolMenuEntry::InitMenuEntry(
							Object->GetFName(),
							FText::FromString(Object->GetName()),
							LOCTEXT("Select Object ToolTip", "Select this object as the debug target"),
							FSlateIcon(),
							FUIAction(
								FExecuteAction::CreateLambda([this, ObjectPtr = MakeWeakObjectPtr(Object)]()
								{
									if(ObjectPtr.IsValid())
									{
										UChooserTable* Chooser = GetRootChooser();
										Chooser->SetDebugTarget(ObjectPtr);
										Chooser->bDebugTestValuesValid = false;
										if (!Chooser->bEnableDebugTesting)
										{
											Chooser->bEnableDebugTesting = true;
											UpdateTableColumns();
										}
									}
								}),
								FCanExecuteAction()
							)
						));
		}
	);
	

}

void FChooserTableEditor::RegisterToolbar()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	UToolMenu* ToolBar;
	FName ParentName;
	const FName MenuName = GetToolMenuToolbarName(ParentName);
	if (ToolMenus->IsMenuRegistered(MenuName))
	{
		ToolBar = ToolMenus->ExtendMenu(MenuName);
	}
	else
	{
		ToolBar = UToolMenus::Get()->RegisterMenu(MenuName, ParentName, EMultiBoxType::ToolBar);
	}

	const FChooserTableEditorCommands& Commands = FChooserTableEditorCommands::Get();
	FToolMenuInsert InsertAfterAssetSection("Asset", EToolMenuInsertType::After);
	{
		FToolMenuSection& Section = ToolBar->AddSection("Chooser", TAttribute<FText>(), InsertAfterAssetSection);
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			Commands.EditChooserSettings,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon("EditorStyle", "FullBlueprintEditor.EditGlobalOptions")));


		Section.AddDynamicEntry("DebuggingCommands", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			UChooserEditorToolMenuContext* Context = InSection.FindContext<UChooserEditorToolMenuContext>();

			if (Context)
			{
				if (TSharedPtr<FChooserTableEditor> ChooserEditor = Context->ChooserEditor.Pin())
				{
					InSection.AddEntry(FToolMenuEntry::InitComboButton( "SelectDebugTarget",
						FToolUIActionChoice(),
					  FNewToolMenuDelegate::CreateSP(ChooserEditor.Get(), &FChooserTableEditor::MakeDebugTargetMenu),
						TAttribute<FText>::CreateLambda([Chooser = ChooserEditor->GetRootChooser() ]
						{
							if (Chooser->HasDebugTarget())
							{
								return  FText::FromString(Chooser->GetDebugTargetName());
							}
							else
							{
								return Chooser->bEnableDebugTesting ? LOCTEXT("Manual Testing", "Manual Testing") : LOCTEXT("Debug Target", "Debug Target");
							}
						}),
						LOCTEXT("Debug Target Tooltip", "Select an object that has recently been the context object for this chooser to visualize the selection results")));
				}
			}
		}));
	}

}

void FChooserTableEditor::InitToolMenuContext(FToolMenuContext& MenuContext)
{
	FAssetEditorToolkit::InitToolMenuContext(MenuContext);

	UChooserEditorToolMenuContext* Context = NewObject<UChooserEditorToolMenuContext>();
	Context->ChooserEditor = SharedThis(this);
	MenuContext.AddObject(Context);
}

void FChooserTableEditor::BindCommands()
{
	const FChooserTableEditorCommands& Commands = FChooserTableEditorCommands::Get();

	ToolkitCommands->MapAction(
		Commands.EditChooserSettings,
		FExecuteAction::CreateSP(this, &FChooserTableEditor::SelectRootProperties));
}

void FChooserTableEditor::InitEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, const TArray<UObject*>& ObjectsToEdit, FGetDetailsViewObjects GetDetailsViewObjects )
{
	EditingObjects = ObjectsToEdit;

	BreadcrumbTrail = SNew(SBreadcrumbTrail<UChooserTable*>)
		.ButtonStyle(FAppStyle::Get(), "GraphBreadcrumbButton")
		.TextStyle(FAppStyle::Get(), "GraphBreadcrumbButtonText")
		.ButtonContentPadding( FMargin(4.f, 2.f) )
		.DelimiterImage( FAppStyle::GetBrush("BreadcrumbTrail.Delimiter") )
		.OnCrumbPushed_Lambda([this](UChooserTable* Table)
		{
			RefreshAll();
		})
		.OnCrumbPopped_Lambda([this](UChooserTable* Table)
		{
			RefreshAll();
		});
		
	UChooserTable* RootTable = GetRootChooser();
	BreadcrumbTrail->PushCrumb(FText::FromString(RootTable->GetName()), RootTable);
	
	
	FCoreUObjectDelegates::OnObjectsReplaced.AddSP(this, &FChooserTableEditor::OnObjectsReplaced);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NotifyHook = this;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = PropertyEditorModule.CreateDetailView( DetailsViewArgs );
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_ChooserTableEditor_Layout_v1" )
	->AddArea
	(
		FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewSplitter()
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.7)
				->AddTab( TableTabId, ETabState::OpenedTab )
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.3)
				->AddTab( PropertiesTabId, ETabState::OpenedTab )
			)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, FChooserTableEditor::ChooserEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectsToEdit );

	BindCommands();
	RegenerateMenusAndToolbars();
	RegisterToolbar();

	SelectRootProperties();

		
	FAnimAssetFindReplaceConfig FindReplaceConfig;
	FindReplaceConfig.InitialProcessorClass = UChooserFindProperties::StaticClass();

	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
}

FName FChooserTableEditor::GetToolkitFName() const
{
	return ToolkitFName;
}

FText FChooserTableEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Chooser Table Editor");
}

void FChooserTableEditor::RefreshAll()
{
	if (HeaderRow)
	{
		UpdateTableColumns();
		UpdateTableRows();
		ClearSelectedColumn();
		SelectRootProperties();
	}
}

void FChooserTableEditor::PostUndo(bool bSuccess)
{
	RefreshAll();
}

void FChooserTableEditor::PostRedo(bool bSuccess)
{
	RefreshAll();
}


void FChooserTableEditor::NotifyPreChange(FProperty* PropertyAboutToChange)
{
}

void FChooserTableEditor::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	// was previously only refreshing when known properties changed, however, changing the contents of an FInstancedStruct in the Details Panel (for example DefaultRowValue on a Column)
	// will invalidate previously cached pointers to that FInstancedStruct, and so we need to refresh the columns every time anything is edited in the Details Panel
	UpdateTableColumns();
	UpdateTableRows();
}


FText FChooserTableEditor::GetToolkitName() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();

	check( EditingObjs.Num() > 0 );

	FFormatNamedArguments Args;
	Args.Add( TEXT("ToolkitName"), GetBaseToolkitName() );

	if( EditingObjs.Num() == 1 )
	{
		const UObject* EditingObject = EditingObjs[ 0 ];
		return FText::FromString(EditingObject->GetName());
	}
	else
	{
		UClass* SharedBaseClass = nullptr;
		for( int32 x = 0; x < EditingObjs.Num(); ++x )
		{
			UObject* Obj = EditingObjs[ x ];
			check( Obj );

			UClass* ObjClass = Cast<UClass>(Obj);
			if (ObjClass == nullptr)
			{
				ObjClass = Obj->GetClass();
			}
			check( ObjClass );

			// Initialize with the class of the first object we encounter.
			if( SharedBaseClass == nullptr )
			{
				SharedBaseClass = ObjClass;
			}

			// If we've encountered an object that's not a subclass of the current best baseclass,
			// climb up a step in the class hierarchy.
			while( !ObjClass->IsChildOf( SharedBaseClass ) )
			{
				SharedBaseClass = SharedBaseClass->GetSuperClass();
			}
		}

		check(SharedBaseClass);

		Args.Add( TEXT("NumberOfObjects"), EditingObjs.Num() );
		Args.Add( TEXT("ClassName"), FText::FromString( SharedBaseClass->GetName() ) );
		return FText::Format( LOCTEXT("ToolkitTitle_EditingMultiple", "{NumberOfObjects} {ClassName} - {ToolkitName}"), Args );
	}
}

FText FChooserTableEditor::GetToolkitToolTipText() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();

	check( EditingObjs.Num() > 0 );

	FFormatNamedArguments Args;
	Args.Add( TEXT("ToolkitName"), GetBaseToolkitName() );

	if( EditingObjs.Num() == 1 )
	{
		const UObject* EditingObject = EditingObjs[ 0 ];
		return FAssetEditorToolkit::GetToolTipTextForObject(EditingObject);
	}
	else
	{
		UClass* SharedBaseClass = NULL;
		for( int32 x = 0; x < EditingObjs.Num(); ++x )
		{
			UObject* Obj = EditingObjs[ x ];
			check( Obj );

			UClass* ObjClass = Cast<UClass>(Obj);
			if (ObjClass == nullptr)
			{
				ObjClass = Obj->GetClass();
			}
			check( ObjClass );

			// Initialize with the class of the first object we encounter.
			if( SharedBaseClass == nullptr )
			{
				SharedBaseClass = ObjClass;
			}

			// If we've encountered an object that's not a subclass of the current best baseclass,
			// climb up a step in the class hierarchy.
			while( !ObjClass->IsChildOf( SharedBaseClass ) )
			{
				SharedBaseClass = SharedBaseClass->GetSuperClass();
			}
		}

		check(SharedBaseClass);

		Args.Add( TEXT("NumberOfObjects"), EditingObjs.Num() );
		Args.Add( TEXT("ClassName"), FText::FromString( SharedBaseClass->GetName() ) );
		return FText::Format( LOCTEXT("ToolkitTitle_EditingMultipleToolTip", "{NumberOfObjects} {ClassName} - {ToolkitName}"), Args );
	}
}

FLinearColor FChooserTableEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.5f, 0.0f, 0.0f, 0.5f );
}

void FChooserTableEditor::SetPropertyVisibilityDelegate(FIsPropertyVisible InVisibilityDelegate)
{
	DetailsView->SetIsPropertyVisibleDelegate(InVisibilityDelegate);
	DetailsView->ForceRefresh();
}

void FChooserTableEditor::SetPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled InPropertyEditingDelegate)
{
	DetailsView->SetIsPropertyEditingEnabledDelegate(InPropertyEditingDelegate);
	DetailsView->ForceRefresh();
}

TSharedRef<SDockTab> FChooserTableEditor::SpawnPropertiesTab( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == PropertiesTabId );

	return SNew(SDockTab)
		.Label( LOCTEXT("GenericDetailsTitle", "Details") )
		.TabColorScale( GetTabColorScale() )
		.OnCanCloseTab_Lambda([]() { return false; })
		[
			DetailsView.ToSharedRef()
		];
}
	
TSharedRef<SDockTab> FChooserTableEditor::SpawnFindReplaceTab( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == FindReplaceTabId );

	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
	FAnimAssetFindReplaceConfig Config;
	Config.InitialProcessorClass = UChooserFindProperties::StaticClass();
	return SNew(SDockTab)
		.Label( LOCTEXT("FindReplaceTitle", "Find/Replace") )
		.TabColorScale( GetTabColorScale() )
	[
		PersonaModule.CreateFindReplaceWidget(Config)
	];
}

class FChooserRowDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FWidgetTemplateDragDropOp, FDecoratedDragDropOp)

	FChooserTableEditor* ChooserEditor;
	uint32 RowIndex;

	/** Constructs the drag drop operation */
	static TSharedRef<FChooserRowDragDropOp> New(FChooserTableEditor* InEditor, uint32 InRowIndex)
	{
		TSharedRef<FChooserRowDragDropOp> Operation = MakeShareable(new FChooserRowDragDropOp());
		Operation->ChooserEditor = InEditor;
		Operation->RowIndex = InRowIndex;
		Operation->DefaultHoverText = LOCTEXT("Chooser Row", "Chooser Row");
		Operation->CurrentHoverText = Operation->DefaultHoverText;
		
		Operation->Construct();

		return Operation;
	};
};

class SChooserRowHandle : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SChooserRowHandle)
	{}
	SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_ARGUMENT(FChooserTableEditor*, ChooserEditor)
	SLATE_ARGUMENT(uint32, RowIndex)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		ChooserEditor = InArgs._ChooserEditor;
		RowIndex = InArgs._RowIndex;

		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBox) .Padding(0.0f) .HAlign(HAlign_Center) .VAlign(VAlign_Center) .WidthOverride(16.0f)
				[
					SNew(SImage)
					.Image(FCoreStyle::Get().GetBrush("VerticalBoxDragIndicatorShort"))
				]
			]
			+ SOverlay::Slot()
			[
				SNew(SBox).Padding(0.0f) .HAlign(HAlign_Center) .VAlign(VAlign_Center) .WidthOverride(16.0f)
				[
					SNew(SImage)
					.Visibility_Lambda([this]()
					{
						return ChooserEditor->GetChooser()->bDebugTestValuesValid && RowIndex == ChooserEditor->GetChooser()->GetDebugSelectedRow() ? EVisibility::HitTestInvisible : EVisibility::Hidden;
					})
					.Image(FAppStyle::Get().GetBrush("Icons.ArrowRight"))
				]
			]
		];
	}
	
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override
	{
		UChooserTable* Chooser = ChooserEditor->GetChooser();

		if (const FNestedChooser* NestedChooserResult = Chooser->ResultsStructs[RowIndex].GetPtr<FNestedChooser>())
		{
			if (NestedChooserResult->Chooser)
			{
				ChooserEditor->PushChooserTableToEdit(NestedChooserResult->Chooser);
			}
		}
		return FReply::Handled();
	}

	FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		ChooserEditor->SelectRow(RowIndex);
		return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
	};

	FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		// clear row selection so that delete key can't cause the selected row to be deleted
		ChooserEditor->ClearSelectedRows();
		
		TSharedRef<FChooserRowDragDropOp> DragDropOp = FChooserRowDragDropOp::New(ChooserEditor, RowIndex);
		return FReply::Handled().BeginDragDrop(DragDropOp);
	}

private:
	FChooserTableEditor* ChooserEditor = nullptr;
	uint32 RowIndex;
};


class SChooserTableRow : public SMultiColumnTableRow<TSharedPtr<FChooserTableEditor::FChooserTableRow>>
{
public:
	SLATE_BEGIN_ARGS(SChooserTableRow) {}
	/** The list item for this row */
	SLATE_ARGUMENT(TSharedPtr<FChooserTableEditor::FChooserTableRow>, Entry)
	SLATE_ARGUMENT(UChooserTable*, Chooser)
	SLATE_ARGUMENT(FChooserTableEditor*, Editor)
	SLATE_END_ARGS()

	enum { SpecialIndex_AddRow = -1, SpecialIndex_Fallback = -2 };

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView)
	{
		RowIndex = Args._Entry;
		Chooser = Args._Chooser;
		Editor = Args._Editor;

		SMultiColumnTableRow<TSharedPtr<FChooserTableEditor::FChooserTableRow>>::Construct(
			FSuperRowType::FArguments(),
			OwnerTableView
		);
	}

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the list view. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		static FName Result = "Result";
		static FName Handles = "Handles";
	
		if (Chooser->ResultsStructs.IsValidIndex(RowIndex->RowIndex))
		{
			if (ColumnName == Handles)
			{
				// row drag handle
				return SNew(SChooserRowHandle).ChooserEditor(Editor).RowIndex(RowIndex->RowIndex);
			}
			else if (ColumnName == Result) 
			{
				UChooserTable* ContextOwner = Chooser->GetContextOwner();
				TSharedPtr<SWidget> ResultWidget = FObjectChooserWidgetFactories::CreateWidget(false, ContextOwner, FObjectChooserBase::StaticStruct(), Chooser->ResultsStructs[RowIndex->RowIndex].GetMutableMemory(), Chooser->ResultsStructs[RowIndex->RowIndex].GetScriptStruct(), ContextOwner->OutputObjectType,
				FOnStructPicked::CreateLambda([this, RowIndex=RowIndex->RowIndex](const UScriptStruct* ChosenStruct)
				{
					UChooserTable* ContextOwner = Chooser->GetContextOwner();
					const FScopedTransaction Transaction(LOCTEXT("Change Row Result Type", "Change Row Result Type"));
					Chooser->Modify(true);
					Chooser->ResultsStructs[RowIndex].InitializeAs(ChosenStruct);
					FObjectChooserWidgetFactories::CreateWidget(false, ContextOwner, FObjectChooserBase::StaticStruct(), Chooser->ResultsStructs[RowIndex].GetMutableMemory(), ChosenStruct, ContextOwner->OutputObjectType, FOnStructPicked(), &CacheBorder);
				}),
				&CacheBorder
				);
			
				return SNew(SOverlay)
						+ SOverlay::Slot()
						[
							ResultWidget.ToSharedRef()
						]
						+ SOverlay::Slot().VAlign(VAlign_Bottom)
						[
							SNew(SSeparator).SeparatorImage(FCoreStyle::Get().GetBrush("FocusRectangle"))
							.Visibility_Lambda([this]() { return bDragActive && !bDropAbove ? EVisibility::Visible : EVisibility::Hidden; })
						]
						+ SOverlay::Slot().VAlign(VAlign_Top)
						[
							SNew(SSeparator).SeparatorImage(FCoreStyle::Get().GetBrush("FocusRectangle"))
							.Visibility_Lambda([this]() { return bDragActive && bDropAbove ? EVisibility::Visible : EVisibility::Hidden; })
						];
			}
			else
			{
				const int ColumnIndex = ColumnName.GetNumber() - 1;
				if (ColumnIndex < Chooser->ColumnsStructs.Num() && ColumnIndex >=0)
				{
					FChooserColumnBase* Column = &Chooser->ColumnsStructs[ColumnIndex].GetMutable<FChooserColumnBase>();
					const UStruct * ColumnStruct = Chooser->ColumnsStructs[ColumnIndex].GetScriptStruct();

					TSharedPtr<SWidget> ColumnWidget = FObjectChooserWidgetFactories::CreateColumnWidget(Column, ColumnStruct, Chooser->GetContextOwner(), RowIndex->RowIndex);
				
					if (ColumnWidget.IsValid())
					{
						return SNew(SOverlay)
						+ SOverlay::Slot()
						[
							ColumnWidget.ToSharedRef()
						]
						+ SOverlay::Slot()
						[
							SNew(SColorBlock).Visibility(EVisibility::HitTestInvisible).Color_Lambda(
									[this,Column]()
									{
										if (Chooser->bDebugTestValuesValid && Column->HasFilters())
										{
											if (Column->EditorTestFilter(RowIndex->RowIndex))
											{
												return FLinearColor(0.0,1.0,0.0,0.30);
											}
											else
											{
												return FLinearColor(1.0,0.0,0.0,0.20);
											}
										}
										return FLinearColor::Transparent;
									})
						];
					}
				}
			}
		}
		else if (RowIndex->RowIndex == SpecialIndex_Fallback)
		{
			if (ColumnName == Handles)
			{
				return SNew(SBox).Padding(0.0f) .HAlign(HAlign_Center) .VAlign(VAlign_Center) .WidthOverride(16.0f)
					[
						SNew(SImage)
						.Image(FChooserEditorStyle::Get().GetBrush("ChooserEditor.FallbackIcon"))
						.ToolTipText(LOCTEXT("FallbackTooltip","Fallback result:  Returned if all rows failed."))
					];
			}
			else if (ColumnName == Result) 
			{
				UChooserTable* ContextOwner = Chooser->GetContextOwner();
				TSharedPtr<SWidget> ResultWidget = FObjectChooserWidgetFactories::CreateWidget(false,ContextOwner, FObjectChooserBase::StaticStruct(), Chooser->FallbackResult.GetMutableMemory(), Chooser->FallbackResult.GetScriptStruct(), ContextOwner->OutputObjectType,
				FOnStructPicked::CreateLambda([this](const UScriptStruct* ChosenStruct)
				{
				UChooserTable* ContextOwner = Chooser->GetContextOwner();
					const FScopedTransaction Transaction(LOCTEXT("Change Row Result Type", "Change Row Result Type"));
					Chooser->Modify(true);
					Chooser->FallbackResult.InitializeAs(ChosenStruct);
					FObjectChooserWidgetFactories::CreateWidget(false, ContextOwner, FObjectChooserBase::StaticStruct(), Chooser->FallbackResult.GetMutableMemory(), ChosenStruct, ContextOwner->OutputObjectType, FOnStructPicked(), &CacheBorder
						,FChooserWidgetValueChanged(), LOCTEXT("Fallback Result", "Fallback Result: (None)"));
				}),
				&CacheBorder
				,FChooserWidgetValueChanged(), LOCTEXT("Fallback Result", "Fallback Result: (None)")
				);
				
				return SNew(SOverlay)
						+ SOverlay::Slot()
						[
							ResultWidget.ToSharedRef()
						]
						+ SOverlay::Slot().VAlign(VAlign_Top)
						[
							SNew(SSeparator).SeparatorImage(FCoreStyle::Get().GetBrush("FocusRectangle"))
							.Visibility_Lambda([this]() { return bDragActive  ? EVisibility::Visible : EVisibility::Hidden; })
						];
			}
			else
			{
				const int ColumnIndex = ColumnName.GetNumber() - 1;
				if (ColumnIndex < Chooser->ColumnsStructs.Num() && ColumnIndex >=0)
				{
					FChooserColumnBase* Column = &Chooser->ColumnsStructs[ColumnIndex].GetMutable<FChooserColumnBase>();
					const UStruct * ColumnStruct = Chooser->ColumnsStructs[ColumnIndex].GetScriptStruct();

					TSharedPtr<SWidget> ColumnWidget = FObjectChooserWidgetFactories::CreateColumnWidget(Column, ColumnStruct, Chooser->GetContextOwner(), -2);
				
					if (ColumnWidget.IsValid())
					{
						return ColumnWidget.ToSharedRef();
					}
				}
			}
		}
		else if (RowIndex->RowIndex == SpecialIndex_AddRow)
		{
			// on the row past the end, show an Add button in the result column
			if (ColumnName == Result)
			{
				return Editor->GetCreateRowComboButton().ToSharedRef();
			}
		}

		return SNullWidget::NullWidget;
	}

	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
	{
		if (TSharedPtr<FChooserRowDragDropOp> Operation = DragDropEvent.GetOperationAs<FChooserRowDragDropOp>())
		{
			bDragActive = true;
			float Center = MyGeometry.Position.Y + MyGeometry.Size.Y;
			bDropAbove = DragDropEvent.GetScreenSpacePosition().Y < Center;
		}
	}
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override
	{
		bDragActive = false;
	}

	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
	{
		if (TSharedPtr<FChooserRowDragDropOp> Operation = DragDropEvent.GetOperationAs<FChooserRowDragDropOp>())
		{
			float Center = MyGeometry.AbsolutePosition.Y + MyGeometry.Size.Y/2;
			bDropAbove = DragDropEvent.GetScreenSpacePosition().Y < Center;
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
	{
		if (TSharedPtr<FChooserRowDragDropOp> Operation = DragDropEvent.GetOperationAs<FChooserRowDragDropOp>())
		{
			if (Chooser == Operation->ChooserEditor->GetChooser())
			{
				int NewRowIndex;
				if (!Chooser->ResultsStructs.IsValidIndex(RowIndex->RowIndex))
				{
					// for special (negative) indices, move to the end
					NewRowIndex = Editor->MoveRow(Operation->RowIndex, Chooser->ResultsStructs.Num());
				}
				else if (bDropAbove)
				{
					NewRowIndex = Editor->MoveRow(Operation->RowIndex, RowIndex->RowIndex);
				}
				else
				{
					NewRowIndex = Editor->MoveRow(Operation->RowIndex, RowIndex->RowIndex+1);
				}
				Editor->SelectRow(NewRowIndex);
				
				return FReply::Handled();		
			}
		}
		return FReply::Unhandled();
	}

private:
	TSharedPtr<FChooserTableEditor::FChooserTableRow> RowIndex;
	UChooserTable* Chooser;
	FChooserTableEditor* Editor;
	TSharedPtr<SBorder> CacheBorder;
	bool bDragActive = false;
	bool bDropAbove = false;
};


TSharedRef<ITableRow> FChooserTableEditor::GenerateTableRow(TSharedPtr<FChooserTableRow> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	UChooserTable* Chooser = GetChooser();

	return SNew(SChooserTableRow, OwnerTable)
		.Entry(InItem).Chooser(Chooser).Editor(this);
}

void FChooserTableEditor::SelectRootProperties()
{
	if( DetailsView.IsValid() )
	{
		// point the details view to the main table
		DetailsView->SetObject( GetRootChooser() );
	}
}

int FChooserTableEditor::MoveRow(int SourceRowIndex, int TargetRowIndex)
{
	UChooserTable* Chooser = GetChooser();
	TargetRowIndex = FMath::Min(TargetRowIndex,Chooser->ResultsStructs.Num());

	const FScopedTransaction Transaction(LOCTEXT("Move Row", "Move Row"));

	Chooser->Modify(true);

	for (FInstancedStruct& ColStruct : Chooser->ColumnsStructs)
	{
		FChooserColumnBase& Column = ColStruct.GetMutable<FChooserColumnBase>();
		Column.MoveRow(SourceRowIndex, TargetRowIndex);
	}

	FInstancedStruct Result = Chooser->ResultsStructs[SourceRowIndex];
	Chooser->ResultsStructs.RemoveAt(SourceRowIndex);
	if (SourceRowIndex < TargetRowIndex)
	{
		TargetRowIndex--;
	}
	Chooser->ResultsStructs.Insert(Result, TargetRowIndex);

	UpdateTableRows();

	return TargetRowIndex;
}
	
void FChooserTableEditor::SelectRow(int32 RowIndex)
{
	if (TableRows.IsValidIndex(RowIndex))
	{
		if (!TableView->IsItemSelected(TableRows[RowIndex]))
		{
			TableView->ClearSelection();
			TableView->SetItemSelection(TableRows[RowIndex], true, ESelectInfo::OnMouseClick);
		}
	}
}
	
void FChooserTableEditor::ClearSelectedRows() 
{
	SelectedRows.SetNum(0);
	TableView->ClearSelection();
	SelectRootProperties();
}

void FChooserTableEditor::UpdateTableColumns()
{
	UChooserTable* Chooser = GetChooser();

	HeaderRow->ClearColumns();

	HeaderRow->AddColumn(SHeaderRow::Column("Handles")
					.DefaultLabel(FText())
					.ManualWidth(30));

	HeaderRow->AddColumn(SHeaderRow::Column("Result")
					.DefaultLabel(LOCTEXT("ResultColumnName", "Result"))
					.ManualWidth(300));

	FName ColumnId("ChooserColumn", 1);
	int NumColumns = Chooser->ColumnsStructs.Num();	
	for(int ColumnIndex = 0; ColumnIndex < NumColumns; ColumnIndex++)
	{
		FChooserColumnBase& Column = Chooser->ColumnsStructs[ColumnIndex].GetMutable<FChooserColumnBase>();

		TSharedPtr<SWidget> HeaderWidget = FObjectChooserWidgetFactories::CreateColumnWidget(&Column, Chooser->ColumnsStructs[ColumnIndex].GetScriptStruct(), Chooser->GetContextOwner(), -1);
		if (!HeaderWidget.IsValid())
		{
			HeaderWidget = SNullWidget::NullWidget;
		}
		
		HeaderRow->AddColumn(SHeaderRow::FColumn::FArguments()
			.ColumnId(ColumnId)
			.ManualWidth(200)
			.OnGetMenuContent_Lambda([this, &Column, Chooser, ColumnIndex, ColumnId]()
			{
				UChooserColumnMenuContext* MenuContext = NewObject<UChooserColumnMenuContext>();
				MenuContext->Editor = this;
				MenuContext->Chooser = Chooser;
				MenuContext->ColumnIndex = ColumnIndex;

				FMenuBuilder MenuBuilder(true,nullptr);

				MenuBuilder.AddMenuEntry(LOCTEXT("Column Properties","Properties"),LOCTEXT("Select Column ToolTip", "Select this Column, and show its properties in the Details panel"),FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([this,Chooser,ColumnIndex, ColumnId, &Column]()
						{
							SelectColumn(Chooser, ColumnId.GetNumber() - 1);
						})
						)
					);

				if (ColumnIndex > 0 && !Chooser->ColumnsStructs[ColumnIndex].Get<FChooserColumnBase>().IsRandomizeColumn())
				{
					MenuBuilder.AddMenuEntry(LOCTEXT("Move Left","Move Left"),LOCTEXT("Move Left ToolTip", "Move this column to the left."),FSlateIcon(),
						FUIAction(
							FExecuteAction::CreateLambda([this,Chooser,ColumnIndex, &Column]()
							{
								const FScopedTransaction Transaction(LOCTEXT("Move Column Left Transaction", "Move Column Left"));
								Chooser->Modify(true);
								Chooser->ColumnsStructs.Swap(ColumnIndex, ColumnIndex - 1);
								UpdateTableColumns();
							})
							));
				}
				if (ColumnIndex < Chooser->ColumnsStructs.Num() - 1 && !Chooser->ColumnsStructs[ColumnIndex+1].Get<FChooserColumnBase>().IsRandomizeColumn())
				{
					MenuBuilder.AddMenuEntry(LOCTEXT("Move Right","Move Right"),LOCTEXT("Move Right ToolTip", "Move this column to the right."),FSlateIcon(),
						FUIAction(
							FExecuteAction::CreateLambda([this,Chooser,ColumnIndex, &Column]()
							{
								const FScopedTransaction Transaction(LOCTEXT("Move Column Right Transaction", "Move Column Right"));
								Chooser->Modify(true);
								Chooser->ColumnsStructs.Swap(ColumnIndex, ColumnIndex + 1);
								UpdateTableColumns();
							})
							));
				}


				MenuBuilder.AddMenuEntry(LOCTEXT("Delete Column", "Delete"), LOCTEXT("Delete Column ToolTip", "Remove this column and all its data from the table"), FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([this, Chooser, ColumnIndex, &Column]()
						{
							DeleteColumn(ColumnIndex);
						})
						));
			
				MenuBuilder.AddSubMenu(LOCTEXT("Input Type", "Input Type"),
					LOCTEXT("InputTypeToolTip", "Change input parameter type"),
					FNewMenuDelegate::CreateLambda([this, Chooser, &ColumnIndex](FMenuBuilder& Builder)
					{
						FStructViewerInitializationOptions Options;
						Options.StructFilter = MakeShared<FStructFilter>(Chooser->ColumnsStructs[ColumnIndex].Get<FChooserColumnBase>().GetInputBaseType());
						Options.bAllowViewOptions = false;
						Options.bShowNoneOption = false;
						Options.NameTypeToDisplay = EStructViewerNameTypeToDisplay::DisplayName;
					
						// Add class filter for columns here
						TSharedRef<SWidget> Widget = FModuleManager::LoadModuleChecked<FStructViewerModule>("StructViewer").CreateStructViewer(Options, FOnStructPicked::CreateLambda([this, ColumnIndex](const UScriptStruct* ChosenStruct)
						{
							const FScopedTransaction Transaction(LOCTEXT("SetColumnInputType", "Set Column Input Type"));
							UChooserTable* ChooserTable = GetChooser();
							ChooserTable->ColumnsStructs[ColumnIndex].GetMutable<FChooserColumnBase>().SetInputType(ChosenStruct);
							ChooserTable->Modify(true);
							UpdateTableColumns();
							UpdateTableRows();
						}));
				
						Builder.AddWidget(Widget, FText());
					}));
			
				return MenuBuilder.MakeWidget();
			})
			.HeaderComboVisibility(EHeaderComboVisibility::Ghosted)
			.HeaderContent()
			[
				SNew(SBorder)
				.VAlign(VAlign_Center)
				.Padding(3)
				.BorderBackgroundColor_Lambda([this, ColumnId] ()
				{
					// unclear why this color is coming out much darker
					return (SelectedColumn && SelectedColumn->Column == ColumnId.GetNumber() - 1) ? FSlateColor(FColor(0x00, 0x70, 0xe0, 0xFF)) : FSlateColor(FLinearColor(0.05f,0.05f,0.05f));
				})
				.OnMouseButtonDown_Lambda([this, Chooser, ColumnIndex, ColumnId](	const FGeometry&, const FPointerEvent& PointerEvent)
				{
					TableView->ClearSelection();
				
					SelectColumn(Chooser, ColumnId.GetNumber() - 1);
					return FReply::Handled();
				})
				[
					HeaderWidget.ToSharedRef()
				]
			
			]);
	
		ColumnId.SetNumber(ColumnId.GetNumber() + 1);
	}

	HeaderRow->AddColumn( SHeaderRow::FColumn::FArguments()
		.ColumnId("Add")
		.FillWidth(1.0)
		.HeaderContent( )
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().MaxWidth(120)
			[
				CreateColumnComboButton.ToSharedRef()
			]
		]
		);

}

void FChooserTableEditor::AddColumn(const UScriptStruct* ColumnType)
{
	CreateColumnComboButton->SetIsOpen(false);
	UChooserTable* Chooser = GetChooser();
	const FScopedTransaction Transaction(LOCTEXT("Add Column Transaction", "Add Column"));
	Chooser->Modify(true);

	FInstancedStruct NewColumn;
	NewColumn.InitializeAs(ColumnType);
	const FChooserColumnBase& NewColumnRef = NewColumn.Get<FChooserColumnBase>();
	int InsertIndex = 0;
	if (NewColumnRef.IsRandomizeColumn())
	{
		// add randomization column at the end (do nothing if there already is one)
		InsertIndex = Chooser->ColumnsStructs.Num();
		if (InsertIndex == 0 || !Chooser->ColumnsStructs[InsertIndex - 1].Get<FChooserColumnBase>().IsRandomizeColumn())
		{
			Chooser->ColumnsStructs.Add(NewColumn);
		}
	}
	else if (NewColumnRef.HasOutputs())
	{
		// add output columns at the end (but before any randomization column)
		InsertIndex = Chooser->ColumnsStructs.Num();
		if (InsertIndex > 0 && Chooser->ColumnsStructs[InsertIndex - 1].Get<FChooserColumnBase>().IsRandomizeColumn())
		{
			InsertIndex--;
		}
		Chooser->ColumnsStructs.Insert(NewColumn, InsertIndex);
	}
	else
	{
		// add other columns after the last non-output, non-randomization column
		while(InsertIndex < Chooser->ColumnsStructs.Num())
		{
			const FChooserColumnBase& Column = Chooser->ColumnsStructs[InsertIndex].Get<FChooserColumnBase>();
			if (Column.HasOutputs() || Column.IsRandomizeColumn())
			{
				break;
			}
			InsertIndex++;
		}
		Chooser->ColumnsStructs.Insert(NewColumn, InsertIndex);
	}

	UpdateTableColumns();
	UpdateTableRows();

	SelectColumn(Chooser, InsertIndex);
}

TSharedRef<SDockTab> FChooserTableEditor::SpawnTableTab( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == TableTabId );

	UChooserTable* Chooser = GetChooser();

	// + button to create new columns
	
	CreateColumnComboButton = SNew(SComboButton).OnGetMenuContent_Lambda([this]()
	{
		FStructViewerInitializationOptions Options;
		Options.StructFilter = MakeShared<FStructFilter>(FChooserColumnBase::StaticStruct());
		Options.bAllowViewOptions = false;
		Options.bShowNoneOption = false;
		Options.NameTypeToDisplay = EStructViewerNameTypeToDisplay::DisplayName;
		
		// Add class filter for columns here
		FStructViewerModule& StructViewerModule = FModuleManager::LoadModuleChecked<FStructViewerModule>("StructViewer");
		TSharedRef<SWidget> Widget = StructViewerModule.CreateStructViewer(Options, FOnStructPicked::CreateRaw(this, &FChooserTableEditor::AddColumn));
		return Widget;
	})
	.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
	.ButtonContent()
	[
		SNew(STextBlock).Text(LOCTEXT("AddColumn", "+ Add Column"))
	];


	CreateRowComboButton = SNew(SComboButton)
		.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
		.ButtonContent()
		[
			SNew(STextBlock).Text(LOCTEXT("AddRow", "+ Add Row"))
		]
		.OnGetMenuContent_Lambda([this]()
		{
			FStructViewerInitializationOptions Options;
			Options.StructFilter = MakeShared<FStructFilter>(FObjectChooserBase::StaticStruct());
			Options.bAllowViewOptions = false;
			Options.bShowNoneOption = false;
			Options.NameTypeToDisplay = EStructViewerNameTypeToDisplay::DisplayName;
			
			TSharedRef<SWidget> Widget = FModuleManager::LoadModuleChecked<FStructViewerModule>("StructViewer").CreateStructViewer(Options, FOnStructPicked::CreateLambda([this](const UScriptStruct* ChosenStruct)
			{
				CreateRowComboButton->SetIsOpen(false);
				UChooserTable* Chooser = GetChooser();
				const FScopedTransaction Transaction(LOCTEXT("Add Row Transaction", "Add Row"));
				Chooser->Modify(true);

				Chooser->ResultsStructs.SetNum(Chooser->ResultsStructs.Num()+1);
				Chooser->ResultsStructs.Last().InitializeAs(ChosenStruct);
				UpdateTableRows();
			}));
			
			return Widget;
		});

	HeaderRow = SNew(SHeaderRow);

	UpdateTableRows();
	UpdateTableColumns();

	TableView = SNew(SListView<TSharedPtr<FChooserTableRow>>)
    			.ListItemsSource(&TableRows)
				.OnKeyDownHandler_Lambda([this](const FGeometry&, const FKeyEvent& Event)
				{
					
					if (Event.GetKey() == EKeys::Delete)
					{
						const FScopedTransaction Transaction(LOCTEXT("Delete Row Transaction", "Delete Row"));
						UChooserTable* Chooser = GetChooser();
						Chooser->Modify(true);
						// delete selected rows.
						TArray<uint32> RowsToDelete;
						for(auto& SelectedRow:SelectedRows)
						{
							RowsToDelete.Add(SelectedRow->Row);
						}

						SelectedRows.SetNum(0);
						SelectRootProperties();

						// sort indices in reverse
						RowsToDelete.Sort([](int32 A, int32 B){ return A>B; });
						for(uint32 RowIndex : RowsToDelete)
						{
							Chooser->ResultsStructs.RemoveAt(RowIndex);
						}

						for(FInstancedStruct& ColumnData : Chooser->ColumnsStructs)
						{
							FChooserColumnBase& Column = ColumnData.GetMutable<FChooserColumnBase>();
							Column.DeleteRows(RowsToDelete);
						}

						UpdateTableRows();
						
						return FReply::Handled();
					}
					return FReply::Unhandled();
				}
				)
				.OnSelectionChanged_Lambda([this](TSharedPtr<FChooserTableRow> SelectedItem,  ESelectInfo::Type SelectInfo)
				{
					// deselect any selected column
					ClearSelectedColumn();
					
					if (SelectedItem)
					{
						for (UObject* SelectedRow : SelectedRows)
						{
							SelectedRow->ClearFlags(RF_Standalone);
						}
						SelectedRows.SetNum(0);
						UChooserTable* Chooser = GetChooser();
						
						if (!Chooser->ResultsStructs.IsValidIndex(SelectedItem->RowIndex))
						{
							SelectRootProperties();
							return;
						}
						
						// Get the list of objects to edit the details of
						TObjectPtr<UChooserRowDetails> Selection = NewObject<UChooserRowDetails>();
						Selection->Chooser = Chooser;
						Selection->Row = SelectedItem->RowIndex;
						Selection->SetFlags(RF_Standalone);
						SelectedRows.Add(Selection);
											
						TArray<UObject*> DetailsObjects;
						for(auto& Item : SelectedRows)
						{
							DetailsObjects.Add(Item.Get());
						}

						if( DetailsView.IsValid() )
						{
							// Make sure details window is pointing to our object
							DetailsView->SetObjects( DetailsObjects );
						}
					}
				})
    			.OnGenerateRow_Raw(this, &FChooserTableEditor::GenerateTableRow)
				.HeaderRow(HeaderRow);


	TSharedRef<SComboButton> EditChooserTableButton = SNew(SComboButton)
		.ButtonStyle(FAppStyle::Get(), "GraphBreadcrumbButton");
	
	EditChooserTableButton->SetOnGetMenuContent(
    		FOnGetContent::CreateLambda(
    			[ EditChooserTableButton, this]()
                			{
    							FMenuBuilder MenuBuilder(true, nullptr);
                            
								UObject* RootChooser = GetRootChooser();
								TArray<UObject*> ObjectsInPackage;
								GetObjectsWithOuter(RootChooser->GetPackage(), ObjectsInPackage);

								for (UObject* Object : ObjectsInPackage)
								{
									if (UChooserTable* Chooser = Cast<UChooserTable>(Object))
									{
										MenuBuilder.AddMenuEntry( FText::FromString(Chooser->GetName()), LOCTEXT("AddExistingObjectTooltip", "Add a reference to this existing Chooser Table."), FSlateIcon(),
											FUIAction(FExecuteAction::CreateLambda([this, EditChooserTableButton, Chooser, RootChooser]()
											{
												while(GetChooser() != RootChooser)
												{
													PopChooserTableToEdit();
												}
												if (Chooser != RootChooser)
												{
													PushChooserTableToEdit(Chooser);
												}
												EditChooserTableButton->SetIsOpen(false);
											})));
									}
                            	}
    
    							return MenuBuilder.MakeWidget();
    
                			})
    		);

	return SNew(SDockTab)
		.Label( LOCTEXT("ChooserTableTitle", "Chooser Table") )
		.TabColorScale( GetTabColorScale() )
		.OnCanCloseTab_Lambda([]() { return false; })
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(3)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					EditChooserTableButton
				]
				+ SHorizontalBox::Slot().FillWidth(1)
				[
					BreadcrumbTrail.ToSharedRef()
				]
			]
			+ SVerticalBox::Slot().FillHeight(1)
			[
				SNew(SScrollBox).Orientation(Orient_Horizontal)
				+ SScrollBox::Slot()
				[
					TableView.ToSharedRef()
				]
			]
		];
}

void FChooserTableEditor::UpdateTableRows()
{
	UChooserTable* Chooser = GetChooser();
	int32 NewNum = Chooser->ResultsStructs.Num();

	// Sync the TableRows array which drives the ui table to match the number of results.
	TableRows.SetNum(0, false);
	for(int i =0; i < NewNum; i++)
	{
		TableRows.Add(MakeShared<FChooserTableRow>(i));
	}

	// Add one at the end, for the Fallback result
	TableRows.Add(MakeShared<FChooserTableRow>(SChooserTableRow::SpecialIndex_Fallback));
	// Add one at the end, for the "Add Row" control
	TableRows.Add(MakeShared<FChooserTableRow>(SChooserTableRow::SpecialIndex_AddRow));

	// Make sure each column has the same number of row datas as there are results
	for(FInstancedStruct& ColumnData : Chooser->ColumnsStructs)
	{
		FChooserColumnBase& Column = ColumnData.GetMutable<FChooserColumnBase>();
		Column.SetNumRows(NewNum);
	}

	if (TableView.IsValid())
	{
		TableView->RebuildList();
	}
}

void FChooserTableEditor::OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap)
{
	bool bChangedAny = false;

	// Refresh our details view if one of the objects replaced was in the map. This gets called before the reinstance GC fixup, so we might as well fixup EditingObjects now too
	for (int32 i = 0; i < EditingObjects.Num(); i++)
	{
		UObject* SourceObject = EditingObjects[i];
		UObject* ReplacedObject = ReplacementMap.FindRef(SourceObject);

		if (ReplacedObject && ReplacedObject != SourceObject)
		{
			EditingObjects[i] = ReplacedObject;
			bChangedAny = true;
		}
	}

	if (bChangedAny)
	{
		SelectRootProperties();
	}
}

FString FChooserTableEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Chooser Table Asset ").ToString();
}

TSharedRef<FChooserTableEditor> FChooserTableEditor::CreateEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit, FGetDetailsViewObjects GetDetailsViewObjects )
{
	TSharedRef< FChooserTableEditor > NewEditor( new FChooserTableEditor() );

	TArray<UObject*> ObjectsToEdit;
	ObjectsToEdit.Add( ObjectToEdit );
	NewEditor->InitEditor( Mode, InitToolkitHost, ObjectsToEdit, GetDetailsViewObjects );

	return NewEditor;
}

TSharedRef<FChooserTableEditor> FChooserTableEditor::CreateEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, const TArray<UObject*>& ObjectsToEdit, FGetDetailsViewObjects GetDetailsViewObjects )
{
	TSharedRef< FChooserTableEditor > NewEditor( new FChooserTableEditor() );
	NewEditor->InitEditor( Mode, InitToolkitHost, ObjectsToEdit, GetDetailsViewObjects );
	return NewEditor;
}
	
void FChooserTableEditor::SelectColumn(UChooserTable* ChooserEditor, int Index)
{
	UChooserTable* Chooser = GetChooser();
   	if (Index < Chooser->ColumnsStructs.Num())
   	{
   		if (SelectedColumn == nullptr)
   		{
   			SelectedColumn = NewObject<UChooserColumnDetails>();
   			SelectedColumn->SetFlags(RF_Standalone);
   		}
   
   		SelectedColumn->Chooser = Chooser;
   		SelectedColumn->Column = Index;
   		DetailsView->SetObject(SelectedColumn, true);
   	}
   	else
   	{
   		SelectRootProperties();
   	}
}
	
void FChooserTableEditor::ClearSelectedColumn()
{
	UChooserTable* Chooser = GetChooser();
	if (SelectedColumn != nullptr)
	{
		SelectedColumn->Column = -1;
		if (DetailsView->GetSelectedObjects().Contains(SelectedColumn))
		{
			SelectRootProperties();
		}
	}
}
	
void FChooserTableEditor::DeleteColumn(int Index)
{
	const FScopedTransaction Transaction(LOCTEXT("Delete Column Transaction", "Delete Column"));
	ClearSelectedColumn();
	SelectRootProperties();
	UChooserTable* Chooser = GetChooser();

	if (Index < Chooser->ColumnsStructs.Num())
	{
		Chooser->Modify(true);
		Chooser->ColumnsStructs.RemoveAt(Index);
		UpdateTableColumns();
	}
}

TSharedRef<SWidget> CreateAssetWidget(bool bReadOnly, UObject* TransactionObject, void* Value, UClass* ResultBaseClass, FChooserWidgetValueChanged ValueChanged)
{
	FAssetChooser* DIAsset = static_cast<FAssetChooser*>(Value);

	UObject* Asset = DIAsset->Asset;
	
	return SNew(SObjectPropertyEntryBox)
		.IsEnabled(!bReadOnly)
		.AllowedClass(ResultBaseClass ? ResultBaseClass : UObject::StaticClass())
		.ObjectPath_Lambda([DIAsset](){ return DIAsset->Asset ? DIAsset->Asset.GetPath() : "";})
		.OnObjectChanged_Lambda([TransactionObject, DIAsset](const FAssetData& AssetData)
		{
			const FScopedTransaction Transaction(LOCTEXT("Edit Asset", "Edit Asset"));
			TransactionObject->Modify(true);
			DIAsset->Asset = AssetData.GetAsset();
		});
}
	
TSharedRef<SWidget> CreateClassWidget(bool bReadOnly, UObject* TransactionObject, void* Value, UClass* ResultBaseClass, FChooserWidgetValueChanged ValueChanged)
{
	FClassChooser* ClassChooser = static_cast<FClassChooser*>(Value);

	UClass* Class = ClassChooser->Class;

	return SNew(SClassPropertyEntryBox)
			.IsEnabled(!bReadOnly)
			.MetaClass(ResultBaseClass ? ResultBaseClass : UObject::StaticClass())
			.SelectedClass_Lambda([ClassChooser]()
			{
				return ClassChooser->Class;
			})
			.OnSetClass_Lambda([TransactionObject, ClassChooser](const UClass* SelectedClass)
			{
				const FScopedTransaction Transaction(LOCTEXT("Edit Class", "Edit Class"));
				TransactionObject->Modify(true);
				ClassChooser->Class = const_cast<UClass*>(SelectedClass);
			});
}

TSharedRef<SWidget> CreateEvaluateChooserWidget(bool bReadOnly, UObject* TransactionObject, void* Value, UClass* ResultBaseClass, FChooserWidgetValueChanged ValueChanged)
{
	FEvaluateChooser* EvaluateChooser = static_cast<FEvaluateChooser*>(Value);
	
	return SNew(SObjectPropertyEntryBox)
		.IsEnabled(!bReadOnly)
		.AllowedClass(UChooserTable::StaticClass())
		.ObjectPath_Lambda([EvaluateChooser](){ return EvaluateChooser->Chooser ? EvaluateChooser->Chooser.GetPath() : "";})
		.OnShouldFilterAsset_Lambda([ResultBaseClass](const FAssetData& InAssetData)
		{
			if (ResultBaseClass == nullptr)
		 	{
		 		return false;
		 	}
		 	if (InAssetData.IsInstanceOf(UChooserTable::StaticClass()))
		 	{
		 		if (UChooserTable* Chooser = Cast<UChooserTable>(InAssetData.GetAsset()))
		 		{
		 			UChooserTable* ContextOwner = Chooser->GetContextOwner();
		 			return !(ContextOwner->OutputObjectType && ContextOwner->OutputObjectType->IsChildOf(ResultBaseClass));
		 		}
		 	}
			return true;
		})
		.OnObjectChanged_Lambda([TransactionObject, EvaluateChooser](const FAssetData& AssetData)
		{
			const FScopedTransaction Transaction(LOCTEXT("Edit Chooser", "Edit Chooser"));
			TransactionObject->Modify(true);
			EvaluateChooser->Chooser = Cast<UChooserTable>(AssetData.GetAsset());
		});
}

TSharedRef<SWidget> CreateNestedChooserWidget(bool bReadOnly, UObject* TransactionObject, void* Value, UClass* ResultBaseClass, FChooserWidgetValueChanged ValueChanged)
{
	FNestedChooser* NestedChooser = static_cast<FNestedChooser*>(Value);
	
	TSharedRef<SComboButton> Button = SNew(SComboButton)
			.ContentPadding(0)
			.ButtonContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SImage).Image(FChooserEditorStyle::Get().GetBrush("ChooserEditor.ChooserTableIconSmall"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0).Padding(2)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text_Lambda([NestedChooser]()
					{
						if (NestedChooser->Chooser)
						{
							return FText::FromString(NestedChooser->Chooser->GetName());
						}
						else
						{
							return LOCTEXT("None", "None");
						}
					})
				]
			];
			
	Button->SetOnGetMenuContent(
		FOnGetContent::CreateLambda(
			[ Button, NestedChooser, TransactionObject]()
            			{
							FMenuBuilder MenuBuilder(true, nullptr);
                        
                        	MenuBuilder.BeginSection(NAME_None, LOCTEXT("Nested Chooser", "Nested Chooser"));
                        	{
							MenuBuilder.AddMenuEntry(
								LOCTEXT("None", "None"), 
								LOCTEXT("NewNestedChooser_Tooltip", "Create a new Chooser Table embedded in this asset, and reference it"),
								FSlateIcon(),
								FUIAction(FExecuteAction::CreateLambda([Button, NestedChooser, TransactionObject]()
								{
									const FScopedTransaction Transaction(LOCTEXT("Clear Nested Chooser", "Clear Nested Chooser"));
									TransactionObject->Modify(true);
									Button->SetIsOpen(false);
									NestedChooser->Chooser = nullptr;
								})));
								
                        		MenuBuilder.AddMenuEntry(
                        			LOCTEXT("New Nested Chooser", "New Nested Chooser"), 
                        			LOCTEXT("NewNestedChooser_Tooltip", "Create a new Chooser Table embedded in this asset, and reference it"),
                        			FSlateIcon(),
                        			FUIAction(FExecuteAction::CreateLambda([Button, NestedChooser, TransactionObject]()
                        			{
                        				Button->SetIsOpen(false);
                        				
										TSharedRef<STextEntryPopup> TextEntry =
											SNew(STextEntryPopup)
											.Label(LOCTEXT("NewNestedChooserLabel", "New Chooser Name"))
											.OnTextCommitted_Lambda([NestedChooser, TransactionObject](FText InText, ETextCommit::Type InCommitType)
											{
												const FScopedTransaction Transaction(LOCTEXT("Assign New Nested Chooser", "Assign New Nested Chooser"));
												TransactionObject->Modify(true);
											
												FSlateApplication::Get().DismissAllMenus();
												NestedChooser->Chooser = NewObject<UChooserTable>(TransactionObject->GetPackage(), UChooserTable::StaticClass(), FName(InText.ToString()));
												NestedChooser->Chooser->ParentTable = Cast<UChooserTable>(TransactionObject);
											});
							
										FSlateApplication& SlateApp = FSlateApplication::Get();
										SlateApp.PushMenu(
											SlateApp.GetInteractiveTopLevelWindows()[0],
											FWidgetPath(),
											TextEntry,
											SlateApp.GetCursorPos(),
											FPopupTransitionEffect::TypeInPopup
											);
                        			})));
								
								MenuBuilder.AddSubMenu(LOCTEXT("Select Existing", "Select Existing"),
									LOCTEXT("SelectExisting_Tooltip", "Select Existing Embedded UChooserTable from this asset"),
									FNewMenuDelegate::CreateLambda([Button, NestedChooser, TransactionObject](FMenuBuilder& SubMenuBuilder)
									{
										SubMenuBuilder.BeginSection("Existing", LOCTEXT("Existing Choosers", "Existing Choosers"));

										TArray<UObject*> ObjectsInPackage;
										GetObjectsWithOuter(TransactionObject->GetPackage(), ObjectsInPackage);

										UObject* RootChooser = TransactionObject->GetPackage()->FindAssetInPackage();

										for (UObject* Object : ObjectsInPackage)
										{
											 // exclude the main chooser 
											if (Object != RootChooser)
											{
												if (UChooserTable* Chooser = Cast<UChooserTable>(Object))
												{
													SubMenuBuilder.AddMenuEntry( FText::FromString(Chooser->GetName()), LOCTEXT("AddExistingObjectTooltip", "Add a reference to this existing Chooser Table."), FSlateIcon(),
														FUIAction(FExecuteAction::CreateLambda([Chooser, Button, NestedChooser, TransactionObject]()
														{
															const FScopedTransaction Transaction(LOCTEXT("Set Nested Chooser", "Set Nested Chooser"));
															TransactionObject->Modify(true);
															Button->SetIsOpen(false);
															NestedChooser->Chooser = Chooser;
														})));
												}
											}
										}
										

									}));
                                     
									
									
                        	}

							return MenuBuilder.MakeWidget();

            			})
		);
		
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0)
		[
			Button
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("Edit","Edit"))
			.OnClicked_Lambda([NestedChooser, TransactionObject]()
			{
				if (NestedChooser->Chooser)
				{
					UObject* RootChooser = TransactionObject->GetPackage()->FindAssetInPackage();
					IAssetEditorInstance* Editor = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(RootChooser, true);
					if (Editor && Editor->GetEditorName() == FChooserTableEditor::EditorName)
					{
						// currently nested tables are only editable in the Chooser Table Editor
						FChooserTableEditor* ChooserEditor = static_cast<FChooserTableEditor*>(Editor);
						ChooserEditor->PushChooserTableToEdit(NestedChooser->Chooser);
					}
				}
				return FReply::Handled();
			})
		];
}
	
class FChooserDetails : public IDetailCustomization
{
public:
	FChooserDetails() {};
	virtual ~FChooserDetails() override {};

	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable( new FChooserDetails() );
	}

	// IDetailCustomization interface
	virtual void CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder) override;
};
		
void FChooserDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	UChooserTable* Chooser = Cast<UChooserTable>(Objects[0]);
	
	IDetailCategoryBuilder& HiddenCategory = DetailBuilder.EditCategory(TEXT("Hidden"));

	TArray<TSharedRef<IPropertyHandle>> HiddenProperties;
	HiddenCategory.GetDefaultProperties(HiddenProperties);
	for(TSharedRef<IPropertyHandle>& PropertyHandle :  HiddenProperties)
	{
		// these (Results and Columns arrays) need to be hidden when showing the root ChooserTable properties
		// but still need to be EditAnywhere so that the Properties exist for displaying when you select a row or column (eg by FChooserRowDetails below)
		PropertyHandle->MarkHiddenByCustomization();
	}
}

class FChooserRowDetails : public IDetailCustomization
{
public:
	FChooserRowDetails() {};
	virtual ~FChooserRowDetails() override {};

	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable( new FChooserRowDetails() );
	}

	// IDetailCustomization interface
	virtual void CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder) override;
};

// Make the details panel show the values for the selected row, showing each column value
void FChooserRowDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	UChooserRowDetails* Row = Cast<UChooserRowDetails>(Objects[0]);
	UChooserTable* Chooser = Row->Chooser;
	
	if (Chooser->ResultsStructs.IsValidIndex(Row->Row))
	{
		IDetailCategoryBuilder& PropertiesCategory = DetailBuilder.EditCategory("Row Properties");

		TSharedPtr<IPropertyHandle> ChooserProperty = DetailBuilder.GetProperty("Chooser", Row->StaticClass());
		DetailBuilder.HideProperty(ChooserProperty);
	
		TSharedPtr<IPropertyHandle> ResultsArrayProperty = ChooserProperty->GetChildHandle("ResultsStructs");
		TSharedPtr<IPropertyHandle> CurrentResultProperty = ResultsArrayProperty->AsArray()->GetElement(Row->Row);
		IDetailPropertyRow& NewResultProperty = PropertiesCategory.AddProperty(CurrentResultProperty);
		NewResultProperty.DisplayName(LOCTEXT("ResultColumnName","Result"));
		NewResultProperty.ShowPropertyButtons(false); // hide array add button
		NewResultProperty.ShouldAutoExpand(true);
	
		for(int ColumnIndex=0; ColumnIndex<Chooser->ColumnsStructs.Num(); ColumnIndex++)
		{
			FChooserColumnBase& Column = Chooser->ColumnsStructs[ColumnIndex].GetMutable<FChooserColumnBase>();
			TSharedRef<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(Chooser->ColumnsStructs[ColumnIndex].GetScriptStruct(), reinterpret_cast<uint8*>(&Column));
			TSharedPtr<IPropertyHandle> ColumnDataProperty = DetailBuilder.AddStructurePropertyData({StructOnScope}, Column.RowValuesPropertyName());
			uint32 NumElements = 0;
			ColumnDataProperty->AsArray()->GetNumElements(NumElements);
			if (Row->Row < (int)NumElements)
			{
				TSharedRef<IPropertyHandle> CellData = ColumnDataProperty->AsArray()->GetElement(Row->Row);
	
				IDetailPropertyRow& NewColumnProperty = PropertiesCategory.AddProperty(CellData);
				FText DisplayName = LOCTEXT("No Input Value", "No Input Value");
				if (FChooserParameterBase* InputValue = Column.GetInputValue())
				{
					InputValue->GetDisplayName(DisplayName); 
				}
				NewColumnProperty.DisplayName(DisplayName);
				NewColumnProperty.ShowPropertyButtons(false); // hide array add button
				NewColumnProperty.ShouldAutoExpand(true);
			}
		}
	}
}

class FChooserColumnDetails : public IDetailCustomization
{
public:
	FChooserColumnDetails() {};
	virtual ~FChooserColumnDetails() override {};

	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable( new FChooserColumnDetails() );
	}

	// IDetailCustomization interface
	virtual void CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder) override;
};

// Make the details panel show the values for the selected row, showing each column value
void FChooserColumnDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	UChooserColumnDetails* Column = Cast<UChooserColumnDetails>(Objects[0]);
	UChooserTable* Chooser = Column->Chooser;
	
	if (Chooser->ColumnsStructs.IsValidIndex(Column->Column))
	{
		IDetailCategoryBuilder& PropertiesCategory = DetailBuilder.EditCategory("Column Properties");

		TSharedPtr<IPropertyHandle> ChooserProperty = DetailBuilder.GetProperty("Chooser", Column->StaticClass());
		DetailBuilder.HideProperty(ChooserProperty);
	
		TSharedPtr<IPropertyHandle> ColumnsArrayProperty = ChooserProperty->GetChildHandle("ColumnsStructs");
		TSharedPtr<IPropertyHandle> CurrentColumnProperty = ColumnsArrayProperty->AsArray()->GetElement(Column->Column);
		IDetailPropertyRow& NewColumnProperty = PropertiesCategory.AddProperty(CurrentColumnProperty);
		NewColumnProperty.DisplayName(LOCTEXT("Selected Column","Selected Column"));
		NewColumnProperty.ShowPropertyButtons(false); // hide array add button
		NewColumnProperty.ShouldAutoExpand(true);
	}
}

void FChooserTableEditor::RegisterWidgets()
{
	FObjectChooserWidgetFactories::RegisterWidgetCreator(FAssetChooser::StaticStruct(), CreateAssetWidget);
	FObjectChooserWidgetFactories::RegisterWidgetCreator(FClassChooser::StaticStruct(), CreateClassWidget);
	FObjectChooserWidgetFactories::RegisterWidgetCreator(FEvaluateChooser::StaticStruct(), CreateEvaluateChooserWidget);
	FObjectChooserWidgetFactories::RegisterWidgetCreator(FNestedChooser::StaticStruct(), CreateNestedChooserWidget);

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	
	PropertyModule.RegisterCustomClassLayout("ChooserTable", FOnGetDetailCustomizationInstance::CreateStatic(&FChooserDetails::MakeInstance));	
	PropertyModule.RegisterCustomClassLayout("ChooserRowDetails", FOnGetDetailCustomizationInstance::CreateStatic(&FChooserRowDetails::MakeInstance));	
	PropertyModule.RegisterCustomClassLayout("ChooserColumnDetails", FOnGetDetailCustomizationInstance::CreateStatic(&FChooserColumnDetails::MakeInstance));	
}
}

#undef LOCTEXT_NAMESPACE
