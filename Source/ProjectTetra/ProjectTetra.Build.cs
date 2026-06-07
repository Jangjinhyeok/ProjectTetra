// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProjectTetra : ModuleRules
{
	public ProjectTetra(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(new string[] { ModuleDirectory });

		// ModelViewViewModel/FieldNotification: HUD ViewModel(MVVM + FieldNotify) — viewmodel.md.
		// UMG/Slate/SlateCore: Board Renderer(#12) — UUserWidget 기반 보드 위젯 + UniformGridPanel/Image 셀.
		// CommonUI/CommonInput: Menu #15 CommonUI 화면스택·입력라우팅 — CommonActivatableWidget/Stack + FUIInputConfig.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "ModelViewViewModel", "FieldNotification", "UMG", "Slate", "SlateCore", "CommonUI", "CommonInput" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
