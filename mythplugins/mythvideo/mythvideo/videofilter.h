#ifndef VIDEOFILTER_H_
#define VIDEOFILTER_H_

#include <mythtv/libmythui/mythscreentype.h>

#include "parentalcontrols.h"

class MythUIButtonList;
class MythUIButtonListItem;
class MythUIButton;
class MythUIText;

class Metadata;
class VideoList;

class VideoFilterSettings
{
  public:
    static const unsigned int FILTER_MASK = 0xFFFE;
    static const unsigned int SORT_MASK = 0x1;
    enum FilterChanges {
        kSortOrderChanged = (1 << 0),
        kFilterCategoryChanged = (1 << 1),
        kFilterGenreChanged = (1 << 2),
        kFilterCountryChanged = (1 << 3),
        kFilterYearChanged = (1 << 4),
        kFilterRuntimeChanged = (1 << 5),
        kFilterUserRatingChanged = (1 << 6),
        kFilterBrowseChanged = (1 << 7),
        kFilterInetRefChanged = (1 << 8),
        kFilterCoverFileChanged = (1 << 9),
        kFilterParentalLevelChanged = (1 << 10),
        kFilterCastChanged = (1 << 11)
    };

  public:
    VideoFilterSettings(bool loaddefaultsettings = true,
                        const QString &_prefix = "");
    VideoFilterSettings(const VideoFilterSettings &rhs);
    VideoFilterSettings &operator=(const VideoFilterSettings &rhs);

    bool matches_filter(const Metadata &mdata) const;
    bool meta_less_than(const Metadata &lhs, const Metadata &rhs,
                        bool sort_ignores_case) const;

    void saveAsDefault();

    enum ordering
    {
        // These values must be explicitly assigned; they represent
        // database values
        kOrderByTitle = 0,
        kOrderByYearDescending = 1,
        kOrderByUserRatingDescending = 2,
        kOrderByLength = 3,
        kOrderByFilename = 4,
        kOrderByID = 5
    };

    int GetCategory() const { return category; }
    void SetCategory(int lcategory)
    {
        m_changed_state |= kFilterCategoryChanged;
        category = lcategory;
    }

    int getGenre() const { return genre; }
    void setGenre(int lgenre)
    {
        m_changed_state |= kFilterGenreChanged;
        genre = lgenre;
    }

    int GetCast() const { return cast; }
    void SetCast(int lcast)
    {
        m_changed_state |= kFilterCastChanged;
        cast = lcast;
    }

    int getCountry() const { return country; }
    void setCountry(int lcountry)
    {
        m_changed_state |= kFilterCountryChanged;
        country = lcountry;
    }

    int getYear() const { return year; }
    void SetYear(int lyear)
    {
        m_changed_state |= kFilterYearChanged;
        year = lyear;
    }

    int getRuntime() const { return runtime; }
    void setRuntime(int lruntime)
    {
        m_changed_state |= kFilterRuntimeChanged;
        runtime = lruntime;
    }

    int GetUserRating() const { return userrating; }
    void SetUserRating(int luserrating)
    {
        m_changed_state |= kFilterUserRatingChanged;
        userrating = luserrating;
    }

    int GetBrowse() const {return browse; }
    void SetBrowse(int lbrowse)
    {
        m_changed_state |= kFilterBrowseChanged;
        browse = lbrowse;
    }

    ordering getOrderby() const { return orderby; }
    void setOrderby(ordering lorderby)
    {
        m_changed_state |= kSortOrderChanged;
        orderby = lorderby;
    }

    ParentalLevel::Level getParentalLevel() const { return m_parental_level; }
    void setParentalLevel(ParentalLevel::Level parental_level)
    {
        m_changed_state |= kFilterParentalLevelChanged;
        m_parental_level = parental_level;
    }

    int getInteRef() const { return m_inetref; }
    void SetInetRef(int inetref)
    {
        m_inetref = inetref;
        m_changed_state |= kFilterInetRefChanged;
    }

    int GetCoverFile() const { return m_coverfile; }
    void SetCoverFile(int coverfile)
    {
        m_coverfile = coverfile;
        m_changed_state |= kFilterCoverFileChanged;
    }

    unsigned int getChangedState()
    {
        unsigned int ret = m_changed_state;
        m_changed_state = 0;
        return ret;
    }

  private:
    int category;
    int genre;
    int country;
    int cast;
    int year;
    int runtime;
    int userrating;
    int browse;
    int m_inetref;
    int m_coverfile;
    ordering orderby;
    ParentalLevel::Level m_parental_level;
    QString prefix;

    unsigned int m_changed_state;
};

struct FilterSettingsProxy
{
    virtual ~FilterSettingsProxy() {}
    virtual const VideoFilterSettings &getSettings() = 0;
    virtual void setSettings(const VideoFilterSettings &settings) = 0;
};

template <typename T>
class BasicFilterSettingsProxy : public FilterSettingsProxy
{
  public:
    BasicFilterSettingsProxy(T &type) : m_type(type) {}

    const VideoFilterSettings &getSettings()
    {
        return m_type.getCurrentVideoFilter();
    }

    void setSettings(const VideoFilterSettings &settings)
    {
        m_type.setCurrentVideoFilter(settings);
    }

  private:
    T &m_type;
};

class VideoFilterDialog : public MythScreenType
{

  Q_OBJECT

  public:
    VideoFilterDialog( MythScreenStack *lparent, QString lname,
                       VideoList *video_list);
    ~VideoFilterDialog();

    bool Create();

  signals:
    void filterChanged();

  public slots:
    void saveAndExit();
    void saveAsDefault();
    void SetYear(MythUIButtonListItem *item);
    void SetUserRating(MythUIButtonListItem *item);
    void SetCategory(MythUIButtonListItem *item);
    void setCountry(MythUIButtonListItem *item);
    void setGenre(MythUIButtonListItem *item);
    void SetCast(MythUIButtonListItem *item);
    void setRunTime(MythUIButtonListItem *item);
    void SetBrowse(MythUIButtonListItem *item);
    void SetInetRef(MythUIButtonListItem *item);
    void SetCoverFile(MythUIButtonListItem *item);
    void setOrderby(MythUIButtonListItem *item);

 private:
    void fillWidgets();
    void update_numvideo();
    VideoFilterSettings m_settings;

    MythUIButtonList  *m_browseList;
    MythUIButtonList  *m_orderbyList;
    MythUIButtonList  *m_yearList;
    MythUIButtonList  *m_userratingList;
    MythUIButtonList  *m_categoryList;
    MythUIButtonList  *m_countryList;
    MythUIButtonList  *m_genreList;
    MythUIButtonList  *m_castList;
    MythUIButtonList  *m_runtimeList;
    MythUIButtonList  *m_inetrefList;
    MythUIButtonList  *m_coverfileList;
    MythUIButton    *m_saveButton;
    MythUIButton    *m_doneButton;
    MythUIText      *m_numvideosText;

    const VideoList &m_videoList;
    FilterSettingsProxy *m_fsp;
};

#endif
