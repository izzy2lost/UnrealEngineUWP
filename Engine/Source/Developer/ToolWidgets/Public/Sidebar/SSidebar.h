// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SidebarDrawerConfig.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SCompoundWidget.h"

class FExtender;
class FSidebarDrawer;
class ISidebarDrawerContent;
class SBox;
class SOverlay;
class SScrollBox;
class SSidebarButton;
class SSidebarDrawer;
class STabDrawer;
class SVerticalBox;
class UToolMenu;
struct FTabId;

/**
 * The direction that a tab drawer opens relative to the location of the sidebar.
 * NOTE: Effort has been made to support top and bottom sidebar locations, but this has not been thoroughly tested
 *   and ironed out because there is currently no use case.
 */
enum class ESidebarTabLocation : uint8
{
	/** Open from left to right */
	Left,
	/** Open from right to left */
	Right,
	/** Open from bottom to top */
	Top,
	/** Open from top to bottom */
	Bottom
};

DECLARE_DELEGATE_OneParam(FOnSidebarDrawerDockStateChanged, const FName /*InDrawerId*/);

/**
 * Static sidebar tab widget that cannot be dragged or moved to a different location. Multiple drawers can be registered
 * to a single sidebar and each drawer can have its own sections, each of which can be displayed single, in combination,
 * or all together through buttons at the top of the drawer.
 */
class TOOLWIDGETS_API SSidebar : public SCompoundWidget
{
public:
	static constexpr float MinTabButtonSize = 100.f;
	static constexpr float MaxTabButtonSize = 200.f;
	static constexpr float TabButtonThickness = 25.f;

	SLATE_BEGIN_ARGS(SSidebar)
		: _TabLocation(ESidebarTabLocation::Right)
		, _HideWhenDocked(false)
		, _AlwaysUseMaxButtonSize(false)
		, _DisablePin(false)
		, _DisableDock(false)
	{}
		/** The direction that a tab drawer opens relative to the location of the sidebar. */
		SLATE_ARGUMENT(ESidebarTabLocation, TabLocation)
		/** Hides the sidebar when a drawer is docked. NOTE: Must provide a way to manually undock the drawer to restore the sidebar visibility. */
		SLATE_ARGUMENT(bool, HideWhenDocked)
		/** Forces the sidebar tab buttons to always be a uniform size of max. */
		SLATE_ARGUMENT(bool, AlwaysUseMaxButtonSize)
		/** Disables the ability to pin a drawer. */
		SLATE_ARGUMENT(bool, DisablePin)
		/** Disables the ability to dock a drawer. */
		SLATE_ARGUMENT(bool, DisableDock)
		/** Event triggered when a drawers dock state changes. */
		SLATE_EVENT(FOnSidebarDrawerDockStateChanged, OnDockStateChanged)
	SLATE_END_ARGS()

	virtual ~SSidebar() override;

	/**
	 * Constructs the sidebar widget.
	 * 
	 * @param InArgs Widget construction arguments
	 * @param InDrawersOverlay Overlay widget used to display the animating drawer
	 * @param InDockLocation Parent widget that will contain the drawer content widget when docked
	 */
	void Construct(const FArguments& InArgs, const TSharedRef<SOverlay>& InDrawersOverlay, const TSharedRef<SBox>& InDockLocation);

	/**
	 * Registers and displays a new drawer in the sidebar.
	 * 
	 * @param InDrawerConfig Configuration info for the new drawer
	 * 
	 * @return True if the new drawer registration was successful.
	 */
	bool RegisterDrawer(FSidebarDrawerConfig&& InDrawerConfig);

	/**
	 * Unregisters and removes a drawer from the sidebar.
	 *
	 * @param InDrawerId Unique drawer Id to unregister
	 * 
	 * @return True if the drawer removal was successful.
	 */
	bool UnregisterDrawer(const FName InDrawerId);

	/**
	 * Checks if a drawer exists in the sidebar.
	 * 
	 * @param InDrawerId Unique drawer Id to check exists
	 * 
	 * @return True if the sidebar contains a drawer with the specified Id.
	 */
	bool ContainsDrawer(const FName InDrawerId) const;

	/** @return The number of drawers that exist in the sidebar. */
	int32 GetDrawerCount() const;

	/**
	 * Registers and displays a new drawer section in the sidebar.
	 * 
	 * @param InDrawerId Unique drawer Id to register
	 * @param InSection Drawer content interface for the section
	 * 
	 * @return True if the new drawer section registration was successful.
	 */
	bool RegisterDrawerSection(const FName InDrawerId, const TSharedPtr<ISidebarDrawerContent>& InSection);

	/**
	 * Unregisters and removes a drawer section from the sidebar.
	 * 
	 * @param InDrawerId Unique drawer Id that contains the section to unregister
	 * @param InSectionId Unique drawer section Id to unregister
	 * 
	 * @return True if the drawer removal was successful.
	 */
	bool UnregisterDrawerSection(const FName InDrawerId, const FName InSectionId);

	/**
	 * Checks if a drawer section exists within a sidebar drawer.
	 * 
	 * @param InDrawerId Unique Id of the drawer to look for the section in
	 * @param InSectionId Unique drawer section Id to check exists
	 * 
	 * @return True if the sidebar contains a drawer with the specified Id.
	 */
	bool ContainsDrawerSection(const FName InDrawerId, const FName InSectionId) const;

	/**
	 * Attempt to open a specific drawer in the sidebar.
	 *
	 * @param InDrawerId Unique Id of the drawer to attempt to open
	 * 
	 * @return True if the drawer exists in this sidebar and was opened.
	 */
	bool TryOpenDrawer(const FName InDrawerId);

	/** Closes any drawers that are open. */
	void CloseAllDrawers(const bool bInAnimate = true);

	/** @return True if the sidebar has any drawer that is opened. */
	bool HasDrawerOpened() const;

	/**
	 * Checks if a drawer is opened.
	 * 
	 * @param InDrawerId Unique Id of the drawer to check
	 * 
	 * @return True if the specified drawer is currently opened.
	 */
	bool IsDrawerOpened(const FName InDrawerId) const;

	/** @return The unique drawer Id that is currently open. */
	FName GetOpenedDrawerId() const;

	/** @return True if the sidebar has any drawer that is pinned. */
	bool HasDrawerPinned() const;

	/**
	 * Checks if a drawer is pinned.
	 * 
	 * @param InDrawerId Unique Id of the drawer to check
	 * 
	 * @return True if the specified drawer is currently pinned.
	 */
	bool IsDrawerPinned(const FName InDrawerId) const;

	/**
	 * Pins a drawer so it stays open even when focus is lost.
	 * 
	 * @param InDrawerId Unique Id of the drawer to pin
	 * @param bInIsPinned New pin state to set
	 */
	void SetDrawerPinned(const FName InDrawerId, const bool bInIsPinned);

	/** @return True if the sidebar has any drawer that is docked. */
	bool HasDrawerDocked() const;

	/** @return True if the specified drawer Id is docked. */
	bool IsDrawerDocked(const FName InDrawerId) const;

	/**
	 * Docks a drawer so it embeds itself into the content.
	 * 
	 * @param InDrawerId Unique Id of the drawer to dock
	 * @param bInIsDocked New dock state to set
	 */
	void SetDrawerDocked(const FName InDrawerId, const bool bInIsDocked);

	/** Undocks any drawers that are docked. */
	void UndockAllDrawers();

	/**
	 * Helper function to update a splitter slot size based on a drawers state.
	 * Sets the slots resize-ability, sizing rule, size value.
	 * 
	 * @param InDrawerId Unique Id of the drawer to update for
	 * @param InSlot The splitter slot to modify the size of
	 * @param bInAutoUndock Automatically undocks the drawer if the size is below the threshold
	 * @param InDefaultDockPercent Default size co-efficient to reset the drawer size to
	 */
	void UpdateDockedSplitterSlot(const FName InDrawerId, SSplitter::FSlot* const InSlot, const bool bInAutoUndock = true
		, const float InDefaultDockPercent = FSidebarDrawerConfig::DefaultSizeCoefficient);

	/** @return True if the sidebar is set to animate horizontally. */
	bool IsHorizontal() const;

	/** @return True if the sidebar is set to animate vertically. */
	bool IsVertical() const;

private:
	void OnTabDrawerButtonPressed(const TSharedRef<FSidebarDrawer>& InDrawer);
	void OnDrawerTabPinToggled(const TSharedRef<FSidebarDrawer>& InDrawer, const bool bIsPinned);
	void OnDrawerTabDockToggled(const TSharedRef<FSidebarDrawer>& InDrawer, const bool bIsDocked);
	void OnTabDrawerFocusLost(const TSharedRef<SSidebarDrawer>& InDrawerWidget);
	void OnOpenAnimationFinish(const TSharedRef<SSidebarDrawer>& InDrawerWidget);
	void OnCloseAnimationFinish(const TSharedRef<SSidebarDrawer>& InDrawerWidget);
	void OnDrawerTargetSizeChanged(const TSharedRef<SSidebarDrawer>& InDrawerWidget, const float InNewSize);
	TSharedRef<SWidget> OnGetTabDrawerContextMenuWidget(TSharedRef<FSidebarDrawer> InDrawer);
	void BuildOptionsMenu(UToolMenu* const InMenu);

	/** Removes a single drawer for a specified tab from this sidebar. Removal is done instantly not waiting for any close animation. */
	void RemoveDrawer(const TSharedRef<FSidebarDrawer>& InDrawer);

	/** Removes all drawers instantly (including drawers for pinned tabs). */
	void RemoveAllDrawers();

	EActiveTimerReturnType OnOpenPendingDrawerTimer(const double InCurrentTime, const float InDeltaTime);
	void OpenDrawerNextFrame(const TSharedRef<FSidebarDrawer>& InDrawer, const bool bInAnimate = true);
	void OpenDrawerInternal(const TSharedRef<FSidebarDrawer>& InDrawer, const bool bInAnimate = true);
	void CloseDrawerInternal(const TSharedRef<FSidebarDrawer>& InDrawer, const bool bInAnimate = true);

	/** Reopens the pinned tab only if there are no other open drawers. This should be used to bring pinned tabs back after other tabs lose focus/are closed. */
	void SummonPinnedTabIfNothingOpened();

	/** Updates the appearance of open drawers. */
	void UpdateDrawerAppearance();

	TSharedPtr<FSidebarDrawer> FindDrawer(const FName InDrawerId) const;

	/** Returns the first tab in this sidebar that is marked pinned. */
	TSharedPtr<FSidebarDrawer> FindFirstPinnedTab() const;

	/** Returns the tab for the last-opened drawer that is still open, excluding any drawers that are in the process of closing. */
	TSharedPtr<FSidebarDrawer> GetForegroundTab() const;

	/** Returns the drawer for the given tab if it's open. */
	TSharedPtr<SSidebarDrawer> FindOpenDrawerWidget(const TSharedRef<FSidebarDrawer>& InDrawer) const;

	TWeakPtr<SOverlay> DrawersOverlayWeak;
	TWeakPtr<SBox> DockLocationWeak;

	ESidebarTabLocation TabLocation = ESidebarTabLocation::Right;
	bool bHideWhenDocked = false;
	bool bAlwaysUseMaxButtonSize = false;
	bool bDisablePin = false;
	bool bDisableDock = false;
	FOnSidebarDrawerDockStateChanged OnDockStateChanged;

	TSharedPtr<SScrollBox> TabButtonContainer;

	TArray<TSharedRef<FSidebarDrawer>> Drawers;

	/** Generally speaking one drawer is only ever open at once but we animate any previous drawer
	 * closing so there could be more than one while an animation is playing. A docked drawer is
	 * also considered open, along with any user opened/pinned drawers. */
	TArray<TSharedRef<SSidebarDrawer>> OpenDrawerWidgets;

	TArray<TSharedRef<SSidebarDrawer>> ClosingDrawerWidgets;

	TWeakPtr<FSidebarDrawer> PendingTabToOpen;
	bool bAnimatePendingTabOpen = false;
	TSharedPtr<FActiveTimerHandle> OpenPendingDrawerTimerHandle;

	TArray<TSharedRef<FSidebarDrawer>> PinnedDrawerTabs;

	TSharedPtr<FSidebarDrawer> DockedDrawerTab;
};
