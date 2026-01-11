// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTToolEditorModeToolkit.h"
#include "PTToolEditorMode.h"
#include "Engine/Selection.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "EditorModeManager.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "PTTool/Core/PTSplinePathActor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "PTTool/Core/PTGameMode.h"

// For undo/redo transactions when editing/deleting actors
#include "ScopedTransaction.h"

// Slate
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"

// Editor selection helpers (used by double-click selection)
#include "Editor.h"

#define LOCTEXT_NAMESPACE "PTToolEditorModeToolkit"
void ExecuteTest();
class FAssetRegistryModule;

namespace
{
	// Forward declarations for helper functions in this translation unit.
	static int32 FindNextAvailableSplineOrder(UWorld* World, int32 DesiredOrder);
	static FPropertyEditorModule& SetupPropertyEditorModule(FPTToolEditorModeToolkit* In);

	static int32 FindNextAvailableSplineOrder(UWorld* World, int32 DesiredOrder)
	{
		if (!World)
		{
			return DesiredOrder;
		}

		int32 MaxOrder = -1;
		bool bHasAny = false;
		for (TActorIterator<APTSplinePathActor> It(World); It; ++It)
		{
			APTSplinePathActor* Actor = *It;
			if (!IsValid(Actor))
			{
				continue;
			}
			if (Actor->Tags.Contains(TEXT("PTTool_Generated")))
			{
				bHasAny = true;
				MaxOrder = FMath::Max(MaxOrder, Actor->SplineTestOrder);
			}
		}
		return bHasAny ? (MaxOrder + 1) : 0;
	}

	static FPropertyEditorModule& SetupPropertyEditorModule(FPTToolEditorModeToolkit* In)
	{
		FPropertyEditorModule& Out = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		FDetailsViewArgs Args;
		Args.bHideSelectionTip = true;
		Args.bLockable = false;
		Args.bAllowSearch = false;

		In->SettingsView = Out.CreateDetailView(Args);
		In->ManageActorDetailsView = Out.CreateDetailView(Args);

		// Hide mobility/other noisy stuff, keep our selected vars and transform (location+rotation).
		In->ManageActorDetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& PropertyAndParent)
		{
			const FProperty* Prop = &PropertyAndParent.Property;
			if (!Prop)
			{
				return false;
			}

			const FName Name = Prop->GetFName();

			// Keep transform editing but hide mobility.
			if (Name == TEXT("Mobility"))
			{
				return false;
			}

			// PTTool variables we want.
			return (Name == GET_MEMBER_NAME_CHECKED(APTSplinePathActor, SplineVelocity) ||
				Name == GET_MEMBER_NAME_CHECKED(APTSplinePathActor, CameraFOV) ||
				Name == GET_MEMBER_NAME_CHECKED(APTSplinePathActor, LoopCount) ||
				Name == GET_MEMBER_NAME_CHECKED(APTSplinePathActor, LocalStartDelay) ||
				Name == GET_MEMBER_NAME_CHECKED(APTSplinePathActor, TestDuration) ||
				Name == GET_MEMBER_NAME_CHECKED(APTSplinePathActor, SplineTestOrder) ||
				Name == GET_MEMBER_NAME_CHECKED(APTSplinePathActor, bEnabled) ||
				Name == GET_MEMBER_NAME_CHECKED(APTSplinePathActor, PreTestCommand) ||
				Name == GET_MEMBER_NAME_CHECKED(APTSplinePathActor, PostTestCommand) ||
				Name == GET_MEMBER_NAME_CHECKED(APTSplinePathActor, TimeScaling) ||
				Name == GET_MEMBER_NAME_CHECKED(APTSplinePathActor, bRecordPerformance));
		}));

		In->ManageActorDetailsView->SetObject(nullptr);

		In->SettingsObject = NewObject<UPTToolSettingsObject>();
		In->SettingsObject->AddToRoot();
		In->SettingsView->SetObject(In->SettingsObject);

		return Out;
	}
}

FPTToolEditorModeToolkit::FPTToolEditorModeToolkit()
	: SelectedTab(EPTToolTab::Generate)
{
}

void FSplineActorTableRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	Item = InArgs._Item;
	Toolkit = InArgs._Toolkit;
	using RowSuper = SMultiColumnTableRow<FSplineActorRowPtr>;
	RowSuper::Construct(RowSuper::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> FSplineActorTableRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (!Item.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	if (ColumnName == "Order")
	{
		return SNew(STextBlock).Text(FText::AsNumber(Item->GetOrder()));
	}
	if (ColumnName == "Name")
	{
		return SNew(STextBlock).Text(FText::FromString(Item->GetName()));
	}
	if (ColumnName == "Location")
	{
		return SNew(STextBlock).Text(FText::FromString(Item->GetLocation().ToString()));
	}
	if (ColumnName == "Actions")
	{
		auto MakeIconButton = [this](const FName& BrushName, const FText& ToolTip, TFunction<void()> OnClick)
		{
			return SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ContentPadding(FMargin(2, 2))
				.ToolTipText(ToolTip)
				.OnClicked_Lambda([OnClick]()
				{
					OnClick();
					return FReply::Handled();
				})
				[
					SNew(SBox)
					.WidthOverride(16)
					.HeightOverride(16)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush(BrushName))
					]
				];
		};

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			MakeIconButton(
				"Icons.ChevronUp",
				FText::FromString("Move Up"),
				[this]()
				{
					if (Toolkit)
					{
						Toolkit->MoveSplineActorRowUp(Item);
					}
				})
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2, 0).VAlign(VAlign_Center)
		[
			MakeIconButton(
				"Icons.ChevronDown",
				FText::FromString("Move Down"),
				[this]()
				{
					if (Toolkit)
					{
						Toolkit->MoveSplineActorRowDown(Item);
					}
				})
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2, 0).VAlign(VAlign_Center)
		[
			MakeIconButton(
				"Icons.Delete",
				FText::FromString("Delete"),
				[this]()
				{
					if (Toolkit)
					{
						Toolkit->DeleteSplineActorRow(Item);
					}
				})
		];
	}

	return SNullWidget::NullWidget;
}

UPTSplineManager* FPTToolEditorModeToolkit::InitializePTSplineManager()
{
	if (SplineManager == nullptr)
	{
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (World != nullptr)
		{
			SplineManager = NewObject<UPTSplineManager>(GetTransientPackage(), UPTSplineManager::StaticClass());
			SplineManager->AddToRoot();
			return SplineManager;
		}
		return nullptr;
	}

	return SplineManager;
}

void FPTToolEditorModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
{
	SetupPropertyEditorModule(this);
	InitializePTSplineManager();

	RefreshSplineActorList();

	FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FPTToolEditorModeToolkit::OnActorPropertyChanged);

	//加载所有spline类
	SetSplineClassItems();
	
	UE_LOG(LogTemp, Warning, TEXT("Class path: %s"), *APTGameMode::StaticClass()->GetPathName());

	SelectedTab = EPTToolTab::Generate; // 默认选中“生成”

	//容器
	ToolkitWidget = SNew(SVerticalBox)

		//Menu Bar
		+ SVerticalBox::Slot().AutoHeight().Padding(2)
		[
			SNew(SHorizontalBox)
			//Generate Tab
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)
			[
				SNew(SCheckBox)
				.Style(FCoreStyle::Get(), "ToggleButtonCheckbox")
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
				{
					SelectedTab = EPTToolTab::Generate;
				})
				.IsChecked_Lambda([this]()
				{
					return SelectedTab == EPTToolTab::Generate ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				[
					SNew(STextBlock).Text(FText::FromString("Generate"))
				]
			]
			//Manage Tab
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)
			[
				SNew(SCheckBox)
				.Style(FCoreStyle::Get(), "ToggleButtonCheckbox")
				.IsChecked_Lambda([this]()
				{
					return SelectedTab == EPTToolTab::Manage ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
				{
					SelectedTab = EPTToolTab::Manage;
				})
				[
					SNew(STextBlock).Text(FText::FromString("Manage"))
				]
			]
			//Test Tab
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)
			[
				SNew(SCheckBox)
				.Style(FCoreStyle::Get(), "ToggleButtonCheckbox")
				.IsChecked_Lambda([this]() { return SelectedTab == EPTToolTab::Test ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) { SelectedTab = EPTToolTab::Test; })
				[
					SNew(STextBlock).Text(FText::FromString("Test"))
				]
			]
		]

		//Content Area
		+ SVerticalBox::Slot().AutoHeight().Padding(4)
		[
			SNew(SBox).MinDesiredHeight(100)
			[
				SNew(SVerticalBox)

				// Generate 内容面板（包括了 Spline 列表，待生成的Spline 类型，放置 Spline 的属性面板）
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					SNew(SBox).Visibility_Lambda([this]()
					{
						return SelectedTab == EPTToolTab::Generate ? EVisibility::Visible : EVisibility::Collapsed;
					})
					[
						SNew(SVerticalBox)

						// 下拉标题
						+ SVerticalBox::Slot().AutoHeight().Padding(2)
						[
							SNew(STextBlock).Text(FText::FromString("Select SplineActor Class:"))
						]

						//List
						+ SVerticalBox::Slot().AutoHeight().Padding(4)
						[
							SAssignNew(SplineListView_Generate, SListView<FSplineActorRowPtr>)
							.ListItemsSource(&SplineActorRows)
							.OnGenerateRow_Lambda([](FSplineActorRowPtr InItem, const TSharedRef<STableViewBase>& OwnerTable)
							{
								return SNew(STableRow<FSplineActorRowPtr>, OwnerTable)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(2.5f)
										[
											SNew(STextBlock).Text(FText::FromString(FString::FromInt(InItem->GetOrder())))
										]
										+ SHorizontalBox::Slot().FillWidth(2.5f)
										[
											SNew(STextBlock).Text(FText::FromString(InItem->GetName()))
										]
										+ SHorizontalBox::Slot().FillWidth(2.5f)
										[
											SNew(STextBlock).Text(FText::FromString(InItem->GetLocation().ToString()))
										]
									];
							})
							.OnSelectionChanged_Lambda([this](FSplineActorRowPtr InItem, ESelectInfo::Type)
							{
								SelectedRow = InItem;
							})
							.OnMouseButtonDoubleClick_Lambda([this](FSplineActorRowPtr Item)
							{
								// 切换到 Manage 页签
								SelectedTab = EPTToolTab::Manage;
								if (Item.IsValid() && Item->GetActor().IsValid())
								{
									GEditor->SelectNone(false, true); // 清除已有选择
									GEditor->SelectActor(Item->GetActor().Get(), true, true); // 选择 Actor

									GEditor->MoveViewportCamerasToActor(*Item->GetActor().Get(), false); // 移动视图到 Actor
								}
							})
							.HeaderRow
							(
								SNew(SHeaderRow)
								+ SHeaderRow::Column("Order")
								.DefaultLabel(FText::FromString("Spline Order"))
								.FillWidth(0.5f)
								+ SHeaderRow::Column("Name")
								.DefaultLabel(FText::FromString("Actor Name"))
								.FillWidth(0.5f)
								+ SHeaderRow::Column("Location")
								.DefaultLabel(FText::FromString("Location"))
								.FillWidth(0.5f)
							)
						]

						// 类型下拉框
						+ SVerticalBox::Slot().AutoHeight().Padding(2)
						[
							SNew(SComboBox<TSharedPtr<FSplineClassItem>>)
							.OptionsSource(&SplineClassItems)
							.OnGenerateWidget_Lambda([](TSharedPtr<FSplineClassItem> Item)
							{
								return SNew(STextBlock).Text(FText::FromString(Item->ToString()));
							})
							.OnSelectionChanged_Lambda([this](TSharedPtr<FSplineClassItem> NewItem, ESelectInfo::Type)
							{
								SelectedSplineItem = NewItem;
							})
							.InitiallySelectedItem(SelectedSplineItem)
							[
								SNew(STextBlock).Text_Lambda([this]()
								{
									return SelectedSplineItem.IsValid() ? FText::FromString(SelectedSplineItem->ToString()) : FText::FromString("Select");
								})
							]
						]

						//变换面板
						+ SVerticalBox::Slot().AutoHeight().Padding(4)
						[
							SettingsView.ToSharedRef()
						]

						// 按钮
						+ SVerticalBox::Slot().AutoHeight().Padding(4)
						[
							SNew(SButton)
							.Text(FText::FromString("Place SplineActor in the Scene"))
							.OnClicked_Lambda([this]()
							{
								if (SelectedSplineItem.IsValid() && SelectedSplineItem->Class)
								{
									if (UWorld* World = GEditor->GetEditorWorldContext().World())
									{
										FActorSpawnParameters Params;
										FTransform SpawnTransform;

										Params.Name = SettingsObject->SplineActorName;
										Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

										SpawnTransform.SetLocation(SettingsObject->Location);
										SpawnTransform.SetRotation(SettingsObject->Rotation.Quaternion());
										SpawnTransform.SetScale3D(SettingsObject->Scale);

										// Auto-resolve order collisions in the current level.
										const int32 DesiredOrder = SettingsObject ? SettingsObject->TestOrder : 0;
										const int32 ResolvedOrder = FindNextAvailableSplineOrder(World, DesiredOrder);

										APTSplinePathActor* ref = World->SpawnActor<APTSplinePathActor>(SelectedSplineItem->Class, SpawnTransform, Params);

										// 添加标记，便于识别和管理
										ref->Tags.Add(TEXT("PTTool_Generated"));

										// 设置输入的属性
										ref->SplineTestOrder = ResolvedOrder;
										ref->CameraFOV = SettingsObject->CameraFOV;
										ref->SplineVelocity = SettingsObject->SplineVelocity;

										// Optional: reflect the resolved order back into the settings so UI matches.
										if (SettingsObject && SettingsObject->TestOrder != ResolvedOrder)
										{
											SettingsObject->Modify();
											SettingsObject->TestOrder = ResolvedOrder;
										}

										// 改进：根据 Actor 朝向和间距均匀放置初始控制点
										{
											// 确保至少有 2 个点
											int32 NumPoints = FMath::Max(2, SettingsObject->InitSplinePoints);

											// 基点使用 Actor 的位置（SpawnTransform 也可）
											const FVector BaseLocation = ref->GetActorLocation();

											// 使用 SpawnTransform 的朝向作为前进方向（保证点随 Actor 旋转）
											const FVector Forward = SpawnTransform.GetRotation().RotateVector(FVector::ForwardVector).GetSafeNormal();

											// 间距：优先参考 SplineVelocity（若有意义），否则使用默认 150
											const float DefaultSpacing = 150.0f;
											float Spacing = DefaultSpacing;
											if (SettingsObject->SplineVelocity > KINDA_SMALL_NUMBER)
											{
												// 一个启发式转换：速度映射到距离（可根据需要调整系数）
												Spacing = FMath::Clamp(SettingsObject->SplineVelocity * 100.0f, 10.0f, 2000.0f);
											}

											for (int32 i = 0; i < NumPoints; ++i)
											{
												const FVector PointLocation = BaseLocation + Forward * Spacing * i;
												ref->AddSplinePoint(PointLocation);
											}
										}
										ref->SplineComponent->UpdateEditorMeshes();

										if (GEditor)
										{
											// 通知编辑器该 Actor 是关卡的一部分并需要保存
											ref->SetFlags(RF_Transactional);
											ref->Modify();

											// 标记关卡为脏，需要保存
											if (UPackage* Pkg = World->GetOutermost())
											{
												Pkg->SetDirtyFlag(true);
											}

										}
										UE_LOG(LogTemp, Warning, TEXT("Spawned SplineActor: %s"), *SelectedSplineItem->Class->GetName());
									}
								}
								RefreshSplineActorList();
								return FReply::Handled();
							})
						]
					]
				]

				// Manage 面板
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					SNew(SBox).Visibility_Lambda([this]() { return SelectedTab == EPTToolTab::Manage ? EVisibility::Visible : EVisibility::Collapsed; })
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(2)
						[
							SNew(SBorder)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SAssignNew(SplineListView_Manage, SListView<FSplineActorRowPtr>)
									.ListItemsSource(&SplineActorRows)
									.OnGenerateRow_Lambda([this](FSplineActorRowPtr InItem, const TSharedRef<STableViewBase>& OwnerTable)
									{
										return SNew(FSplineActorTableRow, OwnerTable)
											.Item(InItem)
											.Toolkit(this);
									})
									.OnSelectionChanged_Lambda([this](FSplineActorRowPtr InItem, ESelectInfo::Type)
									{
										SelectedRow = InItem;
										SelectedActorGuid = InItem.IsValid() ? InItem->ActorGuid : FGuid();
										SelectedActorPath = InItem.IsValid() ? InItem->ActorPath : FSoftObjectPath();
										if (ManageActorDetailsView.IsValid())
										{
											APTSplinePathActor* SelActor = (InItem.IsValid() && InItem->GetActor().IsValid()) ? InItem->GetActor().Get() : nullptr;
											ManageActorDetailsView->SetObject(SelActor);
										}
									})
									.OnMouseButtonDoubleClick_Lambda([this](FSplineActorRowPtr Item)
									{
										if (Item.IsValid() && Item->GetActor().IsValid())
										{
											GEditor->SelectNone(false, true);
											GEditor->SelectActor(Item->GetActor().Get(), true, true);
											GEditor->MoveViewportCamerasToActor(*Item->GetActor().Get(), false);
										}
									})
									.HeaderRow
									(
										SNew(SHeaderRow)
										+ SHeaderRow::Column("Order")
										.DefaultLabel(FText::FromString("Order"))
										.FixedWidth(60.0f)
										+ SHeaderRow::Column("Name")
										.DefaultLabel(FText::FromString("Actor Name"))
										.FillWidth(0.45f)
										+ SHeaderRow::Column("Location")
										.DefaultLabel(FText::FromString("Location"))
										.FillWidth(0.55f)
										+ SHeaderRow::Column("Actions")
										.DefaultLabel(FText::FromString(""))
										.HAlignHeader(HAlign_Right)
										.HAlignCell(HAlign_Right)
										.VAlignCell(VAlign_Center)
										.FixedWidth(90.0f)
									)
								]
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(4, 6, 4, 2)
						[
							SNew(STextBlock)
							.Text(FText::FromString("Selected Actor Properties"))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(4)
						[
							SNew(SBox)
							.MinDesiredHeight(220)
							[
								ManageActorDetailsView.IsValid() ? ManageActorDetailsView.ToSharedRef() : SNullWidget::NullWidget
							]
						]
					]
				]

				// Test 面板
				+ SVerticalBox::Slot().AutoHeight().Padding(4)
				[
					SNew(SButton)
					.Visibility_Lambda([this]() { return SelectedTab == EPTToolTab::Test ? EVisibility::Visible : EVisibility::Collapsed; })
					.Text(FText::FromString("Start Test"))
					.OnClicked_Lambda([this]()
					{
						UE_LOG(LogTemp, Log, TEXT("Test button clicked."));

						ExecuteTest();

						return FReply::Handled();
					})
				]
			]
		];


	FModeToolkit::Init(InitToolkitHost, InOwningMode);
}

void FPTToolEditorModeToolkit::GetToolPaletteNames(TArray<FName>& PaletteNames) const
{
	PaletteNames.Add(NAME_Default);
}


FName FPTToolEditorModeToolkit::GetToolkitFName() const
{
	return FName("PTToolEditorMode");
}

FText FPTToolEditorModeToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("DisplayName", "PTToolEditorMode Toolkit");
}

TSharedPtr<SWidget> FPTToolEditorModeToolkit::GetInlineContent() const
{
	return ToolkitWidget;
}

void FPTToolEditorModeToolkit::RefreshSplineActorList()
{
	// 记录当前选中（当列表重建时用来恢复）
	if (SelectedRow.IsValid() && SelectedRow->GetActor().IsValid())
	{
		SelectedActorGuid = SelectedRow->ActorGuid;
		SelectedActorPath = SelectedRow->ActorPath;
	}
	else if (ManageActorDetailsView.IsValid())
	{
		// 如果 DetailsView 里当前有对象，也尝试从它恢复 key
		if (UObject* Obj = ManageActorDetailsView->GetSelectedObjects().Num() > 0 ? ManageActorDetailsView->GetSelectedObjects()[0].Get() : nullptr)
		{
			if (APTSplinePathActor* ActorFromDetails = Cast<APTSplinePathActor>(Obj))
			{
				SelectedActorGuid = ActorFromDetails->GetActorGuid();
				SelectedActorPath = FSoftObjectPath(ActorFromDetails);
			}
		}
	}

	SplineActorRows.Empty();
	SplineManager->Splines.Empty();
	
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
		return;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		APTSplinePathActor* Actor = Cast<APTSplinePathActor>(*It);

		if (Actor && Actor->Tags.Contains("PTTool_Generated"))
		{
			TSharedPtr<FSplineActorRow> Row = MakeShared<FSplineActorRow>(Actor);

			// 插入到有序位置
			int32 InsertIndex = 0;
			while (InsertIndex < SplineActorRows.Num() && SplineActorRows[InsertIndex]->GetActor()->SplineTestOrder < Actor->SplineTestOrder)
			{
				++InsertIndex;
			}
			SplineActorRows.Insert(Row, InsertIndex);


			// 同理插入到 Manager 中
			int32 InsertIndexMgr = 0;
			while (InsertIndexMgr < SplineManager->Splines.Num() && SplineManager->Splines[InsertIndexMgr]->SplineTestOrder < Actor->SplineTestOrder)
			{
				++InsertIndexMgr;
			}
			SplineManager->Splines.Insert(Actor, InsertIndexMgr);
		}
	}

	if (SplineListView_Generate.IsValid())
	{
		SplineListView_Generate->RequestListRefresh();
	}
	if (SplineListView_Manage.IsValid())
	{
		SplineListView_Manage->RequestListRefresh();

		// 列表重建后恢复选中，避免属性修改触发刷新导致失焦
		FSplineActorRowPtr RowToSelect;
		if (SelectedActorGuid.IsValid())
		{
			for (const FSplineActorRowPtr& Row : SplineActorRows)
			{
				if (Row.IsValid() && Row->ActorGuid == SelectedActorGuid)
				{
					RowToSelect = Row;
					break;
				}
			}
		}
		if (!RowToSelect.IsValid() && SelectedActorPath.IsValid())
		{
			for (const FSplineActorRowPtr& Row : SplineActorRows)
			{
				if (Row.IsValid() && Row->ActorPath == SelectedActorPath)
				{
					RowToSelect = Row;
					break;
				}
			}
		}

		if (RowToSelect.IsValid())
		{
			SelectedRow = RowToSelect;
			SplineListView_Manage->SetSelection(RowToSelect, ESelectInfo::Direct);

			if (ManageActorDetailsView.IsValid())
			{
				APTSplinePathActor* SelActor = RowToSelect->GetActor().Get();
				ManageActorDetailsView->SetObject(SelActor);
			}
		}
		else
		{
			// 如果选中的 Actor 已不存在/不在列表中，清空面板
			SelectedRow.Reset();
			if (ManageActorDetailsView.IsValid())
			{
				ManageActorDetailsView->SetObject(nullptr);
			}
		}
	}
}

void FPTToolEditorModeToolkit::SetSplineClassItems()
{
	SplineClassItems.Empty();
	SelectedSplineItem.Reset();

	// 1. 添加所有原生的 C++ 类（非抽象）
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (It->IsChildOf(APTSplinePathActor::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract))
		{
			const FString Name = It->GetName();
			SplineClassItems.Add(MakeShared<FSplineClassItem>(Name, *It));
		}
	}

	// 2. 添加蓝图生成的类（AssetRegistry）
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	TArray<FAssetData> BlueprintAssets;
	AssetRegistryModule.Get().GetAssetsByClass(
		FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Blueprint")),
		BlueprintAssets,
		true
	);

	for (const FAssetData& Asset : BlueprintAssets)
	{
		FString ParentClassPath;
		if (Asset.GetTagValue("ParentClass", ParentClassPath))
		{
			ParentClassPath = FPackageName::ExportTextPathToObjectPath(ParentClassPath);
			UClass* ParentClass = FindObject<UClass>(nullptr, *ParentClassPath);

			if (ParentClass && ParentClass->IsChildOf(APTSplinePathActor::StaticClass()))
			{
				UBlueprint* Blueprint = Cast<UBlueprint>(Asset.GetAsset());
				if (Blueprint && Blueprint->GeneratedClass)
				{
					SplineClassItems.Add(MakeShared<FSplineClassItem>(Asset.AssetName.ToString(), Blueprint->GeneratedClass));
				}
			}
		}
	}


	if (SplineClassItems.Num() > 0)
	{
		SelectedSplineItem = SplineClassItems[0];
	}
}

void FPTToolEditorModeToolkit::ShutdownUI()
{
	FModeToolkit::ShutdownUI();
	if (SplineManager)
	{
		SplineManager = nullptr;
	}
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
}

void FPTToolEditorModeToolkit::OnActorDeleted(AActor* DeletedActor)
{
	if (DeletedActor && DeletedActor->Tags.Contains("PTTool_Generated"))
	{
		RefreshSplineActorList();
	}
}

void FPTToolEditorModeToolkit::OnActorPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (!SplineManager) return;

	APTSplinePathActor* CastActor = Cast<APTSplinePathActor>(ObjectBeingModified);
	if (!CastActor)
	{
		return;
	}

	// 只在会影响 Manage 列表显示/排序的属性变化时刷新，避免 DetailsView 编辑过程中反复重建列表导致失焦
	FProperty* ChangedProperty = PropertyChangedEvent.Property;
	if (ChangedProperty)
	{
		const FName PropName = ChangedProperty->GetFName();
		static const FName Name_SplineTestOrder(TEXT("SplineTestOrder"));
		// 如果后续列表显示字段新增，可在这里补充
		if (PropName != Name_SplineTestOrder)
		{
			return;
		}
	}

	RefreshSplineActorList();
}
FString GetCurrentMapAssetPath()
{
	if (GEditor)
	{
		UWorld* World = GEditor->GetEditorWorldContext().World();
		if (World)
		{
			FString MapPath = World->GetOutermost()->GetName();  // 返回类似 "/Game/ToolLevel/AnimationCreator"
			return MapPath;
		}
	}
	return TEXT("");  // 出错时返回空字符串
}
void ExecuteTest()
{
	// 获取项目路径（.uproject）
	FString ProjectPath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());

	// 获取当前关卡的路径
	UWorld* World = GEditor->GetEditorWorldContext().World();
	FString MapAssetPath = World && World->PersistentLevel ? World->PersistentLevel->GetOutermost()->GetName() : TEXT("");

	// 确保路径格式为 /Game/xxx
	if (!MapAssetPath.StartsWith(TEXT("/Game")))
	{
		UE_LOG(LogTemp, Error, TEXT("Map path is invalid: %s"), *MapAssetPath);
		return;
	}

	// 拼接命令参数
	FString Params = FString::Printf(
			TEXT("\"%s\" %s?game=/Script/PTTool.PTGameMode -game -log"),
		*ProjectPath,
		*MapAssetPath
	);

	// 查找 UnrealEditor.exe（或 UnrealEditor-Win64-Debug.exe 视你的配置而定）
	FString EditorExe = FPlatformProcess::GenerateApplicationPath(TEXT("UnrealEditor"), FApp::GetBuildConfiguration());

	// 启动新进程
	FProcHandle ProcHandle = FPlatformProcess::CreateProc(
		*EditorExe,
		*Params,
		true,   // bLaunchDetached
		false,  // bLaunchHidden
		false,  // bLaunchReallyHidden
		nullptr,
		0,
		NULL,
		NULL
	);

	if (ProcHandle.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("Launched test process:\nExecutable: %s\nParams: %s"), *EditorExe, *Params);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to launch test process."));
	}
}

// ------------------------------------------------------------
// Manage list actions
// ------------------------------------------------------------

void FPTToolEditorModeToolkit::DeleteSplineActorRow(const FSplineActorRowPtr& Row)
{
	if (!Row.IsValid())
	{
		return;
	}

	APTSplinePathActor* Actor = Row->GetActor().Get();
	if (!IsValid(Actor) || Actor->IsActorBeingDestroyed())
	{
		RefreshSplineActorList();
		return;
	}

	FScopedTransaction Transaction(NSLOCTEXT("PTToolEditorModeToolkit", "DeleteSplineActor", "Delete PTTool Spline Actor"));
	Actor->Modify();

	if (UWorld* World = Actor->GetWorld())
	{
#if WITH_EDITOR
		World->EditorDestroyActor(Actor, true);
#else
		Actor->Destroy();
#endif
	}
	else
	{
		Actor->Destroy();
	}

	if (SelectedRow.IsValid() && SelectedRow == Row)
	{
		SelectedRow.Reset();
		if (ManageActorDetailsView.IsValid())
		{
			ManageActorDetailsView->SetObject(nullptr);
		}
	}

	RefreshSplineActorList();
}

void FPTToolEditorModeToolkit::SwapSplineActorOrders(APTSplinePathActor* A, APTSplinePathActor* B)
{
	if (!IsValid(A) || !IsValid(B) || A == B)
	{
		return;
	}

	FScopedTransaction Transaction(NSLOCTEXT("PTToolEditorModeToolkit", "SwapSplineOrder", "Swap PTTool Spline Order"));
	A->Modify();
	B->Modify();

	const int32 Temp = A->SplineTestOrder;
	A->SplineTestOrder = B->SplineTestOrder;
	B->SplineTestOrder = Temp;
}

void FPTToolEditorModeToolkit::MoveSplineActorRowUp(const FSplineActorRowPtr& Row)
{
	if (!Row.IsValid() || !Row->GetActor().IsValid())
	{
		return;
	}

	const int32 Index = SplineActorRows.IndexOfByKey(Row);
	if (Index == INDEX_NONE || Index <= 0)
	{
		return;
	}

	APTSplinePathActor* Current = Row->GetActor().Get();
	APTSplinePathActor* Prev = SplineActorRows[Index - 1].IsValid() ? SplineActorRows[Index - 1]->GetActor().Get() : nullptr;
	SwapSplineActorOrders(Current, Prev);
	RefreshSplineActorList();
}

void FPTToolEditorModeToolkit::MoveSplineActorRowDown(const FSplineActorRowPtr& Row)
{
	if (!Row.IsValid() || !Row->GetActor().IsValid())
	{
		return;
	}

	const int32 Index = SplineActorRows.IndexOfByKey(Row);
	if (Index == INDEX_NONE || Index >= SplineActorRows.Num() - 1)
	{
		return;
	}

	APTSplinePathActor* Current = Row->GetActor().Get();
	APTSplinePathActor* Next = SplineActorRows[Index + 1].IsValid() ? SplineActorRows[Index + 1]->GetActor().Get() : nullptr;
	SwapSplineActorOrders(Current, Next);
	RefreshSplineActorList();
}

TSharedRef<SWidget> FPTToolEditorModeToolkit::BuildStandaloneWidget()
{
	// Initialize necessary components if not already done
	if (!SettingsView.IsValid())
	{
		SetupPropertyEditorModule(this);
	}
	
	if (!SplineManager)
	{
		InitializePTSplineManager();
	}

	if (SplineActorRows.Num() == 0)
	{
		RefreshSplineActorList();
	}

	if (SplineClassItems.Num() == 0)
	{
		SetSplineClassItems();
	}

	FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FPTToolEditorModeToolkit::OnActorPropertyChanged);

	SelectedTab = EPTToolTab::Generate; // 默认选中"生成"

	//容器
	TSharedRef<SWidget> Widget = SNew(SVerticalBox)

		//Menu Bar
		+ SVerticalBox::Slot().AutoHeight().Padding(2)
		[
			SNew(SHorizontalBox)
			//Generate Tab
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)
			[
				SNew(SCheckBox)
				.Style(FCoreStyle::Get(), "ToggleButtonCheckbox")
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
				{
					SelectedTab = EPTToolTab::Generate;
				})
				.IsChecked_Lambda([this]()
				{
					return SelectedTab == EPTToolTab::Generate ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				[
					SNew(STextBlock).Text(FText::FromString("Generate"))
				]
			]
			//Manage Tab
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)
			[
				SNew(SCheckBox)
				.Style(FCoreStyle::Get(), "ToggleButtonCheckbox")
				.IsChecked_Lambda([this]()
				{
					return SelectedTab == EPTToolTab::Manage ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
				{
					SelectedTab = EPTToolTab::Manage;
				})
				[
					SNew(STextBlock).Text(FText::FromString("Manage"))
				]
			]
			//Test Tab
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)
			[
				SNew(SCheckBox)
				.Style(FCoreStyle::Get(), "ToggleButtonCheckbox")
				.IsChecked_Lambda([this]() { return SelectedTab == EPTToolTab::Test ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) { SelectedTab = EPTToolTab::Test; })
				[
					SNew(STextBlock).Text(FText::FromString("Test"))
				]
			]
		]

		//Content Area
		+ SVerticalBox::Slot().AutoHeight().Padding(4)
		[
			SNew(SBox).MinDesiredHeight(100)
			[
				SNew(SVerticalBox)

				// Generate 内容面板（包括了 Spline 列表，待生成的Spline 类型，放置 Spline 的属性面板）
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					SNew(SBox).Visibility_Lambda([this]()
					{
						return SelectedTab == EPTToolTab::Generate ? EVisibility::Visible : EVisibility::Collapsed;
					})
					[
						SNew(SVerticalBox)

						// 下拉标题
						+ SVerticalBox::Slot().AutoHeight().Padding(2)
						[
							SNew(STextBlock).Text(FText::FromString("Select SplineActor Class:"))
						]

						//List
						+ SVerticalBox::Slot().AutoHeight().Padding(4)
						[
							SAssignNew(SplineListView_Generate, SListView<FSplineActorRowPtr>)
							.ListItemsSource(&SplineActorRows)
							.OnGenerateRow_Lambda([](FSplineActorRowPtr InItem, const TSharedRef<STableViewBase>& OwnerTable)
							{
								return SNew(STableRow<FSplineActorRowPtr>, OwnerTable)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(2.5f)
										[
											SNew(STextBlock).Text(FText::FromString(FString::FromInt(InItem->GetOrder())))
										]
										+ SHorizontalBox::Slot().FillWidth(2.5f)
										[
											SNew(STextBlock).Text(FText::FromString(InItem->GetName()))
										]
										+ SHorizontalBox::Slot().FillWidth(2.5f)
										[
											SNew(STextBlock).Text(FText::FromString(InItem->GetLocation().ToString()))
										]
									];
							})
							.OnSelectionChanged_Lambda([this](FSplineActorRowPtr InItem, ESelectInfo::Type)
							{
								SelectedRow = InItem;
							})
							.OnMouseButtonDoubleClick_Lambda([this](FSplineActorRowPtr Item)
							{
								// 切换到 Manage 页签
								SelectedTab = EPTToolTab::Manage;
								if (Item.IsValid() && Item->GetActor().IsValid())
								{
									GEditor->SelectNone(false, true); // 清除已有选择
									GEditor->SelectActor(Item->GetActor().Get(), true, true); // 选择 Actor

									GEditor->MoveViewportCamerasToActor(*Item->GetActor().Get(), false); // 移动视图到 Actor
								}
							})
							.HeaderRow
							(
								SNew(SHeaderRow)
								+ SHeaderRow::Column("Order")
								.DefaultLabel(FText::FromString("Spline Order"))
								.FillWidth(0.5f)
								+ SHeaderRow::Column("Name")
								.DefaultLabel(FText::FromString("Actor Name"))
								.FillWidth(0.5f)
								+ SHeaderRow::Column("Location")
								.DefaultLabel(FText::FromString("Location"))
								.FillWidth(0.5f)
							)
						]

						// 类型下拉框
						+ SVerticalBox::Slot().AutoHeight().Padding(2)
						[
							SNew(SComboBox<TSharedPtr<FSplineClassItem>>)
							.OptionsSource(&SplineClassItems)
							.OnGenerateWidget_Lambda([](TSharedPtr<FSplineClassItem> Item)
							{
								return SNew(STextBlock).Text(FText::FromString(Item->ToString()));
							})
							.OnSelectionChanged_Lambda([this](TSharedPtr<FSplineClassItem> NewItem, ESelectInfo::Type)
							{
								SelectedSplineItem = NewItem;
							})
							.InitiallySelectedItem(SelectedSplineItem)
							[
								SNew(STextBlock).Text_Lambda([this]()
								{
									return SelectedSplineItem.IsValid() ? FText::FromString(SelectedSplineItem->ToString()) : FText::FromString("Select");
								})
							]
						]

						//变换面板
						+ SVerticalBox::Slot().AutoHeight().Padding(4)
						[
							SettingsView.ToSharedRef()
						]

						// 按钮
						+ SVerticalBox::Slot().AutoHeight().Padding(4)
						[
							SNew(SButton)
							.Text(FText::FromString("Place SplineActor in the Scene"))
							.OnClicked_Lambda([this]()
							{
								if (SelectedSplineItem.IsValid() && SelectedSplineItem->Class)
								{
									if (UWorld* World = GEditor->GetEditorWorldContext().World())
									{
										FActorSpawnParameters Params;
										FTransform SpawnTransform;

										Params.Name = SettingsObject->SplineActorName;
										Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

										SpawnTransform.SetLocation(SettingsObject->Location);
										SpawnTransform.SetRotation(SettingsObject->Rotation.Quaternion());
										SpawnTransform.SetScale3D(SettingsObject->Scale);

										// Auto-resolve order collisions in the current level.
										const int32 DesiredOrder = SettingsObject ? SettingsObject->TestOrder : 0;
										const int32 ResolvedOrder = FindNextAvailableSplineOrder(World, DesiredOrder);

										APTSplinePathActor* ref = World->SpawnActor<APTSplinePathActor>(SelectedSplineItem->Class, SpawnTransform, Params);

										// 添加标记，便于识别和管理
										ref->Tags.Add(TEXT("PTTool_Generated"));

										// 设置输入的属性
										ref->SplineTestOrder = ResolvedOrder;
										ref->CameraFOV = SettingsObject->CameraFOV;
										ref->SplineVelocity = SettingsObject->SplineVelocity;

										// Optional: reflect the resolved order back into the settings so UI matches.
										if (SettingsObject && SettingsObject->TestOrder != ResolvedOrder)
										{
											SettingsObject->Modify();
											SettingsObject->TestOrder = ResolvedOrder;
										}

										// 改进：根据 Actor 朝向和间距均匀放置初始控制点
										{
											// 确保至少有 2 个点
											int32 NumPoints = FMath::Max(2, SettingsObject->InitSplinePoints);

											// 基点使用 Actor 的位置（SpawnTransform 也可）
											const FVector BaseLocation = ref->GetActorLocation();

											// 使用 SpawnTransform 的朝向作为前进方向（保证点随 Actor 旋转）
											const FVector Forward = SpawnTransform.GetRotation().RotateVector(FVector::ForwardVector).GetSafeNormal();

											// 间距：优先参考 SplineVelocity（若有意义），否则使用默认 150
											const float DefaultSpacing = 150.0f;
											float Spacing = DefaultSpacing;
											if (SettingsObject->SplineVelocity > KINDA_SMALL_NUMBER)
											{
												// 一个启发式转换：速度映射到距离（可根据需要调整系数）
												Spacing = FMath::Clamp(SettingsObject->SplineVelocity * 100.0f, 10.0f, 2000.0f);
											}

											for (int32 i = 0; i < NumPoints; ++i)
											{
												const FVector PointLocation = BaseLocation + Forward * Spacing * i;
												ref->AddSplinePoint(PointLocation);
											}
										}
										ref->SplineComponent->UpdateEditorMeshes();

										if (GEditor)
										{
											// 通知编辑器该 Actor 是关卡的一部分并需要保存
											ref->SetFlags(RF_Transactional);
											ref->Modify();

											// 标记关卡为脏，需要保存
											if (UPackage* Pkg = World->GetOutermost())
											{
												Pkg->SetDirtyFlag(true);
											}

										}
										UE_LOG(LogTemp, Warning, TEXT("Spawned SplineActor: %s"), *SelectedSplineItem->Class->GetName());
									}
								}
								RefreshSplineActorList();
								return FReply::Handled();
							})
						]
					]
				]

				// Manage 面板
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					SNew(SBox).Visibility_Lambda([this]() { return SelectedTab == EPTToolTab::Manage ? EVisibility::Visible : EVisibility::Collapsed; })
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(2)
						[
							SNew(SBorder)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SAssignNew(SplineListView_Manage, SListView<FSplineActorRowPtr>)
									.ListItemsSource(&SplineActorRows)
									.OnGenerateRow_Lambda([this](FSplineActorRowPtr InItem, const TSharedRef<STableViewBase>& OwnerTable)
									{
										return SNew(FSplineActorTableRow, OwnerTable)
											.Item(InItem)
											.Toolkit(this);
									})
									.OnSelectionChanged_Lambda([this](FSplineActorRowPtr InItem, ESelectInfo::Type)
									{
										SelectedRow = InItem;
										SelectedActorGuid = InItem.IsValid() ? InItem->ActorGuid : FGuid();
										SelectedActorPath = InItem.IsValid() ? InItem->ActorPath : FSoftObjectPath();
										if (ManageActorDetailsView.IsValid())
										{
											APTSplinePathActor* SelActor = (InItem.IsValid() && InItem->GetActor().IsValid()) ? InItem->GetActor().Get() : nullptr;
											ManageActorDetailsView->SetObject(SelActor);
										}
									})
									.OnMouseButtonDoubleClick_Lambda([this](FSplineActorRowPtr Item)
									{
										if (Item.IsValid() && Item->GetActor().IsValid())
										{
											GEditor->SelectNone(false, true);
											GEditor->SelectActor(Item->GetActor().Get(), true, true);
											GEditor->MoveViewportCamerasToActor(*Item->GetActor().Get(), false);
										}
									})
									.HeaderRow
									(
										SNew(SHeaderRow)
										+ SHeaderRow::Column("Order")
										.DefaultLabel(FText::FromString("Order"))
										.FixedWidth(60.0f)
										+ SHeaderRow::Column("Name")
										.DefaultLabel(FText::FromString("Actor Name"))
										.FillWidth(0.45f)
										+ SHeaderRow::Column("Location")
										.DefaultLabel(FText::FromString("Location"))
										.FillWidth(0.55f)
										+ SHeaderRow::Column("Actions")
										.DefaultLabel(FText::FromString(""))
										.HAlignHeader(HAlign_Right)
										.HAlignCell(HAlign_Right)
										.VAlignCell(VAlign_Center)
										.FixedWidth(90.0f)
									)
								]
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(4, 6, 4, 2)
						[
							SNew(STextBlock)
							.Text(FText::FromString("Selected Actor Properties"))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(4)
						[
							SNew(SBox)
							.MinDesiredHeight(220)
							[
								ManageActorDetailsView.IsValid() ? ManageActorDetailsView.ToSharedRef() : SNullWidget::NullWidget
							]
						]
					]
				]

				// Test 面板
				+ SVerticalBox::Slot().AutoHeight().Padding(4)
				[
					SNew(SButton)
					.Visibility_Lambda([this]() { return SelectedTab == EPTToolTab::Test ? EVisibility::Visible : EVisibility::Collapsed; })
					.Text(FText::FromString("Start Test"))
					.OnClicked_Lambda([this]()
					{
						UE_LOG(LogTemp, Log, TEXT("Test button clicked."));

						ExecuteTest();

						return FReply::Handled();
					})
				]
			]
		];

	return Widget;
}

#undef LOCTEXT_NAMESPACE

