// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CaptureSourceCapability.h"
#include "CaptureSourceFactory.h"
#include "CaptureSourceManager.h"

#include "Widgets/SWidget.h"

class CAPTURESOURCEFRAMEWORK_API FCaptureSourceDetailsCustomization
{
public:

	DECLARE_DELEGATE_OneParam(FCaptureSourceCreated, FCaptureSourceId);

	DECLARE_DELEGATE_RetVal_ThreeParams(TSharedPtr<SWidget>, FPropertyWidgetCreator, FCaptureSourceId, FCaptureSourceCapability*, const FPropertyDesc&);
	DECLARE_DELEGATE_RetVal_ThreeParams(TSharedPtr<SWidget>, FCommandWidgetCreator, FCaptureSourceId, FCaptureSourceCapability*, const FCommandDesc&);
	DECLARE_DELEGATE_RetVal_TwoParams(TSharedPtr<SWidget>, FCapabilityWidgetCreator, FCaptureSourceId, FCaptureSourceCapability*);
	DECLARE_DELEGATE_RetVal_TwoParams(TSharedPtr<SWindow>, FCaptureSourceWidgetCreator, const FCaptureSourceDescriptor&, FCaptureSourceCreated);

	FCaptureSourceDetailsCustomization();

	void RegisterPropertyCustomization(const FString& InCaptureSourceClassId,
									   const FString& InCapabilityName, 
									   const FString& InPropertyName, 
									   FPropertyWidgetCreator InWidgetCreator);

	void RegisterCommandCustomization(const FString& InCaptureSourceClassId,
									  const FString& InCapabilityName,
									  const FString& InCommandName, 
									  FCommandWidgetCreator InWidgetCreator);

	void RegisterCapabilityCustomization(const FString& InCaptureSourceClassId,
										 const FString& InCapabilityName, 
										 FCapabilityWidgetCreator InWidgetCreator);

	void RegisterCaptureSourceCustomization(const FString& InCaptureSourceClassId, 
											FCaptureSourceWidgetCreator InWidgetCreator);

	TSharedPtr<SWidget> CreatePropertyWidget(FCaptureSourceId InCaptureSourceId,
											 FCaptureSourceCapability* InCapability, 
											 const FPropertyDesc& InProperty);

	TSharedPtr<SWidget> CreateCommandWidget(FCaptureSourceId InCaptureSourceId,
											FCaptureSourceCapability* InCapability,
											const FCommandDesc& InCommand);

	TSharedPtr<SWidget> CreateCapabilityWidget(FCaptureSourceId InCaptureSourceId,
											   FCaptureSourceCapability* InCapability);

	TSharedPtr<SWindow> CreateCaptureSourceWidget(const FCaptureSourceDescriptor& InCreationParams,
												  FCaptureSourceCreated InCaptureSourceCreated);
	
private:

	template <typename ... Args>
	FString GenerateKey(Args... InArgs)
	{
		static_assert(sizeof...(Args) != 0 && sizeof...(Args) <= 3,
					  "Method currently supports 1 - 3 arguments");

		FString Format;
		if constexpr (sizeof...(Args) == 1)
		{
			Format = TEXT("{0}");
		}
		else if constexpr (sizeof...(Args) == 2)
		{
			Format = TEXT("{0}.{1}");
		}
		else
		{
			Format = TEXT("{0}.{1}.{2}");
		}
		return FString::Format(*Format, { (InArgs)... });
	}

	FString GenerateDefaultMapKey(const FString& InName);

	bool ContainsPropertyWidgetCustomization(const FString& InFactoryId, 
											 const FString& InCapabilityName, 
											 const FString& InPropertyName);

	TSharedPtr<SWidget> CreateDefaultPropertyWidget(FCaptureSourceCapability* InCapability,
													const FPropertyDesc& InProperty);

	TSharedPtr<SWidget> CreateDefaultCapabilityPropertyWidget(FCaptureSourceCapability* InCapability, 
															  const FPropertyDesc& InProperty,
															  TSharedPtr<FPropertyValue> InPropertyValue);

	TSharedPtr<SWidget> CreateParamWidget(const FPropertyDesc& InProperty, FPropertyValue& InPropertyValue);

	TSharedPtr<SWidget> CreateDefaultCommandWidget(FCaptureSourceCapability* InCapability, const FCommandDesc& InCommand);

	TSharedPtr<SWidget> MakeKeyWidget(const FPropertyDesc& InProperty);
	TSharedPtr<SWidget> MakeValueWidget(FCaptureSourceCapability* InCapability, 
										const FPropertyDesc& InProperty,
										TSharedPtr<FPropertyValue> InPropertyValue);

	TSharedPtr<SWidget> MakeValueWidget(const FPropertyDesc& InProperty, FPropertyValue& InPropertyValue);

	FPropertyValue GetDefaultValueForProperty(const FPropertyDesc& InProperty);

	void HandlePropertyChangedEvent(TSharedPtr<const FCaptureEvent> InEvent,
									TMap<FString, TSharedPtr<FPropertyValue>> InPropertyValueHolder);

	TMap<FString, FPropertyWidgetCreator> PropertyCustomizations;
	TMap<FString, FCommandWidgetCreator> CommandCustomizations;
	TMap<FString, FCaptureSourceWidgetCreator> CreationParamsCustomizations;
	TMap<FString, FCapabilityWidgetCreator> CapabilityCustomizations;
};