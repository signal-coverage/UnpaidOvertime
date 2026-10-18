// UNPAID: OVERTIME - server authority for one working day: setup, shift clock, overtime and evaluation.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UOMatchConfig.h"
#include "UODayClock.h"
#include "UOGameMode.generated.h"

class AUOGameState;
class AUOOfficeBuilder;
class AUOAIEmployee;
class UUOTaskManagerComponent;
class ADirectionalLight;
class ASkyLight;
class ASkyAtmosphere;
class ACameraActor;
class SUOMatchSetupWidget;
class SWidget;

/**
 *  Works on an empty map: spawns its own sky and lighting and a greybox office, shows the setup screen,
 *  then runs the day (tasks, AI coworkers, clock, overtime) and the end-of-day star evaluation.
 *  All rules live here and in the task manager; UI only reads the game state.
 */
UCLASS()
class AUOGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	AUOGameMode();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	/** Validates the configuration on the authority and schedules the day start. Called by the setup screen. */
	FUOConfigValidationResult RequestStartDay(const FUOMatchConfig& Config);

	/** Restarts the level back to the setup screen (only from the evaluation screen) */
	void RequestNewDay(APlayerController* Requester);

	UUOTaskManagerComponent* GetTaskManager() const { return TaskManager; }
	AUOOfficeBuilder* GetOfficeBuilder() const { return OfficeBuilder; }
	const FUOMatchConfig& GetMatchConfig() const { return MatchConfig; }

protected:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UUOTaskManagerComponent> TaskManager;

	UPROPERTY(Transient)
	TObjectPtr<AUOOfficeBuilder> OfficeBuilder;

	UPROPERTY(Transient)
	TObjectPtr<ADirectionalLight> SunLight;

	UPROPERTY(Transient)
	TObjectPtr<ASkyLight> SkyLight;

	UPROPERTY(Transient)
	TObjectPtr<ASkyAtmosphere> SkyAtmosphere;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> OverviewCamera;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AUOAIEmployee>> AIEmployees;

	/** Office-style names for AI coworkers (shuffled per seed) */
	UPROPERTY(EditDefaultsOnly, Category="Employees")
	TArray<FText> AINames;

	UPROPERTY(EditDefaultsOnly, Category="Employees")
	TArray<FLinearColor> AIColors;

	FUOMatchConfig MatchConfig;
	FUODayClock DayClock;
	int32 ActiveSeed = 1;
	int32 DayNumber = 1;
	bool bStartDayPending = false;

	FRotator SunDayRotation = FRotator(-50.f, 35.f, 0.f);
	float SunBaseIntensity = 6.f;
	float SkyBaseIntensity = 1.f;

	TSharedPtr<SUOMatchSetupWidget> SetupWidget;
	TSharedPtr<SWidget> SetupWidgetContainer;

	AUOGameState* GetUOGameState() const;

	void SpawnEnvironment();
	void ShowSetupScreen();
	void HideSetupScreen();

	void StartDay();
	void SpawnHumanEmployees(int32& InOutNextEmployeeId);
	void SpawnAIEmployees(int32 Count, int32& InOutNextEmployeeId);
	void TickDay(float DeltaSeconds);
	void ApplyOvertimeEffects(bool bOvertime);
	void FinishDay();
};
