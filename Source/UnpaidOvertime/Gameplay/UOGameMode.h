// UNPAID: OVERTIME - server authority for one working day: world setup, shift clock, overtime and evaluation.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UOMatchConfig.h"
#include "UODayClock.h"
#include "UOTaskTypes.h"
#include "UOEvaluationTypes.h"
#include "UOKillerTypes.h"
#include "UOGameMode.generated.h"

class AUOGameState;
class AUOOfficeBuilder;
class AUOAIEmployee;
class AUOInteractableStation;
class UUOTaskManagerComponent;
class UUOBossSystemComponent;
class UUOEvaluationDirector;
class UUORoleSystemComponent;
class AUOTerminal;
class AUOBodyActor;
class AUOPlayerState;
class ADirectionalLight;
class ASkyLight;
class ASkyAtmosphere;
class ATextRenderActor;
class APostProcessVolume;
class ULightComponent;
class UMaterialParameterCollection;

/** A pair of camera keys the main menu camera drifts between */
USTRUCT()
struct FUOMenuCameraShot
{
	GENERATED_BODY()

	UPROPERTY()
	FTransform From;

	UPROPERTY()
	FTransform To;
};

/**
 *  Level-driven when the loaded map has station anchors (TargetPoints tagged UOStation.<Type>, plus
 *  UOFurniture.<ActorName>): stations are spawned at the anchors and the level's own lighting is used.
 *  On an empty map it falls back to the procedural greybox office with its own sun and sky.
 *  Menus belong to the player controller; all rules live here and in the task manager.
 */
UCLASS()
class AUOGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	AUOGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	/** Validates the configuration on the authority and schedules the day start. Called by the setup screen. */
	FUOConfigValidationResult RequestStartDay(const FUOMatchConfig& Config);

	/** Back to the main menu (reloads the level). Allowed from the evaluation screen or the pause menu. */
	void RequestReturnToMenu(APlayerController* Requester);

	/**
	 *  The one entry point for evaluation choices from the UI (single player calls it directly; M7 wraps it in a
	 *  server RPC). Returns false when the choice is not what the evaluation is waiting for.
	 */
	bool SubmitEvaluationChoice(APlayerController* Requester, const FUOEvalChoice& Choice);

	/** Evaluation director: the day is over, start the next one (same match, no level reload) */
	void RequestNextDay();

	/** Evaluation director: a new match with the same configuration (new seed unless the config fixes one) */
	void RequestNewHire();

	/** Evaluation director: the match-over screen's "Main menu" */
	void RequestReturnToMenuFromEvaluation();

	/** Evaluation director: an employee left the match (frees their work, hides an AI's body until the next day) */
	void OnEmployeeEliminated(AUOPlayerState* Employee);

	/** Pins or unpins a trace as evidence for the requester (single player calls it directly; M7 wraps it in an RPC) */
	bool SubmitPinToggle(APlayerController* Requester, int32 TraceId);

	// ~ The killer (M4)

	/**
	 *  A coworker was dismissed (optional kill): body (lying on a clear patch of floor), letter, incident trace,
	 *  maybe a public "last seen near" note, emergency meeting
	 */
	void HandleDismissal(AUOPlayerState* Killer, AUOPlayerState* Victim, EUOTraceDetail Detail, bool bLastSeenNote);

	/** Dev automation (non-Shipping switches): force the killer count of the next match (0 = rule 1) */
	void SetDevKillerCount(int32 Count) { DevKillerCount = Count; }

	/** The emergency meeting is over: the shift resumes where it stopped */
	void ResumeAfterMeeting();

	/** Cut camera sabotage: the room's lights go (almost) dark, or come back */
	void SetRoomBlackout(FName Room, bool bBlackout);

	/** Move-object sabotage: a floor spot in another room for the prop */
	bool FindPropDestination(const AUOTerminal* Prop, FTransform& OutDestination, FName& OutRoom) const;

	const TArray<TObjectPtr<AUOTerminal>>& GetTerminals() const { return Terminals; }
	const TArray<TObjectPtr<AUOInteractableStation>>& GetLevelStations() const { return LevelStations; }
	UUORoleSystemComponent* GetRoleSystem() const { return RoleSystem; }
	const TArray<TObjectPtr<AUOBodyActor>>& GetBodies() const { return Bodies; }

	UUOTaskManagerComponent* GetTaskManager() const { return TaskManager; }
	UUOBossSystemComponent* GetBossSystem() const { return BossSystem; }
	UUOEvaluationDirector* GetEvaluationDirector() const { return EvaluationDirector; }
	int32 GetDayNumber() const { return DayNumber; }
	const FUOMatchConfig& GetMatchConfig() const { return MatchConfig; }

	/** True when the world comes from the level's anchors instead of the procedural office */
	bool IsLevelDriven() const { return bLevelDriven; }

	/** Camera moves for the main menu background (level keys, or a slow push over the greybox office) */
	TArray<FUOMenuCameraShot> GetMenuCameraShots() const;

protected:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UUOTaskManagerComponent> TaskManager;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UUOBossSystemComponent> BossSystem;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UUOEvaluationDirector> EvaluationDirector;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UUORoleSystemComponent> RoleSystem;

	/** Terminals and sabotage targets spawned at UOTerminal.<Kind> anchors */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AUOTerminal>> Terminals;

	/** Dismissed employees found today (removed overnight) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AUOBodyActor>> Bodies;

	TSet<FName> BlackoutRooms;
	int32 DevKillerCount = 0;

	/** A clear patch of floor near Near for a body lying along Yaw (away from walls and furniture) */
	bool FindBodySpot(const FVector& Near, float PreferredYaw, FVector& OutFloor, float& OutYaw) const;
	/** Who was in Room during the last Seconds (true positions, for the public note), victim excluded */
	TArray<int32> FindRecentlyNear(FName Room, float Seconds, int32 ExcludeId) const;
	EUODayPhase PhaseBeforeMeeting = EUODayPhase::Shift;

	UPROPERTY(Transient)
	TObjectPtr<AUOOfficeBuilder> OfficeBuilder;

	UPROPERTY(Transient)
	TObjectPtr<ADirectionalLight> SunLight;

	UPROPERTY(Transient)
	TObjectPtr<ASkyLight> SkyLight;

	UPROPERTY(Transient)
	TObjectPtr<ASkyAtmosphere> SkyAtmosphere;

	/** Bodies of the AI employees still in the match */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AUOAIEmployee>> AIEmployees;

	/** Every AI controller of the match (they own the player states, also of fired employees) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AController>> AIControllers;

	/** Stations spawned at the level anchors (level-driven mode) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AUOInteractableStation>> LevelStations;

	UPROPERTY(Transient)
	TObjectPtr<ATextRenderActor> CompanySign;

	UPROPERTY(Transient)
	TObjectPtr<APostProcessVolume> LevelPostProcess;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> OfficeLightingCollection;

	/** Office-style names for AI coworkers (shuffled per seed) */
	UPROPERTY(EditDefaultsOnly, Category="Employees")
	TArray<FText> AINames;

	UPROPERTY(EditDefaultsOnly, Category="Employees")
	TArray<FLinearColor> AIColors;

	/** Level light state captured at startup so overtime dimming is reversible */
	struct FLightSnapshot
	{
		TWeakObjectPtr<ULightComponent> Light;
		float Intensity = 0.f;
		FLinearColor Color = FLinearColor::White;
		FRotator Rotation = FRotator::ZeroRotator;
		bool bDirectional = false;
		bool bSky = false;
		/** Room the light is in (cut camera sabotage darkens one room) */
		FName Room;
	};
	TArray<FLightSnapshot> LevelLights;
	float LevelExposureBias = 0.f;

	FUOMatchConfig MatchConfig;
	FUODayClock DayClock;
	int32 ActiveSeed = 1;
	/** Seed of the current day (derived from the match seed and the day number) */
	int32 DaySeed = 1;
	int32 DayNumber = 1;
	bool bStartDayPending = false;
	bool bNextDayPending = false;
	bool bLevelDriven = false;

	FTransform PlayerStartTransform = FTransform::Identity;
	TArray<FTransform> AISpawnTransforms;
	FVector NavigationHub = FVector::ZeroVector;
	TArray<FUOMenuCameraShot> LevelMenuShots;

	FRotator SunDayRotation = FRotator(-50.f, 35.f, 0.f);
	float SunBaseIntensity = 6.f;
	float SkyBaseIntensity = 1.f;

	AUOGameState* GetUOGameState() const;

	/** Spawns stations at the level anchors; returns false when the level has none */
	bool SetupLevelWorld();
	/** Terminals at UOTerminal.<Kind> anchors, or (older levels) next to known furniture */
	void SetupTerminals(const TArray<class ATargetPoint*>& Anchors, const TMap<FName, AActor*>& ActorsByName);
	void SpawnTerminal(EUOTerminalKind Kind, const FTransform& Stand, AActor* Furniture, int32 Ordinal);
	void SetupProceduralWorld();
	void SpawnProceduralEnvironment();
	void CaptureLevelLighting();
	void SpawnCompanySign(const FTransform& Anchor);
	void SetCompanySignText(const FString& Text);
	void LogNavigationStatus(const TCHAR* Context) const;

	/** Starts a day: day 1 of a new match (fresh roster) or the next day of the running match (survivors only) */
	void StartDay(bool bNewMatch);
	void NameTasksAfterRooms(TArray<FUOTaskInstance>& Tasks) const;
	/** Puts the humans at the lobby; bInitRecords = new match (fresh employee records) */
	void SpawnHumanEmployees(int32& InOutNextEmployeeId, bool bInitRecords);
	void SpawnAIEmployees(int32 Count, int32& InOutNextEmployeeId);
	/** Next day: fired AIs are removed for good, survivors go back to the lobby with today's difficulty */
	void ResetAIEmployeesForNewDay();
	void DestroyAIEmployees();
	void TickDay(float DeltaSeconds);
	void ApplyOvertimeEffects(bool bOvertime);
	void FinishDay();
};
