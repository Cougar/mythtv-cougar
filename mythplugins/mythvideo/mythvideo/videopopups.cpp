#include <QStringList>

#include <mythtv/libmythdb/mythverbose.h>

#include <mythtv/libmythui/mythuibuttonlist.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythuibutton.h>

#include "metadata.h"
#include "videoutils.h"
#include "videopopups.h"

CastDialog::CastDialog(MythScreenStack *lparent, Metadata *metadata) :
    MythScreenType(lparent, "videocastpopup"), m_metadata(metadata)
{
}

bool CastDialog::Create()
{
    if (!LoadWindowFromXML("video-ui.xml", "castpopup", this))
        return false;

    MythUIButtonList *castList;
    MythUIButton *okButton;

    bool err = false;
    UIUtilE::Assign(this, castList, "cast", &err);
    UIUtilE::Assign(this, okButton, "ok", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'castpopup'");
        return false;
    }

    connect(okButton, SIGNAL(Clicked()), SLOT(Close()));

    okButton->SetText(tr("OK"));

    QStringList cast = GetCastList(*m_metadata);
    QStringListIterator castIterator(cast);
    while (castIterator.hasNext())
    {
        new MythUIButtonListItem(castList, castIterator.next());
    }

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist.");

    return true;
}

/////////////////////////////////////////////////////////////

PlotDialog::PlotDialog(MythScreenStack *lparent, Metadata *metadata) :
    MythScreenType(lparent, "videoplotpopup"), m_metadata(metadata)
{
}

bool PlotDialog::Create()
{
    if (!LoadWindowFromXML("video-ui.xml", "plotpopup", this))
        return false;

    MythUIText *plotText;
    MythUIButton *okButton;

    bool err = false;
    UIUtilE::Assign(this, plotText, "plot", &err);
    UIUtilE::Assign(this, okButton, "ok", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'plotpopup'");
        return false;
    }

    plotText->SetText(m_metadata->Plot());
    okButton->SetText(tr("OK"));

    connect(okButton, SIGNAL(Clicked()), SLOT(Close()));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist.");

    return true;
}
