// UNPAID: OVERTIME - small vector icons drawn with Slate lines (no textures, no font glyph dependencies).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

enum class EUOIconType : uint8
{
	Check,
	StarFilled,
	StarEmpty,
	/** Circle outline; Progress (0..1) draws a clockwise arc over a faint full ring */
	Ring,
	/** Small hollow circle used for pending items */
	Circle,
	/** Promotion shield (filled) */
	Shield,
	/** Wrong answer / fired mark */
	Cross,
	/** Rock-paper-scissors picks */
	Rock,
	Paper,
	Scissors,
	/** Voice chat indicator (M7 hook) */
	Mic,
	MicOff
};

class SUOIcon : public SLeafWidget
{
public:

	SLATE_BEGIN_ARGS(SUOIcon)
		: _Type(EUOIconType::Check)
		, _Size(16.f)
		, _Color(FLinearColor::White)
		, _Progress(1.f)
		, _Thickness(2.f)
	{}
		SLATE_ARGUMENT(EUOIconType, Type)
		SLATE_ARGUMENT(float, Size)
		SLATE_ATTRIBUTE(FLinearColor, Color)
		SLATE_ATTRIBUTE(float, Progress)
		SLATE_ARGUMENT(float, Thickness)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:

	EUOIconType Type = EUOIconType::Check;
	float Size = 16.f;
	float Thickness = 2.f;
	TAttribute<FLinearColor> Color;
	TAttribute<float> Progress;

	void PaintStar(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FLinearColor& Tint, bool bFilled) const;
	/** Outline (and optional scanline fill) of a closed polygon in local space */
	void PaintPolygon(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FLinearColor& Tint, const TArray<FVector2f>& Polygon, bool bFilled) const;
	void PaintArc(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FLinearColor& Tint, float Fraction, float Radius, float LineThickness) const;
};
