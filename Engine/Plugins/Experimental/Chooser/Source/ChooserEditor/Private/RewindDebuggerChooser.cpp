// Copyright Epic Games, Inc. All Rights Reserved.

#include "RewindDebuggerChooser.h"

#include "Chooser.h"
#include "ChooserProvider.h"
#include "ObjectTrace.h"

FRewindDebuggerChooser::FRewindDebuggerChooser()
{

}

void FRewindDebuggerChooser::Update(float DeltaTime, IRewindDebugger* RewindDebugger)
{
	const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession();
	TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
	
	const FChooserProvider* ChooserProvider = AnalysisSession->ReadProvider<FChooserProvider>(FChooserProvider::ProviderName);
	
	const TraceServices::IFrameProvider& FrameProvider = TraceServices::ReadFrameProvider(*RewindDebugger->GetAnalysisSession());
	TraceServices::FFrame Frame;
	if (FrameProvider.GetFrameFromTime(ETraceFrameType::TraceFrameType_Game, RewindDebugger->CurrentTraceTime(), Frame))
	{
		
		ChooserProvider->EnumerateChooserEvaluationTimelines([ChooserProvider, RewindDebugger, Frame](uint64 OwnerId, const FChooserProvider::ChooserEvaluationTimeline& ChooserEvaluationTimeline)
		{
			double StartTime = RewindDebugger->GetScrubTime();
			double EndTime = StartTime + Frame.EndTime - Frame.StartTime;
			
			ChooserEvaluationTimeline.EnumerateEvents(StartTime, EndTime, [ChooserProvider, StartTime, EndTime, OwnerId](double InStartTime, double InEndTime, uint32 InDepth, const FChooserEvaluationData& ChooserEvaluationData)
			{
				if (UObject* ChooserObject = FObjectTrace::GetObjectFromId(ChooserEvaluationData.ChooserId))
				{
					if (UChooserTable* Chooser = Cast<UChooserTable>(ChooserObject))
					{
						UChooserTable* ContextOwner = Chooser->GetContextOwner();
						if (ContextOwner->HasDebugTarget())
						{
							if (UObject* ContextObject = FObjectTrace::GetObjectFromId(OwnerId))
							{
								if (ContextOwner->GetDebugTarget() == ContextObject)
								{
									Chooser->SetDebugSelectedRow(ChooserEvaluationData.SelectedIndex);
									Chooser->bDebugTestValuesValid = true;

									ChooserProvider->ReadChooserValueTimeline(OwnerId, [StartTime, EndTime, Chooser, &ChooserEvaluationData](const FChooserProvider::ChooserValueTimeline& ValueTimeline)
									{
										ValueTimeline.EnumerateEvents(StartTime,EndTime,[Chooser, ChooserEvaluationData](double StartTime, double EndTime, uint32 Depth, const FChooserValueData& ValueData)
										{
											if (ChooserEvaluationData.ChooserId == ValueData.ChooserId)
											{
												for(FInstancedStruct& ColumnStruct : Chooser->ColumnsStructs)
												{
													FChooserColumnBase& Column = ColumnStruct.GetMutable<FChooserColumnBase>();
													if(ValueData.Key == Column.GetInputValue()->GetDebugName())
													{
														Column.SetTestValue(ValueData.Value);
													}
												}
											}
											return TraceServices::EEventEnumerate::Continue;
										});
									});
								}
							}
						}
					}
				}
				
				return TraceServices::EEventEnumerate::Continue;
			});
			

		});
	}
}

void FRewindDebuggerChooser::RecordingStarted(IRewindDebugger*)
{
	UE::Trace::ToggleChannel(TEXT("Chooser"), true);
}

void FRewindDebuggerChooser::RecordingStopped(IRewindDebugger*)
{
	UE::Trace::ToggleChannel(TEXT("Chooser"), false);
}