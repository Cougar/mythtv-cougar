// POSIX headers
#include <stdlib.h>

#ifndef USING_MINGW // dlfcn for mingw defined in compat.h
#include <dlfcn.h> // needed for dlopen(), dlerror(), dlsym(), and dlclose()
#else
#include "compat.h"
#endif

// Qt headers
#include <QDir>
#include <QStringList>

// MythTV headers
#include "filtermanager.h"
#include "mythcontext.h"
#include "libmythdb/mythdirs.h"
#include "libmythdb/mythverbose.h"

#define LOC QString("FilterManager: ")
#define LOC_WARN QString("FilterManager, Warning: ")
#define LOC_ERR QString("FilterManager, Error: ")

static const char *FmtToString(VideoFrameType ft)
{
    switch(ft)
    {
        case FMT_NONE:
            return "NONE";
        case FMT_RGB24:
            return "RGB24";
        case FMT_YV12:
            return "YV12";
        case FMT_ARGB32:
            return "ARGB32";
        case FMT_YUV422P:
            return "YUV422P";
        default:
            return "INVALID";
    }
}

FilterChain::~FilterChain()
{
    vector<VideoFilter*>::iterator it = filters.begin();
    for (; it != filters.end(); ++it)
    {
        VideoFilter *filter = *it;
        if (filter->opts)
            free(filter->opts);
        if (filter->cleanup)
            filter->cleanup(filter);
        dlclose(filter->handle);
        free(filter);
    }
    filters.clear();
}

void FilterChain::ProcessFrame(VideoFrame *frame)
{
    if (!frame)
        return;

    vector<VideoFilter*>::iterator it = filters.begin();
    for (; it != filters.end(); ++it)
        (*it)->filter(*it, frame);
}

FilterManager::FilterManager()
{
    QDir FiltDir(GetFiltersDir());

    FiltDir.setFilter(QDir::Files | QDir::Readable);
    if (FiltDir.exists())
    {
        QStringList LibList = FiltDir.entryList();
        for (QStringList::iterator i = LibList.begin(); i != LibList.end();
             i++)
        {
            QString path = FiltDir.filePath(*i);
            if (path.length() <= 1)
                continue;

            VERBOSE(VB_PLAYBACK+VB_FILE, LOC +
                    QString("Loading filter '%1'").arg(path));

            if (!LoadFilterLib(path))
            {
                VERBOSE(VB_IMPORTANT, LOC_WARN +
                        QString("Failed to load filter library: %1").arg(path));
            }
        }
    }
    else
        VERBOSE(VB_IMPORTANT,
                "Filter dir '" + FiltDir.absolutePath() + "' doesn't exist?");
}

FilterManager::~FilterManager()
{
    filter_map_t::iterator itf = filters.begin();
    for (; itf != filters.end(); ++itf)
    {
        FilterInfo *tmp = itf->second;
        itf->second = NULL;

        free(tmp->symbol);
        free(tmp->name);
        free(tmp->descript);
        free(tmp->libname);
        delete [] (tmp->formats);
        delete tmp;
    }
    filters.clear();

    library_map_t::iterator ith = dlhandles.begin();
    for (; ith != dlhandles.end(); ++ith)
    {
        void *tmp = ith->second;
        ith->second = NULL;
        dlclose(tmp);
    }
    dlhandles.clear();
}

bool FilterManager::LoadFilterLib(const QString &path)
{
    dlerror(); // clear out any pre-existing dlerrors

    void *dlhandle = NULL;
    library_map_t::iterator it = dlhandles.find(path);
    if (it != dlhandles.end())
        dlhandle = it->second;

    if (!dlhandle)
    {
        QByteArray apath = path.toAscii();
        dlhandle = dlopen(apath.constData(), RTLD_LAZY);
        if (!dlhandle)
        {
            const char *errmsg = dlerror();
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to load filter library: " +
                    QString("'%1'").arg(path) + "\n\t\t\t" + errmsg);
            return false;
        }
        dlhandles[path] = dlhandle;
    }

    FilterInfo *filtInfo = (FilterInfo*) dlsym(dlhandle, "filter_table");
    if (!filtInfo)
    {
        const char *errmsg = dlerror();
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to load filter symbol: " +
                QString("'%1'").arg(path) + "\n\t\t\t" + errmsg);
        return false;        
    }

    for (; filtInfo->symbol; filtInfo++)
    {
        if (!filtInfo->symbol || !filtInfo->name || !filtInfo->formats)
            break;

        FilterInfo *newFilter = new FilterInfo;
        newFilter->symbol   = strdup(filtInfo->symbol);
        newFilter->name     = strdup(filtInfo->name);
        newFilter->descript = strdup(filtInfo->descript);

        int i = 0;
        for (; filtInfo->formats[i].in != FMT_NONE; i++);

        newFilter->formats = new FmtConv[i + 1];
        memcpy(newFilter->formats, filtInfo->formats,
               sizeof(FmtConv) * (i + 1));

        QByteArray libname = path.toAscii();
        newFilter->libname = strdup(libname.constData());
        filters[newFilter->name] = newFilter;
        VERBOSE(VB_PLAYBACK+VB_EXTRA, LOC + QString("filters[%1] = ")
                .arg(newFilter->name) << newFilter);
    }
    return true;
}

const FilterInfo *FilterManager::GetFilterInfo(const QString &name) const
{
    const FilterInfo *finfo = NULL;
    filter_map_t::const_iterator it = filters.find(name);
    if (it != filters.end())
        finfo = it->second;

    VERBOSE(VB_PLAYBACK, LOC + QString("GetFilterInfo(%1)").arg(name) +
            " returning: "<<finfo);

    return finfo;
}

FilterChain *FilterManager::LoadFilters(QString Filters, 
                                        VideoFrameType &inpixfmt,
                                        VideoFrameType &outpixfmt, int &width,
                                        int &height, int &bufsize)
{
    if (Filters.toLower() == "none")
        return NULL;

    vector<const FilterInfo*> FiltInfoChain;
    FilterChain *FiltChain = new FilterChain;
    vector<FmtConv*> FmtList;
    const FilterInfo *FI;
    const FilterInfo *FI2;
    QString Opts;
    const FilterInfo *Convert = GetFilterInfo("convert");
    QStringList OptsList;
    QStringList FilterList = Filters.split(",", QString::SkipEmptyParts);
    VideoFilter *NewFilt = NULL;
    FmtConv *FC, *FC2, *S1, *S2, *S3;
    VideoFrameType ifmt;
    unsigned int i;
    int nbufsize;
    int cbufsize;
    int postfilt_width = width;
    int postfilt_height = height;

    for (QStringList::Iterator i = FilterList.begin();
         i != FilterList.end(); ++i)
    {
        QString FiltName = (*i).section('=', 0, 0);
        QString FiltOpts = (*i).section('=', 1);

        if (FiltName.contains("opengl"))
            continue;

        FI = GetFilterInfo(FiltName);

        if (FI)
        {
            FiltInfoChain.push_back(FI);
            OptsList.push_back(FiltOpts);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC + QString(
                        "Failed to load filter '%1', "
                        "no such filter exists").arg(FiltName));
            FiltInfoChain.clear();
            break;
        }
    }

    ifmt = inpixfmt;
    for (i = 0; i < FiltInfoChain.size(); i++)
    {
        S1 = S2 = S3 = NULL;
        FI = FiltInfoChain[i];
        if (FiltInfoChain.size() - i == 1)
        {
            for (FC = FI->formats; FC->in != FMT_NONE; FC++)
            {
                if (FC->out == outpixfmt && FC->in == ifmt)
                {
                    S1 = FC;
                    break;
                }
                if (FC->in == ifmt && !S2)
                    S2 = FC;
                if (FC->out == outpixfmt && !S3)
                    S3 = FC;
            }
        }
        else
        {
            FI2 = FiltInfoChain[i+1];
            for (FC = FI->formats; FC->in != FMT_NONE; FC++)
            {
                for (FC2 = FI2->formats; FC2->in != FMT_NONE; FC2++)
                {
                    if (FC->in == ifmt && FC->out == FC2->in)
                    {
                        S1 = FC;
                        break;
                    }
                    if (FC->out == FC2->in && !S3)
                        S3 = FC;
                }
                if (S1)
                    break;
                if (FC->in == ifmt && !S2)
                    S2 = FC;
            }
        }
                
        if (S1)
            FC = S1;
        else if (S2)
            FC = S2;
        else if (S3)
            FC = S3;
        else
            FC = FI->formats;

        if (FC->in != ifmt && (i > 0 || ifmt != FMT_NONE))
        {
            if (!Convert)
            {
                VERBOSE(VB_IMPORTANT, "FilterManager: format conversion "
                        "needed but convert filter not found");
                FiltInfoChain.clear();
                break;
            }
            FiltInfoChain.insert(FiltInfoChain.begin() + i, Convert);
            OptsList.insert(i, QString ());
            FmtList.push_back(new FmtConv);
            if (FmtList.back())
            {
                FmtList.back()->in = ifmt;
                FmtList.back()->out = FC->in;
                i++;
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "FilterManager: memory allocation "
                        "failure, returning empty filter chain");
                FiltInfoChain.clear();
                break;
            }
        }
        FmtList.push_back(new FmtConv);
        if (FmtList.back())
        {
            FmtList.back()->in = FC->in;
            FmtList.back()->out = FC->out;
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "FilterManager: memory allocation failure, "
                    "returning empty filter chain");
            FiltInfoChain.clear();
            break;
        }
        ifmt = FC->out;
    }

    if (ifmt != outpixfmt && outpixfmt != FMT_NONE &&
        (FiltInfoChain.size() || inpixfmt != FMT_NONE))
    {
        if (!Convert)
        {
            VERBOSE(VB_IMPORTANT, "FilterManager: format conversion "
                    "needed but convert filter not found");
            FiltInfoChain.clear();
        }
        else
        {
            FiltInfoChain.push_back(Convert);
            OptsList.push_back( QString ());
            FmtList.push_back(new FmtConv);
            if (FmtList.back())
            {
                FmtList.back()->in = ifmt;
                FmtList.back()->out = outpixfmt;
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "FilterManager: memory allocation "
                        "failure, returning empty filter chain");
                FiltInfoChain.clear();
            }
        }
    }

    nbufsize = -1;

    if (!FiltInfoChain.size())
    {
        delete FiltChain;
        FiltChain = NULL;
    }
    
    for (i = 0; i < FiltInfoChain.size(); i++)
    {
        QByteArray tmp = OptsList[i].toLocal8Bit();
        NewFilt = LoadFilter(FiltInfoChain[i], FmtList[i]->in,
                             FmtList[i]->out, postfilt_width, 
                             postfilt_height, tmp.constData());

        if (!NewFilt)
        {
            delete FiltChain;
            VERBOSE(VB_IMPORTANT,QString("FilterManager: failed to load "
                        "filter %1 %2->%3 with args %4")
                    .arg(FiltInfoChain[i]->name)
                    .arg(FmtToString(FmtList[i]->in))
                    .arg(FmtToString(FmtList[i]->out))
                    .arg(OptsList[i])
                   );
            FiltChain = NULL;
            nbufsize = -1;
            break;
        }

        if (NewFilt->filter)
        {
            FiltChain->Append(NewFilt);
        }
        else
        {
            if (NewFilt->opts)
                free(NewFilt->opts);
            if (NewFilt->cleanup)
                NewFilt->cleanup(NewFilt);
            dlclose(NewFilt->handle);
            free(NewFilt);
        }

        switch (FmtList[i]->out)
        {
            case FMT_YV12:
                cbufsize = postfilt_width * postfilt_height * 3 / 2;
                break;
            case FMT_YUV422P:
                cbufsize = postfilt_width * postfilt_height * 2;
                break;
            case FMT_RGB24:
                cbufsize = postfilt_width * postfilt_height * 3;
                break;
            case FMT_ARGB32:
                cbufsize = postfilt_width * postfilt_height * 4;
                break;
            default:
                cbufsize = 0;
        }

        if (cbufsize > nbufsize)
            nbufsize = cbufsize;
    }

    if (FiltChain)
    {
        if (inpixfmt == FMT_NONE)
            inpixfmt = FmtList.front()->in;
        if (outpixfmt == FMT_NONE)
            inpixfmt = FmtList.back()->out;
        width = postfilt_width;
        height = postfilt_height;
    }
    else
    {
        if (inpixfmt == FMT_NONE && outpixfmt == FMT_NONE)
            inpixfmt = outpixfmt = FMT_YV12;
        else if (inpixfmt == FMT_NONE)
            inpixfmt = outpixfmt;
        else if (outpixfmt == FMT_NONE)
            outpixfmt = inpixfmt;
    }

    switch (inpixfmt)
    {
        case FMT_YV12:
            cbufsize = postfilt_width * postfilt_height * 3 / 2;
            break;
        case FMT_YUV422P:
            cbufsize = postfilt_width * postfilt_height * 2;
            break;
        case FMT_RGB24:
            cbufsize = postfilt_width * postfilt_height * 3;
            break;
        case FMT_ARGB32:
            cbufsize = postfilt_width * postfilt_height * 4;
            break;
        default:
            cbufsize = 0;
    }

    if (cbufsize > nbufsize)
        nbufsize = cbufsize;

    bufsize = nbufsize;

    vector<FmtConv*>::iterator it = FmtList.begin();
    for (; it != FmtList.end(); ++it)
        delete *it;
    FmtList.clear();

    return FiltChain;
}

VideoFilter * FilterManager::LoadFilter(const FilterInfo *FiltInfo, 
                                        VideoFrameType inpixfmt,
                                        VideoFrameType outpixfmt, int &width, 
                                        int &height, const char *opts)
{
    void *handle;
    VideoFilter *Filter;
    VideoFilter *(*InitFilter)(int, int, int *, int *, char *);

    if (FiltInfo == NULL)
    {
        VERBOSE(VB_IMPORTANT, "FilterManager: LoadFilter called with NULL"
                "FilterInfo");
        return NULL;
    }

    if (FiltInfo->libname == NULL)
    {
        VERBOSE(VB_IMPORTANT, "FilterManager: LoadFilter called with invalid "
                "FilterInfo (libname is NULL)");
        return NULL;
    }

    if (FiltInfo->symbol == NULL)
    {
        VERBOSE(VB_IMPORTANT, "FilterManager: LoadFilter called with invalid "
                "FilterInfo (symbol is NULL)");
        return NULL;
    }

    handle = dlopen(FiltInfo->libname, RTLD_NOW);

    if (!handle)
    {
        VERBOSE(VB_IMPORTANT, QString("FilterManager: unable to load "
                "shared library '%1', dlopen reports error '%2'")
                .arg(FiltInfo->libname)
                .arg(dlerror()));
        return NULL;
    }

    InitFilter =
        (VideoFilter * (*)(int, int, int *, int *, char *))dlsym(handle,
                                                                 FiltInfo->
                                                                 symbol);

    if (!InitFilter)
    {
        VERBOSE(VB_IMPORTANT, QString("FilterManager: unable to load symbol "
                "'%1' from shared library '%2', dlopen reports error '%3'")
                .arg(FiltInfo->symbol)
                .arg(FiltInfo->libname)
                .arg(dlerror()));
        dlclose(handle);
        return NULL;
    }

    Filter = (*InitFilter)(inpixfmt, outpixfmt, &width, &height,
                           const_cast<char*>(opts));

    if (Filter == NULL)
    {
        dlclose(handle);
        return NULL;
    }

    Filter->handle = handle;
    Filter->inpixfmt = inpixfmt;
    Filter->outpixfmt = outpixfmt;
    if (opts)
        Filter->opts = strdup(opts);
    else
        Filter->opts = NULL;
    Filter->info = const_cast<FilterInfo*>(FiltInfo);
    return Filter;
}
