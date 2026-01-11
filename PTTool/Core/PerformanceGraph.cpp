#include "PerformanceGraph.h"
#include "Rendering/DrawElements.h"
#include "PTPerformanceSampler.h"

static float GetCurveValue(const FSampledFrameData& S, EPerfCurve Curve)
{
	switch (Curve)
	{
	case EPerfCurve::Frame:
		return S.FrameMS;
	case EPerfCurve::Game:
		return S.GameMS;
	case EPerfCurve::Draw:
		return S.DrawMS;
	case EPerfCurve::RHI:
		return S.RHITMS;
	case EPerfCurve::GPU:
		return S.GPUMS;
	default:
		return 0.f;
	}
}



int32 SPerformanceGraph::OnPaint(const FPaintArgs& Args,const FGeometry& Geo,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const
{
    const int32 NumSamples = SampledFrameData.Num();

    // Diagnostic log once per paint call (verbose)
    UE_LOG(LogTemp, VeryVerbose, TEXT("SPerformanceGraph::OnPaint called. NumSamples=%d, SampleTimes=%d, ViewStart=%d, ViewCount=%d"), NumSamples, SampleTimes.Num(), ViewStart, ViewCount);

    // Strong debug overlay: semi-transparent red full-width bar at top and large orange text




    if (NumSamples < 2 || VisibleCurves.Num() == 0)
        return Layer;

	const FVector2D Size = Geo.GetLocalSize();
	const float W = Size.X;
	const float H = Size.Y;

	// ================== Plot 区域定义 ==================
	//距离边缘的距离
	constexpr float MarginL = 55.f; // Y轴文字
	constexpr float MarginB = 28.f; // X轴文字
	constexpr float MarginT = 20.f;
	constexpr float MarginR = 48.f;

	//计算曲线graph区域
	const float PlotL = MarginL;
	const float PlotR = W - MarginR;
	const float PlotT = MarginT;
	const float PlotB = H - MarginB;

	// Determine visible range based on ViewStart/ViewCount and Zoom
	int32 StartIndex = 0;
	int32 EndIndex = NumSamples - 1;
	if (ViewCount > 0 && ViewCount <= NumSamples)
	{
		StartIndex = FMath::Clamp(ViewStart, 0, NumSamples - 1);
		EndIndex = FMath::Clamp(ViewStart + ViewCount - 1, 0, NumSamples - 1);
	}
	else
	{
		StartIndex = 0;
		EndIndex = NumSamples - 1;
	}
	const int32 VisibleCount = FMath::Max(1, EndIndex - StartIndex + 1);

	// ================== 计算时间范围（ms） ==================
	double TimeStart = 0.0;
	double TimeEnd = 0.0;
	if (SampleTimes.Num() == NumSamples)
	{
		TimeStart = SampleTimes[StartIndex];
		// End is start time of last visible sample plus its frame duration
		TimeEnd = SampleTimes[EndIndex] + SampledFrameData[EndIndex].FrameMS;
	}
	else
	{
		// Fallback: evenly spaced by index
		TimeStart = 0.0;
		TimeEnd = (double)VisibleCount;
	}
	const double TimeRange = FMath::Max(1e-6, TimeEnd - TimeStart);

	// ================== 计算 Y 轴最小/最大值（仅在可见区间内） ==================
	float MaxMs = -FLT_MAX;
	float MinMs = FLT_MAX;

	for (int32 i = StartIndex; i <= EndIndex; ++i)
	{
		const auto& S = SampledFrameData[i];
		for (EPerfCurve C : VisibleCurves)
		{
			float V = GetCurveValue(S, C);
			MaxMs = FMath::Max(MaxMs, V);
			MinMs = FMath::Min(MinMs, V);
		}
	}

	// 防护：如果没有有效值，或者 Min/Max 相等，扩展一个小范围以便绘制
	if (MaxMs < 0.f && MinMs == FLT_MAX)
	{
		// 未找到值，回退到默认
		MinMs = 0.0f;
		MaxMs = 1.0f;
	}
	if (MinMs == FLT_MAX)
		MinMs = 0.0f;
	if (MaxMs <= MinMs)
	{
		// 让绘图区间有最小高度
		const float fallbackRange = FMath::Max(0.01f, FMath::Abs(MaxMs) * 0.1f + 0.01f);
		MinMs = MinMs - fallbackRange * 0.5f;
		MaxMs = MaxMs + fallbackRange * 0.5f;
	}
	// 添加 5% padding 两侧，以免曲线贴边
	{
		const float Range = MaxMs - MinMs;
		const float Pad = Range * 0.05f;
		MinMs = MinMs - Pad;
		MaxMs = MaxMs + Pad;
		if (MinMs < 0.0f)
		{
			// 如果原始值都 >=0，可以保留 0 为下边界（可根据需求调整）
			MinMs = FMath::Max(0.0f, MinMs);
		}
	}

	// ================== 字体 ==================
		const FSlateFontInfo Font =
		FCoreStyle::GetDefaultFontStyle("Regular", 10);

	// ====================================================
	// 1️⃣ 坐标轴（左 + 下）
	// ====================================================
	{
		TArray<FVector2D> Axis;
		Axis.Add(FVector2D(PlotL, PlotT));
		Axis.Add(FVector2D(PlotL, PlotB));
		Axis.Add(FVector2D(PlotL, PlotB));
		Axis.Add(FVector2D(PlotR, PlotB));

		FSlateDrawElement::MakeLines(
			Out,
			Layer,
			Geo.ToPaintGeometry(),
			Axis,
			ESlateDrawEffect::None,
			FLinearColor(0.5f, 0.5f, 0.5f),
			true,
			1.0f
		);
		Layer++;
	}

	// ====================================================
	// 2️⃣ Y 轴刻度（ms） - 使用 MinMs..MaxMs 区间
	// ====================================================
	{
		const int32 NumTicks = 5;
		for (int32 i = 0; i <= NumTicks; ++i)
		{
			const float Alpha = (float)i / NumTicks;
			const float Y = FMath::Lerp(PlotB, PlotT, Alpha);

			// 小刻度
			TArray<FVector2D> Tick;
			Tick.Add(FVector2D(PlotL - 4.f, Y));
			Tick.Add(FVector2D(PlotL, Y));

			FSlateDrawElement::MakeLines(
				Out,
				Layer,
				Geo.ToPaintGeometry(),
				Tick,
				ESlateDrawEffect::None,
				FLinearColor(0.6f, 0.6f, 0.6f),
				true,
				1.0f
			);

			// 标签：从 MinMs 到 MaxMs
			const float ValueMs = FMath::Lerp(MinMs, MaxMs, Alpha);
			const FString Label = FString::Printf(TEXT("%.2f ms"), ValueMs);

			FSlateDrawElement::MakeText(
				Out,
				Layer,
				Geo.ToPaintGeometry(
					FVector2D(2.f, Y - 6.f),
					FVector2D(1.f, 1.f)
				),
				Label,
				Font,
				ESlateDrawEffect::None,
				FLinearColor(0.85f, 0.85f, 0.85f)
			);
		}
		Layer++;
	}

	// ====================================================
	// 3️⃣ X 轴刻度（Time）
	// ====================================================
	{
		const int32 NumTicks = 5;

		for (int32 i = 0; i <= NumTicks; ++i)
		{
			const float Alpha = (float)i / NumTicks;
			const float X = FMath::Lerp(PlotL, PlotR, Alpha);

			// 小刻度
			TArray<FVector2D> Tick;
			Tick.Add(FVector2D(X, PlotB));
			Tick.Add(FVector2D(X, PlotB + 4.f));

			FSlateDrawElement::MakeLines(
				Out,
				Layer,
				Geo.ToPaintGeometry(),
				Tick,
				ESlateDrawEffect::None,
				FLinearColor(0.6f, 0.6f, 0.6f),
				true,
				1.0f
			);

			// 时间标签：TimeStart + Alpha * TimeRange
			const double TimeMs = TimeStart + Alpha * TimeRange;
			FString TimeLabel;
			if (TimeMs >= 1000.0)
			{
				TimeLabel = FString::Printf(TEXT("%.2fs"), TimeMs / 1000.0);
			}
			else
			{
				TimeLabel = FString::Printf(TEXT("%.1fms"), TimeMs);
			}

			FSlateDrawElement::MakeText(
				Out,
				Layer,
				Geo.ToPaintGeometry(
					FVector2D(X - 20.f, PlotB + 6.f),
					FVector2D(1.f, 1.f)
				),
				TimeLabel,
				Font,
				ESlateDrawEffect::None,
				FLinearColor(0.85f, 0.85f, 0.85f)
			);
		}
		Layer++;
	}

	// ====================================================
	// 4️⃣ 曲线绘制（严格限制在 Plot 区域），使用 MinMs..MaxMs 映射 Y
	// ====================================================
	for (EPerfCurve Curve : VisibleCurves)
	{
		TArray<FVector2D> Points;
		Points.Reserve(VisibleCount);

		float SmoothRadiusSamples = TargetSmoothPx	 / FMath::Max(1.0f, (float)VisibleCount / (PlotR - PlotL));
		int32 SmoothRadius = FMath::Max(0, FMath::RoundToInt(SmoothRadiusSamples));

		for (int32 i = 0; i < VisibleCount; ++i)
		{
			int32 GlobalIndex = StartIndex + i;
			float Acc = 0.0f;
			int32 Count = 0;

			for (int32 k = -SmoothRadius; k <= SmoothRadius; ++k)
			{
				const int32 Index = GlobalIndex + k;
				if (Index >= StartIndex && Index <= EndIndex)
				{
					Acc += GetCurveValue(SampledFrameData[Index], Curve);
					++Count;
				}
			}

			const float V = (Count > 0) ? (Acc / Count) : 0.0f;

			// X position based on time
			double SampleTime = TimeStart;
			if (SampleTimes.Num() == NumSamples)
			{
				SampleTime = SampleTimes[GlobalIndex];
			}
			const float AlphaX = (float)((SampleTime - TimeStart) / TimeRange);
			const float X = PlotL + AlphaX * (PlotR - PlotL);

			const float AlphaY = V / MaxMs;

			const float Y = PlotB - AlphaY * (PlotB - PlotT);

			Points.Add(FVector2D(X, Y));
		}

		FSlateDrawElement::MakeLines(
			Out,
			Layer,
			Geo.ToPaintGeometry(),
			Points,
			ESlateDrawEffect::None,
			GetCurveColor(Curve),
			true,
			1.5f
		);
	}

	// ====================================================
	// 4.5️⃣ Selection range overlay
	// ====================================================
	if (SelectionStartIndex != INDEX_NONE && SelectionEndIndex != INDEX_NONE && SampledFrameData.IsValidIndex(SelectionStartIndex) && SampledFrameData.IsValidIndex(SelectionEndIndex))
	{
		int32 SelA = SelectionStartIndex;
		int32 SelB = SelectionEndIndex;
		if (SelA > SelB)
		{
			Swap(SelA, SelB);
		}

		// Clamp to visible window
		SelA = FMath::Clamp(SelA, StartIndex, EndIndex);
		SelB = FMath::Clamp(SelB, StartIndex, EndIndex);

		if (SelA <= SelB)
		{
			auto IndexToX = [&](int32 Idx) -> float
			{
				if (SampleTimes.Num() == NumSamples)
				{
					const double ST = SampleTimes.IsValidIndex(Idx) ? SampleTimes[Idx] : TimeStart;
					const float AX = (float)((ST - TimeStart) / TimeRange);
					return PlotL + AX * (PlotR - PlotL);
				}
				// fallback: by index
				const int32 Span = FMath::Max(1, EndIndex - StartIndex);
				const float AX = (float)(Idx - StartIndex) / (float)Span;
				return PlotL + AX * (PlotR - PlotL);
			};

			const float X1 = IndexToX(SelA);
			const float X2 = IndexToX(SelB);
			const float Left = FMath::Min(X1, X2);
			const float Right = FMath::Max(X1, X2);

			FSlateDrawElement::MakeBox(
				Out,
				Layer,
				Geo.ToPaintGeometry(FVector2D(Left, PlotT), FVector2D(FMath::Max(1.0f, Right - Left), PlotB - PlotT)),
				FCoreStyle::Get().GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				FLinearColor(0.2f, 0.6f, 1.0f, 0.15f)
			);
			Layer++;
		}
	}

	// ====================================================
	// 5️⃣ Hover || Selected frame vertical line and marker
	// ====================================================
	{
		// Only draw when we have hover info
		if (bHasHover || HoveredIndex != INDEX_NONE)
		{
			float LineX = -1.f;

			// If we have a valid hovered index, compute X based on sample time
			if (HoveredIndex != INDEX_NONE && SampledFrameData.IsValidIndex(HoveredIndex))
			{
				if (SampleTimes.Num() == NumSamples)
				{
					double SampleTime = (SampleTimes.IsValidIndex(HoveredIndex) ? SampleTimes[HoveredIndex] : 0.0);
					const float AlphaX = (float)((SampleTime - TimeStart) / TimeRange);
					LineX = PlotL + AlphaX * (PlotR - PlotL);
				}
				else
				{
					// Fallback: map by index evenly if precise SampleTimes unavailable
					int32 ClampedIndex = FMath::Clamp(HoveredIndex, StartIndex, EndIndex);
					const int32 IndexSpan = FMath::Max(1, EndIndex - StartIndex);
					const float AlphaIndex = (float)(ClampedIndex - StartIndex) / (float)IndexSpan;
					LineX = PlotL + AlphaIndex * (PlotR - PlotL);
				}
			}
			else
			{
				// fallback: use current mouse local X
				LineX = HoverLocal.X;
			}

			// ensure the line is inside plot area
			if (LineX >= PlotL - 1.0f && LineX <= PlotR + 1.0f)
			{
				TArray<FVector2D> VLine;
				VLine.Add(FVector2D(LineX, PlotT));
				VLine.Add(FVector2D(LineX, PlotB));

				// Draw vertical line (slightly bright color)
				FSlateDrawElement::MakeLines(
					Out,
					Layer,
					Geo.ToPaintGeometry(),
					VLine,
					ESlateDrawEffect::None,
					FLinearColor(1.0f, 0.85f, 0.2f, 0.9f),
					true,
					1.8f
				);

				// Draw a small marker at the frame value (use Frame curve value if available)
				if (SampledFrameData.IsValidIndex(HoveredIndex))
				{
					float FrameValue = SampledFrameData[HoveredIndex].FrameMS;
					// map to Y
					const float AlphaYVal = FrameValue / MaxMs;
					const float YPos = PlotB - AlphaYVal * (PlotB - PlotT);
					// small cross marker
					TArray<FVector2D> Cross;
					Cross.Add(FVector2D(LineX - 4.0f, YPos - 4.0f));
					Cross.Add(FVector2D(LineX + 4.0f, YPos + 4.0f));
					Cross.Add(FVector2D(LineX - 4.0f, YPos + 4.0f));
					Cross.Add(FVector2D(LineX + 4.0f, YPos - 4.0f));
					FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Cross, ESlateDrawEffect::None, FLinearColor::White, true, 1.2f);
				}
			}
		}
	}



    return Layer + 1;
}


FReply SPerformanceGraph::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	UE_LOG(LogTemp, Warning, TEXT("SPerformanceGraph::OnMouseButtonDown Button=%s"), *MouseEvent.GetEffectingButton().GetFName().ToString());
	const FKey Button = MouseEvent.GetEffectingButton();

	// Selection: Left mouse drag selects a range (for range stats)
	if (Button == EKeys::LeftMouseButton && !MouseEvent.IsControlDown())
	{
		SelectStartLocal = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

		const int32 StartIdx = LocalXToSampleIndex(MyGeometry, SelectStartLocal.X);
		if (StartIdx == INDEX_NONE)
		{
			// Clicked outside plot or no samples.
			return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Mouse);
		}

		bIsSelecting = true;
		SelectionStartIndex = StartIdx;
		SelectionEndIndex = StartIdx;

		// Don't broadcast OnSelectionRangeChanged here.
		// The initial state is (X..X) and the window treats that as "cancel selection".
		// We'll broadcast on mouse-move and mouse-up when the range is meaningful.
		Invalidate(EInvalidateWidget::Paint);
		return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Mouse).CaptureMouse(AsShared());
	}

	// Panning: Middle or Right mouse drag
	const bool bStartPan = (Button == EKeys::MiddleMouseButton) || (Button == EKeys::RightMouseButton);
	if (bStartPan)
	{
		PanStartLocal = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		PanStartViewStart = ViewStart;
		bIsPanning = true;
		PanMouseButton = Button;

		UE_LOG(LogTemp, Warning, TEXT("SPerformanceGraph: begin panning at %s, ViewStart=%d"), *PanStartLocal.ToString(), PanStartViewStart);
		return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Mouse).CaptureMouse(AsShared());
	}

	// Hover lock toggle: Ctrl+Left click
	if (Button == EKeys::LeftMouseButton && MouseEvent.IsControlDown())
	{
		bHoverLocked = !bHoverLocked;
		if (OnHoverSampleChanged.IsBound())
		{
			OnHoverSampleChanged.Execute(HoveredIndex, HoverLocal);
		}
		Invalidate(EInvalidateWidget::Paint);
		return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Mouse);
	}

	return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Mouse);
}

FReply SPerformanceGraph::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	UE_LOG(LogTemp, Warning, TEXT("SPerformanceGraph::OnMouseButtonUp Button=%s bIsPanning=%d bIsSelecting=%d"), *MouseEvent.GetEffectingButton().GetFName().ToString(), bIsPanning, bIsSelecting);
	const FKey Button = MouseEvent.GetEffectingButton();

	if (bIsSelecting && Button == EKeys::LeftMouseButton)
	{
		bIsSelecting = false;

		// Normalize selection order
		if (SelectionStartIndex != INDEX_NONE && SelectionEndIndex != INDEX_NONE && SelectionStartIndex > SelectionEndIndex)
		{
			Swap(SelectionStartIndex, SelectionEndIndex);
		}

		if (OnSelectionRangeChanged.IsBound())
		{
			OnSelectionRangeChanged.Execute(SelectionStartIndex, SelectionEndIndex);
		}

		Invalidate(EInvalidateWidget::Paint);
		return FReply::Handled().ReleaseMouseCapture();
	}

	if (bIsPanning && (Button == EKeys::MiddleMouseButton || Button == EKeys::RightMouseButton))
	{
		if (Button != PanMouseButton)
		{
			return FReply::Unhandled();
		}
		bIsPanning = false;
		UE_LOG(LogTemp, Warning, TEXT("SPerformanceGraph: end panning ViewStart=%d"), ViewStart);
		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

FReply SPerformanceGraph::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Update hover position
	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	HoverLocal = Local;
	bHasHover = true;

	// Compute hovered index using shared helper
	HoveredIndex = LocalXToSampleIndex(MyGeometry, Local.X);

	// Notify delegate about hover change unless hover is locked (still notify to let UI update locked position on toggle)
	if (OnHoverSampleChanged.IsBound())
	{
		OnHoverSampleChanged.Execute(HoveredIndex, HoverLocal);
	}

	// Ensure we repaint so the vertical line and any hover visuals update immediately
	Invalidate(EInvalidateWidget::Paint);

	// existing pan handling
	if (!bIsPanning && !bIsSelecting)
		return FReply::Handled();

	const int32 N = SampledFrameData.Num();
	if (N <= 0)
		return FReply::Handled();

	if (bIsSelecting)
	{
		const int32 CurrIdx = LocalXToSampleIndex(MyGeometry, Local.X);
		SelectionEndIndex = CurrIdx;
		if (OnSelectionRangeChanged.IsBound())
		{
			OnSelectionRangeChanged.Execute(SelectionStartIndex, SelectionEndIndex);
		}
		Invalidate(EInvalidateWidget::Paint);
		return FReply::Handled();
	}

	// Plot area width (must match LocalXToSampleIndex/OnPaint)
	const float W = MyGeometry.GetLocalSize().X;
	const float PlotL = 55.f;
	const float PlotR = W - 48.f;
	const float PlotWidth = FMath::Max(1.0f, PlotR - PlotL);

	// Current view count
	int32 CurrCount = ViewCount > 0 ? ViewCount : N;
	CurrCount = FMath::Clamp(CurrCount, 1, N);

	// Delta from pan start
	const float DeltaX = Local.X - PanStartLocal.X;

	// Pixel delta to sample delta (drag right => view shows earlier => start decreases)
	const float DeltaFrac = DeltaX / PlotWidth;
	const int32 DeltaSamples = FMath::RoundToInt(DeltaFrac * CurrCount);

	int32 NewStart = PanStartViewStart - DeltaSamples;
	NewStart = FMath::Clamp(NewStart, 0, FMath::Max(0, N - CurrCount));
	if (NewStart != ViewStart)
	{
		ViewStart = NewStart;
		Invalidate(EInvalidateWidget::Paint);
	}

	return FReply::Handled();
}

void SPerformanceGraph::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bHasHover = true;
	HoverLocal = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	// repaint so hover visuals (line) update
	Invalidate(EInvalidateWidget::Paint);
}

void SPerformanceGraph::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	if (bIsPanning)
	{
		// Optionally end panning when leaving
		bIsPanning = false;
		UE_LOG(LogTemp, Warning, TEXT("SPerformanceGraph::OnMouseLeave - ending panning"));
	}

	// If hover is not locked, clear hover state
	if (!bHoverLocked)
	{
		bHasHover = false;
		HoveredIndex = INDEX_NONE;
		if (OnHoverSampleChanged.IsBound())
		{
			OnHoverSampleChanged.Execute(HoveredIndex, HoverLocal);
		}
		// ensure we repaint to clear hover line
		Invalidate(EInvalidateWidget::Paint);
	}
}


void SPerformanceGraph::OnMouseCaptureLost()
{
	if (bIsPanning)
	{
		bIsPanning = false;
		UE_LOG(LogTemp, Warning, TEXT("SPerformanceGraph::OnMouseCaptureLost - ending panning"));
	}
	if (bIsSelecting)
	{
		bIsSelecting = false;
	}
}

void SPerformanceGraph::ClearSelection()
{
	SelectionStartIndex = INDEX_NONE;
	SelectionEndIndex = INDEX_NONE;
	bIsSelecting = false;
	if (OnSelectionRangeChanged.IsBound())
	{
		OnSelectionRangeChanged.Execute(SelectionStartIndex, SelectionEndIndex);
	}
	Invalidate(EInvalidateWidget::Paint);
}

FReply SPerformanceGraph::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const int32 N = SampledFrameData.Num();
	if (N <= 0)
		return FReply::Unhandled();

	const float Wheel = MouseEvent.GetWheelDelta();
	if (FMath::IsNearlyZero(Wheel))
		return FReply::Unhandled();

	// Current view range
	int32 CurrCount = ViewCount > 0 ? ViewCount : N;
	CurrCount = FMath::Clamp(CurrCount, 1, N);

	// Mouse local X (used for stable zoom)
	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	// Determine click index (time-based when SampleTimes exists) using shared helper.
	const int32 StartIndex = FMath::Clamp(ViewStart, 0, FMath::Max(0, N - 1));
	const int32 EndIndex = FMath::Clamp(StartIndex + CurrCount - 1, 0, N - 1);

	int32 ClickIndex = LocalXToSampleIndex(MyGeometry, Local.X);
	if (ClickIndex == INDEX_NONE)
	{
		// If cursor is outside plot, zoom around the center of view.
		ClickIndex = StartIndex + (CurrCount / 2);
		ClickIndex = FMath::Clamp(ClickIndex, 0, N - 1);
	}

	// Compute click alpha within current view (prefer time-based so "keep under cursor" feels consistent)
	double TimeStart = SampleTimes.IsValidIndex(StartIndex) ? SampleTimes[StartIndex] : 0.0;
	double TimeEnd = SampleTimes.IsValidIndex(EndIndex) ? (SampleTimes[EndIndex] + SampledFrameData[EndIndex].FrameMS) : (double)(EndIndex - StartIndex + 1);
	const double TimeRange = FMath::Max(1e-6, TimeEnd - TimeStart);
	const double ClickTime = SampleTimes.IsValidIndex(ClickIndex) ? SampleTimes[ClickIndex] : (TimeStart + 0.5 * TimeRange);
	float ClickAlpha = (float)((ClickTime - TimeStart) / TimeRange);
	ClickAlpha = FMath::Clamp(ClickAlpha, 0.0f, 1.0f);

	// Compute new ViewCount based on wheel (simple multiplicative zoom)
	const float ZoomInFactor = 0.8f;   // wheel > 0 => zoom in
	const float ZoomOutFactor = 1.25f; // wheel < 0 => zoom out
	const float Factor = Wheel > 0 ? FMath::Pow(ZoomInFactor, Wheel) : FMath::Pow(ZoomOutFactor, -Wheel);
	const int32 NewCount = FMath::Clamp(FMath::Max(1, FMath::RoundToInt(CurrCount * Factor)), 1, N);

	// Compute new start so ClickIndex remains at same relative position
	int32 NewStart = ClickIndex - FMath::RoundToInt(ClickAlpha * (NewCount - 1));
	NewStart = FMath::Clamp(NewStart, 0, FMath::Max(0, N - NewCount));

	ViewCount = NewCount;
	ViewStart = NewStart;
	Invalidate(EInvalidateWidget::Paint);
	return FReply::Handled();
}


// New helpers implemented below

double SPerformanceGraph::LocalXToTime(const FGeometry& MyGeometry, float LocalX) const
{
	const float W = MyGeometry.GetLocalSize().X;
	const float PlotL = 55.f;
	const float PlotR = W - 48.f;
	if (LocalX < PlotL || LocalX > PlotR)
		return 0.0;

	const float Alpha = (LocalX - PlotL) / FMath::Max(1.0f, PlotR - PlotL);
	const int32 N = SampledFrameData.Num();
	int32 StartIndex = 0;
	int32 EndIndex = N - 1;
	if (ViewCount > 0 && ViewCount <= N)
	{
		StartIndex = FMath::Clamp(ViewStart, 0, N - 1);
		EndIndex = FMath::Clamp(ViewStart + ViewCount - 1, 0, N - 1);
	}
	else
	{
		StartIndex = 0;
		EndIndex = N - 1;
	}
	double TimeStart = SampleTimes.IsValidIndex(StartIndex) ? SampleTimes[StartIndex] : 0.0;
	double TimeEnd = SampleTimes.IsValidIndex(EndIndex) ? (SampleTimes[EndIndex] + SampledFrameData[EndIndex].FrameMS) : (double)(EndIndex - StartIndex + 1);
	const double TimeRange = FMath::Max(1e-6, TimeEnd - TimeStart);
	return TimeStart + Alpha * TimeRange;
}

int32 SPerformanceGraph::LocalXToSampleIndex(const FGeometry& MyGeometry, float LocalX) const
{
	const float W = MyGeometry.GetLocalSize().X;
	const float PlotL = 55.f;
	const float PlotR = W - 48.f;
	if (LocalX < PlotL || LocalX > PlotR || SampledFrameData.Num() == 0)
		return INDEX_NONE;

	const float Alpha = (LocalX - PlotL) / FMath::Max(1.0f, PlotR - PlotL);
	const int32 N = SampledFrameData.Num();
	int32 StartIndex = FMath::Clamp(ViewStart, 0, FMath::Max(0, N - 1));
	int32 EndIndex = StartIndex + FMath::Max(1, ViewCount) - 1;
	EndIndex = FMath::Clamp(EndIndex, 0, N - 1);
	double TimeStart = SampleTimes.IsValidIndex(StartIndex) ? SampleTimes[StartIndex] : 0.0;
	double TimeEnd = SampleTimes.IsValidIndex(EndIndex) ? (SampleTimes[EndIndex] + SampledFrameData[EndIndex].FrameMS) : (double)(EndIndex - StartIndex + 1);
	double ClickTime = TimeStart + Alpha * FMath::Max(1e-6, TimeEnd - TimeStart);

	// binary search for greatest index with time <= ClickTime
	int32 Low = StartIndex, High = EndIndex, Best = StartIndex;
	while (Low <= High)
	{
		int32 Mid = (Low + High) / 2;
		double MidTime = SampleTimes.IsValidIndex(Mid) ? SampleTimes[Mid] : 0.0;
		if (MidTime <= ClickTime)
		{
			Best = Mid;
			Low = Mid + 1;
		}
		else
		{
			High = Mid - 1;
		}
	}
	return Best;
}

