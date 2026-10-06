using UnrealBuildTool;

public class JAFXGame : ModuleRules
{
	public JAFXGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"GameplayTasks",
			"NavigationSystem",
			"Niagara",
			"DeveloperSettings",
			"Slate",
			"SlateCore"
		});
	}
}
