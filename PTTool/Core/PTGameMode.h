// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PTCameraPawn.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "PTSplinePathActor.h"
#include "PTGameMode.generated.h"


DECLARE_MULTICAST_DELEGATE(FOnProcessTestNodeStarted)
DECLARE_MULTICAST_DELEGATE(FOnProcessTestNodeCompleted)
DECLARE_MULTICAST_DELEGATE(FOnTestNodeStartTest)
/**
 * 
 */
UCLASS(Blueprintable)
class PTTOOL_API APTGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	
	APTGameMode();

	static float GetGlobalTestDelay()
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("pttool.StartDelay"));
		//return CVar ? CVar->GetFloat() : 0.0f;
		return 0.5f;
	}

	virtual void Tick(float DeltaTimeSeconds) override;
	virtual void BeginPlay() override;

	bool bShouldTick = false;
	bool bShouldSample = false;
	
	UPROPERTY()
	TArray<APTSplinePathActor*> SplineActors;

	UPROPERTY()
	APTCameraPawn* UniqueCameraPawn;

	UPROPERTY()
	TArray<UPTPerformanceSampler*> PerformanceSampler;

	int TestID = 0;

	// Event triggered when processing a test node
	FOnProcessTestNodeStarted OnProcessTestNodeDelegate;
	FOnProcessTestNodeCompleted OnProcessTestNodeCompleteDelegate;
	FOnTestNodeStartTest OnTestNodeStartTestDelegate;	
	int Order = 0;
	
	void GetSplines();
	
	void GetCameraPawn();
	
	void StartTest();

	void ExecuteTest();
	UFUNCTION()
	void OnCompleteTestNode();
	UFUNCTION()
	void OnTestNodeStartTest();

	void OnCompleteTest();
};
