// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundDetailCustomization.h"

#include "Containers/Set.h"
#include "Delegates/Delegate.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IAudioParameterInterfaceRegistry.h"
#include "IAudioParameterTransmitter.h"
#include "IDetailGroup.h"
#include "Input/Events.h"
#include "Interfaces/MetasoundFrontendInterfaceRegistry.h"
#include "MetasoundAssetBase.h"
#include "MetasoundBuilderSubsystem.h"
#include "MetasoundDocumentBuilderRegistry.h"
#include "MetasoundEditor.h"
#include "MetasoundEditorGraphBuilder.h"
#include "MetasoundEditorSettings.h"
#include "MetasoundEditorSubsystem.h"
#include "MetasoundFrontend.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendSearchEngine.h"
#include "MetasoundSettings.h"
#include "MetasoundSource.h"
#include "MetasoundUObjectRegistry.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorDelegates.h"
#include "PropertyHandle.h"
#include "PropertyRestriction.h"
#include "SGraphPalette.h"
#include "ScopedTransaction.h"
#include "Sound/SoundWave.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"
#include "Templates/Casts.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "MetaSoundEditor"


namespace Metasound::Editor
{
	FName BuildChildPath(const FString& InBasePath, FName InPropertyName)
	{
		return FName(InBasePath + TEXT(".") + InPropertyName.ToString());
	}

	FName BuildChildPath(const FName& InBasePath, FName InPropertyName)
	{
		return FName(InBasePath.ToString() + TEXT(".") + InPropertyName.ToString());
	}

	UObject* FMetaSoundDetailCustomizationBase::GetMetaSound() const
	{
		if (Builder.IsValid())
		{
			const FMetaSoundFrontendDocumentBuilder& DocBuilder = Builder->GetBuilder();
			if (DocBuilder.IsValid())
			{
				return &DocBuilder.CastDocumentObjectChecked<UObject>();
			}
		}

		return nullptr;
	}

	void FMetaSoundDetailCustomizationBase::InitBuilder(UObject& MetaSound)
	{
		using namespace Engine;
		Builder.Reset(&FDocumentBuilderRegistry::GetChecked().FindOrBeginBuilding(MetaSound));
	}

	bool FMetaSoundDetailCustomizationBase::IsGraphEditable() const
	{
		using namespace Engine;

		if (Builder.IsValid())
		{
			const FMetaSoundFrontendDocumentBuilder& DocBuilder = Builder->GetConstBuilder();
			if (DocBuilder.IsValid())
			{
				return DocBuilder.FindConstBuildGraphChecked().Style.bIsGraphEditable;
			}
		}

		return false;
	}

	FMetasoundDetailCustomization::FMetasoundDetailCustomization(FName InDocumentPropertyName)
		: DocumentPropertyName(InDocumentPropertyName)
	{
	}

	FName FMetasoundDetailCustomization::GetInterfaceVersionsPropertyPath() const
	{
		return BuildChildPath(DocumentPropertyName, GET_MEMBER_NAME_CHECKED(FMetasoundFrontendDocument, Interfaces));
	}

	FName FMetasoundDetailCustomization::GetRootClassPropertyPath() const
	{
		return BuildChildPath(DocumentPropertyName, GET_MEMBER_NAME_CHECKED(FMetasoundFrontendDocument, RootGraph));
	}

	FName FMetasoundDetailCustomization::GetMetadataPropertyPath() const
	{
		const FName RootClass = FName(GetRootClassPropertyPath());
		return BuildChildPath(RootClass, GET_MEMBER_NAME_CHECKED(FMetasoundFrontendClass, Metadata));
	}

	void FMetasoundDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
	{
		using namespace Frontend;

		TArray<TWeakObjectPtr<UObject>> Objects;
		DetailLayout.GetObjectsBeingCustomized(Objects);

		// Only support modifying a single MetaSound at a time (Multiple
		// MetaSound editing will be covered most likely by separate tool).
		if (Objects.Num() > 1 || !Objects.Last().IsValid())
		{
			return;
		}

		UObject& MetaSound = *Objects.Last().Get();
		InitBuilder(MetaSound);
		TWeakObjectPtr<UMetaSoundSource> MetaSoundSource = Cast<UMetaSoundSource>(&MetaSound);

		// MetaSound patches don't have source settings, so view MetaSound settings by default 
		EMetasoundActiveDetailView DetailsView = EMetasoundActiveDetailView::Metasound;
		if (MetaSoundSource.IsValid())
		{
			// Show source settings by default unless previously set
			DetailsView = EMetasoundActiveDetailView::General;
			if (const UMetasoundEditorSettings* EditorSettings = GetDefault<UMetasoundEditorSettings>())
			{
				DetailsView = EditorSettings->DetailView;
			}
		}

		switch (DetailsView)
		{
			case EMetasoundActiveDetailView::Metasound:
			{
				IDetailCategoryBuilder& GeneralCategoryBuilder = DetailLayout.EditCategory("MetaSound");
				const FName AuthorPropertyPath = BuildChildPath(GetMetadataPropertyPath(), FMetasoundFrontendClassMetadata::GetAuthorPropertyName());
				const FName CategoryHierarchyPropertyPath = BuildChildPath(GetMetadataPropertyPath(), FMetasoundFrontendClassMetadata::GetCategoryHierarchyPropertyName());
				const FName ClassNamePropertyPath = BuildChildPath(GetMetadataPropertyPath(), FMetasoundFrontendClassMetadata::GetClassNamePropertyName());
				const FName DescPropertyPath = BuildChildPath(GetMetadataPropertyPath(), FMetasoundFrontendClassMetadata::GetDescriptionPropertyName());
				const FName DisplayNamePropertyPath = BuildChildPath(GetMetadataPropertyPath(), FMetasoundFrontendClassMetadata::GetDisplayNamePropertyName());
				const FName KeywordsPropertyPath = BuildChildPath(GetMetadataPropertyPath(), FMetasoundFrontendClassMetadata::GetKeywordsPropertyName());
				const FName IsDeprecatedPropertyPath = BuildChildPath(GetMetadataPropertyPath(), FMetasoundFrontendClassMetadata::GetIsDeprecatedPropertyName());
				const FName VersionPropertyPath = BuildChildPath(GetMetadataPropertyPath(), FMetasoundFrontendClassMetadata::GetVersionPropertyName());

				const FName ClassNameNamePropertyPath = BuildChildPath(ClassNamePropertyPath, GET_MEMBER_NAME_CHECKED(FMetasoundFrontendClassName, Name));

				const FName MajorVersionPropertyPath = BuildChildPath(VersionPropertyPath, GET_MEMBER_NAME_CHECKED(FMetasoundFrontendVersionNumber, Major));
				const FName MinorVersionPropertyPath = BuildChildPath(VersionPropertyPath, GET_MEMBER_NAME_CHECKED(FMetasoundFrontendVersionNumber, Minor));

				const FName InterfaceVersionsPropertyPath = GetInterfaceVersionsPropertyPath();

				TSharedPtr<IPropertyHandle> AuthorHandle = DetailLayout.GetProperty(AuthorPropertyPath);
				TSharedPtr<IPropertyHandle> CategoryHierarchyHandle = DetailLayout.GetProperty(CategoryHierarchyPropertyPath);
				TSharedPtr<IPropertyHandle> ClassNameHandle = DetailLayout.GetProperty(ClassNameNamePropertyPath);
				TSharedPtr<IPropertyHandle> DisplayNameHandle = DetailLayout.GetProperty(DisplayNamePropertyPath);
				TSharedPtr<IPropertyHandle> DescHandle = DetailLayout.GetProperty(DescPropertyPath);
				TSharedPtr<IPropertyHandle> KeywordsHandle = DetailLayout.GetProperty(KeywordsPropertyPath);
				TSharedPtr<IPropertyHandle> IsDeprecatedHandle = DetailLayout.GetProperty(IsDeprecatedPropertyPath);
				TSharedPtr<IPropertyHandle> InterfaceVersionsHandle = DetailLayout.GetProperty(InterfaceVersionsPropertyPath);
				TSharedPtr<IPropertyHandle> MajorVersionHandle = DetailLayout.GetProperty(MajorVersionPropertyPath);
				TSharedPtr<IPropertyHandle> MinorVersionHandle = DetailLayout.GetProperty(MinorVersionPropertyPath);

				// Invalid for UMetaSounds
				TSharedPtr<IPropertyHandle> OutputFormat = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaSoundSource, OutputFormat));
				if (OutputFormat.IsValid())
				{
					if (MetaSoundSource.IsValid())
					{
						OutputFormat->SetOnPropertyValuePreChange(FSimpleDelegate::CreateLambda([Source = MetaSoundSource]()
						{
							if (Source.IsValid())
							{
								TSharedPtr<FEditor> ParentEditor = FGraphBuilder::GetEditorForMetasound(*Source.Get());
								if (ParentEditor.IsValid())
								{
									ParentEditor->Stop();
								};
							}
						}));

						OutputFormat->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([Source = MetaSoundSource]()
						{
							if (Source.IsValid())
							{
								TSharedPtr<FEditor> ParentEditor = FGraphBuilder::GetEditorForMetasound(*Source.Get());
								if (ParentEditor.IsValid())
								{
									ParentEditor->CreateAnalyzers(*Source.Get());
								};
							}
						}));
					}

					TSharedRef<SWidget> OutputFormatValueWidget = OutputFormat->CreatePropertyValueWidget();
					OutputFormatValueWidget->SetEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FMetaSoundDetailCustomizationBase::IsGraphEditable)));

					static const FText OutputFormatName = LOCTEXT("MetasoundOutputFormatPropertyName", "Output Format");
					GeneralCategoryBuilder.AddCustomRow(OutputFormatName)
					.NameContent()
					[
						OutputFormat->CreatePropertyNameWidget()
					]
					.ValueContent()
					[
						OutputFormatValueWidget
					];

					OutputFormat->MarkHiddenByCustomization();
				}

				// Updates FText properties on open editors if required
				{
					FSimpleDelegate RegisterOnChange = FSimpleDelegate::CreateLambda([this]()
					{
						if (Builder.IsValid())
						{
							FMetaSoundFrontendDocumentBuilder& DocBuilder = Builder->GetBuilder();
							if (DocBuilder.IsValid())
							{
								DocBuilder.GetConstDocumentChecked().RootGraph.Style.UpdateChangeID();
							}
							constexpr bool bForceViewSynchronization = true;
							FGraphBuilder::RegisterGraphWithFrontend(DocBuilder.CastDocumentObjectChecked<UObject>(), bForceViewSynchronization);
						}
					});
					AuthorHandle->SetOnChildPropertyValueChanged(RegisterOnChange);
					DescHandle->SetOnPropertyValueChanged(RegisterOnChange);
					DisplayNameHandle->SetOnPropertyValueChanged(RegisterOnChange);
					KeywordsHandle->SetOnPropertyValueChanged(RegisterOnChange);
					KeywordsHandle->SetOnChildPropertyValueChanged(RegisterOnChange);
					IsDeprecatedHandle->SetOnPropertyValueChanged(RegisterOnChange);
				}

				GeneralCategoryBuilder.AddProperty(DisplayNameHandle);
				GeneralCategoryBuilder.AddProperty(DescHandle);
				GeneralCategoryBuilder.AddProperty(AuthorHandle);
				GeneralCategoryBuilder.AddProperty(IsDeprecatedHandle);
				GeneralCategoryBuilder.AddProperty(MajorVersionHandle);
				GeneralCategoryBuilder.AddProperty(MinorVersionHandle);

				static const FText ClassGuidName = LOCTEXT("MetasoundClassGuidPropertyName", "Class Guid");
				GeneralCategoryBuilder.AddCustomRow(ClassGuidName).NameContent()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(ClassGuidName)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					]
				]
				.ValueContent()
				[
					ClassNameHandle->CreatePropertyValueWidget()
				];
				GeneralCategoryBuilder.AddProperty(CategoryHierarchyHandle);
				GeneralCategoryBuilder.AddProperty(KeywordsHandle);

				DetailLayout.HideCategory("Attenuation");
				DetailLayout.HideCategory("Developer");
				DetailLayout.HideCategory("Effects");
				DetailLayout.HideCategory("Loading");
				DetailLayout.HideCategory("Modulation");
				DetailLayout.HideCategory("Sound");
				DetailLayout.HideCategory("Voice Management");
			}
			break;

			case EMetasoundActiveDetailView::General:
			default:
				DetailLayout.HideCategory("MetaSound");

				TArray<TSharedRef<IPropertyHandle>>DeveloperProperties;
				TArray<TSharedRef<IPropertyHandle>>SoundProperties;

				DetailLayout.EditCategory("Sound")
					.GetDefaultProperties(SoundProperties);
				DetailLayout.EditCategory("Developer")
					.GetDefaultProperties(DeveloperProperties);

				auto HideProperties = [](const TSet<FName>& PropsToHide, const TArray<TSharedRef<IPropertyHandle>>& Properties)
				{
					for (TSharedRef<IPropertyHandle> Property : Properties)
					{
						if (PropsToHide.Contains(Property->GetProperty()->GetFName()))
						{
							Property->MarkHiddenByCustomization();
						}
					}
				};

				static const TSet<FName> SoundPropsToHide =
				{
					GET_MEMBER_NAME_CHECKED(USoundWave, bLooping),
					GET_MEMBER_NAME_CHECKED(USoundWave, SoundGroup)
				};
				HideProperties(SoundPropsToHide, SoundProperties);

				static const TSet<FName> DeveloperPropsToHide =
				{
					GET_MEMBER_NAME_CHECKED(USoundBase, Duration),
					GET_MEMBER_NAME_CHECKED(USoundBase, MaxDistance),
					GET_MEMBER_NAME_CHECKED(USoundBase, TotalSamples)
				};
				HideProperties(DeveloperPropsToHide, DeveloperProperties);

				break;
		}

		// Hack to hide parent structs for nested metadata properties
		DetailLayout.HideCategory("CustomView");

		DetailLayout.HideCategory("Analysis");
		DetailLayout.HideCategory("Curves");
		DetailLayout.HideCategory("File Path");
		DetailLayout.HideCategory("Format");
		DetailLayout.HideCategory("Info");
		DetailLayout.HideCategory("Loading");
		DetailLayout.HideCategory("Playback");
		DetailLayout.HideCategory("Subtitles");
		DetailLayout.HideCategory("Waveform Processing");
	}

	FMetasoundPagesDetailCustomization::FMetasoundPagesDetailCustomization()
		: ItemName(TEXT("Page"))
	{
	}

	void FMetasoundPagesDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
	{
		using namespace Engine;
		using namespace Frontend;

		TArray<TWeakObjectPtr<UObject>> Objects;
		DetailLayout.GetObjectsBeingCustomized(Objects);

		// Only support modifying a single MetaSound at a time (Multiple
		// MetaSound editing will be covered most likely by separate tool).
		if (Objects.Num() > 1)
		{
			return;
		}

		if (UMetaSoundSettings* Settings = GetMutableDefault<UMetaSoundSettings>())
		{
			Settings->GetOnPageSettingsUpdatedDelegate().AddSPLambda(this, [this]()
			{
				UpdateItemNames();
				if (ComboBox.IsValid())
				{
					ComboBox->RefreshOptions();
				}
			});
		}

		SAssignNew(ComboBox, SSearchableComboBox)
			.OptionsSource(&AddableItems)
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem)
			{
				return SNew(STextBlock).Text(FText::FromString(*InItem));
			})
			.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NameToAdd, ESelectInfo::Type InSelectInfo)
			{
				using namespace Engine;
				using namespace Frontend;

				if (InSelectInfo != ESelectInfo::OnNavigation)
				{
					UObject& MetaSound = GetMetaSound();

					const FScopedTransaction Transaction(FText::Format(LOCTEXT("AddPageTransactionFormat", "Add MetaSound Page '{0}'"), FText::FromString(*NameToAdd)));
					MetaSound.Modify();

					// Underlying DocBuilder's pageID is a property that is tracked by transaction stack, so signal as modifying behavior
					Builder->Modify();

					constexpr bool bDuplicateLastGraph = true;
					constexpr bool bSetAsBuildGraph = true;

					EMetaSoundBuilderResult Result = EMetaSoundBuilderResult::Failed;
					Builder->AddGraphPage(FName(*NameToAdd.Get()), bDuplicateLastGraph, bSetAsBuildGraph, Result);
				}
			})
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("UpdatePageAction", "Add Page..."))
				.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FMetaSoundDetailCustomizationBase::IsGraphEditable)))
			];

		TSharedRef<SWidget> Utilities = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2.0f)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				ComboBox->AsShared()
			]
			+ SHorizontalBox::Slot()
			.Padding(2.0f)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				PropertyCustomizationHelpers::MakeDeleteButton(FSimpleDelegate::CreateLambda([this]()
				{
					using namespace Frontend;
					UObject& MetaSound = GetMetaSound();

					const FScopedTransaction Transaction(LOCTEXT("RemoveAllPagesTransaction", "Remove All MetaSound Pages"));
					MetaSound.Modify();

					// Underlying DocBuilder's pageID is a property that is tracked by transaction stack, so signal as modifying behavior
					Builder->Modify();
					Builder->RemoveAllGraphPages();

					UpdateItemNames();
					ComboBox->RefreshOptions();
					FGraphBuilder::RegisterGraphWithFrontend(MetaSound);
				}), LOCTEXT("RemoveImplementablePagesTooltip1", "Removes all pages from the given MetaSound."))
			];

		Utilities->SetEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FMetaSoundDetailCustomizationBase::IsGraphEditable)));

		const FText HeaderName = LOCTEXT("PagesGroupDisplayName", "Pages");
		IDetailCategoryBuilder& Category = DetailLayout.EditCategory(FName(ItemName), HeaderName);
		Category.AddCustomRow(HeaderName) [ Utilities ];

		EntryWidgets = SNew(SVerticalBox);
		Category.AddCustomRow(LOCTEXT("ImplementedPagesLabel", "Implemented Pages"))
		[
			EntryWidgets->AsShared()
		];

		// Registration of page listener instance calls OnReload which in turn causes RefreshView, so no need to call directly
		if (UMetasoundEditorViewBase* View = CastChecked<UMetasoundEditorViewBase>(Objects.Last()))
		{
			if (UObject* MetaSound = View->GetMetasound())
			{
				InitBuilder(*MetaSound);
				PageListener = MakeShared<FPageListener>(StaticCastSharedRef<FMetasoundPagesDetailCustomization>(AsShared()));
				Builder->AddTransactionListener(PageListener->AsShared());
			}
		}
	}

	UObject& FMetasoundPagesDetailCustomization::GetMetaSound() const
	{
		return Builder->GetBuilder().CastDocumentObjectChecked<UObject>();
	}

	void FMetasoundPagesDetailCustomization::RebuildImplemented()
	{
		EntryWidgets->ClearChildren();

		auto CreateEntryWidget = [this](bool bIsDefault, FName InName) -> TSharedRef<SWidget>
		{
			using namespace Frontend;

			TSharedRef<SWidget> SelectButtonWidget = PropertyCustomizationHelpers::MakeUseSelectedButton(FSimpleDelegate::CreateLambda([this, InName]()
			{
				EMetaSoundBuilderResult Result = EMetaSoundBuilderResult::Failed;
				constexpr bool bFocusEditor = false; // Already focused by user action
				UMetaSoundEditorSubsystem::GetConstChecked().SetFocusedPage(Builder.Get(), InName, bFocusEditor, Result);
				if (Result == EMetaSoundBuilderResult::Succeeded)
				{
					BuildPageName = InName;
				}
			}),
			TAttribute<FText>::Create([this, InName]()
			{
				return BuildPageName == InName
					? LOCTEXT("FocusedPageTooltip", "Currently focused page.")
					: LOCTEXT("SetFocusedPageTooltip", "Sets the actively focused graph page of the MetaSound.");
			}),
			TAttribute<bool>::Create([this, InName]()
			{
				return BuildPageName != InName;
			}));

			TSharedRef<SHorizontalBox> EntryWidget = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(2.0f)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(FText::FromName(InName))
				];

			if (!bIsDefault)
			{
				TSharedRef<SWidget> RemoveButtonWidget = PropertyCustomizationHelpers::MakeDeleteButton(FSimpleDelegate::CreateLambda([this, InName]()
				{
					using namespace Frontend;
					const FScopedTransaction Transaction(FText::Format(LOCTEXT("RemovePageTransactionFormat", "Remove MetaSound Page '{0}'"), FText::FromName(InName)));
					UObject& MetaSound = GetMetaSound();
					MetaSound.Modify();

					// Removal may modify the builder's build page ID if it is the currently set value
					Builder->Modify();

					EMetaSoundBuilderResult Result = EMetaSoundBuilderResult::Failed;
					Builder->RemoveGraphPage(InName, Result);
					if (Result == EMetaSoundBuilderResult::Succeeded)
					{
						UpdateItemNames();
						ComboBox->RefreshOptions();
					}
				}), LOCTEXT("RemovePageTooltip2", "Removes the associated page from the MetaSound."));
				EntryWidget->AddSlot()
					.Padding(2.0f)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.AutoWidth()[RemoveButtonWidget];
			}

			EntryWidget->AddSlot()
				.Padding(2.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth() [ SelectButtonWidget ];

			EntryWidget->SetEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FMetaSoundDetailCustomizationBase::IsGraphEditable)));
			return EntryWidget;
		};

		const UMetaSoundSettings* Settings = GetDefault<UMetaSoundSettings>();
		check(Settings);
		for (const FMetaSoundPageSettings& PageSettings : Settings->GetPageSettings())
		{
			if (ImplementedNames.Contains(PageSettings.Name))
			{
				EntryWidgets->AddSlot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.AutoHeight()
				[
					CreateEntryWidget(!PageSettings.UniqueId.IsValid(), PageSettings.Name)
				];
			}
		}
	}

	void FMetasoundPagesDetailCustomization::RefreshView()
	{
		if (Builder.IsValid())
		{
			const UMetaSoundSettings* Settings = GetDefault<UMetaSoundSettings>();
			check(Settings);
			FMetaSoundFrontendDocumentBuilder& DocBuilder = Builder->GetBuilder();
			const FGuid& PageID = DocBuilder.GetBuildPageID();
			if (const FMetaSoundPageSettings* PageSettings = Settings->FindPageSettings(PageID))
			{
				BuildPageName = PageSettings->Name;
			}
			else
			{
				constexpr bool bFocusEditor = false; // Already focused by user action
				EMetaSoundBuilderResult Result = EMetaSoundBuilderResult::Failed;
				UMetaSoundEditorSubsystem::GetConstChecked().SetFocusedPage(Builder.Get(), Frontend::DefaultPageName, bFocusEditor, Result);
				if (Result == EMetaSoundBuilderResult::Succeeded)
				{
					BuildPageName = Frontend::DefaultPageName;
				}
			}
		}
		else
		{
			BuildPageName = Frontend::DefaultPageName;
		}

		UpdateItemNames();
		ComboBox->RefreshOptions();
		RebuildImplemented();
	}

	void FMetasoundPagesDetailCustomization::UpdateItemNames()
	{
		using namespace Frontend;

		AddableItems.Reset();
		ImplementedNames.Reset();

		const UMetaSoundSettings* Settings = GetDefault<UMetaSoundSettings>();
		check(Settings);

		TSet<FGuid> ImplementedGuids { FGuid() }; // Default "no guid" is always implemented (base graph for back compat & cook safety to ensure there's always at least one)

		const FMetasoundFrontendDocument& Document = Builder->GetBuilder().GetConstDocumentChecked();
		Document.RootGraph.IterateGraphPages([&ImplementedGuids](const FMetasoundFrontendGraph& Graph)
		{
			ImplementedGuids.Add(Graph.PageID);
		});

		auto GetStringName = [](const FMetaSoundPageSettings& Page) -> TSharedPtr<FString> { return MakeShared<FString>(Page.Name.ToString()); };
		auto RemoveImplementedItem = [&ImplementedGuids](const FMetaSoundPageSettings& Page) { return !ImplementedGuids.Contains(Page.UniqueId); };
		Algo::TransformIf(Settings->GetPageSettings(), AddableItems, RemoveImplementedItem, GetStringName);

		auto GetPageName = [&Settings](const FGuid& PageID)
		{
			const FMetaSoundPageSettings* Page = Settings->FindPageSettings(PageID);
			if (ensure(Page))
			{
				return Page->Name;
			}

			return FName();
		};

		Algo::Transform(ImplementedGuids, ImplementedNames, GetPageName);
	}

	void FMetasoundPagesDetailCustomization::FPageListener::OnBuilderReloaded(Frontend::FDocumentModifyDelegates& OutDelegates)
	{
		if (TSharedPtr<FMetasoundPagesDetailCustomization> ParentPtr = Parent.Pin())
		{
			ParentPtr->RefreshView();
		}

		OutDelegates.PageDelegates.OnPageAdded.AddSP(this, &FPageListener::OnPageAdded);
		OutDelegates.PageDelegates.OnPageSet.AddSP(this, &FPageListener::OnPageSet);
		OutDelegates.PageDelegates.OnRemovingPage.AddSP(this, &FPageListener::OnRemovingPage);
	}

	void FMetasoundPagesDetailCustomization::FPageListener::OnPageAdded(const Frontend::FDocumentMutatePageArgs& Args)
	{
		if (TSharedPtr<FMetasoundPagesDetailCustomization> ParentPtr = Parent.Pin())
		{
			const UMetaSoundSettings* Settings = GetDefault<UMetaSoundSettings>();
			check(Settings);
			if (const FMetaSoundPageSettings* PageSettings = Settings->FindPageSettings(Args.PageID))
			{
				if (PageSettings->Name != ParentPtr->BuildPageName)
				{
					ParentPtr->BuildPageName = PageSettings->Name;
					FGraphBuilder::RegisterGraphWithFrontend(ParentPtr->GetMetaSound());
				}

				ParentPtr->AddableItems.RemoveAll([&PageSettings](const TSharedPtr<FString>& Item) { return Item->Compare(PageSettings->Name.ToString()) == 0; });
				ParentPtr->ImplementedNames.Add(PageSettings->Name);
				ParentPtr->ComboBox->RefreshOptions();
				ParentPtr->RebuildImplemented();
			}
		}
	}

	void FMetasoundPagesDetailCustomization::FPageListener::OnPageSet(const Frontend::FDocumentMutatePageArgs& Args)
	{
		if (TSharedPtr<FMetasoundPagesDetailCustomization> ParentPtr = Parent.Pin())
		{
			const UMetaSoundSettings* Settings = GetDefault<UMetaSoundSettings>();
			check(Settings);
			if (const FMetaSoundPageSettings* PageSettings = Settings->FindPageSettings(Args.PageID))
			{
				ParentPtr->BuildPageName = PageSettings->Name;
				ParentPtr->ComboBox->RefreshOptions();
				ParentPtr->RebuildImplemented();
			}
		}
	}

	void FMetasoundPagesDetailCustomization::FPageListener::OnRemovingPage(const Frontend::FDocumentMutatePageArgs& Args)
	{
		if (TSharedPtr<FMetasoundPagesDetailCustomization> ParentPtr = Parent.Pin())
		{
			const UMetaSoundSettings* Settings = GetDefault<UMetaSoundSettings>();
			check(Settings);
			if (const FMetaSoundPageSettings* PageSettings = Settings->FindPageSettings(Args.PageID))
			{
				if (PageSettings->Name != ParentPtr->BuildPageName)
				{
					ParentPtr->BuildPageName = PageSettings->Name;
					FGraphBuilder::RegisterGraphWithFrontend(ParentPtr->GetMetaSound());
				}

				ParentPtr->AddableItems.Add(MakeShared<FString>(PageSettings->Name.ToString()));
				ParentPtr->ImplementedNames.Remove(PageSettings->Name);
				ParentPtr->ComboBox->RefreshOptions();
				ParentPtr->RebuildImplemented();
			}
		}
	}

	void FMetasoundInterfacesDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
	{
		TArray<TWeakObjectPtr<UObject>> Objects;
		DetailLayout.GetObjectsBeingCustomized(Objects);

		// Only support modifying a single MetaSound at a time (Multiple
		// MetaSound editing will be covered most likely by separate tool).
		if (Objects.Num() > 1)
		{
			return;
		}
		if (UMetasoundInterfacesView* InterfacesView = CastChecked<UMetasoundInterfacesView>(Objects.Last()))
		{
			if (UObject* MetaSound = InterfacesView->GetMetasound())
			{
				InitBuilder(*MetaSound);
			}
		}

		UpdateInterfaceNames();

		SAssignNew(InterfaceComboBox, SSearchableComboBox)
			.OptionsSource(&AddableInterfaceNames)
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(*InItem));
			})
			.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NameToAdd, ESelectInfo::Type InSelectInfo)
			{
				using namespace Metasound;
				using namespace Metasound::Frontend;

				if (Builder.IsValid() && InSelectInfo != ESelectInfo::OnNavigation)
				{
					FMetaSoundFrontendDocumentBuilder& DocBuilder = Builder->GetBuilder();
					UObject& MetaSound = DocBuilder.CastDocumentObjectChecked<UObject>();
					FMetasoundFrontendInterface InterfaceToAdd;
					const FName InterfaceName { *NameToAdd.Get() };
					if (ensure(ISearchEngine::Get().FindInterfaceWithHighestVersion(InterfaceName, InterfaceToAdd)))
					{
						const FScopedTransaction Transaction(FText::Format(LOCTEXT("AddInterfaceTransactionFormat", "Add MetaSound Interface '{0}'"), FText::FromString(InterfaceToAdd.Version.ToString())));
						MetaSound.Modify();
						FModifyInterfaceOptions Options({ }, { InterfaceToAdd });
						Options.bSetDefaultNodeLocations = false; // Don't automatically add nodes to ed graph
						DocBuilder.ModifyInterfaces(MoveTemp(Options));
					}

					UpdateInterfaceNames();
					InterfaceComboBox->RefreshOptions();
					FGraphBuilder::RegisterGraphWithFrontend(MetaSound);
				}
			})
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("UpdateInterfaceAction", "Add Interface..."))
				.IsEnabled(IsGraphEditableAttribute)
			];

		TSharedRef<SWidget> InterfaceUtilities = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2.0f)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				InterfaceComboBox->AsShared()
			]
		+ SHorizontalBox::Slot()
			.Padding(2.0f)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				PropertyCustomizationHelpers::MakeDeleteButton(FSimpleDelegate::CreateLambda([this]()
				{
					using namespace Frontend;

					if (!Builder.IsValid())
					{
						return;
					}

					FMetaSoundFrontendDocumentBuilder& DocBuilder = Builder->GetBuilder();
					if (!DocBuilder.IsValid())
					{
						return;
					}

					TArray<FMetasoundFrontendInterface> ImplementedInterfaces;
					Algo::Transform(ImplementedInterfaceNames, ImplementedInterfaces, [](const FName& Name)
					{
						FMetasoundFrontendInterface Interface;
						ISearchEngine::Get().FindInterfaceWithHighestVersion(Name, Interface);
						return Interface;
					});

					UObject& MetaSound = DocBuilder.CastDocumentObjectChecked<UObject>();
					{
						const FScopedTransaction Transaction(LOCTEXT("RemoveAllInterfacesTransaction", "Remove All MetaSound Interfaces"));
						MetaSound.Modify();
						FModifyInterfaceOptions Options(ImplementedInterfaces, { });
						Options.bSetDefaultNodeLocations = false; // Don't automatically add nodes to ed graph
						DocBuilder.ModifyInterfaces(MoveTemp(Options));
					}

					UpdateInterfaceNames();
					InterfaceComboBox->RefreshOptions();
					FGraphBuilder::RegisterGraphWithFrontend(MetaSound);

				}), LOCTEXT("RemoveInterfaceTooltip1", "Removes all interfaces from the given MetaSound."))
			];
		InterfaceUtilities->SetEnabled(IsGraphEditableAttribute);

		const FText HeaderName = LOCTEXT("InterfacesGroupDisplayName", "Interfaces");
		IDetailCategoryBuilder& InterfaceCategory = DetailLayout.EditCategory("Interfaces", HeaderName);

		InterfaceCategory.AddCustomRow(HeaderName)
		[
			InterfaceUtilities
		];

		auto CreateInterfaceEntryWidget = [&](FName InInterfaceName) -> TSharedRef<SWidget>
		{
			using namespace Frontend;

			FMetasoundFrontendInterface InterfaceEntry;
			if (!ensure(ISearchEngine::Get().FindInterfaceWithHighestVersion(InInterfaceName, InterfaceEntry)))
			{
				return SNullWidget::NullWidget;
			}

			TSharedRef<SWidget> RemoveButtonWidget = PropertyCustomizationHelpers::MakeDeleteButton(FSimpleDelegate::CreateLambda([this, InInterfaceName, InterfaceEntry]()
			{
				using namespace Frontend;

				if (!Builder.IsValid())
				{
					return;
				}

				FMetaSoundFrontendDocumentBuilder& DocBuilder = Builder->GetBuilder();
				if (!DocBuilder.IsValid())
				{
					return;
				}

				UObject& MetaSound = DocBuilder.CastDocumentObjectChecked<UObject>();
				{
					const FScopedTransaction Transaction(FText::Format(LOCTEXT("RemoveInterfaceTransactionFormat", "Remove MetaSound Interface '{0}'"), FText::FromString(InterfaceEntry.Version.ToString())));
					MetaSound.Modify();
					FModifyInterfaceOptions Options({ InterfaceEntry }, {});
					Options.bSetDefaultNodeLocations = false; // Don't automatically add nodes to ed graph
					DocBuilder.ModifyInterfaces(MoveTemp(Options));
				}

				UpdateInterfaceNames();
				InterfaceComboBox->RefreshOptions();
				FGraphBuilder::RegisterGraphWithFrontend(MetaSound);

			}), LOCTEXT("RemoveInterfaceTooltip2", "Removes the associated interface from the MetaSound."));

			TSharedRef<SWidget> EntryWidget = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(2.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(FText::FromName(InterfaceEntry.Version.Name))
				]
			+ SHorizontalBox::Slot()
				.Padding(2.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					RemoveButtonWidget
				];

			EntryWidget->SetEnabled(IsGraphEditableAttribute);
			return EntryWidget;
		};

		TArray<FName> InterfaceNames = ImplementedInterfaceNames.Array();
		InterfaceNames.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
		for (const FName& InterfaceName : InterfaceNames)
		{
			InterfaceCategory.AddCustomRow(FText::FromName(InterfaceName))
			[
				CreateInterfaceEntryWidget(InterfaceName)
			];
		}
	}

	void FMetasoundInterfacesDetailCustomization::UpdateInterfaceNames()
	{
		AddableInterfaceNames.Reset();
		ImplementedInterfaceNames.Reset();

		if (const UObject* MetaSoundObject = GetMetaSound())
		{
			auto GetVersionName = [](const FMetasoundFrontendVersion& Version) { return Version.Name; };
			const UClass* MetaSoundClass = MetaSoundObject->GetClass();
			auto CanAddOrRemoveInterface = [ClassName = MetaSoundClass->GetClassPathName()](const FMetasoundFrontendVersion& Version)
			{
				using namespace Frontend;

				const FInterfaceRegistryKey Key = GetInterfaceRegistryKey(Version);
				if (const IInterfaceRegistryEntry* Entry = IInterfaceRegistry::Get().FindInterfaceRegistryEntry(Key))
				{
					if (const FMetasoundFrontendInterfaceUClassOptions* Options = Entry->GetInterface().FindClassOptions(ClassName))
					{
						return Options->bIsModifiable;
					}

					// If no options are found for the given class, interface is modifiable by default.
					return true;
				}

				return false;
			};

			const TSet<FMetasoundFrontendVersion>& ImplementedInterfaces = Builder->GetBuilder().GetConstDocumentChecked().Interfaces;
			Algo::TransformIf(ImplementedInterfaces, ImplementedInterfaceNames, CanAddOrRemoveInterface, GetVersionName);

			TArray<FMetasoundFrontendInterface> Interfaces = Frontend::ISearchEngine::Get().FindAllInterfaces();
			for (const FMetasoundFrontendInterface& Interface : Interfaces)
			{
				if (!ImplementedInterfaceNames.Contains(Interface.Version.Name))
				{
					if (CanAddOrRemoveInterface(Interface.Version))
					{
						FString Name = Interface.Version.Name.ToString();
						AddableInterfaceNames.Add(MakeShared<FString>(MoveTemp(Name)));
					}
				}
			}

			AddableInterfaceNames.Sort([](const TSharedPtr<FString>& A, const TSharedPtr<FString>& B) { return A->Compare(*B) < 0; });
		}
	}
} // namespace Metasound::Editor
#undef LOCTEXT_NAMESPACE
