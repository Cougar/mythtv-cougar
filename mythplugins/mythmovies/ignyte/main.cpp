#include <qapplication.h>
#include "ignytegrabber.h"

using namespace std;

int main(int argc, char **argv)
{
    QString zip;
    QString radius;
    for(int i = 1; i + 1 < argc; ++i)
    {
        if (!strcmp(argv[i], "--zip"))
        {
            zip = argv[i + 1];
        }
        if (!strcmp(argv[i], "--radius"))
        {
            radius = argv[i + 1];
        }
    }

    QApplication app(argc, argv, false);
    IgnyteGrabber *grab;
    grab = new IgnyteGrabber(zip, radius, &app);
    return app.exec();
}


