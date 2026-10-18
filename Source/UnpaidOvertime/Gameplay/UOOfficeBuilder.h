// UNPAID: OVERTIME - seed-driven greybox office (floor, walls, stations, decoration, sign and lights).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UOTaskTypes.h"
#include "UOOfficeBuilder.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;
class UMaterialInterface;
class AUOInteractableStation;

/**
 *  Builds a 30 x 20 m open-plan office around the world origin. Stations go into slots along the walls
 *  (preferring their zone's wall), the supply closet sits in the south-west corner, and decoration is only
 *  accepted where it does not block the straight line from the central hub to any station, so employees
 *  can always walk hub -> station without a navmesh.
 */
UCLASS()
class AUOOfficeBuilder : public AActor
{
	GENERATED_BODY()

public:

	AUOOfficeBuilder();

	/** Rebuilds the whole office. Returns the spawned stations. */
	TArray<AUOInteractableStation*> BuildOffice(int32 Seed, const FUOStationRequirements& Requirements, const FString& CompanyName);

	/** Removes every generated component and station */
	void ClearOffice();

	/** Tints the office lights for overtime */
	void SetOvertimeMood(bool bOvertime);

	/** Central, always-clear point used as a waypoint between stations */
	FVector GetHubLocation() const;

	FTransform GetPlayerStartTransform() const;
	TArray<FVector> GetAISpawnLocations() const;
	FTransform GetOverviewCameraTransform() const;

	const TArray<TObjectPtr<AUOInteractableStation>>& GetStations() const { return Stations; }

protected:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> GeneratedComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPointLightComponent>> OfficeLights;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AUOInteractableStation>> Stations;

	UStaticMeshComponent* AddBox(const FVector& Center, const FVector& Size, const FLinearColor& Color, float Yaw = 0.f, bool bCollision = true);
	UTextRenderComponent* AddText(const FVector& Location, float Yaw, const FText& Text, float WorldSize, const FColor& Color);
	void AddOfficeLight(const FVector& Location);

	void BuildShell();
	void BuildSign(const FString& CompanyName);
	void PlaceStations(int32 Seed, const FUOStationRequirements& Requirements);
	void BuildDecoration(int32 Seed);
};
