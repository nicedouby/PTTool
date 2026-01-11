#pragma once

#include "CoreMinimal.h"
#include "Styling/CoreStyle.h"  
#include "Widgets/SLeafWidget.h"
#include "PTDataType.h"
enum class EPerfCurve : uint8
{
	Frame,
	Game,
	Draw,
	RHI,
	GPU
};
static FLinearColor GetCurveColor(EPerfCurve Curve)
{
	switch (Curve)
	{
	case EPerfCurve::Frame:
		return FLinearColor::White;
	case EPerfCurve::Game:
		return FLinearColor::Green;
	case EPerfCurve::Draw:
		return FLinearColor::Blue;
	case EPerfCurve::RHI:
		return FLinearColor::Yellow;
	case EPerfCurve::GPU:
		return FLinearColor::Red;
	default:
		return FLinearColor::Gray;
	}
}
// 前向声明，避免 include 依赖爆炸
struct FSampledFrameData;

class SPerformanceGraph : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SPerformanceGraph) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs) {}
	// zoom: 1.0 = show all samples; >1 zooms in (fewer samples visible)
	float Zoom = 1.0f;
	const float TargetSmoothPx = 0.1f;

	// View window in sample indices. ViewCount==0 means full range.
	int32 ViewStart = 0;
	int32 ViewCount = 0;
	
	void SetFrameData(const TArray<FSampledFrameData>& InData)
	{
		SampledFrameData = InData;   // Graph 内部 TArray
		ViewStart = 0;
		// Default to a reasonable initial window so panning works immediately.
		// If there are few samples, show all; otherwise show the most recent 200 samples.
		const int32 Num = SampledFrameData.Num();
		ViewCount = FMath::Min(Num, 1000000000);

		// Compute cumulative times (ms) for each sample. time[0] = 0, time[i] = sum_{j=0..i-1} FrameMS
		SampleTimes.Reset();
		SampleTimes.Reserve(Num);
		double Cum = 0.0;
		for (int32 i = 0; i < Num; ++i)
		{
			SampleTimes.Add(Cum);
			Cum += SampledFrameData[i].FrameMS;
		}
		UE_LOG(LogTemp, Display, TEXT("SampledFrameData"));
		Invalidate(EInvalidateWidget::Paint);
	}

	void SetVisibleCurves(const TSet<EPerfCurve>& InCurves)
	{
		VisibleCurves = InCurves;
	}

	// Convert a local X (widget-local coordinates) into a time in ms based on current view.
	double LocalXToTime(const FGeometry& MyGeometry, float LocalX) const;
	// Convert a local X into the nearest sample index (or INDEX_NONE).
	int32 LocalXToSampleIndex(const FGeometry& MyGeometry, float LocalX) const;

	// Delegate for hover changes: Index (or INDEX_NONE) and local position in graph widget coords
	DECLARE_DELEGATE_TwoParams(FOnHoverSampleChanged, int32 /*Index*/, FVector2D /*Local*/);
	FOnHoverSampleChanged OnHoverSampleChanged;

	// Delegate for selection range changes (inclusive indices). Start/End are INDEX_NONE when cleared.
	DECLARE_DELEGATE_TwoParams(FOnSelectionRangeChanged, int32 /*StartIndex*/, int32 /*EndIndex*/);
	FOnSelectionRangeChanged OnSelectionRangeChanged;

	// Current selection (inclusive). INDEX_NONE means no selection.
	int32 GetSelectionStart() const { return SelectionStartIndex; }
	int32 GetSelectionEnd() const { return SelectionEndIndex; }
	bool HasSelection() const { return SelectionStartIndex != INDEX_NONE && SelectionEndIndex != INDEX_NONE; }
	void ClearSelection();

	void SetHoverLocked(bool bLocked) { bHoverLocked = bLocked; }
	bool IsHoverLocked() const { return bHoverLocked; }

protected:
	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled
	) const override;

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
	{
		return FVector2D(400, 300);
	}

	// Handle mouse wheel for zooming (and keep the point under cursor stable)
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	// Ensure widget can receive focus (helps with input) and respond to mouse down
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual void OnMouseCaptureLost();
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

public:
	TArray<FSampledFrameData> SampledFrameData;
	TSet<EPerfCurve> VisibleCurves;

	// Cumulative time (ms) at each sample index. SampleTimes[0] == 0.
	TArray<double> SampleTimes;

    // Hover state: index under cursor, local position and whether to show tooltip
    int32 HoveredIndex = INDEX_NONE;
    FVector2D HoverLocal = FVector2D::ZeroVector;
    bool bHasHover = false;

private:
    // Panning state for middle-mouse drag
    bool bIsPanning = false;
    FVector2D PanStartLocal = FVector2D::ZeroVector;
    int32 PanStartViewStart = 0;
    FKey PanMouseButton;

    // Hover lock (set via Ctrl+Click). When true, the hover UI can remain visible even when mouse leaves.
    bool bHoverLocked = false;

	// Drag-selection state (Left mouse drag). Selection indices are inclusive.
	bool bIsSelecting = false;
	FVector2D SelectStartLocal = FVector2D::ZeroVector;
	int32 SelectionStartIndex = INDEX_NONE;
	int32 SelectionEndIndex = INDEX_NONE;
};



