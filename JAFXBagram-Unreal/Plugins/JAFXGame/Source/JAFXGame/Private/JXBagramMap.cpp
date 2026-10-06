#include "JXBagramMap.h"
#include "JXMountedGun.h"
#include "JXPickup.h"
#include "JXSettings.h"
#include "JXTank.h"
#include "JXWeaponLibrary.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float M = 100.f;     // meters -> Unreal units
	constexpr float WallT = 0.4f;  // wall thickness (m)
	constexpr float DoorW = 1.6f;
	constexpr float DoorH = 2.4f;

	// Base layout (meters). Runway runs east-west (along X) through the middle.
	constexpr float BaseHalfX = 560.f;
	constexpr float BaseHalfY = 420.f;
	constexpr float PlayHalf = 720.f; // keep in sync with UJXSettings::MapHalfSize (72000 cm)
}

AJXBagramMap::AJXBagramMap()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeF(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylF(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereF(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeF(TEXT("/Engine/BasicShapes/Cone.Cone"));
	UStaticMesh* Cube = CubeF.Object;
	UStaticMesh* Cyl = CylF.Object;
	UStaticMesh* Sphere = SphereF.Object;
	UStaticMesh* Cone = ConeF.Object;

	Ground = MakeISM(TEXT("Ground"), Cube, true);
	Concrete = MakeISM(TEXT("Concrete"), Cube, true);
	Markings = MakeISM(TEXT("Markings"), Cube, false);
	Walls = MakeISM(TEXT("Walls"), Cube, true);
	MudWalls = MakeISM(TEXT("MudWalls"), Cube, true);
	Metal = MakeISM(TEXT("Metal"), Cube, true);
	Roofs = MakeISM(TEXT("Roofs"), Cube, true);
	Hesco = MakeISM(TEXT("Hesco"), Cube, true);
	Crates = MakeISM(TEXT("Crates"), Cube, true);
	Glass = MakeISM(TEXT("Glass"), Cube, true);
	Tanks = MakeISM(TEXT("FuelTanks"), Cyl, true);
	Trunks = MakeISM(TEXT("Trunks"), Cyl, true);
	Leaves = MakeISM(TEXT("Leaves"), Sphere, false);
	Rock = MakeISM(TEXT("Mountains"), Cone, true);
	Snow = MakeISM(TEXT("Snow"), Cone, false);
	Invisible = MakeISM(TEXT("Boundary"), Cube, true);
	Invisible->SetHiddenInGame(true);
	Invisible->SetCastShadow(false);
	Markings->SetCastShadow(false);
}

UInstancedStaticMeshComponent* AJXBagramMap::MakeISM(const TCHAR* Name, UStaticMesh* Mesh, bool bCollision)
{
	UInstancedStaticMeshComponent* ISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
	ISM->SetupAttachment(Root);
	if (Mesh) ISM->SetStaticMesh(Mesh);
	if (bCollision)
	{
		ISM->SetCollisionProfileName(TEXT("BlockAll"));
	}
	else
	{
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->SetCanEverAffectNavigation(false);
	}
	return ISM;
}

void AJXBagramMap::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void AJXBagramMap::ApplyColors()
{
	struct FEntry { UInstancedStaticMeshComponent* ISM; FLinearColor Color; };
	const FEntry Entries[] = {
		{ Ground, FLinearColor(0.36f, 0.30f, 0.20f) },
		{ Concrete, FLinearColor(0.30f, 0.30f, 0.28f) },
		{ Markings, FLinearColor(0.9f, 0.9f, 0.85f) },
		{ Walls, FLinearColor(0.55f, 0.50f, 0.40f) },
		{ MudWalls, FLinearColor(0.45f, 0.33f, 0.22f) },
		{ Metal, FLinearColor(0.33f, 0.36f, 0.35f) },
		{ Roofs, FLinearColor(0.18f, 0.18f, 0.18f) },
		{ Hesco, FLinearColor(0.50f, 0.43f, 0.30f) },
		{ Crates, FLinearColor(0.22f, 0.27f, 0.14f) },
		{ Glass, FLinearColor(0.08f, 0.16f, 0.22f) },
		{ Tanks, FLinearColor(0.75f, 0.75f, 0.72f) },
		{ Trunks, FLinearColor(0.25f, 0.17f, 0.10f) },
		{ Leaves, FLinearColor(0.16f, 0.30f, 0.10f) },
		{ Rock, FLinearColor(0.36f, 0.31f, 0.26f) },
		{ Snow, FLinearColor(0.95f, 0.95f, 1.0f) },
	};
	for (const FEntry& E : Entries)
	{
		if (!E.ISM) continue;
		UMaterialInterface* Base = E.ISM->GetStaticMesh() ? E.ISM->GetStaticMesh()->GetMaterial(0) : nullptr;
		if (!Base) continue;
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
		MID->SetVectorParameterValue(TEXT("Color"), E.Color);
		E.ISM->SetMaterial(0, MID);
	}
}

void AJXBagramMap::ClearAll()
{
	for (UInstancedStaticMeshComponent* ISM : { Ground.Get(), Concrete.Get(), Markings.Get(), Walls.Get(), MudWalls.Get(), Metal.Get(),
		Roofs.Get(), Hesco.Get(), Crates.Get(), Glass.Get(), Tanks.Get(), Trunks.Get(), Leaves.Get(), Rock.Get(), Snow.Get(), Invisible.Get() })
	{
		if (ISM) ISM->ClearInstances();
	}
	LootPoints.Reset();
	TankSpots.Reset();
	GunSpots.Reset();
}

void AJXBagramMap::Box(UInstancedStaticMeshComponent* ISM, float X, float Y, float Z, float SX, float SY, float SZ, float Yaw)
{
	if (!ISM || SX <= 0.f || SY <= 0.f || SZ <= 0.f) return;
	const FTransform T(FRotator(0.f, Yaw, 0.f), FVector(X * M, Y * M, (Z + SZ * 0.5f) * M), FVector(SX, SY, SZ));
	ISM->AddInstance(T);
}

void AJXBagramMap::House(float CX, float CY, float W, float D, float H, UInstancedStaticMeshComponent* WallISM, int32 Doors, int32 Loot)
{
	Box(Concrete, CX, CY, 0.f, W, D, 0.12f);

	// Pick distinct door sides. 0 = south (-Y), 1 = north (+Y), 2 = west (-X), 3 = east (+X).
	TArray<int32> Sides = { 0, 1, 2, 3 };
	for (int32 i = 0; i < 4; ++i) Sides.Swap(i, Rng.RandRange(i, 3));
	TSet<int32> DoorSides;
	for (int32 i = 0; i < FMath::Clamp(Doors, 1, 4); ++i) DoorSides.Add(Sides[i]);

	for (int32 Side = 0; Side < 4; ++Side)
	{
		const bool bAlongX = Side < 2;
		const float Len = bAlongX ? W : D;
		const float Fixed = Side == 0 ? CY - D * 0.5f : Side == 1 ? CY + D * 0.5f : Side == 2 ? CX - W * 0.5f : CX + W * 0.5f;
		const float Center = bAlongX ? CX : CY;

		TArray<TPair<float, float>> Segs;
		if (DoorSides.Contains(Side))
		{
			const float Off = Rng.FRandRange(-1.f, 1.f) * FMath::Max(0.f, Len * 0.5f - DoorW - 0.6f);
			Segs.Add({ -Len * 0.5f, Off - DoorW * 0.5f });
			Segs.Add({ Off + DoorW * 0.5f, Len * 0.5f });
			const float Mid = Center + Off;
			if (bAlongX) Box(WallISM, Mid, Fixed, DoorH, DoorW, WallT, H - DoorH);
			else Box(WallISM, Fixed, Mid, DoorH, WallT, DoorW, H - DoorH);
		}
		else
		{
			Segs.Add({ -Len * 0.5f, Len * 0.5f });
		}
		for (const TPair<float, float>& S : Segs)
		{
			const float L = S.Value - S.Key;
			if (L < 0.05f) continue;
			const float Mid = Center + (S.Key + S.Value) * 0.5f;
			if (bAlongX) Box(WallISM, Mid, Fixed, 0.f, L + WallT, WallT, H);
			else Box(WallISM, Fixed, Mid, 0.f, WallT, L + WallT, H);
		}
	}
	Box(Roofs, CX, CY, H, W + 0.6f, D + 0.6f, 0.3f);

	for (int32 i = 0; i < Loot; ++i)
	{
		LootPoints.Add(FVector((CX + Rng.FRandRange(-0.5f, 0.5f) * (W - 2.f)) * M, (CY + Rng.FRandRange(-0.5f, 0.5f) * (D - 2.f)) * M, 0.2f * M));
	}
}

void AJXBagramMap::Hangar(float CX, float CY)
{
	const float W = 40.f, D = 30.f, H = 12.f, OpenW = 30.f, OpenH = 9.f;
	Box(Concrete, CX, CY, 0.f, W, D, 0.12f);
	// South side (facing the runway) has a big opening.
	const float South = CY - D * 0.5f;
	Box(Metal, CX - (W + OpenW) * 0.25f, South, 0.f, (W - OpenW) * 0.5f, 0.5f, H);
	Box(Metal, CX + (W + OpenW) * 0.25f, South, 0.f, (W - OpenW) * 0.5f, 0.5f, H);
	Box(Metal, CX, South, OpenH, OpenW, 0.5f, H - OpenH);
	Box(Metal, CX, CY + D * 0.5f, 0.f, W, 0.5f, H);
	Box(Metal, CX - W * 0.5f, CY, 0.f, 0.5f, D, H);
	Box(Metal, CX + W * 0.5f, CY, 0.f, 0.5f, D, H);
	Box(Roofs, CX, CY, H, W + 1.f, D + 1.f, 0.5f);

	// Crates and loot inside.
	for (int32 i = 0; i < 6; ++i)
	{
		const float X = CX + Rng.FRandRange(-16.f, 16.f);
		const float Y = CY + Rng.FRandRange(-10.f, 12.f);
		const float S = Rng.FRandRange(1.2f, 2.f);
		Box(Crates, X, Y, 0.f, S, S, S, Rng.FRandRange(0.f, 90.f));
		if (Rng.FRand() < 0.5f) LootPoints.Add(FVector(X * M, Y * M, (S + 0.1f) * M));
	}
	for (int32 i = 0; i < 4; ++i)
	{
		LootPoints.Add(FVector((CX + Rng.FRandRange(-17.f, 17.f)) * M, (CY + Rng.FRandRange(-12.f, 12.f)) * M, 0.2f * M));
	}
}

void AJXBagramMap::GuardTower(float X, float Y)
{
	for (const float SX : { -1.6f, 1.6f })
	{
		for (const float SY : { -1.6f, 1.6f })
		{
			Box(Metal, X + SX, Y + SY, 0.f, 0.3f, 0.3f, 8.f);
			Box(Metal, X + SX * 1.15f, Y + SY * 1.15f, 8.3f, 0.2f, 0.2f, 2.2f);
		}
	}
	Box(Walls, X, Y, 8.f, 4.2f, 4.2f, 0.3f);
	Box(Walls, X, Y - 2.1f, 8.3f, 4.2f, 0.2f, 1.f);
	Box(Walls, X, Y + 2.1f, 8.3f, 4.2f, 0.2f, 1.f);
	Box(Walls, X - 2.1f, Y, 8.3f, 0.2f, 4.2f, 1.f);
	Box(Walls, X + 2.1f, Y, 8.3f, 0.2f, 4.2f, 1.f);
	Box(Roofs, X, Y, 10.5f, 5.f, 5.f, 0.25f);
}

void AJXBagramMap::ControlTower(float X, float Y)
{
	Box(Walls, X, Y, 0.f, 8.f, 8.f, 24.f);
	Box(Glass, X, Y, 24.f, 11.f, 11.f, 4.f);
	Box(Roofs, X, Y, 28.f, 12.f, 12.f, 0.4f);
	Box(Metal, X, Y, 28.4f, 0.3f, 0.3f, 6.f);
	// Small building at its foot with loot.
	House(X + 14.f, Y, 12.f, 9.f, 3.8f, Walls, 2, 3);
}

void AJXBagramMap::PerimeterWall(float HX, float HY)
{
	const float H = 4.f, T = 0.6f, Gate = 8.f, SideGateY = 150.f;
	// North and south walls with a gate in the middle.
	for (const float Y : { -HY, HY })
	{
		Box(Walls, (-HX - Gate) * 0.5f, Y, 0.f, HX - Gate, T, H);
		Box(Walls, (HX + Gate) * 0.5f, Y, 0.f, HX - Gate, T, H);
	}
	// East and west walls with a gate at SideGateY.
	for (const float X : { -HX, HX })
	{
		const float A = SideGateY - Gate, B = SideGateY + Gate;
		Box(Walls, X, (-HY + A) * 0.5f, 0.f, T, A + HY, H);
		Box(Walls, X, (B + HY) * 0.5f, 0.f, T, HY - B, H);
	}
	// Watch towers on corners and along the walls.
	for (const float X : { -HX, -HX * 0.5f, 0.f, HX * 0.5f, HX })
	{
		GuardTower(X + (X == 0.f ? 20.f : 0.f), -HY + 4.f);
		GuardTower(X + (X == 0.f ? 20.f : 0.f), HY - 4.f);
	}
	GuardTower(-HX + 4.f, -HY * 0.4f);
	GuardTower(HX - 4.f, -HY * 0.4f);
	// Hesco barriers at the gates.
	HescoLine(-20.f, -HY + 12.f, -8.f, -HY + 12.f);
	HescoLine(8.f, -HY + 12.f, 20.f, -HY + 12.f);
	HescoLine(-20.f, HY - 12.f, -8.f, HY - 12.f);
	HescoLine(8.f, HY - 12.f, 20.f, HY - 12.f);
}

void AJXBagramMap::Compound(float CX, float CY)
{
	// Traditional walled compound (qala) with a house inside.
	const float S = 22.f, H = 3.2f, T = 0.7f, GateW = 3.f;
	const int32 GateSide = Rng.RandRange(0, 3);
	for (int32 Side = 0; Side < 4; ++Side)
	{
		const bool bAlongX = Side < 2;
		const float Fixed = Side == 0 ? CY - S * 0.5f : Side == 1 ? CY + S * 0.5f : Side == 2 ? CX - S * 0.5f : CX + S * 0.5f;
		const float Center = bAlongX ? CX : CY;
		if (Side == GateSide)
		{
			const float L = (S - GateW) * 0.5f;
			const float A = Center - (GateW + L) * 0.5f, B = Center + (GateW + L) * 0.5f;
			if (bAlongX) { Box(MudWalls, A, Fixed, 0.f, L + T, T, H); Box(MudWalls, B, Fixed, 0.f, L + T, T, H); }
			else { Box(MudWalls, Fixed, A, 0.f, T, L + T, H); Box(MudWalls, Fixed, B, 0.f, T, L + T, H); }
		}
		else
		{
			if (bAlongX) Box(MudWalls, Center, Fixed, 0.f, S + T, T, H);
			else Box(MudWalls, Fixed, Center, 0.f, T, S + T, H);
		}
	}
	House(CX + Rng.FRandRange(-3.f, 3.f), CY + Rng.FRandRange(-3.f, 3.f), 10.f, 7.f, 3.f, MudWalls, 1, 2);
	Tree(CX + S * 0.35f, CY + S * 0.35f, Rng.FRandRange(0.8f, 1.2f));
}

void AJXBagramMap::HescoLine(float X0, float Y0, float X1, float Y1)
{
	const float Len = FMath::Sqrt(FMath::Square(X1 - X0) + FMath::Square(Y1 - Y0));
	const int32 N = FMath::Max(1, FMath::RoundToInt(Len / 1.1f));
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Y1 - Y0, X1 - X0));
	for (int32 i = 0; i < N; ++i)
	{
		const float A = (i + 0.5f) / N;
		Box(Hesco, FMath::Lerp(X0, X1, A), FMath::Lerp(Y0, Y1, A), 0.f, 1.1f, 1.1f, 1.4f, Yaw);
	}
}

void AJXBagramMap::Tree(float X, float Y, float Scale)
{
	Box(Trunks, X, Y, 0.f, 0.35f * Scale, 0.35f * Scale, 3.f * Scale);
	Box(Leaves, X, Y, 2.4f * Scale, 3.2f * Scale, 3.2f * Scale, 3.f * Scale);
}

void AJXBagramMap::Mountains()
{
	// Hindu Kush backdrop: a ring of snowy peaks outside the playable area.
	const int32 Count = 36;
	for (int32 i = 0; i < Count; ++i)
	{
		const float Angle = (i + Rng.FRandRange(-0.3f, 0.3f)) / Count * 2.f * PI;
		const float Dist = Rng.FRandRange(1050.f, 1300.f);
		const float R = Rng.FRandRange(220.f, 420.f);
		const float H = Rng.FRandRange(280.f, 720.f) * (FMath::Sin(Angle) > 0.f ? 1.25f : 0.9f); // higher in the north
		const float X = FMath::Cos(Angle) * Dist, Y = FMath::Sin(Angle) * Dist;
		Box(Rock, X, Y, -5.f, R * 2.f, R * 2.f, H, Rng.FRandRange(0.f, 360.f));
		Box(Snow, X, Y, -5.f + H * 0.66f, R * 2.f * 0.34f, R * 2.f * 0.34f, H * 0.34f);
	}
}

void AJXBagramMap::Boundary()
{
	const float H = 80.f;
	Box(Invisible, 0.f, PlayHalf + 1.f, -5.f, PlayHalf * 2.f + 4.f, 2.f, H);
	Box(Invisible, 0.f, -PlayHalf - 1.f, -5.f, PlayHalf * 2.f + 4.f, 2.f, H);
	Box(Invisible, PlayHalf + 1.f, 0.f, -5.f, 2.f, PlayHalf * 2.f + 4.f, H);
	Box(Invisible, -PlayHalf - 1.f, 0.f, -5.f, 2.f, PlayHalf * 2.f + 4.f, H);
}

void AJXBagramMap::Rebuild()
{
	ClearAll();
	Rng.Initialize(Seed);

	// Desert floor (2.8 km square, top surface at Z = 0).
	Box(Ground, 0.f, 0.f, -1.f, 2800.f, 2800.f, 1.f);

	// Runway with markings, taxiway, connectors and apron.
	Box(Concrete, 0.f, 0.f, 0.f, 960.f, 45.f, 0.1f);
	for (float X = -450.f; X <= 450.f; X += 30.f) Box(Markings, X, 0.f, 0.1f, 15.f, 0.8f, 0.02f);
	for (const float End : { -472.f, 472.f })
	{
		for (int32 i = 0; i < 8; ++i) Box(Markings, End, -17.5f + i * 5.f, 0.1f, 12.f, 1.8f, 0.02f);
	}
	Box(Concrete, 0.f, 70.f, 0.f, 820.f, 20.f, 0.1f);
	for (const float X : { -400.f, -130.f, 130.f, 400.f }) Box(Concrete, X, 41.f, 0.f, 18.f, 38.f, 0.1f);
	Box(Concrete, 0.f, 125.f, 0.f, 840.f, 90.f, 0.1f);

	// Hangars north of the apron, control tower in the gap.
	for (const float X : { -360.f, -240.f, -120.f, 120.f, 240.f, 360.f }) Hangar(X, 200.f);
	ControlTower(0.f, 230.f);

	// Admin / housing area in the north.
	for (const float Y : { 290.f, 352.f })
	{
		for (float X = -480.f; X <= 480.f; X += 60.f)
		{
			if (FMath::Abs(X) < 30.f && Y < 300.f) continue; // keep the tower clear
			House(X + Rng.FRandRange(-6.f, 6.f), Y + Rng.FRandRange(-3.f, 3.f), Rng.FRandRange(12.f, 16.f), Rng.FRandRange(9.f, 11.f),
				Rng.FRandRange(3.6f, 4.2f), Walls, Rng.RandRange(1, 2), Rng.RandRange(1, 2));
		}
	}

	// Barracks in the south.
	for (const float Y : { -140.f, -210.f, -280.f, -350.f })
	{
		for (float X = -455.f; X <= 455.f; X += 70.f)
		{
			House(X, Y, 16.f, 9.f, 3.6f, Walls, 2, Rng.RandRange(1, 2));
		}
	}

	// Fuel depot (east) behind hesco walls.
	for (const float X : { 495.f, 527.f })
	{
		for (const float Y : { 105.f, 140.f })
		{
			Box(Tanks, X, Y, 0.f, 12.f, 12.f, 7.f);
		}
	}
	HescoLine(480.f, 85.f, 545.f, 85.f);
	HescoLine(480.f, 160.f, 545.f, 160.f);
	HescoLine(480.f, 85.f, 480.f, 115.f);
	LootPoints.Add(FVector(511.f * M, 122.f * M, 0.2f * M));

	// Open field between runway and barracks: cover, a DShK nest and the tanks.
	for (int32 i = 0; i < 22; ++i)
	{
		const float X = Rng.FRandRange(-470.f, 470.f);
		const float Y = Rng.FRandRange(-110.f, -35.f);
		const float L = Rng.FRandRange(4.f, 10.f);
		if (Rng.FRand() < 0.5f) HescoLine(X, Y, X + L, Y);
		else
		{
			const float S = Rng.FRandRange(1.2f, 1.8f);
			Box(Crates, X, Y, 0.f, S, S, S, Rng.FRandRange(0.f, 90.f));
			if (Rng.FRand() < 0.6f) Box(Crates, X, Y, S, S * 0.8f, S * 0.8f, S * 0.8f, Rng.FRandRange(0.f, 90.f));
			LootPoints.Add(FVector((X + S + 0.5f) * M, Y * M, 0.f)); // beside the crate
		}
	}
	const FVector2D GunNests[] = { { 0.f, -65.f }, { -300.f, 160.f }, { 300.f, 160.f }, { 0.f, -395.f }, { -540.f, 0.f }, { 540.f, -60.f } };
	for (int32 i = 0; i < FMath::Min(DShKCount, (int32)UE_ARRAY_COUNT(GunNests)); ++i)
	{
		const FVector2D N = GunNests[i];
		// Sandbag ring with an opening at the back.
		for (int32 k = 0; k < 10; ++k)
		{
			const float A = (k + 3) / 13.f * 2.f * PI;
			Box(Hesco, N.X + FMath::Cos(A) * 3.f, N.Y + FMath::Sin(A) * 3.f, 0.f, 1.1f, 1.1f, 1.1f, FMath::RadiansToDegrees(A));
		}
		GunSpots.Add(FTransform(FRotator(0.f, -90.f, 0.f), FVector(N.X * M, N.Y * M, 50.f)));
	}
	const FVector2D TankPads[] = { { -260.f, -75.f }, { 260.f, -75.f }, { -500.f, 300.f }, { 500.f, 300.f } };
	for (int32 i = 0; i < FMath::Min(TankCount, (int32)UE_ARRAY_COUNT(TankPads)); ++i)
	{
		TankSpots.Add(FTransform(FRotator(0.f, i % 2 == 0 ? 0.f : 180.f, 0.f), FVector(TankPads[i].X * M, TankPads[i].Y * M, 300.f)));
	}

	PerimeterWall(BaseHalfX, BaseHalfY);

	// Villages outside the wall.
	TArray<FBox2D> Used;
	auto TryCompound = [this, &Used](float MinX, float MaxX, float MinY, float MaxY)
	{
		for (int32 Attempt = 0; Attempt < 30; ++Attempt)
		{
			const FVector2D C(Rng.FRandRange(MinX, MaxX), Rng.FRandRange(MinY, MaxY));
			const FBox2D B(C - FVector2D(16.f), C + FVector2D(16.f));
			bool bFree = true;
			for (const FBox2D& U : Used) if (U.Intersect(B)) { bFree = false; break; }
			if (!bFree) continue;
			Used.Add(B);
			Compound(C.X, C.Y);
			return;
		}
	};
	for (int32 i = 0; i < 8; ++i) TryCompound(-680.f, 680.f, 470.f, 690.f);
	for (int32 i = 0; i < 8; ++i) TryCompound(-680.f, 680.f, -690.f, -470.f);
	for (int32 i = 0; i < 3; ++i) TryCompound(600.f, 690.f, -380.f, 380.f);
	for (int32 i = 0; i < 3; ++i) TryCompound(-690.f, -600.f, -380.f, 380.f);

	// Scattered trees around the villages.
	for (int32 i = 0; i < 70; ++i)
	{
		const float X = Rng.FRandRange(-700.f, 700.f);
		const float Y = Rng.FRand() < 0.5f ? Rng.FRandRange(440.f, 700.f) : Rng.FRandRange(-700.f, -440.f);
		bool bClear = true;
		for (const FBox2D& U : Used) if (U.IsInside(FVector2D(X, Y))) { bClear = false; break; }
		if (bClear) Tree(X, Y, Rng.FRandRange(0.8f, 1.4f));
	}

	Mountains();
	Boundary();
	ApplyColors();
}

void AJXBagramMap::BeginPlay()
{
	Super::BeginPlay();
	if (!Ground || Ground->GetInstanceCount() == 0)
	{
		Rebuild();
	}
	ApplyColors();

	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !HasAuthority()) return;

	if (bSpawnLoot)
	{
		FRandomStream LootRng(FMath::Rand());
		for (const FVector& P : LootPoints)
		{
			const FVector At = GetActorTransform().TransformPosition(P);
			const FJXItem Item = UJXWeaponLibrary::RandomLoot(LootRng);
			AJXPickup::SpawnPickup(World, Item, At);
			if (Item.Type == EJXItemType::Weapon)
			{
				if (const FJXWeaponDef* D = UJXWeaponLibrary::Find(Item.WeaponId))
				{
					FJXItem Ammo;
					Ammo.Type = EJXItemType::Ammo;
					Ammo.Ammo = D->Ammo;
					Ammo.Amount = UJXWeaponLibrary::AmmoPackSize(D->Ammo) * 2;
					AJXPickup::SpawnPickup(World, Ammo, At + FVector(70.f, 30.f, 0.f));
				}
			}
		}
	}

	if (bSpawnVehicles)
	{
		const UJXSettings* S = UJXSettings::Get();
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		UClass* TankClass = S->TankClass.LoadSynchronous();
		if (!TankClass) TankClass = AJXTank::StaticClass();
		for (const FTransform& T : TankSpots)
		{
			World->SpawnActor<AJXTank>(TankClass, T * GetActorTransform(), P);
		}

		UClass* GunClass = S->MountedGunClass.LoadSynchronous();
		if (!GunClass) GunClass = AJXMountedGun::StaticClass();
		for (const FTransform& T : GunSpots)
		{
			World->SpawnActor<AJXMountedGun>(GunClass, T * GetActorTransform(), P);
		}
	}
}
