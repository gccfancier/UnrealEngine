// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class SSequencerTreeViewRow;
class SSequencerTreeView;
class SSequencerTrackLane;
class FSequencerTimeSliderController;
class FSequencer;

#include "SequencerInputHandlerStack.h"
#include "ISequencerEditTool.h"

/**
 * Structure representing a slot in the track area.
 */
class FTrackAreaSlot
	: public TSlotBase<FTrackAreaSlot>
{
public:

	/** Construction from a track lane */
	FTrackAreaSlot(const TSharedPtr<SSequencerTrackLane>& InSlotContent);

	/** Get the vertical position of this slot inside its parent. */
	float GetVerticalOffset() const;

	/** Horizontal/Vertical alignment for the slot. */
	EHorizontalAlignment HAlignment;
	EVerticalAlignment VAlignment;

	/** The track lane that we represent. */
	TWeakPtr<SSequencerTrackLane> TrackLane;
};


/**
 * The area where tracks( rows of sections ) are displayed.
 */
class SSequencerTrackArea
	: public SPanel
{
public:

	SLATE_BEGIN_ARGS( SSequencerTrackArea )
	{ }
	SLATE_END_ARGS()

	/** Construct this widget */
	void Construct(const FArguments& InArgs, TSharedRef<FSequencerTimeSliderController> InTimeSliderController, TSharedRef<FSequencer> InSequencer);

public:

	/** Empty the track area */
	void Empty();

	/** Add a new track slot to this area for the given node. The slot will be automatically cleaned up when all external references to the supplied slot are removed. */
	void AddTrackSlot(const TSharedRef<FSequencerDisplayNode>& InNode, const TSharedPtr<SSequencerTrackLane>& InSlot);

	/** Attempt to find an existing slot relating to the given node */
	TSharedPtr<SSequencerTrackLane> FindTrackSlot(const TSharedRef<FSequencerDisplayNode>& InNode);

	/** Access the cached geometry for this track area */
	const FGeometry& GetCachedGeometry() const
	{
		return CachedGeometry;
	}

	/** Assign a tree view to this track area. */
	void SetTreeView(const TSharedPtr<SSequencerTreeView>& InTreeView);

	/** Access the currently active track area edit tool */
	const ISequencerEditTool* GetEditTool() const { return EditTool.IsValid() ? EditTool.Get() : nullptr; }

public:

	// SWidget interface

	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual void OnMouseCaptureLost() override;
	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FChildren* GetChildren() override;

protected:

	/** Check whether it's possible to activate the specified tool */
	bool CanActivateEditTool(FName Identifier) const;

	/** Attempt to activate the tool specified by the template parameter */
	template<typename EditToolType>
	bool AttemptToActivateTool();

	/** Update any hover state required for the track area */
	void UpdateHoverStates( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent );

private:

	/** The track area's children. */
	TPanelChildren<FTrackAreaSlot> Children;

private:

	/** Cached geometry. */
	FGeometry CachedGeometry;

	/** A map of child slot content that exist in our view. */
	TMap<TSharedPtr<FSequencerDisplayNode>, TWeakPtr<SSequencerTrackLane>> TrackSlots;

	/** Weak pointer to the sequencer widget. */
	TWeakPtr<FSequencer> Sequencer;

	/** Weak pointer to the tree view (used for scrolling interactions). */
	TWeakPtr<SSequencerTreeView> TreeView;

	/** Time slider controller for controlling zoom/pan etc. */
	TSharedPtr<FSequencerTimeSliderController> TimeSliderController;

	/** Keep a hold of the size of the area so we can maintain zoom levels. */
	TOptional<FVector2D> SizeLastFrame;

	/** The currently active edit tool on this track area */
	TUniquePtr<ISequencerEditTool> EditTool;

private:

	/** Input handler stack responsible for routing input to the different handlers */
	FSequencerInputHandlerStack InputStack;
};
