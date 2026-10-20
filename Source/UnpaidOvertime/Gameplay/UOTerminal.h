// UNPAID: OVERTIME - in-world terminals (device logs) and sabotage-only targets (cameras, movable props).

#pragma once

#include "CoreMinimal.h"
#include "UOInteractableStation.h"
#include "UOKillerTypes.h"
#include "UOTerminal.generated.h"

class AStaticMeshActor;

/**
 *  A readable device (print server, desk PC, badge reader, security monitor, inventory sheet, reception PC) or a
 *  sabotage-only target (ceiling camera, movable prop). Shares the station focus/outline/prompt machinery; the task
 *  manager treats it as "read the log" and the role system offers the killer's sabotage on it.
 *  Spawned by the game mode at level anchors tagged UOTerminal.<Kind> (+ UOFurniture.<Actor>, UORoom.<Room>).
 */
UCLASS()
class AUOTerminal : public AUOInteractableStation
{
	GENERATED_BODY()

public:

	AUOTerminal();

	void ConfigureTerminal(FName InTerminalId, EUOTerminalKind InKind, AActor* InFurniture, FName InRoomId, const FText& InRoomName, int32 InOrdinal);

	virtual bool IsTerminal() const override { return true; }
	virtual FText GetDisplayName() const override;

	EUOTerminalKind GetTerminalKind() const { return TerminalKind; }

	/** Device shown when read (false for cameras and props) */
	bool GetDevice(EUOTraceDevice& OutDevice) const;
	bool IsReadable() const;

	// ~ Camera

	bool IsCameraOffline(float ShiftNow) const { return ShiftNow < OfflineUntil; }
	float GetOfflineUntil() const { return OfflineUntil; }
	void SetOfflineUntil(float InShiftTime);
	/** Camera status light (off while the camera is cut) */
	void SetStatusLight(bool bOn);

	// ~ Movable prop

	bool IsPropMoved() const { return bPropMoved; }
	/** Hides the prop here and shows a copy at Destination (the original actor is static) */
	void MovePropTo(const FTransform& Destination);
	/** Overnight cleanup: the prop is back where it belongs */
	void RestoreProp();

	/** New day: camera back online, prop back in place */
	void ResetForDay();

protected:

	UPROPERTY(VisibleInstanceOnly, Category="Terminal")
	EUOTerminalKind TerminalKind = EUOTerminalKind::DeskComputer;

	float OfflineUntil = 0.f;
	bool bPropMoved = false;

	UPROPERTY(Transient)
	TObjectPtr<AStaticMeshActor> MovedCopy;

	/** Red "REC" light under a camera, off while the camera is offline */
	UPROPERTY(Transient)
	TObjectPtr<class UPointLightComponent> StatusLight;
};
