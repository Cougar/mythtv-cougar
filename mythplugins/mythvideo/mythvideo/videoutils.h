#ifndef VIDEOUTILS_H_
#define VIDEOUTILS_H_

class Metadata;
class MetadataListManager;

void PlayVideo(const QString &filename, const MetadataListManager &video_list);

template <typename T>
void checkedSetText(T *item, const QString &text)
{
    if (item) item->SetText(text);
}

void checkedSetText(class LayerSet *container, const QString &item_name,
                           const QString &text);

QStringList GetVideoDirs();

QString getDisplayYear(int year);
QString getDisplayRating(const QString &rating);
QString getDisplayUserRating(float userrating);
QString getDisplayLength(int length);
QString getDisplayBrowse(bool browse);

bool isDefaultCoverFile(const QString &coverfile);

#endif // VIDEOUTILS_H_
