// Copyright Epic Games, Inc. All Rights Reserved.

#include "StylusInputDebugWidget.h"

#include <StylusInput.h>
#include <StylusInputTabletContext.h>
#include <Framework/Application/SlateApplication.h>
#include <Framework/MultiBox/MultiBoxBuilder.h>
#include <Widgets/SBoxPanel.h>
#include <Widgets/SOverlay.h>
#include <Widgets/Input/SComboButton.h>
#include <Widgets/Layout/SSplitter.h>
#include <Widgets/Text/SMultiLineEditableText.h>
#include <Widgets/Text/STextBlock.h>

#include "StylusInputDebugPaintWidget.h"

#define LOCTEXT_NAMESPACE "StylusInputDebugWidget"

namespace UE::StylusInput::DebugWidget
{
	DECLARE_LOG_CATEGORY_EXTERN(LogStylusInputDebugWidget, Log, All)

	DEFINE_LOG_CATEGORY(LogStylusInputDebugWidget);

	inline void LogError(const FString& Message)
	{
		UE_LOG(LogStylusInputDebugWidget, Error, TEXT("%s"), *Message);
	}

	inline void LogWarning(const FString& Message)
	{
		UE_LOG(LogStylusInputDebugWidget, Warning, TEXT("%s"), *Message);
	}

	inline void LogVerbose(const FString& Message)
	{
		UE_LOG(LogStylusInputDebugWidget, Verbose, TEXT("%s"), *Message);
	}

	void SStylusInputDebugWidget::Construct(const FArguments&)
	{
		// One-time timer to initialize stylus plugin as soon as widget is up.
		RegisterActiveTimer(
			0.0f,
			FWidgetActiveTimerDelegate::CreateLambda([this](double, float)
			{
				AcquireStylusInput();
				RegisterEventHandler();
				return EActiveTimerReturnType::Stop;
			}));

		ChildSlot
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SNew(SSplitter)
				.PhysicalSplitterHandleSize(2.0f)
				+ SSplitter::Slot()
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SAssignNew(PaintWidget, SStylusInputDebugPaintWidget)
					]
					+ SOverlay::Slot()
					.Padding(2.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.Text_Lambda([&TabletContext = LastPacketData.TabletContext]
									{
										return TabletContext
											       ? FText::Format(
												       LOCTEXT("TabletContextID", "Tablet Context ID: {0}"),
												       FText::FromString(FString::Printf(TEXT("%x"), TabletContext->GetID())))
											       : FText();
									})
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.Text_Lambda([&TabletContext = LastPacketData.TabletContext]
									{
										return TabletContext
											       ? FText::Format(
												       LOCTEXT("TabletContextName", "Name: {0}"),
												       FText::FromString(TabletContext->GetName()))
											       : FText();
									})
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.Text_Lambda([&TabletContext = LastPacketData.TabletContext]
									{
										if (TabletContext)
										{
											const FIntRect InputRectangle = TabletContext->GetInputRectangle();
											return FText::Format(
												LOCTEXT("TabletContextInputRectangle", "Input Rectangle: ({0}, {1}) x ({2}, {3})"),
												InputRectangle.Min.X, InputRectangle.Min.Y, InputRectangle.Max.X, InputRectangle.Max.Y);
										}
										return FText();
									})
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.Text_Lambda([&TabletContext = LastPacketData.TabletContext]
									{
										if (TabletContext)
										{
											const ETabletHardwareCapabilities Capabilities = TabletContext->GetHardwareCapabilities();

											FString CapabilitiesStr;
											if (Capabilities == ETabletHardwareCapabilities::None)
											{
												CapabilitiesStr = "None";
											}
											else
											{
												if ((Capabilities & ETabletHardwareCapabilities::Integrated) != ETabletHardwareCapabilities::None)
												{
													CapabilitiesStr += "Integrated";
												}
												if ((Capabilities & ETabletHardwareCapabilities::CursorMustTouch) != ETabletHardwareCapabilities::None)
												{
													CapabilitiesStr += CapabilitiesStr.IsEmpty() ? "CursorMustTouch" : " & CursorMustTouch";
												}
												if ((Capabilities & ETabletHardwareCapabilities::HardProximity) != ETabletHardwareCapabilities::None)
												{
													CapabilitiesStr += CapabilitiesStr.IsEmpty() ? "HardProximity" : " & HardProximity";
												}
												if ((Capabilities & ETabletHardwareCapabilities::CursorsHavePhysicalIds) != ETabletHardwareCapabilities::None)
												{
													CapabilitiesStr += CapabilitiesStr.IsEmpty() ? "CursorsHavePhysicalIds" : " & CursorsHavePhysicalIds";
												}
											}

											if (CapabilitiesStr.IsEmpty())
											{
												CapabilitiesStr = FString::Format(TEXT("Unknown ({0})"), {
													                                  static_cast<std::underlying_type_t<ETabletHardwareCapabilities>>(
														                                  Capabilities)
												                                  });
											}

											return FText::Format(
												LOCTEXT("TabletContextHardwareCapabilities", "Hardware Capabilities: {0}"), FText::FromString(CapabilitiesStr));
										}
										return FText();
									})
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.Text_Lambda([&StylusInfo = LastPacketData.StylusInfo]
									{
										return StylusInfo
											       ? FText::Format(
												       LOCTEXT("StylusID", "Stylus ID: {0}"),
												       FText::FromString(FString::Printf(TEXT("%d"), StylusInfo->GetID())))
											       : FText();
									})
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.Text_Lambda([&StylusInfo = LastPacketData.StylusInfo]
									{
										return StylusInfo
											       ? FText::Format(
												       LOCTEXT("StylusName", "Stylus Name: {0}"),
												       FText::FromString(StylusInfo->GetName()))
											       : FText();
									})
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.Text_Lambda([&StylusInfo = LastPacketData.StylusInfo]
									{
										if (StylusInfo)
										{
											FString ButtonsString = "<none>";
											for (int32 Index = 0, Num = StylusInfo->GetNumButtons(); Index < Num; ++Index)
											{
												if (const auto* Button = StylusInfo->GetButton(Index))
												{
													if (Index == 0)
													{
														ButtonsString = Button->GetName();
													}
													else
													{
														ButtonsString += ", " + Button->GetName();
													}
												}
											}

											return FText::Format(LOCTEXT("StylusButtons", "Stylus Buttons: {0}"), FText::FromString(ButtonsString));
										}
										return FText();
									})
								]
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							[
								SNullWidget::NullWidget
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(SComboButton)
									.OnGetMenuContent(this, &SStylusInputDebugWidget::GetEventHandlerThreadMenu)
									.ButtonContent()
									[
										SNew(STextBlock)
										.Text_Lambda([&EventHandlerThread = EventHandlerThread]
										{
											return FText::FromString(
												EventHandlerThread == EEventHandlerThread::Asynchronous ? "Asynchronous" : "On Game Thread");
										})
									]
								]
								+ SVerticalBox::Slot()
								.FillHeight(1.0f)
								[
									SNullWidget::NullWidget
								]
							]
						]
						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SNullWidget::NullWidget
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text_Lambda([&TabletContext = LastPacketData.TabletContext, &StylusInput = StylusInput, &EventHandlerThread = EventHandlerThread]
							{
								if (TabletContext && StylusInput)
								{
									return FText::Format(
										LOCTEXT("PacketsPerSecond", "Packets Per Second: {0}"),
										FMath::RoundToInt32(StylusInput->GetPacketsPerSecond(EventHandlerThread)));
								}
								return FText();
							})
						]
						/*+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text_Lambda([&bIsSet = LastPacketData.bIsSet, &CursorID = LastPacketData.Packet.CursorID]
							{
								return bIsSet ? FText::Format(LOCTEXT("CursorID", "Cursor ID: {0}"), CursorID) : FText();
							})
						]*/
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Visibility_Lambda([&bIsSet = LastPacketData.bIsSet, &TabletContext = LastPacketData.TabletContext]
							{
								return bIsSet && TabletContext && (TabletContext->GetSupportedProperties() & ETabletSupportedProperties::TimerTick)
								       != ETabletSupportedProperties::None
									       ? EVisibility::Visible
									       : EVisibility::Collapsed;
							})
							.Text_Lambda([&TimerTick = LastPacketData.Packet.TimerTick]
							{
								return FText::Format(LOCTEXT("TimerTick", "Timer Tick: {0}"), TimerTick);
							})
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Visibility_Lambda([&bIsSet = LastPacketData.bIsSet, &TabletContext = LastPacketData.TabletContext]
							{
								return bIsSet && TabletContext && (TabletContext->GetSupportedProperties() & ETabletSupportedProperties::SerialNumber)
								       != ETabletSupportedProperties::None
									       ? EVisibility::Visible
									       : EVisibility::Collapsed;
							})
							.Text_Lambda([&SerialNumber = LastPacketData.Packet.SerialNumber]
							{
								return FText::Format(LOCTEXT("SerialNumber", "Serial Number: {0}"), SerialNumber);
							})
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text_Lambda([&bIsSet = LastPacketData.bIsSet, &PenStatus = LastPacketData.Packet.PenStatus]
							{
								if (bIsSet)
								{
									FString PenStatusStr;
									if (PenStatus == EPenStatus::None)
									{
										PenStatusStr = "None";
									}
									else
									{
										if ((PenStatus & EPenStatus::CursorIsTouching) != EPenStatus::None)
										{
											PenStatusStr += "CursorIsTouching";
										}
										if ((PenStatus & EPenStatus::CursorIsInverted) != EPenStatus::None)
										{
											PenStatusStr += PenStatusStr.IsEmpty() ? "CursorIsInverted" : " & CursorIsInverted";
										}
										if ((PenStatus & EPenStatus::NotUsed) != EPenStatus::None)
										{
											PenStatusStr += PenStatusStr.IsEmpty() ? "NotUsed" : " & NotUsed";
										}
										if ((PenStatus & EPenStatus::BarrelButtonPressed) != EPenStatus::None)
										{
											PenStatusStr += PenStatusStr.IsEmpty() ? "BarrelButtonPressed" : " & BarrelButtonPressed";
										}
									}

									if (PenStatusStr.IsEmpty())
									{
										PenStatusStr = FString::Format(TEXT("Unknown ({0})"), {
											                               static_cast<std::underlying_type_t<EPenStatus>>(PenStatus)
										                               });
									}

									return FText::Format(LOCTEXT("PenStatus", "Pen Status: {0}"), FText::FromString(PenStatusStr));
								}
								return FText();
							})
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Visibility_Lambda([&bIsSet = LastPacketData.bIsSet, &TabletContext = LastPacketData.TabletContext]
							{
								return bIsSet && TabletContext && (TabletContext->GetSupportedProperties() & ETabletSupportedProperties::X)
								       != ETabletSupportedProperties::None
									       ? EVisibility::Visible
									       : EVisibility::Collapsed;
							})
							.Text_Lambda([&X = LastPacketData.Packet.X]
							{
								return FText::Format(LOCTEXT("X", "X: {0}"), X);
							})
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Visibility_Lambda([&bIsSet = LastPacketData.bIsSet, &TabletContext = LastPacketData.TabletContext]
							{
								return bIsSet && TabletContext && (TabletContext->GetSupportedProperties() & ETabletSupportedProperties::Y)
								       != ETabletSupportedProperties::None
									       ? EVisibility::Visible
									       : EVisibility::Collapsed;
							})
							.Text_Lambda([&Y = LastPacketData.Packet.Y]
							{
								return FText::Format(LOCTEXT("Y", "Y: {0}"), Y);
							})
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Visibility_Lambda([&bIsSet = LastPacketData.bIsSet, &TabletContext = LastPacketData.TabletContext]
							{
								return bIsSet && TabletContext && (TabletContext->GetSupportedProperties() & ETabletSupportedProperties::Z)
								       != ETabletSupportedProperties::None
									       ? EVisibility::Visible
									       : EVisibility::Collapsed;
							})
							.Text_Lambda([&Z = LastPacketData.Packet.Z]
							{
								return FText::Format(LOCTEXT("Z", "Z: {0}"), Z);
							})
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Visibility_Lambda([&bIsSet = LastPacketData.bIsSet, &TabletContext = LastPacketData.TabletContext]
							{
								return bIsSet && TabletContext && (TabletContext->GetSupportedProperties() & ETabletSupportedProperties::NormalPressure)
								       != ETabletSupportedProperties::None
									       ? EVisibility::Visible
									       : EVisibility::Collapsed;
							})
							.Text_Lambda([&NormalPressure = LastPacketData.Packet.NormalPressure]
							{
								return FText::Format(LOCTEXT("NormalPressure", "Normal Pressure: {0}"), NormalPressure);
							})
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Visibility_Lambda([&bIsSet = LastPacketData.bIsSet, &TabletContext = LastPacketData.TabletContext]
							{
								return bIsSet && TabletContext && (TabletContext->GetSupportedProperties() & ETabletSupportedProperties::TangentPressure)
								       != ETabletSupportedProperties::None
									       ? EVisibility::Visible
									       : EVisibility::Collapsed;
							})
							.Text_Lambda([&TangentPressure = LastPacketData.Packet.TangentPressure]
							{
								return FText::Format(LOCTEXT("TangentPressure", "Tangent Pressure: {0}"), TangentPressure);
							})
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Visibility_Lambda([&bIsSet = LastPacketData.bIsSet, &TabletContext = LastPacketData.TabletContext]
							{
								return bIsSet && TabletContext && (TabletContext->GetSupportedProperties() & ETabletSupportedProperties::ButtonPressure)
								       != ETabletSupportedProperties::None
									       ? EVisibility::Visible
									       : EVisibility::Collapsed;
							})
							.Text_Lambda([&ButtonPressure = LastPacketData.Packet.ButtonPressure]
							{
								return FText::Format(LOCTEXT("ButtonPressure", "Button Pressure: {0}"), ButtonPressure);
							})
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Visibility_Lambda([&bIsSet = LastPacketData.bIsSet, &TabletContext = LastPacketData.TabletContext]
							{
								return bIsSet && TabletContext && (TabletContext->GetSupportedProperties() & ETabletSupportedProperties::XTiltOrientation)
								       != ETabletSupportedProperties::None
									       ? EVisibility::Visible
									       : EVisibility::Collapsed;
							})
							.Text_Lambda([&XTiltOrientation = LastPacketData.Packet.XTiltOrientation]
							{
								return FText::Format(LOCTEXT("XTiltOrientation", "X Tilt Orientation: {0}"), XTiltOrientation);
							})
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Visibility_Lambda([&bIsSet = LastPacketData.bIsSet, &TabletContext = LastPacketData.TabletContext]
							{
								return bIsSet && TabletContext && (TabletContext->GetSupportedProperties() & ETabletSupportedProperties::YTiltOrientation)
								       != ETabletSupportedProperties::None
									       ? EVisibility::Visible
									       : EVisibility::Collapsed;
							})
							.Text_Lambda([&YTiltOrientation = LastPacketData.Packet.YTiltOrientation]
							{
								return FText::Format(LOCTEXT("YTiltOrientation", "Y Tilt Orientation: {0}"), YTiltOrientation);
							})
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Visibility_Lambda([&bIsSet = LastPacketData.bIsSet, &TabletContext = LastPacketData.TabletContext]
							{
								return bIsSet && TabletContext && (TabletContext->GetSupportedProperties() & ETabletSupportedProperties::AzimuthOrientation)
								       != ETabletSupportedProperties::None
									       ? EVisibility::Visible
									       : EVisibility::Collapsed;
							})
							.Text_Lambda([&AzimuthOrientation = LastPacketData.Packet.AzimuthOrientation]
							{
								return FText::Format(LOCTEXT("AzimuthOrientation", "Azimuth Orientation: {0}"), AzimuthOrientation);
							})
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Visibility_Lambda([&bIsSet = LastPacketData.bIsSet, &TabletContext = LastPacketData.TabletContext]
							{
								return bIsSet && TabletContext && (TabletContext->GetSupportedProperties() & ETabletSupportedProperties::AltitudeOrientation)
								       != ETabletSupportedProperties::None
									       ? EVisibility::Visible
									       : EVisibility::Collapsed;
							})
							.Text_Lambda([&AltitudeOrientation = LastPacketData.Packet.AltitudeOrientation]
							{
								return FText::Format(LOCTEXT("AltitudeOrientation", "Altitude Orientation: {0}"), AltitudeOrientation);
							})
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Visibility_Lambda([&bIsSet = LastPacketData.bIsSet, &TabletContext = LastPacketData.TabletContext]
							{
								return bIsSet && TabletContext && (TabletContext->GetSupportedProperties() & ETabletSupportedProperties::TwistOrientation)
								       != ETabletSupportedProperties::None
									       ? EVisibility::Visible
									       : EVisibility::Collapsed;
							})
							.Text_Lambda([&TwistOrientation = LastPacketData.Packet.TwistOrientation]
							{
								return FText::Format(LOCTEXT("TwistOrientation", "Twist Orientation: {0}"), TwistOrientation);
							})
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Visibility_Lambda([&bIsSet = LastPacketData.bIsSet, &TabletContext = LastPacketData.TabletContext]
							{
								return bIsSet && TabletContext && (TabletContext->GetSupportedProperties() & ETabletSupportedProperties::PitchRotation)
								       != ETabletSupportedProperties::None
									       ? EVisibility::Visible
									       : EVisibility::Collapsed;
							})
							.Text_Lambda([&PitchRotation = LastPacketData.Packet.PitchRotation]
							{
								return FText::Format(LOCTEXT("PitchRotation", "Pitch Rotation: {0}"), PitchRotation);
							})
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Visibility_Lambda([&bIsSet = LastPacketData.bIsSet, &TabletContext = LastPacketData.TabletContext]
							{
								return bIsSet && TabletContext && (TabletContext->GetSupportedProperties() & ETabletSupportedProperties::RollRotation)
								       != ETabletSupportedProperties::None
									       ? EVisibility::Visible
									       : EVisibility::Collapsed;
							})
							.Text_Lambda([&RollRotation = LastPacketData.Packet.RollRotation]
							{
								return FText::Format(LOCTEXT("RollRotation", "Roll Rotation: {0}"), RollRotation);
							})
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Visibility_Lambda([&bIsSet = LastPacketData.bIsSet, &TabletContext = LastPacketData.TabletContext]
							{
								return bIsSet && TabletContext && (TabletContext->GetSupportedProperties() & ETabletSupportedProperties::YawRotation)
								       != ETabletSupportedProperties::None
									       ? EVisibility::Visible
									       : EVisibility::Collapsed;
							})
							.Text_Lambda([&YawRotation = LastPacketData.Packet.YawRotation]
							{
								return FText::Format(LOCTEXT("YawRotation", "Yaw Rotation: {0}"), YawRotation);
							})
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Visibility_Lambda([&bIsSet = LastPacketData.bIsSet, &TabletContext = LastPacketData.TabletContext]
							{
								return bIsSet && TabletContext && (TabletContext->GetSupportedProperties() & ETabletSupportedProperties::Width)
								       != ETabletSupportedProperties::None
									       ? EVisibility::Visible
									       : EVisibility::Collapsed;
							})
							.Text_Lambda([&Width = LastPacketData.Packet.Width]
							{
								return FText::Format(LOCTEXT("Width", "Width: {0}"), Width);
							})
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Visibility_Lambda([&bIsSet = LastPacketData.bIsSet, &TabletContext = LastPacketData.TabletContext]
							{
								return bIsSet && TabletContext && (TabletContext->GetSupportedProperties() & ETabletSupportedProperties::Height)
								       != ETabletSupportedProperties::None
									       ? EVisibility::Visible
									       : EVisibility::Collapsed;
							})
							.Text_Lambda([&Height = LastPacketData.Packet.Height]
							{
								return FText::Format(LOCTEXT("Height", "Height: {0}"), Height);
							})
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Visibility_Lambda([&bIsSet = LastPacketData.bIsSet, &TabletContext = LastPacketData.TabletContext]
							{
								return bIsSet && TabletContext && (TabletContext->GetSupportedProperties() &
									       ETabletSupportedProperties::FingerContactConfidence)
								       != ETabletSupportedProperties::None
									       ? EVisibility::Visible
									       : EVisibility::Collapsed;
							})
							.Text_Lambda([&FingerContactConfidence = LastPacketData.Packet.FingerContactConfidence]
							{
								return FText::Format(LOCTEXT("FingerContactConfidence", "Finger Contact Confidence: {0}"), FingerContactConfidence);
							})
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Visibility_Lambda([&bIsSet = LastPacketData.bIsSet, &TabletContext = LastPacketData.TabletContext]
							{
								return bIsSet && TabletContext && (TabletContext->GetSupportedProperties() & ETabletSupportedProperties::DeviceContactID)
								       != ETabletSupportedProperties::None
									       ? EVisibility::Visible
									       : EVisibility::Collapsed;
							})
							.Text_Lambda([&DeviceContactID = LastPacketData.Packet.DeviceContactID]
							{
								return FText::Format(LOCTEXT("DeviceContactID", "Device Contact ID: {0}"), DeviceContactID);
							})
						]
					]
				]
				+ SSplitter::Slot()
				.Value(0.2f)
				[
					SNew(SMultiLineEditableText)
					.IsReadOnly(true)
					.Text_Lambda([&DebugMessages = DebugMessages] { return FText::FromString(DebugMessages); })
					.VScrollBar(SNew(SScrollBar).Orientation(Orient_Vertical).AlwaysShowScrollbar(true).Thickness(10))
				]
			];
	}

	void SStylusInputDebugWidget::AcquireStylusInput()
	{
		if (StylusInput)
		{
			LogWarning("Stylus input instance has already been acquired.");
			return;
		}

		check(FSlateApplication::IsInitialized());
		const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared());

		if (!Window.IsValid())
		{
			LogError("Could not find widget window; stylus input instance has not been acquired.");
			return;
		}

		StylusInput = CreateInstance(*Window);
		if (!StylusInput)
		{
			LogError("Could not acquire stylus input instance.");
		}
	}

	void SStylusInputDebugWidget::ReleaseStylusInput()
	{
		if (EventHandler)
		{
			LogWarning("Event handler is still registered.");
		}

		if (StylusInput)
		{
			if (!ReleaseInstance(StylusInput))
			{
				LogError("Failed to release stylus input for StylusInput Debug Widget.");
			}

			StylusInput = nullptr;
		}
	}

	void SStylusInputDebugWidget::RegisterEventHandler()
	{
		if (!StylusInput)
		{
			LogWarning("Cannot register event handler since stylus input is unavailable.");
			return;
		}

		if (EventHandler)
		{
			LogWarning("Event handler is not null; please unregister event handler before trying to registering it again.");
		}

		if (EventHandlerThread == EEventHandlerThread::Asynchronous)
		{
			EventHandler = MakeUnique<FDebugEventHandlerAsynchronous>(FOnPacketCallback::CreateRaw(this, &SStylusInputDebugWidget::OnPacket),
			                                                          FOnDebugEventCallback::CreateRaw(this, &SStylusInputDebugWidget::OnDebugEvent));
		}
		else
		{
			EventHandler = MakeUnique<FDebugEventHandlerOnGameThread>(FOnPacketCallback::CreateRaw(this, &SStylusInputDebugWidget::OnPacket),
			                                                          FOnDebugEventCallback::CreateRaw(this, &SStylusInputDebugWidget::OnDebugEvent));
		}

		if (StylusInput->AddEventHandler(EventHandler.Get(), EventHandlerThread))
		{
			LogVerbose(EventHandlerThread == EEventHandlerThread::Asynchronous
				           ? "Registered event handler on asynchronous thread."
				           : "Registered event handler on game thread.");
		}
		else
		{
			LogError("Failed to register event handler.");
		}
	}

	void SStylusInputDebugWidget::UnregisterEventHandler()
	{
		if (!EventHandler)
		{
			LogWarning("Cannot unregister event handler since it is invalid.");
			return;
		}

		if (StylusInput)
		{
			if (StylusInput->RemoveEventHandler(EventHandler.Get()))
			{
				LogVerbose("Unregistered event handler for StylusInput Debug Widget.");
			}
			else
			{
				LogError("Failed to unregister event handler for StylusInput Debug Widget.");
			}
		}
		else
		{
			LogWarning("Cannot unregister event handler since stylus input is unavailable.");
		}

		EventHandler.Reset();
	}

	FDebugEventHandlerAsynchronous::FDebugEventHandlerAsynchronous(FOnPacketCallback&& OnPacketCallback, FOnDebugEventCallback&& OnDebugEventCallback)
		: OnPacketCallback(MoveTemp(OnPacketCallback)), OnDebugEventCallback(MoveTemp(OnDebugEventCallback))
	{
		check(this->OnPacketCallback.IsBound());
		check(this->OnDebugEventCallback.IsBound());
	}

	void FDebugEventHandlerAsynchronous::OnPacket(const FStylusInputPacket& Packet)
	{
		PacketQueue.Enqueue(Packet);
	}

	void FDebugEventHandlerAsynchronous::OnDebugEvent(const FString& Message)
	{
		DebugEventQueue.Enqueue(Message);
	}

	void FDebugEventHandlerAsynchronous::Tick(float DeltaTime)
	{
		FStylusInputPacket Packet;
		while (PacketQueue.Dequeue(Packet))
		{
			OnPacketCallback.Execute(Packet);
		}

		FString Message;
		while (DebugEventQueue.Dequeue(Message))
		{
			OnDebugEventCallback.Execute(Message);
		}
	}

	FDebugEventHandlerOnGameThread::FDebugEventHandlerOnGameThread(FOnPacketCallback&& OnPacketCallback, FOnDebugEventCallback&& OnDebugEventCallback)
		: OnPacketCallback(MoveTemp(OnPacketCallback)), OnDebugEventCallback(MoveTemp(OnDebugEventCallback))
	{
		check(this->OnPacketCallback.IsBound());
		check(this->OnDebugEventCallback.IsBound());
	}

	void FDebugEventHandlerOnGameThread::OnPacket(const FStylusInputPacket& Packet)
	{
		OnPacketCallback.Execute(Packet);
	}

	void FDebugEventHandlerOnGameThread::OnDebugEvent(const FString& Message)
	{
		OnDebugEventCallback.Execute(Message);
	}

	SStylusInputDebugWidget::~SStylusInputDebugWidget()
	{
		UnregisterEventHandler();
		ReleaseStylusInput();
	}

	void SStylusInputDebugWidget::OnPacket(const FStylusInputPacket& Packet)
	{
		LastPacketData.bIsSet = true;
		LastPacketData.Packet = Packet;

		if (!LastPacketData.TabletContext || (LastPacketData.TabletContext && LastPacketData.TabletContext->GetID() != Packet.TabletContextID))
		{
			LastPacketData.TabletContext = GetTabletContext(Packet.TabletContextID);
		}

		if (!LastPacketData.StylusInfo || (LastPacketData.StylusInfo && LastPacketData.StylusInfo->GetID() != Packet.CursorID))
		{
			LastPacketData.StylusInfo = GetStylusInfo(Packet.CursorID);
		}

		if (PaintWidget)
		{
			PaintWidget->Add(Packet);
		}
	}

	void SStylusInputDebugWidget::OnDebugEvent(const FString& Message)
	{
		if (DebugMessages.IsEmpty())
		{
			DebugMessages = Message;
		}
		else
		{
			DebugMessages += "\n" + Message;
		}
	}

	TSharedRef<SWidget> SStylusInputDebugWidget::GetEventHandlerThreadMenu()
	{
		FMenuBuilder MenuBuilder(true, nullptr);

		MenuBuilder.AddMenuEntry(LOCTEXT("Asynchronous", "Asynchronous"),
		                         LOCTEXT("AsynchronousTooltip", "Event handler is evaluated on a dedicated thread"),
		                         FSlateIcon(),
		                         FUIAction(FExecuteAction::CreateLambda([this]
		                                   {
			                                   SetEventHandlerThread(EEventHandlerThread::Asynchronous);
		                                   }),
		                                   FCanExecuteAction(),
		                                   FIsActionChecked::CreateLambda([&EventHandlerThread = EventHandlerThread]
		                                   {
			                                   return EventHandlerThread == EEventHandlerThread::Asynchronous;
		                                   })),
		                         NAME_None,
		                         EUserInterfaceActionType::RadioButton);

		MenuBuilder.AddMenuEntry(LOCTEXT("GameThread", "On Game Thread"),
		                         LOCTEXT("GameThreadTooltip", "Event handler is evaluated on the game thread"),
		                         FSlateIcon(),
		                         FUIAction(FExecuteAction::CreateLambda([this]
		                                   {
			                                   SetEventHandlerThread(EEventHandlerThread::OnGameThread);
		                                   }),
		                                   FCanExecuteAction(),
		                                   FIsActionChecked::CreateLambda([&EventHandlerThread = EventHandlerThread]
		                                   {
			                                   return EventHandlerThread == EEventHandlerThread::OnGameThread;
		                                   })),
		                         NAME_None,
		                         EUserInterfaceActionType::RadioButton);

		return MenuBuilder.MakeWidget();
	}

	void SStylusInputDebugWidget::SetEventHandlerThread(EEventHandlerThread InEventHandlerThread)
	{
		if (EventHandlerThread != InEventHandlerThread)
		{
			UnregisterEventHandler();

			EventHandlerThread = InEventHandlerThread;

			RegisterEventHandler();
		}
	}

	const IStylusInputTabletContext* SStylusInputDebugWidget::GetTabletContext(uint32 TabletContextID)
	{
		if (!StylusInput)
		{
			return nullptr;
		}

		const TSharedPtr<IStylusInputTabletContext>* TabletContext = TabletContexts.Find(TabletContextID);
		if (!TabletContext)
		{
			if (const TSharedPtr<IStylusInputTabletContext>& NewTabletContext = StylusInput->GetTabletContext(TabletContextID))
			{
				TabletContext = &TabletContexts.Emplace(TabletContextID, NewTabletContext);
			}
		}

		return TabletContext ? TabletContext->Get() : nullptr;
	}

	const IStylusInputStylusInfo* SStylusInputDebugWidget::GetStylusInfo(uint32 StylusID)
	{
		if (!StylusInput)
		{
			return nullptr;
		}

		const TSharedPtr<IStylusInputStylusInfo>* StylusInfo = StylusInfos.Find(StylusID);
		if (!StylusInfo)
		{
			if (const TSharedPtr<IStylusInputStylusInfo>& NewStylusInfo = StylusInput->GetStylusInfo(StylusID))
			{
				StylusInfo = &StylusInfos.Emplace(StylusID, NewStylusInfo);
			}
			else
			{
				return nullptr;
			}
		}

		return StylusInfo->Get();
	}
}

#undef LOCTEXT_NAMESPACE
