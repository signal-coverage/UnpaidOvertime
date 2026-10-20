// UNPAID: OVERTIME - in-world terminals and sabotage-only targets.

#include "UOTerminal.h"
#include "UOInvestigation.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "UOTerminal"

AUOTerminal::AUOTerminal()
{
}

void AUOTerminal::ConfigureTerminal(FName InTerminalId, EUOTerminalKind InKind, AActor* InFurniture, FName InRoomId, const FText& InRoomName, int32 InOrdinal)
{
	TerminalKind = InKind;
	// Station type is irrelevant for terminals; the anchored setup builds the interaction box around the furniture
	ConfigureFromAnchor(InTerminalId, EUOStationType::FilingCabinet, InOrdinal, InFurniture, InRoomName);
	SetRoomId(InRoomId);

	if (TerminalKind == EUOTerminalKind::SecurityCamera)
	{
		StatusLight = NewObject<UPointLightComponent>(this, TEXT("CameraStatusLight"));
		StatusLight->SetupAttachment(RootComponent);
		StatusLight->RegisterComponent();
		StatusLight->SetMobility(EComponentMobility::Movable);
		StatusLight->SetWorldLocation(GetFocusLocation() + FVector(0.f, 0.f, -8.f));
		StatusLight->SetLightColor(FLinearColor(1.f, 0.05f, 0.05f));
		StatusLight->SetIntensityUnits(ELightUnits::Candelas);
		StatusLight->SetIntensity(1.5f);
		StatusLight->SetAttenuationRadius(60.f);
		StatusLight->SetCastShadows(false);
	}
}

FText AUOTerminal::GetDisplayName() const
{
	const FText Name = FUOTraceText::GetTerminalName(TerminalKind);
	return RoomName.IsEmpty() ? Name : FText::Format(LOCTEXT("TerminalRoom", "{0} · {1}"), Name, RoomName);
}

bool AUOTerminal::GetDevice(EUOTraceDevice& OutDevice) const
{
	return FUOTraceText::GetTerminalDevice(TerminalKind, OutDevice);
}

bool AUOTerminal::IsReadable() const
{
	EUOTraceDevice Device;
	return GetDevice(Device);
}

void AUOTerminal::SetOfflineUntil(float InShiftTime)
{
	OfflineUntil = InShiftTime;
	SetStatusLight(false);
}

void AUOTerminal::SetStatusLight(bool bOn)
{
	if (StatusLight)
	{
		StatusLight->SetVisibility(bOn);
	}
}

void AUOTerminal::MovePropTo(const FTransform& Destination)
{
	AActor* Prop = GetFurniture();
	if (!Prop || bPropMoved)
	{
		return;
	}
	bPropMoved = true;

	const UStaticMeshComponent* Source = Prop->FindComponentByClass<UStaticMeshComponent>();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	MovedCopy = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Destination, Params);
	if (MovedCopy && Source)
	{
		UStaticMeshComponent* Copy = MovedCopy->GetStaticMeshComponent();
		Copy->SetMobility(EComponentMobility::Movable);
		Copy->SetStaticMesh(Source->GetStaticMesh());
		for (int32 Slot = 0; Slot < Source->GetNumMaterials(); ++Slot)
		{
			Copy->SetMaterial(Slot, Source->GetMaterial(Slot));
		}
		// Props never block employees where the killer dumps them
		Copy->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Copy->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Copy->SetCanEverAffectNavigation(false);
	}
	Prop->SetActorHiddenInGame(true);
	Prop->SetActorEnableCollision(false);
	SetActorEnableCollision(false);
}

void AUOTerminal::RestoreProp()
{
	if (!bPropMoved)
	{
		return;
	}
	bPropMoved = false;
	if (MovedCopy)
	{
		MovedCopy->Destroy();
		MovedCopy = nullptr;
	}
	if (AActor* Prop = GetFurniture())
	{
		Prop->SetActorHiddenInGame(false);
		Prop->SetActorEnableCollision(true);
	}
	SetActorEnableCollision(true);
}

void AUOTerminal::ResetForDay()
{
	ClearSabotage();
	OfflineUntil = 0.f;
	RestoreProp();
	if (StatusLight)
	{
		StatusLight->SetVisibility(true);
	}
}

#undef LOCTEXT_NAMESPACE
