// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PTToolSettingsObject.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FPTToolPlacementSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Placement")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Placement")
	FRotator Rotation = FRotator::ZeroRotator;
};
UCLASS()
class PTTOOL_API UPTToolSettingsObject : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Placement")
	FName SplineActorName;
	
	UPROPERTY(EditAnywhere, Category = "Placement")
	int TestOrder = 0;
	
	UPROPERTY(EditAnywhere, Category = "Placement")
	float SplineVelocity = 100.f;

	UPROPERTY(EditAnywhere, Category = "Placement")
	float CameraFOV = 90.f;

	UPROPERTY(EditAnywhere, Category = "Placement")
	int InitSplinePoints = 4;
	
	UPROPERTY(EditAnywhere, Category = "Placement")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Placement")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, Category = "Placement")
	FVector Scale = FVector(1.0f, 1.0f, 1.0f);
};
