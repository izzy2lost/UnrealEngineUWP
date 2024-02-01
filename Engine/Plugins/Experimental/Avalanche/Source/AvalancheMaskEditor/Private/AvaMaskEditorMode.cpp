// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaMaskEditorMode.h"

#include "AvaMaskEditorCommands.h"
#include "AvaMaskEditorLog.h"
#include "AvaMaskEditorStyle.h"
#include "AvaMaskEditorSubsystem.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/AvaGizmoComponent.h"
#include "GeometryMaskSubsystem.h"
#include "IAvaMaskEditor.h"
#include "IGeometryMaskWriteInterface.h"
#include "LevelEditor.h"
#include "Mask2D/AvaMask2DBaseModifier.h"
#include "Modifiers/ActorModifierCoreStack.h"
#include "Modules/ModuleManager.h"
#include "Selection.h"
#include "Subsystems/ActorModifierCoreSubsystem.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SGeometryMaskCanvasPreview.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "AvalancheMaskEditorMode"

const FEditorModeID UAvaMaskEditorMode::EM_MotionDesignMaskEditorModeId(UE::AvalancheMaskEditor::MotionDesignMaskEditorModeName);

UAvaMaskEditorMode::UAvaMaskEditorMode()
{
	Info = FEditorModeInfo(UAvaMaskEditorMode::EM_MotionDesignMaskEditorModeId,
		LOCTEXT("MotionDesignMaskEditorModeName", "MotionDesign Mask Editor Mode"),
		FSlateIcon(),
		false);
}

void UAvaMaskEditorMode::Enter()
{
	UEdMode::Enter();

	if (GEditor && GEngine)
	{
		UTypedElementSelectionSet* SelectionSet = GetModeManager()->GetSelectedActors()->GetElementSelectionSet();
		SelectionSet->OnChanged().AddUObject(this, &UAvaMaskEditorMode::OnSelectionChanged);
		
		WeakLastSelectedActor = SelectionSet->GetTopSelectedObject<AActor>();
		WeakActorSelectionSet = SelectionSet;
		
		OnActorSpawnedHandle = GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UAvaMaskEditorMode::OnActorSpawned));

		GEngine->OnLevelActorAdded().AddUObject(this, &UAvaMaskEditorMode::OnActorSpawned);
	}

	for (AActor* Actor : TActorRange<AActor>(GetWorld()))
	{
		if (Actor->FindComponentByInterface<UGeometryMaskWriteInterface>())
		{
			WeakMaskWriterActors.Add(Actor);
			if (UAvaGizmoComponent* GizmoComponent = Actor->GetComponentByClass<UAvaGizmoComponent>())
			{
				GizmoComponent->SetVisibleInEditor(true);
			}
		}
	}

	const FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	if (TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor())
	{
		FText DisplayText = LOCTEXT("MaskViewMode", "Mask View");
    
    	TSharedPtr<SWidget> ToolWidget = nullptr;
    	{
    		static FName ToolkitOverlayMenuName = UE::AvalancheMaskEditor::Internal::ToolkitOverlayMenuName;
    		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(ToolkitOverlayMenuName);
    		Menu->SetStyleSet(&FAvalancheMaskEditorStyle::Get());
    		Menu->StyleName = "AvalancheMaskEditor.ViewportOverlayToolbar";
    		{
    			FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("Default"));
    			{
    				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FAvaMaskEditorCommands::Get().ToggleShowAllMasks));
    				Entry.Icon.Set(FSlateIcon(FAvalancheMaskEditorStyle::GetStyleSetName(), TEXT("AvalancheMaskEditor.ToggleShowAllMasks")));
    			}
    
    			{
    				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FAvaMaskEditorCommands::Get().ToggleEnableMask));
    				Entry.Icon.Set(FSlateIcon(FAvalancheMaskEditorStyle::GetStyleSetName(), TEXT("AvalancheMaskEditor.ToggleDisableMask")));
    			}
    
    			{
    				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FAvaMaskEditorCommands::Get().ToggleIsolateMask));
    				Entry.Icon.Set(FSlateIcon(FAvalancheMaskEditorStyle::GetStyleSetName(), TEXT("AvalancheMaskEditor.ToggleIsolateMask")));
    			}
    		}
    
    		ToolWidget = UToolMenus::Get()->GenerateWidget(Menu);	
    	}
    	
		// ViewportOverlay
		SAssignNew(ViewportOverlayWidget, SOverlay)
			+SOverlay::Slot(-2)
			[
				SAssignNew(CanvasPreviewWidget, SGeometryMaskCanvasPreview)
				.Visibility(EVisibility::HitTestInvisible)
				.Opacity(0.5f)
			]
	
			+SOverlay::Slot()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Bottom)
				.Padding(FMargin(0.0f, 0.0f, 0.f, 15.f))
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("EditorViewport.OverlayBrush"))
					.Padding(8.f)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
						[
							SNew(SImage)
							.Image(FAvalancheMaskEditorStyle::Get().GetBrush(TEXT("AvalancheMaskEditor.ToggleMaskMode")))
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
						[
							SNew(STextBlock)
							.Text(DisplayText)
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(FMargin(2.0, 0.f, 0.f, 0.f))
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
							.TextStyle(FAppStyle::Get(), "DialogButtonText")
							.Text(LOCTEXT("ExitEdit", "Exit"))
							.ToolTipText(LOCTEXT("ExitTooltip", "Exit Mask Mode"))
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							.OnClicked_Lambda([EditorModeManager = GetModeManager()]() 
							{
								EditorModeManager->DeactivateMode(UAvaMaskEditorMode::EM_MotionDesignMaskEditorModeId);						
								return FReply::Handled(); 
							})
						]
					]
				]
			];

		const TSharedRef<SWidget> ViewportOverlayWidgetRef = ViewportOverlayWidget.ToSharedRef();
		ViewportOverlayWidgetRef->SetTag(TEXT("AvaMaskOverlayWidget"));

		LevelEditor->AddViewportOverlayWidget(ViewportOverlayWidgetRef);

		if (const TSharedPtr<SOverlay> ParentOverlay = StaticCastSharedPtr<SOverlay>(ViewportOverlayWidget->GetParentWidget());
			ParentOverlay.IsValid())
		{
			// Now that we have the Overlay widget, remove the slot
			ParentOverlay->RemoveSlot(ViewportOverlayWidgetRef);

			// Then re-add it, specifying zorder
			ParentOverlay->AddSlot(-2)
			[
				ViewportOverlayWidgetRef
			];
		}

		// Initialize, re-using the callback
		OnSelectionChanged(WeakActorSelectionSet.Get());
	}
}

void UAvaMaskEditorMode::Exit()
{
	if (GEditor && GEngine)
	{
		for (TWeakObjectPtr<AActor>& WeakActor : WeakMaskWriterActors)
		{
			if (const AActor* Actor = WeakActor.Get())
			{
				if (UAvaGizmoComponent* GizmoComponent = Actor->GetComponentByClass<UAvaGizmoComponent>())
				{
					GizmoComponent->SetVisibleInEditor(false);
				}
			}
		}

		// Remove selection events
		if (UTypedElementSelectionSet* SelectionSet = WeakActorSelectionSet.Get())
		{
			SelectionSet->OnChanged().RemoveAll(this);
		}

		if (ViewportOverlayWidget.IsValid())
		{
			const FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
			if (TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor())
			{
				LevelEditor->RemoveViewportOverlayWidget(ViewportOverlayWidget.ToSharedRef());
			}
		}
		
		WeakLastSelectedActor = nullptr;
		WeakMaskWriterActors.Reset();
		WeakActorSelectionSet.Reset();

		GetWorld()->RemoveOnActorSpawnedHandler(OnActorSpawnedHandle);

		GEngine->OnLevelActorAdded().RemoveAll(this);
	}

	UEdMode::Exit();
}

bool UAvaMaskEditorMode::UsesToolkits() const
{
	return false;
}

void UAvaMaskEditorMode::CreateToolkit()
{
	// Toolkit = MakeShared<FAvalancheMaskEditorModeToolkit>();
}

void UAvaMaskEditorMode::ModeTick(float DeltaTime)
{
	Super::ModeTick(DeltaTime);
}

bool UAvaMaskEditorMode::IsCompatibleWith(FEditorModeID OtherModeID) const
{
	return true;
}

AActor* UAvaMaskEditorMode::GetActorToParentTo() const
{
	if (AActor* LastSelectedActor = WeakLastSelectedActor.Get())
	{
		return LastSelectedActor;
	}

	USelection* SelectedActors = GetModeManager()->GetSelectedActors();
	if (SelectedActors->Num() > 0)
	{
		return SelectedActors->GetTop<AActor>();
	}

	return nullptr;
}

void UAvaMaskEditorMode::OnSelectionChanged(const UTypedElementSelectionSet* InSelectionSet)
{
	if (InSelectionSet)
	{
		PreviewCanvasName = NAME_None;
		PreviewCanvasChannel = EGeometryMaskColorChannel::None;
		
		if (const UTypedElementSelectionSet* SelectionSet = WeakActorSelectionSet.Get())
		{
			TArray<UObject*> AllSelected = SelectionSet->GetSelectedObjects();
			WeakLastSelectedActor = SelectionSet->GetTopSelectedObject<AActor>();
			if (const AActor* FirstActor = SelectionSet->GetTopSelectedObject<AActor>())
			{
				SelectedMaskModifier = UAvaMask2DBaseModifier::FindMaskModifierOnActor(FirstActor);
				if (SelectedMaskModifier.IsValid())
				{
					SelectedMaskCanvas = GetCanvasReferencedByActor(FirstActor);
					for (const TWeakObjectPtr<AActor>& WeakWriterActor : WeakMaskWriterActors)
					{
						if (const AActor* WriterActor = WeakWriterActor.Get())
						{
							if (const IGeometryMaskWriteInterface* WriterObject = Cast<IGeometryMaskWriteInterface>(WriterActor->FindComponentByInterface<UGeometryMaskWriteInterface>()))
							{
								if (UAvaGizmoComponent* GizmoComponent = WriterActor->GetComponentByClass<UAvaGizmoComponent>())
								{
									FName WriterCanvasName = WriterObject->GetParameters().CanvasName;
									GizmoComponent->SetsStencil(WriterCanvasName == SelectedMaskCanvas->GetCanvasName());
									GizmoComponent->SetStencilId(WriterCanvasName != SelectedMaskCanvas->GetCanvasName() ? 0 : 150);
								}
							}
						}
					}

					PreviewCanvasName = SelectedMaskCanvas->GetCanvasName();
					PreviewCanvasChannel = SelectedMaskCanvas->GetColorChannel();
				}
			}
			else
			{
				// Otherwise the selection has changed and the previous MaskModifier is no longer selected
				SelectedMaskModifier = nullptr;
				SelectedMaskCanvas = nullptr;
			}
		}

		UpdatePreviewWidget();
	}
}

UGeometryMaskCanvas* UAvaMaskEditorMode::GetCanvasReferencedByActor(const AActor* InActor)
{
	FName CanvasName = NAME_None;
	if (const IGeometryMaskWriteInterface* WriteComponent = Cast<IGeometryMaskWriteInterface>(InActor->FindComponentByInterface<UGeometryMaskWriteInterface>()))
	{
		const FGeometryMaskWriteParameters WriteComponentParameters = WriteComponent->GetParameters();
		CanvasName = WriteComponentParameters.CanvasName;
	}	
	else if (const IGeometryMaskReadInterface* ReadComponent = Cast<IGeometryMaskReadInterface>(InActor->FindComponentByInterface<UGeometryMaskReadInterface>()))
	{
		const FGeometryMaskReadParameters ReadComponentParameters = ReadComponent->GetParameters();
		CanvasName = ReadComponentParameters.CanvasName;
	}

	if (UGeometryMaskCanvas* Canvas = GEngine->GetEngineSubsystem<UGeometryMaskSubsystem>()->GetNamedCanvas(CanvasName))
	{
		return Canvas;		
	}

	return nullptr;
}

void UAvaMaskEditorMode::UpdatePreviewWidget()
{
	if (CanvasPreviewWidget.IsValid())
	{
		CanvasPreviewWidget->SetCanvasName(PreviewCanvasName);
		CanvasPreviewWidget->SetColorChannel(PreviewCanvasChannel);
	}
}

void UAvaMaskEditorMode::OnActorSpawned(AActor* InActor)
{
	if (CanMaskSelected(WeakLastSelectedActor.Get()))
	{
		if (AddMaskToSelected({InActor}))
		{
			UpdatePreviewWidget();
		}
	}
}

bool UAvaMaskEditorMode::AddMaskToSelected(const TArray<AActor*>& InMaskingActors)
{
	// @note: This assumes CanMaskSelected passed
	
	AActor* ParentActor = GetActorToParentTo();
	
	UAvaMask2DBaseModifier* ParentMaskModifier = FindOrAddMaskModifier(ParentActor);
	if (!ParentMaskModifier)
	{
		return false;
	}

	const UGeometryMaskCanvas* ParentCanvas = GetCanvasReferencedByActor(ParentActor);
	const FName ChannelName = ParentMaskModifier->GetChannel();
	const EGeometryMaskColorChannel ColorChannel = ParentCanvas ? ParentCanvas->GetColorChannel() : EGeometryMaskColorChannel::Red;

	// @todo: replace SetMode
	// ParentMaskModifier->SetMode(EAvaMask2DMode::Read);

	for (AActor* PlacedActor : InMaskingActors)
	{
		// Parent to Actor
		PlacedActor->AttachToActor(ParentActor, FAttachmentTransformRules::KeepWorldTransform);
		
		// Setup modifier
		if (UAvaMask2DBaseModifier* MaskModifier = FindOrAddMaskModifier(PlacedActor))
		{
			// @todo: replace SetMode
			// MaskModifier->SetMode(EAvaMask2DMode::Write);
			MaskModifier->SetChannel(ChannelName);
			MaskModifier->SetUseParentChannel(true);

			// Flush unused canvases, in case temporary actors were created/destroyed
			if (UGeometryMaskSubsystem* Subsystem = GEngine->GetEngineSubsystem<UGeometryMaskSubsystem>())
			{
				Subsystem->RemoveWithoutWriters();
			}

			PreviewCanvasName = ChannelName;
			PreviewCanvasChannel = ColorChannel;
		}
	}
		
	return true;
}

UAvaMask2DBaseModifier* UAvaMaskEditorMode::FindOrAddMaskModifier(AActor* InActor)
{
	if (const UActorModifierCoreSubsystem* ModifierSubsystem = UActorModifierCoreSubsystem::Get())
	{
		UActorModifierCoreStack* ModifierStack = ModifierSubsystem->GetActorModifierStack(InActor);
		if (!ModifierStack)
		{
			ModifierStack = ModifierSubsystem->AddActorModifierStack(InActor);
		}

		const FName MaskModifierName = GetDefault<UAvaMask2DBaseModifier>()->GetModifierName();
		UAvaMask2DBaseModifier* MaskModifier = nullptr;
		
		TArray<UAvaMask2DBaseModifier*> FoundModifiers;
		ModifierStack->GetClassModifiers<UAvaMask2DBaseModifier>(FoundModifiers);
		if (!FoundModifiers.IsEmpty())
		{
			MaskModifier = FoundModifiers.Last();
		}
		else
		{
			FActorModifierCoreStackInsertOp InsertOp;
			InsertOp.NewModifierName = MaskModifierName;
			
			MaskModifier = Cast<UAvaMask2DBaseModifier>(ModifierStack->InsertModifier(InsertOp));
			if (!MaskModifier)
			{
				UE_LOG(LogAvalancheMaskEditor, Error, TEXT("Error inserting Mask modifier."));
			}
			else
			{
				MaskModifier->SetChannel(MaskModifier->GenerateUniqueMaskName());
			}
		}

		return MaskModifier;
	}

	return nullptr;
}

bool UAvaMaskEditorMode::CanMaskSelected(AActor* InSelectedActor)
{
	auto CanMaskActor = [](AActor* InActor)
	{
		if (const UActorModifierCoreSubsystem* ModifierSubsystem = UActorModifierCoreSubsystem::Get())
		{
			const FName MaskModifierName = GetDefault<UAvaMask2DBaseModifier>()->GetModifierName();
			
			if (const UActorModifierCoreStack* ExistingModifierStack = ModifierSubsystem->GetActorModifierStack(InActor))
			{
				if (ExistingModifierStack->ContainsModifier(MaskModifierName))
				{
					return true;
				}				
			}
			 
			const TSet<FName> AllowedModifiers = ModifierSubsystem->GetAllowedModifiers(InActor);
			if (AllowedModifiers.Contains(MaskModifierName))
			{
				return true;
			}
		}

		return false;
	};

	// Check provided Actor if specified
	if (InSelectedActor)
	{
		return CanMaskActor(InSelectedActor);
	}

	// Otherwise get from USelection
	USelection* SelectedActors = GetModeManager()->GetSelectedActors();

	// Can only add to a single Actor
	if (SelectedActors->Num() != 1)
	{
		return false;
	}

	AActor* SelectedActor = SelectedActors->GetTop<AActor>();
	return CanMaskActor(SelectedActor);
}

#undef LOCTEXT_NAMESPACE
