#ifndef __SG_URLView__
#define __SG_URLView__

#include <View.h>
#include <String.h>

typedef enum
{
	WWW_LINK	= 0,
	MAIL_LINK	= 1
}URLType;

class SG_URLView : public BView
{
public:
	
	SG_URLView(BRect frame, const char* label, const char* url, URLType type);
	virtual ~SG_URLView();
	
	virtual	void	Draw(BRect updateRect);
	virtual	void	MouseDown(BPoint point);
	virtual	void	MouseMoved(BPoint point, uint32 transit,const BMessage *message);

private:
	
	bool		mHovering;
	BString		mLabel;
	BString		mURL;
	URLType		mType;
};

#endif //__SG_URLView__