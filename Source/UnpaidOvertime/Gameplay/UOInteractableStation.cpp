// UNPAID: OVERTIME - interactable station.

#include "UOInteractableStation.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "UOInteractableStation"

namespace UOStationPrivate
{
	/** Greybox description of a station type: body and accent boxes (cm, X = depth, Y = width, Z = height) */
	struct FStationLook
	{
		FVector BodySize;
		FLinearColor BodyColor;
		FVector AccentSize;
		FVector AccentOffset;
		FLinearColor AccentColor;
	};

	FStationLook GetLook(EUOStationType Type)
	{
		switch (Type)
		{
		case EUOStationType::Printer:
			return { FVector(80.f, 110.f, 105.f), FLinearColor(0.72f, 0.72f, 0.76f), FVector(50.f, 80.f, 10.f), FVector(0.f, 0.f, 110.f), FLinearColor(0.08f, 0.08f, 0.1f) };
		case EUOStationType::CoffeeMaker:
			return { FVector(70.f, 90.f, 95.f), FLinearColor(0.45f, 0.28f, 0.16f), FVector(40.f, 40.f, 50.f), FVector(0.f, 0.f, 120.f), FLinearColor(0.35f, 0.04f, 0.04f) };
		case EUOStationType::FilingCabinet:
			return { FVector(70.f, 60.f, 140.f), FLinearColor(0.36f, 0.42f, 0.36f), FVector(4.f, 30.f, 110.f), FVector(36.f, 0.f, 70.f), FLinearColor(0.85f, 0.85f, 0.85f) };
		case EUOStationType::InboxTray:
			return { FVector(70.f, 100.f, 80.f), FLinearColor(0.55f, 0.5f, 0.45f), FVector(40.f, 50.f, 12.f), FVector(0.f, 0.f, 86.f), FLinearColor(0.95f, 0.95f, 0.9f) };
		case EUOStationType::SupplyCloset:
		default:
			return { FVector(70.f, 180.f, 210.f), FLinearColor(0.6f, 0.33f, 0.12f), FVector(4.f, 160.f, 12.f), FVector(36.f, 0.f, 120.f), FLinearColor(0.15f, 0.15f, 0.15f) };
		}
	}

	const FLinearColor DoneColor(0.15f, 0.65f, 0.22f);
	const FName ColorParameter(TEXT("Color"));

	/** Margin added around furniture bounds so the box is hit before the mesh itself */
	constexpr float InteractionBoxMargin = 4.f;

	/** Small furniture (desk printers, inbox trays) gets a minimum box so it is easy to aim at */
	constexpr float MinInteractionHalfExtent = 18.f;
}

AUOInteractableStation::AUOInteractableStation()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BaseMaterial = MaterialFinder.Object;

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Root);
	BodyMesh->SetMobility(EComponentMobility::Movable);
	BodyMesh->SetStaticMesh(CubeFinder.Object);
	BodyMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	AccentMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AccentMesh"));
	AccentMesh->SetupAttachment(Root);
	AccentMesh->SetMobility(EComponentMobility::Movable);
	AccentMesh->SetStaticMesh(CubeFinder.Object);
	AccentMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetMobility(EComponentMobility::Movable);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextBottom);
	Label->SetWorldSize(16.f);
	Label->SetTextRenderColor(FColor::White);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
	InteractionBox->SetupAttachment(Root);
	InteractionBox->SetMobility(EComponentMobility::Movable);
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionBox->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionBox->SetCanEverAffectNavigation(false);
	InteractionBox->SetGenerateOverlapEvents(false);
	InteractionBox->SetHiddenInGame(true);
	InteractionBox->SetUsingAbsoluteRotation(true);
}

void AUOInteractableStation::ConfigureStation(FName InStationId, EUOStationType InType, int32 InOrdinal)
{
	StationId = InStationId;
	StationType = InType;
	Ordinal = InOrdinal;
	bWorkDone = false;
	Furniture.Reset();

	const UOStationPrivate::FStationLook Look = UOStationPrivate::GetLook(StationType);
	BodyDepth = Look.BodySize.X;
	BodyColor = Look.BodyColor;

	// The engine cube is 100 cm and centered on its pivot
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, Look.BodySize.Z * 0.5f));
	BodyMesh->SetRelativeScale3D(Look.BodySize / 100.f);
	AccentMesh->SetRelativeLocation(Look.AccentOffset);
	AccentMesh->SetRelativeScale3D(Look.AccentSize / 100.f);

	if (BaseMaterial)
	{
		BodyMaterial = BodyMesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, BaseMaterial);
		AccentMaterial = AccentMesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, BaseMaterial);
		if (AccentMaterial)
		{
			AccentMaterial->SetVectorParameterValue(UOStationPrivate::ColorParameter, Look.AccentColor);
		}
	}

	const float LabelHeight = FMath::Max(Look.BodySize.Z, Look.AccentOffset.Z + Look.AccentSize.Z * 0.5f) + 12.f;
	Label->SetRelativeLocation(FVector(BodyDepth * 0.5f + 4.f, 0.f, LabelHeight));

	RefreshVisuals();
}

void AUOInteractableStation::ConfigureFromAnchor(FName InStationId, EUOStationType InType, int32 InOrdinal, AActor* InFurniture, const FText& InRoomName)
{
	using namespace UOStationPrivate;

	StationId = InStationId;
	StationType = InType;
	Ordinal = InOrdinal;
	RoomName = InRoomName;
	bWorkDone = false;
	Furniture = InFurniture;

	// The level furniture is the look: no greybox body or floating label
	for (UPrimitiveComponent* Greybox : { static_cast<UPrimitiveComponent*>(BodyMesh), static_cast<UPrimitiveComponent*>(AccentMesh), static_cast<UPrimitiveComponent*>(Label) })
	{
		Greybox->SetVisibility(false);
		Greybox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Greybox->SetCanEverAffectNavigation(false);
	}

	FVector Origin = GetActorLocation() + GetActorForwardVector() * 60.f + FVector(0.f, 0.f, 80.f);
	FVector Extent(30.f, 30.f, 40.f);
	if (InFurniture)
	{
		InFurniture->GetActorBounds(false, Origin, Extent, false);
	}
	FurnitureCenter = Origin;

	Extent += FVector(InteractionBoxMargin);
	Extent = Extent.ComponentMax(FVector(MinInteractionHalfExtent));
	InteractionBox->SetWorldLocation(Origin);
	InteractionBox->SetWorldRotation(FRotator::ZeroRotator);
	InteractionBox->SetBoxExtent(Extent, false);
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	// Distance from the stand point to the furniture front, used by the greybox-style helpers
	BodyDepth = 0.f;
}

void AUOInteractableStation::SetJammed(bool bInJammed)
{
	if (bJammed == bInJammed)
	{
		return;
	}
	bJammed = bInJammed;
	if (bJammed && !JamLight)
	{
		JamLight = NewObject<UPointLightComponent>(this, TEXT("JamLight"));
		JamLight->SetupAttachment(Root);
		JamLight->RegisterComponent();
		JamLight->SetMobility(EComponentMobility::Movable);
		JamLight->SetLightColor(FLinearColor(1.f, 0.08f, 0.05f));
		JamLight->SetAttenuationRadius(160.f);
		JamLight->SetIntensityUnits(ELightUnits::Candelas);
		JamLight->SetIntensity(6.f);
		JamLight->SetCastShadows(false);
	}
	if (JamLight)
	{
		JamLight->SetWorldLocation(GetFocusLocation() + FVector(0.f, 0.f, 45.f));
		JamLight->SetVisibility(bJammed);
	}
	RefreshVisuals();
}

FText AUOInteractableStation::GetDisplayName() const
{
	const FText TypeName = UOText::GetStationTypeName(StationType);
	if (!RoomName.IsEmpty())
	{
		return FText::Format(LOCTEXT("StationRoomFormat", "{0} · {1}"), TypeName, RoomName);
	}
	if (StationType == EUOStationType::SupplyCloset)
	{
		return TypeName;
	}
	return FText::Format(LOCTEXT("StationNameFormat", "{0} {1}"), TypeName, FText::AsNumber(Ordinal));
}

EUOItemType AUOInteractableStation::GetProvidedItem() const
{
	switch (StationType)
	{
	case EUOStationType::SupplyCloset:	return EUOItemType::InkCartridge;
	case EUOStationType::InboxTray:		return EUOItemType::Report;
	default:							return EUOItemType::None;
	}
}

FVector AUOInteractableStation::GetInteractionLocation() const
{
	if (Furniture.IsValid())
	{
		// The anchor already is the stand point
		return GetActorLocation();
	}
	return GetActorLocation() + GetActorForwardVector() * (BodyDepth * 0.5f + 75.f);
}

FVector AUOInteractableStation::GetFocusLocation() const
{
	if (Furniture.IsValid())
	{
		return FurnitureCenter;
	}
	return GetActorLocation() + FVector(0.f, 0.f, 90.f);
}

void AUOInteractableStation::SetWorkDone(bool bInWorkDone)
{
	if (bWorkDone != bInWorkDone)
	{
		bWorkDone = bInWorkDone;
		RefreshVisuals();
	}
}

void AUOInteractableStation::SetHighlighted(bool bInHighlighted)
{
	if (bHighlighted == bInHighlighted)
	{
		return;
	}
	bHighlighted = bInHighlighted;

	TArray<UPrimitiveComponent*> Primitives;
	if (AActor* FurnitureActor = Furniture.Get())
	{
		FurnitureActor->GetComponents<UPrimitiveComponent>(Primitives);
	}
	else
	{
		Primitives = { BodyMesh, AccentMesh };
	}

	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive && Primitive->IsVisible())
		{
			Primitive->SetCustomDepthStencilValue(HighlightStencilValue);
			Primitive->SetRenderCustomDepth(bHighlighted);
		}
	}
}

void AUOInteractableStation::RefreshVisuals()
{
	if (Furniture.IsValid())
	{
		return;
	}

	if (BodyMaterial)
	{
		BodyMaterial->SetVectorParameterValue(UOStationPrivate::ColorParameter, bWorkDone ? UOStationPrivate::DoneColor : BodyColor);
	}

	const FText Name = GetDisplayName();
	if (bJammed)
	{
		Label->SetText(FText::Format(LOCTEXT("StationJamLabel", "{0}\nJAMMED"), Name));
		Label->SetTextRenderColor(FColor(255, 90, 80));
		return;
	}
	Label->SetText(bWorkDone ?FText::Format(LOCTEXT("StationDoneLabel", "{0}\nDONE"), Name) : Name);
	Label->SetTextRenderColor(bWorkDone ? FColor(120, 255, 140) : FColor::White);
}

#undef LOCTEXT_NAMESPACE
