// PTToolSplineComponent.cpp

#include "PTToolSplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Editor.h"

UPTToolSplineComponent::UPTToolSplineComponent()
{
	// 运行时不要占用性能
	PrimaryComponentTick.bCanEverTick = false;
}

void UPTToolSplineComponent::UpdateEditorMeshes()
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (!World || World->WorldType != EWorldType::Editor)
		return;

	// 清除旧组件
	for (UStaticMeshComponent* Mesh : EditorMeshes)
	{
		if (Mesh)
		{
			Mesh->DestroyComponent();
		}
	}
	EditorMeshes.Empty();

	// 创建新的 mesh 可视化
	const int32 NumPoints = GetNumberOfSplinePoints();
	for (int32 i = 0; i < NumPoints; ++i)
	{
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(GetOwner());
		Mesh->RegisterComponentWithWorld(World);
		UStaticMesh* CameraMesh = LoadObject<UStaticMesh>(
			nullptr,
			TEXT("/Engine/EditorMeshes/MatineeCam_SM.MatineeCam_SM")
		);
		Mesh->SetStaticMesh(CameraMesh);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetVisibility(true);
		Mesh->SetHiddenInGame(true); // 仅编辑器可见
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// 放置位置和旋转
		const FVector Location = GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
		const FRotator Rotation = GetRotationAtSplinePoint(i, ESplineCoordinateSpace::World);
		Mesh->SetWorldLocation(Location);
		Mesh->SetWorldRotation(Rotation);

		// 附加到 spline 的 owner
		Mesh->AttachToComponent(this, FAttachmentTransformRules::KeepWorldTransform);

		EditorMeshes.Add(Mesh);
	}
#endif
}

void UPTToolSplineComponent::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateEditorMeshes();
}


void UPTToolSplineComponent::OnRegister()
{
	Super::OnRegister();

	// 仅编辑器中创建辅助网格体
	UpdateEditorMeshes();
}

void UPTToolSplineComponent::OnUnregister()
{
#if WITH_EDITOR
	for (UStaticMeshComponent* Mesh : EditorMeshes)
	{
		if (Mesh)
		{
			Mesh->DestroyComponent();
		}
	}
	EditorMeshes.Empty();
#endif

	Super::OnUnregister();
}
