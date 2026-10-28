using UnrealBuildTool;
public class CozyTerrainEditor : ModuleRules
{
    public CozyTerrainEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        // 거대한 UnrealEd 공용 PCH에 의존하지 않아 설치 환경의 메모리 부담을 줄인다.
        PCHUsage = PCHUsageMode.NoPCHs;
        PublicDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine","Landscape","ProceduralMeshComponent"});
        PrivateDependencyModuleNames.AddRange(new[]{"UnrealEd","Foliage","Slate","SlateCore","ToolMenus","PropertyEditor","InputCore","Json","JsonUtilities","AssetRegistry","RenderCore","RHI"});
    }
}
