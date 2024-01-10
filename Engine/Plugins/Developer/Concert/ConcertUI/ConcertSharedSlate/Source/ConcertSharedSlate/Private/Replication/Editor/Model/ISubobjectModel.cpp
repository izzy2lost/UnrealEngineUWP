// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Editor/Model/ISubobjectModel.h"

#include "UObject/SoftObjectPath.h"

namespace UE::ConcertSharedSlate
{
	void ISubobjectModel::ForEachSubobject(TFunctionRef<EBreakBehavior(const FSoftObjectPath& Parent, const FSoftObjectPath& ChildObject)> Callback)
	{
		struct FHelper
		{
			static EBreakBehavior Visit(ISubobjectModel& Model, const FSoftObjectPath& Parent, const FSoftObjectPath& ChildObject, TFunctionRef<EBreakBehavior(const FSoftObjectPath&, const FSoftObjectPath&)> Callback)
			{
				if (Callback(Parent, ChildObject) == EBreakBehavior::Break)
				{
					return EBreakBehavior::Break;
				}
				
				EBreakBehavior Result = EBreakBehavior::Continue;
				Model.ForEachDirectChildSubobject(ChildObject, [&Model, NewParent = ChildObject, &Callback, &Result](const FSoftObjectPath& ChildObject)
				{
					Result = Visit(Model, NewParent, ChildObject, Callback);
					return Result;
				});
				return Result;
			}
		};
			
		for (const FName& Category : GetCategories())
		{
			ForEachRootSubobject(Category, [this, &Callback](const FSoftObjectPath& ChildObject)
			{
				return FHelper::Visit(*this, GetTopLevelObject(), ChildObject, Callback);
			});
		}
	}
}