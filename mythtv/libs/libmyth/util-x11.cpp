/** This file is intended to hold X11 specific utility functions */
#include <map>
#include <vector>
#include <qglobal.h> // for Q_WS_X11 define

#include "config.h" // for CONFIG_DARWIN
#include "mythcontext.h"

QMutex x11_lock;

#ifdef Q_WS_X11
#define USING_XV
#include "util-x11.h"
extern "C" {
#include <X11/extensions/Xinerama.h>
}
typedef int (*XErrorCallbackType)(Display *, XErrorEvent *);
typedef vector<XErrorEvent>       XErrorVectorType;
#else
#include <qapplication.h>
#endif // Q_WS_X11

#ifdef Q_WS_X11
map<Display*, XErrorVectorType>   error_map;
map<Display*, XErrorCallbackType> error_handler_map;
#endif // Q_WS_X11

/** \fn GetNumberOfXineramaScreens(void)
 *  \brief Returns number of Xinerama screens if Xinerama
 *         is available, or 0 if it is not available.
 */
int GetNumberOfXineramaScreens(void)
{
    int nr_xinerama_screens = 0;

#ifdef Q_WS_X11
    Display *d = MythXOpenDisplay();
    X11L;
    int event_base = 0, error_base = 0;
    if (XineramaQueryExtension(d, &event_base, &error_base) &&
        XineramaIsActive(d))
        XFree(XineramaQueryScreens(d, &nr_xinerama_screens));
    XCloseDisplay(d);
    X11U;
#else // if !Q_WS_X11
#if CONFIG_DARWIN
    // Mac OS X when not using X11 server supports Xinerama.
    nr_xinerama_screens = QApplication::desktop()->numScreens();
#endif // CONFIG_DARWIN
#endif // !Q_WS_X11
    return nr_xinerama_screens;
}

#ifdef Q_WS_X11
Display *MythXOpenDisplay(void)
{
    QString dispStr = MythContext::GetX11Display();
    const char *dispCStr = NULL;
    if (!dispStr.isEmpty())
        dispCStr = dispStr.ascii();

    X11L;
    Display *disp = XOpenDisplay(dispCStr);
    X11U;

    if (!disp)
        VERBOSE(VB_IMPORTANT, "MythXOpenDisplay() failed");

    return disp;
}
#endif

// Everything below this line is only compiled if using X11

#ifdef Q_WS_X11
int ErrorCatcher(Display * d, XErrorEvent * xeev)
{
    error_map[d].push_back(*xeev);
    return 0;
}

void InstallXErrorHandler(Display *d)
{
    XErrorVectorType empty;
    error_map[d] = empty;
    X11L;
    XSync(d, 0); /* flush out any pre-existing X errors */
    error_handler_map[d] = XSetErrorHandler(ErrorCatcher);
    X11U;
}

void PrintXErrors(Display *d, const vector<XErrorEvent>& events)
{
    for (int i = events.size() -1; i>=0; --i)
    {
        char buf[200];
        X11S(XGetErrorText(d, events[i].error_code, buf, sizeof(buf)));
	VERBOSE(VB_IMPORTANT, endl
                <<"XError type: "<<events[i].type<<endl
                <<"    display: "<<events[i].display<<endl
                <<"  serial no: "<<events[i].serial<<endl
                <<"   err code: "<<events[i].error_code<<" ("<<buf<<")"<<endl
                <<"   req code: "<<events[i].request_code<<endl
                <<" minor code: "<<events[i].minor_code<<endl
                <<"resource id: "<<events[i].resourceid);
    }
}

vector<XErrorEvent> UninstallXErrorHandler(Display *d, bool printErrors)
{
    vector<XErrorEvent> events;
    X11L;
    XErrorCallbackType old_handler = error_handler_map[d];
    XSync(d, 0); /* flush pending X calls so we see any errors */
    X11U;
    if (old_handler)
    {
        error_handler_map[d] = NULL;
        X11S(XSetErrorHandler(old_handler));
        events = error_map[d];
        error_map[d].clear();
        if (printErrors)
            PrintXErrors(d, events);
    }
    else
    {
        VERBOSE(VB_IMPORTANT,
                "ErrorHandler uninstalled more often than installed");
    }
    return events;
}
#endif // Q_WS_X11
