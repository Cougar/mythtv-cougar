#include <qapplication.h>
#include <qsqldatabase.h>
#include <cstdlib>

#include "libmyth/mythcontext.h"
#include "libmyth/mythdbcon.h"
#include "libmyth/settings.h"

#include "libmythtv/progfind.h"
#include "libmythtv/tv.h"

int main(int argc, char **argv)
{
    QApplication a(argc, argv);

    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if(!gContext->Init(false))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return -1;
    }

    gContext->LoadQtConfig();

    MythMainWindow *mainWindow = new MythMainWindow();
    mainWindow->Show();
    gContext->SetMainWindow(mainWindow);
    
    TV::InitKeys();

    RunProgramFind();

    delete gContext;

    return 0;
}
