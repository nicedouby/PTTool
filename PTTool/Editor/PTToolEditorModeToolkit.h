// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/BaseToolkit.h"
#include "PTToolEditorMode.h"
#include "PTToolSettingsObject.h"
#include "PTTool/Core/PTSplineManager.h"
// Slate widgets
#include "Widgets/Views/STableRow.h"

class UPTToolSettingsObject;

class FPTToolEditorModeToolkit;

struct FSplineActorRow
{
	TWeakObjectPtr<APTSplinePathActor> Actor;
	FGuid ActorGuid;
	FSoftObjectPath ActorPath;

	FSplineActorRow(APTSplinePathActor* InActor)
		: Actor(InActor)
		, ActorGuid(InActor ? InActor->GetActorGuid() : FGuid())
		, ActorPath(InActor)
	{
	}
	int GetOrder() const
	{
		APTSplinePathActor* SplineActor = Cast<APTSplinePathActor>(Actor);
		if (SplineActor)
		{
			return SplineActor->SplineTestOrder;
		}
		else
		{
			return -1;
		}
	}
	FString GetName() const
	{
		return Actor.IsValid() ? Actor->GetName() : TEXT("Invalid");
	}

	FVector GetLocation() const
	{
		return Actor.IsValid() ? Actor->GetActorLocation() : FVector::ZeroVector;
	}
	TWeakObjectPtr<APTSplinePathActor> GetActor() const { return Actor; }
};

using FSplineActorRowPtr = TSharedPtr<FSplineActorRow>;

/** Row widget for Manage list: shows basic columns plus Up/Down/Delete action buttons. */
class FSplineActorTableRow : public SMultiColumnTableRow<FSplineActorRowPtr>
{
public:
	SLATE_BEGIN_ARGS(FSplineActorTableRow) {}
		SLATE_ARGUMENT(FSplineActorRowPtr, Item)
		SLATE_ARGUMENT(FPTToolEditorModeToolkit*, Toolkit)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	FSplineActorRowPtr Item;
	FPTToolEditorModeToolkit* Toolkit = nullptr;
};

enum class EPTToolTab : uint8
{
	Generate,
	Manage,
	Test
};

struct FSplineClassItem
{
	FString DisplayName;
	UClass* Class;

	FSplineClassItem(const FString& InName, UClass* InClass)
		: DisplayName(InName), Class(InClass)
	{
	}

	FString ToString() const { return DisplayName; }
};

class FPTToolEditorModeToolkit : public FModeToolkit
{
public:
	FPTToolEditorModeToolkit();
	virtual ~FPTToolEditorModeToolkit() override = default;

	/** FModeToolkit interface */
	virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode) override;
	virtual void GetToolPaletteNames(TArray<FName>& PaletteNames) const override;

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual TSharedPtr<SWidget> GetInlineContent() const override;

	virtual void RefreshSplineActorList();

	virtual void SetSplineClassItems();
	virtual void ShutdownUI() override;

	/** Build and return the toolkit widget for standalone/reuse scenarios */
	TSharedRef<SWidget> BuildStandaloneWidget();

	void OnActorDeleted(AActor* DeletedActor);
	void OnActorPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent);

	// Manage list actions
	void DeleteSplineActorRow(const FSplineActorRowPtr& Row);
	void MoveSplineActorRowUp(const FSplineActorRowPtr& Row);
	void MoveSplineActorRowDown(const FSplineActorRowPtr& Row);
	void SwapSplineActorOrders(APTSplinePathActor* A, APTSplinePathActor* B);

public:
	EPTToolTab SelectedTab;

	TArray<TSharedPtr<FSplineClassItem>> SplineClassItems;

	TSharedPtr<FSplineClassItem> SelectedSplineItem;
	
	TSharedPtr<STextComboBox> SplineClassComboBox;
	TSharedPtr<IDetailsView> SettingsView;
	TSharedPtr<IDetailsView> ManageActorDetailsView;

	//Rows
	TArray<FSplineActorRowPtr> SplineActorRows;
	TSharedPtr<SListView<FSplineActorRowPtr>> SplineListView_Manage;
	TSharedPtr<SListView<FSplineActorRowPtr>> SplineListView_Generate;
	
	FSplineActorRowPtr SelectedRow;

	// 用于在列表刷新/重建后恢复选中状态
	FGuid SelectedActorGuid;
	FSoftObjectPath SelectedActorPath;

	UPTToolSettingsObject* SettingsObject = nullptr;

	UPTSplineManager* InitializePTSplineManager();

private:
	// Transient manager object used by the editor mode/toolkit.
	UPROPERTY()
	TObjectPtr<UPTSplineManager> SplineManager = nullptr;
}; 
