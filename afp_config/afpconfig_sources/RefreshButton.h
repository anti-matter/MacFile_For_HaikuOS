#include <Button.h>
#include <InterfaceDefs.h>

#include <algorithm>


class RefreshButton : public BButton {
public:
	RefreshButton(BRect frame, const char* name, BMessage* message)
		:
		BButton(frame, name, "", message)
	{
		SetExplicitMinSize(BSize(24.0f, 24.0f));
		SetExplicitPreferredSize(BSize(24.0f, 24.0f));
	}

	void Draw(BRect updateRect) override
	{
		// Draw the normal Haiku button background, border, focus indicator, etc.
		BButton::Draw(updateRect);

		BRect bounds = Bounds();

		// Use a 14x14 icon, or shrink it for unusually small buttons.
		float iconSize = std::min(14.0f,
			std::min(bounds.Width() - 8.0f, bounds.Height() - 8.0f));

		if (iconSize < 8.0f)
			return;

		float left = bounds.left
			+ (bounds.Width() - iconSize) / 2.0f;
		float top = bounds.top
			+ (bounds.Height() - iconSize) / 2.0f;

		BRect iconRect(left, top, left + iconSize, top + iconSize);

		// Match Haiku's normal pressed-button content movement.
		if (Value() == B_CONTROL_ON)
			iconRect.OffsetBy(1.0f, 1.0f);

		rgb_color iconColor = ui_color(B_CONTROL_TEXT_COLOR);

		if (!IsEnabled()) {
			iconColor = tint_color(
				ui_color(B_PANEL_BACKGROUND_COLOR),
				B_DISABLED_LABEL_TINT);
		}

		SetHighColor(iconColor);
		SetDrawingMode(B_OP_ALPHA);
		SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
		SetPenSize(1.0f);
        SetLineMode(B_ROUND_CAP, B_ROUND_JOIN, 10.0f);

        BRect arcRect = iconRect;
        arcRect.InsetBy(1.5f, 1.5f);

        StrokeArc(arcRect, 35.0f, 285.0f);

        /*
		 * Arrowhead at the upper-right end of the arc. It points
		 * downward, indicating clockwise rotation.
		 */
		BPoint tip(
			arcRect.right + 1.0f,
			arcRect.top + 4.0f);

		BPoint arrow[] = {
			tip,
			BPoint(tip.x - 4.5f, tip.y - 1.0f),
			BPoint(tip.x - 1.0f, tip.y + 4.5f)
		};

        FillPolygon(arrow, 3);

		SetPenSize(1.0f);
		SetDrawingMode(B_OP_COPY);
	}
};