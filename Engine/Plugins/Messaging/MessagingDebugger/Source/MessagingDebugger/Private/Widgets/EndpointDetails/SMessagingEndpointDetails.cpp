// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SMessagingEndpointDetails.cpp: Implements the SMessagingEndpointDetails class.
=============================================================================*/

#include "MessagingDebuggerPrivatePCH.h"


#define LOCTEXT_NAMESPACE "SMessagingEndpointDetails"


/* SMessagingEndpointDetails interface
 *****************************************************************************/

void SMessagingEndpointDetails::Construct( const FArguments& InArgs, const FMessagingDebuggerModelRef& InModel, const TSharedRef<ISlateStyle>& InStyle )
{
	Model = InModel;
	Style = InStyle;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f, 2.0f)
			[
				SNew(SGridPanel)
					.FillColumn(1, 1.0f)

				// received messages count
				+ SGridPanel::Slot(0, 0)
					.Padding(0.0f, 4.0f)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("EndpointDetailsReceivedMessagesLabel", "Messages Received:"))
					]

				+ SGridPanel::Slot(1, 0)
					.HAlign(HAlign_Right)
					.Padding(0.0f, 4.0f)
					[
						SNew(STextBlock)
							.Text(this, &SMessagingEndpointDetails::HandleEndpointDetailsReceivedMessagesText)
					]

				// sent messages count
				+ SGridPanel::Slot(0, 1)
					.Padding(0.0f, 4.0f)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("EndpointDetailsReceivedLabel", "Messages Sent:"))
					]

				+ SGridPanel::Slot(1, 1)
					.HAlign(HAlign_Right)
					.Padding(0.0f, 4.0f)
					[
						SNew(STextBlock)
							.Text(this, &SMessagingEndpointDetails::HandleEndpointDetailsSentMessagesText)
					]
			]

		+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(0.0f)
					[
						// address list
						SAssignNew(AddressListView, SListView<FMessageTracerAddressInfoPtr>)
							.ItemHeight(24.0f)
							.ListItemsSource(&AddressList)
							.SelectionMode(ESelectionMode::None)
							.OnGenerateRow(this, &SMessagingEndpointDetails::HandleAddressListGenerateRow)
							.HeaderRow
							(
								SNew(SHeaderRow)

								+ SHeaderRow::Column("Address")
									.DefaultLabel(FText::FromString(TEXT("Endpoint Address")))
									.FillWidth(1.0f)

								+ SHeaderRow::Column("TimeRegistered")
									.DefaultLabel(LOCTEXT("AddressListTimeRegisteredColumnHeader", "Time Registered"))
									.FixedWidth(112.0f)
									.HAlignCell(HAlign_Right)
									.HAlignHeader(HAlign_Right)

								+ SHeaderRow::Column("TimeUnregistered")
									.DefaultLabel(LOCTEXT("AddressListTimeUnregisteredColumnHeader", "Time Unregistered"))
									.FixedWidth(112.0f)
									.HAlignCell(HAlign_Right)
									.HAlignHeader(HAlign_Right)
							)
					]
			]
	];
}


/* SCompoundWidget overrides
 *****************************************************************************/

void SMessagingEndpointDetails::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	RefreshAddressInfo();
}


/* SMessagingMessageDetails implementation
 *****************************************************************************/

void SMessagingEndpointDetails::RefreshAddressInfo( )
{
	FMessageTracerEndpointInfoPtr SelectedEndpoint = Model->GetSelectedEndpoint();

	if (SelectedEndpoint.IsValid())
	{
		SelectedEndpoint->AddressInfos.GenerateValueArray(AddressList);
	}
	else
	{
		AddressList.Reset();
	}

	AddressListView->RequestListRefresh();
}


/* SMessagingEndpointDetails event handlers
 *****************************************************************************/

TSharedRef<ITableRow> SMessagingEndpointDetails::HandleAddressListGenerateRow( FMessageTracerAddressInfoPtr AddressInfo, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(SMessagingAddressTableRow, OwnerTable, Model.ToSharedRef())
		.AddressInfo(AddressInfo)
		.Style(Style);
}


FString SMessagingEndpointDetails::HandleEndpointDetailsReceivedMessagesText( ) const
{
	FMessageTracerEndpointInfoPtr SelectedEndpoint = Model->GetSelectedEndpoint();

	if (SelectedEndpoint.IsValid())
	{
		return FString::Printf(TEXT("%i"), SelectedEndpoint->ReceivedMessages.Num());
	}

	return FString();
}


FString SMessagingEndpointDetails::HandleEndpointDetailsSentMessagesText( ) const
{
	FMessageTracerEndpointInfoPtr SelectedEndpoint = Model->GetSelectedEndpoint();

	if (SelectedEndpoint.IsValid())
	{
		return FString::Printf(TEXT("%i"), SelectedEndpoint->SentMessages.Num());
	}

	return FString();
}


#undef LOCTEXT_NAMESPACE
