// UNPAID: OVERTIME - canvas HUD.

#include "UOHUD.h"
#include "UOGameState.h"
#include "UOPlayerState.h"
#include "UOGameMode.h"
#include "UOCharacter.h"
#include "UOInteractionComponent.h"
#include "UOInteractableStation.h"
#include "UOTaskManagerComponent.h"
#include "UOStarRating.h"
#include "UODayClock.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GlobalRenderResources.h"

#define LOCTEXT_NAMESPACE "UOHUD"

namespace UOHUDPrivate
{
	const FLinearColor White(1.f, 1.f, 1.f);
	const FLinearColor Dim(0.65f, 0.67f, 0.72f);
	const FLinearColor Accent(1.f, 0.72f, 0.2f);
	const FLinearColor Good(0.4f, 0.9f, 0.45f);
	const FLinearColor Bad(1.f, 0.3f, 0.25f);
	const FLinearColor Claimed(1.f, 0.75f, 0.25f);
	const FLinearColor Panel(0.f, 0.f, 0.f, 0.55f);
	const FLinearColor StarGold(1.f, 0.8f, 0.15f);
	const FLinearColor StarEmpty(0.45f, 0.45f, 0.5f);

	FLinearColor GetConsequenceColor(EUOStarConsequence Consequence)
	{
		switch (Consequence)
		{
		case EUOStarConsequence::Fired:					return Bad;
		case EUOStarConsequence::CutVoteCandidate:		return FLinearColor(1.f, 0.55f, 0.2f);
		case EUOStarConsequence::PromotionAvailable:	return StarGold;
		default:										return White;
		}
	}

	/** "mm:ss" for real seconds */
	FString FormatCountdown(float Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::CeilToInt(Seconds));
		return FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60);
	}
}

AUOHUD::AUOHUD()
{
}

UFont* AUOHUD::GetBodyFont() const
{
	return GEngine ? GEngine->GetMediumFont() : nullptr;
}

UFont* AUOHUD::GetBigFont() const
{
	return GEngine ? GEngine->GetLargeFont() : nullptr;
}

void AUOHUD::DrawHUD()
{
	Super::DrawHUD();

	const AUOGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AUOGameState>() : nullptr;
	if (!GameState || !Canvas)
	{
		return;
	}

	UIScale = FMath::Max(0.5f, Canvas->ClipY / 1080.f);

	switch (GameState->GetPhase())
	{
	case EUODayPhase::Shift:
	case EUODayPhase::Overtime:
		if (GameState->GetPhase() == EUODayPhase::Overtime)
		{
			DrawOvertimeTint();
		}
		DrawTopBar(*GameState);
		DrawTaskList(*GameState);
		DrawActivityLog(*GameState);
		DrawCrosshairAndPrompt();
		DrawCarriedItem(GetLocalEmployee());
		break;

	case EUODayPhase::Evaluation:
		DrawEvaluation(*GameState);
		break;

	default:
		break;
	}
}

void AUOHUD::DrawTopBar(const AUOGameState& GameState)
{
	using namespace UOHUDPrivate;

	const float BarHeight = 64.f * UIScale;
	DrawRect(Panel, 0.f, 0.f, Canvas->ClipX, BarHeight);

	// Company (left)
	DrawShadowedText(GameState.GetCompanyName().ToUpper(), Accent, 20.f * UIScale, 10.f * UIScale, GetBodyFont(), 1.5f * UIScale);
	DrawShadowedText(FText::Format(LOCTEXT("DayLabel", "Day {0}"), FText::AsNumber(GameState.GetDayNumber())).ToString(), Dim, 20.f * UIScale, 38.f * UIScale, GetBodyFont(), 1.f * UIScale);

	// Clock (center)
	const FString Clock = GameState.GetClockText();
	float ClockW = 0.f;
	float ClockH = 0.f;
	GetTextSize(Clock, ClockW, ClockH, GetBigFont(), 1.2f * UIScale);
	const bool bOvertime = GameState.GetPhase() == EUODayPhase::Overtime;
	DrawShadowedText(Clock, bOvertime ? Bad : White, (Canvas->ClipX - ClockW) * 0.5f, 6.f * UIScale, GetBigFont(), 1.2f * UIScale);

	// Phase (right)
	const float Time = GetWorld()->GetTimeSeconds();
	FLinearColor PhaseColor = White;
	if (bOvertime)
	{
		PhaseColor = Bad;
		PhaseColor.A = 0.55f + 0.45f * FMath::Abs(FMath::Sin(Time * 3.f));
	}
	const FString Phase = UOText::GetPhaseName(GameState.GetPhase()).ToString();
	float PhaseW = 0.f;
	float PhaseH = 0.f;
	GetTextSize(Phase, PhaseW, PhaseH, GetBodyFont(), 1.6f * UIScale);
	DrawShadowedText(Phase, PhaseColor, Canvas->ClipX - PhaseW - 20.f * UIScale, 6.f * UIScale, GetBodyFont(), 1.6f * UIScale);

	FString Detail;
	if (bOvertime)
	{
		const float HardStop = GameState.GetShiftLengthSeconds() * (1.f + FUODayClock::OvertimeCapFraction);
		Detail = FText::Format(LOCTEXT("OvertimeDetail", "Unpaid! Day force-closes in {0}"),
			FText::FromString(FormatCountdown(HardStop - GameState.GetShiftElapsedSeconds()))).ToString();
	}
	else
	{
		Detail = FText::Format(LOCTEXT("ShiftDetail", "Shift ends in {0}"),
			FText::FromString(FormatCountdown(GameState.GetShiftLengthSeconds() - GameState.GetShiftElapsedSeconds()))).ToString();
	}
	float DetailW = 0.f;
	float DetailH = 0.f;
	GetTextSize(Detail, DetailW, DetailH, GetBodyFont(), 1.f * UIScale);
	DrawShadowedText(Detail, bOvertime ? Bad : Dim, Canvas->ClipX - DetailW - 20.f * UIScale, 40.f * UIScale, GetBodyFont(), 1.f * UIScale);
}

void AUOHUD::DrawTaskList(const AUOGameState& GameState)
{
	using namespace UOHUDPrivate;

	const TArray<FUOTaskInstance>& Tasks = GameState.GetTasks();
	const float X = 20.f * UIScale;
	float Y = 84.f * UIScale;
	const float LineHeight = 40.f * UIScale;
	const float PanelWidth = 380.f * UIScale;

	int32 Done = 0;
	int32 Total = 0;
	for (const FUOTaskInstance& Task : Tasks)
	{
		Done += Task.CountDone();
		Total += Task.SubObjectives.Num();
	}

	DrawRect(Panel, X - 8.f * UIScale, Y - 6.f * UIScale, PanelWidth, (34.f * UIScale) + LineHeight * Tasks.Num() + 8.f * UIScale);
	DrawShadowedText(FText::Format(LOCTEXT("TasksHeader", "TASKS  {0}/{1}"), FText::AsNumber(Done), FText::AsNumber(Total)).ToString(), Accent, X, Y, GetBodyFont(), 1.2f * UIScale);
	Y += 30.f * UIScale;

	const int32 MyId = GetLocalEmployee() ? GetLocalEmployee()->GetEmployeeId() : INDEX_NONE;

	for (const FUOTaskInstance& Task : Tasks)
	{
		const bool bComplete = Task.IsComplete();
		DrawShadowedText(Task.DisplayName.ToString(), bComplete ? Good : White, X, Y, GetBodyFont(), 1.f * UIScale);

		const FString Count = FString::Printf(TEXT("%d/%d"), Task.CountDone(), Task.SubObjectives.Num());
		float CountW = 0.f;
		float CountH = 0.f;
		GetTextSize(Count, CountW, CountH, GetBodyFont(), 1.f * UIScale);
		DrawShadowedText(Count, bComplete ? Good : Dim, X + PanelWidth - CountW - 24.f * UIScale, Y, GetBodyFont(), 1.f * UIScale);

		// One pip per sub-objective: green done, amber claimed (blue if mine), grey open
		float PipX = X;
		const float PipY = Y + 19.f * UIScale;
		const float PipSize = 10.f * UIScale;
		for (const FUOSubObjective& Sub : Task.SubObjectives)
		{
			FLinearColor PipColor = FLinearColor(0.35f, 0.35f, 0.4f);
			if (Sub.IsDone())
			{
				PipColor = Sub.CompletedBy == MyId ? FLinearColor(0.3f, 0.75f, 1.f) : Good;
			}
			else if (Sub.State == EUOSubObjectiveState::Claimed)
			{
				PipColor = Claimed;
			}
			DrawRect(PipColor, PipX, PipY, PipSize, PipSize);
			if (Sub.bRequiresItem && !Sub.IsDone())
			{
				DrawRect(FLinearColor::Black, PipX + PipSize * 0.3f, PipY + PipSize * 0.3f, PipSize * 0.4f, PipSize * 0.4f);
			}
			PipX += PipSize + 5.f * UIScale;
		}

		Y += LineHeight;
	}
}

void AUOHUD::DrawCrosshairAndPrompt()
{
	using namespace UOHUDPrivate;

	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;

	const APlayerController* PC = GetOwningPlayerController();
	const AUOCharacter* Character = PC ? Cast<AUOCharacter>(PC->GetPawn()) : nullptr;
	const UUOInteractionComponent* Interaction = Character ? Character->GetInteraction() : nullptr;
	const AUOInteractableStation* Focused = Interaction ? Interaction->GetFocusedStation() : nullptr;

	// Crosshair
	const FLinearColor CrossColor = Focused ? Accent : White;
	const float Arm = 8.f * UIScale;
	DrawLine(CenterX - Arm, CenterY, CenterX + Arm, CenterY, CrossColor, 2.f);
	DrawLine(CenterX, CenterY - Arm, CenterX, CenterY + Arm, CrossColor, 2.f);

	if (!Focused || !Interaction)
	{
		return;
	}

	const FUOInteractionOption& Option = Interaction->GetCurrentOption();
	float Y = CenterY + 40.f * UIScale;

	auto DrawCentered = [this, CenterX](const FString& Text, const FLinearColor& Color, float InY, float Scale)
	{
		float W = 0.f;
		float H = 0.f;
		GetTextSize(Text, W, H, GetBodyFont(), Scale);
		DrawShadowedText(Text, Color, CenterX - W * 0.5f, InY, GetBodyFont(), Scale);
	};

	DrawCentered(Focused->GetDisplayName().ToString(), Dim, Y, 1.1f * UIScale);
	Y += 26.f * UIScale;

	if (Option.bAvailable)
	{
		const FText Prompt = Option.HoldSeconds > 0.f
			? FText::Format(LOCTEXT("HoldPrompt", "Hold E — {0}"), Option.Prompt)
			: FText::Format(LOCTEXT("PressPrompt", "Press E — {0}"), Option.Prompt);
		DrawCentered(Prompt.ToString(), White, Y, 1.3f * UIScale);
		Y += 32.f * UIScale;

		if (Interaction->IsHolding())
		{
			const float BarWidth = 240.f * UIScale;
			DrawProgressBar(CenterX - BarWidth * 0.5f, Y, BarWidth, 12.f * UIScale, Interaction->GetHoldProgress(), Accent);
		}
	}
	else if (!Option.BlockedReason.IsEmpty())
	{
		DrawCentered(Option.BlockedReason.ToString(), FLinearColor(0.75f, 0.75f, 0.78f, 0.9f), Y, 1.1f * UIScale);
	}
}

void AUOHUD::DrawCarriedItem(const AUOPlayerState* Employee)
{
	using namespace UOHUDPrivate;

	if (!Employee)
	{
		return;
	}

	const FString Text = Employee->IsCarryingItem()
		? FText::Format(LOCTEXT("Carrying", "Carrying: {0}   [Q] drop"), UOText::GetItemName(Employee->GetCarriedItem())).ToString()
		: LOCTEXT("HandsFree", "Hands free").ToString();

	float W = 0.f;
	float H = 0.f;
	GetTextSize(Text, W, H, GetBodyFont(), 1.2f * UIScale);
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = Canvas->ClipY - 60.f * UIScale;
	DrawRect(Panel, X - 12.f * UIScale, Y - 6.f * UIScale, W + 24.f * UIScale, H + 12.f * UIScale);
	DrawShadowedText(Text, Employee->IsCarryingItem() ? Accent : Dim, X, Y, GetBodyFont(), 1.2f * UIScale);

	const FString Score = FText::Format(LOCTEXT("MyScore", "Your completed sub-objectives: {0}"), FText::AsNumber(Employee->GetCompletedCount())).ToString();
	DrawShadowedText(Score, Dim, 20.f * UIScale, Canvas->ClipY - 40.f * UIScale, GetBodyFont(), 1.f * UIScale);
}

void AUOHUD::DrawActivityLog(const AUOGameState& GameState)
{
	using namespace UOHUDPrivate;

	const TArray<FUOEvidenceEntry>& Log = GameState.GetEvidenceLog();
	const int32 MaxLines = 6;
	const int32 First = FMath::Max(0, Log.Num() - MaxLines);
	if (Log.Num() == 0)
	{
		return;
	}

	const float Width = 460.f * UIScale;
	const float X = Canvas->ClipX - Width - 12.f * UIScale;
	float Y = 84.f * UIScale;
	DrawRect(Panel, X - 8.f * UIScale, Y - 6.f * UIScale, Width + 8.f * UIScale, (Log.Num() - First) * 22.f * UIScale + 36.f * UIScale);
	DrawShadowedText(LOCTEXT("ActivityHeader", "ACTIVITY LOG").ToString(), Accent, X, Y, GetBodyFont(), 1.f * UIScale);
	Y += 24.f * UIScale;

	for (int32 Index = First; Index < Log.Num(); ++Index)
	{
		const FUOEvidenceEntry& Entry = Log[Index];
		FText Line;
		switch (Entry.Action)
		{
		case EUOEvidenceAction::PickedUpItem:
			Line = FText::Format(LOCTEXT("LogPickup", "[{0}] {1} picked up {2} at {3}"), FText::FromString(Entry.ClockTime), FText::FromString(Entry.EmployeeName),
				UOText::GetItemName(Entry.Item).ToLower(), FText::FromString(GetStationName(Entry.StationId)));
			break;
		case EUOEvidenceAction::DroppedItem:
			Line = FText::Format(LOCTEXT("LogDrop", "[{0}] {1} dropped {2}"), FText::FromString(Entry.ClockTime), FText::FromString(Entry.EmployeeName),
				UOText::GetItemName(Entry.Item).ToLower());
			break;
		default:
			Line = FText::Format(LOCTEXT("LogComplete", "[{0}] {1} finished work at {2}"), FText::FromString(Entry.ClockTime), FText::FromString(Entry.EmployeeName),
				FText::FromString(GetStationName(Entry.StationId)));
			break;
		}
		DrawShadowedText(Line.ToString(), White, X, Y, GetBodyFont(), 0.9f * UIScale);
		Y += 22.f * UIScale;
	}
}

void AUOHUD::DrawOvertimeTint()
{
	const float Pulse = 0.06f + 0.04f * FMath::Abs(FMath::Sin(GetWorld()->GetTimeSeconds() * 1.5f));
	DrawRect(FLinearColor(0.6f, 0.f, 0.f, Pulse), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
}

void AUOHUD::DrawEvaluation(const AUOGameState& GameState)
{
	using namespace UOHUDPrivate;

	DrawRect(FLinearColor(0.01f, 0.01f, 0.02f, 0.88f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);

	const float Left = Canvas->ClipX * 0.5f - 560.f * UIScale;
	float Y = 70.f * UIScale;

	DrawShadowedText(FText::Format(LOCTEXT("EvalTitle", "END OF DAY {0} — PERFORMANCE REVIEW"), FText::AsNumber(GameState.GetDayNumber())).ToString(),
		Accent, Left, Y, GetBigFont(), 1.1f * UIScale);
	Y += 54.f * UIScale;
	DrawShadowedText(GameState.GetCompanyName(), White, Left, Y, GetBodyFont(), 1.4f * UIScale);
	Y += 36.f * UIScale;

	// Day summary
	const FString ClosedAt = GameState.GetClockText();
	const FText Summary = GameState.WasDayForceClosed()
		? FText::Format(LOCTEXT("ForceClosed", "Day force-closed at {0}. Pending work counts as not done."), FText::FromString(ClosedAt))
		: FText::Format(LOCTEXT("AllDone", "All work done at {0}."), FText::FromString(ClosedAt));
	DrawShadowedText(Summary.ToString(), GameState.WasDayForceClosed() ? Bad : Good, Left, Y, GetBodyFont(), 1.1f * UIScale);
	Y += 28.f * UIScale;

	if (GameState.IsUnpaidOvertime())
	{
		const float OvertimeMinutes = FUODayClock::ComputeTimeOfDayMinutes(GameState.GetShiftElapsedSeconds(), GameState.GetShiftLengthSeconds()) - FUODayClock::ShiftEndMinutes;
		DrawShadowedText(FText::Format(LOCTEXT("UnpaidOvertime", "Unpaid overtime: {0} min. Penalty flagged by HR."), FText::AsNumber(FMath::Max(0, FMath::RoundToInt(OvertimeMinutes)))).ToString(),
			Bad, Left, Y, GetBodyFont(), 1.1f * UIScale);
		Y += 28.f * UIScale;
	}

	int32 Done = 0;
	int32 Total = 0;
	for (const FUOTaskInstance& Task : GameState.GetTasks())
	{
		Done += Task.CountDone();
		Total += Task.SubObjectives.Num();
	}
	DrawShadowedText(FText::Format(LOCTEXT("SubObjectivesDone", "Sub-objectives completed: {0} / {1}"), FText::AsNumber(Done), FText::AsNumber(Total)).ToString(),
		Dim, Left, Y, GetBodyFont(), 1.1f * UIScale);
	Y += 48.f * UIScale;

	// Table
	const float ColName = Left;
	const float ColDone = Left + 380.f * UIScale;
	const float ColShare = Left + 520.f * UIScale;
	const float ColStars = Left + 660.f * UIScale;
	const float ColConsequence = Left + 800.f * UIScale;
	const float HeaderScale = 1.1f * UIScale;

	DrawShadowedText(LOCTEXT("ColEmployee", "EMPLOYEE").ToString(), Dim, ColName, Y, GetBodyFont(), HeaderScale);
	DrawShadowedText(LOCTEXT("ColDone", "DONE").ToString(), Dim, ColDone, Y, GetBodyFont(), HeaderScale);
	DrawShadowedText(LOCTEXT("ColShare", "FAIR SHARE").ToString(), Dim, ColShare, Y, GetBodyFont(), HeaderScale);
	DrawShadowedText(LOCTEXT("ColStars", "STARS").ToString(), Dim, ColStars, Y, GetBodyFont(), HeaderScale);
	DrawShadowedText(LOCTEXT("ColConsequence", "CONSEQUENCE").ToString(), Dim, ColConsequence, Y, GetBodyFont(), HeaderScale);
	Y += 30.f * UIScale;
	DrawLine(Left, Y, Left + 1120.f * UIScale, Y, Dim, 1.f);
	Y += 12.f * UIScale;

	TArray<AUOPlayerState*> Rows;
	for (AUOPlayerState* Employee : GameState.GetEmployees())
	{
		if (Employee)
		{
			Rows.Add(Employee);
		}
	}
	Rows.Sort([](const AUOPlayerState& A, const AUOPlayerState& B) { return A.GetCompletedCount() > B.GetCompletedCount(); });

	const AUOPlayerState* Me = GetLocalEmployee();
	const float RowScale = 1.3f * UIScale;
	for (const AUOPlayerState* Employee : Rows)
	{
		const FString Name = Employee == Me
			? FText::Format(LOCTEXT("YouSuffix", "{0} (you)"), FText::FromString(Employee->GetDisplayName())).ToString()
			: Employee->GetDisplayName();
		DrawShadowedText(Name, Employee->GetEmployeeColor(), ColName, Y, GetBodyFont(), RowScale);
		DrawShadowedText(FString::FromInt(Employee->GetCompletedCount()), White, ColDone, Y, GetBodyFont(), RowScale);
		DrawShadowedText(FString::Printf(TEXT("%.1f"), Employee->GetFairShare()), White, ColShare, Y, GetBodyFont(), RowScale);
		DrawStars(ColStars, Y + 14.f * UIScale, Employee->GetStars(), 12.f * UIScale);
		DrawShadowedText(FUOStarRating::GetConsequenceLabel(Employee->GetConsequence()).ToString(), GetConsequenceColor(Employee->GetConsequence()), ColConsequence, Y, GetBodyFont(), 1.1f * UIScale);
		Y += 46.f * UIScale;
	}

	Y += 30.f * UIScale;
	const FString Restart = LOCTEXT("PressR", "Press R to start a new day").ToString();
	float W = 0.f;
	float H = 0.f;
	GetTextSize(Restart, W, H, GetBigFont(), 0.9f * UIScale);
	const float Blink = 0.6f + 0.4f * FMath::Abs(FMath::Sin(GetWorld()->GetTimeSeconds() * 2.f));
	DrawShadowedText(Restart, FLinearColor(1.f, 1.f, 1.f, Blink), (Canvas->ClipX - W) * 0.5f, FMath::Max(Y, Canvas->ClipY - 90.f * UIScale), GetBigFont(), 0.9f * UIScale);
}

void AUOHUD::DrawShadowedText(const FString& Text, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale)
{
	DrawText(Text, FLinearColor(0.f, 0.f, 0.f, Color.A * 0.8f), X + 1.5f, Y + 1.5f, Font, Scale);
	DrawText(Text, Color, X, Y, Font, Scale);
}

void AUOHUD::DrawStar(float CenterX, float CenterY, float Radius, bool bFilled, const FLinearColor& Color)
{
	FVector2D Points[10];
	for (int32 Index = 0; Index < 10; ++Index)
	{
		const float Angle = FMath::DegreesToRadians(-90.f + 36.f * Index);
		const float PointRadius = (Index % 2 == 0) ? Radius : Radius * 0.45f;
		Points[Index] = FVector2D(CenterX + PointRadius * FMath::Cos(Angle), CenterY + PointRadius * FMath::Sin(Angle));
	}

	if (bFilled)
	{
		TArray<FCanvasUVTri> Triangles;
		const FVector2D Center(CenterX, CenterY);
		for (int32 Index = 0; Index < 10; ++Index)
		{
			FCanvasUVTri& Tri = Triangles.AddDefaulted_GetRef();
			Tri.V0_Pos = Center;
			Tri.V1_Pos = Points[Index];
			Tri.V2_Pos = Points[(Index + 1) % 10];
			Tri.V0_Color = Color;
			Tri.V1_Color = Color;
			Tri.V2_Color = Color;
		}
		FCanvasTriangleItem Item(Triangles, GWhiteTexture);
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	}

	for (int32 Index = 0; Index < 10; ++Index)
	{
		const FVector2D& A = Points[Index];
		const FVector2D& B = Points[(Index + 1) % 10];
		DrawLine(A.X, A.Y, B.X, B.Y, Color, 1.5f);
	}
}

void AUOHUD::DrawStars(float X, float CenterY, int32 Stars, float Radius)
{
	for (int32 Index = 0; Index < FUOStarRating::MaxStars; ++Index)
	{
		const bool bFilled = Index < Stars;
		DrawStar(X + Radius + Index * Radius * 2.6f, CenterY, Radius, bFilled, bFilled ? UOHUDPrivate::StarGold : UOHUDPrivate::StarEmpty);
	}
}

void AUOHUD::DrawProgressBar(float X, float Y, float Width, float Height, float Progress, const FLinearColor& FillColor)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X - 2.f, Y - 2.f, Width + 4.f, Height + 4.f);
	DrawRect(FillColor, X, Y, Width * FMath::Clamp(Progress, 0.f, 1.f), Height);
}

FString AUOHUD::GetStationName(FName StationId) const
{
	const AUOGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AUOGameMode>() : nullptr;
	const UUOTaskManagerComponent* TaskManager = GameMode ? GameMode->GetTaskManager() : nullptr;
	const AUOInteractableStation* Station = TaskManager ? TaskManager->FindStation(StationId) : nullptr;
	return Station ? Station->GetDisplayName().ToString() : StationId.ToString();
}

AUOPlayerState* AUOHUD::GetLocalEmployee() const
{
	const APlayerController* PC = GetOwningPlayerController();
	return PC ? PC->GetPlayerState<AUOPlayerState>() : nullptr;
}

#undef LOCTEXT_NAMESPACE
