// UNPAID: OVERTIME - small vector icons.

#include "SUOIcon.h"
#include "Rendering/DrawElements.h"

void SUOIcon::Construct(const FArguments& InArgs)
{
	Type = InArgs._Type;
	Size = InArgs._Size;
	Thickness = InArgs._Thickness;
	Color = InArgs._Color;
	Progress = InArgs._Progress;
}

FVector2D SUOIcon::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(Size, Size);
}

int32 SUOIcon::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	FLinearColor Tint = Color.Get() * InWidgetStyle.GetColorAndOpacityTint();
	const FVector2f Local = AllottedGeometry.GetLocalSize();
	const float S = FMath::Min(Local.X, Local.Y);

	switch (Type)
	{
	case EUOIconType::Check:
	{
		TArray<FVector2f> Points = { FVector2f(0.16f, 0.54f) * S, FVector2f(0.4f, 0.78f) * S, FVector2f(0.86f, 0.24f) * S };
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Tint, true, Thickness);
		break;
	}

	case EUOIconType::StarFilled:
	case EUOIconType::StarEmpty:
		PaintStar(AllottedGeometry, OutDrawElements, LayerId, Tint, Type == EUOIconType::StarFilled);
		break;

	case EUOIconType::Ring:
	{
		const float Radius = S * 0.5f - Thickness;
		FLinearColor Faint = Tint;
		Faint.A *= 0.18f;
		PaintArc(AllottedGeometry, OutDrawElements, LayerId, Faint, 1.f, Radius, Thickness);
		PaintArc(AllottedGeometry, OutDrawElements, LayerId + 1, Tint, FMath::Clamp(Progress.Get(), 0.f, 1.f), Radius, Thickness);
		return LayerId + 1;
	}

	case EUOIconType::Circle:
		PaintArc(AllottedGeometry, OutDrawElements, LayerId, Tint, 1.f, S * 0.5f - Thickness, Thickness);
		break;

	case EUOIconType::Shield:
	{
		TArray<FVector2f> Polygon;
		const FVector2f Offset((Local.X - S) * 0.5f, (Local.Y - S) * 0.5f);
		for (const FVector2f& P : { FVector2f(0.5f, 0.06f), FVector2f(0.88f, 0.2f), FVector2f(0.84f, 0.56f), FVector2f(0.5f, 0.94f), FVector2f(0.16f, 0.56f), FVector2f(0.12f, 0.2f) })
		{
			Polygon.Add(Offset + P * S);
		}
		PaintPolygon(AllottedGeometry, OutDrawElements, LayerId, Tint, Polygon, true);
		break;
	}

	case EUOIconType::Cross:
	{
		TArray<FVector2f> A = { FVector2f(0.22f, 0.22f) * S, FVector2f(0.78f, 0.78f) * S };
		TArray<FVector2f> B = { FVector2f(0.78f, 0.22f) * S, FVector2f(0.22f, 0.78f) * S };
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), A, ESlateDrawEffect::None, Tint, true, Thickness);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), B, ESlateDrawEffect::None, Tint, true, Thickness);
		break;
	}

	case EUOIconType::Rock:
	{
		// A lumpy boulder
		TArray<FVector2f> Polygon;
		const FVector2f Offset((Local.X - S) * 0.5f, (Local.Y - S) * 0.5f);
		for (const FVector2f& P : { FVector2f(0.22f, 0.42f), FVector2f(0.36f, 0.24f), FVector2f(0.58f, 0.2f), FVector2f(0.8f, 0.34f), FVector2f(0.88f, 0.58f),
			FVector2f(0.74f, 0.8f), FVector2f(0.46f, 0.86f), FVector2f(0.2f, 0.74f), FVector2f(0.12f, 0.56f) })
		{
			Polygon.Add(Offset + P * S);
		}
		PaintPolygon(AllottedGeometry, OutDrawElements, LayerId, Tint, Polygon, true);
		break;
	}

	case EUOIconType::Paper:
	{
		// A sheet with a folded corner and text lines
		const FVector2f O((Local.X - S) * 0.5f, (Local.Y - S) * 0.5f);
		TArray<FVector2f> Sheet = { O + FVector2f(0.22f, 0.1f) * S, O + FVector2f(0.64f, 0.1f) * S, O + FVector2f(0.8f, 0.26f) * S, O + FVector2f(0.8f, 0.9f) * S,
			O + FVector2f(0.22f, 0.9f) * S, O + FVector2f(0.22f, 0.1f) * S };
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Sheet, ESlateDrawEffect::None, Tint, true, Thickness);
		for (const float Y : { 0.4f, 0.54f, 0.68f })
		{
			TArray<FVector2f> Line = { O + FVector2f(0.32f, Y) * S, O + FVector2f(0.7f, Y) * S };
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Line, ESlateDrawEffect::None, Tint, true, FMath::Max(1.f, Thickness * 0.6f));
		}
		break;
	}

	case EUOIconType::Scissors:
	{
		// Two finger rings and two crossed blades
		const FVector2f O((Local.X - S) * 0.5f, (Local.Y - S) * 0.5f);
		auto Ring = [&](FVector2f Center, float Radius)
		{
			TArray<FVector2f> Points;
			for (int32 Index = 0; Index <= 20; ++Index)
			{
				const float Angle = 2.f * PI * Index / 20.f;
				Points.Add(O + (Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius) * S);
			}
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Tint, true, Thickness);
		};
		Ring(FVector2f(0.3f, 0.76f), 0.13f);
		Ring(FVector2f(0.7f, 0.76f), 0.13f);
		TArray<FVector2f> BladeA = { O + FVector2f(0.38f, 0.65f) * S, O + FVector2f(0.76f, 0.1f) * S };
		TArray<FVector2f> BladeB = { O + FVector2f(0.62f, 0.65f) * S, O + FVector2f(0.24f, 0.1f) * S };
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), BladeA, ESlateDrawEffect::None, Tint, true, Thickness * 1.2f);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), BladeB, ESlateDrawEffect::None, Tint, true, Thickness * 1.2f);
		break;
	}

	case EUOIconType::Mic:
	case EUOIconType::MicOff:
	{
		const FVector2f O((Local.X - S) * 0.5f, (Local.Y - S) * 0.5f);
		TArray<FVector2f> Capsule;
		for (int32 Index = 0; Index <= 10; ++Index)
		{
			const float Angle = PI + PI * Index / 10.f;
			Capsule.Add(O + (FVector2f(0.5f, 0.3f) + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * 0.14f) * S);
		}
		for (int32 Index = 0; Index <= 10; ++Index)
		{
			const float Angle = PI * Index / 10.f;
			Capsule.Add(O + (FVector2f(0.5f, 0.5f) + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * 0.14f) * S);
		}
		const FVector2f First = Capsule[0];
		Capsule.Add(First);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Capsule, ESlateDrawEffect::None, Tint, true, Thickness);
		TArray<FVector2f> Cup;
		for (int32 Index = 0; Index <= 12; ++Index)
		{
			const float Angle = PI * Index / 12.f;
			Cup.Add(O + (FVector2f(0.5f, 0.5f) + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * 0.26f) * S);
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Cup, ESlateDrawEffect::None, Tint, true, Thickness);
		TArray<FVector2f> Stand = { O + FVector2f(0.5f, 0.76f) * S, O + FVector2f(0.5f, 0.9f) * S };
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Stand, ESlateDrawEffect::None, Tint, true, Thickness);
		if (Type == EUOIconType::MicOff)
		{
			TArray<FVector2f> Slash = { O + FVector2f(0.16f, 0.12f) * S, O + FVector2f(0.84f, 0.9f) * S };
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Slash, ESlateDrawEffect::None, Tint, true, Thickness * 1.3f);
		}
		break;
	}
	}
	return LayerId;
}

void SUOIcon::PaintStar(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FLinearColor& Tint, bool bFilled) const
{
	const FVector2f Local = Geometry.GetLocalSize();
	const float S = FMath::Min(Local.X, Local.Y);
	const FVector2f Center(Local.X * 0.5f, Local.Y * 0.53f);
	const float Outer = S * 0.5f;
	const float Inner = Outer * 0.45f;

	TArray<FVector2f> Polygon;
	for (int32 Index = 0; Index < 10; ++Index)
	{
		const float Angle = FMath::DegreesToRadians(-90.f + 36.f * Index);
		const float Radius = (Index % 2 == 0) ? Outer : Inner;
		Polygon.Add(Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
	}

	PaintPolygon(Geometry, OutDrawElements, LayerId, Tint, Polygon, bFilled);
}

void SUOIcon::PaintPolygon(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FLinearColor& Tint, const TArray<FVector2f>& Polygon, bool bFilled) const
{
	if (bFilled)
	{
		// Scanline fill: one horizontal line per pixel row between polygon edge crossings
		float MinY = Polygon[0].Y;
		float MaxY = Polygon[0].Y;
		for (const FVector2f& P : Polygon)
		{
			MinY = FMath::Min(MinY, P.Y);
			MaxY = FMath::Max(MaxY, P.Y);
		}
		for (float Y = MinY + 0.5f; Y < MaxY; Y += 1.f)
		{
			TArray<float, TInlineAllocator<8>> Crossings;
			for (int32 Index = 0; Index < Polygon.Num(); ++Index)
			{
				const FVector2f& A = Polygon[Index];
				const FVector2f& B = Polygon[(Index + 1) % Polygon.Num()];
				if ((A.Y <= Y && B.Y > Y) || (B.Y <= Y && A.Y > Y))
				{
					Crossings.Add(A.X + (Y - A.Y) / (B.Y - A.Y) * (B.X - A.X));
				}
			}
			Crossings.Sort();
			for (int32 Index = 0; Index + 1 < Crossings.Num(); Index += 2)
			{
				TArray<FVector2f> Row = { FVector2f(Crossings[Index], Y), FVector2f(Crossings[Index + 1], Y) };
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geometry.ToPaintGeometry(), Row, ESlateDrawEffect::None, Tint, false, 1.2f);
			}
		}
	}

	TArray<FVector2f> Outline = Polygon;
	Outline.Add(Polygon[0]);
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geometry.ToPaintGeometry(), Outline, ESlateDrawEffect::None, Tint, true, 1.2f);
}

void SUOIcon::PaintArc(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FLinearColor& Tint, float Fraction, float Radius, float LineThickness) const
{
	if (Fraction <= 0.f)
	{
		return;
	}

	const FVector2f Local = Geometry.GetLocalSize();
	const FVector2f Center = Local * 0.5f;
	const int32 Segments = FMath::Max(4, FMath::CeilToInt(48.f * Fraction));

	TArray<FVector2f> Points;
	for (int32 Index = 0; Index <= Segments; ++Index)
	{
		const float Angle = FMath::DegreesToRadians(-90.f + 360.f * Fraction * Index / Segments);
		Points.Add(Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
	}
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Tint, true, LineThickness);
}
