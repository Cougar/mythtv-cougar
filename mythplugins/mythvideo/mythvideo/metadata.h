#ifndef METADATA_H_
#define METADATA_H_

// C++ headers
#include <utility>
#include <vector>

// QT headers
#include <QString>

// Mythvideo headers
#include "parentalcontrols.h"

class MSqlQuery;
class MetadataListManager;

enum { VIDEO_YEAR_DEFAULT = 1895 };

struct SortData;

class Metadata
{
  public:
    typedef std::pair<int, QString> genre_entry;
    typedef std::pair<int, QString> country_entry;
    typedef std::pair<int, QString> cast_entry;
    typedef std::vector<genre_entry> genre_list;
    typedef std::vector<country_entry> country_list;
    typedef std::vector<cast_entry> cast_list;

  public:
    class SortKey
    {
      public:
        SortKey();
        SortKey(const SortData &data);
        SortKey(const SortKey &other);
        SortKey &operator=(const SortKey &rhs);
        ~SortKey();

        bool isSet() const;
        void Clear();

      public:
        SortData *m_sd;
    };

  public:
    static SortKey GenerateDefaultSortKey(const Metadata &m, bool ignore_case);
    static QString FilenameToTitle(const QString &file_name);
    static QString trimTitle(const QString &title, bool ignore_case);
    static QString getPlayer(const Metadata *item);
    static QString getPlayer(const Metadata *item, QString &internal_mrl);
    static QString getPlayCommand(const Metadata *item);
    static bool getPlayer(const QString &extension, QString &player,
            bool &use_default);

  public:
    Metadata(const QString &filename = "", const QString &coverfile = "",
             const QString &title = "", int year = VIDEO_YEAR_DEFAULT,
             const QString &inetref = "", const QString &director = "",
             const QString &plot = "", float userrating = 0.0,
             const QString &rating = "", int length = 0,
             int id = 0,
             ParentalLevel::Level showlevel = ParentalLevel::plLowest,
             int categoryID = 0,
             int childID = -1, bool browse = true,
             const QString &playcommand = "", const QString &category = "",
             const genre_list &genres = genre_list(),
             const country_list &countries = country_list(),
             const cast_list &cast = cast_list());
    ~Metadata();
    Metadata(MSqlQuery &query);
    Metadata(const Metadata &rhs);
    Metadata &operator=(const Metadata &rhs);

    // returns a string to use when sorting
    bool hasSortKey() const;
    const SortKey &getSortKey() const;
    void setSortKey(const SortKey &sort_key);

    // flat index
    void setFlatIndex(int index);
    int getFlatIndex() const;

    const QString &getPrefix() const;
    void setPrefix(const QString &prefix);

    const QString &Title() const;
    void setTitle(const QString& title);

    int Year() const;
    void setYear(int year);

    const QString &InetRef() const;
    void setInetRef(const QString &inetRef);

    const QString &Director() const;
    void setDirector(const QString &director);

    const QString &Plot() const;
    void setPlot(const QString &plot);

    float UserRating() const;
    void setUserRating(float userRating);

    const QString &Rating() const;
    void setRating(const QString &rating);

    int Length() const;
    void setLength(int length);

    unsigned int ID() const;
    void setID(int id);

    int ChildID() const;
    void setChildID(int childID);

    bool Browse() const;
    void setBrowse(bool browse);

    const QString &PlayCommand() const;
    void setPlayCommand(const QString &playCommand);

    ParentalLevel::Level ShowLevel() const;
    void setShowLevel(ParentalLevel::Level showLevel);

    const QString& Filename() const;
    void setFilename(const QString &filename);

    QString getFilenameNoPrefix() const;

    const QString &CoverFile() const;
    void setCoverFile(const QString &coverFile);

    const QString &Category() const;
//    void setCategory(const QString &category);

    const genre_list &Genres() const;
    void setGenres(const genre_list &genres);

    const cast_list &getCast() const;
    void setCast(const cast_list &cast);

    const country_list &Countries() const;
    void setCountries(const country_list &countries);

    int getCategoryID() const;
    void setCategoryID(int id);

    void dumpToDatabase();
    void updateDatabase();
//    bool fillDataFromID(const MetadataListManager &cache);
    bool fillDataFromFilename(const MetadataListManager &cache);

    // If you aren't VideoList don't call this
    bool deleteFile();

    // drops the metadata from the DB
    bool dropFromDB();

    /// Resets to default metadata
    void Reset();

  private:
    class MetadataImp *m_imp;
};

bool operator==(const Metadata &a, const Metadata &b);
bool operator!=(const Metadata &a, const Metadata &b);

bool operator<(const Metadata::SortKey &lhs, const Metadata::SortKey &rhs);

#endif
