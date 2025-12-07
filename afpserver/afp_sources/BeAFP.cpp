#include "afpGlobals.h"
#include "afpServerApplication.h"

int afpAppReturnValue(0);

/*
 * main()
 *
 * Description:
 *
 * Returns:
 */

int main(int, char **)
{
	afpAppReturnValue = B_ERROR;
				
	new afpServerApplication();
	
	if (be_app != NULL)
	{
		afpAppReturnValue = B_OK;
		be_app->Run();
	}
	else
	{
		afpAppReturnValue = B_NO_MEMORY;
	}
	
	delete be_app;
	
	return( afpAppReturnValue );
}