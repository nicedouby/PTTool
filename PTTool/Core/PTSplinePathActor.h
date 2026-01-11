// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PTToolSplineComponent.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraComponent.h"
#include "PTSplinePathActor.generated.h"

UCLASS()
class PTTOOL_API APTSplinePathActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	APTSplinePathActor();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	USceneComponent* Root;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UPTToolSplineComponent* SplineComponent;

	UPROPERTY(VisibleAnywhere, Category = "PTTool|Camera")
	UCameraComponent* PreviewCamera;
	
	float DistanceAlongSpline = 0.f;

	UPROPERTY(EditAnywhere, Category = "Test Parameter")
	float SplineVelocity = 100.f;

	UPROPERTY(EditAnywhere, Category = "Test Parameter")
	float CameraFOV = 90.f;

	UPROPERTY(EditAnywhere, Category = "Test Parameter")
	int LoopCount = 1;

	UPROPERTY(EditAnywhere, Category = "Test Parameter")
	float LocalStartDelay = 0.f;

	UPROPERTY(EditAnywhere, Category = "Test Parameter")
	float TestDuration = -1.f;

	UPROPERTY(EditAnywhere)
	int SplineTestOrder = 0;

	UPROPERTY(EditAnywhere)
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Category = "Automation")
	FString PreTestCommand;

	UPROPERTY(EditAnywhere, Category = "Automation")
	FString PostTestCommand;
	
	UPROPERTY(EditAnywhere, Category = "Interesting Feature")
	float TimeScaling = 1.f;

	UPROPERTY(EditAnywhere, Category = "Sampling Parameter")
	bool bRecordPerformance = true;
	
	
	virtual void TickSpline(float DeltaTime);
	void UpdateCameraAlongSpline(float InDistanceAlongSpline);
	
	void AddSplinePoint(const FVector& WorldLocation);
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	
#endif

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
                       