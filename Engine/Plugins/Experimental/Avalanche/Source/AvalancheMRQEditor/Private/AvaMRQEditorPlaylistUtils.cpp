// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaMRQEditorPlaylistUtils.h"
#include "AvaMRQRundownPageSetting.h"
#include "AvaSceneSubsystem.h"
#include "AvaSequencerUtils.h"
#include "EditorWorldUtils.h"
#include "IAvaSceneInterface.h"
#include "IAvaSequenceProvider.h"
#include "MoviePipelineDeferredPasses.h"
#include "MoviePipelineImageSequenceOutput.h"
#include "MoviePipelineOutputSetting.h"
#include "MoviePipelinePIEExecutor.h"
#include "MoviePipelineQueueSubsystem.h"
#include "Playback/AvaMediaPlaybackUtils.h"
#include "Playlist/AvaPlaylistEditor.h"

DEFINE_LOG_CATEGORY_STATIC(LogAvaMRQEditorPlaylist, Log, All);

namespace UE::AvaMRQEditor::Private
{
	void CleanupRootedWorlds(TConstArrayView<TWeakObjectPtr<UWorld>> InRootedWorlds)
	{
		for (const TWeakObjectPtr<UWorld>& RootedWorldWeak : InRootedWorlds)
		{
			if (UWorld* const RootedWorld = RootedWorldWeak.Get())
			{
				RootedWorld->RemoveFromRoot();
			}
		}
	}

	struct FAvaMRQScopedRender
	{
		FAvaMRQScopedRender()
		{
			QueueSubsystem = GEditor ? GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>() : nullptr;
			ensureMsgf(QueueSubsystem, TEXT("Not able to access UMoviePipelineQueueSubsystem (returned null)"));
		}

		~FAvaMRQScopedRender()
		{
			if (JobAllocationCount > 0)
			{
				UMoviePipelineExecutorBase* Executor = QueueSubsystem->RenderQueueWithExecutor(UMoviePipelinePIEExecutor::StaticClass());
				if (Executor && !RootedWorlds.IsEmpty())
				{
					Executor->OnExecutorFinished().AddLambda([RootedWorlds = MoveTemp(RootedWorlds)](UMoviePipelineExecutorBase*, bool)
					{
						CleanupRootedWorlds(RootedWorlds);
					});
				}
			}
		}

		UMoviePipelineExecutorJob* AllocateJob()
		{
			UMoviePipelineQueue* const Queue = QueueSubsystem->GetQueue();
			if (!Queue)
			{
				return nullptr;
			}
			if (JobAllocationCount == 0)
			{
				Queue->DeleteAllJobs();
			}
			if (UMoviePipelineExecutorJob* Job = Queue->AllocateNewJob(UMoviePipelineExecutorJob::StaticClass()))
			{
				++JobAllocationCount;
				return Job;
			}
			return nullptr;
		}

		bool IsValid() const
		{
			return ::IsValid(QueueSubsystem) && ::IsValid(QueueSubsystem->GetQueue());
		}

		void EnsureRootedWorld(UWorld* InWorldChecked)
		{
			check(InWorldChecked);
			if (!InWorldChecked->IsRooted())
			{
				InWorldChecked->AddToRoot();
				RootedWorlds.Add(InWorldChecked);
			}
		}

	private:		
		UMoviePipelineQueueSubsystem* QueueSubsystem = nullptr;

		uint32 JobAllocationCount = 0;

		TArray<TWeakObjectPtr<UWorld>> RootedWorlds;
	};

	void RenderSequence(FAvaMRQScopedRender& InScopedRender, UWorld* InWorld, const UAvalanchePlaylist& InPlaylist, const FAvalanchePage& InPage, UAvaSequence* InSequence)
	{
		UMoviePipelineExecutorJob* Job = InScopedRender.AllocateJob();
		if (!Job)
		{
			return;
		}

		constexpr const TCHAR* Separator = TEXT("-");

		Job->JobName = InPlaylist.GetName() + Separator + TEXT("Page_") + FString::FromInt(InPage.GetPageId()) + Separator + InSequence->GetName();
		Job->Map = InWorld;
		Job->SetSequence(InSequence);

		UMoviePipelinePrimaryConfig* MRQConfig = Job->GetConfiguration();
		check(MRQConfig);

		auto CreatePipelineSetting = [MRQConfig]<typename InSettingType>(InSettingType*& OutSettings)
		{
			OutSettings = Cast<InSettingType>(MRQConfig->FindOrAddSettingByClass(InSettingType::StaticClass()));
		};

		UMoviePipelineImageSequenceOutput_PNG* OutputPNG;
		CreatePipelineSetting(OutputPNG);

		UMoviePipelineDeferredPassBase* DeferredPass;
		CreatePipelineSetting(DeferredPass);

		UMoviePipelineOutputSetting* OutputSettings;
		CreatePipelineSetting(OutputSettings);
		OutputSettings->FileNameFormat = TEXT("{job_name}.{frame_number}");;

		UAvaMRQRundownPageSetting* RundownPageSetting;
		CreatePipelineSetting(RundownPageSetting);
		RundownPageSetting->RundownPage.Rundown = &InPlaylist;
		RundownPageSetting->RundownPage.PageId  = InPage.GetPageId();

		UE_LOG(LogAvaMRQEditorPlaylist, Log
			, TEXT("New MRQ Job '%s' created for World '%s' and LevelSequence '%s'")
			, *Job->GetFullName()
			, *InWorld->GetFullName()
			, *InSequence->GetFullName());
	}

	void RenderPage(FAvaMRQScopedRender& InScopedRender, const UAvalanchePlaylist& InPlaylist, const FAvalanchePage& InPage)
	{
		FSoftObjectPath PageAssetPath = InPage.GetAvalancheAssetPath(&InPlaylist);
		if (!FAvaMediaPlaybackUtils::IsMapAsset(PageAssetPath.GetLongPackageName()))
		{
			UE_LOG(LogAvaMRQEditorPlaylist, Error
				, TEXT("Page asset path '%s' (Page Id: '%d', Playlist '%s) is not a map asset and will not be processed")
				, *PageAssetPath.ToString()
				, InPage.GetPageId()
				, *InPlaylist.GetName());
			return;
		}

		UWorld* const World = Cast<UWorld>(PageAssetPath.TryLoad());
		if (!World)
		{
			UE_LOG(LogAvaMRQEditorPlaylist, Error
				, TEXT("World path '%s' (Page Id: '%d', Playlist '%s) did not load a valid world and will not be processed")
				, *PageAssetPath.ToString()
				, InPage.GetPageId()
				, *InPlaylist.GetName());
			return;
		}
		InScopedRender.EnsureRootedWorld(World);

		IAvaSceneInterface* SceneInterface = UAvaSceneSubsystem::FindSceneInterface(World->PersistentLevel);
		if (!SceneInterface || !SceneInterface->GetSequenceProvider())
		{
			UE_LOG(LogAvaMRQEditorPlaylist, Error
				, TEXT("World '%s' (Page Id: '%d', Playlist '%s) did not have a valid Scene Interface to retrieve the Sequences so will not be processed")
				, *PageAssetPath.ToString()
				, InPage.GetPageId()
				, *InPlaylist.GetName());
			return;
		}

		TConstArrayView<UAvaSequence*> Sequences = SceneInterface->GetSequenceProvider()->GetSequences();
		for (UAvaSequence* Sequence : Sequences)
		{
			RenderSequence(InScopedRender, World, InPlaylist, InPage, Sequence);
		}
	}

	void RenderPages(FAvaMRQScopedRender& InScopedRender, const UAvalanchePlaylist& InPlaylist, TConstArrayView<int32> InPageIds)
	{
		TSet<int32> ProcessedPages;
		ProcessedPages.Reserve(InPageIds.Num());

		for (int32 PageId : InPageIds)
		{
			bool bIsAlreadyInSet = false;
			ProcessedPages.Add(PageId, &bIsAlreadyInSet);

			if (bIsAlreadyInSet)
			{
				UE_LOG(LogAvaMRQEditorPlaylist, Warning
					, TEXT("Page Id '%d' is repeated in the page ids to export for Playlist '%s' and will not be processed multiple times")
					, PageId
					, *InPlaylist.GetName());
				continue;
			}

			const FAvalanchePage& Page = InPlaylist.GetPage(PageId);
			if (!Page.IsValidPage())
			{
				UE_LOG(LogAvaMRQEditorPlaylist, Warning
					, TEXT("Page Id '%d' is invalid for Playlist '%s' and will not be processed")
					, PageId
					, *InPlaylist.GetName());
				continue;
			}

			RenderPage(InScopedRender, InPlaylist, Page);
		}
	}
}

void FAvaMRQEditorPlaylistUtils::RenderSelectedPages(TConstArrayView<TWeakPtr<const FAvaPlaylistEditor>> InPlaylistEditors)
{
	if (InPlaylistEditors.IsEmpty())
	{
		return;
	}

	using namespace UE::AvaMRQEditor;
	TSet<TWeakPtr<const FAvaPlaylistEditor>> ProcessedPlaylistEditors;
	ProcessedPlaylistEditors.Reserve(InPlaylistEditors.Num());

	Private::FAvaMRQScopedRender ScopedRender;
	if (!ScopedRender.IsValid())
	{
		return;
	}

	for (TWeakPtr<const FAvaPlaylistEditor> PlaylistEditorWeak : InPlaylistEditors)
	{
		TSharedPtr<const FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin();
		if (!PlaylistEditor || !PlaylistEditor->IsPlaylistValid())
		{
			continue;
		}

		UAvalanchePlaylist* Playlist = PlaylistEditor->GetPlaylist();
		check(Playlist);

		bool bIsAlreadyInSet = false;
		ProcessedPlaylistEditors.Add(PlaylistEditorWeak, &bIsAlreadyInSet);

		if (bIsAlreadyInSet)
		{
			UE_LOG(LogAvaMRQEditorPlaylist, Warning
				, TEXT("Playlist Editor '%s' is repeated in the editors to export and will not be processed again")
				, *Playlist->GetName());
			continue;
		}

		TConstArrayView<int32> PageIds = PlaylistEditor->GetSelectedPagesOnActiveSubListWidget();
		Private::RenderPages(ScopedRender, *Playlist, PageIds);
	}
}

void FAvaMRQEditorPlaylistUtils::RenderPages(const UAvalanchePlaylist& InPlaylist, TConstArrayView<int32> InPageIds)
{
	if (InPageIds.IsEmpty())
	{
		return;
	}

	using namespace UE::AvaMRQEditor;
	Private::FAvaMRQScopedRender ScopedRender;
	if (!ScopedRender.IsValid())
	{
		return;
	}

	Private::RenderPages(ScopedRender, InPlaylist, InPageIds);
}

void FAvaMRQEditorPlaylistUtils::RenderPage(const UAvalanchePlaylist& InPlaylist, const FAvalanchePage& InPage)
{
	using namespace UE::AvaMRQEditor;
	Private::FAvaMRQScopedRender ScopedRender;
	if (!ScopedRender.IsValid())
	{
		return;
	}

	Private::RenderPage(ScopedRender, InPlaylist, InPage);
}
