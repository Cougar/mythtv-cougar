/*
        MythProgramFind
	January 19th, 2003
        Updated: 4/8/2003, John Danner
                 Updated code to use new ui.xml file.

        By John Danner
*/

#include <qlabel.h>
#include <qaccel.h>
#include <qlayout.h>
#include <qdatetime.h>
#include <qtimer.h>
#include <qapplication.h>
#include <qsqldatabase.h>
#include <qstringlist.h> 
#include <qcursor.h>
#include <qregexp.h>
#include <unistd.h>
#include <time.h>

#include "progfind.h"
#include "infodialog.h"
#include "infostructs.h"
#include "programinfo.h"
#include "oldsettings.h"
#include "tv.h"

using namespace std;

#include "libmyth/mythcontext.h"


void RunProgramFind(bool thread)
{
    if (thread)
        qApp->lock();

    ProgFinder programFind;

    if (thread)
    {
        programFind.show();
        qApp->unlock();

        while (programFind.isVisible())
            usleep(50);
    }
    else
        programFind.exec();

    return;
}


ProgFinder::ProgFinder(QWidget *parent, const char *name)
           : MythDialog(parent, name)
{
    running = true;
    m_db = QSqlDatabase::database();

    baseDir = gContext->GetInstallPrefix();

    accel = new QAccel(this);
    accel->connectItem(accel->insertItem(Key_Left), this, SLOT(cursorLeft()));
    accel->connectItem(accel->insertItem(Key_Right), this, SLOT(cursorRight()));
    accel->connectItem(accel->insertItem(Key_Enter), this, SLOT(cursorRight()));
    accel->connectItem(accel->insertItem(Key_Return), this, SLOT(cursorRight()));
    accel->connectItem(accel->insertItem(Key_Space), this, SLOT(cursorRight()));
    accel->connectItem(accel->insertItem(Key_Up), this, SLOT(cursorUp()));
    accel->connectItem(accel->insertItem(Key_Down), this, SLOT(cursorDown()));
    accel->connectItem(accel->insertItem(Key_PageUp), this, SLOT(pageUp()));
    accel->connectItem(accel->insertItem(Key_PageDown), this, SLOT(pageDown()));
    accel->connectItem(accel->insertItem(Key_3), this, SLOT(pageUp()));
    accel->connectItem(accel->insertItem(Key_9), this, SLOT(pageDown()));
    accel->connectItem(accel->insertItem(Key_I), this, SLOT(getInfo()));
    accel->connectItem(accel->insertItem(Key_4), this, SLOT(showGuide()));
    accel->connectItem(accel->insertItem(Key_Escape), this, SLOT(escape()));
    connect(this, SIGNAL(killTheApp()), this, SLOT(accept()));

    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    theme->LoadTheme(xmldata, "programfind");
    LoadWindow(xmldata);

    LayerSet *container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("alphabet");
        if (ltype)
        {
            showsPerListing = ltype->GetItems();
        }
    }
    else
    {
        cerr << "Failed to get selector object.\n";
        exit(0);
    }
    updateBackground();

    curSearch = 10;
    progData = new QString[1];
    listCount = 1;
    if (showsPerListing < 1)
        showsPerListing = 7;
    if (showsPerListing % 2 == 0)
	showsPerListing = showsPerListing + 1;

    inSearch = 0;
    pastInitial = false;
    searchCount = 36;
    recordingCount = 0;

    initData = new QString[(int)(searchCount*showsPerListing)];
    gotInitData = new int[searchCount];
    searchData = new QString[searchCount];
    int curLabel = 0;

    for (int charNum = 48; charNum < 91; charNum++)
    {
        if (charNum == 58)
        	charNum = 65;

	gotInitData[curLabel] = 0; 
        searchData[curLabel] = (char)charNum;
	curLabel++;
    }

    update_Timer = new QTimer(this);
    connect(update_Timer, SIGNAL(timeout()), SLOT(update_timeout()) );

    getSearchData(curSearch);
    showSearchList();

    update_Timer->start((int)(100));

    WFlags flags = getWFlags();
    flags |= WRepaintNoErase;
    setWFlags(flags);

    showInfo = false;

    showFullScreen();
    setActiveWindow();
//    raise();
//    setFocus();

}

ProgFinder::~ProgFinder()
{

    if (inSearch > 0)
        delete [] progData;
    if (inSearch == 2)
    	delete [] showData;
    delete [] searchData;
    delete [] initData; 

    delete [] gotInitData;

    delete update_Timer;

    delete theme;
    delete accel;

}

void ProgFinder::updateBackground(void)
{
    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    LayerSet *container = theme->GetSet("background");
    if (container)
    {
        if (gContext->GetNumSetting("EPGProgramBar", 1) == 1)
            container->Draw(&tmp, 0, 1);
        else
            container->Draw(&tmp, 0, 0);
    }

    tmp.end();

    setPaletteBackgroundPixmap(bground);
}

void ProgFinder::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();
    QPainter p(this);

    if (r.intersects(listRect()))
    {
        updateList(&p);
    }
    if (r.intersects(infoRect()))
    {
        updateInfo(&p);
    }
}

void ProgFinder::updateList(QPainter *p)
{
    QRect pr = listRect();
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    LayerSet *container = NULL;
    container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("alphabet");
        if (ltype)
        {
            if (inSearch == 0)
            {
                ltype->SetItemCurrent((int)(ltype->GetItems() / 2));
                ltype->SetActive(true);
            }
            else 
            {
                ltype->SetItemCurrent(-1);
                ltype->SetActive(false);
            }
        }

        ltype = (UIListType *)container->GetType("shows");
        if (ltype)
        {
            if (inSearch == 1)
            {
                ltype->SetItemCurrent((int)(ltype->GetItems() / 2));
                ltype->SetActive(true);
            }
            else
            {
                ltype->SetItemCurrent(-1);
                ltype->SetActive(false);
            }
        }

        ltype = (UIListType *)container->GetType("times");
        if (ltype)
        {
            if (inSearch == 2)
            {
                ltype->SetItemCurrent((int)(ltype->GetItems() / 2));
                ltype->SetActive(true);
            }
            else
            {
                ltype->SetItemCurrent(-1);
                ltype->SetActive(false);
            }
        }

        container->Draw(&tmp, 0, 0);
        container->Draw(&tmp, 1, 0);
        container->Draw(&tmp, 2, 0);
        container->Draw(&tmp, 3, 0);
        container->Draw(&tmp, 4, 0);
        container->Draw(&tmp, 5, 0);
        container->Draw(&tmp, 6, 0);
        container->Draw(&tmp, 7, 0);
        container->Draw(&tmp, 8, 0);
    }

    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);
}

void ProgFinder::updateInfo(QPainter *p)
{
    QRect pr = infoRect();
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    LayerSet *container = NULL;

    if (inSearch == 0)
    {
        QString title = "";
        QString description = "";
        if (gotInitData[curSearch] == 1)
            container = theme->GetSet("noprograms");
        else
            container = theme->GetSet("help_info_1");
    }
    else if (inSearch == 1)
    {
        container = theme->GetSet("help_info_2");
    }
    else if (inSearch == 2)
    {
        QString title = "";
        QString subtitle = "";
        QString timedate = "";
        QString description = "";
        QString channum = "";
        QString channame = "";
        QString recording = "";

        channum = showData[curShow].channelNum;
        channame = showData[curShow].channelCallsign;
        title = progData[curProgram];
        timedate = showData[curShow].startDisplay + " - " + showData[curShow].endDisplay;
        if (showData[curShow].subtitle.stripWhiteSpace().length() > 0)
            subtitle = "\"" + showData[curShow].subtitle + "\"";
        else 
            subtitle = "";
        description = showData[curShow].description;
        recording = showData[curShow].recText;

        if (gotInitData[curSearch] == 1)
        {
            title = "No Programs";
            description = "There are no available programs under this search. Please select another search.";
        }

        container = theme->GetSet("program_info");
        if (container)
        {
            UITextType *type = (UITextType *)container->GetType("title");
            if (type)
                type->SetText(title);

            type = (UITextType *)container->GetType("subtitle");
            if (type)
                type->SetText(subtitle);

            type = (UITextType *)container->GetType("timedate");
            if (type)
                type->SetText(timedate);

            type = (UITextType *)container->GetType("description");
            if (type)
                type->SetText(description);

            type = (UITextType *)container->GetType("channelnum");
            if (type)
                type->SetText(channum);

            type = (UITextType *)container->GetType("channelname");
            if (type)
                type->SetText(channame);

            type = (UITextType *)container->GetType("recordingstatus");
            if (type)
                type->SetText(recording);
        }
    }

    if (container)
        container->Draw(&tmp, 6, 0);

    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);
}

void ProgFinder::escape()
{
    running = false;
    hide();

    unsetCursor();
    emit killTheApp();
}

void ProgFinder::hideEvent(QHideEvent *e)
{
    QDialog::hideEvent(e);
}

void ProgFinder::showGuide()
{
    running = false;
    hide();

    unsetCursor();
    emit killTheApp();
}


void ProgFinder::LoadWindow(QDomElement &element)
{

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement e = child.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "font")
            {
                theme->parseFont(e);
            }
            else if (e.tagName() == "container")
            {
                parseContainer(e);
            }
            else
            {
                cerr << "Unknown element: " << e.tagName() << endl;
                exit(0);
            }
        }
    }
}

void ProgFinder::parseContainer(QDomElement &element)
{
    QRect area;
    QString name;
    int context;
    theme->parseContainer(element, name, context, area);

    if (name.lower() == "selector")
    {
        rectListLeft = area.left();
        rectListTop = area.top();
        rectListWidth = area.width();
        rectListHeight = area.height();
    }
    if (name.lower() == "program_info")
    {
        rectInfoLeft = area.left();
        rectInfoTop = area.top();
        rectInfoWidth = area.width();
        rectInfoHeight = area.height();
    }
}

void ProgFinder::getInfo()
{
    int rectype = 0;
    QString data = "";
    if (inSearch == 2)
    {
	showInfo = 1;
	ProgramInfo *curPick = ProgramInfo::GetProgramAtDateTime(curChannel,curDateTime + "50");

  	if (curPick)
    	{
        	InfoDialog diag(curPick, this, "Program Info");
        	diag.setCaption("BLAH!!!");
        	diag.exec();
    	}
    	else
	{
        	return;
	}
	showInfo = 0;

    	curPick->GetProgramRecordingStatus(m_db);

	getRecordingInfo();

	for (int i = 0; i < showCount; i++)
	{
		rectype = checkRecordingStatus(i);
                showData[i].recording = rectype;

                switch (rectype)
                {
                        case ScheduledRecording::SingleRecord:
                            data = tr("Recording just this showing");
                            break;
                        case ScheduledRecording::TimeslotRecord:
                            data = tr("Recording when shown in this timeslot");
                            break;
                        case ScheduledRecording::ChannelRecord:
                            data = tr("Recording when shown on this channel");
                            break;
                        case ScheduledRecording::AllRecord:
                            data = tr("Recording all showings");
                            break;
                        case ScheduledRecording::NotRecording:
                            data = tr("Not recording this showing");
                            break;
                        default:
                            data = "Error!";
                }

                showData[i].recText = data;
	}

	showShowingList();

    	setActiveWindow();
    	setFocus();

    }

}

void ProgFinder::update_timeout()
{
	if (pastInitial == false)
	{
		update_Timer->stop();
		pastInitial = true;
		getInitialProgramData();
	}
	else
	{
	  if (inSearch == 0 && gotInitData[curSearch] == 0)
	  {
	    int cnt = 0;

    	    for (int j = 0; j < searchCount; j++)
   	    {
        	  if (gotInitData[j] > 1)
        	        cnt++;
 	    } 

 	    int amountDone = (int)(100.0 * (float)((float)cnt / (float)searchCount));

 	    QString data = QString(" Loading Data...%1% Complete").arg(amountDone);
            cout << data << endl;
          
            LayerSet *container = theme->GetSet("selector");
            if (container)
            {
                UIListType *ltype = (UIListType *)container->GetType("shows");
                if (ltype)
                    ltype->SetItemText((int)(showsPerListing / 2), data);
            }
            update(listRect());
   	  }
	}
}


void ProgFinder::cursorLeft()
{
    inSearch--;
    if (inSearch == -1)
        inSearch = 0;
    else 
    {
        if (inSearch == 0)
            showSearchList();
        else if (inSearch == 1)
        {
            showProgramList();
            clearShowData();
        }
    }
    update(infoRect());
    update(listRect());
}

void ProgFinder::cursorRight()
{
    if (inSearch == 2) {
        getInfo();
    }
    else
    {
        inSearch++;
        if (inSearch == 1)
        {
            if (gotInitData[curSearch] == 0)
		getSearchData(curSearch);
            if (gotInitData[curSearch] >= 10)
                selectSearchData();
            if (gotInitData[curSearch] == 1)
            {
                LayerSet *container = theme->GetSet("selector");
                if (container)
                {
                    UIListType *ltype = (UIListType *)container->GetType("shows");
                    if (ltype)
                        ltype->SetItemText((int)(showsPerListing / 2), "       !! No Programs !!");
                }
		inSearch = 0;
	    }
        }
        if (inSearch == 2)
        {
            if (gotInitData[curSearch] > 10)
                selectShowData(progData[curProgram]);
            else
	        inSearch = 1;
        }
    }
    update(infoRect());
    update(listRect());
}

void ProgFinder::pageUp()
{
   if (inSearch == 0)
   {
        curSearch = curSearch - showsPerListing;
        if (curSearch <= -1)
                curSearch = searchCount + curSearch;

	if (gotInitData[curSearch] <= 1)
		clearProgramList();
        else
        	showSearchList();
   }
   if (inSearch == 1)
   {
        curProgram = curProgram - showsPerListing;
        if (curProgram <= -1)
                curProgram = listCount + curProgram;

        showProgramList();
   }
   if (inSearch == 2)
   {
	curShow = curShow - showsPerListing;
	if (curShow <= -1)
		curShow = showCount + curShow;

	showShowingList();
   }
}

void ProgFinder::pageDown()
{
   if (inSearch == 0)
   {
        curSearch = curSearch + showsPerListing;

        if (curSearch >= searchCount)
                curSearch = curSearch - searchCount;

	if (gotInitData[curSearch] <= 1)
		clearProgramList();
        else
        	showSearchList();
   }
   if (inSearch == 1)
   {
        curProgram = curProgram + showsPerListing;
        if (curProgram >= listCount)
                curProgram = curProgram - listCount;

        showProgramList();
   }
   if (inSearch == 2)
   {
        curShow = curShow + showsPerListing;
        if (curShow >= showCount)
                curShow = curShow - showCount;

        showShowingList();
   }
}

void ProgFinder::cursorUp()
{

   if (inSearch == 0)
   {
	curSearch--;
	if (curSearch == -1)
		curSearch = searchCount - 1;

	if (gotInitData[curSearch] <= 1)
		clearProgramList();
        else
		showSearchList();
   }
   if (inSearch == 1)
   {
	curProgram--;
        if (curProgram == -1)
	{
              curProgram = listCount - 1;
	      while (progData[curProgram] == "**!0")
		   curProgram--;

	}

	showProgramList();
   }
   if (inSearch == 2)
   {
	curShow--;
	if (curShow == -1)
	{
		curShow = showCount - 1;
		while (showData[curShow].title == "**!0")
			curShow--;
	}

	showShowingList();
   }
}

void ProgFinder::cursorDown()
{
   if (inSearch == 0)
   {
	curSearch++;
	if (curSearch >= searchCount)
		curSearch = 0;

	if (gotInitData[curSearch] <= 1)
		clearProgramList();
        else
		showSearchList();
   }
   if (inSearch == 1)
   {
        if ((curProgram + 1) >= listCount)
		curProgram = -1;

	if (progData[curProgram + 1] != "**!0")
	{
		curProgram++;
		if (curProgram == listCount)
			curProgram = 0;

		showProgramList();
	}
	else
	{
		curProgram = 0;
		showProgramList();
	}
   }
   if (inSearch == 2)
   {
        if ((curShow + 1) >= showCount)
		curShow = -1;
	
	if (showData[curShow + 1].title != "**!0")
	{
		curShow++;
		if (curShow == showCount)
			curShow = 0;

		showShowingList();
	}
	else
	{
		curShow = 0;
		showShowingList();
	}
   }
}

void ProgFinder::showSearchList()
{
/*
	if (programTitle->text() == "No Programs")
	{
		programTitle->setText(tr("Select a program..."));
    		description->setText(tr("Select the title of the program you wish to find. When finished return with the left arrow key. Hitting 'Info' will allow you to setup recording options"));
	}
*/

    int curLabel = 0;
    int t = 0;
    int tempSearch;
    QString placeHold;

    tempSearch = curSearch;

    LayerSet *container = NULL;
    container = theme->GetSet("selector");
    if (container)
    {

        UIListType *ltype = (UIListType *)container->GetType("alphabet");
        if (ltype)
        {
            ltype->ResetList();
	    for (int i = (tempSearch - ((showsPerListing - 1) / 2));
	         i < (tempSearch + ((showsPerListing + 1) / 2)); i++)
	    {
		t = i;
                if (i < 0)
                        t = i + searchCount;
                if (i >= searchCount)
                        t = i - searchCount;
		if (t < 0)
			cerr << "MythProgFind: -1 Error in showSearchList()\n";

                if (searchData[t] != NULL)
                {
			if (curLabel == (int)(showsPerListing / 2))
                                ltype->SetItemText(curLabel, " " + searchData[t] + " ");
		        else
                                ltype->SetItemText(curLabel, " " + searchData[t] + " ");
                }
                else
                        ltype->SetItemText(curLabel, "");
                curLabel++;
            }
	}

        ltype = (UIListType *)container->GetType("shows");
        if (ltype)
        {
            ltype->ResetList();
	    if (gotInitData[curSearch] > 1)
	    {
                if (update_Timer->isActive() == true)
		   update_Timer->stop();

                curLabel = 0; 

	        for (int i = (int)(tempSearch*showsPerListing); i < (int)((tempSearch+1)*showsPerListing); i++)
	        {    
		     if (initData[i] != NULL)
		     {
			if (curLabel == (int)(showsPerListing / 2))
                            ltype->SetItemText(curLabel, " " + initData[i] + " ");
                        else
                            ltype->SetItemText(curLabel, " " + initData[i] + " ");
		     }
		     else
                        ltype->SetItemText(curLabel, "");
		     curLabel++;
	        }
            }
	}
	else
	{
		update_Timer->start((int)250);
	}
    }
    update(listRect());
    update(infoRect());
}

void ProgFinder::showProgramList()
{

    if (gotInitData[curSearch] > 1)
    {
         LayerSet *container = NULL;
        container = theme->GetSet("selector");
        if (container)
        {

            UIListType *ltype = (UIListType *)container->GetType("shows");
            if (ltype)
            {

	        int curLabel = 0;
	        int t = 0;
	        for (int i = (curProgram - ((showsPerListing - 1) / 2)); 
   	             i < (curProgram + ((showsPerListing + 1) / 2)); i++)
	        {
		     t = i;
		     if (i < 0)
			 t = i + listCount;
		     if (i >= listCount)
			 t = i - listCount;
	
                     if (progData[t] != NULL)
                     {
		         if (progData[t] != "**!0")
		         {
		             if (curLabel == (int)(showsPerListing / 2))
                                 ltype->SetItemText(curLabel, " " + progData[t] + " ");
			     else
                                ltype->SetItemText(curLabel, " " + progData[t] + " ");
  		         }
		         else
	                     ltype->SetItemText(curLabel, "");
		     }
		     else
                         ltype->SetItemText(curLabel, "");
		     curLabel++;
                }
            }
        }
        update(listRect());

    }
}

void ProgFinder::showShowingList()
{
    int curLabel = 0;
    QString tempData = "";

    if (showCount > 0)
    {
        LayerSet *container = NULL;
        container = theme->GetSet("selector");
        if (container)
        {
            UIListType *ltype = (UIListType *)container->GetType("times");
            if (ltype)
            {
                ltype->ResetList();
                int t = 0;
                for (int i = (curShow - ((showsPerListing - 1) / 2));
                     i < (curShow + ((showsPerListing + 1) / 2)); i++)
                {
                     t = i;
                     if (i < 0)
                         t = i + showCount;
                     if (i >= showCount)
                         t = i - showCount;

	  	     if ((&showData[t]) != NULL)
		     {
		         if (showData[t].title != "**!0")
		         {
                             ltype->SetItemText(curLabel, " " + showData[t].startDisplay);
			
		  	     if (curLabel != (int)(showsPerListing / 2) &&
                                 showData[t].recording > 0)
                                 ltype->EnableForcedFont(curLabel, "recording");

                         }  
		         else
                             ltype->SetItemText(curLabel, "");
		      }	
		      else
		      {
                        ltype->SetItemText(curLabel, "");
		      }
		      curLabel++;
                 }
             } 
        }
	curChannel = showData[curShow].channelID;
        curDateTime = showData[curShow].starttime;
    }
    update(infoRect());
    update(listRect());
}

void ProgFinder::selectSearchData()
{
    if (running == false)
	return;

    accel->setEnabled(false);
    QDateTime progStart = QDateTime::currentDateTime();
    QString thequery;
    QString data;

    thequery = QString("SELECT DISTINCT title "
                       "FROM program "
                       "WHERE ( title LIKE '%1%' OR title LIKE 'The %2%' OR title LIKE 'A %3%' ) "
		       "AND starttime > %4 "
                       "ORDER BY title;")
                        .arg(searchData[curSearch])
			.arg(searchData[curSearch])
			.arg(searchData[curSearch])
			.arg(progStart.toString("yyyyMMddhhmm50"));

    QSqlQuery query = m_db->exec(thequery);
 
    int rows = query.numRowsAffected();

    if (rows == -1)
    {
        cerr << "MythProgFind: Error executing query! (selectSearchData)\n";
        cerr << "MythProgFind: QUERY = " << thequery << endl;
        return;
    }

    delete [] progData;

    listCount = 0;

    if (query.isActive() && rows > 0)
    {
	typedef QMap<QString,QString> ShowData;
	ShowData tempList;

        while (query.next())
        {
		if (running == false)
			return;
		data = QString::fromUtf8(query.value(0).toString());

		if (searchData[curSearch] == "T" || searchData[curSearch] == "A")
		{

		    if (data.left(5) == "The T" && searchData[curSearch] == "T")
		    {
			data = data.mid(4) + ", The";
			tempList[data.lower()] = data;
                	listCount++;
		    }
		    else if (data.left(5) == "The A" && searchData[curSearch] == "A")
                    {
                        data = data.mid(4) + ", The";
			tempList[data.lower()] = data;
                        listCount++;
                    }
		    else if (data.left(3) == "A T" && searchData[curSearch] == "T")
		    {
			data = data.mid(2) + ", A";
			tempList[data.lower()] = data;
                        listCount++;
		    }
		    else if (data.left(3) == "A A" && searchData[curSearch] == "A")
                    {
                        data = data.mid(2) + ", A";
			tempList[data.lower()] = data;
                        listCount++;
                    }
		   else if (data.left(4) != "The " && data.left(2) != "A ")
		    {
			tempList[data.lower()] = data;
                        listCount++;
                    }

		}	
		else
		{
		    if (data.left(4) == "The ")
                        data = data.mid(4) + ", The";
		    if (data.left(2) == "A ")
			data = data.mid(2) + ", A";

		    tempList[data.lower()] = data;
                    listCount++;

		}
	}

	if (listCount < showsPerListing)
    	{
       	 	progData = new QString[showsPerListing];
       	 	for (int i = 0; i < showsPerListing; i++)
               	progData[i] = "**!0";
    	}
    	else
        	progData = new QString[(int)listCount];

	int cnt = 0;

	ShowData::Iterator it;
	for ( it = tempList.begin(); it != tempList.end(); ++it )
	{
		progData[cnt] = it.data();

		if (progData[cnt].right(5) == ", The")
			progData[cnt] = "The " + progData[cnt].left(progData[cnt].length() - 5);
		if (progData[cnt].right(3) == ", A")
		        progData[cnt] = "A " + progData[cnt].left(progData[cnt].length() - 3);

		cnt++;
	}
    }

    if (rows < showsPerListing)
	listCount = showsPerListing;

    curProgram = 0;
    accel->setEnabled(true);
    showProgramList();
}

void ProgFinder::clearProgramList()
{
  if (gotInitData[curSearch] == 0)
  {
    int cnt = 0;
    LayerSet *container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("shows");
        if (ltype)
            for (int i = 0; i < showsPerListing; i++)
            {
                ltype->SetItemText(i, "");
            }
    }

    for (int j = 0; j < searchCount; j++)
    {
	if (gotInitData[j] > 0)
		cnt++;
    }

    int amountDone = (int)(100.0 * (float)((float)cnt / (float)searchCount));

    QString data = QString(" Loading Data...%1% Complete").arg(amountDone);

    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("shows");
        if (ltype)
            ltype->SetItemText((int)(showsPerListing / 2), data);
    }
    showSearchList();
  }
  else
  {
    showSearchList();
  }
}

void ProgFinder::clearShowData()
{
    delete [] showData;

    LayerSet *container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("times");
        if (ltype)
        {

            for (int i = 0; i < showsPerListing; i++)
            {
                 ltype->SetItemText(i, "");
            }
        }
    }
    update(infoRect());
}

int ProgFinder::checkRecordingStatus(int showNum)
{
    for (int j = 0; j < recordingCount; j++)
    {
	if (showData[showNum].title == curRecordings[j].title)
	{
	    	if (showData[showNum].subtitle == curRecordings[j].subtitle &&
	    	    showData[showNum].description == curRecordings[j].description)
	    	{

			if (curRecordings[j].type == ScheduledRecording::SingleRecord)
			{
				if (showData[showNum].startdatetime == curRecordings[j].startdatetime)	
				{
					return curRecordings[j].type;
				}
			}
		}
	 	if (curRecordings[j].type == ScheduledRecording::TimeslotRecord)
		{
			if ((showData[showNum].startdatetime).time() == (curRecordings[j].startdatetime).time()
			  && showData[showNum].channelID == curRecordings[j].chanid)
			{
				return curRecordings[j].type;
			}
		}
		if (curRecordings[j].type == ScheduledRecording::ChannelRecord)
                {
                        if (showData[showNum].channelID == curRecordings[j].chanid)
                        {
                                return curRecordings[j].type;
                        }
                }
		if (curRecordings[j].type == ScheduledRecording::AllRecord)
                {
			return curRecordings[j].type;
                }
	}
    }
    return 0;

}

void ProgFinder::getRecordingInfo()
{
    if (running == false)
	return;
    QDateTime recDateTime;
    QString thequery;
    QString data;
    thequery = QString("SELECT chanid,starttime,startdate,title,subtitle,description,type "
                       "FROM record ");

    QSqlQuery query = m_db->exec(thequery);

    int rows = query.numRowsAffected();

    if (rows == -1)
    {
        cerr << "MythProgFind: Error executing query! (getRecordingInfo)\n";
        cerr << "MythProgFind: QUERY = " << thequery << endl;
        return;
    }

    recordingCount = 0;
    if (rows > 0)
    {

    curRecordings = new recordingRecord[(int)rows];

    if (query.isActive() && rows > 0)
    {
        while (query.next())
        {
		recDateTime = QDateTime::fromString(query.value(2).toString() + "T" + query.value(1).toString(),
                                                  Qt::ISODate);

		curRecordings[recordingCount].chanid = query.value(0).toString();
		curRecordings[recordingCount].startdatetime = recDateTime;
		curRecordings[recordingCount].title = QString::fromUtf8(query.value(3).toString());
		curRecordings[recordingCount].subtitle = QString::fromUtf8(query.value(4).toString());
		curRecordings[recordingCount].description = QString::fromUtf8(query.value(5).toString());
		curRecordings[recordingCount].type = query.value(6).toInt();

                if (curRecordings[recordingCount].title == QString::null)
                    curRecordings[recordingCount].title = "";
                if (curRecordings[recordingCount].subtitle == QString::null)
                    curRecordings[recordingCount].subtitle = "";
                if (curRecordings[recordingCount].description == QString::null)
                    curRecordings[recordingCount].description = "";

		recordingCount++;
	}
    }
   
    }
}

void ProgFinder::selectShowData(QString progTitle)
{
    if (running == false)
	return;
    accel->setEnabled(false);
    QDateTime progStart = QDateTime::currentDateTime();
    QDateTime progEnd;
    QString thequery;
    QString data;
    int rectype = 0;

    thequery = QString("SELECT subtitle,starttime,channel.channum,"
		       "channel.callsign,description,endtime,channel.chanid "
                       "FROM program,channel "
                       "WHERE program.title = \"%1\" AND program.chanid = channel.chanid "
		       "AND program.starttime > %2 "
                       "ORDER BY program.starttime;")
                        .arg(progTitle.utf8()).arg(progStart.toString("yyyyMMddhhmm50"));

    QSqlQuery query = m_db->exec(thequery);

    int rows = query.numRowsAffected();

    if (rows == -1)
    {
	cerr << "MythProgFind: Error executing query! (selectShowData)\n";
	cerr << "MythProgFind: QUERY = " << thequery << endl;
	return;
    }

    showCount = 0;
 
    if (rows < showsPerListing)
    {
	showData = new showRecord[showsPerListing];
	for (int i = 0; i < showsPerListing; i++)
		showData[i].title = "**!0";
    }
    else
    {
    	showData = new showRecord[rows];
    }

    if (query.isActive() && rows > 0)
    {
        while (query.next())
        {
                progStart = QDateTime::fromString(query.value(1).toString(),
                                                  Qt::ISODate);
		progEnd = QDateTime::fromString(query.value(5).toString(),
						  Qt::ISODate);

		// DATE, CHANNEL NUM, CHANNEL CALLSIGN, SUBTITLE, DESCRIPTION, SQL START, SQL END

		showData[showCount].startdatetime = progStart;
		QString DateTimeFormat = gContext->GetSetting("DateFormat")+" "+gContext->GetSetting("TimeFormat");

		showData[showCount].startDisplay = progStart.toString(DateTimeFormat);
		showData[showCount].endDisplay = progEnd.toString(gContext->GetSetting("TimeFormat"));
		showData[showCount].channelNum = query.value(2).toString();
		showData[showCount].channelCallsign = query.value(3).toString();
		showData[showCount].title = progTitle;
		showData[showCount].subtitle = QString::fromUtf8(query.value(0).toString());
		showData[showCount].description = QString::fromUtf8(query.value(4).toString());
		showData[showCount].channelID = query.value(6).toString();
		showData[showCount].starttime = progStart.toString("yyyyMMddhhmm");
		showData[showCount].endtime = progEnd.toString("yyyyMMddhhmm");
		rectype = checkRecordingStatus(showCount);
		showData[showCount].recording = rectype;

                if (showData[showCount].subtitle == QString::null)
                    showData[showCount].subtitle = "";
                if (showData[showCount].description == QString::null)
                    showData[showCount].description = "";

		switch (rectype)
		{
			case ScheduledRecording::SingleRecord:
                            data = tr("Recording just this showing");
                            break;
                        case ScheduledRecording::TimeslotRecord:
                            data = tr("Recording when shown in this timeslot");
                            break;
                        case ScheduledRecording::ChannelRecord:
                            data = tr("Recording when shown on this channel");
                            break;
                        case ScheduledRecording::AllRecord:
                            data = tr("Recording all showings");
                            break;
                        case ScheduledRecording::NotRecording:
			    data = tr("Not recording this showing");
                            break;
			default:
			    data = "Error!";
		}
		
		showData[showCount].recText = data;

		/*
                data = progStart.toString("ddd M/dd     h:mm ap") + "~" + 
			progEnd.toString("hh:mm ap") + "~" +
                        query.value(2).toString() + "~" + 
			query.value(3).toString() + "~" +
			query.value(0).toString() + " ~" + 
			query.value(4).toString() + 
			progStart.toString(" ~yyyyMMddhhmm") + 
			progEnd.toString("~yyyyMMddhhmm~") +
			query.value(6).toString();

                showData[showCount] = data;
		*/

                showCount++;
        }
    }

    if (rows < showsPerListing)
	showCount = showsPerListing;

    curShow = 0;
    accel->setEnabled(true);
    showShowingList();
}

void ProgFinder::getInitialProgramData()
{
    getRecordingInfo();

    getSearchData(8);
    getSearchData(9);
    getSearchData(11);
    getSearchData(12);

    for (int charNum = 0; charNum < searchCount; charNum++)
    {
	if (charNum == 8)
            charNum = 13;

	getSearchData(charNum);
    }

}

void ProgFinder::getSearchData(int charNum)
{
    if (running == false)
	return;
    int cnts = 0;
    int dataNum = 0;
    int startPlace = 0;
    QDateTime progStart = QDateTime::currentDateTime();
    QString thequery;
    QString data;

    thequery = QString("SELECT DISTINCT title "
                       "FROM program "
                       "WHERE ( title LIKE '%1%' OR title LIKE 'The %2%' OR title LIKE 'A %3%' ) "
		       "AND starttime > %4 "
                       "ORDER BY title;")
			.arg(searchData[charNum])
			.arg(searchData[charNum])
			.arg(searchData[charNum])
			.arg(progStart.toString("yyyyMMddhhmm50"));

    QSqlQuery query = m_db->exec(thequery);

    int rows = query.numRowsAffected();

    if (rows == -1)
    {
        cerr << "MythProgFind: Error executing query! (getSearchData)\n";
        cerr << "MythProgFind: QUERY = " << thequery << endl;
        return;
    }

    startPlace = (int)(charNum*showsPerListing);
    dataNum = (int)(showsPerListing / 2);

    if (query.isActive() && rows > 0)
    {
	typedef QMap<QString,QString> ShowData;
        ShowData tempList;

        while (query.next())
        {
		data = QString::fromUtf8(query.value(0).toString());
		
		if (charNum == 29 || charNum == 10)
                {
		   if (data.left(5) == "The T" && charNum == 29)
                   {
                        data = data.mid(4) + ", The";
                        tempList[data.lower()] = data;
                        cnts++;
                   }
                   else if (data.left(5) == "The A" && charNum == 10)
                   {
                        data = data.mid(4) + ", The";
			tempList[data.lower()] = data;
                        cnts++;
                   }
                   else if (data.left(3) == "A T" && charNum == 29)
                   {
                        data = data.mid(2) + ", A";
			tempList[data.lower()] = data;
                        cnts++;
                   }
                   else if (data.left(3) == "A A" && charNum == 10)
                   {
                        data = data.mid(2) + ", A";
			tempList[data.lower()] = data;
                        cnts++;
                   }
                   else if (data.left(4) != "The " && data.left(2) != "A ")
                   {
			tempList[data.lower()] = data;
                        cnts++;
                   }
		}
		else
                {
                    if (data.left(4) == "The ")
                        data = data.mid(4) + ", The";
		    if (data.left(2) == "A ")
			data = data.mid(2) + ", A";

		    tempList[data.lower()] = data;
		    cnts++;

                }
	}

        int cnt = 0;
	int dNum = 0;

	ShowData::Iterator it;
        for ( it = tempList.begin(); it != tempList.end(); ++it )
        {
		if (cnt <= (int)(showsPerListing / 2))
		{
			data = it.data();

			if (data.right(5) == ", The")
				data = "The " + data.left(data.length() - 5);
			if (data.right(3) == ", A")
                                data = "A " + data.left(data.length() - 3);

			initData[startPlace + dataNum] = data;
			dataNum++;
		}	

		if (cnt >= (cnts - (int)(showsPerListing / 2)) && rows >= showsPerListing)
		{
			data = it.data();

                        if (data.right(5) == ", The")
                                data = "The " + data.left(data.length() - 5);
			if (data.right(3) == ", A")
                                data = "A " + data.left(data.length() - 3);

                        initData[startPlace + dNum] = data;
                        dNum++;
		}

                cnt++;
        }

    }


    if (cnts == 0)
         gotInitData[charNum] = 1;
    else
    {
         gotInitData[charNum] = 10 + cnts;
    }

    if (charNum == curSearch)
	showSearchList();

}


QRect ProgFinder::listRect() const
{
    QRect r(rectListLeft, rectListTop, rectListWidth, rectListHeight);
    return r;
}

QRect ProgFinder::infoRect() const
{
    QRect r(rectInfoLeft, rectInfoTop, rectInfoWidth, rectInfoHeight);
    return r;
}

/*

QStringList sortedList, KeyValuePair;
QString sortKey, IP;
int hits;

sortedList.clear();
IPLIST::iterator it = myList.begin();
while (it != myList.end()){
	sortKey = "0000000000|"+QString::number(it.data());
	sortKey = sortKey.rightJustify(10)+"|"+it.key();
	sortedList << sortKey;
}
sortedList.sort();
QStringList::iterator sit = sortedList.end();
while (sit != sortedList.begin()){
	KeyValuePair = QStringList::split(QRegExp("|"),*sit);
	hits = KeyValuePair.begin().toInt();
	IP = KeyValuePair.end();
	it--;
}

*/
