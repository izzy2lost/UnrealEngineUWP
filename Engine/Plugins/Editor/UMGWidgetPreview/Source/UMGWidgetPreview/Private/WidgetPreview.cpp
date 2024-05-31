// Copyright Epic Games, Inc. All Rights Reserved.

#include "WidgetPreview.h"

#include "Blueprint/UserWidget.h"
#include "WidgetBlueprint.h"
#include "WidgetPreviewTypesPrivate.h"

const TArray<FName>& UWidgetPreview::GetLayoutSlotNames()
{
	if (!SlotNameCache.IsEmpty())
	{
		return SlotNameCache;
	}

	if (!LayoutWidgetType.IsNull())
	{
		if (INamedSlotInterface* WidgetWithSlots = Cast<INamedSlotInterface>(LayoutWidgetType.LoadSynchronous()->GetDefaultObject<UUserWidget>()))
		{
			TArray<FName> SlotNames;
			WidgetWithSlots->GetSlotNames(SlotNames);

			SlotNameCache = SlotNames;
			return SlotNameCache;
		}
	}

	static TArray<FName> Empty;
	return Empty;
}

UUserWidget* UWidgetPreview::GetOrCreateWidgetInstance(UWorld* InWorld, const bool bInForceRecreate)
{
	if (bInForceRecreate)
	{
		WidgetInstance = nullptr;
	}
	else if (WidgetInstance)
	{
		return WidgetInstance;
	}

	if (!ensure(InWorld))
	{
		return nullptr;
	}

	TArray<const UUserWidget*> UnsupportedWidgets;
	if (!CanCallInitializedWithoutPlayerContext(true, UnsupportedWidgets))
	{
		// No need to log, this is an expected outcome
		return nullptr;
	}

	if (UUserWidget* Widget = GetWidget())
	{
		auto MakeWidget = [InWorld](UClass* InClass) -> UUserWidget*
		{
			UUserWidget* NewWidget = NewObject<UUserWidget>(InWorld, InClass);
			NewWidget->ClearFlags(RF_Transactional);
			return NewWidget;
		};

		// @todo: always wrap with a container UWidget to account for warning here: void UWidget::RemoveFromParent()

		WidgetInstance = MakeWidget(Widget->GetClass());

		if (!LayoutWidgetType.IsNull() && !SlotWidgets.IsEmpty())
		{
			TArray<FName> ValidSlotNames;
			WidgetInstance->GetSlotNames(ValidSlotNames);

			for (const TPair<FName, TSoftClassPtr<UUserWidget>>& SlotWidget : SlotWidgets)
			{
				if (!SlotWidget.Value.IsNull()
					&& ValidSlotNames.Contains(SlotWidget.Key))
				{
					WidgetInstance->SetContentForSlot(SlotWidget.Key, MakeWidget(SlotWidget.Value.LoadSynchronous()));
				}
			}
		}

		if (ULocalPlayer* LocalPlayer = InWorld->GetFirstLocalPlayerFromController())
		{
			WidgetInstance->SetPlayerContext(LocalPlayer);
		}

		OnWidgetChanged().Broadcast(EWidgetPreviewWidgetChangeType::Reinstanced);

		return WidgetInstance;
	}

	return nullptr;
}

UUserWidget* UWidgetPreview::GetWidget() const
{
	// If the LayoutWidget is in use, return it (as the root widget).
	if (!LayoutWidgetType.IsNull())
	{
		UUserWidget* LayoutWidgetCDO = LayoutWidgetType.LoadSynchronous()->GetDefaultObject<UUserWidget>();
		if (INamedSlotInterface* WidgetWithSlots = Cast<INamedSlotInterface>(LayoutWidgetCDO))
		{
			TArray<FName> SlotNames;
			WidgetWithSlots->GetSlotNames(SlotNames);
			if (!SlotNames.IsEmpty())
			{
				return LayoutWidgetCDO;
			}
		}
	}

	if (!WidgetType.IsNull())
	{
		return WidgetType.LoadSynchronous()->GetDefaultObject<UUserWidget>();
	}

	return nullptr;
}

const UUserWidget* UWidgetPreview::GetWidgetForSlot(const FName InSlotName) const
{
	if (const TSoftClassPtr<UUserWidget>* WidgetInSlot = SlotWidgets.Find(InSlotName))
	{
		if (!WidgetInSlot->IsNull())
		{
			return WidgetInSlot->LoadSynchronous()->GetDefaultObject<UUserWidget>();	
		}

		UE_LOG(LogTemp, Warning, TEXT("Slot %s has invalid widget."), *InSlotName.ToString());
	}

	return nullptr;
}

bool UWidgetPreview::CanCallInitializedWithoutPlayerContext(const bool bInRecursive, TArray<const UUserWidget*>& OutFailedWidgets)
{
	bool bResult = true;

	if (!LayoutWidgetType.IsNull())
	{
		const UUserWidget* LayoutWidgetCDO = LayoutWidgetType.LoadSynchronous()->GetDefaultObject<UUserWidget>();
		bResult = bResult && CanCallInitializedWithoutPlayerContextOnWidget(LayoutWidgetCDO, bInRecursive, OutFailedWidgets);

		for (const TPair<FName, TSoftClassPtr<UUserWidget>>& SlotWidget : SlotWidgets)
		{
			if (!SlotWidget.Value.IsNull())
			{
				const UUserWidget* SlotWidgetCDO = SlotWidget.Value.LoadSynchronous()->GetDefaultObject<UUserWidget>();
				bResult = bResult && CanCallInitializedWithoutPlayerContextOnWidget(SlotWidgetCDO, bInRecursive, OutFailedWidgets);
			}
		}
	}

	if (!WidgetType.IsNull())
	{
		const UUserWidget* WidgetCDO = WidgetType.LoadSynchronous()->GetDefaultObject<UUserWidget>();
		bResult = bResult && CanCallInitializedWithoutPlayerContextOnWidget(WidgetCDO, bInRecursive, OutFailedWidgets);
	}

	// In case there are no widgets to display, we want to return true
	return bResult;
}

bool UWidgetPreview::CanCallInitializedWithoutPlayerContextOnWidget(
	const UUserWidget* InUserWidget,
	const bool bInRecursive,
	TArray<const UUserWidget*>& OutFailedWidgets)
{
	TFunction<bool(const UWidget* InWidgetGeneratedClass)> CanCallInitializedWithoutPlayerContextInternal;
	CanCallInitializedWithoutPlayerContextInternal = [CanCallInitializedWithoutPlayerContextInternal, bInRecursive, &OutFailedWidgets](const UWidget* InWidget)
	{
		// In case there are no widgets to display, we want to return true
		bool bResultInternal = true;

		if (const UUserWidget* AsUserWidget = Cast<UUserWidget>(InWidget))
		{
			UE::UMGWidgetPreview::Private::FWidgetTypeTuple WidgetTuple(AsUserWidget);
			if (WidgetTuple.BlueprintGeneratedClass)
			{
				bResultInternal = WidgetTuple.BlueprintGeneratedClass->bCanCallInitializedWithoutPlayerContext;
				if (!bResultInternal)
				{
					OutFailedWidgets.Emplace(WidgetTuple.ClassDefaultObject);
				}
			}

			if (bInRecursive)
			{
				if (const INamedSlotInterface* WidgetWithSlots = Cast<INamedSlotInterface>(AsUserWidget))
				{
					TArray<FName> SlotNames;
					WidgetWithSlots->GetSlotNames(SlotNames);
					if (!SlotNames.IsEmpty())
					{
						for (const FName SlotName : SlotNames)
						{
							if (const UWidget* SlotWidget = Cast<UWidget>(WidgetWithSlots->GetContentForSlot(SlotName)))
							{
								bResultInternal = bResultInternal && CanCallInitializedWithoutPlayerContextInternal(SlotWidget);
							}
						}
					}
				}
			}
		}

		return bResultInternal;
	};

	return CanCallInitializedWithoutPlayerContextInternal(InUserWidget);
}

const TSoftClassPtr<UUserWidget>& UWidgetPreview::GetWidgetType() const
{
	return WidgetType;
}

void UWidgetPreview::SetWidgetType(const TSoftClassPtr<UUserWidget>& InWidget)
{
	if (WidgetType != InWidget)
	{
		WidgetType = InWidget;
		WidgetInstance = nullptr;
		UpdateWidgets();

		OnWidgetChanged().Broadcast(EWidgetPreviewWidgetChangeType::Assignment);
	}
}

const TSoftClassPtr<UUserWidget>& UWidgetPreview::GetLayoutWidgetType() const
{
	return LayoutWidgetType;
}

void UWidgetPreview::SetLayoutWidgetType(const TSoftClassPtr<UUserWidget>& InWidget)
{
	if (LayoutWidgetType != InWidget)
	{
		LayoutWidgetType = InWidget;
		WidgetInstance = nullptr;
		UpdateWidgets();

		OnWidgetChanged().Broadcast(EWidgetPreviewWidgetChangeType::Assignment);
	}
}

const TMap<FName, TSoftClassPtr<UUserWidget>>& UWidgetPreview::GetSlotWidgets() const
{
	return SlotWidgets;
}

void UWidgetPreview::SetSlotWidgets(const TMap<FName, TSoftClassPtr<UUserWidget>>& InWidgets)
{
	if (!SlotWidgets.OrderIndependentCompareEqual(InWidgets))
	{
		SlotWidgets = InWidgets;
		WidgetInstance = nullptr;
		UpdateWidgets();

		OnWidgetChanged().Broadcast(EWidgetPreviewWidgetChangeType::Assignment);
	}
}

void UWidgetPreview::PostLoad()
{
	UObject::PostLoad();

	UpdateWidgets();
}

void UWidgetPreview::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UWidgetPreview, WidgetType)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UWidgetPreview, LayoutWidgetType)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UWidgetPreview, SlotWidgets)
		|| PropertyName.IsNone()) // None can be an Undo operation
	{
		WidgetInstance = nullptr;
		UpdateWidgets();
		OnWidgetChanged().Broadcast(EWidgetPreviewWidgetChangeType::Assignment);
	}
}

void UWidgetPreview::OnWidgetBlueprintChanged(UBlueprint* InBlueprint)
{
	OnWidgetChanged().Broadcast(EWidgetPreviewWidgetChangeType::Structure);
}

void UWidgetPreview::UpdateWidgets()
{
	auto AddOnChanged = [this](const UUserWidget* InUserWidget)
	{
		const UE::UMGWidgetPreview::Private::FWidgetTypeTuple WidgetTuple(InUserWidget);
		if (UWidgetBlueprint* BP = WidgetTuple.Blueprint)
		{
			BP->OnChanged().AddUObject(this, &UWidgetPreview::OnWidgetBlueprintChanged);
		}
	};

	if (!LayoutWidgetType.IsNull())
	{
		SlotNameCache.Reset();

		AddOnChanged(LayoutWidgetType.LoadSynchronous()->GetDefaultObject<UUserWidget>());

		for (const TPair<FName, TSoftClassPtr<UUserWidget>>& SlotWidget : SlotWidgets)
		{
			if (!SlotWidget.Value.IsNull())
			{
				AddOnChanged(SlotWidget.Value.LoadSynchronous()->GetDefaultObject<UUserWidget>());
			}
		}
	}

	if (!WidgetType.IsNull())
	{
		AddOnChanged(WidgetType.LoadSynchronous()->GetDefaultObject<UUserWidget>());
	}
}

TArray<FName> UWidgetPreview::GetAvailableLayoutSlotNames()
{
	const TArray<FName> AllSlotNames = GetLayoutSlotNames();

	TArray<FName> UsedSlotNamesArray;
	SlotWidgets.GenerateKeyArray(UsedSlotNamesArray);

	const TSet<FName> UsedSlotNames(UsedSlotNamesArray);

	return TSet<FName>(AllSlotNames)
		.Difference(UsedSlotNames)
		.Array();
}
