// Copyright Epic Games, Inc. All Rights Reserved.
#include "View/SMixerInsightInspectorView.h"

#include "2D/Tex.h"
#include "MixerInsight.h"
#include "Model/MixerInsightSession.h"
#include "View/SMixerInsightBlobView.h"
#include "View/SMixerInsightDeviceBufferView.h"
#include "View/SMixerInsightDeviceView.h"
#include "View/SMixerInsightRecordTrailView.h"
#include "View/SMixerInsightSessionView.h"
#include "Widgets/SNullWidget.h"

#include "Device/FX/DeviceBuffer_FX.h"
#include <Widgets/Layout/SHeader.h>


void SMixerInsightBatchInspectorView::Construct(const FArguments& Args)
{
	_rootItems.Empty(0);

	InspectBatch(Args._recordID);
};

TSharedRef<ITableRow> SMixerInsightBatchInspectorView::OnGenerateTileForView(FItem item, const TSharedRef<STableViewBase>& OwnerTable)
{
	const auto& br = MixerInsight::Instance()->GetSession()->GetRecord().GetBatch(item->_batchID);

	RecordID targetRID = br.ResultBlobs[item->_targetIndex];
	FString targetTypeName = TextureHelper::TextureTypeToString((TextureType) item->_targetIndex);
	FString label = "Target " + targetTypeName + " : " + FString::FromInt(targetRID.Blob());
	return SNew(STableRow<FItem>, OwnerTable)
		.Padding(4)
		.Content()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHeader)
				[
					SNew(STextBlock)
					.Text(FText::FromString(label))
				]
			]
			+ SVerticalBox::Slot()
			[
				SNew(SMixerInsightBlobView)
				.recordID(targetRID)
			]
		];
}

void SMixerInsightBatchInspectorView::InspectBatch(RecordID batchID)
{
	if (!batchID.IsBatch())
	{
		batchID = RecordID();
	}

	_rootItems.Empty();

	const auto& br = MixerInsight::Instance()->GetSession()->GetRecord().GetBatch(batchID);

	int32 resultIdx = 0;
	for (auto inputBlobId : br.ResultBlobs)
	{
		if (inputBlobId.IsValid())
			_rootItems.Add(MakeShareable(new FItemData(batchID, resultIdx)));
		resultIdx++;
	}

	ChildSlot
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)
		+ SSplitter::Slot()
		.Value(3)
		[

			SAssignNew(_view, SItemTileView)
			.ListItemsSource(&_rootItems)
		.ItemWidth(256)
		.ItemHeight(256)
		.OnGenerateTile(this, &SMixerInsightBatchInspectorView::OnGenerateTileForView)
		]
		];


	_view->RequestListRefresh();
}


void SMixerInsightJobInspectorView::Construct(const FArguments& Args)
{
	_rootItems.Empty(0);

	InspectJob(Args._recordID);
};

TSharedRef<ITableRow> SMixerInsightJobInspectorView::OnGenerateTileForView(FItem item, const TSharedRef<STableViewBase>& OwnerTable)
{
	const auto& br = MixerInsight::Instance()->GetSession()->GetRecord().GetBatch(item->_jobID);
	const auto& jr = br.GetJob(item->_jobID);

	RecordID inputRID = jr.InputBlobIds[item->_inputIndex];

	FString label = jr.InputArgNames[item->_inputIndex] + " : " + FString::FromInt(inputRID.Blob());
	return SNew(STableRow<FItem>, OwnerTable)
		.Padding(4)
		.Content()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHeader)
				[
					SNew(STextBlock)
					.Text(FText::FromString(label))
				]
			]
			+ SVerticalBox::Slot()
			[
				SNew(SMixerInsightBlobView)
				.recordID(inputRID)
				.tilesMask(item->_tilesMask)
			]
		];
}

void SMixerInsightJobInspectorView::InspectJob(RecordID jobID)
{
	if (!jobID.IsJob())
	{
		jobID = RecordID();
	}

	_rootItems.Empty();

	const auto& br = MixerInsight::Instance()->GetSession()->GetRecord().GetBatch(jobID);
	const auto& jr = br.GetJob(jobID);

	int32 inputIdx = 0;
	for (auto inputBlobId : jr.InputBlobIds)
	{
		if (inputBlobId.IsValid())
			_rootItems.Add(MakeShareable(new FItemData(jobID, inputIdx)));
		inputIdx++;
	}

	ChildSlot
		[
		SNew(SSplitter)
		.Orientation(Orient_Vertical)
		+ SSplitter::Slot()
		.Value(3)
		[
			SAssignNew(_outBlobView, SMixerInsightBlobView)
			.recordID(jr.ResultBlobId)
			.tilesMask(jr.Tiles)
		]
		+ SSplitter::Slot()
		[
			SAssignNew(_view, SItemTileView)
			.ListItemsSource(&_rootItems)
			.ItemWidth(256)
			.ItemHeight(256)
			.OnGenerateTile(this, &SMixerInsightJobInspectorView::OnGenerateTileForView)
		]
		];


	_view->RequestListRefresh();
}


void SMixerInsightBlobInspectorView::Construct(const FArguments& Args)
{
	InspectBlob(Args._recordID);
};


void SMixerInsightBlobInspectorView::InspectBlob(RecordID blobID)
{
	ChildSlot
		[
			SAssignNew(_blobView, SMixerInsightBlobView)
				.recordID(blobID)
		];
}

void SMixerInsightDeviceBufferInspectorView::Construct(const FArguments& Args)
{
	InspectBuffer(Args._recordID);
};


void SMixerInsightDeviceBufferInspectorView::InspectBuffer(RecordID rid)
{
	if (rid.IsDevice())
	{
		ChildSlot
		[
			SNew(SMixerInsightDeviceView)
			.deviceType((DeviceType) rid.Buffer_DeviceType())
		];
	}
	else
	{
		ChildSlot
		[
			SNew(SMixerInsightDeviceBufferView)
			.recordID(rid)
			.withDescription(true)
		];
	}
}


void SMixerInsightInspectorView::Construct(const FArguments& Args)
{
	ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(_recordTrail, SMixerInsightRecordTrailView)
			]
			+ SVerticalBox::Slot()
			[
				SAssignNew(_stack, SOverlay)
			]
		];

	// install the observer notifications
	auto sr = StaticCastSharedRef<SMixerInsightInspectorView>(this->AsShared());
	MixerInsight::Instance()->GetSession()->OnBatchInspected().AddSP(sr, &SMixerInsightInspectorView::OnInspected);
	MixerInsight::Instance()->GetSession()->OnJobInspected().AddSP(sr, &SMixerInsightInspectorView::OnInspected);
	MixerInsight::Instance()->GetSession()->OnBlobInspected().AddSP(sr, &SMixerInsightInspectorView::OnInspected);
	MixerInsight::Instance()->GetSession()->OnBufferInspected().AddSP(sr, &SMixerInsightInspectorView::OnInspected);

	MixerInsight::Instance()->GetSession()->OnEngineReset().AddSP(sr, &SMixerInsightInspectorView::OnEngineReset);
};


void SMixerInsightInspectorView::OnInspected(RecordID rid)
{
	_recordTrail->Refresh(rid);
	_stack->ClearChildren();

	if (rid.IsBuffer())
	{
		_stack->AddSlot()
			[
				SNew(SMixerInsightDeviceBufferInspectorView)
				.recordID(rid)
			];
	}
	else if (rid.IsBlob())
	{
		_stack->AddSlot()
			[
				SNew(SMixerInsightBlobInspectorView)
				.recordID(rid)
			];
	}
	else if (rid.IsJob())
	{
		_stack->AddSlot()
			[
				SNew(SMixerInsightJobInspectorView)
				.recordID(rid)
			];
	}
	else if (rid.IsBatch())
	{
		_stack->AddSlot()
			[
				SNew(SMixerInsightBatchInspectorView)
				.recordID(rid)
			];
	}
}


void SMixerInsightInspectorView::OnEngineReset(int32 id)
{
	// clear the inspector _trail and _stack when engine is destroyed
	_recordTrail->Refresh(RecordID());
	_stack->ClearChildren();
}
