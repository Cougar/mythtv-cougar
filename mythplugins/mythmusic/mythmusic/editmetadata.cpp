#include <mythtv/mythcontext.h>
#include <qdir.h>
#include <qsqldatabase.h>
#include "editmetadata.h"
#include "decoder.h"
#include "genres.c"

EditMetadataDialog::EditMetadataDialog(Metadata *source_metadata,
                                 MythMainWindow *parent,
                                 QString window_name,
                                 QString theme_filename,
                                 const char* name)
                :MythThemedDialog(parent, window_name, theme_filename, name)
{
    // make a copy so we can abandon changes
    m_metadata = new Metadata(*source_metadata);
    m_sourceMetadata = source_metadata;
    wireUpTheme();
    fillWidgets();
    assignFirstFocus();
}

void EditMetadataDialog::fillWidgets()
{
    if (album_edit)
    {
        album_edit->setText(m_metadata->Album());
    }

    if (artist_edit)
    {
        artist_edit->setText(m_metadata->Artist());
    }

    if (title_edit)
    {
        title_edit->setText(m_metadata->Title());
    }

    if (genre_edit)
    {
        genre_edit->setText(m_metadata->Genre());
    }

    if (year_edit)
    {
        QString s;
        s = s.setNum(m_metadata->Year());
        year_edit->setText(s);
    }
    
    if (track_edit)
    {
        QString s;
        s = s.setNum(m_metadata->Track());
        track_edit->setText(s);
    }

    if (playcount_text)
    {
        QString s;
        s = s.setNum(m_metadata->Playcount());
        playcount_text->SetText(s);
    }

    if (lastplay_text)
    {
        QString timestamp = m_metadata->LastPlayStr();

        if (timestamp.contains('-') < 1)
        {
            timestamp.insert(4, '-');
            timestamp.insert(7, '-');
            timestamp.insert(10, 'T');
            timestamp.insert(13, ':');
            timestamp.insert(16, ':');
        }

        QDateTime dt = QDateTime::fromString(timestamp, Qt::ISODate);
        lastplay_text->SetText(dt.toString(gContext->GetSetting("dateformat") +
                               " " + gContext->GetSetting("timeformat")));
    }

    if (filename_text)
    {
        filename_text->SetText(m_metadata->Filename());
    }

    if (rating_image)
    {
        rating_image->setRepeat(m_metadata->Rating());
    }
    
    if (compilation_check)
    {
        compilation_check->setState(m_metadata->Compilation());
    }
}

void EditMetadataDialog::incRating(bool up_or_down)
{
    if (up_or_down)
        m_metadata->incRating();
    else
        m_metadata->decRating();

    fillWidgets();
}

void EditMetadataDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Video", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
            nextPrevWidgetFocus(false);
        else if (action == "DOWN")
            nextPrevWidgetFocus(true);
        else if (action == "LEFT")
        {
            if (getCurrentFocusWidget() == rating_button)
            {
                rating_button->push();
                incRating(false);
            }
            
            if (getCurrentFocusWidget() == compilation_check)
            {
                compilation_check->activate();
            }
        }
        else if (action == "RIGHT")
        {
            if (getCurrentFocusWidget() == rating_button)
            {
                rating_button->push();
                incRating(true);
            }
            
            if (getCurrentFocusWidget() == compilation_check)
            {
                compilation_check->activate();
            }
            
        }
        else if (action == "SELECT")
        {
            activateCurrent();
        }
        else if (action == "0")
        {
            if (done_button)
                done_button->push();
        }
        else if (action == "1")
        {
        }
        else
            handled = false;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void EditMetadataDialog::wireUpTheme()
{
    artist_edit = getUIRemoteEditType("artist_edit");
    if (artist_edit)
    {
        artist_edit->createEdit(this);
        connect(artist_edit, SIGNAL(loosingFocus()), this, SLOT(editLostFocus()));
    }
    
    album_edit = getUIRemoteEditType("album_edit");
    if (album_edit)
    {
        album_edit->createEdit(this);
        connect(album_edit, SIGNAL(loosingFocus()), this, SLOT(editLostFocus()));
    }
    
    title_edit = getUIRemoteEditType("title_edit");
    if (title_edit)
    {
        title_edit->createEdit(this);
        connect(title_edit, SIGNAL(loosingFocus()), this, SLOT(editLostFocus()));
    }
         
    genre_edit = getUIRemoteEditType("genre_edit");
    if (genre_edit)
    {
        genre_edit->createEdit(this);
        connect(genre_edit, SIGNAL(loosingFocus()), this, SLOT(editLostFocus()));
    }
        
    year_edit = getUIRemoteEditType("year_edit");
    if (year_edit)
    {
        year_edit->createEdit(this);
        connect(year_edit, SIGNAL(loosingFocus()), this, SLOT(editLostFocus()));
    }

    track_edit = getUIRemoteEditType("track_edit");
    if (track_edit)
    {
        track_edit->createEdit(this);
        connect(track_edit, SIGNAL(loosingFocus()), this, SLOT(editLostFocus()));
    }
            
    lastplay_text = getUITextType("lastplay_text");
    playcount_text = getUITextType("playcount_text");
    filename_text = getUITextType("filename_text");
    rating_image = getUIRepeatedImageType("rating_image");
    
    compilation_check = getUICheckBoxType("compilation_check");
    if (compilation_check)
    {
        connect(compilation_check, SIGNAL(pushed(bool)), this, SLOT(checkClicked(bool)));
    }
    
    searchartist_button = getUIPushButtonType("searchartist_button");
    if (searchartist_button)
    {
        connect(searchartist_button, SIGNAL(pushed()), this, SLOT(searchArtist()));
    }

    searchalbum_button = getUIPushButtonType("searchalbum_button");
    if (searchalbum_button)
    {
        connect(searchalbum_button, SIGNAL(pushed()), this, SLOT(searchAlbum()));
    }

    searchgenre_button = getUIPushButtonType("searchgenre_button");
    if (searchgenre_button)
    {
        connect(searchgenre_button, SIGNAL(pushed()), this, SLOT(searchGenre()));
    }

    done_button = getUITextButtonType("done_button");
    if (done_button)
    {
        done_button->setText(tr("Done"));
        connect(done_button, SIGNAL(pushed()), this, SLOT(closeDialog()));
    }

    save_button = getUITextButtonType("save_button");
    if (save_button)
    {
        save_button->setText(tr("Save"));
        connect(save_button, SIGNAL(pushed()), this, SLOT(showSaveMenu()));
    }

    rating_button = getUISelectorType("rating_button");
    if (rating_button)
    {
        
    }

    dbStatistics_button = getUITextButtonType("dbstatistics_button");
    if (dbStatistics_button)
    {
        dbStatistics_button->setText(tr("DB Statistics"));
     }

    buildFocusList();
}

void EditMetadataDialog::editLostFocus()
{
    UIRemoteEditType *whichEditor = (UIRemoteEditType *) getCurrentFocusWidget();
    
    if (whichEditor == album_edit)
    {
        m_metadata->setAlbum(album_edit->getText());
    }
    else if (whichEditor == artist_edit)
    {
        m_metadata->setArtist(artist_edit->getText());
    }
    else if (whichEditor == title_edit)
    {
        m_metadata->setTitle(title_edit->getText());
    }
    else if (whichEditor == genre_edit)
    {
        m_metadata->setGenre(genre_edit->getText());
    }
    else if (whichEditor == year_edit)
    {
        m_metadata->setYear(year_edit->getText().toInt());
    }
    else if (whichEditor == track_edit)
    {
        m_metadata->setTrack(track_edit->getText().toInt());
    }

}

void EditMetadataDialog::checkClicked(bool state)
{
    m_metadata->setCompilation(state);
}

void EditMetadataDialog::closeDialog()
{
    //FIXME should ask to save any changes 
    *m_sourceMetadata = m_metadata;
    done(1);
}

bool EditMetadataDialog::showList(QString caption, QString &value)
{
    bool res = false;
    
    MythSearchDialog *searchDialog = new MythSearchDialog(gContext->GetMainWindow(), "");
    searchDialog->setCaption(caption);
    searchDialog->setSearchText(value);
    searchDialog->setItems(searchList);
    if (searchDialog->ExecPopup() == 0)
    {
        value = searchDialog->getResult();
        res = true;
    }
    
    delete searchDialog;
    setActiveWindow();
    
    return res;     
}

void EditMetadataDialog::fillSearchList(QString field)
{
    searchList.clear();
    
    QSqlQuery query;
    QString querystr;
    querystr = QString("SELECT DISTINCT %1 FROM musicmetadata ORDER BY %2").arg(field).arg(field);
         
    query.exec(querystr);
    if (query.isActive() && query.numRowsAffected())
    {
        while (query.next())
        {
            searchList << query.value(0).toString();
        }
    }         
}
    
void EditMetadataDialog::searchArtist()
{
    QString s;
    
    fillSearchList("artist");
    
    s = m_metadata->Artist();
    if (showList(tr("Select an Artist"), s))
    {
        m_metadata->setArtist(s);
        fillWidgets();
    }
}

void EditMetadataDialog::searchAlbum()
{
    QString s;
    
    fillSearchList("album");
    
    s = m_metadata->Album();
    if (showList(tr("Select an Album"), s))
    {
        m_metadata->setAlbum(s);
        fillWidgets();
    }
}

void EditMetadataDialog::searchGenre()
{
    QString s;

    // load genre list
    searchList.clear();
    for (int x = 0; x < genre_table_size; x++)
        searchList.push_back(QString(genre_table[x]));
    searchList.sort();

    s = m_metadata->Genre();
    if (showList(tr("Select a Genre"), s))
    {
        m_metadata->setGenre(s);
        fillWidgets();
    }
   
}

void EditMetadataDialog::showSaveMenu()
{
    popup = new MythPopupBox(gContext->GetMainWindow(), "Menu");

    QLabel *label = popup->addLabel(tr("Save Changes?"), MythPopupBox::Large, false);
    label->setAlignment(Qt::AlignCenter | Qt::WordBreak);

    QButton *topButton = popup->addButton(tr("Save to Database Only"), this,
                         SLOT(saveToDatabase()));
    popup->addButton(tr("Save to File Only"), this,
                         SLOT(saveToFile()));
    popup->addButton(tr("Save to File and Database"), this,
                         SLOT(saveAll()));
    popup->addButton(tr("Cancel"), this, SLOT(cancelPopup()));

    popup->ShowPopup(this, SLOT(cancelPopup()));

    topButton->setFocus();
}

void EditMetadataDialog::cancelPopup()
{
  if (!popup)
      return;

  popup->hide();

  delete popup;
  popup = NULL;
  setActiveWindow();
}

void EditMetadataDialog::saveToDatabase()
{
   cancelPopup();

   QSqlDatabase *db = QSqlDatabase::database();

   m_metadata->updateDatabase(db, QString::null);
   *m_sourceMetadata = m_metadata;
}

void EditMetadataDialog::saveToFile()
{
   cancelPopup();
   Decoder *decoder = Decoder::create(m_metadata->Filename(), NULL, NULL, true);
   if (decoder)
   {
       decoder->commitMetadata(m_metadata);
       delete decoder;
   }
}

void EditMetadataDialog::saveAll()
{
   cancelPopup();
}

EditMetadataDialog::~EditMetadataDialog()
{
    delete m_metadata;
}
