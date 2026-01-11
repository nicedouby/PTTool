// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTToolSimpleTool.h"
#include "InteractiveToolManager.h"
#include "ToolBuilderUtil.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "Misc/MessageDialog.h"
#include "PTTool/Core/PTSplinePathActor.h"

// localization namespace
#define LOCTEXT_NAMESPACE "PTToolSimpleTool"

/*
 * ToolBuilder implementation
 */

UInteractiveTool* UPTToolSimpleToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UPTToolSimpleTool* NewTool = NewObject<UPTToolSimpleTool>(SceneState.ToolManager);
	NewTool->SetWorld(SceneState.World);
	return NewTool;
}



/*
 * ToolProperties implementation
 */

UPTToolSimpleToolProperties::UPTToolSimpleToolProperties()
{
	ShowExtendedInfo = true;
}


/*
 * Tool implementation
 */

UPTToolSimpleTool::UPTToolSimpleTool()
{
}


void UPTToolSimpleTool::SetWorld(UWorld* World)
{
	this->TargetWorld = World;
}


void UPTToolSimpleTool::Setup()
{
	USingleClickTool::Setup();

	Properties = NewObject<UPTToolSimpleToolProperties>(this);
	AddToolPropertySource(Properties);

}


void UPTToolSimpleTool::OnClicked(const FInputDeviceRay& ClickPos)
{
	FVector RayStart = ClickPos.WorldRay.Origin;
	FVector RayEnd = ClickPos.WorldRay.PointAt(99999999.f);
	FCollisionObjectQueryParams QueryParams(FCollisionObjectQueryParams::AllObjects);

	FHitResult Result;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

	if (World && World->LineTraceSingleByObjectType(Result, RayStart, RayEnd, QueryParams))
	{
		FVector HitLocation = Result.ImpactPoint;

		if (!SplineActor.IsValid())
		{
			SplineActor = World->SpawnActor<APTSplinePathActor>(HitLocation, FRotator::ZeroRotator);
		}

		if (SplineActor.IsValid())
		{
			SplineActor->AddSplinePoint(HitLocation);
		}
	}
}


#undef LOCTEXT_NAMESPACE