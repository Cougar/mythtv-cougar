#ifndef OSD_H
#define OSD_H

#include <qstring.h>
#include <qrect.h>
#include <qpoint.h>
#include <time.h>
#include <pthread.h>

class QImage;
class OSDImage;
class TTFFont;
 
class OSD
{
 public:
    OSD(int width, int height, const QString &filename, const QString &prefix,
        const QString &osdtheme);
   ~OSD(void);

    void Display(unsigned char *yuvptr);
    
    void SetInfoText(const QString &text, const QString &subtitle, 
                     const QString &desc, const QString &category,
                     const QString &start, const QString &end, 
                     const QString &callsign, const QString &iconpath,
                     int length);
    void SetChannumText(const QString &text, int length);

    void ShowLast(int length);
    void TurnOff(void);
   
    void SetDialogBox(const QString &message, const QString &optionone, 
                      const QString &optiontwo, const QString &optionthree,
                      int length);
    void DialogUp(void);
    void DialogDown(void);
    bool DialogShowing(void) { return show_dialog; } 
    int GetDialogSelection(void) { return currentdialogoption; }

    // position is 0 - 1000 
    void StartPause(int position, bool fill, QString msgtext,
                    QString slidertext, int displaytime);
    void UpdatePause(int position, QString slidertext);
    void EndPause(void);

    bool Visible(void) { return (time(NULL) <= displayframes); }
  
 private:
    void SetNoThemeDefaults();
    TTFFont *LoadFont(QString name, int size); 
    QString FindTheme(QString name);
   
    bool LoadTheme();
    QPoint parsePoint(QString text);
    QRect parseRect(QString text);
    void normalizeRect(QRect *rect);
    
    void DarkenBox(QRect &rect, unsigned char *screen);
    void DrawStringIntoBox(QRect rect, const QString &text, 
                           unsigned char *screen, int fadeframes);

    void DrawStringCentered(unsigned char *yuvptr, QRect rect,
                            const QString &text, TTFFont *font, int fadeframes);

    void DrawStringWithOutline(unsigned char *yuvptr, QRect rect, 
                               const QString &text, TTFFont *font,
                               int fadeframes, bool rightjustify = false);

    void DrawRectangle(QRect &rect, unsigned char *screen);

    void BlendImage(OSDImage *image, int xstart, int ystart, 
                    unsigned char *screen, int fadeframes);
    void BlendFillSlider(OSDImage *image, int xstart, int ystart,
                         int drawwidth, unsigned char *screen, int fadeframes);
    
    void DisplayDialogNoTheme(unsigned char *yuvptr);
    void DisplayInfo(unsigned char *yuvptr);
    void DisplayChannumNoTheme(unsigned char *yuvptr);
    void DisplayPause(unsigned char *yuvptr);
    
    QString fontname;

    int vid_width;
    int vid_height;

    QRect titleRect;
    QRect infoRect;
    bool show_info;
    QString infotext;
    QString subtitletext;
    QString desctext;
    TTFFont *info_font;
    int infofontsize;
    OSDImage *infobackground;
 
    bool useinfoicon; 
    QPoint infoiconpos;
    OSDImage *infoicon;
    QString infocallsign;
    QRect callsignRect;
    QRect timeRect;
    QString timeFormat;
    
    QRect channumRect; 
    QString channumtext;
    bool show_channum;
    TTFFont *channum_font;
    int channumfontsize;

    int displayframes;

    int space_width;

    bool enableosd;

    QRect dialogRect;
    QString dialogmessagetext;
    QString dialogoptionone;
    QString dialogoptiontwo;
    QString dialogoptionthree;

    int currentdialogoption;
    bool show_dialog;

    QString fontprefix;
    
    bool usingtheme;
    QString themepath;

    float hmult, wmult;

    bool usepause;
    bool show_pause;
    int pauseposition;
    QString pauseslidertext;
    OSDImage *pausebackground;
    OSDImage *pausesliderfill;
    OSDImage *pausesliderdelete;
    OSDImage *pausesliderpos;
    QRect pausestatusRect;
    QRect pausesliderRect;
    QRect pausesliderTextRect;
    QString pausestatus;
    int pausesliderfontsize;
    TTFFont *pausesliderfont;
    int pauseyoffset;
    int pausemovementperframe;

    bool hidingpause;
 
    int fadingframes;
    int pausefadingframes;
    int totalfadeframes;
   
    int CalcNewOffset(OSDImage *image, int curoffset);

    int displaypausetime;
    bool pausefilltype;

    pthread_mutex_t osdlock;
};
    
#endif
