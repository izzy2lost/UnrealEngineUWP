// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playlist/AvalanchePage.h"

#include "AssetRegistry/AssetData.h"
#include "AvaScene.h"
#include "AvaTransitionTree.h"
#include "AvalancheBroadcast.h"
#include "IAvaMediaModule.h"
#include "Playback/AvaMediaPlaybackClient.h"
#include "Playback/AvaMediaPlaybackManager.h"
#include "Playback/AvalancheRemoteControl.h"
#include "Playlist/AvalancheManagedInstanceCache.h"
#include "Playlist/AvalanchePageAssetUtils.h"
#include "Playlist/AvalanchePlaylist.h"
#include "RCVirtualProperty.h"

#define LOCTEXT_NAMESPACE "AvalanchePage"

FAvalanchePage FAvalanchePage::NullPage;
const int32 FAvalanchePage::InvalidPageId = -1;

FAvalanchePage::FAvalanchePage(int32 InPageId, int32 InTemplateId)
	: PageId(InPageId)
	, TemplateId(InTemplateId)
	, PageName(TEXT("New Page"))
{
}

bool FAvalanchePage::IsValidPage() const
{
	return this != &FAvalanchePage::NullPage
		&& PageId != InvalidPageId;
}

void FAvalanchePage::Rename(const FString& InNewName)
{
	PageName = InNewName;
}

void FAvalanchePage::RenameFriendlyName(const FString& InNewName)
{
	FriendlyName = FText::FromString(InNewName);
}

namespace UE::AvalanchePage::Private
{
	// Determines the page status for a given playback status and secondary states.
	FAvalanchePageStatus GetPageStatus(EAvaBroadcastChannelType InType, EAvaMediaPlaybackStatus InPlaybackStatus, bool bInPagePlaying, bool bInAssetNeedSync)
	{
		switch (InPlaybackStatus)
		{
		case EAvaMediaPlaybackStatus::Unknown:
			return {InType,!bInAssetNeedSync ? EAvalanchePageStatus::Unknown : EAvalanchePageStatus::NeedsSync, bInAssetNeedSync};
		case EAvaMediaPlaybackStatus::Missing:			
			return {InType, EAvalanchePageStatus::Missing, false};
		case EAvaMediaPlaybackStatus::Syncing:
			return {InType, EAvalanchePageStatus::Syncing, false};
		case EAvaMediaPlaybackStatus::Available:
			// There is an explicit "needs sync" page status, along with the flag. This is just to make it more explicit.
			return {InType, !bInAssetNeedSync ? EAvalanchePageStatus::Available : EAvalanchePageStatus::NeedsSync, bInAssetNeedSync};
		case EAvaMediaPlaybackStatus::Loading:
			return {InType, EAvalanchePageStatus::Loading, bInAssetNeedSync};
		case EAvaMediaPlaybackStatus::Loaded:
			return {InType, EAvalanchePageStatus::Loaded, bInAssetNeedSync};
		case EAvaMediaPlaybackStatus::Starting:
			return {InType, EAvalanchePageStatus::Loading, bInAssetNeedSync};
		case EAvaMediaPlaybackStatus::Started:
			return {InType, bInPagePlaying ? EAvalanchePageStatus::Playing : EAvalanchePageStatus::Loaded, bInAssetNeedSync};
		case EAvaMediaPlaybackStatus::Stopping:
		case EAvaMediaPlaybackStatus::Unloading:
			return {InType, EAvalanchePageStatus::Available, bInAssetNeedSync};
		case EAvaMediaPlaybackStatus::Error:
		default:
			return {InType, EAvalanchePageStatus::Error, bInAssetNeedSync};
		}
	}

	FAvalanchePageStatus GetProgramPageStatus(EAvaMediaPlaybackStatus InPlaybackStatus, bool bInPagePlaying, bool bInAssetNeedSync)
	{
		return GetPageStatus(EAvaBroadcastChannelType::Program, InPlaybackStatus, bInPagePlaying, bInAssetNeedSync);
	}
	
	EAvaMediaPlaybackStatus GetPlaybackStatus(EAvaMediaPlaybackAssetStatus InAssetStatus)
	{
		switch (InAssetStatus)
		{
		case EAvaMediaPlaybackAssetStatus::Unknown:
			return EAvaMediaPlaybackStatus::Unknown;
		case EAvaMediaPlaybackAssetStatus::Missing:
			return EAvaMediaPlaybackStatus::Missing;
		case EAvaMediaPlaybackAssetStatus::MissingDependencies:
		case EAvaMediaPlaybackAssetStatus::NeedsSync:
		case EAvaMediaPlaybackAssetStatus::Available:
			return EAvaMediaPlaybackStatus::Available;
		default:
			return EAvaMediaPlaybackStatus::Unknown;
		}
	}
}


FText FAvalanchePage::GetPageDescription() const
{
	if (HasPageFriendlyName())
	{
		return FriendlyName;
	}

	if (HasPageSummary())
	{
		return PageSummary;
	}

	return FText::FromString(PageName);
}

bool FAvalanchePage::UpdatePageSummary(const UAvalanchePlaylist* InPlaylist)
{
	const bool bShouldUpdateDescription = !HasPageSummary();
	if (!IsValidPage() || IsTemplate() || !bShouldUpdateDescription)
	{
		return false;
	}

	const TArray<FSoftObjectPath> AvaAssetPaths = GetAvalancheAssetPaths(InPlaylist);
	TArray<const URemoteControlPreset*> Presets;
	Presets.Reserve(AvaAssetPaths.Num());

	FAvalancheManagedInstanceCache& ManagedInstanceCache = IAvaMediaModule::Get().GetManagedInstanceCache();
	
	for (const FSoftObjectPath& AvaAssetPath : AvaAssetPaths)
	{
		if (const TSharedPtr<FAvalancheManagedInstance> ManagedInstance = ManagedInstanceCache.GetOrLoadAvalancheInstance(AvaAssetPath))
		{
			if (const URemoteControlPreset* Preset = ManagedInstance->GetRemoteControlPreset())
			{
				Presets.Add(Preset);
			}
		}
	}
	
	return UpdatePageSummary(Presets, false);
}

bool FAvalanchePage::UpdatePageSummary(const TArray<const URemoteControlPreset*>& InPresets, bool bInIsPresetChanged)
{
	const bool bShouldUpdateDescription = bInIsPresetChanged ? true : !HasPageSummary();
	if (!IsValidPage() || IsTemplate() || InPresets.IsEmpty() || !bShouldUpdateDescription)
	{
		return false;
	}
	
	TSortedMap<int32, FText> OrderedDescription;

	int32 DisplayIndexOffset = 0;
	for (const URemoteControlPreset* Preset : InPresets)
	{
		for (const URCVirtualPropertyBase* VirtualProperty : Preset->GetControllers())
		{
			FString StringValue = VirtualProperty->GetDisplayValueAsString();
			const FText TextToAdd = FText::FromString(StringValue);
			if (!TextToAdd.IsEmptyOrWhitespace())
			{
				OrderedDescription.FindOrAdd(VirtualProperty->DisplayIndex + DisplayIndexOffset) = TextToAdd;
			}
		}
		DisplayIndexOffset += Preset->GetNumControllers();
	}
	
	if (!OrderedDescription.IsEmpty())
	{
		FFormatOrderedArguments InitialDescription;
		InitialDescription.Reserve(OrderedDescription.Num());

		constexpr int32 FinishDescriptionWhenExceedLimit = 50;
		int32 CurrentLenght = 0;
		for (const TPair<int32, FText>& OrderedValue : OrderedDescription)
		{
			InitialDescription.Add(OrderedValue.Value);
			CurrentLenght += OrderedValue.Value.ToString().Len();
			if (CurrentLenght >= FinishDescriptionWhenExceedLimit)
			{
				InitialDescription.Add(LOCTEXT("EndOfDescription", "..."));
				break;
			}
		}

		PageSummary = FText::Join(LOCTEXT("DescriptionDelimiter", " / "), InitialDescription);
		return true;
	}

	return false;
}

bool FAvalanchePage::UpdateTransitionLogic()
{
	check(IsTemplate());
	if (const UObject* LoadedSourceAsset = AssetPath.TryLoad())
	{
		if (const IAvaSceneInterface* SceneInterface = FAvalanchePageAssetUtils::GetSceneInterface(LoadedSourceAsset))
		{
			const UAvaTransitionTree* TransitionTree = FAvalanchePageAssetUtils::FindTransitionTree(SceneInterface);
			bHasTransitionLogic = TransitionTree ? TransitionTree->IsEnabled() : false;
			TransitionLayerTag = FAvalanchePageAssetUtils::GetTransitionLayerTag(TransitionTree);
		}
	}
	return false;
}

bool FAvalanchePage::HasTransitionLogic(const UAvalanchePlaylist* InPlaylist) const
{	
	const FAvalanchePage& Template = ResolveTemplate(InPlaylist);
	if (Template.IsValidPage())
	{
		// Combo templates have Transition Logic.
		return Template.IsComboTemplate() || Template.bHasTransitionLogic;
	}

	return bHasTransitionLogic;
}

FAvaTagHandle FAvalanchePage::GetTransitionLayer(const UAvalanchePlaylist* InPlaylist, int32 InTemplateIndex) const
{
	const FAvalanchePage& Template = GetTemplate(InPlaylist, InTemplateIndex);
	return Template.IsValidPage() ?  Template.TransitionLayerTag : TransitionLayerTag;
}

TArray<FAvaTagHandle> FAvalanchePage::GetTransitionLayers(const UAvalanchePlaylist* InPlaylist) const
{
	TArray<FAvaTagHandle> TransitionLayers;

	const int32 NumTemplates = GetNumTemplates(InPlaylist);
	TransitionLayers.Reserve(NumTemplates);

	for(int32 TemplateIndex = 0; TemplateIndex < NumTemplates; ++TemplateIndex)
	{
		const FAvalanchePage& Template = GetTemplate(InPlaylist, TemplateIndex);
		if (Template.IsValidPage())
		{
			TransitionLayers.Add(Template.TransitionLayerTag);
		}
	}

	return TransitionLayers;
}

int32 FAvalanchePage::AppendPageProgramStatuses(const UAvalanchePlaylist* InParentPlaylist, TArray<FAvalanchePageStatus>& OutPageStatuses) const
{
	if (!InParentPlaylist || IsTemplate())
	{
		return 0;
	}
	
	const int32 PreviousNumStatuses = OutPageStatuses.Num();
	
	IAvaMediaModule& AvaMediaModule = IAvaMediaModule::Get();
	FAvaMediaPlaybackManager& PlaybackManager = InParentPlaylist->GetPlaybackManager();
	const FSoftObjectPath ResolvedAssetPath = GetAvalancheAssetPath(InParentPlaylist);
	const FString ChannelName = GetChannelName().ToString();
	const UAvalanchePagePlayer* ProgramPagePlayer = InParentPlaylist->FindPlayerForProgramPage(GetPageId());
	const bool bIsPagePlaying = ProgramPagePlayer ? ProgramPagePlayer->IsPlaying() : false;
	const FAvaMediaPlaybackInstance* LocalPlaybackInstance = ProgramPagePlayer ? ProgramPagePlayer->GetPlaybackInstance() : nullptr;
	const FGuid LocalPlaybackInstanceId = LocalPlaybackInstance ? LocalPlaybackInstance->GetInstanceId() : FGuid();
	
	bool bLocalStatusAdded = false;

	auto AddLocalProgramStatusOnce = [LocalPlaybackInstance, &PlaybackManager, &bLocalStatusAdded, &OutPageStatuses, bIsPagePlaying, &ResolvedAssetPath, &ChannelName]()
	{
		using namespace UE::AvalanchePage::Private;

		// Local status is added only once.
		if (bLocalStatusAdded)
		{
			return;
		}
		
		// if page is playing, access the status from the page player's playback instance.
		if (LocalPlaybackInstance)
		{
			OutPageStatuses.Add(GetProgramPageStatus(LocalPlaybackInstance->GetStatus(), bIsPagePlaying, false));
		}
		else
		{
			// If the page is not playing, try to find a cached playback instance of the asset for the given channel.
			constexpr FGuid InvalidId;	// Providing an invalid instance id will fallback to searching by channel. 
			if (const TSharedPtr<FAvaMediaPlaybackInstance> PlaybackInstance = PlaybackManager.FindPlaybackInstance(InvalidId, ResolvedAssetPath, ChannelName))
			{
				OutPageStatuses.Add(GetProgramPageStatus(PlaybackInstance->GetStatus(), bIsPagePlaying, false));
			}
			else
			{
				OutPageStatuses.Add(GetProgramPageStatus(PlaybackManager.GetUnloadedPlaybackStatus(ResolvedAssetPath), bIsPagePlaying, false));
			}
		}
		bLocalStatusAdded = true;
	};
	
	const FAvaOutputChannel& Channel = UAvalancheBroadcast::Get().GetCurrentProfile().GetChannel(GetChannelName());
	const TArray<UMediaOutput*>& Outputs = Channel.GetMediaOutputs();
	IAvaMediaPlaybackClient& PlaybackClient = AvaMediaModule.GetMediaPlaybackClient();

	TSet<FString> AddedServers;

	for (const UMediaOutput* Output : Outputs)
	{
		if (Channel.IsMediaOutputRemote(Output))
		{
			if (Channel.GetMediaOutputState(Output) != EAvaMediaOutputState::Offline)
			{
				const FString& ServerForOutput = Channel.GetMediaOutputServerName(Output);

				if (AddedServers.Contains(ServerForOutput))
				{
					continue; // We have already added that server.
				}

				AddedServers.Add(ServerForOutput);	// Keep track of our servers so we add it only once.

				// There is only one playback/asset status per server, even if it has many outputs.
				TOptional<EAvaMediaPlaybackStatus> PlaybackStatus = PlaybackClient.GetRemotePlaybackStatus(LocalPlaybackInstanceId, ResolvedAssetPath, ChannelName, ServerForOutput);
				const TOptional<EAvaMediaPlaybackAssetStatus> PlaybackAssetStatus = PlaybackClient.GetRemotePlaybackAssetStatus(ResolvedAssetPath, ServerForOutput);

				if (!PlaybackAssetStatus.IsSet())
				{
					PlaybackClient.RequestPlaybackAssetStatus(ResolvedAssetPath, ServerForOutput, false);
				}

				using namespace UE::AvalanchePage::Private;
				if (!PlaybackStatus.IsSet())
				{
					PlaybackClient.RequestPlayback(LocalPlaybackInstanceId, ResolvedAssetPath, ChannelName, EAvaMediaPlaybackAction::Status);
					// Derive playback status from the asset status.
					PlaybackStatus = PlaybackAssetStatus.IsSet() ? GetPlaybackStatus(PlaybackAssetStatus.GetValue()) : EAvaMediaPlaybackStatus::Unknown;
				}

				const bool bAssetNeedsSync = PlaybackAssetStatus.IsSet() && PlaybackAssetStatus.GetValue() == EAvaMediaPlaybackAssetStatus::NeedsSync;
				OutPageStatuses.Add(GetProgramPageStatus(PlaybackStatus.GetValue(), bIsPagePlaying, bAssetNeedsSync));
			}
			else
			{
				OutPageStatuses.Add({EAvaBroadcastChannelType::Program, EAvalanchePageStatus::Offline, false});
			}
		}
		else
		{
			// All local outputs lead to only one status, the local one.
			AddLocalProgramStatusOnce();
		}
	}

	// If there are no outputs defined, use the local status.
	if (Outputs.IsEmpty())
	{
		AddLocalProgramStatusOnce();
	}
	
	return OutPageStatuses.Num() - PreviousNumStatuses;
}

int32 FAvalanchePage::AppendPagePreviewStatuses(const UAvalanchePlaylist* InParentPlaylist, const FName& InPreviewChannelName, TArray<FAvalanchePageStatus>& OutPageStatuses) const
{
	if (!InParentPlaylist)
	{
		return 0;
	}
	
	FAvaMediaPlaybackManager& PlaybackManager = InParentPlaylist->GetPlaybackManager();
	const FSoftObjectPath ResolvedAssetPath = GetAvalancheAssetPath(InParentPlaylist);

	const int32 PreviousNumStatuses = OutPageStatuses.Num();
	
	// For the preview, we only add the status if it is playing.
	// We are not really interested if the preview is loaded.
	if (InParentPlaylist->IsPagePreviewing(PageId))
	{
		OutPageStatuses.Add({EAvaBroadcastChannelType::Preview, EAvalanchePageStatus::Previewing, false});
	}
	else
	{
		using namespace UE::AvalanchePage::Private;
		const FString PreviewChannelName = InPreviewChannelName.IsNone() ? InParentPlaylist->GetDefaultPreviewChannelName().ToString() : InPreviewChannelName.ToString();

		// If the page is not previewing, try to find a cached playback instance of the asset for the given preview channel.
		constexpr FGuid InvalidId;	// Providing an invalid instance id will fallback to searching by channel. 
		if (const TSharedPtr<FAvaMediaPlaybackInstance> PlaybackInstance = PlaybackManager.FindPlaybackInstance(InvalidId, ResolvedAssetPath, PreviewChannelName))
		{
			OutPageStatuses.Add(GetPageStatus(EAvaBroadcastChannelType::Preview, PlaybackInstance->GetStatus(), false, false));
		}
		else
		{
			OutPageStatuses.Add(GetPageStatus(EAvaBroadcastChannelType::Preview, PlaybackManager.GetUnloadedPlaybackStatus(ResolvedAssetPath), false, false));
		}
	}

	return OutPageStatuses.Num() - PreviousNumStatuses;
}

TArray<FAvalanchePageStatus> FAvalanchePage::GetPageStatuses(const UAvalanchePlaylist* InParentPlaylist) const
{
	TArray<FAvalanchePageStatus> Statuses;
	if (InParentPlaylist)
	{
		// Typical instanced page: 1 program channel + 1 preview channel.
		// Template page only has a preview channel.
		Statuses.Reserve(IsTemplate() ? 1 : 2);
		
		if (!IsTemplate())
		{
			AppendPageProgramStatuses(InParentPlaylist, Statuses);
		}
		AppendPagePreviewStatuses(InParentPlaylist, InParentPlaylist->GetDefaultPreviewChannelName(), Statuses);
	}
	return Statuses;
}

TArray<FAvalanchePageStatus> FAvalanchePage::GetPageContextualStatuses(const UAvalanchePlaylist* InParentPlaylist) const
{
	TArray<FAvalanchePageStatus> Statuses;
	if (InParentPlaylist)
	{
		Statuses.Reserve(1);
		
		if (!IsTemplate())
		{
			AppendPageProgramStatuses(InParentPlaylist, Statuses);
		}
		else
		{
			AppendPagePreviewStatuses(InParentPlaylist, InParentPlaylist->GetDefaultPreviewChannelName(), Statuses);
		}
	}
	return Statuses;
}

TArray<FAvalanchePageStatus> FAvalanchePage::GetPageProgramStatuses(const UAvalanchePlaylist* InParentPlaylist) const
{
	TArray<FAvalanchePageStatus> Statuses;
	if (InParentPlaylist && !IsTemplate())
	{
		Statuses.Reserve(1);
		AppendPageProgramStatuses(InParentPlaylist, Statuses);
	}
	return Statuses;
}

TArray<FAvalanchePageStatus> FAvalanchePage::GetPagePreviewStatuses(const UAvalanchePlaylist* InParentPlaylist, const FName& InPreviewChannelName) const
{
	TArray<FAvalanchePageStatus> Statuses;
	if (InParentPlaylist)
	{
		Statuses.Reserve(1);
		AppendPagePreviewStatuses(InParentPlaylist, InPreviewChannelName, Statuses);
	}
	return Statuses;
}

FSoftObjectPath FAvalanchePage::GetAvalancheAssetPath(const UAvalanchePlaylist* InPlaylist, int32 InTemplateIndex) const
{
	const FAvalanchePage& Template = GetTemplate(InPlaylist, InTemplateIndex);
	return Template.IsValidPage() ? Template.AssetPath : AssetPath;
}

TArray<FSoftObjectPath> FAvalanchePage::GetAvalancheAssetPaths(const UAvalanchePlaylist* InPlaylist) const
{
	TArray<FSoftObjectPath> AssetPaths;

	const int32 NumTemplates = GetNumTemplates(InPlaylist);
	AssetPaths.Reserve(NumTemplates);

	for(int32 TemplateIndex = 0; TemplateIndex < NumTemplates; ++TemplateIndex)
	{
		const FAvalanchePage& Template = GetTemplate(InPlaylist, TemplateIndex);
		if (Template.IsValidPage())
		{
			AssetPaths.Add(Template.AssetPath);
		}
	}

	return AssetPaths;
}

bool FAvalanchePage::UpdateAvalancheAsset(const FSoftObjectPath& InAssetPath, bool bInReimportPage)
{
	if (IsComboTemplate())
	{
		UE_LOG(LogAvaPlaylist, Error, TEXT("Can't update asset on a combo page directly."));
		return false;
	}
	
	if (InAssetPath != AssetPath || bInReimportPage)
	{
		AssetPath = InAssetPath;

		if (!HasPageFriendlyName())
		{
			// Is GetName() the same as the asset name from soft object path?
			PageName = InAssetPath.GetAssetName();
		}

		UpdateTransitionLogic();
		return true;
	}
	return false;
}

FName FAvalanchePage::GetChannelName() const
{
	return UAvalancheBroadcast::Get().GetChannelName(OutputChannel);
}

void FAvalanchePage::SetChannelName(FName InChannelName)
{
	OutputChannel = UAvalancheBroadcast::Get().GetChannelIndex(InChannelName);
	if (OutputChannel == INDEX_NONE)
	{
		UE_LOG(LogAvaBroadcast, Error, TEXT("Channel %s was not found in broadcast channels, using %s instead."),
			*InChannelName.ToString(), *UAvalancheBroadcast::Get().GetChannelName(0).ToString());
		OutputChannel = 0;
	}
}

EAvaRemoteControlChanges FAvalanchePage::PruneRemoteControlValues(const FAvalancheRemoteControlValues& InRemoteControlValues)
{
	return RemoteControlValues.PruneRemoteControlValues(InRemoteControlValues);
}

EAvaRemoteControlChanges FAvalanchePage::UpdateRemoteControlValues(const FAvalancheRemoteControlValues& InRemoteControlValues, bool bInUpdateDefaults)
{
	return RemoteControlValues.UpdateRemoteControlValues(InRemoteControlValues, bInUpdateDefaults);
}

void FAvalanchePage::SetRemoteControlEntityValue(const FGuid& InId, const FAvalancheRemoteControlValue& InValue)
{
	RemoteControlValues.SetEntityValue(InId, InValue);
}

void FAvalanchePage::SetRemoteControlControllerValue(const FGuid& InId, const FAvalancheRemoteControlValue& InValue)
{
	RemoteControlValues.SetControllerValue(InId, InValue);
}

void FAvalanchePage::PostLoad()
{
	if (!AvalancheBlueprint_DEPRECATED.IsNull() && !AssetPath.IsValid())
	{
		AssetPath = AvalancheBlueprint_DEPRECATED.ToSoftObjectPath();
	}
	AvalancheBlueprint_DEPRECATED.Reset();
}

int32 FAvalanchePage::GetNumTemplates(const UAvalanchePlaylist* InPlaylist) const
{
	const FAvalanchePage& Template = ResolveTemplate(InPlaylist);
	if (Template.IsValidPage())
	{
		const int32 NumCombinedTemplates = Template.CombinedTemplateIds.Num();
		return NumCombinedTemplates > 0 ? NumCombinedTemplates : 1;
	}
	return 0;
}

const FAvalanchePage& FAvalanchePage::GetTemplate(const UAvalanchePlaylist* InPlaylist, int32 InIndex) const
{
	const FAvalanchePage& Template = ResolveTemplate(InPlaylist);
	
	if (Template.IsValidPage() && Template.CombinedTemplateIds.Num())
	{
		if (Template.CombinedTemplateIds.IsValidIndex(InIndex))
		{
			// Remark: not supporting recursive template combos.
			const FAvalanchePage& OtherTemplate  = InPlaylist->GetPage(Template.CombinedTemplateIds[InIndex]);
			if (OtherTemplate.IsValidPage())
			{
				return OtherTemplate;
			}
			
			UE_LOG(LogAvaPlaylist, Error,
				TEXT("Internal error while accessing page %d's template (%d index %d): reference to template Id %d is not valid."),
				GetPageId(), Template.GetPageId(), InIndex, Template.CombinedTemplateIds[InIndex]);
		}
		
		UE_LOG(LogAvaPlaylist, Error,
			TEXT("Internal error while accessing page %d's template (%d): specified index %d is not valid."),
			GetPageId(), Template.GetPageId(), InIndex);
	}
	
	return Template;
}

const FAvalanchePage& FAvalanchePage::ResolveTemplate(const UAvalanchePlaylist* InPlaylist) const
{
	if (IsTemplate())
	{
		return *this;
	}

	if (IsValid(InPlaylist))
	{
		if (InPlaylist->GetTemplatePages().PageIndices.Contains(GetPageId()))
		{
			UE_LOG(LogAvaPlaylist, Warning,
				TEXT("PageId %d is in the template list but has \"IsTemplate\" flag to false."), GetPageId());

			// We're obviously a template, but there's been a mix-up...
			return *this;
		}

		if (InPlaylist->GetInstancedPages().PageIndices.Contains(GetPageId()))
		{
			if (const int32* TemplateIdxPtr = InPlaylist->GetTemplatePages().PageIndices.Find(GetTemplateId()))
			{
				return InPlaylist->GetTemplatePages().Pages[*TemplateIdxPtr];
			}
			
			UE_LOG(LogAvaPlaylist, Error,
				TEXT("PageId [%d] is an instanced page, has template id [%d], but that template doesn't exist."),
				GetPageId(), GetTemplateId());
		}
	}

	return NullPage;
}

bool FAvalanchePage::IsTemplateMatchingByValue(const FAvalanchePage& InTemplatePage) const
{
	if (!IsTemplate() || !InTemplatePage.IsTemplate())
	{
		return false;
	}

	if (AssetPath != InTemplatePage.AssetPath)
	{
		return false;
	}

	if (!RemoteControlValues.HasSameEntityValues(InTemplatePage.RemoteControlValues)
		|| !RemoteControlValues.HasSameControllerValues(InTemplatePage.RemoteControlValues))
	{
		return false;
	}
	
	return true;
}

#undef LOCTEXT_NAMESPACE
