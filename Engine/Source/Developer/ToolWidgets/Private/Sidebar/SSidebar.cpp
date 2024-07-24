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
			SAssignNew(TabContainer, SScrollBox)
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
	TabContainer->AddSlot()
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

	DrawerTabs.Add(NewDrawer);

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
	
	return true;
}

bool SSidebar::UnregisterDrawer(const FName InDrawerId)
{
	if (IsDrawerOpened(InDrawerId))
	{
		CloseAllDrawers();
	}
	
	const int32 FoundIndex = DrawerTabs.IndexOfByPredicate(
		[InDrawerId](const TSharedRef<FSidebarDrawer>& InDrawer)
		{
			return InDrawerId == InDrawer->GetUniqueId();
		});
	if (FoundIndex == INDEX_NONE)
	{
		return false;
	}

	DrawerTabs.RemoveAt(FoundIndex);
	TabContainer->RemoveSlot(DrawerTabs[FoundIndex]->ButtonWidget.ToSharedRef());

	RemoveDrawer(DrawerTabs[FoundIndex]);
	SummonPinnedTabIfNothingOpened();

	// Clear the pinned flag when the tab is removed from the sidebar.
	// (Users probably expect that pinning a tab, restoring it/closing it,
	// then moving it to the sidebar again will leave it unpinned the second time.)
	SetDrawerPinned(InDrawerId, false);

	if (DrawerTabs.Num() == 0)
	{
		SetVisibility(EVisibility::Collapsed);
	}

	return true;
}

bool SSidebar::ContainsDrawer(const FName InDrawerId) const
{
	return FindDrawer(InDrawerId).IsValid();
}

int32 SSidebar::GetDrawerCount() const
{
	return DrawerTabs.Num();
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
	
	if (const TSharedPtr<FSidebarDrawer> InDrawer = FindDrawer(InDrawerId))
	{
		OpenDrawerNextFrame(InDrawer.ToSharedRef(), /*bAnimateOpen=*/ true);
		return true;
	}
	
	return false;
}

void SSidebar::CloseAllDrawers()
{
	for (const TSharedRef<FSidebarDrawer>& DrawerTab : DrawerTabs)
	{
		CloseDrawerInternal(DrawerTab);
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
		OpenDrawerInternal(InDrawer, /*bAnimateOpen=*/true);
	}
}

void SSidebar::OnDrawerTabPinToggled(const TSharedRef<FSidebarDrawer>& InDrawer, const bool bIsPinned)
{
	// Set pin state for given tab; clear the pin state for all other tabs
	for (const TSharedRef<FSidebarDrawer>& DrawerTab : DrawerTabs)
	{
		SetDrawerPinned(DrawerTab->GetUniqueId(), DrawerTab == InDrawer ? bIsPinned : false);
	}

	// Open any newly-pinned tab
	if (bIsPinned)
	{
		OpenDrawerInternal(InDrawer, /*bAnimateOpen=*/true);
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
	const TSharedPtr<FSidebarDrawer> DrawerWidget = InDrawerWidget->GetDrawer();
	if (!DrawerWidget.IsValid())
	{
		return;
	}

	// Don't automatically close a pinned tab that is in the foreground
	if (IsDrawerPinned(DrawerWidget->GetUniqueId()) && DrawerWidget == GetForegroundTab())
	{
		return;
	}

	CloseDrawerInternal(DrawerWidget.ToSharedRef());
}

void SSidebar::OnTabDrawerClosed(const TSharedRef<SSidebarDrawer>& InDrawerWidget)
{
	RemoveDrawer(InDrawerWidget->GetDrawer().ToSharedRef());
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
	TArray<TSharedRef<SSidebarDrawer>> OpenedDrawersCopy = OpenedDrawers;

	for (TSharedRef<SSidebarDrawer>& Drawer : OpenedDrawersCopy)
	{
		RemoveDrawer(Drawer->GetDrawer().ToSharedRef());
	}
}

EActiveTimerReturnType SSidebar::OnOpenPendingDrawerTimer(const double CurrentTime, const float DeltaTime)
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

void SSidebar::OpenDrawerNextFrame(const TSharedRef<FSidebarDrawer>& InDrawer, const bool bAnimateOpen)
{
	PendingTabToOpen = InDrawer;
	bAnimatePendingTabOpen = bAnimateOpen;

	if (!OpenPendingDrawerTimerHandle.IsValid())
	{
		OpenPendingDrawerTimerHandle = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SSidebar::OnOpenPendingDrawerTimer));
	}
}

void SSidebar::OpenDrawerInternal(const TSharedRef<FSidebarDrawer>& InDrawer, const bool bAnimateOpen)
{
	if (FindOpenedDrawer(InDrawer))
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

	InDrawer->DrawerWidget =
		SNew(SSidebarDrawer, InDrawer, TabLocation)
		.MinDrawerSize(MinDrawerSize)
		.TargetDrawerSize(TargetDrawerSize)
		.MaxDrawerSize(MaxDrawerSize)
		.OnDrawerFocusLost(this, &SSidebar::OnTabDrawerFocusLost)
		.OnDrawerClosed(this, &SSidebar::OnTabDrawerClosed)
		.OnDrawerTargetSizeChanged(this, &SSidebar::OnDrawerTargetSizeChanged);

	DrawersOverlay->AddSlot()
		.Padding(SlotPadding)
		.HAlign(TabLocation == ESidebarTabLocation::Left ? HAlign_Left : HAlign_Right)
		[
			InDrawer->DrawerWidget.ToSharedRef()
		];

	InDrawer->DrawerWidget->Open(bAnimateOpen);

	OpenedDrawers.Add(InDrawer->DrawerWidget.ToSharedRef());

	for (const TSharedRef<FSidebarDrawer>& DrawerTab : DrawerTabs)
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

void SSidebar::CloseDrawerInternal(const TSharedRef<FSidebarDrawer>& InDrawer)
{
	if (const TSharedPtr<SSidebarDrawer> OpenedDrawer = FindOpenedDrawer(InDrawer))
	{
		OpenedDrawer->Close();
		const TSharedRef<SSidebarDrawer> OpenedDrawerRef = OpenedDrawer.ToSharedRef();

		if (const TSharedPtr<SOverlay> DrawersOverlay = DrawersOverlayWeak.Pin())
		{
			DrawersOverlay->RemoveSlot(OpenedDrawerRef);
		}

		OpenedDrawers.Remove(OpenedDrawerRef);
	}

	InDrawer->bIsOpen = false;

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
		OpenDrawerInternal(PinnedTab.ToSharedRef(), /*bAnimateOpen=*/true);
	}
}

void SSidebar::UpdateDrawerAppearance()
{
	TSharedPtr<FSidebarDrawer> OpenedTab;
	if (OpenedDrawers.Num() > 0)
	{
		OpenedTab = OpenedDrawers.Last()->GetDrawer();
	}

	for (const TSharedRef<FSidebarDrawer>& Drawer : DrawerTabs)
	{
		const TSharedPtr<SSidebarButton> TabButton = StaticCastSharedPtr<SSidebarButton>(Drawer->ButtonWidget);
		if (TabButton.IsValid())
		{
			TabButton->UpdateAppearance(OpenedTab);
		}
	}
}

TSharedPtr<FSidebarDrawer> SSidebar::FindDrawer(const FName InDrawerId) const
{
	const TSharedRef<FSidebarDrawer>* const FoundDrawer = DrawerTabs.FindByPredicate(
		[InDrawerId](const TSharedRef<FSidebarDrawer>& InDrawer)
		{
			return InDrawerId == InDrawer->GetUniqueId();
		});
	return FoundDrawer ? *FoundDrawer : TSharedPtr<FSidebarDrawer>();
}

TSharedPtr<FSidebarDrawer> SSidebar::FindFirstPinnedTab() const
{
	for (const TSharedRef<FSidebarDrawer>& DrawerTab : DrawerTabs)
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
	const int32 Index = OpenedDrawers.FindLastByPredicate(
		[](const TSharedRef<SSidebarDrawer>& Drawer)
		{
			return Drawer->IsOpen() && !Drawer->IsClosing();
		});
	return Index == INDEX_NONE ? nullptr : OpenedDrawers[Index]->GetDrawer();
}

TSharedPtr<SSidebarDrawer> SSidebar::FindOpenedDrawer(const TSharedRef<FSidebarDrawer>& InDrawer) const
{
	const TSharedRef<SSidebarDrawer>* OpenedDrawer = OpenedDrawers.FindByPredicate(
		[&InDrawer](const TSharedRef<SSidebarDrawer>& Drawer)
		{
			return InDrawer == Drawer->GetDrawer();
		});
	return OpenedDrawer ? TSharedPtr<SSidebarDrawer>(*OpenedDrawer) : nullptr;
}

bool SSidebar::HasDrawerOpened() const
{
	for (const TSharedRef<FSidebarDrawer>& DrawerTab : DrawerTabs)
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
	for (const TSharedRef<FSidebarDrawer>& DrawerTab : DrawerTabs)
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
	if (OpenedDrawers.IsEmpty())
	{
		return NAME_None;
	}
	
	const TSharedRef<SSidebarDrawer> LastOpenedDrawer = OpenedDrawers.Last();
	return LastOpenedDrawer->GetDrawer()->GetUniqueId();
}

bool SSidebar::HasDrawerPinned() const
{
	for (const TSharedRef<FSidebarDrawer>& DrawerTab : DrawerTabs)
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
	const TSharedPtr<FSidebarDrawer> Drawer = FindDrawer(InDrawerId);
	if (!Drawer.IsValid() || Drawer->bIsPinned == bInIsPinned)
	{
		return;
	}

	if (bInIsPinned)
	{
		if (Drawer->bIsDocked)
		{
			SetDrawerDocked(InDrawerId, false);
		}

		if (!Drawer->bIsOpen)
		{
			OpenDrawerInternal(Drawer.ToSharedRef(), false);
		}
		if (!Drawer->bIsOpen)
		{
			return;
		}
	}

	Drawer->bIsPinned = bInIsPinned;

	if (bInIsPinned)
	{
		PinnedDrawerTabs.AddUnique(Drawer.ToSharedRef());
	}
	else
	{
		PinnedDrawerTabs.Remove(Drawer.ToSharedRef());
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

	const TSharedPtr<FSidebarDrawer> Drawer = FindDrawer(InDrawerId);
	if (!Drawer.IsValid() || Drawer->bIsDocked == bInIsDocked)
	{
		return;
	}

	if (bInIsDocked)
	{
		if (Drawer->bIsPinned)
		{
			SetDrawerPinned(InDrawerId, false);
		}

		CloseAllDrawers();

		if (DockedDrawerTab.IsValid())
		{
			UndockAllDrawers();
		}

		DockedDrawerTab = Drawer;

		DockedDrawerTab->bIsPinned = false;
		DockedDrawerTab->bIsDocked = true;

		if (Drawer->ContentWidget.IsValid())
		{
			DockLocation->SetContent(Drawer->ContentWidget.ToSharedRef());
			
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
	for (const TSharedRef<FSidebarDrawer>& DrawerTab : DrawerTabs)
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
