#include <stdio.h>
#include <Roster.h>

#include "SG_URLView.h"

/*
 * SG_URLView()
 *
 * Description:
 *		Initialize the URL view class.
 *
 * Returns:
 */

SG_URLView::SG_URLView(BRect frame, const char* label, const char* url, URLType type) :
				BView(
					frame,
					label,
					B_FOLLOW_LEFT_RIGHT,
					B_WILL_DRAW | B_FRAME_EVENTS
					)
{
	mLabel		= label;
	mURL		= url;
	mType		= type;
	mHovering	= false;
	
	//
	//If we're email, make sure the "mailto:" part is prepended.
	//
	if (mType == MAIL_LINK && mURL.FindFirst("mailto:") == B_ERROR)
	{
		//
		//Didn't find the text, so prepend it.
		//
		mURL.Prepend("mailto:");
	}
	
	SetViewColor(255,255,255);
}


/*
 * ~SG_URLView()
 *
 * Description:
 *		Destructor.
 *
 * Returns:
 */

SG_URLView::~SG_URLView()
{
}


/*
 * MouseDown()
 *
 * Description:
 *		Handle a mouse down within our view. Do what we're supposed to do, either
 *		open a browser window with the http link or launch the mail app.
 *
 * Returns: none
 */

void SG_URLView::MouseDown(BPoint point)
{
	char	*url;

	url = mURL.LockBuffer(0);
	
	switch(mType)
	{
	case WWW_LINK:
		be_roster->Launch("text/html", 1, &url);
		break;
		
	case MAIL_LINK:
		be_roster->Launch("text/x-email", 1, &url);
		break;
		
	default:
		break;
	}

	mURL.UnlockBuffer();
}


/*
 * MouseMoved()
 *
 * Description:
 *		When the user floats on top of the URL, we want to give an indication
 *		that we are a live link.
 *
 * Returns: none
 */

void SG_URLView::MouseMoved(BPoint point, uint32 transit, const BMessage *message)
{
	switch(transit)
	{
		case B_ENTERED_VIEW:
			mHovering = true;
			Invalidate();
			break;
			
		case B_EXITED_VIEW:
			mHovering = false;
			Invalidate();
			break;
			
		case B_INSIDE_VIEW:
			if(!mHovering)
				Invalidate();
			break;
		
		default:
			break;
	}
}


/*
 * Draw()
 *
 * Description:
 *		Draw the link as appropriate taking into account whether or
 *		not the point is hovering over the link.
 *
 * Returns: none
 */

void SG_URLView::Draw(BRect updateRect)
{
	BRect	bounds = Bounds();

	SetHighColor(ViewColor());
	FillRect(bounds);

	SetLowColor(ViewColor());
	SetHighColor(50,50,170);
		
	if(mHovering)
		SetHighColor(200,0,100);	

	DrawString(mLabel.String(),BPoint(0,10));
}


