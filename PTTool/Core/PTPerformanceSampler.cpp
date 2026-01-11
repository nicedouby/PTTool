// Fill out your copyright notice in the Description page of Project Settings.


#include "PTPerformanceSampler.h"
#include "RHI.h"
#include "Stats/Stats.h"
#include "GPUProfiler.h"
#include "HAL/PlatformTime.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "RenderTimer.h"
void ComputeChunkedAverage(
	const TArray<FSampledFrameData>& FrameData,
	TArray<FSampledFrameData>& OutAvg,
	int32 ChunkSize)
{
	check(ChunkSize > 0);

	const int32 NumFrames = FrameData.Num();
	const int32 NumChunks = (NumFrames + ChunkSize - 1) / ChunkSize;

	OutAvg.Reset();
	OutAvg.Reserve(NumChunks);

	for (int32 ChunkIndex = 0; ChunkIndex < NumFrames; ChunkIndex += ChunkSize)
	{
		const int32 Start = ChunkIndex;
		const int32 End = FMath::Min(Start + ChunkSize, NumFrames);

		double Frame = 0.0;
		double Game = 0.0;
		double Draw = 0.0;
		double RHIT = 0.0;
		double GPU = 0.0;

		const int32 Count = End - Start;

		for (int32 i = Start; i < End; ++i)
		{
			const FSampledFrameData& S = FrameData[i];
			Frame += S.FrameMS;
			Game += S.GameMS;
			Draw += S.DrawMS;
			RHIT += S.RHITMS;
			GPU += S.GPUMS;
		}

		OutAvg.Add({
			(float)(Frame / Count),
			(float)(Game / Count),
			(float)(Draw / Count),
			(float)(RHIT / Count),
			(float)(GPU / Count)
		});
	}
}

void UPTPerformanceSampler::OnStartSampling()
{
}

void UPTPerformanceSampler::OnCompleteSampling()
{
	AvgFrameData.GameMS = 0.0f;
	AvgFrameData.DrawMS = 0.0f;
	AvgFrameData.RHITMS = 0.0f;
	AvgFrameData.GPUMS = 0.0f;
	AvgFrameData.FrameMS = 0.0f;
	const int32 MinStartIndex = FMath::Clamp(FrameData.Num() / 10, 0, FrameData.Num() - 1);
	MinFrameData = FrameData[MinStartIndex];
	UE_LOG(LogTemp, Warning, TEXT(""));
	UE_LOG(LogTemp, Warning, TEXT("   %d "), FrameData.Num());
	if (FrameData.Num() <36000)
	{
		int slot = 0;
		for (const FSampledFrameData& Sample : FrameData)
		{
			
			AvgFrameData.FrameMS += Sample.FrameMS;
			AvgFrameData.GameMS += Sample.GameMS;
			AvgFrameData.DrawMS += Sample.DrawMS;
			AvgFrameData.RHITMS += Sample.RHITMS;
			AvgFrameData.GPUMS += Sample.GPUMS;

			// Max
			MaxFrameData.FrameMS = FMath::Max(MaxFrameData.FrameMS, Sample.FrameMS);
			MaxFrameData.GameMS  = FMath::Max(MaxFrameData.GameMS,  Sample.GameMS);
			MaxFrameData.DrawMS  = FMath::Max(MaxFrameData.DrawMS,  Sample.DrawMS);
			MaxFrameData.RHITMS  = FMath::Max(MaxFrameData.RHITMS,  Sample.RHITMS);
			MaxFrameData.GPUMS   = FMath::Max(MaxFrameData.GPUMS,   Sample.GPUMS);

			if (slot >= MinStartIndex && Sample.FrameMS > 0.0f)
			{
				MinFrameData.FrameMS = FMath::Min(MinFrameData.FrameMS, Sample.FrameMS);
				MinFrameData.GameMS  = FMath::Min(MinFrameData.GameMS,  Sample.GameMS);
				MinFrameData.DrawMS  = FMath::Min(MinFrameData.DrawMS,  Sample.DrawMS);
				MinFrameData.RHITMS  = FMath::Min(MinFrameData.RHITMS,  Sample.RHITMS);
				MinFrameData.GPUMS   = FMath::Min(MinFrameData.GPUMS,   Sample.GPUMS);
			}
			slot++;
			
		}
		AvgFrameData.FrameMS = AvgFrameData.FrameMS / FrameData.Num();
		AvgFrameData.GameMS = AvgFrameData.GameMS / FrameData.Num();
		AvgFrameData.DrawMS = AvgFrameData.DrawMS / FrameData.Num();
		AvgFrameData.RHITMS = AvgFrameData.RHITMS / FrameData.Num();
		AvgFrameData.GPUMS = AvgFrameData.GPUMS / FrameData.Num();
	}
	else
	{
		int32 ChunkSize = 0;
		TArray<FSampledFrameData> FrameAvg;
		if (FrameData.Num() < 3600000)
		{
			ChunkSize = 10000;
		}
		else if (FrameData.Num() < 360000000)
		{
			ChunkSize = 100'000;
		}
		ComputeChunkedAverage(FrameData, FrameAvg, ChunkSize);
		const int32 Count = FrameAvg.Num();
		double Frame = 0.0;
		double Game = 0.0;
		double Draw = 0.0;
		double RHIT = 0.0;
		double GPU = 0.0;
		for (const FSampledFrameData& S : FrameAvg)
		{
			Frame += S.FrameMS;
			Game += S.GameMS;
			Draw += S.DrawMS;
			RHIT += S.RHITMS;
			GPU += S.GPUMS;
		}

		if (Count > 0)
		{
			AvgFrameData.FrameMS = (float)(Frame / Count);
			AvgFrameData.GameMS = (float)(Game / Count);
			AvgFrameData.DrawMS = (float)(Draw / Count);
			AvgFrameData.RHITMS = (float)(RHIT / Count);
			AvgFrameData.GPUMS = (float)(GPU / Count);
		}
	}
	UE_LOG(LogTemp, Log,
	       TEXT(" Frame %.2f | Game %.2f | Draw %.2f | RHI %.2f | GPU %.2f"),
	       AvgFrameData.FrameMS,
	       AvgFrameData.GameMS,
	       AvgFrameData.DrawMS,
	       AvgFrameData.RHITMS,
	       AvgFrameData.GPUMS)
                              
	UE_LOG(LogTemp, Log,
	   TEXT(" MAX Frame %.2f | Game %.2f | Draw %.2f | RHI %.2f | GPU %.2f"),
	   MaxFrameData.FrameMS,
	   MaxFrameData.GameMS,
	   MaxFrameData.DrawMS,
	   MaxFrameData.RHITMS,
	   MaxFrameData.GPUMS)

	                              
	UE_LOG(LogTemp, Log,
	   TEXT(" MIN  Frame %.2f | Game %.2f | Draw %.2f | RHI %.2f | GPU %.2f"),
	   MinFrameData.FrameMS,
	   MinFrameData.GameMS,
	   MinFrameData.DrawMS,
	   MinFrameData.RHITMS,
	   MinFrameData.GPUMS)
	
	// ----- 新增：遍历 FrameData，统计各线程远高于平均值的事件次数和占比 -----
	{
		// Assumptions: "远高于平均值" defined as > ThresholdMultiplier * Average.
		// If average is nearly zero, we treat values > AbsoluteThresholdMs as an event.
		const double ThresholdMultiplier = 1.5; // 1.5x average
		const double AbsoluteThresholdMs = 1.0; // when average ~ 0, treat >1ms as high

		int32 TotalFrames = FrameData.Num();
		double TotalTimeMs = 0.0;
		for (const FSampledFrameData& S : FrameData)
		{
			TotalTimeMs += S.FrameMS;
		}
		if (TotalFrames > 0 && TotalTimeMs > 0.0)
		{
			int32 CountGame = 0;
			int32 CountDraw = 0;
			int32 CountRHI = 0;
			int32 CountGPU = 0;

			double TimeGameMs = 0.0;
			double TimeDrawMs = 0.0;
			double TimeRHIMs = 0.0;
			double TimeGPUMs = 0.0;

			const double AvgGame = FMath::Max<double>(AvgFrameData.GameMS, 0.0);
			const double AvgDraw = FMath::Max<double>(AvgFrameData.DrawMS, 0.0);
			const double AvgRHI = FMath::Max<double>(AvgFrameData.RHITMS, 0.0);
			const double AvgGPU = FMath::Max<double>(AvgFrameData.GPUMS, 0.0);

			for (const FSampledFrameData& S : FrameData)
			{
				bool bGameHigh = false;
				bool bDrawHigh = false;
				bool bRHIHigh = false;
				bool bGPUHigh = false;

				// Game thread
				if (AvgGame > KINDA_SMALL_NUMBER)
				{
					if (S.GameMS > AvgGame * ThresholdMultiplier)
						bGameHigh = true;
				}
				else
				{
					if (S.GameMS > AbsoluteThresholdMs)
						bGameHigh = true;
				}

				// Draw thread
				if (AvgDraw > KINDA_SMALL_NUMBER)
				{
					if (S.DrawMS > AvgDraw * ThresholdMultiplier)
						bDrawHigh = true;
				}
				else
				{
					if (S.DrawMS > AbsoluteThresholdMs)
						bDrawHigh = true;
				}

				// RHI thread
				if (AvgRHI > KINDA_SMALL_NUMBER)
				{
					if (S.RHITMS > AvgRHI * ThresholdMultiplier)
						bRHIHigh = true;
				}
				else
				{
					if (S.RHITMS > AbsoluteThresholdMs)
						bRHIHigh = true;
				}

				// GPU
				if (AvgGPU > KINDA_SMALL_NUMBER)
				{
					if (S.GPUMS > AvgGPU * ThresholdMultiplier)
						bGPUHigh = true;
				}
				else
				{
					if (S.GPUMS > AbsoluteThresholdMs)
						bGPUHigh = true;
				}

				if (bGameHigh)
				{
					CountGame++;
					TimeGameMs += S.FrameMS;
				}
				if (bDrawHigh)
				{
					CountDraw++;
					TimeDrawMs += S.FrameMS;
				}
				if (bRHIHigh)
				{
					CountRHI++;
					TimeRHIMs += S.FrameMS;
				}
				if (bGPUHigh)
				{
					CountGPU++;
					TimeGPUMs += S.FrameMS;
				}
			}

			// Log summary: counts, percent of frames, percent of total time
			UE_LOG(LogTemp, Log, TEXT("--- High-value event summary (threshold %.2fx avg or > %.2f ms if avg~0) ---"), ThresholdMultiplier, AbsoluteThresholdMs);
			UE_LOG(LogTemp, Log, TEXT("Game: Count=%d | Frames%%=%.2f%% | Time%%=%.2f%%"), CountGame, (double)CountGame * 100.0 / (double)TotalFrames, TimeGameMs * 100.0 / TotalTimeMs);
			UE_LOG(LogTemp, Log, TEXT("Draw: Count=%d | Frames%%=%.2f%% | Time%%=%.2f%%"), CountDraw, (double)CountDraw * 100.0 / (double)TotalFrames, TimeDrawMs * 100.0 / TotalTimeMs);
			UE_LOG(LogTemp, Log, TEXT("RHI:  Count=%d | Frames%%=%.2f%% | Time%%=%.2f%%"), CountRHI, (double)CountRHI * 100.0 / (double)TotalFrames, TimeRHIMs * 100.0 / TotalTimeMs);
			UE_LOG(LogTemp, Log, TEXT("GPU:  Count=%d | Frames%%=%.2f%% | Time%%=%.2f%%"), CountGPU, (double)CountGPU * 100.0 / (double)TotalFrames, TimeGPUMs * 100.0 / TotalTimeMs);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("FrameData empty or zero total time; skipping high-value event analysis"));
		}
	}

	// -----  -----
	// Whole capture per-thread Avg/Min/Max stats
	{
		CaptureThreadStats.Threads.Reset();

		// name -> index in CaptureThreadStats.Threads
		TMap<FString, int32> NameToIndex;
		TMap<FString, double> SumByName;
		TMap<FString, int32> CountByName;
		TMap<FString, float> MinByName;
		TMap<FString, float> MaxByName;

		for (const FSampledFrameData& S : FrameData)
		{
			for (const FThreadSample& T : S.ThreadData)
			{
				// accumulate sum/count
				SumByName.FindOrAdd(T.ThreadName) += (double)T.TimeMs;
				CountByName.FindOrAdd(T.ThreadName) += 1;

				// min/max
				float& CurMin = MinByName.FindOrAdd(T.ThreadName);
				float& CurMax = MaxByName.FindOrAdd(T.ThreadName);
				if (CountByName[T.ThreadName] == 1)
				{
					CurMin = T.TimeMs;
					CurMax = T.TimeMs;
				}
				else
				{
					CurMin = FMath::Min(CurMin, T.TimeMs);
					CurMax = FMath::Max(CurMax, T.TimeMs);
				}
			}
		}

		// Build summaries
		CaptureThreadStats.Threads.Reserve(CountByName.Num());
		for (const TPair<FString, int32>& Pair : CountByName)
		{
			const FString& Name = Pair.Key;
			const int32 Count = Pair.Value;
			const double Sum = SumByName.FindRef(Name);

			FThreadStatSummary Summary;
			Summary.ThreadName = Name;
			Summary.AvgMs = Count > 0 ? (float)(Sum / (double)Count) : 0.f;
			Summary.MinMs = MinByName.FindRef(Name);
			Summary.MaxMs = MaxByName.FindRef(Name);
			CaptureThreadStats.Threads.Add(Summary);
		}

		// Sort by Avg descending (bottleneck-first)
		CaptureThreadStats.Threads.Sort([](const FThreadStatSummary& A, const FThreadStatSummary& B)
		{
			return A.AvgMs > B.AvgMs;
		});
	}

}

void UPTPerformanceSampler::SampleFrame(float DeltaTime)
{
	// 累计时间
	TimeDuration += DeltaTime;
	
	double RawGameThreadMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
	double RawRenderThreadMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
	double RawRHIMs = FPlatformTime::ToMilliseconds(GRHIThreadTime);
	double RawGPUMs = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0)); // GPU 0
	double RawFrameMs = (FApp::GetCurrentTime() - FApp::GetLastTime()) * 1000.0;

	// =======================
	// 平滑值（stat unit 同款 EMA）
	// =======================

	// 静态保存上一次的平滑结果（等价于 FStatUnitData 成员）
	static double GameThreadMs = 0.0;
	static double RenderThreadMs = 0.0; // Draw
	static double RHIMs = 0.0;
	static double GPUMs = 0.0;
	static double FrameMs = 0.0;
	// stat unit 使用的权重
	constexpr double Alpha = 0.1;
	constexpr double OneMinusAlpha = 0.9;

	FrameMs =
		OneMinusAlpha * FrameMs +
		Alpha * RawFrameMs;
	// Game
	GameThreadMs = OneMinusAlpha * GameThreadMs + Alpha * RawGameThreadMs;

	// Draw (Render Thread)
	RenderThreadMs = OneMinusAlpha * RenderThreadMs + Alpha * RawRenderThreadMs;

	// RHI
	RHIMs = OneMinusAlpha * RHIMs + Alpha * RawRHIMs;

	// GPU
	GPUMs = OneMinusAlpha * GPUMs + Alpha * RawGPUMs;

	FSampledFrameData SampledFrameData;
	SampledFrameData.GameMS = GameThreadMs;
	SampledFrameData.DrawMS = RenderThreadMs;
	SampledFrameData.RHITMS = RHIMs;
	SampledFrameData.GPUMS = GPUMs;
	SampledFrameData.FrameMS = FrameMs;

	// Fill per-thread breakdown (fallback using available metrics)
	{
		FThreadSample T;
		T.ThreadName = TEXT("Game Thread");
		T.TimeMs = (float)GameThreadMs;
		SampledFrameData.ThreadData.Add(T);

		T.ThreadName = TEXT("Render Thread");
		T.TimeMs = (float)RenderThreadMs;
		SampledFrameData.ThreadData.Add(T);

		T.ThreadName = TEXT("RHI Thread");
		T.TimeMs = (float)RHIMs;
		SampledFrameData.ThreadData.Add(T);

		T.ThreadName = TEXT("GPU");
		T.TimeMs = (float)GPUMs;
		SampledFrameData.ThreadData.Add(T);
	}
	FrameData.Add(SampledFrameData);
	// =======================
	// 现在这些值就是 stat unit 等价结果
	// =======================

	// -------- 使用 --------
	UE_LOG(LogTemp, Log,
	       TEXT("Unit | Frame %.2f | Game %.2f | Draw %.2f | RHI %.2f | GPU %.2f"),
	       FrameMs,
	       GameThreadMs,
	       RenderThreadMs,
	       RHIMs,
	       GPUMs)
}
