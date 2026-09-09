#include "InstallerApp.h"

/*
 * main()
 *
 * Description:
 *
 * Returns:
 */

int main(int, char **)
{
	InstallerApp* app = NULL;

	app = new InstallerApp();
	app->Run();

	delete be_app;

	return( 0 );
}
