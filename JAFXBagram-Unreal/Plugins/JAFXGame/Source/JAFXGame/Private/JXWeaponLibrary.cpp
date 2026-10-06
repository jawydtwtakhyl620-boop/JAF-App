#include "JXWeaponLibrary.h"

namespace
{
	FJXWeaponDef MakeWeapon(FName Id, const TCHAR* Name, EJXWeaponCategory Cat, EJXAmmoType Ammo,
		float Damage, float RPM, int32 Mag, float Reload, float Hip, float Ads, float RangeMeters,
		bool bAuto, float RecoilPitch, float RecoilYaw, float AdsFov, float LootWeight)
	{
		FJXWeaponDef D;
		D.Id = Id;
		D.DisplayName = Name;
		D.Category = Cat;
		D.Ammo = Ammo;
		D.Damage = Damage;
		D.RPM = RPM;
		D.MagSize = Mag;
		D.ReloadTime = Reload;
		D.HipSpread = Hip;
		D.AdsSpread = Ads;
		D.Range = RangeMeters * 100.f;
		D.bAutomatic = bAuto;
		D.RecoilPitch = RecoilPitch;
		D.RecoilYaw = RecoilYaw;
		D.AdsFov = AdsFov;
		D.LootWeight = LootWeight;
		return D;
	}

	TArray<FJXWeaponDef> BuildCatalogue()
	{
		using C = EJXWeaponCategory;
		using A = EJXAmmoType;
		TArray<FJXWeaponDef> W;
		//                   Id        Name       Category         Ammo         Dmg   RPM   Mag Reload Hip  Ads   Range Auto   RecP  RecY  Fov Loot
		W.Add(MakeWeapon("K47",  TEXT("K-47"),   C::AssaultRifle, A::A762,     48.f, 600.f, 30, 2.9f, 2.4f, 0.6f, 400.f, true,  0.90f, 0.35f, 60.f, 12.f));
		W.Add(MakeWeapon("M4A",  TEXT("M-4A"),   C::AssaultRifle, A::A556,     41.f, 700.f, 30, 2.1f, 2.0f, 0.4f, 400.f, true,  0.55f, 0.20f, 60.f, 12.f));
		W.Add(MakeWeapon("SCRL", TEXT("SCR-L"),  C::AssaultRifle, A::A556,     41.f, 625.f, 30, 2.4f, 2.0f, 0.45f,400.f, true,  0.50f, 0.20f, 60.f, 10.f));
		W.Add(MakeWeapon("G36",  TEXT("G-36"),   C::AssaultRifle, A::A556,     43.f, 650.f, 30, 2.5f, 2.2f, 0.5f, 400.f, true,  0.70f, 0.30f, 60.f, 9.f));
		W.Add(MakeWeapon("UZ9",  TEXT("UZ-9"),   C::SMG,          A::A9mm,     26.f, 950.f, 25, 1.8f, 2.0f, 1.0f, 100.f, true,  0.35f, 0.25f, 65.f, 10.f));
		W.Add(MakeWeapon("VK45", TEXT("VK-45"),  C::SMG,          A::A9mm,     31.f, 1100.f,19, 1.8f, 1.8f, 0.9f, 100.f, true,  0.30f, 0.20f, 65.f, 8.f));
		W.Add(MakeWeapon("UM45", TEXT("UM-45"),  C::SMG,          A::A45,      41.f, 670.f, 25, 2.2f, 1.8f, 0.8f, 150.f, true,  0.30f, 0.20f, 65.f, 10.f));
		W.Add(MakeWeapon("K98",  TEXT("K-98"),   C::Sniper,       A::A762,     79.f, 40.f,  5,  4.0f, 4.0f, 0.05f,800.f, false, 3.00f, 0.30f, 22.f, 6.f));
		W.Add(MakeWeapon("AWM",  TEXT("AW-M"),   C::Sniper,       A::A300,    105.f, 33.f,  5,  4.6f, 4.0f, 0.02f,1000.f,false, 3.50f, 0.30f, 18.f, 1.5f));
		W.Add(MakeWeapon("SKSD", TEXT("SKS-D"),  C::DMR,          A::A762,     53.f, 450.f, 10, 2.9f, 2.5f, 0.25f,800.f, false, 1.40f, 0.40f, 35.f, 7.f));
		W.Add(MakeWeapon("S12",  TEXT("S-12"),   C::Shotgun,      A::A12Gauge, 22.f, 240.f, 5,  3.0f, 4.5f, 3.5f, 50.f,  false, 1.80f, 0.50f, 70.f, 6.f));
		W.Add(MakeWeapon("S686", TEXT("S-686"),  C::Shotgun,      A::A12Gauge, 26.f, 300.f, 2,  2.6f, 4.0f, 3.0f, 50.f,  false, 2.20f, 0.50f, 70.f, 7.f));
		W.Add(MakeWeapon("PKM",  TEXT("PK-M"),   C::LMG,          A::A762,     44.f, 550.f, 75, 5.0f, 3.0f, 0.8f, 400.f, true,  0.80f, 0.40f, 60.f, 3.f));
		W.Add(MakeWeapon("P92",  TEXT("P-92"),   C::Pistol,       A::A9mm,     35.f, 400.f, 15, 2.0f, 2.0f, 1.0f, 100.f, false, 0.60f, 0.20f, 70.f, 12.f));
		W.Add(MakeWeapon("R45",  TEXT("R-45"),   C::Pistol,       A::A45,      55.f, 200.f, 6,  3.0f, 2.0f, 0.8f, 100.f, false, 1.20f, 0.30f, 70.f, 8.f));
		W.Add(MakeWeapon("RPG7J",TEXT("RPG-7J"), C::Launcher,     A::Rocket,    0.f, 60.f,  1,  4.0f, 2.0f, 0.5f, 600.f, false, 2.50f, 0.50f, 50.f, 2.f));

		for (FJXWeaponDef& D : W)
		{
			if (D.Category == C::Shotgun) { D.Pellets = 9; D.HeadMultiplier = 1.5f; }
			if (D.Category == C::Sniper) { D.HeadMultiplier = 2.5f; }
			if (D.Category == C::DMR) { D.HeadMultiplier = 2.35f; }
			if (D.Category == C::Launcher)
			{
				D.bProjectile = true;
				D.ProjectileSpeed = 7000.f;
				D.ExplosionDamage = 160.f;
				D.ExplosionRadius = 550.f;
			}
		}
		return W;
	}
}

const TArray<FJXWeaponDef>& UJXWeaponLibrary::GetAll()
{
	static const TArray<FJXWeaponDef> Catalogue = BuildCatalogue();
	return Catalogue;
}

const FJXWeaponDef* UJXWeaponLibrary::Find(FName Id)
{
	return GetAll().FindByPredicate([Id](const FJXWeaponDef& D) { return D.Id == Id; });
}

bool UJXWeaponLibrary::FindWeapon(FName Id, FJXWeaponDef& OutDef)
{
	if (const FJXWeaponDef* D = Find(Id))
	{
		OutDef = *D;
		return true;
	}
	return false;
}

FString UJXWeaponLibrary::AmmoName(EJXAmmoType Ammo)
{
	switch (Ammo)
	{
	case EJXAmmoType::A556: return TEXT("5.56mm");
	case EJXAmmoType::A762: return TEXT("7.62mm");
	case EJXAmmoType::A9mm: return TEXT("9mm");
	case EJXAmmoType::A45: return TEXT(".45 ACP");
	case EJXAmmoType::A12Gauge: return TEXT("12 Gauge");
	case EJXAmmoType::A300: return TEXT(".300 Magnum");
	case EJXAmmoType::Rocket: return TEXT("Rocket");
	default: return TEXT("-");
	}
}

int32 UJXWeaponLibrary::AmmoPackSize(EJXAmmoType Ammo)
{
	switch (Ammo)
	{
	case EJXAmmoType::A556:
	case EJXAmmoType::A762: return 30;
	case EJXAmmoType::A9mm:
	case EJXAmmoType::A45: return 30;
	case EJXAmmoType::A12Gauge: return 10;
	case EJXAmmoType::A300: return 5;
	case EJXAmmoType::Rocket: return 2;
	default: return 0;
	}
}

FName UJXWeaponLibrary::RandomWeaponId(FRandomStream& Rng)
{
	float Total = 0.f;
	for (const FJXWeaponDef& D : GetAll()) Total += D.LootWeight;
	float Pick = Rng.FRand() * Total;
	for (const FJXWeaponDef& D : GetAll())
	{
		Pick -= D.LootWeight;
		if (Pick <= 0.f) return D.Id;
	}
	return GetAll()[0].Id;
}

FJXItem UJXWeaponLibrary::RandomLoot(FRandomStream& Rng)
{
	FJXItem Item;
	const float R = Rng.FRand();
	if (R < 0.30f)
	{
		Item.Type = EJXItemType::Weapon;
		Item.WeaponId = RandomWeaponId(Rng);
		const FJXWeaponDef* D = Find(Item.WeaponId);
		Item.Amount = D ? D->MagSize : 0;
	}
	else if (R < 0.58f)
	{
		static const EJXAmmoType Types[] = { EJXAmmoType::A556, EJXAmmoType::A762, EJXAmmoType::A9mm,
			EJXAmmoType::A45, EJXAmmoType::A12Gauge, EJXAmmoType::A762, EJXAmmoType::A556 };
		Item.Type = EJXItemType::Ammo;
		Item.Ammo = Types[Rng.RandRange(0, UE_ARRAY_COUNT(Types) - 1)];
		Item.Amount = AmmoPackSize(Item.Ammo);
	}
	else if (R < 0.68f) { Item.Type = EJXItemType::Bandage; Item.Amount = 5; }
	else if (R < 0.73f) { Item.Type = EJXItemType::FirstAid; }
	else if (R < 0.75f) { Item.Type = EJXItemType::MedKit; }
	else if (R < 0.79f) { Item.Type = EJXItemType::EnergyDrink; }
	else if (R < 0.86f)
	{
		Item.Type = Rng.FRand() < 0.5f ? EJXItemType::Vest : EJXItemType::Helmet;
		const float L = Rng.FRand();
		Item.Level = L < 0.55f ? 1 : (L < 0.9f ? 2 : 3);
		Item.Durability = Item.Type == EJXItemType::Vest ? VestDurability(Item.Level) : HelmetDurability(Item.Level);
	}
	else if (R < 0.93f) { Item.Type = EJXItemType::FragGrenade; }
	else if (R < 0.97f) { Item.Type = EJXItemType::SmokeGrenade; }
	else { Item.Type = EJXItemType::FlashGrenade; }
	return Item;
}

FString UJXWeaponLibrary::ItemLabel(const FJXItem& Item)
{
	switch (Item.Type)
	{
	case EJXItemType::Weapon:
	{
		const FJXWeaponDef* D = Find(Item.WeaponId);
		return D ? D->DisplayName : Item.WeaponId.ToString();
	}
	case EJXItemType::Ammo: return FString::Printf(TEXT("%s x%d"), *AmmoName(Item.Ammo), Item.Amount);
	case EJXItemType::Bandage: return FString::Printf(TEXT("Bandage x%d"), Item.Amount);
	case EJXItemType::FirstAid: return TEXT("First Aid Kit");
	case EJXItemType::MedKit: return TEXT("Med Kit");
	case EJXItemType::EnergyDrink: return TEXT("Energy Drink");
	case EJXItemType::Vest: return FString::Printf(TEXT("Vest Lv.%d"), Item.Level);
	case EJXItemType::Helmet: return FString::Printf(TEXT("Helmet Lv.%d"), Item.Level);
	case EJXItemType::FragGrenade: return TEXT("Frag Grenade");
	case EJXItemType::SmokeGrenade: return TEXT("Smoke Grenade");
	case EJXItemType::FlashGrenade: return TEXT("Flash Grenade");
	default: return TEXT("?");
	}
}

float UJXWeaponLibrary::VestReduction(int32 Level) { return Level >= 3 ? 0.55f : Level == 2 ? 0.40f : Level == 1 ? 0.30f : 0.f; }
float UJXWeaponLibrary::HelmetReduction(int32 Level) { return Level >= 3 ? 0.55f : Level == 2 ? 0.40f : Level == 1 ? 0.30f : 0.f; }
float UJXWeaponLibrary::VestDurability(int32 Level) { return Level >= 3 ? 250.f : Level == 2 ? 220.f : 200.f; }
float UJXWeaponLibrary::HelmetDurability(int32 Level) { return Level >= 3 ? 230.f : Level == 2 ? 150.f : 80.f; }
