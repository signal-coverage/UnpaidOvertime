// UNPAID: OVERTIME - greybox interactable station (printer, coffee maker, filing cabinet, inbox tray, supply closet).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UOCoreTypes.h"
#include "UOInteractableStation.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 *  A station employees interact with. Its type decides the greybox look and whether it hands out items.
 *  Stations hold no task logic: the task manager decides what an interaction means.
 *  The actor's forward vector faces into the room; its origin sits on the floor.
 */
UCLASS()
class AUOInteractableStation : public AActor
{
	GENERATED_BODY()

public:

	AUOInteractableStation();

	/** Sets identity and builds the look for the station type */
	void ConfigureStation(FName InStationId, EUOStationType InType, int32 InOrdinal);

	FName GetStationId() const { return StationId; }
	EUOStationType GetStationType() const { return StationType; }

	/** "Printer 3" */
	FText GetDisplayName() const;

	/** Supply closets hand out ink cartridges, inbox trays hand out reports */
	bool IsItemSource() const { return GetProvidedItem() != EUOItemType::None; }
	EUOItemType GetProvidedItem() const;

	/** Floor point in front of the station where an employee stands to use it */
	FVector GetInteractionLocation() const;

	/** Visual feedback once every sub-objective at this station is done */
	void SetWorkDone(bool bInWorkDone);
	bool IsWorkDone() const { return bWorkDone; }

protected:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> AccentMesh;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> AccentMaterial;

	UPROPERTY(VisibleInstanceOnly, Category="Station")
	FName StationId;

	UPROPERTY(VisibleInstanceOnly, Category="Station")
	EUOStationType StationType = EUOStationType::Printer;

	UPROPERTY(VisibleInstanceOnly, Category="Station")
	int32 Ordinal = 1;

	UPROPERTY(VisibleInstanceOnly, Category="Station")
	bool bWorkDone = false;

	/** Depth of the body along the forward axis (cm) */
	float BodyDepth = 80.f;

	FLinearColor BodyColor = FLinearColor::Gray;

	void RefreshVisuals();
};
