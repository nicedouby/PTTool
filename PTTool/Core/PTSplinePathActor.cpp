// Fill out your copyright notice in the Description page of Project Settings.


#include "PTSplinePathActor.h"

#include "MovieSceneTracksComponentTypes.h"
#include "PTGameMode.h"
#include "Kismet/GameplayStatics.h"


// Sets default values
APTSplinePathActor::APTSplinePathActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(Root);

	SplineComponent = CreateDefaultSubobject<UPTToolSplineComponent>(TEXT("SplineComponent"));
	SplineComponent->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);


	PreviewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PreviewCamera"));

	PreviewCamera->SetupAttachment(SplineComponent); // 你已有的 spline
	PreviewCamera->bUsePawnControlRotation = false;
	PreviewCamera->SetRelativeLocation(FVector::ZeroVector); // 之后我们会动态更新它


}


// Called when the game starts or when spawned
void APTSplinePathActor::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(false);         
}

void APTSplinePathActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (SplineComponent)
	{
		SplineComponent->UpdateEditorMeshes();
	}
}

// Called every frame
void APTSplinePathActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	
}

void APTSplinePathActor::TickSpline(float DeltaTime)
{
	DistanceAlongSpline = DistanceAlongSpline + SplineVelocity * DeltaTime;
	UpdateCameraAlongSpline(DistanceAlongSpline);

	if (DistanceAlongSpline > SplineComponent->GetSplineLength())
	{
		UE_LOG(LogTemp, Display, TEXT("Spline 走完了"));
		LoopCount--;
		if (LoopCount>0)
		{
			DistanceAlongSpline = 0.f;
			UE_LOG(LogTemp, Display, TEXT("Spline 重新开始，剩余循环次数：%d"), LoopCount);
			return;
		}
		
		SetActorTickEnabled(false);
		
		if (APTGameMode* GM = Cast<APTGameMode>(UGameplayStatics::GetGameMode(this)))
		{
			GM->OnProcessTestNodeCompleteDelegate.Broadcast();
		}
	}
}

void APTSplinePathActor::UpdateCameraAlongSpline(float InDistanceAlongSpline)
{
	if (!SplineComponent || !PreviewCamera)
		return;

	// 保证距离在有效范围内
	const float MaxDistance = SplineComponent->GetSplineLength();
	InDistanceAlongSpline = FMath::Clamp(InDistanceAlongSpline, 0.f, MaxDistance);

	// 获取位置和旋转
	const FVector NewLocation = SplineComponent->GetLocationAtDistanceAlongSpline(InDistanceAlongSpline, ESplineCoordinateSpace::World);
	const FRotator NewRotation = SplineComponent->GetRotationAtDistanceAlongSpline(InDistanceAlongSpline, ESplineCoordinateSpace::World);

	PreviewCamera->SetWorldLocation(NewLocation);
	PreviewCamera->SetWorldRotation(NewRotation);
}

void APTSplinePathActor::AddSplinePoint(const FVector& WorldLocation)
{
	SplineComponent->AddSplinePoint(WorldLocation, ESplineCoordinateSpace::World);
	SplineComponent->UpdateSpline();
}
