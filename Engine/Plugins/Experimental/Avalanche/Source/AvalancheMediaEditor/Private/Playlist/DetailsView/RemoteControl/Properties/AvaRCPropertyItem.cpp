// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaRCPropertyItem.h"

#include "RemoteControlEntity.h"
#include "SAvaRCPropertyItemRow.h"

FAvaRCPropertyItem::FAvaRCPropertyItem(TSharedRef<FRemoteControlEntity> InEntity, bool bInControlled)
{
	Entity = InEntity;
	bEntityControlled = bInControlled;
}

TSharedRef<ITableRow> FAvaRCPropertyItem::CreateWidget(TSharedRef<SAvaPageRemoteControlProps> InPropertyPanel, const TSharedRef<STableViewBase>& InOwnerTable) const
{
	return SNew(SAvaRCPropertyItemRow, InPropertyPanel, InOwnerTable, SharedThis(this));
}
