#include "SFrameHoverWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Styling/CoreStyle.h"

void SFrameHoverWidget::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBorder)
		.Padding(6)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SBox)
			.WidthOverride(300)
			.MaxDesiredHeight(300)
			[
				SAssignNew(ScrollBox, SScrollBox)
				+ SScrollBox::Slot()
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("No data")))
				]
			]
		]
	];
}

void SFrameHoverWidget::SetFrameData(const FSampledFrameData* InData, int32 InIndex)
{
	CurrentData = InData;
	CurrentIndex = InIndex;
	RebuildContents();
}

void SFrameHoverWidget::RebuildContents()
{
	if (!ScrollBox.IsValid())
		return;

	ScrollBox->ClearChildren();

	if (!CurrentData)
	{
		ScrollBox->AddSlot()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("No frame data")))
		];
		return;
	}

	// Identify bottleneck thread (max TimeMs). If thread data is missing/empty, no highlighting.
	int32 BottleneckThreadIndex = INDEX_NONE;
	float BottleneckTimeMs = -FLT_MAX;
	if (CurrentData->ThreadData.Num() > 0)
	{
		for (int32 i = 0; i < CurrentData->ThreadData.Num(); ++i)
		{
			const FThreadSample& T = CurrentData->ThreadData[i];
			if (VisibleThreads.IsValid() && VisibleThreads->Num() > 0 && !VisibleThreads->Contains(T.ThreadName))
			{
				continue;
			}
			if (T.TimeMs > BottleneckTimeMs)
			{
				BottleneckTimeMs = T.TimeMs;
				BottleneckThreadIndex = i;
			}
		}
	}

	// Title
	{
		FString Title = FString::Printf(TEXT("Frame %d - %.2f ms"), CurrentIndex, CurrentData->FrameMS);
		ScrollBox->AddSlot()
		[
			SNew(STextBlock)
			.Text(FText::FromString(Title))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		];
	}

	// Range stats block (optional)
	if (RangeStats.bHasRange && RangeStats.NumFrames > 0)
	{
		ScrollBox->AddSlot().Padding(2)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(
				TEXT("Range [%d..%d] (%d frames)"),
				RangeStats.StartIndex,
				RangeStats.EndIndex,
				RangeStats.NumFrames)))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		];

		ScrollBox->AddSlot().Padding(2)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(
				TEXT("AvgFPS %.1f | Avg %.2f ms | Min %.2f ms | Max %.2f ms"),
				RangeStats.AvgFPS,
				RangeStats.AvgFrameMs,
				RangeStats.MinFrameMs,
				RangeStats.MaxFrameMs)))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
		];

		// Separator
		ScrollBox->AddSlot()
		[
			SNew(SSeparator)
			.Orientation(EOrientation::Orient_Horizontal)
		];
	}

	// Separator
	ScrollBox->AddSlot()
	[
		SNew(SSeparator)
		.Orientation(EOrientation::Orient_Horizontal)
	];

	// Add rows for major curves
	auto AddRow = [&](const FString& Name, float Value)
	{
		ScrollBox->AddSlot().Padding(2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(Name)).MinDesiredWidth(120).AutoWrapText(false)
			]
			+ SHorizontalBox::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%.2f ms"), Value)))
			]
		];
	};

	AddRow(TEXT("Frame"), CurrentData->FrameMS);
	AddRow(TEXT("Game"), CurrentData->GameMS);
	AddRow(TEXT("Draw"), CurrentData->DrawMS);
	AddRow(TEXT("RHI"), CurrentData->RHITMS);
	AddRow(TEXT("GPU"), CurrentData->GPUMS);

	// Per-thread breakdown
	if (CurrentData->ThreadData.Num() > 0)
	{
		ScrollBox->AddSlot().Padding(4)
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("Threads:"))).Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
		];

		for (int32 i = 0; i < CurrentData->ThreadData.Num(); ++i)
		{
			const FThreadSample& T = CurrentData->ThreadData[i];
			if (VisibleThreads.IsValid() && VisibleThreads->Num() > 0 && !VisibleThreads->Contains(T.ThreadName))
			{
				continue;
			}

			const bool bIsBottleneck = (i == BottleneckThreadIndex);

			const FSlateFontInfo RowFont = bIsBottleneck
				? FCoreStyle::GetDefaultFontStyle("Bold", 10)
				: FCoreStyle::GetDefaultFontStyle("Regular", 10);
			const FSlateColor RowColor = bIsBottleneck
				? FSlateColor(FLinearColor(1.0f, 0.15f, 0.15f, 1.0f))
				: FSlateColor::UseForeground();

			ScrollBox->AddSlot().Padding(1)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(T.ThreadName))
					.ColorAndOpacity(RowColor)
					.Font(RowFont)
					.MinDesiredWidth(120)
					.ToolTipText(FText::FromString(T.ThreadName))
				]
				+ SHorizontalBox::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("%.2f ms"), T.TimeMs)))
					.ColorAndOpacity(RowColor)
					.Font(RowFont)
				]
			];
		}
	}
}
