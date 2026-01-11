#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "PTDataType.h"

class SFrameHoverWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFrameHoverWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// Update the widget to show a particular sampled frame (pointer may be null to clear)
	void SetFrameData(const FSampledFrameData* InData, int32 InIndex = INDEX_NONE);

	// Optional: filter which thread rows are shown. When nullptr or empty, shows all.
	void SetVisibleThreadFilter(const TSharedPtr<const TSet<FString>>& InVisibleThreads)
	{
		VisibleThreads = InVisibleThreads;
		RebuildContents();
	}

	struct FRangeStats
	{
		bool bHasRange = false;
		int32 StartIndex = INDEX_NONE;
		int32 EndIndex = INDEX_NONE;
		int32 NumFrames = 0;

		float AvgFrameMs = 0.f;
		float MinFrameMs = 0.f;
		float MaxFrameMs = 0.f;
		float AvgFPS = 0.f;
	};

	void SetRangeStats(const FRangeStats& InStats)
	{
		RangeStats = InStats;
		RebuildContents();
	}

	void ClearRangeStats()
	{
		RangeStats = FRangeStats{};
		RebuildContents();
	}

private:
	const FSampledFrameData* CurrentData = nullptr;
	int32 CurrentIndex = INDEX_NONE;

	TSharedPtr<class SScrollBox> ScrollBox;
	TSharedPtr<const TSet<FString>> VisibleThreads;

	FRangeStats RangeStats;

	void RebuildContents();
};
