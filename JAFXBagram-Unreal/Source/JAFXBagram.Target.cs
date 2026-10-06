using UnrealBuildTool;
using System.Collections.Generic;

public class JAFXBagramTarget : TargetRules
{
	public JAFXBagramTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("JAFXBagram");
	}
}
