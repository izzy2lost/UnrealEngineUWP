// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Bindings/MVVMCompiledBindingLibrary.h"
#include "Blueprint/UserWidget.h"
#include "MVVMViewClassExtension.h"
#include "MVVMViewPanelWidgetExtension.generated.h"

class UMVVMView;
class UPanelWidget;
class UUserWidget;

UCLASS()
class MODELVIEWVIEWMODEL_API UMVVMViewPanelWidgetExtension : public UMVVMViewClassExtension
{
	GENERATED_BODY()

public:
	//~ Begin UMVVMViewClassExtension overrides
	virtual void OnViewConstructed(UUserWidget* UserWidget, UMVVMView* View) override;
	virtual void OnViewDestructed(UUserWidget* UserWidget, UMVVMView* View) override;
	//~ End UMVVMViewClassExtension overrides

#if WITH_EDITOR
	struct FInitPanelWidgetExtensionArgs
	{
		FInitPanelWidgetExtensionArgs(FName InWidgetName, FName InEntryViewModelName, const FMVVMVCompiledFieldPath& InWidgetPath, const TSubclassOf<UUserWidget>& InEntryWidgetClass, UPanelSlot* InSlotTemplate, FName InPanelPropertyName, UClass* InEntryViewModelClass)
			: WidgetName(InWidgetName),
			EntryViewModelName(InEntryViewModelName),
			WidgetPath(InWidgetPath),
			EntryWidgetClass(InEntryWidgetClass),
			SlotTemplate(InSlotTemplate),
			PanelPropertyName(InPanelPropertyName),
			EntryViewModelClass(InEntryViewModelClass)
		{}

		FName WidgetName;
		FName EntryViewModelName;
		FMVVMVCompiledFieldPath WidgetPath;
		TSubclassOf<UUserWidget> EntryWidgetClass;
		TObjectPtr<UPanelSlot> SlotTemplate;
		FName PanelPropertyName;
		TObjectPtr<UClass> EntryViewModelClass;
	};

	void Initialize(FInitPanelWidgetExtensionArgs InArgs);

#endif

	FName GetEntryViewModelName() const
	{ 
		return EntryViewModelName; 
	}
	
	UFUNCTION(BlueprintCallable, Category = PanelWidget, meta = (AllowPrivateAccess = true, DisplayName = "Set Items", ViewmodelBlueprintWidgetExtension = "EntryViewModel"))
	virtual void BP_SetItems(const TArray<UObject*>& InItems);

private:
	void SetViewModelOnEntryWidget(UUserWidget* EntryWidget, UObject* ViewModelObject, UUserWidget* OwningUserWidget);
	void ReplaceAllSlots(TArrayView<TTuple<UPanelSlot*, UWidget*>> NewSlots);

private:
	UPROPERTY(VisibleAnywhere, Category = "MVVM Extension")
	FName WidgetName;

	UPROPERTY(VisibleAnywhere, Category = "MVVM Extension")
	FName EntryViewModelName;

	UPROPERTY(VisibleAnywhere, Category = "MVVM Extension")
	TSubclassOf<UUserWidget> EntryWidgetClass;

	UPROPERTY(VisibleAnywhere, Category = "MVVM Extension")
	TObjectPtr<UPanelSlot> SlotTemplate;

	UPROPERTY()
	FName PanelPropertyName;

	UPROPERTY()
	TObjectPtr<UClass> EntryViewModelClass = nullptr;

	UPROPERTY()
	FMVVMVCompiledFieldPath WidgetPath;

	UPROPERTY(Transient)
	TWeakObjectPtr<UPanelWidget> CachedPanelWidget;

	UPROPERTY(Transient)
	TWeakObjectPtr<UUserWidget> CachedOwningUserWidget;
};