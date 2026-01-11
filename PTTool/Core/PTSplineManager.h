// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PTSplinePathActor.h"
#include "GameFramework/Actor.h"
#include "PTSplineManager.generated.h"

USTRUCT()
struct FSplineData
{
	GENERATED_BODY()

	UPROPERTY()
	APTSplinePathActor* SplineActor = nullptr;

	UPROPERTY(EditAnywhere, Category = "Test Parameter")
	float SplineVelocity = 100.f;

	UPROPERTY(EditAnywhere, Category = "Test Parameter")
	int LoopCount = 1;

	UPROPERTY(EditAnywhere, Category = "Test Parameter")
	float LocalStartDelay = 0.f;

	UPROPERTY(EditAnywhere, Category = "Test Parameter")
	float TestDuration = -1.f;

	UPROPERTY()
	int SplineTestOrder = 0;

	UPROPERTY()
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Category = "Automation")
	FString PreTestCommand;

	UPROPERTY(EditAnywhere, Category = "Automation")
	FString PostTestCommand;
	
	UPROPERTY(EditAnywhere, Category = "Interesting Feature")
	float TimeScaling = 1.f;

	UPROPERTY(EditAnywhere, Category = "Sampling Parameter")
	bool bRecordPerformance = true;

	UPROPERTY(EditAnywhere, Category = "Sampling Parameter")
	float SamplingInterval = 0.1f;

	UPROPERTY(EditAnywhere, Category = "Saving Parameter")
	bool bUseIndividualFile = false;

	UPROPERTY(EditAnywhere, Category = "Saving Parameter")
	FString SavePath = "";


	//......
};

UCLASS()
class PTTOOL_API UPTSplineManager : public UObject
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	UPTSplineManager();
	~UPTSplineManager()
	{
		UE_LOG(LogTemp, Warning, TEXT("SplineManager 被销毁"));
	}
	UPROPERTY()
	TArray<APTSplinePathActor*> Splines;
};
