#ifndef VIDEOLIST_H_
#define VIDEOLIST_H_

// Type of the item added to the tree
enum TreeNodeType {
    kSubFolder = -1,
    kUpFolder = -2,
    kRootNode = -3
};

// Tree node attribute index
enum TreeNodeAttributes {
    kNodeSort
};

class MythGenericTree;
class VideoFilterSettings;
class MetadataListManager;
class ParentalLevel;

class VideoList
{
  public:
    VideoList();
    ~VideoList();

    MythGenericTree *buildVideoList(bool filebrowser, bool flatlist,
                                const ParentalLevel &parental_level,
                                bool include_updirs);

    void refreshList(bool filebrowser, const ParentalLevel &parental_level,
                     bool flatlist);

    unsigned int count() const;

    const VideoFilterSettings &getCurrentVideoFilter();
    void setCurrentVideoFilter(const VideoFilterSettings &filter);

    // returns the number of videos matched by this filter
    int TryFilter(const VideoFilterSettings &filter) const;

    unsigned int getFilterChangedState();

    bool Delete(int video_id);

    const MetadataListManager &getListCache() const;

    MythGenericTree *GetTreeRoot();

  private:
    class VideoListImp *m_imp;
};

class Metadata;
class TreeNodeData
{
  public:
    TreeNodeData();
    TreeNodeData(Metadata *metadata);
    TreeNodeData(QString path, QString host, QString prefix);

    TreeNodeData(const TreeNodeData &other);
    TreeNodeData &operator=(const TreeNodeData &rhs);

    ~TreeNodeData();

    Metadata *GetMetadata();
    const Metadata *GetMetadata() const;
    QString GetPath() const;
    QString GetHost() const;
    QString GetPrefix() const;

  private:
    class TreeNodeDataPrivate *m_d;
};

Q_DECLARE_METATYPE(TreeNodeData);

#endif // VIDEOLIST_H
