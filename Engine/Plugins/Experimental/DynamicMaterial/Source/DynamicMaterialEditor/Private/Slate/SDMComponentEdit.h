// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMEDefs.h"
#include "Delegates/IDelegateInstance.h"
#include "EditorUndoClient.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"

class FAssetThumbnailPool;
class FScopedTransaction;
class ICustomDetailsViewItem;
class IDetailKeyframeHandler;
class IDetailTreeNode;
class IPropertyHandle;
class IPropertyRowGenerator;
class SBox;
class SDMEditor;
class SDMEditor;
class SDMStage;
class SWidget;
class UDMMaterialComponent;
class UDMMaterialEffect;
class UDMMaterialEffectStack;
class UDMMaterialSlot;
class UDMMaterialStageExpressionTextureSample;
class UDMMaterialStageInputTextureUV;
class UDMMaterialStageInputThroughput;
class UDMMaterialStageInputValue;
class UDMMaterialStageSource;
class UDMMaterialStageThroughput;
class UDMMaterialValueFloat1;
class UDMTextureUV;
class UDynamicMaterialModelEditorOnlyData;
class UMaterial;
enum class ECheckBoxState : uint8;
struct FDMPropertyHandle;
struct FSlateIcon;

class DYNAMICMATERIALEDITOR_API SDMComponentEdit : public SCompoundWidget, public FSelfRegisteringEditorUndoClient
{
public:
	static ECheckBoxState IsInputPerChannelMapped(UDMMaterialStageThroughput* InThroughput, int32 InInputIdx);
	static FText GetInputChannelMapDescription(UDMMaterialStageThroughput* InThroughput, int32 InInputIdx, int32 InChannelIdx);
	static void CreateKeyFrame(TSharedPtr<IPropertyHandle> InPropertyHandle);
	static TSharedRef<SWidget> CreateExtensionButtons(const TSharedPtr<SWidget>& InPropertyOwner, UDMMaterialComponent* InComponent,
		const FName& InPropertyName, bool bInAllowKeyframe, FSimpleDelegate InOnResetDelegate);
	static TSharedRef<SWidget> CreateExtensionButtons(const TSharedPtr<SDMComponentEdit>& InComponentEditWidget, UDMMaterialComponent* InComponent,
		const FName& InPropertyName, bool bInAllowKeyframe);

	SLATE_BEGIN_ARGS(SDMComponentEdit)
		{}
	SLATE_END_ARGS()

	SDMComponentEdit() = default;
	virtual ~SDMComponentEdit() override;

	void Construct(const FArguments& InArgs, UDMMaterialComponent* InComponent, const TWeakPtr<SDMEditor>& InEditorWidget);

	FORCEINLINE UDMMaterialComponent* GetComponent() const { return ComponentWeak.Get(); }
	FORCEINLINE TSharedPtr<SDMEditor> GetEditorWidget() const { return EditorWidgetWeak.Pin(); }

	TSharedPtr<SWidget> CreateSinglePropertyEditWidget(UDMMaterialComponent* InComponent, const FName& InPropertyName);

	//~ Begin FUndoClient
	virtual void PostUndo(bool bInSuccess) override { OnUndo(); }
	virtual void PostRedo(bool bInSuccess) override { OnUndo(); }
	//~ End FUndoClient

protected:
	static void GenerateMaterialModelPropertyRows(const TSharedRef<SDMEditor> InEditorWidget, UDynamicMaterialModel* InMaterialModel, 
		TArray<FDMPropertyHandle>& InOutPropertyRows, TSet<UDMMaterialComponent*>& InOutProcessedObjects);

	TWeakObjectPtr<UDMMaterialComponent> ComponentWeak;
	TWeakPtr<SDMEditor> EditorWidgetWeak;

	bool bConstructing = false;

	TSharedPtr<IDetailKeyframeHandler> KeyframeHandler;

	TSharedPtr<SBox> Container;

	bool bCreatedWithLinkedUVs;

	FDelegateHandle UpdateHandle;

	TSet<FName> Categories;

	TSharedRef<SWidget> CreateEditWidget();

	TArray<FDMPropertyHandle> GetEditRows();

	TSharedRef<SWidget> CreateSourceTypeEditWidget();

	TSharedRef<SWidget> MakeSourceTypeEditWidgetMenuContent();
	FText GetSourceTypeEditWidgetText() const;

	void OnUndo();

	void OnExpansionStateChanged(const TSharedRef<ICustomDetailsViewItem>& InItem, bool bInExpansionState);
};
