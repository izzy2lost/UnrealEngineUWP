// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sidebar/SSidebar.h"
#include "Framework/Application/SlateApplication.h"
#include "SidebarButtonMenuContext.h"
#include "Sidebar/ISidebarDrawerContent.h"
#include "Sidebar/SSidebarButton.h"
#include "Sidebar/SSidebarDrawer.h"
#include "Sidebar/SSidebarDrawerContent.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "SSidebar"

SSidebar::~SSidebar()
{
	RemoveAllDrawers();
}

void SSidebar::Construct(const FArguments& InArgs, const TSharedRef<SOverlay>& InDrawersOverlay, const TSharedRef<SBox>& InDockLocation)
{
	DrawersOverlayWeak = InDrawersOverlay;
	DockLocationWeak = InDockLocation;

	TabLocation = InArgs._TabLocation;
	bHideWhenDocked = InArgs._HideWhenDocked;
	bAlwaysUseMaxButtonSize = InArgs._AlwaysUseMaxButtonSize;
	bDisablePin = InArgs._DisablePin;
	bDisableDock = InArgs._DisableDock;
	OnDockStateChanged = InArgs._OnDockStateChanged;

	SetVisibility(EVisibility::SelfHitTestInvisible);
	
	ChildSlot
	.Padding(FMargin(
		TabLocation == ESidebarTabLocation::Right ? 2.f : 0.f,
		TabLocation == ESidebarTabLocation::Bottom ? 2.f : 0.f,
		TabLocation == ESidebarTabLocation::Left ? 2.f : 0.f,
		TabLocation == ESidebarTabLocation::Top ? 2.f : 0.f))
	[
		SNew(SBorder)
		.Padding(0.0f)
		.BorderImage(FAppStyle::Get().GetBrush(TEXT("Docking.Sidebar.Background")))
		[
			SAssignNew(TabButtonContainer, SScrollBox)
			.Orientation(IsHorizontal() ? EOrientation::Orient_Horizontal : EOrientation::Orient_Vertical)
			.ScrollBarAlwaysVisible(false)
			.ScrollBarVisibility(EVisibility::Collapsed)
		]
	];
}

bool SSidebar::RegisterDrawer(FSidebarDrawerConfig&& InDrawerConfig)
{
	if (ContainsDrawer(InDrawerConfig.UniqueId))
	{
		return false;
	}

	const TSharedRef<FSidebarDrawer> NewDrawer = MakeShared<FSidebarDrawer>(MoveTemp(InDrawerConfig));
	NewDrawer->bDisablePin = bDisablePin;
	NewDrawer->bDisableDock = bDisableDock;
	NewDrawer->ContentWidget = NewDrawer->Config.OverrideContentWidget.IsValid()
		? NewDrawer->ContentWidget = NewDrawer->Config.OverrideContentWidget
		: NewDrawer->ContentWidget = SNew(SSidebarDrawerContent, NewDrawer);

	// Add tab button
	TabButtonContainer->AddSlot()
		[
			SAssignNew(NewDrawer->ButtonWidget, SSidebarButton, NewDrawer, TabLocation)
			.MinButtonSize(bAlwaysUseMaxButtonSize ? MaxTabButtonSize : MinTabButtonSize)
			.MaxButtonSize(MaxTabButtonSize)
			.ButtonThickness(TabButtonThickness)
			.OnPressed(this, &SSidebar::OnTabDrawerButtonPressed)
			.OnPinToggled(this, &SSidebar::OnDrawerTabPinToggled)
			.OnDockToggled(this, &SSidebar::OnDrawerTabDockToggled)
			.OnGetContextMenuContent(this, &SSidebar::OnGetTabDrawerContextMenuWidget, NewDrawer)
		];

	Drawers.Add(NewDrawer);

	// Figure out the size this tab should be when opened later. We do it now when the tab still has valid geometry. Once it is moved to the sidebar it will not.
	float TargetDrawerSizePct = NewDrawer->SizeCoefficient;
	if (TargetDrawerSizePct == 0)
	{
		TSharedPtr<SWindow> MyWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		if (MyWindow.IsValid() && NewDrawer->ContentWidget.IsValid())
		{
			TargetDrawerSizePct = NewDrawer->ContentWidget->GetTickSpaceGeometry().GetLocalSize().X / MyWindow->GetPaintSpaceGeometry().GetLocalSize().X;
			NewDrawer->SizeCoefficient = TargetDrawerSizePct;
		}
	}

	// We don't currently allow more than one pinned tab per sidebar, so enforce that
	// Note: it's possible to relax this if users actually want multiple pinned tabs
	if (FindFirstPinnedTab())
	{
		SetDrawerPinned(NewDrawer->GetUniqueId(), false);
	}

	if (NewDrawer->bIsPinned)
	{
		// If this tab is a pinned tab, then open the drawer automatically after it's added
		OpenDrawerNextFrame(NewDrawer, /*bAnimateOpen=*/false);
	}
	else if (NewDrawer->Config.bInitiallyDocked)
	{
		SetDrawerDocked(NewDrawer->GetUniqueId(), true);
	}

	UpdateDrawerAppearance();

	return true;
}

bool SSidebar::UnregisterDrawer(const FName InDrawerId)
{
	if (IsDrawerOpened(InDrawerId))
	{
		CloseAllDrawers();
	}
	
	const int32 IndexToRemove = Drawers.IndexOfByPredicate(
		[InDrawerId](const TSharedRef<FSidebarDrawer>& InDrawer)
		{
			return InDrawerId == InDrawer->GetUniqueId();
		});
	if (IndexToRemove == INDEX_NONE)
	{
		return false;
	}

	TabButtonContainer->RemoveSlot(Drawers[IndexToRemove]->ButtonWidget.ToSharedRef());

	RemoveDrawer(Drawers[IndexToRemove]);
	Drawers.RemoveAt(IndexToRemove);

	SummonPinnedTabIfNothingOpened();

	// Clear the pinned flag when the tab is removed from the sidebar.
	// (Users probably expect that pinning a tab, restoring it/closing it,
	// then moving it to the sidebar again will leave it unpinned the second time.)
	SetDrawerPinned(InDrawerId, false);

	if (Drawers.Num() == 0)
	{
		SetVisibility(EVisibility::Collapsed);
	}
	else
	{
		UpdateDrawerAppearance();
	}

	return true;
}

bool SSidebar::ContainsDrawer(const FName InDrawerId) const
{
	return FindDrawer(InDrawerId).IsValid();
}

int32 SSidebar::GetDrawerCount() const
{
	return Drawers.Num();
}

bool SSidebar::RegisterDrawerSection(const FName InDrawerId, const TSharedPtr<ISidebarDrawerContent>& InSection)
{
	const TSharedPtr<FSidebarDrawer> Drawer = FindDrawer(InDrawerId);
	if (!Drawer.IsValid())
	{
		return false;
	}

	const FName SectionUniqueId = InSection->GetUniqueId();
	if (Drawer->ContentSections.Contains(SectionUniqueId))
	{
		return false;
	}

	Drawer->ContentSections.Add(SectionUniqueId, InSection.ToSharedRef());

	const TSharedPtr<SSidebarDrawerContent> DrawerSection = StaticCastSharedPtr<SSidebarDrawerContent>(Drawer->ContentWidget);
	if (DrawerSection.IsValid())
	{
		DrawerSection->BuildContent();
	}

	return false;
}

bool SSidebar::UnregisterDrawerSection(const FName InDrawerId, const FName InSectionId)
{
	const TSharedPtr<FSidebarDrawer> Drawer = FindDrawer(InDrawerId);
	if (!Drawer.IsValid())
	{
		return false;
	}

	if (!Drawer->ContentSections.Contains(InSectionId))
	{
		return false;
	}

	Drawer->ContentSections.Remove(InSectionId);

	return false;
}

bool SSidebar::TryOpenDrawer(const FName InDrawerId)
{
	if (IsDrawerOpened(InDrawerId) || IsDrawerDocked(InDrawerId))
	{
		return false;
	}

	const TSharedPtr<FSidebarDrawer> Drawer = FindDrawer(InDrawerId);
	if (!Drawer.IsValid())
	{
		return false;
	}

	OpenDrawerNextFrame(Drawer.ToSharedRef());
	return true;
}

void SSidebar::CloseAllDrawers(const bool bInAnimate)
{
	for (const TSharedRef<FSidebarDrawer>& DrawerTab : Drawers)
	{
		CloseDrawerInternal(DrawerTab, bInAnimate);
	}
}

void SSidebar::OnTabDrawerButtonPressed(const TSharedRef<FSidebarDrawer>& InDrawer)
{
	if (InDrawer->bIsOpen)
	{
		// When clicking on the button of an active (but unpinned) tab, close that tab drawer
		if (!IsDrawerPinned(InDrawer->GetUniqueId()))
		{
			CloseDrawerInternal(InDrawer);
		}
	}
	else if (!InDrawer->bIsDocked)
	{
		// Otherwise clicking on an inactive tab should open the drawer
		OpenDrawerInternal(InDrawer);
	}
	else if (InDrawer->bIsDocked && InDrawer->DrawerWidget.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(InDrawer->DrawerWidget);
	}
}

void SSidebar::OnDrawerTabPinToggled(const TSharedRef<FSidebarDrawer>& InDrawer, const bool bIsPinned)
{
	// Set pin state for given tab; clear the pin state for all other tabs
	for (const TSharedRef<FSidebarDrawer>& DrawerTab : Drawers)
	{
		SetDrawerPinned(DrawerTab->GetUniqueId(), DrawerTab == InDrawer ? bIsPinned : false);
	}

	// Open any newly-pinned tab
	if (bIsPinned)
	{
		OpenDrawerInternal(InDrawer);
	}
}

void SSidebar::OnDrawerTabDockToggled(const TSharedRef<FSidebarDrawer>& InDrawer, const bool bIsDocked)
{
	// Undock the previously docked drawer
	if (DockedDrawerTab.IsValid())
	{
		SetDrawerDocked(DockedDrawerTab->GetUniqueId(), false);
	}

	// Dock new drawer if needed
	if (bIsDocked)
	{
		SetDrawerDocked(InDrawer->GetUniqueId(), bIsDocked);
	}
}

void SSidebar::OnTabDrawerFocusLost(const TSharedRef<SSidebarDrawer>& InDrawerWidget)
{
	const TSharedPtr<FSidebarDrawer> Drawer = InDrawerWidget->GetDrawer();
	if (!Drawer.IsValid() || IsDrawerPinned(Drawer->GetUniqueId()))
	{
		return;
	}

	CloseDrawerInternal(Drawer.ToSharedRef());
}

void SSidebar::OnOpenAnimationFinish(const TSharedRef<SSidebarDrawer>& InDrawerWidget)
{
}

void SSidebar::OnCloseAnimationFinish(const TSharedRef<SSidebarDrawer>& InDrawerWidget)
{
	if (const TSharedPtr<SOverlay> DrawersOverlay = DrawersOverlayWeak.Pin())
	{
		DrawersOverlay->RemoveSlot(InDrawerWidget);
	}

	ClosingDrawerWidgets.Remove(InDrawerWidget);
}

void SSidebar::OnDrawerTargetSizeChanged(const TSharedRef<SSidebarDrawer>& InDrawerWidget, const float InNewSize)
{
	const TSharedPtr<SOverlay> DrawersOverlay = DrawersOverlayWeak.Pin();
	if (!DrawersOverlay.IsValid())
	{
		return;
	}
	
	const TSharedPtr<FSidebarDrawer> DrawerWidget = InDrawerWidget->GetDrawer();
	if (!DrawerWidget.IsValid())
	{
		return;
	}
	
	DrawerWidget->SizeCoefficient = InNewSize / DrawersOverlay->GetPaintSpaceGeometry().GetLocalSize().X;
}

TSharedRef<SWidget> SSidebar::OnGetTabDrawerContextMenuWidget(TSharedRef<FSidebarDrawer> InDrawer)
{
	UToolMenus* const ToolMenus = UToolMenus::Get();
	if (!IsValid(ToolMenus))
	{
		return SNullWidget::NullWidget;
	}

	static constexpr const TCHAR* MenuName = TEXT("SidebarTabMenu");

	if (!ToolMenus->IsMenuRegistered(MenuName))
	{
		UToolMenu* const NewMenu = ToolMenus->RegisterMenu(MenuName, NAME_None, EMultiBoxType::Menu);
		check(IsValid(NewMenu));
		
		NewMenu->AddDynamicSection(TEXT("Options"), FNewToolMenuDelegate::CreateSP(this, &SSidebar::BuildOptionsMenu));
	}

	USidebarButtonMenuContext* const ContextObject = NewObject<USidebarButtonMenuContext>();
	ContextObject->Init(SharedThis(this), InDrawer);

	const FToolMenuContext MenuContext(nullptr, nullptr, ContextObject);
	return ToolMenus->GenerateWidget(MenuName, MenuContext);
}

void SSidebar::BuildOptionsMenu(UToolMenu* const InMenu)
{
	if (!IsValid(InMenu))
	{
		return;
	}

	USidebarButtonMenuContext* const ContextMenu = InMenu->FindContext<USidebarButtonMenuContext>();
	if (!IsValid(ContextMenu))
	{
		return;
	}

	const TSharedPtr<FSidebarDrawer> Drawer = ContextMenu->GetDrawer();
	if (!Drawer.IsValid())
	{
		return;
	}

	FToolMenuSection& Section = InMenu->FindOrAddSection(TEXT("Options"), LOCTEXT("Options", "Options"));

	if (Drawer->bIsDocked)
	{
		Section.AddMenuEntry(TEXT("Undock"),
			LOCTEXT("UndockLabel", "Undock"),
			LOCTEXT("UndockToolTip", "Undocks the drawer"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SSidebar::SetDrawerDocked, Drawer->GetUniqueId(), false)));
	}
	else
	{
		Section.AddMenuEntry(TEXT("Dock"),
			LOCTEXT("DockLabel", "Dock"),
			LOCTEXT("DockToolTip", "Docks the drawer"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SSidebar::SetDrawerDocked, Drawer->GetUniqueId(), true)));
	}

	if (Drawer->bIsPinned)
	{
		Section.AddMenuEntry(TEXT("Unpin"),
			LOCTEXT("UnpinLabel", "Unpin"),
			LOCTEXT("UnpinTooltip", "Unpins the drawer from always being displayed"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SSidebar::SetDrawerPinned, Drawer->GetUniqueId(), false)));
	}
	else
	{
		Section.AddMenuEntry(TEXT("Pin"),
			LOCTEXT("PinLabel", "Pin"),
			LOCTEXT("PinTooltip", "Pins the drawer to always be displayed"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SSidebar::SetDrawerPinned, Drawer->GetUniqueId(), true)));
	}
}

void SSidebar::RemoveDrawer(const TSharedRef<FSidebarDrawer>& InDrawer)
{
	if (InDrawer->DrawerWidget.IsValid())
	{
		if (const TSharedPtr<SOverlay> DrawersOverlay = DrawersOverlayWeak.Pin())
		{
			DrawersOverlay->RemoveSlot(InDrawer->DrawerWidget.ToSharedRef());
		}
	}

	InDrawer->bIsOpen = false;

	InDrawer->DrawerClosedDelegate.ExecuteIfBound(InDrawer->GetUniqueId());

	UpdateDrawerAppearance();
}

void SSidebar::RemoveAllDrawers()
{
	PendingTabToOpen.Reset();
	bAnimatePendingTabOpen = false;

	// Closing drawers can remove them from the opened drawers list so copy the list first
	TArray<TSharedRef<SSidebarDrawer>> OpenDrawerWidgetsCopy = OpenDrawerWidgets;

	for (const TSharedRef<SSidebarDrawer>& Drawer : OpenDrawerWidgetsCopy)
	{
		RemoveDrawer(Drawer->GetDrawer().ToSharedRef());
	}

	Drawers.Empty();
}

EActiveTimerReturnType SSidebar::OnOpenPendingDrawerTimer(const double InCurrentTime, const float InDeltaTime)
{
	if (const TSharedPtr<FSidebarDrawer> TabToOpen = PendingTabToOpen.Pin())
	{
		// Wait until the drawers overlay has been arranged once to open the drawer
		// It might not have geometry yet if we're adding back tabs on startup
		if (const TSharedPtr<SOverlay> DrawersOverlay = DrawersOverlayWeak.Pin())
		{
			if (DrawersOverlay->GetTickSpaceGeometry().GetLocalSize().IsZero())
			{
				return EActiveTimerReturnType::Continue;
			}
		}

		OpenDrawerInternal(TabToOpen.ToSharedRef(), bAnimatePendingTabOpen);
	}

	PendingTabToOpen.Reset();
	bAnimatePendingTabOpen = false;
	OpenPendingDrawerTimerHandle.Reset();

	return EActiveTimerReturnType::Stop;
}

void SSidebar::OpenDrawerNextFrame(const TSharedRef<FSidebarDrawer>& InDrawer, const bool bInAnimate)
{
	PendingTabToOpen = InDrawer;
	bAnimatePendingTabOpen = bInAnimate;

	if (!OpenPendingDrawerTimerHandle.IsValid())
	{
		OpenPendingDrawerTimerHandle = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SSidebar::OnOpenPendingDrawerTimer));
	}
}

void SSidebar::OpenDrawerInternal(const TSharedRef<FSidebarDrawer>& InDrawer, const bool bInAnimate)
{
	if (OpenDrawerWidgets.Contains(InDrawer->DrawerWidget))
	{
		return;
	}

	const TSharedPtr<SOverlay> DrawersOverlay = DrawersOverlayWeak.Pin();
	if (!DrawersOverlay.IsValid())
	{
		return;
	}

	PendingTabToOpen.Reset();
	bAnimatePendingTabOpen = false;

	const FGeometry DrawersOverlayGeometry = DrawersOverlay->GetTickSpaceGeometry();
	const FGeometry Geometry = GetTickSpaceGeometry();

	// Calculate padding for the drawer itself
	const float MinDrawerSize = Geometry.GetLocalSize().X - 4.f; // overlap with sidebar border slightly
	const FVector2D ShadowOffset(8.f, 8.f);
	const FMargin SlotPadding(
		TabLocation == ESidebarTabLocation::Left ? MinDrawerSize : 0.f,
		-ShadowOffset.Y,
		TabLocation == ESidebarTabLocation::Right ? MinDrawerSize : 0.f,
		-ShadowOffset.Y);
	const float AvailableWidth = DrawersOverlayGeometry.GetLocalSize().X - SlotPadding.GetTotalSpaceAlong<EOrientation::Orient_Horizontal>();
	const float MaxDrawerSize = AvailableWidth * 0.5f;

	float TargetDrawerSizePct = InDrawer->SizeCoefficient;
	TargetDrawerSizePct = FMath::Clamp(TargetDrawerSizePct, 0.f, 0.5f);

	const float TargetDrawerSize = AvailableWidth * TargetDrawerSizePct;

	if (!InDrawer->DrawerWidget.IsValid())
	{
		InDrawer->DrawerWidget =
			SNew(SSidebarDrawer, InDrawer, TabLocation)
			.MinDrawerSize(MinDrawerSize)
			.TargetDrawerSize(TargetDrawerSize)
			.MaxDrawerSize(MaxDrawerSize)
			.OnDrawerFocusLost(this, &SSidebar::OnTabDrawerFocusLost)
			.OnOpenAnimationFinish(this, &SSidebar::OnOpenAnimationFinish)
			.OnCloseAnimationFinish(this, &SSidebar::OnCloseAnimationFinish)
			.OnDrawerTargetSizeChanged(this, &SSidebar::OnDrawerTargetSizeChanged);
	}

	const TSharedRef<SSidebarDrawer> DrawerWidgetRef = InDrawer->DrawerWidget.ToSharedRef();

	if (ClosingDrawerWidgets.Contains(DrawerWidgetRef))
	{
		ClosingDrawerWidgets.Remove(DrawerWidgetRef);
	}
	else
	{
		DrawersOverlay->AddSlot()
			.Padding(SlotPadding)
			.HAlign(TabLocation == ESidebarTabLocation::Left ? HAlign_Left : HAlign_Right)
			[
				DrawerWidgetRef
			];
	}

	OpenDrawerWidgets.Add(DrawerWidgetRef);

	InDrawer->DrawerWidget->Open(bInAnimate);

	for (const TSharedRef<FSidebarDrawer>& DrawerTab : Drawers)
	{
		DrawerTab->bIsOpen = false;
	}
	InDrawer->bIsOpen = true;

	InDrawer->DrawerOpenedDelegate.ExecuteIfBound(InDrawer->GetUniqueId());

	UpdateDrawerAppearance();

	// This changes the focus and will trigger focus-related events, such as closing other tabs,
	// so it's important that we only call it after we added the new drawer to OpenedDrawers.
	FSlateApplication::Get().SetKeyboardFocus(InDrawer->DrawerWidget);
}

void SSidebar::CloseDrawerInternal(const TSharedRef<FSidebarDrawer>& InDrawer, const bool bInAnimate)
{
	const TSharedPtr<SSidebarDrawer> FoundDrawerWidget = FindOpenDrawerWidget(InDrawer);
	if (OpenDrawerWidgets.Contains(FoundDrawerWidget) && !ClosingDrawerWidgets.Contains(InDrawer->DrawerWidget))
	{
		const TSharedRef<SSidebarDrawer> FoundDrawerWidgetRef = FoundDrawerWidget.ToSharedRef();

		FoundDrawerWidgetRef->Close(bInAnimate);

		if (bInAnimate)
		{
			ClosingDrawerWidgets.Add(FoundDrawerWidgetRef);
		}
		else
		{
			if (const TSharedPtr<SOverlay> DrawersOverlay = DrawersOverlayWeak.Pin())
			{
				DrawersOverlay->RemoveSlot(FoundDrawerWidgetRef);
			}
		}

		OpenDrawerWidgets.Remove(FoundDrawerWidgetRef);

		InDrawer->bIsOpen = false;
	}

	SummonPinnedTabIfNothingOpened();
	UpdateDrawerAppearance();
}

void SSidebar::SummonPinnedTabIfNothingOpened()
{
	// If there's already a tab in the foreground, don't bring the pinned tab forward
	if (GetForegroundTab())
	{
		return;
	}

	// But if there's no current foreground tab, then bring forward a pinned tab (there should be at most one)
	// This should happen when:
	// - the current foreground tab is not pinned and loses focus
	// - the current foreground tab's drawer is manually closed by pressing on the tab button
	// - closing or restoring the current foreground tab
	if (const TSharedPtr<FSidebarDrawer> PinnedTab = FindFirstPinnedTab())
	{
		OpenDrawerInternal(PinnedTab.ToSharedRef());
	}
}

void SSidebar::UpdateDrawerAppearance()
{
	TSharedPtr<FSidebarDrawer> OpenedDrawer;
	if (OpenDrawerWidgets.Num() > 0)
	{
		OpenedDrawer = OpenDrawerWidgets.Last()->GetDrawer();
	}

	for (const TSharedRef<FSidebarDrawer>& Drawer : Drawers)
	{
		if (const TSharedPtr<SSidebarButton> TabButton = StaticCastSharedPtr<SSidebarButton>(Drawer->ButtonWidget))
		{
			TabButton->UpdateAppearance(OpenedDrawer);
		}
	}
}

TSharedPtr<FSidebarDrawer> SSidebar::FindDrawer(const FName InDrawerId) const
{
	const TSharedRef<FSidebarDrawer>* const FoundDrawer = Drawers.FindByPredicate(
		[InDrawerId](const TSharedRef<FSidebarDrawer>& InDrawer)
		{
			return InDrawerId == InDrawer->GetUniqueId();
		});
	return FoundDrawer ? *FoundDrawer : TSharedPtr<FSidebarDrawer>();
}

TSharedPtr<FSidebarDrawer> SSidebar::FindFirstPinnedTab() const
{
	for (const TSharedRef<FSidebarDrawer>& DrawerTab : Drawers)
	{
		if (DrawerTab->bIsPinned)
		{
			return DrawerTab;
		}
	}
	return nullptr;
}

TSharedPtr<FSidebarDrawer> SSidebar::GetForegroundTab() const
{
	const int32 Index = OpenDrawerWidgets.FindLastByPredicate(
		[](const TSharedRef<SSidebarDrawer>& InDrawerWidget)
		{
			return InDrawerWidget->IsOpen() && !InDrawerWidget->IsClosing();
		});
	return Index == INDEX_NONE ? nullptr : OpenDrawerWidgets[Index]->GetDrawer();
}

TSharedPtr<SSidebarDrawer> SSidebar::FindOpenDrawerWidget(const TSharedRef<FSidebarDrawer>& InDrawer) const
{
	const TSharedRef<SSidebarDrawer>* const OpenDrawWidget = OpenDrawerWidgets.FindByPredicate(
		[&InDrawer](const TSharedRef<SSidebarDrawer>& Drawer)
		{
			return InDrawer == Drawer->GetDrawer();
		});
	return OpenDrawWidget ? OpenDrawWidget->ToSharedPtr(): nullptr;
}

bool SSidebar::HasDrawerOpened() const
{
	for (const TSharedRef<FSidebarDrawer>& DrawerTab : Drawers)
	{
		if (DrawerTab->bIsOpen)
		{
			return true;
		}
	}
	return false;
}

bool SSidebar::IsDrawerOpened(const FName InDrawerId) const
{
	for (const TSharedRef<FSidebarDrawer>& DrawerTab : Drawers)
	{
		if (DrawerTab->bIsOpen && DrawerTab->GetUniqueId() == InDrawerId)
		{
			return true;
		}
	}
	return false;
}

FName SSidebar::GetOpenedDrawerId() const
{
	if (OpenDrawerWidgets.IsEmpty())
	{
		return NAME_None;
	}
	
	const TSharedRef<SSidebarDrawer> LastOpenDrawerWidget = OpenDrawerWidgets.Last();
	return LastOpenDrawerWidget->GetDrawer()->GetUniqueId();
}

bool SSidebar::HasDrawerPinned() const
{
	for (const TSharedRef<FSidebarDrawer>& DrawerTab : Drawers)
	{
		if (DrawerTab->bIsPinned)
		{
			return true;
		}
	}
	return false;
}

bool SSidebar::IsDrawerPinned(const FName InDrawerId) const
{
	if (const TSharedPtr<FSidebarDrawer> Drawer = FindDrawer(InDrawerId))
	{
		return PinnedDrawerTabs.Contains(Drawer.ToSharedRef());
	}
	return false;
}

void SSidebar::SetDrawerPinned(const FName InDrawerId, const bool bInIsPinned)
{
	const TSharedPtr<FSidebarDrawer> DrawerToPin = FindDrawer(InDrawerId);
	if (!DrawerToPin.IsValid() || DrawerToPin->bIsPinned == bInIsPinned)
	{
		return;
	}

	if (bInIsPinned)
	{
		if (DrawerToPin->bIsDocked)
		{
			SetDrawerDocked(InDrawerId, false);
		}

		if (!DrawerToPin->bIsOpen)
		{
			OpenDrawerInternal(DrawerToPin.ToSharedRef(), false);
		}
		if (!DrawerToPin->bIsOpen)
		{
			return;
		}

		// In case two modules attempt to register drawers with initially pinned states
		for (const TSharedRef<FSidebarDrawer>& Drawer : Drawers)
		{
			Drawer->bIsPinned = false;
		}
	}

	DrawerToPin->bIsPinned = bInIsPinned;

	if (bInIsPinned)
	{
		PinnedDrawerTabs.AddUnique(DrawerToPin.ToSharedRef());
	}
	else
	{
		PinnedDrawerTabs.Remove(DrawerToPin.ToSharedRef());
	}
}

bool SSidebar::HasDrawerDocked() const
{
	return DockedDrawerTab.IsValid();
}

bool SSidebar::IsDrawerDocked(const FName InDrawerId) const
{
	if (!DockedDrawerTab.IsValid())
	{
		return false;
	}

	const TSharedPtr<FSidebarDrawer> DrawerConfig = FindDrawer(InDrawerId);
	if (!DrawerConfig.IsValid())
	{
		return false;
	}

	return DrawerConfig->GetUniqueId() == DockedDrawerTab->GetUniqueId();
}

void SSidebar::SetDrawerDocked(const FName InDrawerId, const bool bInIsDocked)
{
	const TSharedPtr<SBox> DockLocation = DockLocationWeak.Pin();
	if (!DockLocation.IsValid())
	{
		return;
	}

	const TSharedPtr<FSidebarDrawer> DrawerToDock = FindDrawer(InDrawerId);
	if (!DrawerToDock.IsValid() || DrawerToDock->bIsDocked == bInIsDocked)
	{
		return;
	}

	if (bInIsDocked)
	{
		if (DrawerToDock->bIsPinned)
		{
			SetDrawerPinned(InDrawerId, false);
		}

		CloseAllDrawers(false);

		if (DockedDrawerTab.IsValid())
		{
			UndockAllDrawers();
		}

		DockedDrawerTab = DrawerToDock;

		// In case two modules attempt to register drawers with initially docked states
		for (const TSharedRef<FSidebarDrawer>& Drawer : Drawers)
		{
			Drawer->bIsDocked = false;
		}

		DockedDrawerTab->bIsOpen = true;
		DockedDrawerTab->bIsPinned = false;
		DockedDrawerTab->bIsDocked = true;

		if (DrawerToDock->ContentWidget.IsValid())
		{
			DockLocation->SetContent(DrawerToDock->ContentWidget.ToSharedRef());
			
			if (bHideWhenDocked)
			{
				SetVisibility(EVisibility::Collapsed);
			}
		}
		else
		{
			DockLocation->SetContent(SNullWidget::NullWidget);
			
			if (bHideWhenDocked)
			{
				SetVisibility(EVisibility::Visible);
			}
		}
	}
	else
	{
		if (DockedDrawerTab.IsValid())
		{
			DockedDrawerTab->bIsOpen = false;
			DockedDrawerTab->bIsDocked = false;
			DockedDrawerTab.Reset();

			DockLocation->SetContent(SNullWidget::NullWidget);
			if (bHideWhenDocked)
			{
				SetVisibility(EVisibility::Visible);
			}
		}
	}

	OnDockStateChanged.ExecuteIfBound(InDrawerId);
}

void SSidebar::UndockAllDrawers()
{
	for (const TSharedRef<FSidebarDrawer>& DrawerTab : Drawers)
	{
		SetDrawerDocked(DrawerTab->GetUniqueId(), false);
	}
}

void SSidebar::UpdateDockedSplitterSlot(const FName InDrawerId, SSplitter::FSlot* const InSlot, const bool bInAutoUndock, const float InDefaultDockPercent)
{
	if (!InSlot)
	{
		return;
	}
	
	const TSharedPtr<FSidebarDrawer> Drawer = FindDrawer(InDrawerId);
	if (!Drawer.IsValid())
	{
		return;
	}

	const bool bDocked = IsDrawerDocked(InDrawerId);

	InSlot->SetSizingRule(bDocked ? SSplitter::ESizeRule::FractionOfParent : SSplitter::ESizeRule::SizeToContent);
	InSlot->SetResizable(bDocked);
	if (bInAutoUndock && InSlot->GetSizeValue() < 0.01f)
	{
		Drawer->SizeCoefficient = bDocked ? InDefaultDockPercent : 0.f;
		InSlot->SetSizeValue(Drawer->SizeCoefficient);
	}
}

bool SSidebar::ContainsDrawerSection(const FName InDrawerId, const FName InDrawerSectionId) const
{
	const TSharedPtr<FSidebarDrawer> Drawer = FindDrawer(InDrawerId);
	if (!Drawer.IsValid())
	{
		return false;
	}

	for (const TPair<FName, TSharedRef<ISidebarDrawerContent>>& DrawerSection : Drawer->ContentSections)
	{
		if (DrawerSection.Value->GetSectionId() == InDrawerSectionId)
		{
			return true;
		}
	}

	return false;
}

bool SSidebar::IsHorizontal() const
{
	return TabLocation == ESidebarTabLocation::Top || TabLocation == ESidebarTabLocation::Bottom;
}

bool SSidebar::IsVertical() const
{
	return TabLocation == ESidebarTabLocation::Left || TabLocation == ESidebarTabLocation::Right;
}

#undef LOCTEXT_NAMESPACE
