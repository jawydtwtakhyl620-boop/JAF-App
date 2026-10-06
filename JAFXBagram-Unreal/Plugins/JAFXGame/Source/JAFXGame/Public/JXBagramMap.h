// JAF X BAGRAM - builds a playable "blockout" of Bagram airbase out of simple shapes.
// Drop one into an empty level. Later, artists replace these shapes with real buildings.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JXBagramMap.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UVolumetricCloudComponent;
class UExponentialHeightFogComponent;

UCLASS()
class JAFXGAME_API AJXBagramMap : public AActor
{
	GENERATED_BODY()

public:
	AJXBagramMap();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bagram") int32 Seed = 2001;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bagram") bool bSpawnLoot = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bagram") bool bSpawnVehicles = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bagram", meta = (ClampMin = 0, ClampMax = 4)) int32 TankCount = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bagram", meta = (ClampMin = 0, ClampMax = 6)) int32 DShKCount = 1;
	/** Turn off once you sculpt a real Landscape, so the flat placeholder ground disappears. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bagram") bool bBuildGround = true;
	/** Turn off once real mountains exist in the Landscape. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bagram") bool bBuildMountains = true;
	/** Adds a realistic afternoon sky (sun, atmosphere, volumetric clouds, fog) unless the level already has a sun. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bagram") bool bAddSkyAndLighting = true;
	/** Sun angle above the horizon in degrees (lower = warmer, longer shadows). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bagram", meta = (ClampMin = 5, ClampMax = 90)) float SunElevation = 38.f;
	/** Compass direction the sun comes from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bagram") float SunHeading = 225.f;

	/** Rebuilds every shape (button in the Details panel). */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Bagram")
	void Rebuild();

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

private:
	UInstancedStaticMeshComponent* MakeISM(const TCHAR* Name, UStaticMesh* Mesh, bool bCollision);
	void ApplyColors();
	void ClearAll();

	// Building helpers. All sizes in meters; Z is the bottom of the shape.
	void Box(UInstancedStaticMeshComponent* ISM, float X, float Y, float Z, float SX, float SY, float SZ, float Yaw = 0.f);
	void House(float CX, float CY, float W, float D, float H, UInstancedStaticMeshComponent* WallISM, int32 Doors, int32 Loot);
	void Hangar(float CX, float CY);
	void GuardTower(float X, float Y);
	void ControlTower(float X, float Y);
	void PerimeterWall(float HalfX, float HalfY);
	void Compound(float CX, float CY);
	void HescoLine(float X0, float Y0, float X1, float Y1);
	void Tree(float X, float Y, float Scale);
	void Mountains();
	void WaterTower(float X, float Y);
	void PanelFence(float X0, float Y0, float X1, float Y1);
	void UpdateSky();
	void Boundary();

	UPROPERTY(VisibleAnywhere, Category = "Bagram") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Bagram") TObjectPtr<UInstancedStaticMeshComponent> Ground;
	UPROPERTY(VisibleAnywhere, Category = "Bagram") TObjectPtr<UInstancedStaticMeshComponent> Concrete;
	UPROPERTY(VisibleAnywhere, Category = "Bagram") TObjectPtr<UInstancedStaticMeshComponent> Markings;
	UPROPERTY(VisibleAnywhere, Category = "Bagram") TObjectPtr<UInstancedStaticMeshComponent> Walls;
	UPROPERTY(VisibleAnywhere, Category = "Bagram") TObjectPtr<UInstancedStaticMeshComponent> MudWalls;
	UPROPERTY(VisibleAnywhere, Category = "Bagram") TObjectPtr<UInstancedStaticMeshComponent> Metal;
	UPROPERTY(VisibleAnywhere, Category = "Bagram") TObjectPtr<UInstancedStaticMeshComponent> Roofs;
	UPROPERTY(VisibleAnywhere, Category = "Bagram") TObjectPtr<UInstancedStaticMeshComponent> Hesco;
	UPROPERTY(VisibleAnywhere, Category = "Bagram") TObjectPtr<UInstancedStaticMeshComponent> Crates;
	UPROPERTY(VisibleAnywhere, Category = "Bagram") TObjectPtr<UInstancedStaticMeshComponent> Glass;
	UPROPERTY(VisibleAnywhere, Category = "Bagram") TObjectPtr<UInstancedStaticMeshComponent> Tanks;
	UPROPERTY(VisibleAnywhere, Category = "Bagram") TObjectPtr<UInstancedStaticMeshComponent> Trunks;
	UPROPERTY(VisibleAnywhere, Category = "Bagram") TObjectPtr<UInstancedStaticMeshComponent> Leaves;
	UPROPERTY(VisibleAnywhere, Category = "Bagram") TObjectPtr<UInstancedStaticMeshComponent> Rock;
	UPROPERTY(VisibleAnywhere, Category = "Bagram") TObjectPtr<UInstancedStaticMeshComponent> Snow;
	UPROPERTY(VisibleAnywhere, Category = "Bagram") TObjectPtr<UInstancedStaticMeshComponent> Invisible;
	UPROPERTY(VisibleAnywhere, Category = "Bagram") TObjectPtr<UInstancedStaticMeshComponent> WaterTanks;

	UPROPERTY(VisibleAnywhere, Category = "Sky") TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY(VisibleAnywhere, Category = "Sky") TObjectPtr<USkyAtmosphereComponent> Atmosphere;
	UPROPERTY(VisibleAnywhere, Category = "Sky") TObjectPtr<USkyLightComponent> SkyLight;
	UPROPERTY(VisibleAnywhere, Category = "Sky") TObjectPtr<UVolumetricCloudComponent> Clouds;
	UPROPERTY(VisibleAnywhere, Category = "Sky") TObjectPtr<UExponentialHeightFogComponent> Fog;

	UPROPERTY() TArray<FVector> LootPoints;
	UPROPERTY() TArray<FTransform> TankSpots;
	UPROPERTY() TArray<FTransform> GunSpots;

	FRandomStream Rng;
};
