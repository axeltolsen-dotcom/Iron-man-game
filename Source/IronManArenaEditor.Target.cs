// Copyright notice: sample project, free to modify.
using UnrealBuildTool;
using System.Collections.Generic;

public class IronManArenaEditorTarget : TargetRules
{
	public IronManArenaEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("IronManArena");
	}
}
