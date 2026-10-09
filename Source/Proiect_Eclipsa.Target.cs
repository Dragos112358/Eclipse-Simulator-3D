// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class Proiect_EclipsaTarget : TargetRules
{
    public Proiect_EclipsaTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
        ExtraModuleNames.Add("Proiect_Eclipsa");

       // BuildEnvironment = TargetBuildEnvironment.Unique;

        bUseLoggingInShipping = false;
        //bUseUnityBuild = true;
    }
}