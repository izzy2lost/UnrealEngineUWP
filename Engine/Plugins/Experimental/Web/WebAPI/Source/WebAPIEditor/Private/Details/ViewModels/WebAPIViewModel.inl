// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WebAPIEnumViewModel.h"
#include "WebAPIModelViewModel.h"
#include "WebAPIOperationViewModel.h"
#include "WebAPIParameterViewModel.h"
#include "WebAPIServiceViewModel.h"
#include "WebAPIViewModel.h"
#include "Dom/WebAPIService.h"

template <typename ModelType, class ParentViewModelType, class ViewModelType>
TSharedPtr<ViewModelType> UE::WebAPI::Details::CreateViewModel(const TSharedRef<ParentViewModelType>& InParentViewModel, ModelType* InModel)
{
 	if(!InModel)
	{
		return nullptr;
	}
	
	if constexpr (std::is_same_v<UWebAPIEnum, ModelType>)
	{
		return FWebAPIEnumViewModel::Create(InParentViewModel, InModel);
	}
	else if constexpr (std::is_same_v<UWebAPIEnumValue, ModelType>)
    {
    	return FWebAPIEnumValueViewModel::Create(InParentViewModel, InModel);
    }
	else if constexpr (std::is_same_v<UWebAPIModel, ModelType>)
	{
		return FWebAPIModelViewModel::Create(InParentViewModel, InModel);
	}
	else if constexpr (std::is_same_v<UWebAPIProperty, ModelType>)
	{
		return FWebAPIPropertyViewModel::Create(InParentViewModel, InModel);
	}
	else if constexpr (std::is_same_v<UWebAPIService, ModelType>)
	{
		return FWebAPIServiceViewModel::Create(InParentViewModel, InModel);
	}
	else if constexpr (std::is_same_v<UWebAPIOperation, ModelType>)
	{
		return FWebAPIOperationViewModel::Create(InParentViewModel, InModel);
	}
	else if constexpr (std::is_same_v<UWebAPIParameter, ModelType>)
	{
		return FWebAPIParameterViewModel::Create(InParentViewModel, InModel);
	}
	else
	{
		static_assert(false, "Unsupported model type");
		return nullptr;
	}
}
