// UNPAID: OVERTIME - interactable station (printer, coffee maker, filing cabinet, inbox tray, supply closet).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UOCoreTypes.h"
#include "UOInteractableStation.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;
class UBoxComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 *  A station employees interact with. Stations hold no task logic: the task manager decides what an
 *  interaction means. Two flavours:
 *   - Anchored (level-driven): an invisible actor at a level anchor (the spot where an employee stands,
 *     facing the furniture). A query-only box around the furniture's bounds catches the interaction trace,
 *     so looking at the real furniture mesh focuses the station.
 *   - Greybox (procedural fallback office): draws its own boxes and a floating label. Origin on the floor,
 *     forward vector facing into the room.
 */
UCLASS()
class AUOInteractableStation : public AActor
{
	GENERATED_BODY()

public:

	/** Custom depth stencil value used by the focus outline post-process */
	static constexpr int32 HighlightStencilValue = 1;

	AUOInteractableStation();

	/** Greybox station: sets identity and builds the look for the station type */
	void ConfigureStation(FName InStationId, EUOStationType InType, int32 InOrdinal);

	/** Level station: invisible, bound to a furniture actor placed in the level */
	void ConfigureFromAnchor(FName InStationId, EUOStationType InType, int32 InOrdinal, AActor* InFurniture, const FText& InRoomName);

	FName GetStationId() const { return StationId; }
	EUOStationType GetStationType() const { return StationType; }
	bool IsAnchored() const { return Furniture.IsValid(); }
	AActor* GetFurniture() const { return Furniture.Get(); }

	/** "Printer 3" (greybox) or "Printer · Copy room" (level) */
	virtual FText GetDisplayName() const;

	/** Terminals (AUOTerminal) are readable devices and sabotage targets, never task stations */
	virtual bool IsTerminal() const { return false; }

	/** Room id (UORoom) the station is in, NAME_None if unknown */
	FName GetRoomId() const { return RoomId; }
	void SetRoomId(FName InRoomId) { RoomId = InRoomId; }

	// ~ Sabotage state (server-decided; public: everyone sees a jammed or locked machine)

	bool IsJammed() const { return bJammed; }
	void SetJammed(bool bInJammed);

	/** Locked until this shift time (0 = not locked) */
	bool IsLocked(float ShiftNow) const { return ShiftNow < LockedUntil; }
	float GetLockedUntil() const { return LockedUntil; }
	void SetLockedUntil(float InShiftTime) { LockedUntil = InShiftTime; }

	/** New day: clears jams and locks */
	void ClearSabotage() { SetJammed(false); LockedUntil = 0.f; }

	/** Room the station is in (level stations only, empty otherwise) */
	const FText& GetRoomName() const { return RoomName; }

	/** Supply closets hand out ink cartridges, inbox trays hand out reports */
	bool IsItemSource() const { return GetProvidedItem() != EUOItemType::None; }
	EUOItemType GetProvidedItem() const;

	/** Floor point in front of the station where an employee stands to use it */
	FVector GetInteractionLocation() const;

	/** Point employees look at while working (centre of the furniture) */
	FVector GetFocusLocation() const;

	/** Visual feedback once every sub-objective at this station is done (greybox label) */
	void SetWorkDone(bool bInWorkDone);
	bool IsWorkDone() const { return bWorkDone; }

	/** Local presentation: outline the furniture (custom depth stencil) while the local player focuses it */
	void SetHighlighted(bool bInHighlighted);

protected:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> AccentMesh;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UTextRenderComponent> Label;

	/** Catches the interaction trace around level furniture (Visibility channel only, no physics, no navigation) */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UBoxComponent> InteractionBox;

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

	UPROPERTY(VisibleInstanceOnly, Category="Station")
	TWeakObjectPtr<AActor> Furniture;

	FText RoomName;

	FName RoomId;

	bool bJammed = false;
	float LockedUntil = 0.f;

	/** Small red light that blinks over a jammed printer */
	UPROPERTY(Transient)
	TObjectPtr<class UPointLightComponent> JamLight;

	/** Centre of the furniture bounds (level stations) */
	FVector FurnitureCenter = FVector::ZeroVector;

	bool bHighlighted = false;

	/** Depth of the body along the forward axis (cm) */
	float BodyDepth = 80.f;

	FLinearColor BodyColor = FLinearColor::Gray;

	void RefreshVisuals();
};
