using UnrealBuildTool;
using System.Collections.Generic;

public class JAFXBagramEditorTarget : TargetRules
{
	public JAFXBagramEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("JAFXBagram");
	}
}
