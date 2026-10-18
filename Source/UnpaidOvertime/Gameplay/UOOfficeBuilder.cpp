// UNPAID: OVERTIME - seed-driven greybox office.

#include "UOOfficeBuilder.h"
#include "UOInteractableStation.h"
#include "UOTaskGenerator.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/RandomStream.h"
#include "UObject/ConstructorHelpers.h"
#include "UnpaidOvertime.h"

#define LOCTEXT_NAMESPACE "UOOfficeBuilder"

namespace UOOfficeLayout
{
	constexpr float HalfX = 1500.f;
	constexpr float HalfY = 1000.f;
	constexpr float WallHeight = 300.f;
	constexpr float WallThickness = 20.f;

	/** Distance from the wall's inner face to a station's origin */
	constexpr float StationWallOffset = 55.f;

	/** Clearance kept around hub-to-station walking lines (capsule radius + margin) */
	constexpr float CorridorMargin = 70.f;

	/** Decoration keeps this distance from any station's standing point */
	constexpr float StationClearance = 200.f;

	/** Decoration stays out of this radius around the hub */
	constexpr float HubClearRadius = 380.f;

	constexpr int32 MaxDecorations = 14;
	constexpr int32 MaxDecorationAttempts = 160;

	const FLinearColor FloorColor(0.16f, 0.18f, 0.23f);
	const FLinearColor WallColor(0.74f, 0.71f, 0.64f);
	const FLinearColor TrimColor(0.3f, 0.3f, 0.33f);
	const FLinearColor SignBoardColor(0.05f, 0.08f, 0.2f);
	const FLinearColor ClosetFloorColor(0.3f, 0.22f, 0.12f);
	const FLinearColor LightNormalColor(1.f, 0.95f, 0.85f);
	const FLinearColor LightOvertimeColor(1.f, 0.22f, 0.16f);
	constexpr float LightIntensityLumens = 3000.f;
	constexpr float LightOvertimeScale = 0.45f;

	const FName ColorParameter(TEXT("Color"));

	/** Wall index for a zone: 0 North (+Y), 1 South (-Y), 2 East (+X), 3 West (-X) */
	int32 GetWallForZone(int32 ZoneIndex)
	{
		return ZoneIndex >= 0 ? ZoneIndex % 4 : 0;
	}

	/** Every wall slot, grouped by wall. Station origins sit on the floor and face into the room. */
	void BuildWallSlots(TArray<FTransform> OutSlots[4])
	{
		const float NorthY = HalfY - StationWallOffset;
		const float EastX = HalfX - StationWallOffset;

		for (int32 Index = 0; Index < 11; ++Index)
		{
			const float X = -1150.f + 230.f * Index;
			OutSlots[0].Add(FTransform(FRotator(0.f, -90.f, 0.f), FVector(X, NorthY, 0.f)));
			OutSlots[1].Add(FTransform(FRotator(0.f, 90.f, 0.f), FVector(X, -NorthY, 0.f)));
		}
		for (int32 Index = 0; Index < 7; ++Index)
		{
			const float Y = -690.f + 230.f * Index;
			OutSlots[2].Add(FTransform(FRotator(0.f, 180.f, 0.f), FVector(EastX, Y, 0.f)));
			OutSlots[3].Add(FTransform(FRotator(0.f, 0.f, 0.f), FVector(-EastX, Y, 0.f)));
		}
	}

	FTransform GetSupplyClosetTransform()
	{
		return FTransform(FRotator(0.f, 45.f, 0.f), FVector(-HalfX + 130.f, -HalfY + 130.f, 0.f));
	}

	/** Liang-Barsky segment vs axis-aligned box (2D) */
	bool SegmentIntersectsBox2D(const FVector2D& A, const FVector2D& B, const FVector2D& Min, const FVector2D& Max)
	{
		const FVector2D Delta = B - A;
		double T0 = 0.0;
		double T1 = 1.0;

		auto Clip = [&T0, &T1](double P, double Q) -> bool
		{
			if (FMath::IsNearlyZero(P))
			{
				return Q >= 0.0;
			}
			const double R = Q / P;
			if (P < 0.0)
			{
				if (R > T1) { return false; }
				T0 = FMath::Max(T0, R);
			}
			else
			{
				if (R < T0) { return false; }
				T1 = FMath::Min(T1, R);
			}
			return true;
		};

		return Clip(-Delta.X, A.X - Min.X) && Clip(Delta.X, Max.X - A.X) && Clip(-Delta.Y, A.Y - Min.Y) && Clip(Delta.Y, Max.Y - A.Y);
	}

	double PointToBoxDistance2D(const FVector2D& Point, const FVector2D& Min, const FVector2D& Max)
	{
		const double DX = FMath::Max3(Min.X - Point.X, 0.0, Point.X - Max.X);
		const double DY = FMath::Max3(Min.Y - Point.Y, 0.0, Point.Y - Max.Y);
		return FMath::Sqrt(DX * DX + DY * DY);
	}

	bool BoxesOverlap2D(const FVector2D& MinA, const FVector2D& MaxA, const FVector2D& MinB, const FVector2D& MaxB)
	{
		return MinA.X < MaxB.X && MaxA.X > MinB.X && MinA.Y < MaxB.Y && MaxA.Y > MinB.Y;
	}

	/** A decoration piece made of one or two boxes */
	struct FDecorationPiece
	{
		FVector BaseSize;
		FLinearColor BaseColor;
		FVector TopSize;
		FLinearColor TopColor;
		FVector TopOffset;
	};

	FDecorationPiece GetDecoration(int32 Kind)
	{
		switch (Kind)
		{
		case 0:		// Desk with a monitor
			return { FVector(160.f, 80.f, 75.f), FLinearColor(0.6f, 0.47f, 0.3f), FVector(10.f, 55.f, 35.f), FLinearColor(0.03f, 0.03f, 0.04f), FVector(-20.f, 0.f, 92.f) };
		case 1:		// Cubicle partition
			return { FVector(200.f, 8.f, 140.f), FLinearColor(0.3f, 0.36f, 0.46f), FVector::ZeroVector, FLinearColor::Black, FVector::ZeroVector };
		case 2:		// Potted plant
			return { FVector(40.f, 40.f, 40.f), FLinearColor(0.55f, 0.25f, 0.12f), FVector(60.f, 60.f, 70.f), FLinearColor(0.1f, 0.45f, 0.12f), FVector(0.f, 0.f, 75.f) };
		default:	// Water cooler
			return { FVector(35.f, 35.f, 120.f), FLinearColor(0.6f, 0.8f, 0.95f), FVector::ZeroVector, FLinearColor::Black, FVector::ZeroVector };
		}
	}
}

AUOOfficeBuilder::AUOOfficeBuilder()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = CubeFinder.Object;
	BaseMaterial = MaterialFinder.Object;
}

TArray<AUOInteractableStation*> AUOOfficeBuilder::BuildOffice(int32 Seed, const FUOStationRequirements& Requirements, const FString& CompanyName)
{
	ClearOffice();
	SetActorLocation(FVector::ZeroVector);

	BuildShell();
	BuildSign(CompanyName);
	PlaceStations(Seed, Requirements);
	BuildDecoration(Seed);

	TArray<AUOInteractableStation*> Result;
	for (AUOInteractableStation* Station : Stations)
	{
		Result.Add(Station);
	}
	return Result;
}

void AUOOfficeBuilder::ClearOffice()
{
	for (USceneComponent* Component : GeneratedComponents)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	GeneratedComponents.Reset();
	OfficeLights.Reset();

	for (AUOInteractableStation* Station : Stations)
	{
		if (Station)
		{
			Station->Destroy();
		}
	}
	Stations.Reset();
}

void AUOOfficeBuilder::SetOvertimeMood(bool bOvertime)
{
	for (UPointLightComponent* Light : OfficeLights)
	{
		if (Light)
		{
			Light->SetLightColor(bOvertime ? UOOfficeLayout::LightOvertimeColor : UOOfficeLayout::LightNormalColor);
			Light->SetIntensity(UOOfficeLayout::LightIntensityLumens * (bOvertime ? UOOfficeLayout::LightOvertimeScale : 1.f));
		}
	}
}

FVector AUOOfficeBuilder::GetHubLocation() const
{
	return FVector::ZeroVector;
}

FTransform AUOOfficeBuilder::GetPlayerStartTransform() const
{
	// Facing north, where the company sign hangs
	return FTransform(FRotator(0.f, 90.f, 0.f), FVector(0.f, -150.f, 100.f));
}

TArray<FVector> AUOOfficeBuilder::GetAISpawnLocations() const
{
	return { FVector(-220.f, 60.f, 100.f), FVector(220.f, 60.f, 100.f), FVector(0.f, 240.f, 100.f), FVector(-160.f, -260.f, 100.f), FVector(160.f, -260.f, 100.f) };
}

FTransform AUOOfficeBuilder::GetOverviewCameraTransform() const
{
	const FVector Location(0.f, -2100.f, 1700.f);
	const FRotator Rotation = (FVector(0.f, 0.f, 0.f) - Location).Rotation();
	return FTransform(Rotation, Location);
}

UStaticMeshComponent* AUOOfficeBuilder::AddBox(const FVector& Center, const FVector& Size, const FLinearColor& Color, float Yaw, bool bCollision)
{
	UStaticMeshComponent* Box = NewObject<UStaticMeshComponent>(this);
	Box->SetMobility(EComponentMobility::Movable);
	Box->SetStaticMesh(CubeMesh);
	Box->SetupAttachment(Root);
	Box->SetRelativeLocation(Center);
	Box->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
	Box->SetRelativeScale3D(Size / 100.f);
	Box->SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	Box->RegisterComponent();

	if (BaseMaterial)
	{
		if (UMaterialInstanceDynamic* Material = Box->CreateAndSetMaterialInstanceDynamicFromMaterial(0, BaseMaterial))
		{
			Material->SetVectorParameterValue(UOOfficeLayout::ColorParameter, Color);
		}
	}

	GeneratedComponents.Add(Box);
	return Box;
}

UTextRenderComponent* AUOOfficeBuilder::AddText(const FVector& Location, float Yaw, const FText& Text, float WorldSize, const FColor& Color)
{
	UTextRenderComponent* TextComponent = NewObject<UTextRenderComponent>(this);
	TextComponent->SetMobility(EComponentMobility::Movable);
	TextComponent->SetupAttachment(Root);
	TextComponent->SetRelativeLocation(Location);
	TextComponent->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
	TextComponent->SetHorizontalAlignment(EHTA_Center);
	TextComponent->SetVerticalAlignment(EVRTA_TextCenter);
	TextComponent->SetWorldSize(WorldSize);
	TextComponent->SetTextRenderColor(Color);
	TextComponent->SetText(Text);
	TextComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TextComponent->RegisterComponent();

	GeneratedComponents.Add(TextComponent);
	return TextComponent;
}

void AUOOfficeBuilder::AddOfficeLight(const FVector& Location)
{
	UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(Location);
	Light->SetIntensityUnits(ELightUnits::Lumens);
	Light->SetIntensity(UOOfficeLayout::LightIntensityLumens);
	Light->SetAttenuationRadius(1400.f);
	Light->SetLightColor(UOOfficeLayout::LightNormalColor);
	Light->SetCastShadows(false);
	Light->RegisterComponent();

	GeneratedComponents.Add(Light);
	OfficeLights.Add(Light);
}

void AUOOfficeBuilder::BuildShell()
{
	using namespace UOOfficeLayout;

	const float FullX = HalfX * 2.f + WallThickness * 2.f;
	const float FullY = HalfY * 2.f + WallThickness * 2.f;

	// Floor (top surface at Z = 0)
	AddBox(FVector(0.f, 0.f, -10.f), FVector(FullX, FullY, 20.f), FloorColor);

	// Outer walls
	AddBox(FVector(0.f, HalfY + WallThickness * 0.5f, WallHeight * 0.5f), FVector(FullX, WallThickness, WallHeight), WallColor);
	AddBox(FVector(0.f, -HalfY - WallThickness * 0.5f, WallHeight * 0.5f), FVector(FullX, WallThickness, WallHeight), WallColor);
	AddBox(FVector(HalfX + WallThickness * 0.5f, 0.f, WallHeight * 0.5f), FVector(WallThickness, FullY, WallHeight), WallColor);
	AddBox(FVector(-HalfX - WallThickness * 0.5f, 0.f, WallHeight * 0.5f), FVector(WallThickness, FullY, WallHeight), WallColor);

	// Skirting trim, purely visual
	AddBox(FVector(0.f, HalfY - 2.f, 6.f), FVector(HalfX * 2.f, 4.f, 12.f), TrimColor, 0.f, false);
	AddBox(FVector(0.f, -HalfY + 2.f, 6.f), FVector(HalfX * 2.f, 4.f, 12.f), TrimColor, 0.f, false);
	AddBox(FVector(HalfX - 2.f, 0.f, 6.f), FVector(4.f, HalfY * 2.f, 12.f), TrimColor, 0.f, false);
	AddBox(FVector(-HalfX + 2.f, 0.f, 6.f), FVector(4.f, HalfY * 2.f, 12.f), TrimColor, 0.f, false);

	// Supply corner floor patch
	const FTransform Closet = GetSupplyClosetTransform();
	AddBox(Closet.GetLocation() + FVector(0.f, 0.f, 0.5f), FVector(260.f, 260.f, 1.f), ClosetFloorColor, 45.f, false);

	// Office lights (open ceiling, lights hang just below the wall top)
	for (const float X : { -1000.f, 0.f, 1000.f })
	{
		for (const float Y : { -500.f, 500.f })
		{
			AddOfficeLight(FVector(X, Y, WallHeight - 20.f));
		}
	}
}

void AUOOfficeBuilder::BuildSign(const FString& CompanyName)
{
	using namespace UOOfficeLayout;

	const float SignZ = 245.f;
	AddBox(FVector(0.f, HalfY - 4.f, SignZ), FVector(1000.f, 6.f, 90.f), SignBoardColor, 0.f, false);

	const FText Name = CompanyName.IsEmpty() ? LOCTEXT("SignPlaceholder", "YOUR COMPANY HERE") : FText::FromString(CompanyName.ToUpper());
	AddText(FVector(0.f, HalfY - 9.f, SignZ + 8.f), -90.f, Name, 55.f, FColor(255, 214, 90));
	AddText(FVector(0.f, HalfY - 9.f, SignZ - 30.f), -90.f, LOCTEXT("SignTagline", "Where every hour counts. Especially the unpaid ones."), 14.f, FColor(220, 220, 230));
}

void AUOOfficeBuilder::PlaceStations(int32 Seed, const FUOStationRequirements& Requirements)
{
	using namespace UOOfficeLayout;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FTransform> WallSlots[4];
	BuildWallSlots(WallSlots);

	// Seeded shuffle of each wall's slots
	FRandomStream Rng(Seed * 31 + 7);
	for (TArray<FTransform>& Slots : WallSlots)
	{
		for (int32 Index = Slots.Num() - 1; Index > 0; --Index)
		{
			Slots.Swap(Index, Rng.RandRange(0, Index));
		}
	}
	int32 NextSlot[4] = { 0, 0, 0, 0 };

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (const FUOStationSpec& Spec : Requirements.Stations)
	{
		FTransform SlotTransform;
		if (Spec.Type == EUOStationType::SupplyCloset)
		{
			SlotTransform = GetSupplyClosetTransform();
		}
		else
		{
			int32 Wall = GetWallForZone(Spec.ZoneIndex);
			if (NextSlot[Wall] >= WallSlots[Wall].Num())
			{
				// Preferred wall is full: use the wall with the most free slots
				int32 BestFree = 0;
				for (int32 Candidate = 0; Candidate < 4; ++Candidate)
				{
					const int32 Free = WallSlots[Candidate].Num() - NextSlot[Candidate];
					if (Free > BestFree)
					{
						BestFree = Free;
						Wall = Candidate;
					}
				}
				if (BestFree <= 0)
				{
					UE_LOG(LogUnpaidOvertime, Error, TEXT("Office builder: no free slot for station %s"), *Spec.StationId.ToString());
					continue;
				}
			}
			SlotTransform = WallSlots[Wall][NextSlot[Wall]++];
		}

		AUOInteractableStation* Station = World->SpawnActor<AUOInteractableStation>(AUOInteractableStation::StaticClass(), SlotTransform, SpawnParams);
		if (Station)
		{
			Station->ConfigureStation(Spec.StationId, Spec.Type, Spec.Ordinal);
			Stations.Add(Station);
		}
	}
}

void AUOOfficeBuilder::BuildDecoration(int32 Seed)
{
	using namespace UOOfficeLayout;

	// Walking lines that must stay clear: hub -> every station standing point, hub -> every AI spawn
	const FVector2D Hub(GetHubLocation());
	TArray<FVector2D> LineEnds;
	TArray<FVector2D> StandingPoints;
	for (const AUOInteractableStation* Station : Stations)
	{
		if (Station)
		{
			const FVector2D Point(Station->GetInteractionLocation());
			LineEnds.Add(Point);
			StandingPoints.Add(Point);
		}
	}
	for (const FVector& Spawn : GetAISpawnLocations())
	{
		LineEnds.Add(FVector2D(Spawn));
	}
	LineEnds.Add(FVector2D(GetPlayerStartTransform().GetLocation()));

	FRandomStream Rng(Seed * 131 + 17);
	TArray<TPair<FVector2D, FVector2D>> PlacedBoxes;

	for (int32 Attempt = 0; Attempt < MaxDecorationAttempts && PlacedBoxes.Num() < MaxDecorations; ++Attempt)
	{
		const int32 Kind = Rng.RandRange(0, 4) % 4;
		const bool bRotated = Rng.FRand() < 0.5f;
		const FVector2D Center(Rng.FRandRange(-1050.f, 1050.f), Rng.FRandRange(-620.f, 620.f));
		if (Center.Size() < HubClearRadius)
		{
			continue;
		}

		const FDecorationPiece Piece = GetDecoration(Kind);
		const FVector2D Half = bRotated ? FVector2D(Piece.BaseSize.Y, Piece.BaseSize.X) * 0.5 : FVector2D(Piece.BaseSize.X, Piece.BaseSize.Y) * 0.5;
		const FVector2D Min = Center - Half;
		const FVector2D Max = Center + Half;
		const FVector2D Margin(CorridorMargin, CorridorMargin);

		bool bBlocked = false;
		for (const FVector2D& End : LineEnds)
		{
			if (SegmentIntersectsBox2D(Hub, End, Min - Margin, Max + Margin))
			{
				bBlocked = true;
				break;
			}
		}
		for (int32 Index = 0; !bBlocked && Index < StandingPoints.Num(); ++Index)
		{
			bBlocked = PointToBoxDistance2D(StandingPoints[Index], Min, Max) < StationClearance;
		}
		for (int32 Index = 0; !bBlocked && Index < PlacedBoxes.Num(); ++Index)
		{
			bBlocked = BoxesOverlap2D(Min - Margin, Max + Margin, PlacedBoxes[Index].Key, PlacedBoxes[Index].Value);
		}
		if (bBlocked)
		{
			continue;
		}

		PlacedBoxes.Add(TPair<FVector2D, FVector2D>(Min, Max));

		const float Yaw = bRotated ? 90.f : 0.f;
		const FVector BaseCenter(Center.X, Center.Y, Piece.BaseSize.Z * 0.5f);
		AddBox(BaseCenter, Piece.BaseSize, Piece.BaseColor, Yaw);
		if (!Piece.TopSize.IsNearlyZero())
		{
			const FVector TopOffset = FRotator(0.f, Yaw, 0.f).RotateVector(FVector(Piece.TopOffset.X, Piece.TopOffset.Y, 0.f));
			AddBox(FVector(Center.X + TopOffset.X, Center.Y + TopOffset.Y, Piece.TopOffset.Z), Piece.TopSize, Piece.TopColor, Yaw);
		}
	}
}

#undef LOCTEXT_NAMESPACE
