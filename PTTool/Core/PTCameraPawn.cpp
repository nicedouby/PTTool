// Fill out your copyright notice in the Description page of Project Settings.


#include "PTCameraPawn.h"

#include "PTGameMode.h"
#include "PTSplinePathActor.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
APTCameraPawn::APTCameraPawn()
{
	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// 创建 Spring Arm
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 0.f;
	SpringArm->bUsePawnControlRotation = false;

	// 创建 Camera
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
	Camera->bUsePawnControlRotation = false;

	Camera->bCameraMeshHiddenInGame = false;
}

void APTCameraPawn::OnProcessingTestNode()
{
	UE_LOG(LogTemp, Warning, TEXT(" 正在处理测试节点"));

	UE_LOG(LogTemp, Warning, TEXT("%s is Processing %s"), *GetName(), *TargetSplineActor->GetName());
	
	//附加相机到样条线预览相机
	AttachToComponent(TargetSplineActor->PreviewCamera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	//强制初始化位置
	TargetSplineActor->UpdateCameraAlongSpline(0);


	//执行时间缩放命令
	FString TimeScalingCommand = FString::Printf(TEXT("slomo %f"), TargetSplineActor->TimeScaling);
	UE_LOG(LogTemp, Warning, TEXT("执行时间缩放命令: %s"), *TimeScalingCommand);
	
	UGameplayStatics::GetPlayerController(this, 0)->ConsoleCommand(*TimeScalingCommand);
	UGameplayStatics::GetPlayerController(this, 0)->ConsoleCommand(TargetSplineActor->PreTestCommand);
	//设置相机FOV
	
	Camera->FieldOfView = TargetSplineActor->CameraFOV;
}

void APTCameraPawn::OnProcessTestNodeComplete()
{
	UE_LOG(LogTemp, Warning, TEXT("完成测试测试节点"));

	UE_LOG(LogTemp, Warning, TEXT("%s Complete Testing Test Node %s"), *GetName(), *TargetSplineActor->GetName());

	UGameplayStatics::GetPlayerController(this, 0)->ConsoleCommand(TargetSplineActor->PostTestCommand);
	//执行下一个节点
	if (APTGameMode* GM = Cast<APTGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->ExecuteTest();
	}
	
}

void APTCameraPawn::OnTestNodeStartTest()
{
	UE_LOG(LogTemp, Warning, TEXT("开始测试测试节点"));
	if (APTGameMode* GM = Cast<APTGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		
	}
}

// Called when the game starts or when spawned
void APTCameraPawn::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void APTCameraPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void APTCameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

