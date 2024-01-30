// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playlist/AvalanchePagePlayer.h"

#include "AvalancheMediaSettings.h"
#include "Playback/AvaMediaPlaybackManager.h"
#include "Playback/AvalanchePlayable.h"
#include "Playback/AvaMediaPlaybackUtils.h"
#include "Playlist/AvalanchePlaylist.h"

namespace UE::AvaMedia::Rundown::PagePlayer::Private
{
	static void PushAllAnimations(UAvalanchePlayback* InPlaybackObject, const UAvalanchePlaylist* InPlaylist, const FAvalanchePage& InPage
		, const FString& InChannelName, EAvaMediaAnimAction InAnimAction)
	{
		const int32 NumTemplates = InPage.GetNumTemplates(InPlaylist);
		for (int32 TemplateIndex = 0; TemplateIndex < NumTemplates; ++TemplateIndex)
		{
			// Note: default FAnimPlaySettings (with NAME_None) will play all animations.
			InPlaybackObject->PushAnimationCommand(InPage.GetAvalancheAssetPath(InPlaylist, TemplateIndex), InChannelName,
				InAnimAction, FAnimPlaySettings());
		}
	}

	static void PushCameraCut(UAvalanchePlayback* InPlaybackObject, const UAvalanchePlaylist* InPlaylist, const FAvalanchePage& InPage
		, const FString& InChannelName)
	{
		InPlaybackObject->PushAnimationCommand(InPage.GetAvalancheAssetPath(InPlaylist), InChannelName,
			EAvaMediaAnimAction::CameraCut, FAnimPlaySettings());
	}
}

UAvaRundownPlaybackInstancePlayer::UAvaRundownPlaybackInstancePlayer() = default;
UAvaRundownPlaybackInstancePlayer::~UAvaRundownPlaybackInstancePlayer() = default;

bool UAvaRundownPlaybackInstancePlayer::Load(const UAvalanchePagePlayer& InPagePlayer, const UAvalanchePlaylist* InPlaylist, const FAvalanchePage& InPage, int32 InSubPageIndex, const FGuid& InInstanceId)
{
	SourceAssetPath = InPage.GetAvalancheAssetPath(InPlaylist, InSubPageIndex);
	TransitionLayer = InPage.GetTransitionLayer(InPlaylist, InSubPageIndex);
	
	FAvaMediaPlaybackManager& Manager = InPlaylist->GetPlaybackManager();
	PlaybackInstance = Manager.AcquireOrLoadPlaybackInstance(SourceAssetPath, InPagePlayer.ChannelName);
	Playback = PlaybackInstance ? PlaybackInstance->GetPlayback() : nullptr;

	// If restoring from a remote instance.
	if (InInstanceId.IsValid())
	{
		PlaybackInstance->SetInstanceId(InInstanceId);
	}

	// Setup user instance data to be able to track this page.
	if (PlaybackInstance)
	{
		UAvalanchePagePlayer::SetInstanceUserDataFromPage(*PlaybackInstance, InPage);
	}

	if (Playback && InPagePlayer.bIsPreview)
	{
		Playback->SetPreviewChannelName(InPagePlayer.ChannelFName);
	}

	return IsLoaded();
}

bool UAvaRundownPlaybackInstancePlayer::IsLoaded() const
{
	return Playback != nullptr;
}

void UAvaRundownPlaybackInstancePlayer::Play(const UAvalanchePagePlayer& InPagePlayer, const UAvalanchePlaylist* InPlaylist, EAvaPlayType InPlayType, bool bInIsUsingTransitionLogic)
{
	using namespace UE::AvaMedia::Rundown::PagePlayer::Private;
	
	if (!Playback || !InPlaylist)
	{
		return;
	}

	const bool bPlaybackObjectWasPlaying = Playback->IsPlaying();
	
	if (!Playback->IsPlaying())
	{
		Playback->Play();
	}

	const FAvalanchePage& Page = InPlaylist->GetPage(InPagePlayer.PageId);

	// When not using transition logic, we start all animations manually.
	if (!bInIsUsingTransitionLogic)
	{
		Playback->PushRemoteControlValues(SourceAssetPath, InPagePlayer.ChannelName, MakeShared<FAvalancheRemoteControlValues>(Page.GetRemoteControlValues()));

		// TODO: play type should be transmitted to the transition behavior.
		const EAvaMediaAnimAction AnimAction = (InPlayType == EAvaPlayType::PreviewFromFrame)
			? EAvaMediaAnimAction::PreviewFrame
			: EAvaMediaAnimAction::Play;
	
		// Play all the animations with the page's animation settings.
		PushAllAnimations(Playback, InPlaylist, Page, InPagePlayer.ChannelName, AnimAction);
	}
	
	if (bPlaybackObjectWasPlaying)
	{
		PushCameraCut(Playback, InPlaylist, Page, InPagePlayer.ChannelName);
	}
}

bool UAvaRundownPlaybackInstancePlayer::IsPlaying() const
{
	return Playback != nullptr && Playback->IsPlaying();
}

bool UAvaRundownPlaybackInstancePlayer::Continue(const FString& InChannelName)
{
	if (Playback && Playback->IsPlaying())
	{
		// Animation command, within this playback, needs channel for now.
		const FAnimPlaySettings AnimSettings;	// Note: Leaving the name to None means the action apply to all animations.
		Playback->PushAnimationCommand(SourceAssetPath, InChannelName, EAvaMediaAnimAction::Continue, AnimSettings);
		return true;
	}
	return false;
}

bool UAvaRundownPlaybackInstancePlayer::Stop()
{
	if (!Playback)
	{
		return false;
	}
	
	const bool bUnload = !UAvalancheMediaSettings::Get().bKeepPagesLoaded;
	
	if (Playback->IsPlaying())
	{
		// Propagate the unload options in case this object is playing remote.
		const EAvaPlaybackStopOptions PlaybackStopOptions = bUnload ?
			EAvaPlaybackStopOptions::Default | EAvaPlaybackStopOptions::Unload : EAvaPlaybackStopOptions::Default;
		Playback->Stop(PlaybackStopOptions);
	}

	if (PlaybackInstance)
	{
		// Unload the local object as well.
		if (bUnload)
		{
			PlaybackInstance->Unload();
		}
		else
		{
			PlaybackInstance->Recycle();
		}
	}

	Playback = nullptr;
	PlaybackInstance.Reset();
	return true;
}

bool UAvaRundownPlaybackInstancePlayer::HasPlayable(const UAvalanchePlayable* InPlayable) const
{
	return Playback && Playback->HasPlayable(InPlayable);
}

UAvalanchePlayable* UAvaRundownPlaybackInstancePlayer::GetFirstPlayable() const
{
	return Playback ? Playback->GetFirstPlayable() : nullptr;
}

UAvalanchePagePlayer* UAvaRundownPlaybackInstancePlayer::GetPagePlayer() const
{
	return Cast<UAvalanchePagePlayer>(GetOuter());
}

void UAvaRundownPlaybackInstancePlayer::SetPagePlayer(UAvalanchePagePlayer* InPagePlayer)
{
	LowLevelRename(GetFName(), InPagePlayer);
}

UAvalanchePagePlayer::UAvalanchePagePlayer()
{
	UAvalanchePlayable::OnSequenceEvent().AddUObject(this, &UAvalanchePagePlayer::HandleOnPlayableSequenceEvent);
}

UAvalanchePagePlayer::~UAvalanchePagePlayer()
{
	UAvalanchePlayable::OnSequenceEvent().RemoveAll(this);
}

bool UAvalanchePagePlayer::Initialize(UAvalanchePlaylist* InPlaylist, const FAvalanchePage& InPage, bool bInIsPreview, const FName& InPreviewChannel)
{
	if (!InPlaylist)
	{
		UE_LOG(LogAvaPlaylist, Error, TEXT("UAvalanchePagePlayer::Initialize: Invalid playlist."));
		return false;
	}

	if (!InPage.IsValidPage())
	{
		UE_LOG(LogAvaPlaylist, Error, TEXT("UAvalanchePagePlayer::Initialize: Invalid page."));
		return false;
	}
	
	checkf(InstancePlayers.IsEmpty(), TEXT("Can't initialize a page player if already loaded or playing."));

	PlaylistWeak = InPlaylist;
	PageId = InPage.GetPageId();
	bIsPreview = bInIsPreview;
	ChannelFName = bIsPreview ? InPreviewChannel : InPage.GetChannelName();
	ChannelName = ChannelFName.ToString();
	return true;
}

bool UAvalanchePagePlayer::InitializeAndLoad(UAvalanchePlaylist* InPlaylist, const FAvalanchePage& InPage, bool bInIsPreview, const FName& InPreviewChannel)
{
	if (!Initialize(InPlaylist, InPage, bInIsPreview, InPreviewChannel))
	{
		return false;
	}
	
	const int32 NumTemplates = InPage.GetNumTemplates(InPlaylist);
	for (int32 SubPageIndex = 0; SubPageIndex < NumTemplates; ++SubPageIndex)
	{
		CreateAndLoadInstancePlayer(InPlaylist, InPage, SubPageIndex, FGuid());
	}
	return InstancePlayers.Num() > 0;
}

UAvaRundownPlaybackInstancePlayer* UAvalanchePagePlayer::LoadInstancePlayer(int32 InSubPageIndex, const FGuid& InInstanceId)
{
	const UAvalanchePlaylist* Playlist = PlaylistWeak.Get();
	if (!Playlist)
	{
		UE_LOG(LogAvaPlaylist, Error, TEXT("UAvalanchePagePlayer::LoadSubPage: Playlist is no longuer valid."));
		return nullptr;
	}

	const FAvalanchePage& Page = Playlist->GetPage(PageId);
	if (!Page.IsValidPage())
	{
		UE_LOG(LogAvaPlaylist, Error, TEXT("UAvalanchePagePlayer::LoadSubPage: Invalid pageId %d."), PageId);
		return nullptr;
	}

	return CreateAndLoadInstancePlayer(Playlist, Page, InSubPageIndex, InInstanceId);
}

void UAvalanchePagePlayer::AddInstancePlayer(UAvaRundownPlaybackInstancePlayer* InExistingInstancePlayer)
{
	// Remove from previous player.
	if (UAvalanchePagePlayer* PreviousPagePlayer = InExistingInstancePlayer->GetPagePlayer())
	{
		PreviousPagePlayer->RemoveInstancePlayer(InExistingInstancePlayer);
	}

	InstancePlayers.Add(InExistingInstancePlayer);
	InExistingInstancePlayer->SetPagePlayer(this);
}

bool UAvalanchePagePlayer::IsLoaded() const
{
	for (const TObjectPtr<UAvaRundownPlaybackInstancePlayer>& InstancePlayer : InstancePlayers)
	{
		if (InstancePlayer && InstancePlayer->IsLoaded())
		{
			return true;
		}
	}
	return false;
}

bool UAvalanchePagePlayer::Play(EAvaPlayType InPlayType, bool bInIsUsingTransitionLogic)
{
	const UAvalanchePlaylist* Playlist = PlaylistWeak.Get();
	if (!Playlist)
	{
		return false;
	}

	bool bIsPlaying = false;
	for (const TObjectPtr<UAvaRundownPlaybackInstancePlayer>& InstancePlayer : InstancePlayers)
	{
		InstancePlayer->Play(*this, Playlist, InPlayType, bInIsUsingTransitionLogic);
		bIsPlaying |= InstancePlayer->IsPlaying();
	}

	return bIsPlaying;
}

bool UAvalanchePagePlayer::IsPlaying() const
{
	for (const TObjectPtr<UAvaRundownPlaybackInstancePlayer>& InstancePlayer : InstancePlayers)
	{
		if (InstancePlayer && InstancePlayer->IsPlaying())
		{
			return true;
		}
	}
	return false;
}

bool UAvalanchePagePlayer::Continue()
{
	bool bSuccess = false;
	for (const TObjectPtr<UAvaRundownPlaybackInstancePlayer>& InstancePlayer : InstancePlayers)
	{
		bSuccess |= InstancePlayer->Continue(ChannelName);
	}
	return bSuccess;
}

bool UAvalanchePagePlayer::Stop()
{
	bool bSuccess = false;
	for (const TObjectPtr<UAvaRundownPlaybackInstancePlayer>& InstancePlayer : InstancePlayers)
	{
		bSuccess |= InstancePlayer->Stop();
	}
	if (PlaylistWeak.IsValid())
	{
		PlaylistWeak->NotifyPageStopped(PageId);
	}
	return bSuccess;
}

int32 UAvalanchePagePlayer::GetPageIdFromInstanceUserData(const FString& InUserData)
{
	FString PageIdString;
	if (FParse::Value(*InUserData, TEXT("PageId="), PageIdString))
	{
		return FCString::Atoi(*PageIdString);
	}
	return FAvalanchePage::InvalidPageId;
}

void UAvalanchePagePlayer::SetInstanceUserDataFromPage(FAvaMediaPlaybackInstance& InPlaybackInstance, const FAvalanchePage& InPage)
{
	InPlaybackInstance.SetInstanceUserData(FString::Printf(TEXT("PageId=%d"), InPage.GetPageId()));
}

bool UAvalanchePagePlayer::HasPlayable(const UAvalanchePlayable* InPlayable) const
{
	for (const TObjectPtr<UAvaRundownPlaybackInstancePlayer>& InstancePlayer : InstancePlayers)
	{
		if (InstancePlayer->HasPlayable(InPlayable))
		{
			return true;
		}
	}
	return false;
}

UAvaRundownPlaybackInstancePlayer* UAvalanchePagePlayer::FindInstancePlayerForPlayable(const UAvalanchePlayable* InPlayable) const
{
	for (const TObjectPtr<UAvaRundownPlaybackInstancePlayer>& InstancePlayer : InstancePlayers)
	{
		if (InstancePlayer && InstancePlayer->HasPlayable(InPlayable))
		{
			return InstancePlayer;
		}
	}
	return nullptr;
}

UAvaRundownPlaybackInstancePlayer* UAvalanchePagePlayer::FindInstancePlayerByInstanceId(const FGuid& InInstanceId) const
{
	for (const TObjectPtr<UAvaRundownPlaybackInstancePlayer>& InstancePlayer : InstancePlayers)
	{
		if (InstancePlayer && InstancePlayer->GetPlaybackInstanceId() == InInstanceId)
		{
			return InstancePlayer;
		}
	}
	return nullptr;
}

UAvaRundownPlaybackInstancePlayer* UAvalanchePagePlayer::FindInstancePlayerByAssetPath(const FSoftObjectPath& InAssetPath) const
{
	for (const TObjectPtr<UAvaRundownPlaybackInstancePlayer>& InstancePlayer : InstancePlayers)
	{
		if (InstancePlayer && InstancePlayer->SourceAssetPath == InAssetPath)
		{
			return InstancePlayer;
		}
	}
	return nullptr;
}

UAvaRundownPlaybackInstancePlayer* UAvalanchePagePlayer::CreateAndLoadInstancePlayer(const UAvalanchePlaylist* InPlaylist, const FAvalanchePage& InPage, int32 InSubPageIndex, const FGuid& InInstanceId)
{
	UAvaRundownPlaybackInstancePlayer* InstancePlayer = NewObject<UAvaRundownPlaybackInstancePlayer>(this);
	if (InstancePlayer->Load(*this, InPlaylist, InPage, InSubPageIndex, InInstanceId))
	{
		InstancePlayers.Add(InstancePlayer);
		return InstancePlayer;
	}

	return nullptr;
}

void UAvalanchePagePlayer::RemoveInstancePlayer(UAvaRundownPlaybackInstancePlayer* InInstancePlayer)
{
	InstancePlayers.Remove(InInstancePlayer);
}

void UAvalanchePagePlayer::HandleOnPlayableSequenceEvent(UAvalanchePlayable* InPlayable, const FName& SequenceName, EAvalanchePlayableSequenceEventType InEventType)
{
	// Check that this is the playable for this page player.
	if (!HasPlayable(InPlayable))
	{
		return;
	}
	
	// Notify the playlist.
	if (PlaylistWeak.IsValid())
	{
		using namespace UE::AvaMediaPlayback::Utils;
		if (InEventType == EAvalanchePlayableSequenceEventType::Started)
		{
			UE_LOG(LogAvaPlaylist, Verbose, TEXT("%s Playlist Page %d: Sequence Started \"%s\"."), *GetBriefFrameInfo(), PageId, *SequenceName.ToString());
		}

		if (InEventType == EAvalanchePlayableSequenceEventType::Finished)
		{
			UE_LOG(LogAvaPlaylist, Verbose, TEXT("%s Playlist Page %d: Sequence Finished \"%s\"."), *GetBriefFrameInfo(), PageId, *SequenceName.ToString());
			PlaylistWeak->NotifyPageSequenceFinished(PageId);
		}
	}
}
