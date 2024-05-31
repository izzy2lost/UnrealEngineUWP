// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "WidgetPreview.generated.h"

class UWidgetBlueprint;
struct FImage;
class UWidget;
class UUserWidget;

enum class EWidgetPreviewWidgetChangeType
{
	Assignment = 0,
	Reinstanced = 1,
	Structure = 2,
	ChildReference = 3,
};

// @todo: allow nested previews

UCLASS(BlueprintType, NotBlueprintable, AutoExpandCategories = "Widgets")
class UMGWIDGETPREVIEW_API UWidgetPreview
	: public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Layout")
	const TArray<FName>& GetLayoutSlotNames();

	/** Returns or builds and returns an instance of the root widget for previewing. Can be used to trigger a rebuild. */
	[[maybe_unused]] UUserWidget* GetOrCreateWidgetInstance(UWorld* InWorld, const bool bInForceRecreate = false);

	UUserWidget* GetWidget() const;
	const UUserWidget* GetWidgetForSlot(const FName InSlotName) const;

	/** Convenience function to check that all utilized widgets have bCanCallInitializedWithoutPlayerContext set to true, and reports any that don't. */
	bool CanCallInitializedWithoutPlayerContext(const bool bInRecursive, TArray<const UUserWidget*>& OutFailedWidgets);

	// @todo: move to utility func somewhere else?
	/** Convenience function to check that the provided widget (and it's children) has bCanCallInitializedWithoutPlayerContext set to true, and reports any that don't. */
	static bool CanCallInitializedWithoutPlayerContextOnWidget(const UUserWidget* InUserWidget, const bool bInRecursive, TArray<const UUserWidget*>& OutFailedWidgets);

public:
	using FOnWidgetChanged = TMulticastDelegate<void(const EWidgetPreviewWidgetChangeType)>;

	FOnWidgetChanged& OnWidgetChanged() { return OnWidgetChangedDelegate; }
	
	const TSoftClassPtr<UUserWidget>& GetWidgetType() const;
	void SetWidgetType(const TSoftClassPtr<UUserWidget>& InWidget);
	
	const TSoftClassPtr<UUserWidget>& GetLayoutWidgetType() const;
	void SetLayoutWidgetType(const TSoftClassPtr<UUserWidget>& InWidget);

	const TMap<FName, TSoftClassPtr<UUserWidget>>& GetSlotWidgets() const;
	void SetSlotWidgets(const TMap<FName, TSoftClassPtr<UUserWidget>>& InWidgets);

protected:
	virtual void PostLoad() override;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	void OnWidgetBlueprintChanged(UBlueprint* InBlueprint);

	/** Misc. functionality to perform after a widget assignment is changed. */
	void UpdateWidgets();

	/** Returns slot names not already occupied in SlotWidgets. */
	UFUNCTION(BlueprintCallable, Category = "Layout")
	TArray<FName> GetAvailableLayoutSlotNames();

private:
	/** Widget to use. Will be overlaid on the Layout Widget (if specified). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Widget", DisplayName = "Widget", meta = (AllowPrivateAccess = "true"))
	TSoftClassPtr<UUserWidget> WidgetType;

	/** Optional widget to use for layout, containing named slots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Layout", DisplayName = "Layout Widget", meta = (AllowPrivateAccess = "true"))
	TSoftClassPtr<UUserWidget> LayoutWidgetType;

	/** Widget per-slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Layout", meta = (AllowPrivateAccess = "true", EditCondition = "LayoutWidgetType", GetKeyOptions = "GetAvailableLayoutSlotNames"))
	TMap<FName, TSoftClassPtr<UUserWidget>> SlotWidgets;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> WidgetInstance;

	/** Slot names available in LayoutWidgetType. */
	UPROPERTY(Transient)
	TArray<FName> SlotNameCache;

	FOnWidgetChanged OnWidgetChangedDelegate;
};
