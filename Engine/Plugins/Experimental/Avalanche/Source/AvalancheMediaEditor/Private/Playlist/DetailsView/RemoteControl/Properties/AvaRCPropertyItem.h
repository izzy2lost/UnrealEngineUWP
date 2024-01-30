// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailTreeNode.h"
#include "Internationalization/Text.h"
#include "Templates/SharedPointer.h"

class IDetailTreeNode;
class ITableRow;
class SAvaPageRemoteControlProps;
class STableViewBase;
class URemoteControlPreset;
struct FAvaPageRemoteControlGenerateWidgetArgs;
struct FRemoteControlEntity;

class FAvaRCPropertyItem : public TSharedFromThis<FAvaRCPropertyItem>
{
public:
	FAvaRCPropertyItem(TSharedRef<FRemoteControlEntity> InEntity, bool bInControlled);

	TSharedRef<ITableRow> CreateWidget(TSharedRef<SAvaPageRemoteControlProps> InPropertyPanel, const TSharedRef<STableViewBase>& InOwnerTable) const;

	TSharedPtr<FRemoteControlEntity> GetEntity() const { return Entity.Pin(); }
	bool IsEntityControlled() const { return bEntityControlled; }

private:
	TWeakPtr<FRemoteControlEntity> Entity = nullptr;
	bool bEntityControlled = false;
};
