#pragma once
#include "CoreMinimal.h"
#include "PTDataType.generated.h"

#define PT_PERFORMANCE_GRAPH_WINDOW_SIZE_X 1280
#define PT_PERFORMANCE_GRAPH_WINDOW_SIZE_Y 720

USTRUCT()
struct FThreadSample
{
	GENERATED_BODY()

	UPROPERTY()
	FString ThreadName;

	UPROPERTY()
	float TimeMs = 0.f;
};

USTRUCT()
struct FSampledFrameData
{
	GENERATED_BODY()
	
	float FrameMS;
	float GameMS;
	float DrawMS;
	float RHITMS;
	float GPUMS;

	// Per-thread breakdown (optional, may be empty if sampler doesn't provide detailed thread timings)
	UPROPERTY()
	TArray<FThreadSample> ThreadData;

};

// Aggregated per-thread stats (avg/min/max) for a whole capture or a selected range.
USTRUCT()
struct FThreadStatSummary
{
	GENERATED_BODY()

	UPROPERTY()
	FString ThreadName;

	UPROPERTY()
	float AvgMs = 0.f;

	UPROPERTY()
	float MinMs = 0.f;

	UPROPERTY()
	float MaxMs = 0.f;
};

// Container for per-thread summaries. Designed to be reused for "whole capture" and "selected range" stats.
USTRUCT()
struct FFrameThreadStats
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FThreadStatSummary> Threads;
};

USTRUCT()
struct FPTGraphStatInfo
{
	GENERATED_BODY()
	
	float TestTime;

	FSampledFrameData AvgFrameData;

	FSampledFrameData MaxFrameData;

	FSampledFrameData MinFrameData;

	
};
USTRUCT()
struct FSampledGraphData
{
	GENERATED_BODY()
	TArray<FSampledFrameData> FrameData;
	FString SplineName;
	FPTGraphStatInfo StatInfo;
};

//@TODO: 在分析中点出总共存在多少次样本远高于平均值的峰值，并列出这些峰值所在的帧数和对应的时间戳。同时应当设定一个阈值，只有超过该阈值的峰值才会被记录和显示。并列出这些远高于均值的样本占当前线程的总时间百分比，以便用户了解这些异常峰值对整体性能的影响程度。
