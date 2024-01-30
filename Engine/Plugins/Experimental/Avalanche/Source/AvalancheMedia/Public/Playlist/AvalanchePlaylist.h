// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvalanchePage.h"
#include "AvaMediaDefines.h"
#include "UObject/Object.h"
#include "AvalanchePagePlayer.h"
#include "AvalanchePlaylist.generated.h"

AVALANCHEMEDIA_API DECLARE_LOG_CATEGORY_EXTERN(LogAvaPlaylist, Log, All);

class FAvaMediaPlaybackManager;
class FAvaPlaylistPlaybackClientWatcher;
class FAvalanchePageTransitionBuilder;
class UAvalanchePageTransition;
class UAvalanchePlayback;
class UTextureRenderTarget2D;

/**
 * @brief Defines the insertion position in a page list.
 */
struct FAvaPageInsertPosition
{
	/**
	 * The position is defined with a page id because it is coupled with the id generation.
	 * For the id generator, if an insertion position is defined, we also want the generated
	 * ids to be in relation to that.
	 */
	int32 AdjacentId;
		
	/** Defines the insertion position relative to the reference page i.e. above or below the adjacent page. */
	bool bAddBelow;

	explicit FAvaPageInsertPosition(int32 InAdjacentId = FAvalanchePage::InvalidPageId, bool bInAddBelow = true) : AdjacentId(InAdjacentId), bAddBelow(bInAddBelow) {} 

	bool IsValid() const { return AdjacentId != FAvalanchePage::InvalidPageId; }

	bool IsAddAbove() const { return !bAddBelow;}
	
	bool IsAddBelow() const { return bAddBelow;}

	/** Update the id only if it was initially valid. */
	void ConditionalUpdateAdjacentId(int32 InNewAdjacentId)
	{
		if (IsValid())
		{
			AdjacentId = InNewAdjacentId;
		}
	}
};

/**
 * Defines the parameters for the page id generator algorithm.
 * The Id generator uses a sequence strategy to search for an unused id.
 * It is defined by a starting id and a search direction.
 */
struct FAvaPageIdGeneratorParams
{
	/** Starting Id for the search. */
	int32 ReferenceId;
		
	/**
	 * @brief (Initial) Search increment.
	 * @remark For negative increment search, the limit of the search space can be reached. If no unique id is found,
	 *		   the search will continue in the positive direction instead.		   
	 */
	int32 Increment;
		
	explicit FAvaPageIdGeneratorParams(int32 InReferenceId = FAvalanchePage::InvalidPageId, int32 InIncrement = 1)
		: ReferenceId(InReferenceId), Increment(InIncrement) {}

	/** Operation helper: Determines id generation from the insert parameters. */
	static FAvaPageIdGeneratorParams FromInsertPosition(const FAvaPageInsertPosition& InInsertPosition)
	{
		// When used with insertion in a page list, if the element is added above, we will first try to generate
		// the id in decreasing order.
		return FAvaPageIdGeneratorParams( InInsertPosition.AdjacentId, InInsertPosition.bAddBelow ? 1 : -1);
	}

	/** Operation helper: Id generation prefers using insertion parameters (if specified) over source id. */
	static FAvaPageIdGeneratorParams FromInsertPositionOrSourceId(int32 InSourceId, const FAvaPageInsertPosition& InInsertPosition)
	{
		return InInsertPosition.IsValid() ? FromInsertPosition(InInsertPosition) : FAvaPageIdGeneratorParams(InSourceId);
	}
};

struct FAvaPageListChangeParams
{
	UAvalanchePlaylist* Playlist;
	EAvaPageListChange ChangeType;
	TArray<int32> AffectedPages;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnAvaPageListChanged, const FAvaPageListChangeParams&)
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnAvaPagesChanged, const UAvalanchePlaylist*, const FAvalanchePage&, EAvaPageChanges)

USTRUCT()
struct FAvalanchePageCollection
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FAvalanchePage> Pages;

	/** Cache mapping the Page Id to the index where the Page with such Page Id is at */
	TMap<int32, int32> PageIndices;

	FOnAvaPageListChanged OnPageListChanged;

	int32 GetPageIndex(const int32 InPageId) const
	{
		if (InPageId != FAvalanchePage::InvalidPageId)
		{
			const int32* PageIndex = PageIndices.Find(InPageId);
			return PageIndex ? *PageIndex : INDEX_NONE;
		}
		return INDEX_NONE;
	}

	AVALANCHEMEDIA_API void Empty(UAvalanchePlaylist* InPlaylist);

	/** Complete refresh of the page indices. */
	void RefreshPageIndices()
	{
		PageIndices.Empty(Pages.Num());
		for (int32 Index = 0; Index < Pages.Num(); ++Index)
		{
			PageIndices.Add(Pages[Index].GetPageId(), Index);
		}
	}

	/** Refresh page indices after a new page has been inserted at the given index. */
	void PostInsertRefreshPageIndices(const int32 InStartAtIndex)
	{
		for (int32 Index = InStartAtIndex; Index < Pages.Num(); ++Index)
		{
			PageIndices.Add(Pages[Index].GetPageId(), Index);
		}
	}
};

USTRUCT()
struct FAvalancheSubList
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<int32> PageIds;

	UPROPERTY()
	FText Name;

	FOnAvaPageListChanged OnPageListChanged;
};

namespace UE::AvalanchePlaylist
{
	inline bool IsPreviewPlayType(EAvaPlayType InPlayType)
	{
		return InPlayType == EAvaPlayType::PreviewFromStart || InPlayType == EAvaPlayType::PreviewFromFrame;
	}
}

/**
 * Carries the context for the playback of a page list.
 * In particular, what is the current play head to be able to move to
 * the next page in the page list.
 * Note: a "playlist" has many page lists and each page lists has 2 play type (preview or program),
 * therefore, we have many instances of this page list context for each page lists and play type within
 * a "playlist" object.
 */
struct FAvaPageListPlaybackContext
{
	/** Keeps track of the page id the play head is on, i.e. this is the last played page. */
	int32 PlayHeadPageId = FAvalanchePage::InvalidPageId;
};

/**
 * This class is a container for all the page list contexts are playlist can have.
 * The current implementation only keeps track of the play type, i.e. preview vs program.
 * The design for this is not settle yet. The requirement for a page list context per preview channel comes
 * from the playlist server as it may have a preview channel dedicated per client connection, which implies a page list
 * context for each one of them. There is only one "program" page list context for now.
 */
struct FAvaPageListPlaybackContextCollection
{
	TSharedPtr<FAvaPageListPlaybackContext> GetContext(bool bInIsPreview, const FName& InPreviewChannelName) const
	{
		if (bInIsPreview)
		{
			const TSharedPtr<FAvaPageListPlaybackContext>* FoundContext = PreviewContexts.Find(InPreviewChannelName);
			return FoundContext ? *FoundContext : TSharedPtr<FAvaPageListPlaybackContext>();
		}
		return ProgramContext;
	}

	FAvaPageListPlaybackContext& GetOrCreateContext(bool bInIsPreview, const FName& InPreviewChannelName)
	{
		const TSharedPtr<FAvaPageListPlaybackContext> ExistingContext = GetContext(bInIsPreview, InPreviewChannelName);
		return ExistingContext ? *ExistingContext : *CreateContext(bInIsPreview, InPreviewChannelName);
	}

protected:
	TSharedPtr<FAvaPageListPlaybackContext> CreateContext(bool bInIsPreview, const FName& InPreviewChannelName)
	{
		if (bInIsPreview)
		{
			TSharedPtr<FAvaPageListPlaybackContext> NewPreviewContext = MakeShared<FAvaPageListPlaybackContext>(); 
			PreviewContexts.Add(InPreviewChannelName, NewPreviewContext);
			return NewPreviewContext;
		}
		ProgramContext = MakeShared<FAvaPageListPlaybackContext>();
		return ProgramContext;
	}

	TSharedPtr<FAvaPageListPlaybackContext> ProgramContext;
	TMap<FName, TSharedPtr<FAvaPageListPlaybackContext>> PreviewContexts;
};

UENUM()
enum class EAvaPlaylistPageStopOptions : uint8
{
	/**
	 * Default option will stop the page with transitions if available.
	 */
	None				= 0,
	/**
	 * Forces the page to stop without transitions.
	 */
	ForceNoTransition	= 1 << 1,
	/**
	 * Default option will stop the page with transitions if available.
	 */
	Default				= None
};
ENUM_CLASS_FLAGS(EAvaPlaylistPageStopOptions);

/**
 * @brief Manages page pre-loading.
 */
class IAvaPlaylistPageLoadingManager
{
public:
	IAvaPlaylistPageLoadingManager() = default;
	virtual ~IAvaPlaylistPageLoadingManager() = default;

	virtual bool RequestLoadPage(int32 InPageId, bool bInIsPreview, const FName& InPreviewChannelName) = 0;
};

/**
 * This class is a container for what could be described as a "show" for broadcast purposes.
 *
 * It goes beyond a simple playlist of items. It contains the following:
 * - a list of Avalanche Template Pages (or just Templates).
 * - a list of Avalanche Instanced Pages (or just Pages).
 * - a list of page views (or just Views).
 *
 * Workflow:
 *
 * 1- Templates
 * 
 * The first step in the work flow consist in importing templates. The source asset is not actually imported
 * in the "show" container, it is just soft referenced. However, the import process will load and cache some information
 * about the template (exposed properties, default values, animations, transition logic layer, etc).
 * Given that this information is cached, it may become stale if the source asset is updated. Therefore, reimporting
 * the templates may be necessary within the normal work flow.
 * Todo: keep a hash of the source asset to determine if it has changed.
 *
 * 2- Pages
 *
 * The pages are instances of the templates, allowing to change the exposed properties and controllers, also selecting
 * an output program channel for the given page. Only one program channel is allowed per page.
 *
 * 3- Page Views
 *
 * Separate page views can be made in order to create "playlists" for separate segments/parts of a show.
 *
 * "Page Groups" Discussion:
 * "Page Groups" are not implemented. It would be different than page views, i.e.
 * pages could be grouped in either of the page list or page views.
 * Other applications support page grouping to emulate MOS's hierarchy.
 * In the MOS/NCS hierarchies: Rundown -> Stories/Segments -> Parts -> Pieces/Items
 * Although full emulation of MOS schema may not be necessary within the Avalanche playback framework.
 *
 */
UCLASS(NotBlueprintable, BlueprintType)
class AVALANCHEMEDIA_API UAvalanchePlaylist : public UObject
{
	GENERATED_BODY()

public:
	UAvalanchePlaylist();

	virtual ~UAvalanchePlaylist() override;

	static const FAvaPageListReference TemplatePageList;
	static const FAvaPageListReference InstancePageList;

	static FAvaPageListReference CreateSubListReference(int32 InSubListIndex) { return {EAvaPageListType::View, InSubListIndex}; }

protected:
	bool IsPageIdUnique(int32 InPageId) const { return !TemplatePages.PageIndices.Contains(InPageId) && !InstancedPages.PageIndices.Contains(InPageId); }
	int32 GenerateUniquePageId(int32 InReferencePageId = FAvalanchePage::InvalidPageId, int32 InIncrement = 1) const;
	int32 GenerateUniquePageId(const FAvaPageIdGeneratorParams& InParams) const;
	
	/** Caches the Page's Id to its Index in the Pages Array*/
	void RefreshPageIndices();
	
public:
	static const FAvalancheSubList InvalidSubList;

	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif
	//~ End UObject Interface

	/** Returns true if the playlist is empty, i.e. no pages and no templates. */
	bool IsEmpty() const;

	/**
	 * Clear the playlist of all it's content.
	 * @remark Will be prevented if the playlist is playing.
	 */
	bool Empty();

	int32 AddTemplateInternal(const FAvaPageIdGeneratorParams& InIdGeneratorParams, const TFunctionRef<bool(FAvalanchePage&)> InSetupTemplateFunction);
	
	/** Add empty template. */
	int32 AddTemplate(const FAvaPageIdGeneratorParams& InIdGeneratorParams = FAvaPageIdGeneratorParams());

	int32 AddComboTemplate(const TArray<int32>& InTemplateIds, const FAvaPageIdGeneratorParams& InIdGeneratorParams = FAvaPageIdGeneratorParams());

	/**
	 * @brief Add templates from existing source.
	 * @param InSourceTemplates Source template to add.
	 * @return The new template Ids created.
	 *
	 * For the id generation, it will attempt to reuse the source ids, but
	 * in case of collision, new ids are generated with the positive increment
	 * sequence search method.
	 */
	TArray<int32> AddTemplates(const TArray<FAvalanchePage>& InSourceTemplates);

	/**
	 * @brief Create new pages in the page list for teh given template Ids.
	 * @param InTemplateIds Templates to use for each page. A new page is created for each entry in that array.
	 * @return The new page Ids created.
	 */
	TArray<int32> AddPagesFromTemplates(const TArray<int32>& InTemplateIds);

	/**
	 * @brief Creates a new page in the page list using the given template.
	 * @param InTemplateId Reference to the template to use for that page.
	 * @param InIdGeneratorParams Defines how the page id is going to be generated.
	 * @param InInsertAt Specifies the insertion location in the page list (i.e. the index in the page list).
	 * @return Returns the page Id of the created page.
	 */
	int32 AddPageFromTemplate(int32 InTemplateId, const FAvaPageIdGeneratorParams& InIdGeneratorParams = FAvaPageIdGeneratorParams(), const FAvaPageInsertPosition& InInsertAt = FAvaPageInsertPosition());

	bool CanAddPage() const;

	bool CanChangePageOrder() const;
	/** Reorders the pages, swapping the old indices for the new ones. */
	bool ChangePageOrder(const FAvaPageListReference& InPageListReference, const TArray<int32>& InPageIndices);

	bool RemovePage(int32 InPageId);
	bool CanRemovePage(int32 InPageId) const;

	/** Remove all the Pages in the Array. Returns the number of Pages removed.*/
	int32 RemovePages(const TArray<int32>& InPageIds);
	bool CanRemovePages(const TArray<int32>& InPageIds) const;

	bool RenumberPageId(int32 InPageId, int32 InNewPageId);
	bool CanRenumberPageId(int32 InPageId) const;
	bool CanRenumberPageId(int32 InPageId, int32 InNewPageId) const;

	bool SetRemoteControlEntityValue(int32 InPageId, const FGuid& InId, const FAvalancheRemoteControlValue& InValue);
	bool SetRemoteControlControllerValue(int32 InPageId, const FGuid& InId,const FAvalancheRemoteControlValue& InValue);
	EAvaRemoteControlChanges UpdateRemoteControlValues(int32 InPageId, const FAvalancheRemoteControlValues& InRemoteControlValues, bool bInUpdateDefaults);

	void InvalidateManagedInstanceCacheForPages(const TArray<int32>& InPageIds) const;

	void UpdateAvalancheAssetForPages(const TArray<int32>& InPageIds, bool bInReimportPage);
	
	const FAvalanchePage& GetPage(int32 InPageId) const;
	FAvalanchePage& GetPage(int32 InPageId);
	const FAvalanchePageCollection& GetTemplatePages() const { return TemplatePages; }
	const FAvalanchePageCollection& GetInstancedPages() const { return InstancedPages; }
	static const FAvalanchePage& GetPageSafe(const UAvalanchePlaylist* InPlaylist, int32 InPageId)
	{
		return InPlaylist ? InPlaylist->GetPage(InPageId) : FAvalanchePage::NullPage;
	}
	/**
	 * Gets the page following the page with the given page id in the given page list.
	 * If the given page is not in the given list, the returned page is invalid.
	 */
	const FAvalanchePage& GetNextPage(int32 InPageId, const FAvaPageListReference& InPageListReference) const;
	
	/**
	 * Gets the page following the page with the given page id in the given page list.
	 * If the given page is not in the given list, the returned page is invalid.
	 */
	FAvalanchePage& GetNextPage(int32 InPageId, const FAvaPageListReference& InPageListReference);

	/** Gets the page following the page with the given page id. Using current active page list. */
	const FAvalanchePage& GetNextPage(int32 InPageId) const { return GetNextPage(InPageId, ActivePageList);}

	/** Gets the page following the page with the given page id. Using current active page list. */
	FAvalanchePage& GetNextPage(int32 InPageId)  { return GetNextPage(InPageId, ActivePageList);}
	
	FOnAvaPageListChanged& GetOnTemplatePageListChanged() { return TemplatePages.OnPageListChanged; }
	FOnAvaPageListChanged& GetOnInstancedPageListChanged() { return InstancedPages.OnPageListChanged; }
	FOnAvaPagesChanged& GetOnPagesChanged() { return OnPagesChanged; }

	/**
	 * Since the playback context is part of the asset for now, there is an explicit call to initialize it.
	 * This would be done by the editor (or server).
	 * A future refactor will extract the playlist "player" functionality in another class and it will
	 * be possible to create multiple instance of a playlist player for the same playlist.
	 */
	void InitializePlaybackContext();

	/**
	 * Similarly, when the editor is done, it can close the playback context. 
	 * This will clean up the internal structures for playback and optionally stop all the pages.
	 */
	void ClosePlaybackContext(bool bInStopAllPages);

	/**
	 * Returns true if the any page is either playing or previewing.
	 */
	bool IsPlaying() const;

	/** Returns true if the page with the given page Id is being previewed (in any preview channel). */
	bool IsPagePreviewing(int32 InPageId) const;

	/** Returns true if the page is playing in it's assigned program channel. */
	bool IsPagePlaying(int32 InPageId) const;
	
	bool IsPagePlaying(const FAvalanchePage& InPage) const { return IsPagePlaying(InPage.GetPageId()); }

	bool IsPagePlayingOrPreviewing(int32 InPageId) const;

	bool UnloadPage(int32 InPageId, const FString& InChannelName);

	struct FLoadedInstanceInfo
	{
		FGuid InstanceId;
		FSoftObjectPath AssetPath;
	};
	
	/**
	 * @brief Preload the given page so it has an asset ready for playback.
	 * @return UUIDs (and asset paths) of the playback instances that where loaded (or are scheduled for loading).
	 */
	TArray<FLoadedInstanceInfo> LoadPage(int32 InPageId,  bool bInPreview, const FName& InPreviewChannelName);

	/**
	 * @brief Start the playback of the asset defined in the given pages.
	 * @remark If the play type is a preview, the default preview channel is used.
	 * @param InPageIds Playlist's pages to play out.
	 * @param InPlayType Either play on program or preview
	 * @return page Ids that where started.
	 */
	TArray<int32> PlayPages(const TArray<int32>& InPageIds, EAvaPlayType InPlayType);

	/**
	 * @brief Start the playback of the asset defined in the given pages.
	 * @remark If the play type is a preview, the given preview channel is used.
	 * @param InPageIds Playlist's pages to play out.
	 * @param InPlayType Either play on program or preview
	 * @param InPreviewChannelName Channel to use for preview. Only used if play type is preview.
	 * @return page Ids that where started.
	 */
	TArray<int32> PlayPages(const TArray<int32>& InPageIds, EAvaPlayType InPlayType, const FName& InPreviewChannelName);

	bool PlayPage(int32 InPageId, EAvaPlayType InPlayType) { return !PlayPages({InPageId}, InPlayType).IsEmpty(); }
	bool PlayPage(int32 InPageId, EAvaPlayType InPlayType, const FName& InPreviewChannelName) { return !PlayPages({InPageId}, InPlayType, InPreviewChannelName).IsEmpty(); }

	bool CanPlayPage(int32 InPageId, bool bInPreview) const;
	bool CanPlayPage(int32 InPageId, bool bInPreview, const FName& InPreviewChannelName) const;

	TArray<int32> StopPages(const TArray<int32>& InPageIds, EAvaPlaylistPageStopOptions InOptions, bool bInPreview);
	TArray<int32> StopPages(const TArray<int32>& InPageIds, EAvaPlaylistPageStopOptions InOptions, bool bInPreview, const FName& InPreviewChannelName);

	bool StopPage(int32 InPageId, EAvaPlaylistPageStopOptions InOptions, bool bInPreview) { return !StopPages({InPageId}, InOptions, bInPreview).IsEmpty(); }
	bool StopPage(int32 InPageId, EAvaPlaylistPageStopOptions InOptions, bool bInPreview, const FName& InPreviewChannelName) { return !StopPages({InPageId}, InOptions, bInPreview, InPreviewChannelName).IsEmpty(); }
	
	bool CanStopPage(int32 InPageId, EAvaPlaylistPageStopOptions InOptions, bool bInPreview) const;
	bool CanStopPage(int32 InPageId, EAvaPlaylistPageStopOptions InOptions, bool bInPreview, const FName& InPreviewChannelName) const;

	bool StopChannel(const FString& InChannelName);
	bool CanStopChannel(const FString& InChannelName) const;

	bool ContinuePage(int32 InPageId, bool bInPreview);
	bool ContinuePage(int32 InPageId, bool bInPreview, const FName& InPreviewChannelName);

	bool CanContinuePage(int32 InPageId, bool bInPreview) const;
	bool CanContinuePage(int32 InPageId, bool bInPreview, const FName& InPreviewChannelName) const;

	/** Used to reconcile playing state with a remote playback if connection was lost. */
	bool RestorePlaySubPage(int32 InPageId, int32 InSubPageIndex, const FGuid& InExistingInstanceId, bool bInIsPreview, const FName& InPreviewChannelName);

	int32 AddSubList();

	/**
	 * Return the current playing Page Ids on the specified program channel.
	 * If program channel is none, returns all playing pages on all program channels. 
	 */
	TArray<int32> GetPlayingPageIds(const FName InProgramChannelName = NAME_None) const;

	/**
	 * Return the current previewing Page Ids on the specified preview channel.
	 * If preview channel is none, returns all previewing pages on all channels. 
	 */
	TArray<int32> GetPreviewingPageIds(const FName InPreviewChannelName = NAME_None) const;

	const FAvaPageListReference& GetActivePageListReference() const { return ActivePageList; }

	bool SetActivePageList(const FAvaPageListReference& InPageListReference);

	/** Returns true only if a sub list, not the main list, is active. */
	bool HasActiveSubList() const;

	const FAvalancheSubList& GetActiveSubList() const { return GetSubList(ActivePageList.SubListIndex); }
	FAvalancheSubList& GetActiveSubList() { return GetSubList(ActivePageList.SubListIndex); }

	const FAvalancheSubList& GetSubList(int32 InSubListIndex) const;

	FAvalancheSubList& GetSubList(int32 InSubListIndex);

	bool IsValidSubListIndex(int32 InIndex) const { return SubLists.IsValidIndex(InIndex); }
	bool IsValidSubList(const FAvaPageListReference& InPageListReference) const;

	const TArray<FAvalancheSubList>& GetSubLists() const { return SubLists; }

	bool AddPageToSubList(int32 InSubListIndex, int32 InPageId, const FAvaPageInsertPosition& InInsertPosition = FAvaPageInsertPosition());
	bool AddPagesToSubList(int32 InSubListIndex, const TArray<int32>& InPages);

	int32 RemovePagesFromSubList(int32 InSubListIndex, const TArray<int32>& InPages);

	DECLARE_MULTICAST_DELEGATE(FOnActiveListChanged)
	FOnActiveListChanged& GetOnActiveListChanged() { return OnActiveListChanged; }

	UTextureRenderTarget2D* GetPreviewRenderTarget() const { return GetPreviewRenderTarget(GetDefaultPreviewChannelName());}
	UTextureRenderTarget2D* GetPreviewRenderTarget(const FName& InPreviewChannel) const;

	/** Returns the currently selected preview channel (from the settings). */
	static FName GetDefaultPreviewChannelName();

	/** Clean up playing status on a system tear down. */
	void OnParentWordBeginTearDown();

	/**
	 * Push the page's RC values to the runtime playback instances.
	 * @remark If updating preview, will update all preview channels by default. 
	 */
	bool PushRuntimeRemoteControlValues(int32 InPageId, bool bInIsPreview, const FName& InPreviewChannelName = NAME_None) const;
	
	void NotifyPageRemoteControlValueChanged(int32 InPageId, EAvaRemoteControlChanges InRemoteControlChanges);
	
	void NotifyPageStopped(int32 InPageId) const
	{
		OnPagesChanged.Broadcast(this, GetPage(InPageId), EAvaPageChanges::Status);
	}
	
	void NotifyPageSequenceFinished(int32 InPageId)
	{
		OnPagesChanged.Broadcast(this, GetPage(InPageId), EAvaPageChanges::Status);
	}

#if WITH_EDITOR
	void NotifyPIEEnded(const bool);
#endif

	FAvaMediaPlaybackManager& GetPlaybackManager() const;

	/** Access the page loading manager for this playlist. */
	IAvaPlaylistPageLoadingManager& GetPageLoadingManager()
	{
		return PageLoadingManager ? *PageLoadingManager : MakePageLoadingManager();
	}

protected:
	int32 AddPageFromTemplateInternal(int32 InTemplateId, const FAvaPageIdGeneratorParams& InIdGeneratorParams = FAvaPageIdGeneratorParams(), const FAvaPageInsertPosition& InInsertAt = FAvaPageInsertPosition());
	
	void InitializePage(FAvalanchePage& InOutPage, int32 InPageId, int32 InTemplateId) const;

	/**
	 * Returns true if the selected page can play on the given channel.
	 * This enforces that the channel type (program or preview) is compatible with the requested operation.
	 */
	bool IsChannelTypeCompatibleForRequest(const FAvalanchePage& InSelectedPage, bool bInIsPreview, const FName& InPreviewChannelName, bool bInLogFailureReason) const;

private:
	IAvaPlaylistPageLoadingManager& MakePageLoadingManager();

	bool PlayPageNoTransition(const FAvalanchePage& InPage, EAvaPlayType InPlayType, bool bInIsPreview, const FName& InPreviewChannelName);
	bool PlayPageWithTransition(FAvalanchePageTransitionBuilder& InBuilder, const FAvalanchePage& InPage, EAvaPlayType InPlayType, bool bInIsPreview, const FName& InPreviewChannelName);
	bool StopPageNoTransition(const FAvalanchePage& InPage, bool bInPreview, const FName& InPreviewChannelName);
	bool StopPageWithTransition(FAvalanchePageTransitionBuilder& InBuilder, const FAvalanchePage& InPage, bool bInPreview, const FName& InPreviewChannelName);
	
	const FAvalanchePage& GetNextFromPages(const TArray<FAvalanchePage>& InPages, int32 InStartingIndex) const;
	FAvalanchePage& GetNextFromPages(TArray<FAvalanchePage>& InPages, int32 InStartingIndex) const;

	const FAvalanchePage& GetNextFromSubList(const TArray<int32>& InSubListIds, int32 InStartingIndex) const;
	FAvalanchePage& GetNextFromSubList(TArray<int32>& InSubListIds, int32 InStartingIndex);

protected:
	UPROPERTY()
	TArray<FAvalanchePage> Pages_DEPRECATED;

	UPROPERTY()
	FAvalanchePageCollection TemplatePages;

	UPROPERTY()
	FAvalanchePageCollection InstancedPages;

	UPROPERTY()
	TArray<FAvalancheSubList> SubLists;

	/** ==InstancePageList Indicates that the entire list is being played, rather than a specific view. */
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	FAvaPageListReference ActivePageList = InstancePageList;

	/**
	 * Keeping track of playing pages.
	 * 
	 * Note: For the playlist editor, we can only preview one page on the selected preview channel,
	 * however, playlist server will eventually require that more than one page is previewed on different preview channels
	 * for different connected clients.
	 */
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	TArray<TObjectPtr<UAvalanchePagePlayer>> PagePlayers;
	
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	TArray<TObjectPtr<UAvalanchePageTransition>> PageTransitions;

	TUniquePtr<FAvaPageListPlaybackContextCollection> PageListPlaybackContextCollection;

	TUniquePtr<IAvaPlaylistPageLoadingManager> PageLoadingManager;
	
	/**
	 * Playback Client Watcher ensures external playback events are reconciled.
	 */
	friend class FAvaPlaylistPlaybackClientWatcher;
	TPimplPtr<FAvaPlaylistPlaybackClientWatcher> PlaybackClientWatcher;

protected:
	void AddPagePlayer(UAvalanchePagePlayer* InPagePlayer);
	
public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPagePlayerEvent, UAvalanchePlaylist*, UAvalanchePagePlayer*);

	/**
	 * Playlist Player Event - Called when a new page player is added to the playback context.
	 */
	FOnPagePlayerEvent OnPagePlayerAdded;
	
	/**
	 * Playlist Player Event - Called when a stopped page player is about to be removed from the playback context.
	 */
	FOnPagePlayerEvent OnPagePlayerRemoving;

	FOnPagePlayerEvent& GetOnPagePlayerAdded() { return OnPagePlayerAdded; }
	FOnPagePlayerEvent& GetOnPagePlayerRemoving() { return OnPagePlayerRemoving; }
	
	void AddPageTransition(UAvalanchePageTransition* InPageTransition)
	{
		PageTransitions.Add(InPageTransition);
	}

	void RemovePageTransition(UAvalanchePageTransition* InPageTransition)
	{
		PageTransitions.Remove(InPageTransition);
	}

	bool CanStartTransitionForPage(const FAvalanchePage& InPage, bool bInIsPreview, const FName& InPreviewChannelName) const;

	void StopPageTransitionsForPage(const FAvalanchePage& InPage, bool bInIsPreview, const FName& InPreviewChannelName);

	const TArray<TObjectPtr<UAvalanchePagePlayer>>& GetPagePlayers() const { return PagePlayers; }
	
	UAvalanchePagePlayer* FindPlayerForProgramPage(int32 InPageId) const
	{
		const TObjectPtr<UAvalanchePagePlayer>* FoundPlayer = PagePlayers.FindByPredicate([InPageId](const UAvalanchePagePlayer* InPagePlayer)
		{
			return InPagePlayer->PageId == InPageId && !InPagePlayer->bIsPreview;
		});
		return FoundPlayer ? *FoundPlayer : nullptr;
	}

	UAvalanchePagePlayer* FindPlayerForPreviewPage(int32 InPageId, const FName& InPreviewChannelFName) const
	{
		const TObjectPtr<UAvalanchePagePlayer>* FoundPlayer = PagePlayers.FindByPredicate([InPageId, InPreviewChannelFName](const UAvalanchePagePlayer* InPagePlayer)
		{
			return InPagePlayer->PageId == InPageId && InPagePlayer->bIsPreview && InPagePlayer->ChannelFName == InPreviewChannelFName;
		});
		return FoundPlayer ? *FoundPlayer : nullptr;
	}
	
	UAvalanchePagePlayer* FindPlayerForPage(int32 InPageId, bool bInIsPreview, const FName& InPreviewChannelName) const
	{
		return bInIsPreview ?  FindPlayerForPreviewPage(InPageId, InPreviewChannelName) : FindPlayerForProgramPage(InPageId);
	}

	void RemoveStoppedPagePlayers();

	FAvaPageListPlaybackContextCollection* GetPageListPlaybackContextCollection() const
	{
		return PageListPlaybackContextCollection.Get();
	}

	FAvaPageListPlaybackContextCollection& GetOrCreatePageListPlaybackContextCollection()
	{
		if (!PageListPlaybackContextCollection.IsValid())
		{
			PageListPlaybackContextCollection = MakeUnique<FAvaPageListPlaybackContextCollection>();
		}
		check(PageListPlaybackContextCollection.IsValid());
		return *PageListPlaybackContextCollection;
	}

protected:
	FOnAvaPagesChanged OnPagesChanged;
	FOnActiveListChanged OnActiveListChanged;
};
