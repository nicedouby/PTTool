#include "PTToolLegacyEdMode.h"
#include "PTToolLegacyModeToolkit.h"
#include "EditorModeManager.h"
#include "PTToolModule.h"

// 定义类的静态模式 ID（确保与模块中注册时使用的 ID 一致）
const FName FPTToolLegacyEdMode::EM_PTToolLegacyEdModeId = TEXT("EM_PTToolLegacyEdMode");

// 构造函数（如果头文件里声明了，请确保实现存在）
FPTToolLegacyEdMode::FPTToolLegacyEdMode()
	: FEdMode()
{
	// 初始化成员（如有需要）
}

// 析构函数（你之前缺失，链接器因此报错）
FPTToolLegacyEdMode::~FPTToolLegacyEdMode()
{
	// 清理（如有需要）
}

// 注册 Tab（Enter）/ 注销 Tab（Exit）
void FPTToolLegacyEdMode::Enter()
{
	FEdMode::Enter();

	// Ensure tool UI is visible as a dockable tab (like Landscape).
	FPTToolModule::TryInvokeLegacyTab();

	// Use the built-in Editor Mode toolkit host.
	if (!Toolkit.IsValid())
	{
		LegacyToolkit = MakeShared<FPTToolLegacyModeToolkit>();
		Toolkit = LegacyToolkit;
		Toolkit->Init(Owner->GetToolkitHost());
	}
}

void FPTToolLegacyEdMode::Exit()
{
	// Let the base clean up the toolkit.
	FEdMode::Exit();
}

// 以下两个虚函数以前在头里声明了但没有实现，导致链接错误。
// 简单实现为返回 true（允许使用变换小部件）。根据你的工具需要可改为 false 或更复杂的逻辑。
bool FPTToolLegacyEdMode::UsesTransformWidget() const
{
	return true;
}

bool FPTToolLegacyEdMode::UsesTransformWidget(UE::Widget::EWidgetMode InWidgetMode) const
{
	// 这里可以根据 InWidgetMode 决定是否启用变换小部件
	return true;
}