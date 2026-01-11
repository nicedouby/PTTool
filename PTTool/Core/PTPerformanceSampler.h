// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PTDataType.h"
#include "PTPerformanceSampler.generated.h"



UCLASS()
class PTTOOL_API UPTPerformanceSampler : public UObject
{
	GENERATED_BODY()

public:
	UPTPerformanceSampler() = default;
	bool bIsSampling = false;

	bool bRecordFPS = true;
	bool bRecordThreadTimes = true;
	bool bRecordMemory = true;

	virtual void OnStartSampling();
	virtual void OnCompleteSampling();
	virtual void SampleFrame(float DeltaTime);

	float GameThreadTimeMs = 0.0f;
	float DrawThreadTimeMs = 0.0f;
	float GPUTimeMs = 0.0f;

	float TimeDuration = 0.00f;
	UPROPERTY(EditAnywhere)
	TArray<FSampledFrameData> FrameData;

	FSampledFrameData AvgFrameData = {};
	FSampledFrameData MaxFrameData = {};
	FSampledFrameData MinFrameData = {};

	// Whole-capture per-thread Avg/Min/Max computed from FrameData.
	UPROPERTY(VisibleAnywhere)
	FFrameThreadStats CaptureThreadStats;
};
