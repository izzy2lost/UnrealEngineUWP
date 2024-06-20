// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IDetailCustomization.h"
#include "SGraphActionMenu.h"
#include "SSearchableComboBox.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"
#include "UObject/NameTypes.h"


// Forward Declarations
class FPropertyRestriction;
class IDetailLayoutBuilder;
class UMetaSoundBuilderBase;
struct FPointerEvent;

namespace Metasound
{
	namespace Editor
	{
		class FMetaSoundDetailCustomizationBase : public IDetailCustomization
		{
		public:
			virtual ~FMetaSoundDetailCustomizationBase() = default;

			bool IsGraphEditable() const;

		protected:
			UObject* GetMetaSound() const;
			void InitBuilder(UObject& MetaSound);

			TStrongObjectPtr<UMetaSoundBuilderBase> Builder;
		};

		class FMetasoundDetailCustomization : public FMetaSoundDetailCustomizationBase
		{
		public:
			FMetasoundDetailCustomization(FName InDocumentPropertyName);
			virtual ~FMetasoundDetailCustomization() = default;

			// IDetailCustomization interface
			virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
			// End of IDetailCustomization interface

		private:
			FName GetInterfaceVersionsPath() const;
			FName GetMetadataRootClassPath() const;
			FName GetMetadataPropertyPath() const;

			FName DocumentPropertyName;
		};

		class FMetasoundInterfacesDetailCustomization : public FMetaSoundDetailCustomizationBase
		{
		public:
			// IDetailCustomization interface
			virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
			// End of IDetailCustomization interface

		private:
			void UpdateInterfaceNames();

			TArray<TSharedPtr<FString>> AddableInterfaceNames;

			TSet<FName> ImplementedInterfaceNames;
			TSharedPtr<SSearchableComboBox> InterfaceComboBox;
			TAttribute<bool> IsGraphEditableAttribute;
		};
	} // namespace Editor
} // namespace Metasound