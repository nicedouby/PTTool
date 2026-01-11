                                                                   // Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "PTToolSplineComponent.generated.h"



UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PTTOOL_API UPTToolSplineComponent : public USplineComponent
{
	GENERATED_BODY()

public:
	UPTToolSplineComponent();

	virtual USplineMetadata* GetSplinePointsMetadata() override { return nullptr; } 


#if WITH_EDITOR
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	void UpdateEditorMeshes();
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	UPROPERTY()
	TArray<UStaticMeshComponent*> EditorMeshes;

	
};