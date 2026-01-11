#pragma once

// Screenshot export folder (relative to ProjectSavedDir). You can override this at build time
// by defining PTTOOL_CAPTURE_EXPORT_SUBDIR (e.g. in your Target.cs/Build.cs via PublicDefinitions).
#ifndef PTTOOL_CAPTURE_EXPORT_SUBDIR
#define PTTOOL_CAPTURE_EXPORT_SUBDIR TEXT("WindowCaptures")
#endif

#include "CoreMinimal.h"
#include "PerformanceGraph.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "PTDataType.h"
#include "Misc/Paths.h"

#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Modules/ModuleManager.h"

#include "HAL/FileManager.h"


// Include Windows headers safely for native window capture on Windows
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

// Use SOverlay to layer hover widget
#include "Widgets/SOverlay.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h" // SHorizontalBox/SVerticalBox

#include "SFrameHoverWidget.h"


inline TArray<TSharedPtr<FSampledGraphData>> ListItems;

inline void OpenPerformanceAnalyzerWindow(TArray<FSampledGraphData> Sample)
{
	TSharedPtr<SListView<TSharedPtr<FSampledGraphData>>> ListView;
	ListItems.Reset();
	ListItems.Reserve(Sample.Num());

	for (const FSampledGraphData& Data : Sample)
	{
		ListItems.Add(MakeShared<FSampledGraphData>(Data));
	}

	TSharedRef<SPerformanceGraph> PerformanceGraph =
		SNew(SPerformanceGraph);

	// Track currently selected item so stats can update.
	TSharedPtr<TSharedPtr<FSampledGraphData>> SelectedItem = MakeShared<TSharedPtr<FSampledGraphData>>();

	// Track current selection range (inclusive indices, in graph's SampledFrameData index space)
	TSharedPtr<int32> RangeStartIndex = MakeShared<int32>(INDEX_NONE);
	TSharedPtr<int32> RangeEndIndex = MakeShared<int32>(INDEX_NONE);

	// Visible thread filter (legend toggles). Empty => show all.
	TSharedPtr<TSet<FString>> VisibleThreads = MakeShared<TSet<FString>>();

	// Visible curve toggles (for Frame/Game/Draw/RHI/GPU)
	TSharedPtr<TSet<EPerfCurve>> VisibleCurves = MakeShared<TSet<EPerfCurve>>(
		TSet<EPerfCurve>({ EPerfCurve::Frame, EPerfCurve::Game, EPerfCurve::Draw, EPerfCurve::GPU })
	);


	//根据宏定义创建窗口
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString(TEXT("Performance Analyzer")))
		.ClientSize(FVector2D(PT_PERFORMANCE_GRAPH_WINDOW_SIZE_X, PT_PERFORMANCE_GRAPH_WINDOW_SIZE_Y));

	// Create hover widget
	TSharedPtr<class SFrameHoverWidget> HoverWidget;

	// Shared hover position in local overlay coordinates (updated by graph delegate)
	TSharedPtr<FVector2D> HoverPos = MakeShared<FVector2D>(FVector2D::ZeroVector);

	// Shared range stats cache for hover widget
	TSharedPtr<SFrameHoverWidget::FRangeStats> RangeStatsCache = MakeShared<SFrameHoverWidget::FRangeStats>();

	Window->SetContent(
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)

			+ SSplitter::Slot()
			.Value(0.25f) // 初始占比 25%
			[
				SNew(SExpandableArea)
				.InitiallyCollapsed(false)

				.HeaderContent()
				[
					SNew(SBorder)
					.Padding(6)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Performance Test List")))
					]
				]

				.BodyContent()
				[
					// 把原有 List 放在左侧，并在右侧添加一个按钮列
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew(ListView, SListView<TSharedPtr<FSampledGraphData>>)
						.ListItemsSource(&ListItems)
						.SelectionMode(ESelectionMode::Single)
						.OnGenerateRow_Lambda(
							[](TSharedPtr<FSampledGraphData> Item, const TSharedRef<STableViewBase>& Owner)
							{
								return SNew(STableRow<TSharedPtr<FSampledGraphData>>, Owner)
									[
										SNew(STextBlock).Text(FText::FromString(Item->SplineName))
									];
							}
						)
						.OnSelectionChanged_Lambda(
							[PerformanceGraph, SelectedItem, VisibleThreads](TSharedPtr<FSampledGraphData> Item, ESelectInfo::Type SelectType)
							{
								if (!Item.IsValid())
								{
									return;
								}

								*SelectedItem = Item;
								PerformanceGraph->SetFrameData(Item->FrameData);

								// When selecting an item, initialize VisibleThreads with all thread names (so legend works immediately).
								VisibleThreads->Reset();
								for (const FSampledFrameData& Frame : Item->FrameData)
								{
									for (const FThreadSample& Thread : Frame.ThreadData)
									{
										VisibleThreads->Add(Thread.ThreadName);
									}
								}
							}
						)
					]

					// 新增：右侧按钮列（包含截图按钮）
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6, 0, 0, 0)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(2)
						[
							SNew(SButton)
							.Text(FText::FromString(TEXT("Capture Window")))
							.ToolTipText(FText::FromString(TEXT("Capture current PTTool window to PNG.\nSaved to: <Project>/Saved/" PTTOOL_CAPTURE_EXPORT_SUBDIR "\nYou can customize the export folder in C++ by defining PTTOOL_CAPTURE_EXPORT_SUBDIR.")))
							.OnClicked_Lambda([Window]() -> FReply
							{
								// Capture the native window contents on Windows using GDI and save as PNG into Saved folder
#if PLATFORM_WINDOWS
								void* OSHandle = nullptr;
								// TSharedRef doesn't have IsValid(); check the native window instead
								if (Window->GetNativeWindow().IsValid())
								{
									OSHandle = Window->GetNativeWindow()->GetOSWindowHandle();
								}
								if (!OSHandle)
								{
									UE_LOG(LogTemp, Warning, TEXT("CaptureWindow: native OS handle not available"));
									return FReply::Handled();
								}
								HWND Hwnd = (HWND)OSHandle;
								RECT rc;
								if (!GetClientRect(Hwnd, &rc))
								{
									UE_LOG(LogTemp, Warning, TEXT("CaptureWindow: GetClientRect failed"));
									return FReply::Handled();
								}
								int32 Width = rc.right - rc.left;
								int32 Height = rc.bottom - rc.top;
								if (Width <= 0 || Height <= 0)
								{
									UE_LOG(LogTemp, Warning, TEXT("CaptureWindow: invalid size %d x %d"), Width, Height);
									return FReply::Handled();
								}

								HDC hdcWindow = GetDC(Hwnd);
								HDC hdcMemDC = CreateCompatibleDC(hdcWindow);
								HBITMAP hBitmap = CreateCompatibleBitmap(hdcWindow, Width, Height);
								HGDIOBJ hOld = SelectObject(hdcMemDC, hBitmap);
								// Include layered windows / transparent areas
								BitBlt(hdcMemDC, 0, 0, Width, Height, hdcWindow, 0, 0, SRCCOPY | CAPTUREBLT);

								// Prepare BITMAPINFO for 32-bit BGRA
								BITMAPINFO bmi;
								FMemory::Memzero(bmi);
								bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
								bmi.bmiHeader.biWidth = Width;
								// negative height to get top-down DIB
								bmi.bmiHeader.biHeight = -Height;
								bmi.bmiHeader.biPlanes = 1;
								bmi.bmiHeader.biBitCount = 32;
								bmi.bmiHeader.biCompression = BI_RGB;

								TArray<uint8> RawData;
								RawData.AddUninitialized(Width * Height * 4);
								if (GetDIBits(hdcWindow, hBitmap, 0, Height, RawData.GetData(), &bmi, DIB_RGB_COLORS) == 0)
								{
									UE_LOG(LogTemp, Warning, TEXT("CaptureWindow: GetDIBits failed"));
								}
								else
								{
									// Convert BGRA -> RGBA for ImageWrapper
									for (int32 i = 0; i < Width * Height; ++i)
									{
										uint8 B = RawData[i * 4 + 0];
										uint8 G = RawData[i * 4 + 1];
										uint8 R = RawData[i * 4 + 2];
										uint8 A = RawData[i * 4 + 3];
										RawData[i * 4 + 0] = R;
										RawData[i * 4 + 1] = G;
										RawData[i * 4 + 2] = B;
										RawData[i * 4 + 3] = A;
									}

									IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
									TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
									if (ImageWrapper.IsValid())
									{
										if (ImageWrapper->SetRaw(RawData.GetData(), RawData.Num(), Width, Height, ERGBFormat::RGBA, 8))
										{
											auto Compressed = ImageWrapper->GetCompressed(100);
											FString Filename = FPaths::ProjectSavedDir() / PTTOOL_CAPTURE_EXPORT_SUBDIR;

											IFileManager::Get().MakeDirectory(*Filename, true);
											FString FilePath = Filename / (FString::Printf(TEXT("PerformanceWindow_%s.png"), *FDateTime::Now().ToString()));
											// Write compressed bytes using file writer to support different TArray types
											FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*FilePath);
											if (FileWriter)
											{
												FileWriter->Serialize(Compressed.GetData(), (int64)Compressed.Num());
												FileWriter->Close();
												delete FileWriter;
												UE_LOG(LogTemp, Log, TEXT("Capture saved to %s"), *FilePath);
											}
										}
									}
								}

								// cleanup
								SelectObject(hdcMemDC, hOld);
								DeleteObject(hBitmap);
								DeleteDC(hdcMemDC);
								ReleaseDC(Hwnd, hdcWindow);
#endif
								return FReply::Handled();
							})
						]

						+ SVerticalBox::Slot().AutoHeight().Padding(2)
						[
							SNew(SButton)
							.Text(FText::FromString(TEXT("Button 2")))
							.OnClicked_Lambda([]() -> FReply { return FReply::Handled(); })
						]

						+ SVerticalBox::Slot().AutoHeight().Padding(2)
						[
							SNew(SButton)
							.Text(FText::FromString(TEXT("Button 3")))
							.OnClicked_Lambda([]() -> FReply { return FReply::Handled(); })
						]
					]
				]
			]

			// ===== 下半：Graph（高度不受折叠影响）=====
			+ SSplitter::Slot()
			.Value(0.75f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					PerformanceGraph
				]
				// place hover widget using dynamic padding so it follows graph-local coords
				+ SOverlay::Slot()
				.Padding(TAttribute<FMargin>::CreateLambda([HoverPos]() { return FMargin(HoverPos->X, HoverPos->Y, 0, 0); }))
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Top)
				[
					SAssignNew(HoverWidget, SFrameHoverWidget)
				]


			]
		]
		// ===== 上半：List =====


		+ SVerticalBox::Slot()
		.AutoHeight()     
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8, 2)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.BorderBackgroundColor(GetCurveColor(EPerfCurve::Frame))
					.Padding(FMargin(6, 6))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Frame")))
				]
			]

			// Game
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8, 2)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.BorderBackgroundColor(GetCurveColor(EPerfCurve::Game))
					.Padding(FMargin(6, 6))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Game")))
				]
			]

			// Draw
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8, 2)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.BorderBackgroundColor(GetCurveColor(EPerfCurve::Draw))
					.Padding(FMargin(6, 6))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Draw")))
				]
			]

			// RHI
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8, 2)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.BorderBackgroundColor(GetCurveColor(EPerfCurve::RHI))
					.Padding(FMargin(6, 6))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("RHI")))
				]
			]

			// GPU
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8, 2)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.BorderBackgroundColor(GetCurveColor(EPerfCurve::GPU))
					.Padding(FMargin(6, 6))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("GPU")))
				]
			]
		]


		// ===== Bottom stats bar =====
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8, 4)
		[
			SNew(SBorder)
			.Padding(6)
			.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Thread Stats (click legend to show/hide)")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				]

				// Legend row
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 4, 0, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Legend:")))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(6,0,0,0)
					[
						SNew(SHorizontalBox)
						// Dynamic buttons
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SHorizontalBox)
							// This inner box will be rebuilt by lambda into text blocks with separators.
							+ SHorizontalBox::Slot().FillWidth(1.0f)
							[
								SNew(STextBlock)
								.Text_Lambda([SelectedItem]()
								{
									TSharedPtr<FSampledGraphData> Item = SelectedItem.IsValid() ? *SelectedItem : nullptr;
									return Item.IsValid() ? FText::FromString(TEXT("")) : FText::FromString(TEXT("(select a test)"));
								})
							]
						]
					]
				]

				// Whole capture
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 4, 0, 0)
				[
					SNew(STextBlock)
					.Text_Lambda([SelectedItem, VisibleThreads]()
					{
						TSharedPtr<FSampledGraphData> Item = SelectedItem.IsValid() ? *SelectedItem : nullptr;
						if (!Item.IsValid())
						{
							return FText::FromString(TEXT("Whole Capture: No selection"));
						}
						if (Item->FrameData.Num() == 0)
						{
							return FText::FromString(TEXT("Whole Capture: No FrameData"));
						}

						struct FAcc { double Sum = 0.0; int32 Count = 0; float Min = 0.f; float Max = 0.f; bool bInit = false; };
						TMap<FString, FAcc> Acc;
						for (const FSampledFrameData& F : Item->FrameData)
						{
							for (const FThreadSample& T : F.ThreadData)
							{
								if (VisibleThreads.IsValid() && VisibleThreads->Num() > 0 && !VisibleThreads->Contains(T.ThreadName))
								{
									continue;
								}
								FAcc& A = Acc.FindOrAdd(T.ThreadName);
								A.Sum += (double)T.TimeMs;
								A.Count += 1;
								if (!A.bInit)
								{
									A.Min = T.TimeMs;
									A.Max = T.TimeMs;
									A.bInit = true;
								}
								else
								{
									A.Min = FMath::Min(A.Min, T.TimeMs);
									A.Max = FMath::Max(A.Max, T.TimeMs);
								}
							}
						}
						if (Acc.Num() == 0)
						{
							return FText::FromString(TEXT("Whole Capture: No ThreadData recorded (or filtered out)"));
						}

						TArray<TPair<FString, FAcc>> Pairs;
						for (const TPair<FString, FAcc>& KV : Acc)
						{
							Pairs.Add(KV);
						}
						Pairs.Sort([](const TPair<FString, FAcc>& A, const TPair<FString, FAcc>& B)
						{
							const double AvgA = A.Value.Count > 0 ? A.Value.Sum / (double)A.Value.Count : 0.0;
							const double AvgB = B.Value.Count > 0 ? B.Value.Sum / (double)B.Value.Count : 0.0;
							return AvgA > AvgB;
						});

						FString Out(TEXT("Whole Capture: "));
						for (int32 i = 0; i < Pairs.Num(); ++i)
						{
							const FString& Name = Pairs[i].Key;
							const FAcc& A = Pairs[i].Value;
							const float Avg = A.Count > 0 ? (float)(A.Sum / (double)A.Count) : 0.f;
							Out += FString::Printf(TEXT("%s Avg %.2f | Min %.2f | Max %.2f"), *Name, Avg, A.Min, A.Max);
							if (i != Pairs.Num() - 1)
							{
								Out += TEXT("    ||    ");
							}
						}
						return FText::FromString(Out);
					})
					.AutoWrapText(true)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				]

				// Range stats (filtered)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 4, 0, 0)
				[
					SNew(STextBlock)
					.Text_Lambda([SelectedItem, RangeStartIndex, RangeEndIndex, VisibleThreads]()
					{
						TSharedPtr<FSampledGraphData> Item = SelectedItem.IsValid() ? *SelectedItem : nullptr;
						if (!Item.IsValid())
						{
							return FText::FromString(TEXT("Range: No selection"));
						}
						if (Item->FrameData.Num() == 0)
						{
							return FText::FromString(TEXT("Range: No FrameData"));
						}

						int32 SIdx = RangeStartIndex.IsValid() ? *RangeStartIndex : INDEX_NONE;
						int32 EIdx = RangeEndIndex.IsValid() ? *RangeEndIndex : INDEX_NONE;
						if (SIdx == INDEX_NONE || EIdx == INDEX_NONE)
						{
							return FText::FromString(TEXT("Range: (drag on graph to select)"));
						}
						if (SIdx > EIdx)
						{
							Swap(SIdx, EIdx);
						}

						SIdx = FMath::Clamp(SIdx, 0, Item->FrameData.Num() - 1);
						EIdx = FMath::Clamp(EIdx, 0, Item->FrameData.Num() - 1);
						if (SIdx > EIdx)
						{
							return FText::FromString(TEXT("Range: invalid"));
						}

						struct FAcc { double Sum = 0.0; int32 Count = 0; float Min = 0.f; float Max = 0.f; bool bInit = false; };
						TMap<FString, FAcc> Acc;
						for (int32 i = SIdx; i <= EIdx; ++i)
						{
							const FSampledFrameData& F = Item->FrameData[i];
							for (const FThreadSample& T : F.ThreadData)
							{
								if (VisibleThreads.IsValid() && VisibleThreads->Num() > 0 && !VisibleThreads->Contains(T.ThreadName))
								{
									continue;
								}
								FAcc& A = Acc.FindOrAdd(T.ThreadName);
								A.Sum += (double)T.TimeMs;
								A.Count += 1;
								if (!A.bInit)
								{
									A.Min = T.TimeMs;
									A.Max = T.TimeMs;
									A.bInit = true;
								}
								else
								{
									A.Min = FMath::Min(A.Min, T.TimeMs);
									A.Max = FMath::Max(A.Max, T.TimeMs);
								}
							}
						}

						if (Acc.Num() == 0)
						{
							return FText::FromString(FString::Printf(TEXT("Range [%d..%d]: No ThreadData"), SIdx, EIdx));
						}

						TArray<TPair<FString, FAcc>> Pairs;
						for (const TPair<FString, FAcc>& KV : Acc)
						{
							Pairs.Add(KV);
						}
						Pairs.Sort([](const TPair<FString, FAcc>& A, const TPair<FString, FAcc>& B)
						{
							const double AvgA = A.Value.Count > 0 ? A.Value.Sum / (double)A.Value.Count : 0.0;
							const double AvgB = B.Value.Count > 0 ? B.Value.Sum / (double)B.Value.Count : 0.0;
							return AvgA > AvgB;
						});

						FString Out = FString::Printf(TEXT("Range [%d..%d]: "), SIdx, EIdx);
						for (int32 i = 0; i < Pairs.Num(); ++i)
						{
							const FString& Name = Pairs[i].Key;
							const FAcc& A = Pairs[i].Value;
							const float Avg = A.Count > 0 ? (float)(A.Sum / (double)A.Count) : 0.f;
							Out += FString::Printf(TEXT("%s Avg %.2f | Min %.2f | Max %.2f"), *Name, Avg, A.Min, A.Max);
							if (i != Pairs.Num() - 1)
							{
								Out += TEXT("    ||    ");
							}
						}
						return FText::FromString(Out);
					})
					.AutoWrapText(true)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				]
			]
		]

	);

	// Bind graph hover delegate to update hover widget
	{
		TWeakPtr<SFrameHoverWidget> WeakHover = HoverWidget;
		TWeakPtr<SPerformanceGraph> WeakGraph = PerformanceGraph;
		TSharedPtr<FVector2D> LocalHoverPos = HoverPos;
		PerformanceGraph->OnHoverSampleChanged.BindLambda([WeakHover, WeakGraph, LocalHoverPos, VisibleThreads, RangeStatsCache](int32 Index, FVector2D Local)
		{
			TSharedPtr<SFrameHoverWidget> HW = WeakHover.Pin();
			TSharedPtr<SPerformanceGraph> PG = WeakGraph.Pin();
			if (!HW.IsValid() || !PG.IsValid())
				return;

			// keep hover widget filtered according to legend
			HW->SetVisibleThreadFilter(VisibleThreads);

			// keep range stats displayed in hover
			if (RangeStatsCache.IsValid() && RangeStatsCache->bHasRange)
			{
				HW->SetRangeStats(*RangeStatsCache);
			}
			else
			{
				HW->ClearRangeStats();
			}

			// Always update hover position so tooltip follows cursor
			LocalHoverPos->X = Local.X + 10.0f;
			LocalHoverPos->Y = Local.Y + 10.0f;

			if (Index != INDEX_NONE)
			{
				if (PG->SampledFrameData.IsValidIndex(Index))
				{
					HW->SetFrameData(&PG->SampledFrameData[Index], Index);
					HW->SetVisibility(EVisibility::Visible);
				}
				else
				{
					HW->SetFrameData(nullptr, INDEX_NONE);
					HW->SetVisibility(EVisibility::Collapsed);
				}
			}
			else
			{
				// If hover locked, keep visible; otherwise hide
				if (!PG->IsHoverLocked())
				{
					HW->SetVisibility(EVisibility::Collapsed);
				}
			}
		});
	}

	// Bind selection range delegate
	PerformanceGraph->OnSelectionRangeChanged.BindLambda([RangeStartIndex, RangeEndIndex, Window, SelectedItem, RangeStatsCache, WeakHover = TWeakPtr<SFrameHoverWidget>(HoverWidget), WeakGraph = TWeakPtr<SPerformanceGraph>(PerformanceGraph)](int32 Start, int32 End)
	{
		// Treat single-point range (X..X) as cancel selection
		if (Start != INDEX_NONE && End != INDEX_NONE && Start == End)
		{
			*RangeStartIndex = INDEX_NONE;
			*RangeEndIndex = INDEX_NONE;

			if (TSharedPtr<SPerformanceGraph> PG = WeakGraph.Pin())
			{
				PG->ClearSelection();
			}

			if (RangeStatsCache.IsValid())
			{
				*RangeStatsCache = SFrameHoverWidget::FRangeStats{};
			}
			if (TSharedPtr<SFrameHoverWidget> HW = WeakHover.Pin())
			{
				HW->ClearRangeStats();
			}

			Window->Invalidate(EInvalidateWidget::LayoutAndVolatility);
			return;
		}

		*RangeStartIndex = Start;
		*RangeEndIndex = End;

		// compute and push range stats to hover widget
		TSharedPtr<FSampledGraphData> Item = SelectedItem.IsValid() ? *SelectedItem : nullptr;
		TSharedPtr<SFrameHoverWidget> HW = WeakHover.Pin();

		if (!Item.IsValid() || Item->FrameData.Num() == 0 || Start == INDEX_NONE || End == INDEX_NONE)
		{
			if (RangeStatsCache.IsValid())
			{
				*RangeStatsCache = SFrameHoverWidget::FRangeStats{};
			}
			if (HW.IsValid())
			{
				HW->ClearRangeStats();
			}
			Window->Invalidate(EInvalidateWidget::LayoutAndVolatility);
			return;
		}

		int32 SIdx = Start;
		int32 EIdx = End;
		if (SIdx > EIdx)
		{
			Swap(SIdx, EIdx);
		}
		SIdx = FMath::Clamp(SIdx, 0, Item->FrameData.Num() - 1);
		EIdx = FMath::Clamp(EIdx, 0, Item->FrameData.Num() - 1);
		if (SIdx > EIdx)
		{
			if (RangeStatsCache.IsValid())
			{
				*RangeStatsCache = SFrameHoverWidget::FRangeStats{};
			}
			if (HW.IsValid())
			{
				HW->ClearRangeStats();
			}
			Window->Invalidate(EInvalidateWidget::LayoutAndVolatility);
			return;
		}

		float SumMs = 0.f;
		float MinMs = FLT_MAX;
		float MaxMs = -FLT_MAX;
		int32 Count = 0;
		for (int32 i = SIdx; i <= EIdx; ++i)
		{
			const float Ms = Item->FrameData[i].FrameMS;
			SumMs += Ms;
			MinMs = FMath::Min(MinMs, Ms);
			MaxMs = FMath::Max(MaxMs, Ms);
			++Count;
		}

		SFrameHoverWidget::FRangeStats Stats;
		Stats.bHasRange = (Count > 0);
		Stats.StartIndex = SIdx;
		Stats.EndIndex = EIdx;
		Stats.NumFrames = Count;
		Stats.AvgFrameMs = Count > 0 ? (SumMs / (float)Count) : 0.f;
		Stats.MinFrameMs = (Count > 0 && MinMs != FLT_MAX) ? MinMs : 0.f;
		Stats.MaxFrameMs = (Count > 0 && MaxMs != -FLT_MAX) ? MaxMs : 0.f;
		Stats.AvgFPS = Stats.AvgFrameMs > KINDA_SMALL_NUMBER ? (1000.0f / Stats.AvgFrameMs) : 0.0f;

		if (RangeStatsCache.IsValid())
		{
			*RangeStatsCache = Stats;
		}
		if (HW.IsValid())
		{
			HW->SetRangeStats(Stats);
		}

		Window->Invalidate(EInvalidateWidget::LayoutAndVolatility);
	});

	if (ListItems.Num() > 0 && ListView.IsValid())
	{
		ListView->SetSelection(ListItems[0]);
	}
	// ===== 多曲线选择 =====

	auto ApplyVisibleCurves = [PerformanceGraph, VisibleCurves]()
	{
		PerformanceGraph->SetVisibleCurves(*VisibleCurves);
	};
	ApplyVisibleCurves();

	// Helper to make a checkbox row like your screenshot
	auto MakeCurveToggle = [VisibleCurves, ApplyVisibleCurves](EPerfCurve Curve, const TCHAR* Label) -> TSharedRef<SWidget>
	{
		return SNew(SCheckBox)
			.Style(FCoreStyle::Get(), "Checkbox")
			.IsChecked_Lambda([VisibleCurves, Curve]()
			{
				return (VisibleCurves.IsValid() && VisibleCurves->Contains(Curve)) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([VisibleCurves, ApplyVisibleCurves, Curve](ECheckBoxState State)
			{
				if (!VisibleCurves.IsValid())
					return;
				if (State == ECheckBoxState::Checked)
				{
					VisibleCurves->Add(Curve);
				}
				else
				{
					VisibleCurves->Remove(Curve);
				}
				ApplyVisibleCurves();
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.Padding(FMargin(6, 6))
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(GetCurveColor(Curve))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Label))
				]
			];
	};

	Window->SetContent(
		SNew(SVerticalBox)

		// ===== 上半：List =====
		+ SVerticalBox::Slot()
		.AutoHeight()     
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8, 2)
			[
				MakeCurveToggle(EPerfCurve::Frame, TEXT("Frame"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8, 2)
			[
				MakeCurveToggle(EPerfCurve::Game, TEXT("Game"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8, 2)
			[
				MakeCurveToggle(EPerfCurve::Draw, TEXT("Draw"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8, 2)
			[
				MakeCurveToggle(EPerfCurve::RHI, TEXT("RHI"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8, 2)
			[
				MakeCurveToggle(EPerfCurve::GPU, TEXT("GPU"))
			]
		]


		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)

			+ SSplitter::Slot()
			.Value(0.25f) // 初始占比 25%
			[
				SNew(SExpandableArea)
				.InitiallyCollapsed(false)

				.HeaderContent()
				[
					SNew(SBorder)
					.Padding(6)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Performance Test List")))
					]
				]

				.BodyContent()
				[
					// 把原有 List 放在左侧，并在右侧添加一个按钮列
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew(ListView, SListView<TSharedPtr<FSampledGraphData>>)
						.ListItemsSource(&ListItems)
						.SelectionMode(ESelectionMode::Single)
						.OnGenerateRow_Lambda(
							[](TSharedPtr<FSampledGraphData> Item, const TSharedRef<STableViewBase>& Owner)
							{
								return SNew(STableRow<TSharedPtr<FSampledGraphData>>, Owner)
									[
										SNew(STextBlock).Text(FText::FromString(Item->SplineName))
									];
							}
						)
						.OnSelectionChanged_Lambda(
							[PerformanceGraph, SelectedItem, VisibleThreads](TSharedPtr<FSampledGraphData> Item, ESelectInfo::Type SelectType)
							{
								if (!Item.IsValid())
								{
									return;
								}

								*SelectedItem = Item;
								PerformanceGraph->SetFrameData(Item->FrameData);

								// When selecting an item, initialize VisibleThreads with all thread names (so legend works immediately).
								VisibleThreads->Reset();
								for (const FSampledFrameData& Frame : Item->FrameData)
								{
									for (const FThreadSample& Thread : Frame.ThreadData)
									{
										VisibleThreads->Add(Thread.ThreadName);
									}
								}
							}
						)
					]

					// 新增：右侧按钮列（包含截图按钮）
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6, 0, 0, 0)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(2)
						[
							SNew(SButton)
							.Text(FText::FromString(TEXT("Capture Window")))
							.ToolTipText(FText::FromString(TEXT("Capture current PTTool window to PNG.\nSaved to: <Project>/Saved/" PTTOOL_CAPTURE_EXPORT_SUBDIR "\nYou can customize the export folder in C++ by defining PTTOOL_CAPTURE_EXPORT_SUBDIR.")))
							.OnClicked_Lambda([Window]() -> FReply
							{
								// Capture the native window contents on Windows using GDI and save as PNG into Saved folder
#if PLATFORM_WINDOWS
								void* OSHandle = nullptr;
								// TSharedRef doesn't have IsValid(); check the native window instead
								if (Window->GetNativeWindow().IsValid())
								{
									OSHandle = Window->GetNativeWindow()->GetOSWindowHandle();
								}
								if (!OSHandle)
								{
									UE_LOG(LogTemp, Warning, TEXT("CaptureWindow: native OS handle not available"));
									return FReply::Handled();
								}
								HWND Hwnd = (HWND)OSHandle;
								RECT rc;
								if (!GetClientRect(Hwnd, &rc))
								{
									UE_LOG(LogTemp, Warning, TEXT("CaptureWindow: GetClientRect failed"));
									return FReply::Handled();
								}
								int32 Width = rc.right - rc.left;
								int32 Height = rc.bottom - rc.top;
								if (Width <= 0 || Height <= 0)
								{
									UE_LOG(LogTemp, Warning, TEXT("CaptureWindow: invalid size %d x %d"), Width, Height);
									return FReply::Handled();
								}

								HDC hdcWindow = GetDC(Hwnd);
								HDC hdcMemDC = CreateCompatibleDC(hdcWindow);
								HBITMAP hBitmap = CreateCompatibleBitmap(hdcWindow, Width, Height);
								HGDIOBJ hOld = SelectObject(hdcMemDC, hBitmap);
								// Include layered windows / transparent areas
								BitBlt(hdcMemDC, 0, 0, Width, Height, hdcWindow, 0, 0, SRCCOPY | CAPTUREBLT);

								// Prepare BITMAPINFO for 32-bit BGRA
								BITMAPINFO bmi;
								FMemory::Memzero(bmi);
								bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
								bmi.bmiHeader.biWidth = Width;
								// negative height to get top-down DIB
								bmi.bmiHeader.biHeight = -Height;
								bmi.bmiHeader.biPlanes = 1;
								bmi.bmiHeader.biBitCount = 32;
								bmi.bmiHeader.biCompression = BI_RGB;

								TArray<uint8> RawData;
								RawData.AddUninitialized(Width * Height * 4);
								if (GetDIBits(hdcWindow, hBitmap, 0, Height, RawData.GetData(), &bmi, DIB_RGB_COLORS) == 0)
								{
									UE_LOG(LogTemp, Warning, TEXT("CaptureWindow: GetDIBits failed"));
								}
								else
								{
									// Convert BGRA -> RGBA for ImageWrapper
									for (int32 i = 0; i < Width * Height; ++i)
									{
										uint8 B = RawData[i * 4 + 0];
										uint8 G = RawData[i * 4 + 1];
										uint8 R = RawData[i * 4 + 2];
										uint8 A = RawData[i * 4 + 3];
										RawData[i * 4 + 0] = R;
										RawData[i * 4 + 1] = G;
										RawData[i * 4 + 2] = B;
										RawData[i * 4 + 3] = A;
									}

									IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
									TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
									if (ImageWrapper.IsValid())
									{
										if (ImageWrapper->SetRaw(RawData.GetData(), RawData.Num(), Width, Height, ERGBFormat::RGBA, 8))
										{
											auto Compressed = ImageWrapper->GetCompressed(100);
											FString Filename = FPaths::ProjectSavedDir() / PTTOOL_CAPTURE_EXPORT_SUBDIR;

											IFileManager::Get().MakeDirectory(*Filename, true);
											FString FilePath = Filename / (FString::Printf(TEXT("PerformanceWindow_%s.png"), *FDateTime::Now().ToString()));
											// Write compressed bytes using file writer to support different TArray types
											FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*FilePath);
											if (FileWriter)
											{
												FileWriter->Serialize(Compressed.GetData(), (int64)Compressed.Num());
												FileWriter->Close();
												delete FileWriter;
												UE_LOG(LogTemp, Log, TEXT("Capture saved to %s"), *FilePath);
											}
										}
									}
								}

								// cleanup
								SelectObject(hdcMemDC, hOld);
								DeleteObject(hBitmap);
								DeleteDC(hdcMemDC);
								ReleaseDC(Hwnd, hdcWindow);
#endif
								return FReply::Handled();
							})
						]

						+ SVerticalBox::Slot().AutoHeight().Padding(2)
						[
							SNew(SButton)
							.Text(FText::FromString(TEXT("Button 2")))
							.OnClicked_Lambda([]() -> FReply { return FReply::Handled(); })
						]

						+ SVerticalBox::Slot().AutoHeight().Padding(2)
						[
							SNew(SButton)
							.Text(FText::FromString(TEXT("Button 3")))
							.OnClicked_Lambda([]() -> FReply { return FReply::Handled(); })
						]
					]
				]
			]

			// ===== 下半：Graph（高度不受折叠影响）=====
			+ SSplitter::Slot()
			.Value(0.75f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					PerformanceGraph
				]
				// place hover widget using dynamic padding so it follows graph-local coords
				+ SOverlay::Slot()
				.Padding(TAttribute<FMargin>::CreateLambda([HoverPos]() { return FMargin(HoverPos->X, HoverPos->Y, 0, 0); }))
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Top)
				[
					SAssignNew(HoverWidget, SFrameHoverWidget)
				]


			]
		]
		// ===== 上半：List =====


		+ SVerticalBox::Slot()
		.AutoHeight()     
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8, 2)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.BorderBackgroundColor(GetCurveColor(EPerfCurve::Frame))
					.Padding(FMargin(6, 6))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Frame")))
				]
			]

			// Game
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8, 2)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.BorderBackgroundColor(GetCurveColor(EPerfCurve::Game))
					.Padding(FMargin(6, 6))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Game")))
				]
			]

			// Draw
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8, 2)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.BorderBackgroundColor(GetCurveColor(EPerfCurve::Draw))
					.Padding(FMargin(6, 6))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Draw")))
				]
			]

			// RHI
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8, 2)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.BorderBackgroundColor(GetCurveColor(EPerfCurve::RHI))
					.Padding(FMargin(6, 6))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("RHI")))
				]
			]

			// GPU
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8, 2)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.BorderBackgroundColor(GetCurveColor(EPerfCurve::GPU))
					.Padding(FMargin(6, 6))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("GPU")))
				]
			]
		]


		// ===== Bottom stats bar =====
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8, 4)
		[
			SNew(SBorder)
			.Padding(6)
			.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Thread Stats (click legend to show/hide)")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				]

				// Legend row
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 4, 0, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Legend:")))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(6,0,0,0)
					[
						SNew(SHorizontalBox)
						// Dynamic buttons
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SHorizontalBox)
							// This inner box will be rebuilt by lambda into text blocks with separators.
							+ SHorizontalBox::Slot().FillWidth(1.0f)
							[
								SNew(STextBlock)
								.Text_Lambda([SelectedItem]()
								{
									TSharedPtr<FSampledGraphData> Item = SelectedItem.IsValid() ? *SelectedItem : nullptr;
									return Item.IsValid() ? FText::FromString(TEXT("")) : FText::FromString(TEXT("(select a test)"));
								})
							]
						]
					]
				]

				// Whole capture
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 4, 0, 0)
				[
					SNew(STextBlock)
					.Text_Lambda([SelectedItem, VisibleThreads]()
					{
						TSharedPtr<FSampledGraphData> Item = SelectedItem.IsValid() ? *SelectedItem : nullptr;
						if (!Item.IsValid())
						{
							return FText::FromString(TEXT("Whole Capture: No selection"));
						}
						if (Item->FrameData.Num() == 0)
						{
							return FText::FromString(TEXT("Whole Capture: No FrameData"));
						}

						struct FAcc { double Sum = 0.0; int32 Count = 0; float Min = 0.f; float Max = 0.f; bool bInit = false; };
						TMap<FString, FAcc> Acc;
						for (const FSampledFrameData& F : Item->FrameData)
						{
							for (const FThreadSample& T : F.ThreadData)
							{
								if (VisibleThreads.IsValid() && VisibleThreads->Num() > 0 && !VisibleThreads->Contains(T.ThreadName))
								{
									continue;
								}
								FAcc& A = Acc.FindOrAdd(T.ThreadName);
								A.Sum += (double)T.TimeMs;
								A.Count += 1;
								if (!A.bInit)
								{
									A.Min = T.TimeMs;
									A.Max = T.TimeMs;
									A.bInit = true;
								}
								else
								{
									A.Min = FMath::Min(A.Min, T.TimeMs);
									A.Max = FMath::Max(A.Max, T.TimeMs);
								}
							}
						}
						if (Acc.Num() == 0)
						{
							return FText::FromString(TEXT("Whole Capture: No ThreadData recorded (or filtered out)"));
						}

						TArray<TPair<FString, FAcc>> Pairs;
						for (const TPair<FString, FAcc>& KV : Acc)
						{
							Pairs.Add(KV);
						}
						Pairs.Sort([](const TPair<FString, FAcc>& A, const TPair<FString, FAcc>& B)
						{
							const double AvgA = A.Value.Count > 0 ? A.Value.Sum / (double)A.Value.Count : 0.0;
							const double AvgB = B.Value.Count > 0 ? B.Value.Sum / (double)B.Value.Count : 0.0;
							return AvgA > AvgB;
						});

						FString Out(TEXT("Whole Capture: "));
						for (int32 i = 0; i < Pairs.Num(); ++i)
						{
							const FString& Name = Pairs[i].Key;
							const FAcc& A = Pairs[i].Value;
							const float Avg = A.Count > 0 ? (float)(A.Sum / (double)A.Count) : 0.f;
							Out += FString::Printf(TEXT("%s Avg %.2f | Min %.2f | Max %.2f"), *Name, Avg, A.Min, A.Max);
							if (i != Pairs.Num() - 1)
							{
								Out += TEXT("    ||    ");
							}
						}
						return FText::FromString(Out);
					})
					.AutoWrapText(true)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				]

				// Range stats (filtered)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 4, 0, 0)
				[
					SNew(STextBlock)
					.Text_Lambda([SelectedItem, RangeStartIndex, RangeEndIndex, VisibleThreads]()
					{
						TSharedPtr<FSampledGraphData> Item = SelectedItem.IsValid() ? *SelectedItem : nullptr;
						if (!Item.IsValid())
						{
							return FText::FromString(TEXT("Range: No selection"));
						}
						if (Item->FrameData.Num() == 0)
						{
							return FText::FromString(TEXT("Range: No FrameData"));
						}

						int32 SIdx = RangeStartIndex.IsValid() ? *RangeStartIndex : INDEX_NONE;
						int32 EIdx = RangeEndIndex.IsValid() ? *RangeEndIndex : INDEX_NONE;
						if (SIdx == INDEX_NONE || EIdx == INDEX_NONE)
						{
							return FText::FromString(TEXT("Range: (drag on graph to select)"));
						}
						if (SIdx > EIdx)
						{
							Swap(SIdx, EIdx);
						}

						SIdx = FMath::Clamp(SIdx, 0, Item->FrameData.Num() - 1);
						EIdx = FMath::Clamp(EIdx, 0, Item->FrameData.Num() - 1);
						if (SIdx > EIdx)
						{
							return FText::FromString(TEXT("Range: invalid"));
						}

						struct FAcc { double Sum = 0.0; int32 Count = 0; float Min = 0.f; float Max = 0.f; bool bInit = false; };
						TMap<FString, FAcc> Acc;
						for (int32 i = SIdx; i <= EIdx; ++i)
						{
							const FSampledFrameData& F = Item->FrameData[i];
							for (const FThreadSample& T : F.ThreadData)
							{
								if (VisibleThreads.IsValid() && VisibleThreads->Num() > 0 && !VisibleThreads->Contains(T.ThreadName))
								{
									continue;
								}
								FAcc& A = Acc.FindOrAdd(T.ThreadName);
								A.Sum += (double)T.TimeMs;
								A.Count += 1;
								if (!A.bInit)
								{
									A.Min = T.TimeMs;
									A.Max = T.TimeMs;
									A.bInit = true;
								}
								else
								{
									A.Min = FMath::Min(A.Min, T.TimeMs);
									A.Max = FMath::Max(A.Max, T.TimeMs);
								}
							}
						}

						if (Acc.Num() == 0)
						{
							return FText::FromString(FString::Printf(TEXT("Range [%d..%d]: No ThreadData"), SIdx, EIdx));
						}

						TArray<TPair<FString, FAcc>> Pairs;
						for (const TPair<FString, FAcc>& KV : Acc)
						{
							Pairs.Add(KV);
						}
						Pairs.Sort([](const TPair<FString, FAcc>& A, const TPair<FString, FAcc>& B)
						{
							const double AvgA = A.Value.Count > 0 ? A.Value.Sum / (double)A.Value.Count : 0.0;
							const double AvgB = B.Value.Count > 0 ? B.Value.Sum / (double)B.Value.Count : 0.0;
							return AvgA > AvgB;
						});

						FString Out = FString::Printf(TEXT("Range [%d..%d]: "), SIdx, EIdx);
						for (int32 i = 0; i < Pairs.Num(); ++i)
						{
							const FString& Name = Pairs[i].Key;
							const FAcc& A = Pairs[i].Value;
							const float Avg = A.Count > 0 ? (float)(A.Sum / (double)A.Count) : 0.f;
							Out += FString::Printf(TEXT("%s Avg %.2f | Min %.2f | Max %.2f"), *Name, Avg, A.Min, A.Max);
							if (i != Pairs.Num() - 1)
							{
								Out += TEXT("    ||    ");
							}
						}
						return FText::FromString(Out);
					})
					.AutoWrapText(true)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				]
			]
		]

	);

	// Bind graph hover delegate to update hover widget
	{
		TWeakPtr<SFrameHoverWidget> WeakHover = HoverWidget;
		TWeakPtr<SPerformanceGraph> WeakGraph = PerformanceGraph;
		TSharedPtr<FVector2D> LocalHoverPos = HoverPos;
		PerformanceGraph->OnHoverSampleChanged.BindLambda([WeakHover, WeakGraph, LocalHoverPos, VisibleThreads, RangeStatsCache](int32 Index, FVector2D Local)
		{
			TSharedPtr<SFrameHoverWidget> HW = WeakHover.Pin();
			TSharedPtr<SPerformanceGraph> PG = WeakGraph.Pin();
			if (!HW.IsValid() || !PG.IsValid())
				return;

			// keep hover widget filtered according to legend
			HW->SetVisibleThreadFilter(VisibleThreads);

			// keep range stats displayed in hover
			if (RangeStatsCache.IsValid() && RangeStatsCache->bHasRange)
			{
				HW->SetRangeStats(*RangeStatsCache);
			}
			else
			{
				HW->ClearRangeStats();
			}

			// Always update hover position so tooltip follows cursor
			LocalHoverPos->X = Local.X + 10.0f;
			LocalHoverPos->Y = Local.Y + 10.0f;

			if (Index != INDEX_NONE)
			{
				if (PG->SampledFrameData.IsValidIndex(Index))
				{
					HW->SetFrameData(&PG->SampledFrameData[Index], Index);
					HW->SetVisibility(EVisibility::Visible);
				}
				else
				{
					HW->SetFrameData(nullptr, INDEX_NONE);
					HW->SetVisibility(EVisibility::Collapsed);
				}
			}
			else
			{
				// If hover locked, keep visible; otherwise hide
				if (!PG->IsHoverLocked())
				{
					HW->SetVisibility(EVisibility::Collapsed);
				}
			}
		});
	}

	// Bind selection range delegate
	PerformanceGraph->OnSelectionRangeChanged.BindLambda([RangeStartIndex, RangeEndIndex, Window, SelectedItem, RangeStatsCache, WeakHover = TWeakPtr<SFrameHoverWidget>(HoverWidget), WeakGraph = TWeakPtr<SPerformanceGraph>(PerformanceGraph)](int32 Start, int32 End)
	{
		// Treat single-point range (X..X) as cancel selection
		if (Start != INDEX_NONE && End != INDEX_NONE && Start == End)
		{
			*RangeStartIndex = INDEX_NONE;
			*RangeEndIndex = INDEX_NONE;

			if (TSharedPtr<SPerformanceGraph> PG = WeakGraph.Pin())
			{
				PG->ClearSelection();
			}

			if (RangeStatsCache.IsValid())
			{
				*RangeStatsCache = SFrameHoverWidget::FRangeStats{};
			}
			if (TSharedPtr<SFrameHoverWidget> HW = WeakHover.Pin())
			{
				HW->ClearRangeStats();
			}

			Window->Invalidate(EInvalidateWidget::LayoutAndVolatility);
			return;
		}

		*RangeStartIndex = Start;
		*RangeEndIndex = End;

		// compute and push range stats to hover widget
		TSharedPtr<FSampledGraphData> Item = SelectedItem.IsValid() ? *SelectedItem : nullptr;
		TSharedPtr<SFrameHoverWidget> HW = WeakHover.Pin();

		if (!Item.IsValid() || Item->FrameData.Num() == 0 || Start == INDEX_NONE || End == INDEX_NONE)
		{
			if (RangeStatsCache.IsValid())
			{
				*RangeStatsCache = SFrameHoverWidget::FRangeStats{};
			}
			if (HW.IsValid())
			{
				HW->ClearRangeStats();
			}
			Window->Invalidate(EInvalidateWidget::LayoutAndVolatility);
			return;
		}

		int32 SIdx = Start;
		int32 EIdx = End;
		if (SIdx > EIdx)
		{
			Swap(SIdx, EIdx);
		}
		SIdx = FMath::Clamp(SIdx, 0, Item->FrameData.Num() - 1);
		EIdx = FMath::Clamp(EIdx, 0, Item->FrameData.Num() - 1);
		if (SIdx > EIdx)
		{
			if (RangeStatsCache.IsValid())
			{
				*RangeStatsCache = SFrameHoverWidget::FRangeStats{};
			}
			if (HW.IsValid())
			{
				HW->ClearRangeStats();
			}
			Window->Invalidate(EInvalidateWidget::LayoutAndVolatility);
			return;
		}

		float SumMs = 0.f;
		float MinMs = FLT_MAX;
		float MaxMs = -FLT_MAX;
		int32 Count = 0;
		for (int32 i = SIdx; i <= EIdx; ++i)
		{
			const float Ms = Item->FrameData[i].FrameMS;
			SumMs += Ms;
			MinMs = FMath::Min(MinMs, Ms);
			MaxMs = FMath::Max(MaxMs, Ms);
			++Count;
		}

		SFrameHoverWidget::FRangeStats Stats;
		Stats.bHasRange = (Count > 0);
		Stats.StartIndex = SIdx;
		Stats.EndIndex = EIdx;
		Stats.NumFrames = Count;
		Stats.AvgFrameMs = Count > 0 ? (SumMs / (float)Count) : 0.f;
		Stats.MinFrameMs = (Count > 0 && MinMs != FLT_MAX) ? MinMs : 0.f;
		Stats.MaxFrameMs = (Count > 0 && MaxMs != -FLT_MAX) ? MaxMs : 0.f;
		Stats.AvgFPS = Stats.AvgFrameMs > KINDA_SMALL_NUMBER ? (1000.0f / Stats.AvgFrameMs) : 0.0f;

		if (RangeStatsCache.IsValid())
		{
			*RangeStatsCache = Stats;
		}
		if (HW.IsValid())
		{
			HW->SetRangeStats(Stats);
		}

		Window->Invalidate(EInvalidateWidget::LayoutAndVolatility);
	});

	if (ListItems.Num() > 0 && ListView.IsValid())
	{
		ListView->SetSelection(ListItems[0]);
	}
	// ===== 多曲线选择 =====

	PerformanceGraph->SetVisibleCurves({
		EPerfCurve::Frame,
		EPerfCurve::Game,
		EPerfCurve::Draw,
		EPerfCurve::GPU
	});


	// Position the window close to the right edge of the primary work area before adding it.
	{
		FDisplayMetrics DisplayMetrics;
		FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);

		// Work area rectangle (left, top, right, bottom)
		const int32 WorkLeft = DisplayMetrics.PrimaryDisplayWorkAreaRect.Left;
		const int32 WorkTop = DisplayMetrics.PrimaryDisplayWorkAreaRect.Top;
		const int32 WorkRight = DisplayMetrics.PrimaryDisplayWorkAreaRect.Right;
		// const int32 WorkBottom = DisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom;

		const float WorkW = (float)(WorkRight - WorkLeft);
		// margin from right edge
		const float MarginRight = 40.0f;
		// top offset
		const float TopOffset = 80.0f;

		// Compute desired X/Y so the window hugs the right side with margin
		const float PosX = WorkLeft + FMath::Max(0.0f, WorkW - PT_PERFORMANCE_GRAPH_WINDOW_SIZE_X - MarginRight);
		const float PosY = WorkTop + TopOffset;

		Window->MoveWindowTo(FVector2D(PosX, PosY));
	}

	FSlateApplication::Get().AddWindow(Window);
}

