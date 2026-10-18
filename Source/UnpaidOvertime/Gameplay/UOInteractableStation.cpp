// UNPAID: OVERTIME - greybox interactable station.

#include "UOInteractableStation.h"
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
}

void AUOInteractableStation::ConfigureStation(FName InStationId, EUOStationType InType, int32 InOrdinal)
{
	StationId = InStationId;
	StationType = InType;
	Ordinal = InOrdinal;
	bWorkDone = false;

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

FText AUOInteractableStation::GetDisplayName() const
{
	if (StationType == EUOStationType::SupplyCloset)
	{
		return UOText::GetStationTypeName(StationType);
	}
	return FText::Format(LOCTEXT("StationNameFormat", "{0} {1}"), UOText::GetStationTypeName(StationType), FText::AsNumber(Ordinal));
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
	return GetActorLocation() + GetActorForwardVector() * (BodyDepth * 0.5f + 75.f);
}

void AUOInteractableStation::SetWorkDone(bool bInWorkDone)
{
	if (bWorkDone != bInWorkDone)
	{
		bWorkDone = bInWorkDone;
		RefreshVisuals();
	}
}

void AUOInteractableStation::RefreshVisuals()
{
	if (BodyMaterial)
	{
		BodyMaterial->SetVectorParameterValue(UOStationPrivate::ColorParameter, bWorkDone ? UOStationPrivate::DoneColor : BodyColor);
	}

	const FText Name = GetDisplayName();
	Label->SetText(bWorkDone ? FText::Format(LOCTEXT("StationDoneLabel", "{0}\nDONE"), Name) : Name);
	Label->SetTextRenderColor(bWorkDone ? FColor(120, 255, 140) : FColor::White);
}

#undef LOCTEXT_NAMESPACE
