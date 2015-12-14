﻿// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "PlainTextLayoutMarshaller.h"
#include "TextBlockLayout.h"

DECLARE_CYCLE_STAT(TEXT("STextBlock::SetText Time"), Stat_SlateTextBlockSetText, STATGROUP_SlateVerbose)
DECLARE_CYCLE_STAT(TEXT("STextBlock::OnPaint Time"), Stat_SlateTextBlockOnPaint, STATGROUP_SlateVerbose)
DECLARE_CYCLE_STAT(TEXT("STextBlock::ComputeDesiredSize"), Stat_SlateTextBlockCDS, STATGROUP_SlateVerbose)
DECLARE_CYCLE_STAT(TEXT("STextBlock::ComputeVolitility"), Stat_SlateTextBlockCV, STATGROUP_SlateVerbose)

void STextBlock::Construct( const FArguments& InArgs )
{
	TextStyle = InArgs._TextStyle;

	HighlightText = InArgs._HighlightText;
	WrapTextAt = InArgs._WrapTextAt;
	AutoWrapText = InArgs._AutoWrapText;
	Margin = InArgs._Margin;
	LineHeightPercentage = InArgs._LineHeightPercentage;
	Justification = InArgs._Justification;
	MinDesiredWidth = InArgs._MinDesiredWidth;

	Font = InArgs._Font;
	ColorAndOpacity = InArgs._ColorAndOpacity;
	ShadowOffset = InArgs._ShadowOffset;
	ShadowColorAndOpacity = InArgs._ShadowColorAndOpacity;
	HighlightColor = InArgs._HighlightColor;
	HighlightShape = InArgs._HighlightShape;

	OnDoubleClicked = InArgs._OnDoubleClicked;

	BoundText = InArgs._Text;

#if WITH_FANCY_TEXT

	// We use a dummy style here (as it may not be safe to call the delegates used to compute the style), but the correct style is set by ComputeDesiredSize
	TextLayoutCache = FTextBlockLayout::Create(FTextBlockStyle::GetDefault(), InArgs._TextShapingMethod, InArgs._TextFlowDirection, FPlainTextLayoutMarshaller::Create(), InArgs._LineBreakPolicy);

#endif//WITH_FANCY_TEXT
}

FSlateFontInfo STextBlock::GetFont() const
{
	return Font.IsSet() ? Font.Get() : TextStyle->Font;
}

FSlateColor STextBlock::GetColorAndOpacity() const
{
	return ColorAndOpacity.IsSet() ? ColorAndOpacity.Get() : TextStyle->ColorAndOpacity;
}

FVector2D STextBlock::GetShadowOffset() const
{
	return ShadowOffset.IsSet() ? ShadowOffset.Get() : TextStyle->ShadowOffset;
}

FLinearColor STextBlock::GetShadowColorAndOpacity() const
{
	return ShadowColorAndOpacity.IsSet() ? ShadowColorAndOpacity.Get() : TextStyle->ShadowColorAndOpacity;
}

FLinearColor STextBlock::GetHighlightColor() const
{
	return HighlightColor.IsSet() ? HighlightColor.Get() : TextStyle->HighlightColor;
}

const FSlateBrush* STextBlock::GetHighlightShape() const
{
	return HighlightShape.IsSet() ? HighlightShape.Get() : &TextStyle->HighlightShape;
}

void STextBlock::SetText( const TAttribute< FString >& InText )
{
	SCOPE_CYCLE_COUNTER(Stat_SlateTextBlockSetText);
	struct Local
	{
		static FText PassThroughAttribute( TAttribute< FString > InString )
		{
			return FText::FromString( InString.Get( TEXT("") ) );
		}
	};

	BoundText = TAttribute< FText >::Create(TAttribute<FText>::FGetter::CreateStatic( &Local::PassThroughAttribute, InText) );

	Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

void STextBlock::SetText( const FString& InText )
{
	SCOPE_CYCLE_COUNTER(Stat_SlateTextBlockSetText);
	BoundText = FText::FromString( InText );
	Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

void STextBlock::SetText( const TAttribute< FText >& InText )
{
	SCOPE_CYCLE_COUNTER(Stat_SlateTextBlockSetText);
	BoundText = InText;
	Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

void STextBlock::SetText( const FText& InText )
{
	SCOPE_CYCLE_COUNTER(Stat_SlateTextBlockSetText);

	if ( !BoundText.IsBound() )
	{
		const FString& OldString = BoundText.Get().ToString();
		const FString& NewString = InText.ToString();
		const int32 OldLength = OldString.Len();
		const int32 NewLength = NewString.Len();

		// Only compare reasonably sized strings, it's not worth checking this
		// for large blocks of text.
		if ( OldLength <= 20 )
		{
			if ( OldString.Compare(NewString, ESearchCase::CaseSensitive) == 0 )
			{
				return;
			}
		}
	}

	BoundText = InText;
	Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

void STextBlock::SetHighlightText(TAttribute<FText> InText)
{
	HighlightText = InText;
}

int32 STextBlock::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	SCOPE_CYCLE_COUNTER(Stat_SlateTextBlockOnPaint);

#if WITH_FANCY_TEXT

	//FPlatformMisc::BeginNamedEvent(FColor::Orange, "STextBlock");

	// OnPaint will also update the text layout cache if required
	LayerId = TextLayoutCache->OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, ShouldBeEnabled(bParentEnabled));

	//FPlatformMisc::EndNamedEvent();

#else//WITH_FANCY_TEXT

	// Simple stubbed version for when WITH_FANCY_TEXT is disabled (which should only ever be for testing purposes)
	// Just draw the text as is, with no attempt at handling wrapping or highlighting
	{
		const FSlateRect ClippingRect = AllottedGeometry.GetClippingRect().IntersectionWith(MyClippingRect);
		const ESlateDrawEffect::Type DrawEffects = ShouldBeEnabled(bParentEnabled) ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
		const FLinearColor CurShadowColor = GetShadowColorAndOpacity();
		const FVector2D CurShadowOffset = GetShadowOffset();
		const bool ShouldDropShadow = CurShadowColor.A > 0.f && CurShadowOffset.SizeSquared() > 0.f;
		const FSlateFontInfo FontInfo = GetFont();
		const FText& TextToDraw = BoundText.Get(FText::GetEmpty());

		// Draw the optional shadow
		if (ShouldDropShadow)
		{
			FSlateDrawElement::MakeText(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(CurShadowOffset, AllottedGeometry.Size),
				TextToDraw,
				FontInfo,
				ClippingRect,
				DrawEffects,
				CurShadowColor * InWidgetStyle.GetColorAndOpacityTint()
				);
		}

		// Draw the text itself
		FSlateDrawElement::MakeText(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(),
			TextToDraw,
			FontInfo,
			ClippingRect,
			DrawEffects,
			InWidgetStyle.GetColorAndOpacityTint() * GetColorAndOpacity().GetColor(InWidgetStyle)
			);
	}

#endif//WITH_FANCY_TEXT

	return LayerId;
}

FReply STextBlock::OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	if ( InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		if( OnDoubleClicked.IsBound() )
		{
			return OnDoubleClicked.Execute();
		}
	}

	return FReply::Unhandled();
}

FVector2D STextBlock::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	SCOPE_CYCLE_COUNTER(Stat_SlateTextBlockCDS);
#if WITH_FANCY_TEXT

	// ComputeDesiredSize will also update the text layout cache if required
	const FVector2D TextSize = TextLayoutCache->ComputeDesiredSize(
		FTextBlockLayout::FWidgetArgs(BoundText, HighlightText, WrapTextAt, AutoWrapText, Margin, LineHeightPercentage, Justification), 
		LayoutScaleMultiplier, GetComputedTextStyle()
		);

#else//WITH_FANCY_TEXT

	// Simple stubbed version for when WITH_FANCY_TEXT is disabled (which should only ever be for testing purposes)
	// Just measure the text as is, with no attempt at handling wrapping
	const TSharedRef<FSlateFontMeasure> FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D TextSize = FontMeasureService->Measure(BoundText.Get(FText::GetEmpty()), GetFont());

#endif//WITH_FANCY_TEXT

	return FVector2D(FMath::Max(MinDesiredWidth.Get(0.0f), TextSize.X), TextSize.Y);
}

bool STextBlock::ComputeVolatility() const
{
	SCOPE_CYCLE_COUNTER(Stat_SlateTextBlockCV);
	return SLeafWidget::ComputeVolatility() || BoundText.IsBound();
}

void STextBlock::SetFont(const TAttribute< FSlateFontInfo >& InFont)
{
	Font = InFont;
	Invalidate(EInvalidateWidget::Layout);
}

void STextBlock::SetColorAndOpacity(const TAttribute<FSlateColor>& InColorAndOpacity)
{
	if ( !ColorAndOpacity.IdenticalTo(InColorAndOpacity) )
	{
		ColorAndOpacity = InColorAndOpacity;
		Invalidate(EInvalidateWidget::Layout);
	}
}

void STextBlock::SetTextStyle(const FTextBlockStyle* InTextStyle)
{
	TextStyle = InTextStyle;
	Invalidate(EInvalidateWidget::Layout);
}

void STextBlock::SetTextShapingMethod(const TOptional<ETextShapingMethod>& InTextShapingMethod)
{
	TextLayoutCache->SetTextShapingMethod(InTextShapingMethod);
	Invalidate(EInvalidateWidget::Layout);
}

void STextBlock::SetTextFlowDirection(const TOptional<ETextFlowDirection>& InTextFlowDirection)
{
	TextLayoutCache->SetTextFlowDirection(InTextFlowDirection);
	Invalidate(EInvalidateWidget::Layout);
}

void STextBlock::SetWrapTextAt(const TAttribute<float>& InWrapTextAt)
{
	WrapTextAt = InWrapTextAt;
	Invalidate(EInvalidateWidget::Layout);
}

void STextBlock::SetAutoWrapText(const TAttribute<bool>& InAutoWrapText)
{
	AutoWrapText = InAutoWrapText;
	Invalidate(EInvalidateWidget::Layout);
}

void STextBlock::SetShadowOffset(const TAttribute<FVector2D>& InShadowOffset)
{
	ShadowOffset = InShadowOffset;
	Invalidate(EInvalidateWidget::Layout);
}

void STextBlock::SetShadowColorAndOpacity(const TAttribute<FLinearColor>& InShadowColorAndOpacity)
{
	ShadowColorAndOpacity = InShadowColorAndOpacity;
	Invalidate(EInvalidateWidget::Layout);
}

void STextBlock::SetMinDesiredWidth(const TAttribute<float>& InMinDesiredWidth)
{
	MinDesiredWidth = InMinDesiredWidth;
	Invalidate(EInvalidateWidget::Layout);
}

void STextBlock::SetLineHeightPercentage(const TAttribute<float>& InLineHeightPercentage)
{
	LineHeightPercentage = InLineHeightPercentage;
	Invalidate(EInvalidateWidget::Layout);
}

void STextBlock::SetMargin(const TAttribute<FMargin>& InMargin)
{
	Margin = InMargin;
	Invalidate(EInvalidateWidget::Layout);
}

void STextBlock::SetJustification(const TAttribute<ETextJustify::Type>& InJustification)
{
	Justification = InJustification;
	Invalidate(EInvalidateWidget::Layout);
}

FTextBlockStyle STextBlock::GetComputedTextStyle() const
{
	FTextBlockStyle ComputedStyle = *TextStyle;
	ComputedStyle.SetFont( GetFont() );
	ComputedStyle.SetColorAndOpacity( GetColorAndOpacity() );
	ComputedStyle.SetShadowOffset( GetShadowOffset() );
	ComputedStyle.SetShadowColorAndOpacity( GetShadowColorAndOpacity() );
	ComputedStyle.SetHighlightColor( GetHighlightColor() );
	ComputedStyle.SetHighlightShape( *GetHighlightShape() );
	return ComputedStyle;
}
