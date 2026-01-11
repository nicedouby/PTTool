// Fill out your copyright notice in the Description page of Project Settings.


#include "PTGameMode.h"
#include "PTCameraPawn.h"
#include "EngineUtils.h"
#include "PerformanceWindow.h"
APTGameMode::APTGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = APTCameraPawn::StaticClass();
}

void APTGameMode::Tick(float DeltaTimeSeconds)
{
	Super::Tick(DeltaTimeSeconds);
	if (UniqueCameraPawn&&bShouldTick)
	{
		//@todo: tick Target Actor
		UniqueCameraPawn->TargetSplineActor->TickSpline(DeltaTimeSeconds);
		if (bShouldSample)
		{
			PerformanceSampler[TestID]->SampleFrame(DeltaTimeSeconds);

			
		}
	}
	

}

void APTGameMode::BeginPlay()
{
	Super::BeginPlay();

	GetSplines();

	GetCameraPawn();

	OnProcessTestNodeDelegate.AddUObject(UniqueCameraPawn, &APTCameraPawn::OnProcessingTestNode);

	OnProcessTestNodeCompleteDelegate.AddUObject(UniqueCameraPawn, &APTCameraPawn::OnProcessTestNodeComplete);
	OnProcessTestNodeCompleteDelegate.AddUObject(this, &APTGameMode::OnCompleteTestNode);

	OnTestNodeStartTestDelegate.AddUObject(UniqueCameraPawn, &APTCameraPawn::OnTestNodeStartTest);
	OnTestNodeStartTestDelegate.AddUObject(this, &APTGameMode::OnTestNodeStartTest);
	
	FTimerHandle TimerHandle;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, this,&APTGameMode::StartTest, 1, false, GetGlobalTestDelay());


	UE_LOG(LogTemp, Warning, TEXT("测试即将开始"));
}

void APTGameMode::GetSplines()
{
	for (TActorIterator<APTSplinePathActor> It(GetWorld()); It; ++It)
	{
		APTSplinePathActor* Actor = *It;
		if (Actor && Actor->Tags.Contains("PTTool_Generated"))
		{
			SplineActors.Add(Actor);
			PerformanceSampler.Add(NewObject<UPTPerformanceSampler>());
		}
	}

	// 按照 SplineTestOrder 排序
	SplineActors.Sort([](const APTSplinePathActor& A, const APTSplinePathActor& B)
	{
		return A.SplineTestOrder < B.SplineTestOrder;
	});
	
	// 验证排序结果
	for (APTSplinePathActor* Actor : SplineActors)
	{
		UE_LOG(LogTemp, Log, TEXT("Sorted Actor: %s, Order: %d"), *Actor->GetName(), Actor->SplineTestOrder);
	}
}

void APTGameMode::GetCameraPawn()
{
	for (TActorIterator<APTCameraPawn> It(GetWorld()); It; ++It)
	{
		UniqueCameraPawn = *It;
		if (UniqueCameraPawn)
		{
			UE_LOG(LogTemp, Log, TEXT("Found Camera Pawn: %s"), *UniqueCameraPawn->GetName());
			break;
		}
	}
	if (!UniqueCameraPawn)
	{
		UE_LOG(LogTemp, Fatal, TEXT("No Camera Pawn"));
		return;
		//@TODO 创建一个新的 CameraPawn
	}
}

void APTGameMode::StartTest()
{
	if (!UniqueCameraPawn)
	{
		UE_LOG(LogTemp, Fatal, TEXT("UniqueCameraPawn is null! Cannot proceed with the test."));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("测试开始"));
	ExecuteTest();
}

void APTGameMode::ExecuteTest()
{
	if (TestID < SplineActors.Num())
	{
		UniqueCameraPawn->TargetSplineActor = SplineActors[TestID];


		if (UniqueCameraPawn->TargetSplineActor->LocalStartDelay > 0.0f)
		{
			FTimerHandle TimerHandle;
			
			//GetWorldTimerManager().SetTimer(TimerHandle, UniqueCameraPawn, &APTCameraPawn::OnTestNodeStartTest, UniqueCameraPawn->TargetSplineActor->LocalStartDelay, false);
			GetWorld()->GetTimerManager().SetTimer(
				TimerHandle,
				[this]()
				{
					OnTestNodeStartTestDelegate.Broadcast();
				},
				1.f,
				false,
				UniqueCameraPawn->TargetSplineActor->LocalStartDelay
			);
			OnProcessTestNodeDelegate.Broadcast();
		}
		else
		{
			OnProcessTestNodeDelegate.Broadcast();
			OnTestNodeStartTestDelegate.Broadcast();
		}
		
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No Spline Actors"));
		OnCompleteTest();
	}
}

void APTGameMode::OnCompleteTestNode()
{
	bShouldTick = false;
	bShouldSample = false;
	PerformanceSampler[TestID]->OnCompleteSampling();
	TestID++;
}

void APTGameMode::OnTestNodeStartTest()
{
	bShouldTick = true;
	bShouldSample = true;
	PerformanceSampler[TestID]->OnStartSampling();
}

void APTGameMode::OnCompleteTest()
{
	TArray<FSampledGraphData> SampledGraphData;
	SampledGraphData.Reserve(PerformanceSampler.Num());

	for (int32 i = 0; i < PerformanceSampler.Num(); ++i)
	{
		UPTPerformanceSampler* Sampler = PerformanceSampler[i];
		if (!IsValid(Sampler))
		{
			continue;
		}

		APTSplinePathActor* Spline = nullptr;
		if (SplineActors.IsValidIndex(i))
		{
			Spline = SplineActors[i];
		}

		FSampledGraphData GraphData;

		if (Spline)
		{
			GraphData.SplineName = Spline->GetName();
		}
		else
		{
			GraphData.SplineName = TEXT("UnknownSpline");
		}

		// 值拷贝，Stop Playing 后安全
		GraphData.FrameData = Sampler->FrameData;

		GraphData.StatInfo.AvgFrameData = Sampler->AvgFrameData;

		GraphData.StatInfo.MaxFrameData = Sampler->MaxFrameData;

		GraphData.StatInfo.MinFrameData = Sampler->MinFrameData;


		GraphData.StatInfo.TestTime = Sampler->TimeDuration;
		
		SampledGraphData.Add(MoveTemp(GraphData));
	}


	OpenPerformanceAnalyzerWindow(SampledGraphData);
}
