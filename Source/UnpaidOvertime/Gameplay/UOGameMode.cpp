// UNPAID: OVERTIME - server authority for one working day.

#include "UOGameMode.h"
#include "UOGameState.h"
#include "UOPlayerState.h"
#include "UOPlayerController.h"
#include "UOCharacter.h"
#include "UOHUD.h"
#include "UOOfficeBuilder.h"
#include "UOInteractableStation.h"
#include "UOTaskManagerComponent.h"
#include "UOTaskGenerator.h"
#include "UOTaskBoard.h"
#include "UOStarRating.h"
#include "UOAIDifficulty.h"
#include "UOAIEmployee.h"
#include "UOAIController.h"
#include "UOBossSystemComponent.h"
#include "UOEvaluationDirector.h"
#include "UOBossQuestions.h"
#include "UODayCalendar.h"
#include "UODayDifficulty.h"
#include "UORoleSystemComponent.h"
#include "UOEvidenceSubsystem.h"
#include "UOTerminal.h"
#include "UOBodyActor.h"
#include "UOAudioSubsystem.h"
#include "UOAudioRules.h"
#include "UOKillerRules.h"
#include "UOInvestigation.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/TargetPoint.h"
#include "Engine/TextRenderActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Math/RandomStream.h"
#include "Misc/DateTime.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "UnpaidOvertime.h"

#define LOCTEXT_NAMESPACE "UOGameMode"

namespace UOGameModePrivate
{
	const FLinearColor HumanColor(0.3f, 0.75f, 1.f);

	// Procedural office overtime look
	const FLinearColor SunOvertimeColor(1.f, 0.32f, 0.22f);
	const FLinearColor SkyOvertimeColor(1.f, 0.4f, 0.35f);
	const FRotator SunOvertimeRotation(-7.f, 35.f, 0.f);
	constexpr float SunOvertimeScale = 0.2f;
	constexpr float SkyOvertimeScale = 0.3f;

	// Level overtime look: half the panels "off", cold light, sunset outside, darker exposure
	constexpr float LocalLightOvertimeScale = 0.3f;
	const FLinearColor LocalLightOvertimeColor(0.8f, 0.9f, 1.f);
	constexpr float SunLevelOvertimeScale = 0.12f;
	const FLinearColor SunLevelOvertimeColor(1.f, 0.42f, 0.22f);
	constexpr float SunsetPitch = -5.f;
	constexpr float SkyLevelOvertimeScale = 0.25f;
	constexpr float OvertimeLightLevel = 0.3f;
	constexpr float OvertimeExposureOffset = -1.f;
	constexpr float OvertimeBlendSeconds = 4.f;

	const FName StationTagPrefix(TEXT("UOStation."));
	const FString TerminalTagPrefix(TEXT("UOTerminal."));
	const FString FurnitureTagPrefix(TEXT("UOFurniture."));
	const FName AISpawnTag(TEXT("UOSpawn.AI"));
	const FName CompanySignTag(TEXT("UOSign.Company"));
	const FName MenuCameraTag(TEXT("UOCamera.Menu"));
	const FString OrderTagPrefix(TEXT("UOOrder."));
	const TCHAR* OfficeLightingCollectionPath = TEXT("/Game/UO/Materials/MPC_UO_Office.MPC_UO_Office");
	const FName LightLevelParameter(TEXT("LightLevel"));
	const TCHAR* SignTextMaterialPath = TEXT("/Game/UO/Materials/M_UO_SignText.M_UO_SignText");

	/** Sign board text area (Lvl_Office SM_CompanySignBoard is 240 x 80 cm with a red strip at the bottom) */
	constexpr float SignTextMaxWidth = 205.f;
	constexpr float SignTextMaxSize = 34.f;

	bool ParseStationType(const FString& Name, EUOStationType& OutType)
	{
		static const TPair<const TCHAR*, EUOStationType> Types[] = {
			{ TEXT("Printer"), EUOStationType::Printer },
			{ TEXT("CoffeeMaker"), EUOStationType::CoffeeMaker },
			{ TEXT("FilingCabinet"), EUOStationType::FilingCabinet },
			{ TEXT("InboxTray"), EUOStationType::InboxTray },
			{ TEXT("SupplyCloset"), EUOStationType::SupplyCloset }
		};
		for (const TPair<const TCHAR*, EUOStationType>& Entry : Types)
		{
			if (Name.Equals(Entry.Key, ESearchCase::IgnoreCase))
			{
				OutType = Entry.Value;
				return true;
			}
		}
		return false;
	}

	const FString RoomTagPrefix(TEXT("UORoom."));

	/**
	 *  Room id of a station: the furniture's UORoom.<Id> tag (placed by uo_level.py), else a guess from the
	 *  furniture actor name ("Printer_Floor_CopyRoom_01" -> CopyRoom) for levels built before room tags existed.
	 */
	FName GetRoomId(const AActor* Furniture, const FString& FallbackName)
	{
		if (Furniture)
		{
			for (const FName& Tag : Furniture->Tags)
			{
				const FString TagString = Tag.ToString();
				if (TagString.StartsWith(RoomTagPrefix))
				{
					return FName(*TagString.RightChop(RoomTagPrefix.Len()));
				}
			}
		}

		static const TPair<const TCHAR*, const TCHAR*> Tokens[] = {
			{ TEXT("Reception"), TEXT("Lobby") },
			{ TEXT("OpenPlan"), TEXT("OpenPlan") },
			{ TEXT("CornerA"), TEXT("OpenPlan") },
			{ TEXT("CopyRoom"), TEXT("CopyRoom") },
			{ TEXT("Manager"), TEXT("Manager") },
			{ TEXT("Records"), TEXT("Records") },
			{ TEXT("CornerB"), TEXT("Records") },
			{ TEXT("Kitchen"), TEXT("Kitchen") },
			{ TEXT("Main"), TEXT("Supply") }
		};
		const FString Name = Furniture ? Furniture->GetName() : FallbackName;
		for (const TPair<const TCHAR*, const TCHAR*>& Token : Tokens)
		{
			if (Name.Contains(Token.Key))
			{
				return FName(Token.Value);
			}
		}
		return NAME_None;
	}

	int32 GetOrderTag(const AActor* Actor)
	{
		for (const FName& Tag : Actor->Tags)
		{
			const FString TagString = Tag.ToString();
			if (TagString.StartsWith(OrderTagPrefix))
			{
				return FCString::Atoi(*TagString.RightChop(OrderTagPrefix.Len()));
			}
		}
		return 0;
	}

	FTransform FloorTransform(const AActor* Actor)
	{
		return FTransform(FRotator(0.f, Actor->GetActorRotation().Yaw, 0.f), Actor->GetActorLocation());
	}
}

AUOGameMode::AUOGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	DefaultPawnClass = AUOCharacter::StaticClass();
	HUDClass = AUOHUD::StaticClass();
	GameStateClass = AUOGameState::StaticClass();
	PlayerStateClass = AUOPlayerState::StaticClass();
	PlayerControllerClass = AUOPlayerController::StaticClass();

	TaskManager = CreateDefaultSubobject<UUOTaskManagerComponent>(TEXT("TaskManager"));
	BossSystem = CreateDefaultSubobject<UUOBossSystemComponent>(TEXT("BossSystem"));
	EvaluationDirector = CreateDefaultSubobject<UUOEvaluationDirector>(TEXT("EvaluationDirector"));
	RoleSystem = CreateDefaultSubobject<UUORoleSystemComponent>(TEXT("RoleSystem"));

	AINames = {
		LOCTEXT("AIName0", "Brenda from Accounts"),
		LOCTEXT("AIName1", "Kevin from IT"),
		LOCTEXT("AIName2", "Linda from HR"),
		LOCTEXT("AIName3", "Doug from Sales"),
		LOCTEXT("AIName4", "Patricia from Legal"),
		LOCTEXT("AIName5", "Stan from Payroll"),
		LOCTEXT("AIName6", "Rita from Reception"),
		LOCTEXT("AIName7", "Marv from Facilities")
	};

	AIColors = {
		FLinearColor(0.85f, 0.18f, 0.16f),
		FLinearColor(0.2f, 0.62f, 0.3f),
		FLinearColor(0.55f, 0.28f, 0.75f),
		FLinearColor(0.95f, 0.55f, 0.1f),
		FLinearColor(0.12f, 0.6f, 0.65f),
		FLinearColor(0.9f, 0.78f, 0.2f),
		FLinearColor(0.25f, 0.4f, 0.85f)
	};
}

AUOGameState* AUOGameMode::GetUOGameState() const
{
	return GetGameState<AUOGameState>();
}

void AUOGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Before the day starts nobody gets a pawn: the player controller shows the menus over a camera fly-through
	const AUOGameState* UOGameState = GetUOGameState();
	if (!UOGameState || UOGameState->GetPhase() == EUODayPhase::PreGame)
	{
		return;
	}
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

void AUOGameMode::BeginPlay()
{
	Super::BeginPlay();

	bLevelDriven = SetupLevelWorld();
	if (!bLevelDriven)
	{
		SetupProceduralWorld();
	}

	if (AUOGameState* UOGameState = GetUOGameState())
	{
		UOGameState->SetPhase(EUODayPhase::PreGame);
	}

	LogNavigationStatus(TEXT("startup"));
}

bool AUOGameMode::SetupLevelWorld()
{
	using namespace UOGameModePrivate;

	UWorld* World = GetWorld();

	// Index every actor by name so anchors can find their furniture
	TMap<FName, AActor*> ActorsByName;
	TArray<ATargetPoint*> StationAnchors;
	TArray<ATargetPoint*> TerminalAnchors;
	TArray<ATargetPoint*> CameraKeys;
	const ATargetPoint* SignAnchor = nullptr;
	AISpawnTransforms.Reset();

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		ActorsByName.Add(Actor->GetFName(), Actor);

		ATargetPoint* Point = Cast<ATargetPoint>(Actor);
		if (!Point)
		{
			continue;
		}
		for (const FName& Tag : Point->Tags)
		{
			const FString TagString = Tag.ToString();
			if (TagString.StartsWith(StationTagPrefix.ToString()))
			{
				StationAnchors.Add(Point);
				break;
			}
			if (TagString.StartsWith(TerminalTagPrefix))
			{
				TerminalAnchors.Add(Point);
				break;
			}
			if (Tag == AISpawnTag)
			{
				AISpawnTransforms.Add(FloorTransform(Point));
				break;
			}
			if (Tag == CompanySignTag)
			{
				SignAnchor = Point;
				break;
			}
			if (Tag == MenuCameraTag)
			{
				CameraKeys.Add(Point);
				break;
			}
		}
	}

	if (StationAnchors.Num() == 0)
	{
		return false;
	}

	// Stable ordinals per type: anchors sorted by name
	StationAnchors.Sort([](const ATargetPoint& A, const ATargetPoint& B) { return A.GetFName().LexicalLess(B.GetFName()); });

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	FUOStationSlots Counts;
	for (const ATargetPoint* Anchor : StationAnchors)
	{
		EUOStationType Type = EUOStationType::Printer;
		bool bHasType = false;
		AActor* Furniture = nullptr;
		for (const FName& Tag : Anchor->Tags)
		{
			const FString TagString = Tag.ToString();
			if (TagString.StartsWith(StationTagPrefix.ToString()))
			{
				bHasType = ParseStationType(TagString.RightChop(StationTagPrefix.GetStringLength()), Type);
			}
			else if (TagString.StartsWith(FurnitureTagPrefix))
			{
				if (AActor** Found = ActorsByName.Find(FName(*TagString.RightChop(FurnitureTagPrefix.Len()))))
				{
					Furniture = *Found;
				}
			}
		}

		if (!bHasType)
		{
			UE_LOG(LogUnpaidOvertime, Warning, TEXT("Level anchor %s has an unknown station type, skipped."), *Anchor->GetName());
			continue;
		}
		if (!Furniture)
		{
			UE_LOG(LogUnpaidOvertime, Warning, TEXT("Level anchor %s has no furniture actor; the station will use a default interaction box."), *Anchor->GetName());
		}

		Counts.Add(Type);
		const int32 Ordinal = Counts.Get(Type);
		AUOInteractableStation* Station = World->SpawnActor<AUOInteractableStation>(AUOInteractableStation::StaticClass(), FloorTransform(Anchor), SpawnParams);
		if (!Station)
		{
			continue;
		}
		const FName StationId = FUOTaskGenerator::MakeStationId(Type, Ordinal);
		const FName RoomId = GetRoomId(Furniture, Anchor->GetName());
		Station->ConfigureFromAnchor(StationId, Type, Ordinal, Furniture, RoomId.IsNone() ? LOCTEXT("RoomOffice", "Office") : FUORooms::GetDisplayName(RoomId));
		Station->SetRoomId(RoomId);
		BossSystem->RegisterStationRoom(StationId, RoomId);
		LevelStations.Add(Station);
	}

	UE_LOG(LogUnpaidOvertime, Log, TEXT("Level-driven office: %d stations (printers %d, coffee makers %d, filing cabinets %d, inbox trays %d, supply closets %d), %d AI spawns."),
		LevelStations.Num(), Counts.Printers, Counts.CoffeeMakers, Counts.FilingCabinets, Counts.InboxTrays, Counts.SupplyClosets, AISpawnTransforms.Num());

	// Player start: the lobby one if named, else the first
	PlayerStartTransform = FTransform::Identity;
	bool bFoundStart = false;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		if (!bFoundStart || It->GetName().Contains(TEXT("Lobby")))
		{
			PlayerStartTransform = FloorTransform(*It);
			bFoundStart = true;
		}
	}

	// Steering fallback hub: centre of the AI spawns (an open, walkable spot), else the player start
	NavigationHub = PlayerStartTransform.GetLocation();
	if (AISpawnTransforms.Num() > 0)
	{
		FVector Sum = FVector::ZeroVector;
		for (const FTransform& Spawn : AISpawnTransforms)
		{
			Sum += Spawn.GetLocation();
		}
		NavigationHub = Sum / AISpawnTransforms.Num();
	}
	if (AISpawnTransforms.Num() == 0)
	{
		AISpawnTransforms.Add(PlayerStartTransform);
	}

	// Menu camera: consecutive keys (by UOOrder.N) form shots
	CameraKeys.Sort([](const ATargetPoint& A, const ATargetPoint& B) { return GetOrderTag(&A) < GetOrderTag(&B); });
	for (int32 Index = 0; Index + 1 < CameraKeys.Num(); Index += 2)
	{
		FUOMenuCameraShot& Shot = LevelMenuShots.AddDefaulted_GetRef();
		Shot.From = CameraKeys[Index]->GetActorTransform();
		Shot.To = CameraKeys[Index + 1]->GetActorTransform();
	}

	if (SignAnchor)
	{
		SpawnCompanySign(SignAnchor->GetActorTransform());
	}

	CaptureLevelLighting();
	BossSystem->ScanLevel();
	SetupTerminals(TerminalAnchors, ActorsByName);

	// Lights know their room (cut camera sabotage)
	for (FLightSnapshot& Snapshot : LevelLights)
	{
		if (const ULightComponent* Light = Snapshot.Light.Get())
		{
			Snapshot.Room = BossSystem->GetRoomAt(Light->GetComponentLocation());
		}
	}
	return true;
}

void AUOGameMode::SpawnTerminal(EUOTerminalKind Kind, const FTransform& Stand, AActor* Furniture, int32 Ordinal)
{
	using namespace UOGameModePrivate;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AUOTerminal* Terminal = GetWorld()->SpawnActor<AUOTerminal>(AUOTerminal::StaticClass(), Stand, SpawnParams);
	if (!Terminal)
	{
		return;
	}
	const FName RoomId = Furniture ? GetRoomId(Furniture, Furniture->GetName()) : BossSystem->GetRoomAt(Stand.GetLocation());
	const FName Id(*FString::Printf(TEXT("%s_%02d"), *StaticEnum<EUOTerminalKind>()->GetNameStringByValue(static_cast<int64>(Kind)), Ordinal));
	Terminal->ConfigureTerminal(Id, Kind, Furniture, RoomId, RoomId.IsNone() ? FText::GetEmpty() : FUORooms::GetDisplayName(RoomId), Ordinal);
	Terminals.Add(Terminal);
}

void AUOGameMode::SetupTerminals(const TArray<ATargetPoint*>& Anchors, const TMap<FName, AActor*>& ActorsByName)
{
	using namespace UOGameModePrivate;

	TMap<EUOTerminalKind, int32> Ordinals;
	for (const ATargetPoint* Anchor : Anchors)
	{
		EUOTerminalKind Kind = EUOTerminalKind::DeskComputer;
		bool bKind = false;
		AActor* Furniture = nullptr;
		for (const FName& Tag : Anchor->Tags)
		{
			const FString TagString = Tag.ToString();
			if (TagString.StartsWith(TerminalTagPrefix))
			{
				const int64 Value = StaticEnum<EUOTerminalKind>()->GetValueByNameString(TagString.RightChop(TerminalTagPrefix.Len()));
				bKind = Value != INDEX_NONE;
				Kind = bKind ? static_cast<EUOTerminalKind>(Value) : Kind;
			}
			else if (TagString.StartsWith(FurnitureTagPrefix))
			{
				if (AActor* const* Found = ActorsByName.Find(FName(*TagString.RightChop(FurnitureTagPrefix.Len()))))
				{
					Furniture = *Found;
				}
			}
		}
		if (!bKind)
		{
			UE_LOG(LogUnpaidOvertime, Warning, TEXT("Terminal anchor %s has an unknown kind, skipped."), *Anchor->GetName());
			continue;
		}
		SpawnTerminal(Kind, FloorTransform(Anchor), Furniture, ++Ordinals.FindOrAdd(Kind));
	}

	// Levels built before the M4 pipeline: terminals next to known furniture
	if (Anchors.Num() == 0)
	{
		UE_LOG(LogUnpaidOvertime, Warning, TEXT("No UOTerminal anchors in this level: placing terminals next to known furniture (rebuild with Scripts\\BuildArt.bat meshes,level for the real props)."));
		for (const TPair<FName, AActor*>& Pair : ActorsByName)
		{
			const FString Name = Pair.Key.ToString();
			EUOTerminalKind Kind;
			if (Name == TEXT("Reception_Monitor"))					{ Kind = EUOTerminalKind::ActivityBoard; }
			else if (Name == TEXT("Manager_Monitor"))				{ Kind = EUOTerminalKind::SecurityMonitor; }
			else if (Name == TEXT("CopyRoom_Printer_Extra"))		{ Kind = EUOTerminalKind::PrintServer; }
			else if (Name == TEXT("Supply_Shelf_West"))				{ Kind = EUOTerminalKind::InventorySheet; }
			else if (Name.StartsWith(TEXT("Pod")) && Name.EndsWith(TEXT("_Monitor")) && Name.Contains(TEXT("Desk1")))	{ Kind = EUOTerminalKind::DeskComputer; }
			else if (Name.StartsWith(TEXT("Lobby_Plant")) || Name.StartsWith(TEXT("OpenPlan_Plant_S")) || Name == TEXT("CopyRoom_Plant") || Name == TEXT("Records_Plant")) { Kind = EUOTerminalKind::MovableProp; }
			else { continue; }

			AActor* Furniture = Pair.Value;
			FVector Origin;
			FVector Extent;
			Furniture->GetActorBounds(false, Origin, Extent);
			const FVector Forward = Furniture->GetActorForwardVector().GetSafeNormal2D();
			const FVector Stand = FVector(Origin.X, Origin.Y, Furniture->GetActorLocation().Z) + Forward * (FMath::Max(Extent.X, Extent.Y) + 60.f);
			SpawnTerminal(Kind, FTransform((-Forward).Rotation(), FVector(Stand.X, Stand.Y, 5.f)), Furniture, ++Ordinals.FindOrAdd(Kind));
		}
	}

	FString Summary;
	for (const TPair<EUOTerminalKind, int32>& Pair : Ordinals)
	{
		Summary += FString::Printf(TEXT("%s=%d "), *StaticEnum<EUOTerminalKind>()->GetNameStringByValue(static_cast<int64>(Pair.Key)), Pair.Value);
	}
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Terminals: %d placed (%s)."), Terminals.Num(), *Summary);
}

void AUOGameMode::SetupProceduralWorld()
{
	UE_LOG(LogUnpaidOvertime, Log, TEXT("No station anchors in this map: building the procedural greybox office."));

	SpawnProceduralEnvironment();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	OfficeBuilder = GetWorld()->SpawnActor<AUOOfficeBuilder>(AUOOfficeBuilder::StaticClass(), FTransform::Identity, SpawnParams);

	// Empty preview office behind the menus
	OfficeBuilder->BuildOffice(1, FUOStationRequirements(), FString());

	PlayerStartTransform = OfficeBuilder->GetPlayerStartTransform();
	NavigationHub = OfficeBuilder->GetHubLocation();
	AISpawnTransforms.Reset();
	for (const FVector& Location : OfficeBuilder->GetAISpawnLocations())
	{
		AISpawnTransforms.Add(FTransform(Location));
	}
}

void AUOGameMode::SpawnProceduralEnvironment()
{
	UWorld* World = GetWorld();
	const FTransform SunTransform(SunDayRotation, FVector(0.f, 0.f, 2000.f));

	SunLight = World->SpawnActorDeferred<ADirectionalLight>(ADirectionalLight::StaticClass(), SunTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	UDirectionalLightComponent* Sun = SunLight ? Cast<UDirectionalLightComponent>(SunLight->GetLightComponent()) : nullptr;
	if (Sun)
	{
		Sun->SetMobility(EComponentMobility::Movable);
		Sun->SetIntensity(SunBaseIntensity);
		Sun->SetAtmosphereSunLight(true);
	}
	if (SunLight)
	{
		SunLight->FinishSpawning(SunTransform);
	}

	const FTransform SkyTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 1000.f));
	SkyLight = World->SpawnActorDeferred<ASkyLight>(ASkyLight::StaticClass(), SkyTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (SkyLight)
	{
		USkyLightComponent* Sky = SkyLight->GetLightComponent();
		Sky->SetMobility(EComponentMobility::Movable);
		Sky->bRealTimeCapture = true;
		Sky->SetIntensity(SkyBaseIntensity);
		SkyLight->FinishSpawning(SkyTransform);
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SkyAtmosphere = World->SpawnActor<ASkyAtmosphere>(ASkyAtmosphere::StaticClass(), FTransform::Identity, SpawnParams);
}

void AUOGameMode::CaptureLevelLighting()
{
	LevelLights.Reset();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TInlineComponentArray<ULightComponentBase*> Lights(*It);
		for (ULightComponentBase* Base : Lights)
		{
			if (!Base || Base->Mobility != EComponentMobility::Movable || !Base->IsRegistered())
			{
				continue;
			}

			FLightSnapshot& Snapshot = LevelLights.AddDefaulted_GetRef();
			Snapshot.Intensity = Base->Intensity;
			Snapshot.Color = Base->GetLightColor();
			Snapshot.Rotation = Base->GetComponentRotation();
			if (ULightComponent* Light = Cast<ULightComponent>(Base))
			{
				Snapshot.Light = Light;
				Snapshot.bDirectional = Light->IsA<UDirectionalLightComponent>();
			}
			else if (Base->IsA<USkyLightComponent>())
			{
				SkyLight = Cast<ASkyLight>(*It);
				Snapshot.bSky = true;
			}
		}
	}

	for (TActorIterator<APostProcessVolume> It(GetWorld()); It; ++It)
	{
		if (It->bUnbound)
		{
			LevelPostProcess = *It;
			LevelExposureBias = It->Settings.AutoExposureBias;
			break;
		}
	}

	OfficeLightingCollection = LoadObject<UMaterialParameterCollection>(nullptr, UOGameModePrivate::OfficeLightingCollectionPath);
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Level lighting captured: %d movable lights, post-process %s, lighting collection %s."),
		LevelLights.Num(), LevelPostProcess ? TEXT("found") : TEXT("missing"), OfficeLightingCollection ? TEXT("found") : TEXT("missing"));
}

void AUOGameMode::SpawnCompanySign(const FTransform& Anchor)
{
	using namespace UOGameModePrivate;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Text sits just in front of the board face, facing the anchor's forward (into the lobby)
	FTransform TextTransform(FRotator(0.f, Anchor.Rotator().Yaw, 0.f), Anchor.GetLocation() + Anchor.GetRotation().GetForwardVector() * 1.f + FVector(0.f, 0.f, 4.f));
	CompanySign = GetWorld()->SpawnActor<ATextRenderActor>(ATextRenderActor::StaticClass(), TextTransform, SpawnParams);
	if (!CompanySign)
	{
		return;
	}

	UTextRenderComponent* Text = CompanySign->GetTextRender();
	Text->SetMobility(EComponentMobility::Movable);
	Text->SetHorizontalAlignment(EHTA_Center);
	Text->SetVerticalAlignment(EVRTA_TextCenter);
	Text->SetTextRenderColor(FColor(28, 28, 32));
	if (UMaterialInterface* SignMaterial = LoadObject<UMaterialInterface>(nullptr, SignTextMaterialPath))
	{
		Text->SetTextMaterial(SignMaterial);
	}
	Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Text->SetCastShadow(false);
	SetCompanySignText(LOCTEXT("SignPlaceholder", "NOW HIRING").ToString());
}

void AUOGameMode::SetCompanySignText(const FString& InText)
{
	using namespace UOGameModePrivate;

	if (!CompanySign)
	{
		return;
	}

	UTextRenderComponent* Text = CompanySign->GetTextRender();
	Text->SetWorldSize(SignTextMaxSize);
	Text->SetText(FText::FromString(InText.ToUpper()));

	// Shrink long names to the board width (local size Y is the text width)
	const FVector LocalSize = Text->GetTextLocalSize();
	if (LocalSize.Y > SignTextMaxWidth)
	{
		Text->SetWorldSize(SignTextMaxSize * SignTextMaxWidth / LocalSize.Y);
	}
}

void AUOGameMode::LogNavigationStatus(const TCHAR* Context) const
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		UE_LOG(LogUnpaidOvertime, Warning, TEXT("Navmesh (%s): no navigation system; AIs will steer without paths."), Context);
		return;
	}

	const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
	const bool bBuilding = NavSys->IsNavigationBuildInProgress();
	if (!NavData)
	{
		UE_LOG(LogUnpaidOvertime, Warning, TEXT("Navmesh (%s): no navigation data (building %d); AIs will steer without paths."), Context, bBuilding ? 1 : 0);
		return;
	}

	const TArray<TObjectPtr<AUOInteractableStation>>& Stations = LevelStations.Num() > 0 ? LevelStations : (OfficeBuilder ? OfficeBuilder->GetStations() : LevelStations);
	const FVector From = AISpawnTransforms.Num() > 0 ? AISpawnTransforms[0].GetLocation() : PlayerStartTransform.GetLocation();
	int32 Reachable = 0;
	int32 Partial = 0;
	for (const AUOInteractableStation* Station : Stations)
	{
		if (!Station)
		{
			continue;
		}
		const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, From, Station->GetInteractionLocation());
		if (Path && Path->IsValid())
		{
			Path->IsPartial() ? ++Partial : ++Reachable;
		}
	}

	UE_LOG(LogUnpaidOvertime, Log, TEXT("Navmesh (%s): %s ready, building %d, %d/%d stations reachable from the AI spawn (%d partial)."),
		Context, *NavData->GetName(), bBuilding ? 1 : 0, Reachable, Stations.Num(), Partial);
}

TArray<FUOMenuCameraShot> AUOGameMode::GetMenuCameraShots() const
{
	if (LevelMenuShots.Num() > 0)
	{
		return LevelMenuShots;
	}

	FUOMenuCameraShot Shot;
	if (OfficeBuilder)
	{
		Shot.From = OfficeBuilder->GetOverviewCameraTransform();
		Shot.To = Shot.From;
		Shot.To.AddToTranslation(Shot.From.GetRotation().GetRightVector() * 300.f);
	}
	else
	{
		Shot.From = FTransform(FRotator(-10.f, 45.f, 0.f), PlayerStartTransform.GetLocation() + FVector(0.f, 0.f, 120.f));
		Shot.To = FTransform(FRotator(-10.f, 60.f, 0.f), Shot.From.GetLocation() + FVector(200.f, 200.f, 0.f));
	}
	return { Shot };
}

FUOConfigValidationResult AUOGameMode::RequestStartDay(const FUOMatchConfig& Config)
{
	// Server-side validation: never trust what the UI sent
	const FUOConfigValidationResult Result = Config.Validate();
	if (!Result.bIsValid)
	{
		return Result;
	}

	const AUOGameState* UOGameState = GetUOGameState();
	if (!UOGameState || UOGameState->GetPhase() != EUODayPhase::PreGame || bStartDayPending)
	{
		return Result;
	}

	MatchConfig = Config;
	MatchConfig.CompanyName = Config.GetSanitizedCompanyName();
	ActiveSeed = MatchConfig.ResolveSeed(FMath::Rand() ^ static_cast<int32>(FDateTime::Now().GetTicks() & 0x7fffffff));

	// Start on the next tick so the setup screen is not torn down inside its own click handler
	bStartDayPending = true;
	return Result;
}

void AUOGameMode::RequestReturnToMenu(APlayerController* Requester)
{
	if (Requester)
	{
		Requester->SetPause(false);
		Requester->RestartLevel();
	}
}

bool AUOGameMode::SubmitEvaluationChoice(APlayerController* Requester, const FUOEvalChoice& Choice)
{
	const AUOPlayerState* Employee = Requester ? Requester->GetPlayerState<AUOPlayerState>() : nullptr;
	return Employee && EvaluationDirector->SubmitChoice(Employee->GetEmployeeId(), Choice);
}

bool AUOGameMode::SubmitPinToggle(APlayerController* Requester, int32 TraceId)
{
	AUOPlayerState* Employee = Requester ? Requester->GetPlayerState<AUOPlayerState>() : nullptr;
	UUOEvidenceSubsystem* Evidence = UUOEvidenceSubsystem::Get(this);
	return Employee && Evidence && Evidence->TogglePin(Employee, TraceId);
}

void AUOGameMode::SetRoomBlackout(FName Room, bool bBlackout)
{
	if (Room.IsNone())
	{
		return;
	}
	bBlackout ? (void)BlackoutRooms.Add(Room) : (void)BlackoutRooms.Remove(Room);
	const AUOGameState* UOGameState = GetUOGameState();
	ApplyOvertimeEffects(UOGameState && UOGameState->GetPhase() == EUODayPhase::Overtime);
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Lights: %s %s."), *Room.ToString(), bBlackout ? TEXT("blacked out (camera cut)") : TEXT("back on"));
}

bool AUOGameMode::FindPropDestination(const AUOTerminal* Prop, FTransform& OutDestination, FName& OutRoom) const
{
	if (!Prop)
	{
		return false;
	}
	TArray<const AUOInteractableStation*> Candidates;
	for (const AUOInteractableStation* Station : LevelStations)
	{
		if (Station && !Station->GetRoomId().IsNone() && Station->GetRoomId() != Prop->GetRoomId() && Station->GetStationType() != EUOStationType::SupplyCloset)
		{
			Candidates.Add(Station);
		}
	}
	if (Candidates.Num() == 0)
	{
		return false;
	}
	FRandomStream Rng(DaySeed + Prop->GetUniqueID());
	const AUOInteractableStation* Near = Candidates[Rng.RandRange(0, Candidates.Num() - 1)];
	// Beside the stand point, out of the way of the person using the station
	const FVector Side = Near->GetActorRightVector() * (Rng.FRand() < 0.5f ? 95.f : -95.f);
	OutDestination = FTransform(FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f), Near->GetInteractionLocation() + Side - FVector(0.f, 0.f, 5.f));
	OutRoom = Near->GetRoomId();
	return true;
}

TArray<int32> AUOGameMode::FindRecentlyNear(FName Room, float Seconds, int32 ExcludeId) const
{
	TArray<int32> Ids;
	const AUOGameState* UOGameState = GetUOGameState();
	if (!UOGameState || Room.IsNone())
	{
		return Ids;
	}
	const float Now = UOGameState->GetShiftElapsedSeconds();
	for (const TPair<int32, TArray<FUOPresenceInterval>>& Pair : BossSystem->GetPresence().ByEmployee)
	{
		const AUOPlayerState* Employee = UOGameState->FindEmployee(Pair.Key);
		if (Pair.Key == ExcludeId || !Employee || !Employee->IsAlive())
		{
			continue;
		}
		for (const FUOPresenceInterval& Interval : Pair.Value)
		{
			if (Interval.Room == Room && Interval.ExitTime >= Now - Seconds)
			{
				Ids.AddUnique(Pair.Key);
				break;
			}
		}
	}
	return Ids;
}

bool AUOGameMode::FindBodySpot(const FVector& Near, float PreferredYaw, FVector& OutFloor, float& OutYaw) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	// A body is ~1.9 m long: keep a clear box (with a margin from walls and furniture) on the floor
	const FVector HalfExtent(115.f, 55.f, 30.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(UOBodySpot), false);
	FCollisionObjectQueryParams Statics;
	Statics.AddObjectTypesToQuery(ECC_WorldStatic);
	Statics.AddObjectTypesToQuery(ECC_WorldDynamic);
	const float Radii[] = { 0.f, 60.f, 120.f, 180.f, 240.f, 320.f, 420.f };
	for (const float Radius : Radii)
	{
		const int32 Steps = Radius <= 0.f ? 1 : 12;
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			const float Angle = 2.f * PI * Step / Steps;
			const FVector Candidate = Near + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;
			FHitResult Floor;
			if (!World->LineTraceSingleByObjectType(Floor, Candidate + FVector(0.f, 0.f, 60.f), Candidate - FVector(0.f, 0.f, 260.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
			{
				continue;
			}
			// The floor must be flat and the line from the victim to the spot must not cross a wall
			if (Floor.ImpactNormal.Z < 0.9f || World->LineTraceTestByObjectType(Near, Floor.ImpactPoint + FVector(0.f, 0.f, 40.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
			{
				continue;
			}
			for (int32 YawStep = 0; YawStep < 4; ++YawStep)
			{
				const float Yaw = PreferredYaw + 45.f * YawStep;
				const FQuat Rotation = FRotator(0.f, Yaw, 0.f).Quaternion();
				const FVector Center = Floor.ImpactPoint + FVector(0.f, 0.f, HalfExtent.Z + 4.f);
				if (!World->OverlapAnyTestByObjectType(Center, Rotation, Statics, FCollisionShape::MakeBox(HalfExtent), Params))
				{
					OutFloor = Floor.ImpactPoint;
					OutYaw = Yaw;
					return true;
				}
			}
		}
	}
	return false;
}

void AUOGameMode::HandleDismissal(AUOPlayerState* Killer, AUOPlayerState* Victim, EUOTraceDetail Detail, bool bLastSeenNote)
{
	AUOGameState* UOGameState = GetUOGameState();
	UUOEvidenceSubsystem* Evidence = UUOEvidenceSubsystem::Get(this);
	APawn* VictimPawn = Victim ? Victim->GetPawn() : nullptr;
	if (!UOGameState || !Victim || !VictimPawn || !Evidence)
	{
		return;
	}

	const FVector Where = VictimPawn->GetActorLocation();
	const FName Room = Evidence->GetRoomOf(VictimPawn);

	// The incident trace (every kill this match makes the next one more detailed)
	FUOTraceRecord Record;
	Record.Device = EUOTraceDevice::CameraLog;
	Record.Kind = EUOTraceKind::BodyFound;
	Record.TrueActorId = Killer ? Killer->GetEmployeeId() : INDEX_NONE;
	Record.Detail = Detail;
	Record.Room = Room;
	Record.Subject = Victim->GetDisplayName();
	Record.bAct = true;
	Evidence->Record(Record);

	// The public "last seen near" note: who was really around (the killer had to be), plus anyone else who was
	TArray<int32> LastSeen;
	if (bLastSeenNote)
	{
		LastSeen = FindRecentlyNear(Room, FUOKillerTuning::LastSeenWindowSeconds, Victim->GetEmployeeId());
		if (Killer)
		{
			LastSeen.AddUnique(Killer->GetEmployeeId());
		}
		FRandomStream Rng(DaySeed + Victim->GetEmployeeId() * 97 + static_cast<int32>(UOGameState->GetShiftElapsedSeconds()));
		for (int32 Index = LastSeen.Num() - 1; Index > 0; --Index)
		{
			LastSeen.Swap(Index, Rng.RandRange(0, Index));
		}
		// At most three names (the killer stays among them)
		while (LastSeen.Num() > 3)
		{
			const int32 Drop = Rng.RandRange(0, LastSeen.Num() - 1);
			if (!Killer || LastSeen[Drop] != Killer->GetEmployeeId())
			{
				LastSeen.RemoveAt(Drop);
			}
		}
		FUOTraceRecord Note;
		Note.Device = EUOTraceDevice::ActivityBoard;
		Note.Kind = EUOTraceKind::LastSeenNear;
		Note.Room = Room;
		Note.Subject = Victim->GetDisplayName();
		Note.TrueActorId = Killer ? Killer->GetEmployeeId() : INDEX_NONE;
		Note.ShownOverride = LastSeen;
		Evidence->Record(Note);
		FString Names;
		for (const int32 Id : LastSeen)
		{
			Names += UOGameState->GetEmployeeName(Id) + TEXT("; ");
		}
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Dismissal: public note, last seen near the %s: %s[audit: the killer is among them]"), *Room.ToString(), *Names);
	}

	// The body: the victim's mannequin lying flat on a clear patch of floor nearby, a termination letter beside it
	USkeletalMesh* Mesh = nullptr;
	if (const ACharacter* AsCharacter = Cast<ACharacter>(VictimPawn))
	{
		Mesh = AsCharacter->GetMesh() ? AsCharacter->GetMesh()->GetSkeletalMeshAsset() : nullptr;
	}
	if (!Mesh)
	{
		Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	}
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FVector Floor = Where - FVector(0.f, 0.f, VictimPawn->GetSimpleCollisionHalfHeight());
	float Yaw = VictimPawn->GetActorRotation().Yaw;
	// The victim's pawn must not block the search
	VictimPawn->SetActorEnableCollision(false);
	const bool bClear = FindBodySpot(Where, Yaw, Floor, Yaw);
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Dismissal: body placed %s (%.0f cm from where %s stood, yaw %.0f)."), bClear ? TEXT("on a clear patch of floor") : TEXT("where they stood (no clear spot found)"),
		FVector::Dist2D(Floor, Where), *Victim->GetDisplayName(), Yaw);
	if (AUOBodyActor* Body = GetWorld()->SpawnActor<AUOBodyActor>(AUOBodyActor::StaticClass(), FTransform(FRotator(0.f, Yaw, 0.f), Floor), SpawnParams))
	{
		Body->SetupBody(Mesh, Victim->GetEmployeeColor(), Victim->GetDisplayName());
		Bodies.Add(Body);
	}
	if (UUOAudioSubsystem* Audio = UUOAudioSubsystem::Get(this))
	{
		Audio->PlayAtLocation(UOSoundId::KillSting, Where);
	}

	Victim->MarkEliminated(EUOExitCause::Dismissed, DayNumber);
	RoleSystem->OnEmployeeEliminated(Victim);
	OnEmployeeEliminated(Victim);
	if (!Victim->IsAIEmployee())
	{
		VictimPawn->SetActorHiddenInGame(true);
		VictimPawn->DisableInput(nullptr);
	}

	// Emergency meeting: the clock stops, everyone stops where they are
	PhaseBeforeMeeting = UOGameState->GetPhase();
	UOGameState->SetPhase(EUODayPhase::Meeting);
	for (AUOAIEmployee* AIPawn : AIEmployees)
	{
		if (AController* Controller = AIPawn ? AIPawn->GetController() : nullptr)
		{
			Controller->StopMovement();
		}
	}
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Dismissal: %s was dismissed in %s at %s. Emergency meeting."), *Victim->GetDisplayName(), *Room.ToString(), *UOGameState->GetClockText());
	EvaluationDirector->BeginEmergencyMeeting(Victim->GetEmployeeId(), Room, UOGameState->GetClockText(), LastSeen, DayNumber, DaySeed, MatchConfig.AIDifficulty);
}

void AUOGameMode::ResumeAfterMeeting()
{
	AUOGameState* UOGameState = GetUOGameState();
	if (!UOGameState || UOGameState->GetPhase() != EUODayPhase::Meeting)
	{
		return;
	}
	UOGameState->ClearEvaluation();
	UOGameState->SetPhase(PhaseBeforeMeeting);
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Emergency meeting over: back to work (%s)."), *UOText::GetPhaseName(PhaseBeforeMeeting).ToString());
}

void AUOGameMode::RequestNextDay()
{
	bNextDayPending = true;
}

void AUOGameMode::RequestNewHire()
{
	if (MatchConfig.Seed == 0)
	{
		ActiveSeed = MatchConfig.ResolveSeed(FMath::Rand() ^ static_cast<int32>(FDateTime::Now().GetTicks() & 0x7fffffff));
	}
	UE_LOG(LogUnpaidOvertime, Log, TEXT("New hire: starting a new match for '%s' (seed %d)."), *MatchConfig.CompanyName, ActiveSeed);
	bStartDayPending = true;
}

void AUOGameMode::RequestReturnToMenuFromEvaluation()
{
	RequestReturnToMenu(GetWorld()->GetFirstPlayerController());
}

void AUOGameMode::OnEmployeeEliminated(AUOPlayerState* Employee)
{
	if (!Employee)
	{
		return;
	}

	TaskManager->ReleaseEmployee(Employee);
	RoleSystem->OnEmployeeEliminated(Employee);
	UE_LOG(LogUnpaidOvertime, Log, TEXT("%s leaves the company (%s, day %d)."), *Employee->GetDisplayName(),
		*StaticEnum<EUOExitCause>()->GetNameStringByValue(static_cast<int64>(Employee->GetExitCause())), DayNumber);

	// The AI's body leaves (simply vanishes for now); it is destroyed when the next day starts
	if (Employee->IsAIEmployee())
	{
		if (APawn* Pawn = Employee->GetPawn())
		{
			Pawn->SetActorHiddenInGame(true);
			Pawn->SetActorEnableCollision(false);
		}
	}
}

void AUOGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bStartDayPending)
	{
		bStartDayPending = false;
		StartDay(true);
		return;
	}

	if (bNextDayPending)
	{
		bNextDayPending = false;
		++DayNumber;
		StartDay(false);
		return;
	}

	TickDay(DeltaSeconds);
}

void AUOGameMode::StartDay(bool bNewMatch)
{
	AUOGameState* UOGameState = GetUOGameState();
	if (!UOGameState)
	{
		return;
	}

	if (bNewMatch)
	{
		DayNumber = 1;
	}
	// Every day gets new tasks from the match seed (day 1 keeps the match seed itself)
	DaySeed = ActiveSeed + (DayNumber - 1) * 7919;

	// Task difficulty and shift follow the day (hardcore boost on the overtime weekend only); AIs follow the plain day number
	const float Difficulty = FUODayDifficulty::GetTaskDifficulty(DayNumber);
	const bool bHardcore = FUODayDifficulty::IsHardcoreDay(DayNumber);
	const FUOAIDifficultyParams AIParams = FUOAIDifficulty::Compute(MatchConfig.AIDifficulty, DayNumber);
	int32 MinSubs = 0;
	int32 MaxSubs = 0;
	FUOTaskGenerator::GetSubObjectiveRange(Difficulty, MinSubs, MaxSubs);
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Starting %s for '%s' (%s, day seed %d, %d tasks, %s, AI %s, %s office)"), *FUODayCalendar::FormatDayName(DayNumber).ToString(),
		*MatchConfig.CompanyName, bNewMatch ? TEXT("new match") : TEXT("same match"), DaySeed, MatchConfig.TasksPerDay, *UOText::GetDayLengthName(MatchConfig.DayLength).ToString(),
		*UOText::GetDifficultyName(MatchConfig.AIDifficulty).ToString(), bLevelDriven ? TEXT("level") : TEXT("procedural"));
	UE_LOG(LogUnpaidOvertime, Log, TEXT("Difficulty day %d%s: D=%.2f (curve %.2f), sub-objectives %d-%d per task, work time x%.2f, shift x%.2f | AI speed %.2f precision %.2f think %.2fs answer accuracy %.2f vote noise %.2f"),
		DayNumber, bHardcore ? TEXT(" [HARDCORE overtime weekend]") : TEXT(""), Difficulty, FUOTaskGenerator::GetDifficultyForDay(DayNumber), MinSubs, MaxSubs,
		FUOTaskGenerator::GetWorkSecondsScale(Difficulty), FUODayDifficulty::GetShiftScale(DayNumber), AIParams.WorkSpeedMultiplier, AIParams.Precision,
		AIParams.ThinkDelaySeconds, AIParams.AnswerAccuracy, AIParams.VoteNoise);

	UOGameState->SetCompanyName(MatchConfig.CompanyName);
	UOGameState->SetDayNumber(DayNumber);
	UOGameState->SetUnpaidOvertime(false);
	UOGameState->SetDayForceClosed(false);
	UOGameState->ClearEvaluation();
	EvaluationDirector->Reset(bNewMatch);
	BossSystem->ResetForDay();
	if (UUOEvidenceSubsystem* Evidence = UUOEvidenceSubsystem::Get(this))
	{
		Evidence->ResetForDay(DayNumber, DaySeed);
	}
	// Overnight: jams cleared, locks released, cameras back online, props back, bodies removed, lights on
	for (AUOInteractableStation* Station : LevelStations)
	{
		if (Station)
		{
			Station->ClearSabotage();
		}
	}
	for (AUOTerminal* Terminal : Terminals)
	{
		if (Terminal)
		{
			Terminal->ResetForDay();
		}
	}
	for (AUOBodyActor* Body : Bodies)
	{
		if (Body)
		{
			Body->Destroy();
		}
	}
	Bodies.Reset();
	BlackoutRooms.Reset();
	if (bNewMatch)
	{
		DestroyAIEmployees();
		UOGameState->ClearEmployees();
	}
	if (bLevelDriven)
	{
		FUOStationSlots Slots;
		for (const AUOInteractableStation* Station : LevelStations)
		{
			Slots.Add(Station->GetStationType());
		}

		FUOGeneratedDay Day = FUOTaskGenerator::GenerateForSlots(DaySeed, MatchConfig.TasksPerDay, Difficulty, Slots);
		NameTasksAfterRooms(Day.Tasks);

		TArray<AUOInteractableStation*> Stations;
		for (AUOInteractableStation* Station : LevelStations)
		{
			Station->SetWorkDone(false);
			Stations.Add(Station);
		}
		TaskManager->StartDay(Day.Tasks, Stations);
		SetCompanySignText(MatchConfig.CompanyName);
		LogNavigationStatus(TEXT("day start"));
	}
	else if (OfficeBuilder)
	{
		const FUOGeneratedDay Day = FUOTaskGenerator::Generate(DaySeed, MatchConfig.TasksPerDay, Difficulty);
		const TArray<AUOInteractableStation*> Stations = OfficeBuilder->BuildOffice(DaySeed, Day.Stations, MatchConfig.CompanyName);
		TaskManager->StartDay(Day.Tasks, Stations);
	}

	int32 SubObjectives = 0;
	for (const FUOTaskInstance& Task : UOGameState->GetTasks())
	{
		SubObjectives += Task.SubObjectives.Num();
		UE_LOG(LogUnpaidOvertime, Log, TEXT("  Task %d: %s (%d sub-objectives)"), Task.Id, *Task.DisplayName.ToString(), Task.SubObjectives.Num());
	}

	if (bNewMatch)
	{
		int32 NextEmployeeId = 0;
		SpawnHumanEmployees(NextEmployeeId, true);
		SpawnAIEmployees(MatchConfig.GetAIPlayers(), NextEmployeeId);
		RoleSystem->AssignRoles(MatchConfig.KillerAssignment, ActiveSeed, MatchConfig.bKillerCanKill, DevKillerCount);
	}
	else
	{
		int32 Unused = 0;
		SpawnHumanEmployees(Unused, false);
		ResetAIEmployeesForNewDay();
	}

	RoleSystem->StartDay(DayNumber);

	DayClock.Start(FUODayDifficulty::GetShiftSeconds(MatchConfig.DayLength, DayNumber, FUODayClock::GetConsoleShiftSecondsOverride()));
	UOGameState->SetClock(DayClock.GetElapsedSeconds(), DayClock.GetShiftLengthSeconds());
	for (AController* Controller : AIControllers)
	{
		if (AUOAIController* Brain = Cast<AUOAIController>(Controller))
		{
			Brain->BeginDay();
		}
	}
	UOGameState->SetPhase(EUODayPhase::Shift);
	ApplyOvertimeEffects(false);

	UE_LOG(LogUnpaidOvertime, Log, TEXT("Day %d started: %d tasks, %d sub-objectives, shift %.0fs, %d employees alive."), DayNumber, UOGameState->GetTasks().Num(), SubObjectives,
		DayClock.GetShiftLengthSeconds(), UOGameState->CountAliveEmployees());
}

void AUOGameMode::NameTasksAfterRooms(TArray<FUOTaskInstance>& Tasks) const
{
	TMap<FString, int32> NameUses;
	for (FUOTaskInstance& Task : Tasks)
	{
		TArray<FText> Rooms;
		for (const FUOSubObjective& Sub : Task.SubObjectives)
		{
			const AUOInteractableStation* Station = TaskManager->FindStation(Sub.StationId);
			if (!Station)
			{
				for (const AUOInteractableStation* Candidate : LevelStations)
				{
					if (Candidate && Candidate->GetStationId() == Sub.StationId)
					{
						Station = Candidate;
						break;
					}
				}
			}
			if (Station && !Station->GetRoomName().IsEmpty()
				&& !Rooms.ContainsByPredicate([Station](const FText& Room) { return Room.EqualTo(Station->GetRoomName()); }))
			{
				Rooms.Add(Station->GetRoomName());
			}
		}

		FText Where;
		if (Rooms.Num() == 1)
		{
			Where = Rooms[0];
		}
		else if (Rooms.Num() == 2)
		{
			Where = FText::Format(LOCTEXT("TwoRooms", "{0} & {1}"), Rooms[0], Rooms[1]);
		}
		else if (Rooms.Num() > 2)
		{
			Where = FText::Format(LOCTEXT("ManyRooms", "{0} rooms"), FText::AsNumber(Rooms.Num()));
		}
		else
		{
			continue;
		}

		FText Name = FText::Format(LOCTEXT("RoomTaskName", "{0} — {1}"), UOText::GetTaskTypeName(Task.Type), Where);
		int32& Uses = NameUses.FindOrAdd(Name.ToString());
		if (++Uses > 1)
		{
			Name = FText::Format(LOCTEXT("RoomTaskNameN", "{0} ({1})"), Name, FText::AsNumber(Uses));
		}
		Task.DisplayName = Name;
	}
}

void AUOGameMode::SpawnHumanEmployees(int32& InOutNextEmployeeId, bool bInitRecords)
{
	AUOGameState* UOGameState = GetUOGameState();
	int32 HumanIndex = 0;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		AUOPlayerState* Employee = PC ? PC->GetPlayerState<AUOPlayerState>() : nullptr;
		if (!Employee)
		{
			continue;
		}

		if (bInitRecords)
		{
			const FText Name = HumanIndex == 0
				? LOCTEXT("HumanName", "New hire")
				: FText::Format(LOCTEXT("HumanNameN", "New hire #{0}"), FText::AsNumber(HumanIndex + 1));
			Employee->InitEmployee(InOutNextEmployeeId++, Name.ToString(), false, UOGameModePrivate::HumanColor, 1.f);
			UOGameState->AddEmployee(Employee);
		}
		else
		{
			Employee->ResetForNewDay();
		}

		if (APawn* OldPawn = PC->GetPawn())
		{
			PC->UnPossess();
			OldPawn->Destroy();
		}

		// Spread extra humans out a little
		FTransform SpawnTransform = PlayerStartTransform;
		SpawnTransform.AddToTranslation(SpawnTransform.GetRotation().GetRightVector() * 120.f * HumanIndex);
		RestartPlayerAtTransform(PC, SpawnTransform);
		++HumanIndex;
	}
}

void AUOGameMode::SpawnAIEmployees(int32 Count, int32& InOutNextEmployeeId)
{
	AUOGameState* UOGameState = GetUOGameState();
	UWorld* World = GetWorld();

	// Seeded pick of names, colors and bodies
	FRandomStream Rng(ActiveSeed * 7 + 3);
	TArray<int32> NameOrder;
	for (int32 Index = 0; Index < AINames.Num(); ++Index)
	{
		NameOrder.Add(Index);
	}
	for (int32 Index = NameOrder.Num() - 1; Index > 0; --Index)
	{
		NameOrder.Swap(Index, Rng.RandRange(0, Index));
	}
	const bool bFirstIsQuinn = Rng.FRand() < 0.5f;

	const FUOAIDifficultyParams Difficulty = FUOAIDifficulty::Compute(MatchConfig.AIDifficulty, DayNumber);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FTransform Spawn = AISpawnTransforms.Num() > 0 ? AISpawnTransforms[Index % AISpawnTransforms.Num()] : PlayerStartTransform;
		// Bigger teams than spawn anchors: step aside from the anchor already taken
		const int32 Lap = AISpawnTransforms.Num() > 0 ? Index / AISpawnTransforms.Num() : Index;
		const FVector Location = Spawn.GetLocation() + Spawn.GetRotation().GetRightVector() * 110.f * Lap + FVector(0.f, 0.f, 95.f);
		const FRotator Rotation(0.f, Spawn.Rotator().Yaw + Rng.FRandRange(-40.f, 40.f), 0.f);
		AUOAIEmployee* AIPawn = World->SpawnActor<AUOAIEmployee>(AUOAIEmployee::StaticClass(), Location, Rotation, SpawnParams);
		if (!AIPawn)
		{
			UE_LOG(LogUnpaidOvertime, Error, TEXT("UOGameMode: failed to spawn AI employee %d"), Index);
			continue;
		}

		if (!AIPawn->GetController())
		{
			AIPawn->SpawnDefaultController();
		}

		AUOAIController* Brain = Cast<AUOAIController>(AIPawn->GetController());
		AUOPlayerState* Employee = AIPawn->GetPlayerState<AUOPlayerState>();
		if (!Brain || !Employee)
		{
			UE_LOG(LogUnpaidOvertime, Error, TEXT("UOGameMode: AI employee %d has no controller or player state"), Index);
			AIPawn->Destroy();
			continue;
		}

		const FString Name = AINames.Num() > 0 ? AINames[NameOrder[Index % NameOrder.Num()]].ToString() : FString::Printf(TEXT("Coworker %d"), Index + 1);
		const FLinearColor Color = AIColors.Num() > 0 ? AIColors[Index % AIColors.Num()] : FLinearColor::White;
		const bool bQuinn = (Index % 2 == 0) == bFirstIsQuinn;

		Employee->InitEmployee(InOutNextEmployeeId++, Name, true, Color, Difficulty.WorkSpeedMultiplier);
		AIPawn->SetEmployeeLook(Name, Color, bQuinn);
		Brain->ConfigureEmployee(Difficulty, NavigationHub, DaySeed + 101 * (Index + 1));

		UOGameState->AddEmployee(Employee);
		AIEmployees.Add(AIPawn);
		AIControllers.Add(Brain);
	}
}

void AUOGameMode::ResetAIEmployeesForNewDay()
{
	const FUOAIDifficultyParams Difficulty = FUOAIDifficulty::Compute(MatchConfig.AIDifficulty, DayNumber);

	// Fired employees are gone for the rest of the match
	for (int32 Index = AIEmployees.Num() - 1; Index >= 0; --Index)
	{
		AUOAIEmployee* AIPawn = AIEmployees[Index];
		const AUOPlayerState* Employee = AIPawn ? AIPawn->GetPlayerState<AUOPlayerState>() : nullptr;
		if (!AIPawn || !Employee || !Employee->IsAlive())
		{
			if (AIPawn)
			{
				UE_LOG(LogUnpaidOvertime, Log, TEXT("Day %d: %s's desk is empty."), DayNumber, Employee ? *Employee->GetDisplayName() : TEXT("an AI"));
				AIPawn->Destroy();
			}
			AIEmployees.RemoveAt(Index);
		}
	}

	for (int32 Index = 0; Index < AIEmployees.Num(); ++Index)
	{
		AUOAIEmployee* AIPawn = AIEmployees[Index];
		AUOPlayerState* Employee = AIPawn->GetPlayerState<AUOPlayerState>();
		AUOAIController* Brain = Cast<AUOAIController>(AIPawn->GetController());

		Employee->ResetForNewDay();
		Employee->SetWorkSpeedMultiplier(Difficulty.WorkSpeedMultiplier);

		const FTransform Spawn = AISpawnTransforms.Num() > 0 ? AISpawnTransforms[Index % AISpawnTransforms.Num()] : PlayerStartTransform;
		AIPawn->TeleportTo(Spawn.GetLocation() + FVector(0.f, 0.f, 95.f), FRotator(0.f, Spawn.Rotator().Yaw, 0.f), false, true);
		if (Brain)
		{
			Brain->ResetForNewDay();
			Brain->ConfigureEmployee(Difficulty, NavigationHub, DaySeed + 101 * (Index + 1));
		}
	}
}

void AUOGameMode::DestroyAIEmployees()
{
	for (AUOAIEmployee* AIPawn : AIEmployees)
	{
		if (AIPawn)
		{
			AIPawn->Destroy();
		}
	}
	for (AController* Controller : AIControllers)
	{
		if (Controller)
		{
			Controller->Destroy();
		}
	}
	AIEmployees.Reset();
	AIControllers.Reset();
}

void AUOGameMode::TickDay(float DeltaSeconds)
{
	AUOGameState* UOGameState = GetUOGameState();
	if (!UOGameState || !UOGameState->IsWorkPhase())
	{
		return;
	}

	RoleSystem->TickShift(DeltaSeconds);
	if (UUOEvidenceSubsystem* Evidence = UUOEvidenceSubsystem::Get(this))
	{
		Evidence->TickShift(DeltaSeconds);
	}

	const EUODayPhase Before = DayClock.GetPhase();
	const EUODayPhase After = DayClock.Advance(DeltaSeconds, TaskManager->AreAllSubObjectivesDone());
	UOGameState->SetClock(DayClock.GetElapsedSeconds(), DayClock.GetShiftLengthSeconds());

	// Overtime lighting fades in over a few seconds
	if (UOGameState->GetPhase() == EUODayPhase::Overtime && bLevelDriven && DayClock.GetOvertimeElapsedSeconds() <= UOGameModePrivate::OvertimeBlendSeconds + 0.5f)
	{
		ApplyOvertimeEffects(true);
	}

	if (Before == After)
	{
		return;
	}

	UOGameState->SetPhase(After);

	if (After == EUODayPhase::Overtime)
	{
		UE_LOG(LogUnpaidOvertime, Log, TEXT("Shift over with work pending: OVERTIME (unpaid)."));
		UOGameState->SetUnpaidOvertime(true);
		ApplyOvertimeEffects(true);
	}
	else if (After == EUODayPhase::Evaluation)
	{
		FinishDay();
	}
}

void AUOGameMode::ApplyOvertimeEffects(bool bOvertime)
{
	using namespace UOGameModePrivate;

	if (bLevelDriven)
	{
		const float OvertimeSeconds = DayClock.GetOvertimeElapsedSeconds();
		const float Blend = bOvertime ? FMath::SmoothStep(0.f, 1.f, FMath::Clamp(OvertimeSeconds / OvertimeBlendSeconds, 0.f, 1.f)) : 0.f;

		for (const FLightSnapshot& Snapshot : LevelLights)
		{
			if (Snapshot.bSky)
			{
				if (SkyLight)
				{
					SkyLight->GetLightComponent()->SetIntensity(Snapshot.Intensity * FMath::Lerp(1.f, SkyLevelOvertimeScale, Blend));
				}
				continue;
			}

			ULightComponent* Light = Snapshot.Light.Get();
			if (!Light)
			{
				continue;
			}

			if (Snapshot.bDirectional)
			{
				Light->SetIntensity(Snapshot.Intensity * FMath::Lerp(1.f, SunLevelOvertimeScale, Blend));
				Light->SetLightColor(FMath::Lerp(Snapshot.Color, SunLevelOvertimeColor, Blend));
				FRotator Sunset = Snapshot.Rotation;
				Sunset.Pitch = FMath::Lerp(Snapshot.Rotation.Pitch, SunsetPitch, Blend);
				Light->SetWorldRotation(Sunset);
			}
			else
			{
				const float RoomScale = !Snapshot.Room.IsNone() && BlackoutRooms.Contains(Snapshot.Room) ? FUOKillerTuning::BlackoutLightScale : 1.f;
				Light->SetIntensity(Snapshot.Intensity * FMath::Lerp(1.f, LocalLightOvertimeScale, Blend) * RoomScale);
				Light->SetLightColor(FMath::Lerp(Snapshot.Color, Snapshot.Color * LocalLightOvertimeColor, Blend));
			}
		}

		if (LevelPostProcess)
		{
			LevelPostProcess->Settings.bOverride_AutoExposureBias = true;
			LevelPostProcess->Settings.AutoExposureBias = LevelExposureBias + OvertimeExposureOffset * Blend;
		}

		if (OfficeLightingCollection)
		{
			UKismetMaterialLibrary::SetScalarParameterValue(this, OfficeLightingCollection, LightLevelParameter, FMath::Lerp(1.f, OvertimeLightLevel, Blend));
		}
		return;
	}

	if (UDirectionalLightComponent* Sun = SunLight ? Cast<UDirectionalLightComponent>(SunLight->GetLightComponent()) : nullptr)
	{
		Sun->SetIntensity(SunBaseIntensity * (bOvertime ? SunOvertimeScale : 1.f));
		Sun->SetLightColor(bOvertime ? SunOvertimeColor : FLinearColor::White);
		SunLight->SetActorRotation(bOvertime ? SunOvertimeRotation : SunDayRotation);
	}

	if (SkyLight)
	{
		USkyLightComponent* Sky = SkyLight->GetLightComponent();
		Sky->SetIntensity(SkyBaseIntensity * (bOvertime ? SkyOvertimeScale : 1.f));
		Sky->SetLightColor(bOvertime ? SkyOvertimeColor : FLinearColor::White);
	}

	if (OfficeBuilder)
	{
		OfficeBuilder->SetOvertimeMood(bOvertime);
	}
}

void AUOGameMode::FinishDay()
{
	AUOGameState* UOGameState = GetUOGameState();
	if (!UOGameState)
	{
		return;
	}

	TaskManager->EndDay();
	UOGameState->SetDayForceClosed(DayClock.WasForceClosed());

	const TArray<FUOTaskInstance>& Tasks = UOGameState->GetTasks();
	const int32 Total = FUOTaskBoard::CountTotal(Tasks);
	const int32 Alive = UOGameState->CountAliveEmployees();
	const float FairShare = FUOStarRating::ComputeFairShare(Total, Alive);

	for (AUOPlayerState* Employee : UOGameState->GetEmployees())
	{
		if (!Employee || !Employee->IsAlive())
		{
			continue;
		}

		const int32 Completed = FUOTaskBoard::CountCompletedBy(Tasks, Employee->GetEmployeeId());
		const int32 Stars = FUOStarRating::ComputeStars(Completed, Total, Alive);
		Employee->SetCompletedCount(Completed);
		Employee->SetEvaluation(Stars, FairShare, FUOStarRating::GetConsequence(Stars), FUOStarRating::ComputeContributionRatio(Completed, Total, Alive));

		UE_LOG(LogUnpaidOvertime, Log, TEXT("Evaluation: %s completed %d (fair share %.2f) -> %d stars"), *Employee->GetDisplayName(), Completed, FairShare, Stars);
	}

	// AIs stop where they are
	for (AUOAIEmployee* AIPawn : AIEmployees)
	{
		if (AController* Controller = AIPawn ? AIPawn->GetController() : nullptr)
		{
			Controller->StopMovement();
		}
	}

	UE_LOG(LogUnpaidOvertime, Log, TEXT("Day %d closed at %s (%s)."), DayNumber, *UOGameState->GetClockText(),
		DayClock.WasForceClosed() ? TEXT("force-closed") : TEXT("all work done"));

	EvaluationDirector->BeginEvaluation(DayNumber, DaySeed, MatchConfig.AIDifficulty);
}

#undef LOCTEXT_NAMESPACE
