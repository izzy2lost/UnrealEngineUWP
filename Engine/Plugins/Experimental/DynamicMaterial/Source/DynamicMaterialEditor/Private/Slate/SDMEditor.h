// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Components/DMMaterialStage.h"
#include "DMEDefs.h"
#include "DMObjectMaterialProperty.h"
#include "EditorUndoClient.h"
#include "UObject/ObjectKey.h"
#include "Widgets/Layout/SSplitter.h"

class FAssetThumbnailPool;
class FUICommandList;
class IDetailTreeNode;
class IPropertyHandle;
class IPropertyRowGenerator;
class IStructureDetailsView;
class SBox;
class SDMPropertyEdit;
class SDMSlot;
class SDMToolBar;
class SDockTab;
class SExpandableArea;
class SScrollBox;
class SWidgetSwitcher;
class UDMMaterialSlot;
class UDMMaterialStage;
class UDMMaterialStageExpression;
class UDMMaterialValueFloat1;
class UDynamicMaterialModel;
class UDynamicMaterialModelBase;
class UMaterial;
class UMaterialInstanceDynamic;
enum class ECheckBoxState : uint8;
enum class EDMExpressionMenu : uint8;
enum EMaterialDomain : int;
struct FDMActorMaterialSlot;

class SDMEditor : public SCompoundWidget, public FSelfRegisteringEditorUndoClient
{
	SLATE_DECLARE_WIDGET(SDMEditor, SCompoundWidget);

	SLATE_BEGIN_ARGS(SDMEditor) {}
	SLATE_END_ARGS()

public:
	static TSharedRef<FAssetThumbnailPool> GetThumbnailPool();

	static TSharedRef<SWidget> GetEmptyContent();

	static bool GetExpansionState(UObject* InOwner, FName InName, bool& bOutExpanded);
	static void SetExpansionState(UObject* InOwner, FName InName, bool bInIsExpanded);

	static FDMPropertyHandle GetPropertyHandle(const SWidget* InOwningWidget, UObject* InObject, const FName& InPropertyName);

	static void ClearPropertyHandles(const SWidget* InOwningWidget);

	void Construct(const FArguments& InArgs);

	virtual ~SDMEditor() override;

	UDynamicMaterialModelBase* GetMaterialModelBase() const { return MaterialModelBaseWeak.Get(); }
	void SetMaterialModelBase(UDynamicMaterialModelBase* InMaterialModelBase);

	UDynamicMaterialModel* GetMaterialModel() const;

	bool IsDynamicModel() const;

	const FDMObjectMaterialProperty& GetMaterialObjectProperty() const { return ObjectProperty; }
	void SetMaterialObjectProperty(const FDMObjectMaterialProperty& InObjectProperty);

	AActor* GetMaterialActor() const;
	void SetMaterialActor(AActor* InActor);

	void OnMaterialModelSelected(UDynamicMaterialModelBase* InMaterialModelBase);

	void OnMaterialInstanceSelected(UDynamicMaterialInstance* InMaterialInstance);

	void OnActorSelected(AActor* InActor);

	TSharedPtr<SDMSlot> GetActiveSlotWidget() const;

	void RefreshSlotPickerList();
	void RefreshSlotWidget();
	void RefreshComponentEditWidget();

	void InvalidateComponentEditWidget();

	UDMMaterialComponent* GetEditedComponent() const;

	void SetEditedComponent(UDMMaterialComponent* InComponent);

	void ClearEditor();

	void ResetEditor();

	int32 GetActiveSlotIndex() const { return ActiveSlotIndex; }
	void SetActiveSlotIndex(int InSlotIndex);

	const TSharedPtr<FUICommandList>& GetCommandList() const { return CommandList; }

	bool CanAddNewLayer() const;
	void AddNewLayer();

	bool CanInsertNewLayer() const;
	void InsertNewLayer();

	bool CanCopySelectedLayer() const;
	void CopySelectedLayer();

	bool CanCutSelectedLayer() const;
	void CutSelectedLayer();

	bool CanPasteLayer() const;
	void PasteLayer();

	bool CanDuplicateSelectedLayer() const;
	void DuplicateSelectedLayer();

	bool CanDeleteSelectedLayer() const;
	void DeleteSelectedLayer();

	UMaterial* CreatePreviewMaterial(UObject* InPreviewing);
	void FreePreviewMaterial(UObject* InPreviewing);

	UMaterialInstanceDynamic* CreateMID(UMaterial* InMaterialBase);
	void FreeMID(UMaterial* InMaterialBase);

	//~ Begin SWidget
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End SWidget

	//~ Begin FUndoClient
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	//~ End FUndoClient

protected:
	struct FExpansionItem
	{
		TObjectKey<UObject> Owner;
		FName Name;

		friend uint32 GetTypeHash(const FExpansionItem& InItem)
		{
			return HashCombineFast(
				GetTypeHash(InItem.Owner),
				GetTypeHash(InItem.Name)
			);
		}

		bool operator==(const FExpansionItem& InOther) const
		{
			return Owner == InOther.Owner
				&& Name == InOther.Name;
		}
	};

	static TMap<FExpansionItem, bool> ExpansionStates;

	static TSharedPtr<FAssetThumbnailPool> ThumbnailPool;

	static TMap<const SWidget*, TArray<FDMPropertyHandle>> PropertyHandleMap;

	static FDMPropertyHandle CreatePropertyHandle(const void* InOwningWidget, UObject* InObject, const FName& InPropertyName);

	TSharedPtr<SBox> Container;
	TSharedPtr<SDMToolBar> Toolbar;
	TSharedPtr<SBox> SlotPickerContainer;
	TSharedPtr<SSplitter> SplitterContainer;
	TSharedPtr<SScrollBox> SlotContainer;
	TSharedPtr<SScrollBox> ComponentEditContainer;
	SSplitter::FSlot* LayerViewSplitterSlot = nullptr;
	SSplitter::FSlot* ExtraSpaceSplitterSlot = nullptr;
	TSharedPtr<SDMSlot> ActiveSlotWidget;
	bool bHasActiveLayout = false;

	TSharedPtr<FUICommandList> CommandList;
	int32 ActiveSlotIndex;
	TWeakObjectPtr<UDynamicMaterialModelBase> MaterialModelBaseWeak;
	FDMObjectMaterialProperty ObjectProperty;
	TWeakObjectPtr<UDMMaterialComponent> EditedComponent;
	bool bInvalidateComponentEditWidget;

	TMap<FObjectKey, TStrongObjectPtr<UMaterial>> PreviewMaterials;
	TMap<FObjectKey, TStrongObjectPtr<UMaterialInstanceDynamic>> PreviewMaterialDynamics;

	void BindCommands();

	TSharedRef<SWidget> CreateMainLayout();
	TSharedRef<SWidget> CreateSlotPickerWidget();
	TSharedRef<SWidget> CreateSlotWidget();
	TSharedRef<SWidget> CreateComponentEditWidget();

	void OnSplitterResized() const;

	void OnComponentUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType);

	TSharedRef<SWidget> CreateActorMaterialSlotSelector(AActor* InActor);

	void OnMaterialBuilt(UDynamicMaterialModelBase* InMaterialModelBase);
	void OnValuesUpdated(UDynamicMaterialModelBase* InMaterialModelBase);
	void OnSlotsUpdated(UDynamicMaterialModelBase* InMaterialModelBase);

	bool IsPropertyValidForModel(EDMMaterialPropertyType InProperty) const;
	ECheckBoxState GetSlotCheckState(EDMMaterialPropertyType InProperty) const;
	FText GetToolTipForProperty(EDMMaterialPropertyType InProperty) const;
	void OnSlotCheckStateChanged(ECheckBoxState InCheckState, EDMMaterialPropertyType InProperty);

	FReply OnCreateMaterialButtonClicked(FDMObjectMaterialProperty InMaterialProperty);

	void OnToolBarPropertyChanged(TSharedPtr<FDMObjectMaterialProperty> InNewSelectedProperty);
	TSharedRef<SWidget> MakeToolBarSettingsMenu();

	void OnSettingsChanged(const FPropertyChangedEvent& InPropertyChangedEvent);

	void OnUndo();

	UDMMaterialSlot* GetSlotForMaterialProperty(EDMMaterialPropertyType InProperty) const;

	void SetEmptyLayout();
};
