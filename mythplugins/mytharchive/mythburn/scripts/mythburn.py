# mythburn.py
# The ported MythBurn scripts which feature:

# Burning of recordings (including HDTV) and videos
# of ANY format to DVDR.  Menus are created using themes
# and are easily customised.

# See mydata.xml for format of input file

# spit2k1
# 11 January 2006
# 6 Feb 2006 - Added into CVS for the first time

# paulh
# 4 May 2006 - Added into mythtv svn

#For this script to work you need to have...
#Python2.3.5
#python2.3-mysqldb
#python2.3-imaging (PIL)
#dvdauthor - v0.6.11
#ffmpeg - 0.4.6
#dvd+rw-tools - v5.21.4.10.8
#cdrtools - v2.01

#Optional (only needed for tcrequant)
#transcode - v1.0.2

#Optional (for Right To Left languages)
#pyfribidi

#******************************************************************************
#******************************************************************************
#******************************************************************************

# version of script - change after each update
VERSION="0.1.20080106-1"


##You can use this debug flag when testing out new themes
##pick some small recordings, run them through as normal
##set this variable to True and then re-run the scripts
##the temp. files will not be deleted and it will run through
##very much quicker!
debug_secondrunthrough = False

# default encoding profile to use
defaultEncodingProfile = "SP"

# add audio sync offset when re-muxing
useSyncOffset = True

# if the theme doesn't have a chapter menu and this is set to true then the
# chapter marks will be set to the cut point end marks
addCutlistChapters = False

#*********************************************************************************
#Dont change the stuff below!!
#*********************************************************************************
import os, string, socket, sys, getopt, traceback, signal
import xml.dom.minidom
import Image, ImageDraw, ImageFont, ImageColor
import MySQLdb, codecs
import time, datetime, tempfile
from fcntl import ioctl
import CDROM
from shutil import copy

# media types (should match the enum in mytharchivewizard.h)
DVD_SL = 0
DVD_DL = 1
DVD_RW = 2
FILE   = 3

dvdPAL=(720,576)
dvdNTSC=(720,480)
dvdPALdpi=(75,80)
dvdNTSCdpi=(81,72)

dvdPALHalfD1="352x576"
dvdNTSCHalfD1="352x480"
dvdPALD1="%sx%s" % (dvdPAL[0],dvdPAL[1])
dvdNTSCD1="%sx%s" % (dvdNTSC[0],dvdNTSC[1])

#Single and dual layer recordable DVD free space in MBytes
dvdrsize=(4482,8964)

frameratePAL=25
framerateNTSC=29.97

#any aspect ratio above this value is assumed to be 16:9
aspectRatioThreshold = 1.4

#Just blank globals at startup
temppath=""
logpath=""
scriptpath=""
sharepath=""
videopath=""
defaultsettings=""
videomode=""
gallerypath=""
musicpath=""
dateformat=""
timeformat=""
dbVersion=""
preferredlang1=""
preferredlang2=""
useFIFO = True
encodetoac3 = False
alwaysRunMythtranscode = False
copyremoteFiles = False
thumboffset = 10
usebookmark = True
clearArchiveTable = True
nicelevel = 17;
drivespeed = 0;

#main menu aspect ratio (4:3 or 16:9)
mainmenuAspectRatio = "16:9"

#chapter menu aspect ratio (4:3, 16:9 or Video) 
#video means same aspect ratio as the video title
chaptermenuAspectRatio = "Video"

#default chapter length in seconds
chapterLength = 5 * 60;

#name of the default job file
jobfile="mydata.xml"

#progress log filename and file object
progresslog = ""
progressfile = open("/dev/null", 'w')

#default location of DVD drive
dvddrivepath = "/dev/dvd"

#default option settings
docreateiso = False
doburn = True
erasedvdrw = False
mediatype = DVD_SL
savefilename = ''

configHostname = socket.gethostname()
installPrefix = ""

# job xml file
jobDOM = None

# theme xml file
themeDOM = None
themeName = ''

#dictionary of font definitions used in theme
themeFonts = {}

# no. of processors we have access to
cpuCount = 1

#############################################################

# fix rtl text where pyfribidi is not available
# should write a simple algorithm, meanwhile just return the original string
def simple_fix_rtl(str):
  return str

# Bind the name fix_rtl to the appropriate function
try:
    import pyfribidi
except ImportError:
    sys.stdout.write("Using simple_fix_rtl\n")
    fix_rtl = simple_fix_rtl
else:
    sys.stdout.write("Using pyfribidi.log2vis\n")
    fix_rtl = pyfribidi.log2vis

#############################################################
# class to hold a font definition

class FontDef(object):
    def __init__(self, name=None, fontFile=None, size=19, color="white", effect="normal", shadowColor="black", shadowSize=1):
        self.name = name
        self.fontFile = fontFile
        self.size = size
        self.color = color
        self.effect = effect
        self.shadowColor = shadowColor
        self.shadowSize = shadowSize
        self.font = None

    def getFont(self):
        if self.font == None:
            self.font = ImageFont.truetype(self.fontFile, int(self.size))

        return self.font

    def drawText(self, text, color=None):
        if self.font == None:
            self.font = ImageFont.truetype(self.fontFile, int(self.size))

        if color == None:
            color = self.color

        textwidth, textheight = self.font.getsize(text)

        image = Image.new("RGBA", (textwidth + (self.shadowSize * 2), textheight), (0,0,0,0))
        draw = ImageDraw.ImageDraw(image)

        if self.effect == "shadow":
            draw.text((self.shadowSize,self.shadowSize), text, font=self.font, fill=self.shadowColor)
            draw.text((0,0), text, font=self.font, fill=color)
        elif self.effect == "outline":
            for x in range(0, self.shadowSize * 2 + 1):
                for y in range(0, self.shadowSize * 2 + 1):
                    draw.text((x, y), text, font=self.font, fill=self.shadowColor)

            draw.text((self.shadowSize,self.shadowSize), text, font=self.font, fill=color)
        else:
            draw.text((0,0), text, font=self.font, fill=color)

        bbox = image.getbbox()
        image = image.crop(bbox)
        return image

#############################################################
# Write a string to stdout and optionaly to a progress log file

def write(text, progress=True):
    """Simple place to channel all text output through"""
    sys.stdout.write(text + "\n")
    sys.stdout.flush()

    if progress == True and progresslog != "":
        progressfile.write(time.strftime("%Y-%m-%d %H:%M:%S ") + text + "\n")
        progressfile.flush()

#############################################################
# Display an error message and exit

def fatalError(msg):
    """Display an error message and exit app"""
    write("*"*60)
    write("ERROR: " + msg)
    write("*"*60)
    write("")
    saveSetting("MythArchiveLastRunResult", "Failed: " + quoteString(msg));
    saveSetting("MythArchiveLastRunEnd", time.strftime("%Y-%m-%d %H:%M:%S "))
    sys.exit(0)

#############################################################
# Return the input string with single quotes escaped.

def quoteString(str):
     """Return the input string with single quotes escaped."""
     return str.replace("'", "'\"'\"'")

#############################################################
# Directory where all temporary files will be created.

def getTempPath():
    """This is the folder where all temporary files will be created."""
    return temppath

#############################################################
# Try to work out how many cpus we have available

def getCPUCount():
    """return the number of CPU's"""
    cpustat = open("/proc/cpuinfo")
    cpudata = cpustat.readlines()
    cpustat.close()

    cpucount = 0
    for line in cpudata:
        tokens = line.split()
        if len(tokens) > 0:
            if tokens[0] == "processor":
                cpucount += 1

    if cpucount == 0:
        cpucount = 1

    write("Found %d CPUs" % cpucount)

    return cpucount

#############################################################
# Get the directory where all encoder profile files are located.

def getEncodingProfilePath():
    """This is the folder where all encoder profile files are located."""
    return os.path.join(sharepath, "mytharchive", "encoder_profiles")

#############################################################
# Get the connection parameters needed to connect to the mythconverg DB 

def getMysqlDBParameters():
    global mysql_host
    global mysql_user
    global mysql_passwd
    global mysql_db
    global configHostname
    global installPrefix

    f = tempfile.NamedTemporaryFile();
    result = os.spawnlp(os.P_WAIT, 'mytharchivehelper','mytharchivehelper',
                        '-p', f.name)
    if result <> 0:
        write("Failed to run mytharchivehelper to get mysql database parameters! "
              "Exit code: %d" % result)
        if result == 254: 
            fatalError("Failed to init mythcontext.\n"
                       "Please check the troubleshooting section of the README for ways to fix this error")

    f.seek(0)
    mysql_host = f.readline()[:-1]
    mysql_user = f.readline()[:-1]
    mysql_passwd = f.readline()[:-1]
    mysql_db = f.readline()[:-1]
    configHostname = f.readline()[:-1]
    installPrefix = f.readline()[:-1]
    f.close()
    del f

#############################################################
# Returns a mySQL connection to mythconverg database.

def getDatabaseConnection():
    """Returns a mySQL connection to mythconverg database."""
    return MySQLdb.connect(host=mysql_host, user=mysql_user, passwd=mysql_passwd, db=mysql_db)

#############################################################
# Returns true/false if a given file or path exists.

def doesFileExist(file):
    """Returns true/false if a given file or path exists."""
    return os.path.exists( file )

#############################################################
# Escape quotes in a filename

def quoteFilename(filename):
    filename = filename.replace('"', '\\"')
    filename = filename.replace('`', '\\`')
    return '"%s"' % filename

#############################################################
# Returns the text contents from a given XML element.

def getText(node):
    """Returns the text contents from a given XML element."""
    if node.childNodes.length>0:
        return node.childNodes[0].data
    else:
        return ""

#############################################################
# Try to find a theme file

def getThemeFile(theme,file):
    """Find a theme file - first look in the specified theme directory then look in the
       shared music and image directories"""
    if os.path.exists(os.path.join(sharepath, "mytharchive", "themes", theme, file)):
        return os.path.join(sharepath, "mytharchive", "themes", theme, file)

    if os.path.exists(os.path.join(sharepath, "mytharchive", "images", file)):
        return os.path.join(sharepath, "mytharchive", "images", file)

    if os.path.exists(os.path.join(sharepath, "mytharchive", "intro", file)):
        return os.path.join(sharepath, "mytharchive", "intro", file)

    if os.path.exists(os.path.join(sharepath, "mytharchive", "music", file)):
        return os.path.join(sharepath, "mytharchive", "music", file)

    fatalError("Cannot find theme file '%s' in theme '%s'" % (file, theme))

#############################################################
# Returns the path where we can find our fonts

def getFontPathName(fontname):
    return os.path.join(sharepath, fontname)

#############################################################
# Creates a file path where the temp files for a video file can be created

def getItemTempPath(itemnumber):
    return os.path.join(getTempPath(),"%s" % itemnumber)

#############################################################
# Returns True if the theme.xml file can be found for the given theme

def validateTheme(theme):
    #write( "Checking theme", theme
    file = getThemeFile(theme,"theme.xml")
    write("Looking for: " + file)
    return doesFileExist( getThemeFile(theme,"theme.xml") )

#############################################################
# Returns True if the given resolution is a DVD compliant one

def isResolutionOkayForDVD(videoresolution):
    if videomode=="ntsc":
        return videoresolution==(720,480) or videoresolution==(704,480) or videoresolution==(352,480) or videoresolution==(352,240)
    else:
        return videoresolution==(720,576) or videoresolution==(704,576) or videoresolution==(352,576) or videoresolution==(352,288)

#############################################################
# Romoves all the files from a directory

def deleteAllFilesInFolder(folder):
    """Does what it says on the tin!."""
    for root, dirs, deletefiles in os.walk(folder, topdown=False):
        for name in deletefiles:
                os.remove(os.path.join(root, name))

#############################################################
# Check to see if the user has cancelled the DVD creation process

def checkCancelFlag():
    """Checks to see if the user has cancelled this run"""
    if os.path.exists(os.path.join(logpath, "mythburncancel.lck")):
        os.remove(os.path.join(logpath, "mythburncancel.lck"))
        write('*'*60)
        write("Job has been cancelled at users request")
        write('*'*60)
        sys.exit(1)

#############################################################
# Runs an external command checking to see if the user has cancelled
# the DVD creation process

def runCommand(command):
    checkCancelFlag()

    result = os.system(command)

    if os.WIFEXITED(result):
        result = os.WEXITSTATUS(result)
    checkCancelFlag()
    return result

#############################################################
# Convert a time in seconds to a frame number

def secondsToFrames(seconds):
    """Convert a time in seconds to a frame position"""
    if videomode=="pal":
        framespersecond=frameratePAL
    else:
        framespersecond=framerateNTSC

    frames=int(seconds * framespersecond)
    return frames

#############################################################
# Creates a short mpeg file from a jpeg image and an ac3 sound track

def encodeMenu(background, tempvideo, music, musiclength, tempmovie, xmlfile, finaloutput, aspectratio):
    if videomode=="pal":
        framespersecond=frameratePAL
    else:
        framespersecond=framerateNTSC

    totalframes=int(musiclength * framespersecond)

    command = path_jpeg2yuv[0] + " -n %s -v0 -I p -f %s -j '%s' | %s -b 5000 -a %s -v 1 -f 8 -o '%s'" \
              % (totalframes, framespersecond, background, path_mpeg2enc[0], aspectratio, tempvideo)
    result = runCommand(command)
    if result<>0:
        fatalError("Failed while running jpeg2yuv - %s" % command)

    command = path_mplex[0] + " -f 8 -v 0 -o '%s' '%s' '%s'" % (tempmovie, tempvideo, music)
    result = runCommand(command)
    if result<>0:
        fatalError("Failed while running mplex - %s" % command)

    if xmlfile != "":
        command = path_spumux[0] + " -m dvd -s 0 '%s' < '%s' > '%s'" % (xmlfile, tempmovie, finaloutput)
        result = runCommand(command)
        if result<>0:
            fatalError("Failed while running spumux - %s" % command)
    else:
        os.rename(tempmovie, finaloutput)

    if os.path.exists(tempvideo):
            os.remove(tempvideo)
    if os.path.exists(tempmovie):
            os.remove(tempmovie)

#############################################################
# Return an xml node from a re-encoding profile xml file for 
# a given profile name

def findEncodingProfile(profile):
    """Returns the XML node for the given encoding profile"""

    # which encoding file do we need

    # first look for a custom profile file in ~/.mythtv/MythArchive/
    if videomode == "ntsc":
        filename = os.path.expanduser("~/.mythtv/MythArchive/ffmpeg_dvd_ntsc.xml")
    else:
        filename = os.path.expanduser("~/.mythtv/MythArchive/ffmpeg_dvd_pal.xml")

    if not os.path.exists(filename):
        # not found so use the default profiles
        if videomode == "ntsc":
            filename = getEncodingProfilePath() + "/ffmpeg_dvd_ntsc.xml"
        else:
            filename = getEncodingProfilePath() + "/ffmpeg_dvd_pal.xml"

    write("Using encoder profiles from %s" % filename)

    DOM = xml.dom.minidom.parse(filename)

    #Error out if its the wrong XML
    if DOM.documentElement.tagName != "encoderprofiles":
        fatalError("Profile xml file doesn't look right (%s)" % filename)

    profiles = DOM.getElementsByTagName("profile")
    for node in profiles:
        if getText(node.getElementsByTagName("name")[0]) == profile:
            write("Encoding profile (%s) found" % profile)
            return node

    fatalError("Encoding profile (%s) not found" % profile)
    return None

#############################################################
# Load the theme.xml file for a DVD theme

def getThemeConfigurationXML(theme):
    """Loads the XML file from disk for a specific theme"""

    #Load XML input file from disk
    themeDOM = xml.dom.minidom.parse( getThemeFile(theme,"theme.xml") )
    #Error out if its the wrong XML
    if themeDOM.documentElement.tagName != "mythburntheme":
        fatalError("Theme xml file doesn't look right (%s)" % theme)
    return themeDOM

#############################################################
# Gets the duration of a video file from its stream info file

def getLengthOfVideo(index):
    """Returns the length of a video file (in seconds)"""

    #open the XML containing information about this file
    infoDOM = xml.dom.minidom.parse(os.path.join(getItemTempPath(index), 'streaminfo.xml'))

    #error out if its the wrong XML
    if infoDOM.documentElement.tagName != "file":
        fatalError("Stream info file doesn't look right (%s)" % os.path.join(getItemTempPath(index), 'streaminfo.xml'))
    file = infoDOM.getElementsByTagName("file")[0]
    if file.attributes["duration"].value != 'N/A':
        duration = int(file.attributes["duration"].value)
    else:
        duration = 0;

    return duration

#############################################################
# Gets the audio sample rate and number of channels of a video file 
# from its stream info file

def getAudioParams(folder):
    """Returns the audio bitrate and no of channels for a file from its streaminfo.xml"""

    #open the XML containing information about this file
    infoDOM = xml.dom.minidom.parse(os.path.join(folder, 'streaminfo.xml'))

    #error out if its the wrong XML
    if infoDOM.documentElement.tagName != "file":
        fatalError("Stream info file doesn't look right (%s)" % os.path.join(folder, 'streaminfo.xml'))
    audio = infoDOM.getElementsByTagName("file")[0].getElementsByTagName("streams")[0].getElementsByTagName("audio")[0]

    samplerate = audio.attributes["samplerate"].value
    channels = audio.attributes["channels"].value

    return (samplerate, channels)

#############################################################
# Gets the video resolution, frames per second and aspect ratio
# of a video file from its stream info file

def getVideoParams(folder):
    """Returns the video resolution, fps and aspect ratio for the video file from the streamindo.xml file"""

    #open the XML containing information about this file
    infoDOM = xml.dom.minidom.parse(os.path.join(folder, 'streaminfo.xml'))

    #error out if its the wrong XML
    if infoDOM.documentElement.tagName != "file":
        fatalError("Stream info file doesn't look right (%s)" % os.path.join(getItemTempPath(index), 'streaminfo.xml'))
    video = infoDOM.getElementsByTagName("file")[0].getElementsByTagName("streams")[0].getElementsByTagName("video")[0]

    if video.attributes["aspectratio"].value != 'N/A':
        aspect_ratio = video.attributes["aspectratio"].value
    else:
        aspect_ratio = "1.77778" 

    videores = video.attributes["width"].value + 'x' + video.attributes["height"].value
    fps = video.attributes["fps"].value

    #sanity check the fps
    if videomode=="pal":
        fr=frameratePAL
    else:
        fr=framerateNTSC

    if float(fr) != float(fps):
        write("WARNING: frames rates do not match")
        write("The frame rate for %s should be %s but the stream info file "
              "report a fps of %s" % (videomode, fr, fps))
        fps = fr

    return (videores, fps, aspect_ratio)

#############################################################
# Gets the aspect ratio of a video file from its stream info file

def getAspectRatioOfVideo(index):
    """Returns the aspect ratio of the video file (1.333, 1.778, etc)"""

    #open the XML containing information about this file
    infoDOM = xml.dom.minidom.parse(os.path.join(getItemTempPath(index), 'streaminfo.xml'))

    #error out if its the wrong XML
    if infoDOM.documentElement.tagName != "file":
        fatalError("Stream info file doesn't look right (%s)" % os.path.join(getItemTempPath(index), 'streaminfo.xml'))
    video = infoDOM.getElementsByTagName("file")[0].getElementsByTagName("streams")[0].getElementsByTagName("video")[0]
    if video.attributes["aspectratio"].value != 'N/A':
        aspect_ratio = float(video.attributes["aspectratio"].value)
    else:
        aspect_ratio = 1.77778; # default
    write("aspect ratio is: %s" % aspect_ratio)
    return aspect_ratio

#############################################################
# Calculates the sync offset between the video and first audio stream

def calcSyncOffset(index):
    """Returns the sync offset between the video and first audio stream"""

    #open the XML containing information about this file
    #infoDOM = xml.dom.minidom.parse(os.path.join(getItemTempPath(index), 'streaminfo_orig.xml'))
    infoDOM = xml.dom.minidom.parse(os.path.join(getItemTempPath(index), 'streaminfo.xml'))

    #error out if its the wrong XML
    if infoDOM.documentElement.tagName != "file":
        fatalError("Stream info file doesn't look right (%s)" % os.path.join(getItemTempPath(index), 'streaminfo_orig.xml'))

    video = infoDOM.getElementsByTagName("file")[0].getElementsByTagName("streams")[0].getElementsByTagName("video")[0]
    video_start = float(video.attributes["start_time"].value)

    audio = infoDOM.getElementsByTagName("file")[0].getElementsByTagName("streams")[0].getElementsByTagName("audio")[0]
    audio_start = float(audio.attributes["start_time"].value)

#    write("Video start time is: %s" % video_start)
#    write("Audio start time is: %s" % audio_start)

    sync_offset = int((video_start - audio_start) * 1000)

#    write("Sync offset is: %s" % sync_offset)
    return sync_offset

#############################################################
# Gets the length of a video file and returns it as a string

def getFormatedLengthOfVideo(index):
    duration = getLengthOfVideo(index)

    minutes = int(duration / 60)
    seconds = duration % 60
    hours = int(minutes / 60)
    minutes %= 60

    return '%02d:%02d:%02d' % (hours, minutes, seconds)

#############################################################
# Convert a frame number to a time string

def frameToTime(frame, fps):
    sec = int(frame / fps)
    frame = frame - int(sec * fps)
    mins = sec / 60
    sec %= 60
    hour = mins / 60
    mins %= 60

    return '%02d:%02d:%02d' % (hour, mins, sec)

#############################################################
# Creates a set of chapter points evenly spread thoughout a file
# Optionally grabs the thumbnails from the file

def createVideoChapters(itemnum, numofchapters, lengthofvideo, getthumbnails):
    """Returns numofchapters chapter marks even spaced through a certain time period"""

    # if there are user defined thumb images already available use them
    infoDOM = xml.dom.minidom.parse(os.path.join(getItemTempPath(itemnum),"info.xml"))
    thumblistNode = infoDOM.getElementsByTagName("thumblist")
    if thumblistNode.length > 0:
        thumblist = getText(thumblistNode[0])
        write("Using user defined thumb images - %s" % thumblist)
        return thumblist

    # no user defined thumbs so create them
    segment=int(lengthofvideo / numofchapters)

    write( "Video length is %s seconds. Each chapter will be %s seconds" % (lengthofvideo,segment))

    chapters=""

    thumbList=""
    starttime=0
    count=1
    while count<=numofchapters:
        chapters+=time.strftime("%H:%M:%S",time.gmtime(starttime))

        if starttime==0:
            if thumboffset < segment:
                thumbList+="%s," % thumboffset
            else:
                thumbList+="%s," % starttime
        else:
            thumbList+="%s," % starttime

        if numofchapters>1:
            chapters+=","

        starttime+=segment
        count+=1

    if getthumbnails==True:
        extractVideoFrames( os.path.join(getItemTempPath(itemnum),"stream.mv2"),
            os.path.join(getItemTempPath(itemnum),"chapter-%1.jpg"), thumbList)

    return chapters

#############################################################
# Creates some fixed length chapter marks

def createVideoChaptersFixedLength(itemnum, segment, lengthofvideo): 
    """Returns chapter marks at cut list ends, 
       or evenly spaced chapters 'segment' seconds through the file"""


    if addCutlistChapters == True:
        # we've been asked to use the cut list as chapter marks
        # so if there is a cut list available, use it

        infoDOM = xml.dom.minidom.parse(os.path.join(getItemTempPath(itemnum),"info.xml"))
        chapterlistNode = infoDOM.getElementsByTagName("chapterlist")
        if chapterlistNode.length > 0:
            chapterlist = getText(chapterlistNode[0])
            write("Using commercial end marks - %s" % chapterlist)
            return chapterlist

    if lengthofvideo < segment:
        return "00:00:00"

    numofchapters = lengthofvideo / segment;
    chapters = ""
    starttime = 0
    count = 1
    while count <= numofchapters:
        chapters += time.strftime("%H:%M:%S", time.gmtime(starttime)) + ","
        starttime += segment
        count += 1

    return chapters

#############################################################
# Reads a load of settings from DB

def getDefaultParametersFromMythTVDB():
    """Reads settings from MythTV database"""

    write( "Obtaining MythTV settings from MySQL database for hostname " + configHostname)

    #TVFormat is not dependant upon the hostname.
    sqlstatement="""select value, data from settings where value in('DBSchemaVer') 
                    or (hostname='""" + configHostname + """' and value in(
                        'VideoStartupDir',
                        'GalleryDir',
                        'MusicLocation',
                        'MythArchiveVideoFormat',
                        'MythArchiveTempDir',
                        'MythArchiveFfmpegCmd',
                        'MythArchiveMplexCmd',
                        'MythArchiveDvdauthorCmd',
                        'MythArchiveMkisofsCmd',
                        'MythArchiveTcrequantCmd',
                        'MythArchiveMpg123Cmd',
                        'MythArchiveProjectXCmd',
                        'MythArchiveDVDLocation',
                        'MythArchiveGrowisofsCmd',
                        'MythArchiveJpeg2yuvCmd',
                        'MythArchiveSpumuxCmd',
                        'MythArchiveMpeg2encCmd',
                        'MythArchiveEncodeToAc3',
                        'MythArchiveCopyRemoteFiles',
                        'MythArchiveAlwaysUseMythTranscode',
                        'MythArchiveUseFIFO',
                        'MythArchiveMainMenuAR',
                        'MythArchiveChapterMenuAR',
                        'MythArchiveDateFormat',
                        'MythArchiveTimeFormat',
                        'MythArchiveClearArchiveTable',
                        'MythArchiveDriveSpeed',
                        'ISO639Language0',
                        'ISO639Language1',
                        'JobQueueCPU'
                        )) order by value"""

    #write( sqlstatement)

    # connect
    db = getDatabaseConnection()
    # create a cursor
    cursor = db.cursor()
    # execute SQL statement
    cursor.execute(sqlstatement)
    # get the resultset as a tuple
    result = cursor.fetchall()

    db.close()
    del db
    del cursor

    cfg = {}
    for i in range(len(result)):
       cfg[result[i][0]] = result[i][1]

    #bail out if we can't find the temp dir setting
    if not "MythArchiveTempDir" in cfg:
        fatalError("Can't find the setting for the temp directory. \nHave you run setup in the frontend?")
    return cfg

#############################################################
# Save a setting to the settings table in the DB

def saveSetting(name, data):
    db = getDatabaseConnection()
    cursor = db.cursor()

    query = "DELETE from settings WHERE value = '" + name + "' AND hostname = '" + configHostname + "'"
    cursor.execute(query)

    query = "INSERT INTO settings (value, data, hostname) VALUES ('" + name + "', '" + data + "', '" + configHostname + "')"
    cursor.execute(query)

    db.close()
    del db
    del cursor

#############################################################
# Remove all archive items from the archiveitems DB table

def clearArchiveItems():
    ''' Remove all archive items from the archiveitems DB table'''

    write("Removing all archive items from the archiveitems DB table")

    db = getDatabaseConnection()
    cursor = db.cursor()

    cursor.execute("DELETE from archiveitems;")

    db.close()
    del db
    del cursor

#############################################################
# Load the options from the options node passed in the job file

def getOptions(options):
    global doburn
    global docreateiso
    global erasedvdrw
    global mediatype
    global savefilename

    if options.length == 0:
        fatalError("Trying to read the options from the job file but none found?")
    options = options[0]

    doburn = options.attributes["doburn"].value != '0'
    docreateiso = options.attributes["createiso"].value != '0'
    erasedvdrw = options.attributes["erasedvdrw"].value != '0'
    mediatype = int(options.attributes["mediatype"].value)
    savefilename = options.attributes["savefilename"].value

    write("Options - mediatype = %d, doburn = %d, createiso = %d, erasedvdrw = %d" \
           % (mediatype, doburn, docreateiso, erasedvdrw))
    write("          savefilename = '%s'" % savefilename)

#############################################################
# Substitutes some text from a theme file with the required values

def expandItemText(infoDOM, text, itemnumber, pagenumber, keynumber,chapternumber, chapterlist ):
    """Replaces keywords in a string with variables from the XML and filesystem"""
    text=string.replace(text,"%page","%s" % pagenumber)

    #See if we can use the thumbnail/cover file for videos if there is one.
    if getText( infoDOM.getElementsByTagName("coverfile")[0]) =="":
        text=string.replace(text,"%thumbnail", os.path.join( getItemTempPath(itemnumber), "title.jpg"))
    else:
        text=string.replace(text,"%thumbnail", getText( infoDOM.getElementsByTagName("coverfile")[0]) )

    text=string.replace(text,"%itemnumber","%s" % itemnumber )
    text=string.replace(text,"%keynumber","%s" % keynumber )

    text=string.replace(text,"%title",getText( infoDOM.getElementsByTagName("title")[0]) )
    text=string.replace(text,"%subtitle",getText( infoDOM.getElementsByTagName("subtitle")[0]) )
    text=string.replace(text,"%description",getText( infoDOM.getElementsByTagName("description")[0]) )
    text=string.replace(text,"%type",getText( infoDOM.getElementsByTagName("type")[0]) )

    text=string.replace(text,"%recordingdate",getText( infoDOM.getElementsByTagName("recordingdate")[0]) )
    text=string.replace(text,"%recordingtime",getText( infoDOM.getElementsByTagName("recordingtime")[0]) )

    text=string.replace(text,"%duration", getFormatedLengthOfVideo(itemnumber))

    text=string.replace(text,"%myfolder",getThemeFile(themeName,""))

    if chapternumber>0:
        text=string.replace(text,"%chapternumber","%s" % chapternumber )
        text=string.replace(text,"%chaptertime","%s" % chapterlist[chapternumber - 1] )
        text=string.replace(text,"%chapterthumbnail", os.path.join( getItemTempPath(itemnumber), "chapter-%s.jpg" % chapternumber))

    return text

#############################################################
# Scale a theme position/size depending on the current video mode

def getScaledAttribute(node, attribute):
    """ Returns a value taken from attribute in node scaled for the current video mode"""

    if videomode == "pal" or attribute == "x" or attribute == "w":
        return int(node.attributes[attribute].value)
    else:
        return int(float(node.attributes[attribute].value) / 1.2)

#############################################################
# Splits some text into lines so it will fit into a given container

def intelliDraw(drawer, text, font, containerWidth):
    """Based on http://mail.python.org/pipermail/image-sig/2004-December/003064.html"""
    #Args:
    #  drawer: Instance of "ImageDraw.Draw()"
    #  text: string of long text to be wrapped
    #  font: instance of ImageFont (I use .truetype)
    #  containerWidth: number of pixels text lines have to fit into.

    #write("containerWidth: %s" % containerWidth)
    words = text.split()
    lines = [] # prepare a return argument
    lines.append(words) 
    finished = False
    line = 0
    while not finished:
        thistext = lines[line]
        newline = []
        innerFinished = False
        while not innerFinished:
            #write( 'thistext: '+str(thistext))
            #write("textWidth: %s" % drawer.textsize(' '.join(thistext),font)[0])

            if drawer.textsize(' '.join(thistext),font.getFont())[0] > containerWidth:
                # this is the heart of the algorithm: we pop words off the current
                # sentence until the width is ok, then in the next outer loop
                # we move on to the next sentence. 
                if str(thistext).find(' ') != -1:
                    newline.insert(0,thistext.pop(-1))
                else:
                    # FIXME should truncate the string here
                    innerFinished = True
            else:
                innerFinished = True
        if len(newline) > 0:
            lines.append(newline)
            line = line + 1
        else:
            finished = True
    tmp = []
    for i in lines:
        tmp.append( fix_rtl( ' '.join(i) ) )
    lines = tmp
    return lines

#############################################################
# Paints a background rectangle onto an image

def paintBackground(image, node):
    if node.hasAttribute("bgcolor"):
        bgcolor = node.attributes["bgcolor"].value
        x = getScaledAttribute(node, "x")
        y = getScaledAttribute(node, "y")
        w = getScaledAttribute(node, "w")
        h = getScaledAttribute(node, "h")
        r,g,b = ImageColor.getrgb(bgcolor)

        if node.hasAttribute("bgalpha"):
            a = int(node.attributes["bgalpha"].value)
        else:
            a = 255

        image.paste((r, g, b, a), (x, y, x + w, y + h))


#############################################################
# Paints a button onto an image

def paintButton(draw, bgimage, bgimagemask, node, infoDOM, itemnum, page,
                itemsonthispage, chapternumber, chapterlist):

    imagefilename = getThemeFile(themeName, node.attributes["filename"].value)
    if not doesFileExist(imagefilename):
        fatalError("Cannot find image for menu button (%s)." % imagefilename)
    maskimagefilename = getThemeFile(themeName, node.attributes["mask"].value)
    if not doesFileExist(maskimagefilename):
        fatalError("Cannot find mask image for menu button (%s)." % maskimagefilename)

    picture = Image.open(imagefilename,"r").resize(
              (getScaledAttribute(node, "w"), getScaledAttribute(node, "h")))
    picture = picture.convert("RGBA")
    bgimage.paste(picture, (getScaledAttribute(node, "x"),
                  getScaledAttribute(node, "y")), picture)
    del picture

    # if we have some text paint that over the image
    textnode = node.getElementsByTagName("textnormal")
    if textnode.length > 0:
        textnode = textnode[0]
        text = expandItemText(infoDOM,textnode.attributes["value"].value,
                              itemnum, page, itemsonthispage,
                              chapternumber,chapterlist)

        if text > "":
            paintText(draw, bgimage, text, textnode)

        del text

    write( "Added button image %s" % imagefilename)

    picture = Image.open(maskimagefilename,"r").resize(
              (getScaledAttribute(node, "w"), getScaledAttribute(node, "h")))
    picture = picture.convert("RGBA")
    bgimagemask.paste(picture, (getScaledAttribute(node, "x"),
                      getScaledAttribute(node, "y")),picture)
    #del picture

    # if we have some text paint that over the image
    textnode = node.getElementsByTagName("textselected")
    if textnode.length > 0:
        textnode = textnode[0]
        text = expandItemText(infoDOM, textnode.attributes["value"].value,
                              itemnum, page, itemsonthispage,
                              chapternumber, chapterlist)
        textImage = Image.new("RGBA",picture.size)
        textDraw = ImageDraw.Draw(textImage)

        if text > "":
            paintText(textDraw, textImage, text, textnode, "white",
                      getScaledAttribute(node, "x") - getScaledAttribute(textnode, "x"),
                      getScaledAttribute(node, "y") - getScaledAttribute(textnode, "y"),
                      getScaledAttribute(textnode, "w"),
                      getScaledAttribute(textnode, "h"))

        #convert the RGB image to a 1 bit image
        (width, height) = textImage.size
        for y in range(height):
            for x in range(width):
                if textImage.getpixel((x,y)) < (100, 100, 100, 255):
                    textImage.putpixel((x,y), (0, 0, 0, 0))
                else:
                    textImage.putpixel((x,y), (255, 255, 255, 255))

        if textnode.hasAttribute("colour"):
            color = textnode.attributes["colour"].value
        elif textnode.hasAttribute("color"):
            color = textnode.attributes["color"].value
        else:
            color = "white"

        bgimagemask.paste(color,
                         (getScaledAttribute(textnode, "x"),
                          getScaledAttribute(textnode, "y")),
                          textImage)

        del text, textImage, textDraw
    del picture

#############################################################
# Paint some theme text on to an image

def paintText(draw, image, text, node, color = None, 
              x = None, y = None, width = None, height = None):
    """Takes a piece of text and draws it onto an image inside a bounding box."""
    #The text is wider than the width of the bounding box

    if x == None:
        x = getScaledAttribute(node, "x") 
        y = getScaledAttribute(node, "y")
        width = getScaledAttribute(node, "w")
        height = getScaledAttribute(node, "h")

    font = themeFonts[node.attributes["font"].value]

    if color == None:
        if node.hasAttribute("colour"):
            color = node.attributes["colour"].value
        elif node.hasAttribute("color"):
            color = node.attributes["color"].value
        else:
            color = None

    if node.hasAttribute("halign"):
        halign = node.attributes["halign"].value
    elif node.hasAttribute("align"):
        halign = node.attributes["align"].value
    else:
        halign = "left"

    if node.hasAttribute("valign"):
        valign = node.attributes["valign"].value
    else:
        valign = "top"

    if node.hasAttribute("vindent"):
        vindent = int(node.attributes["vindent"].value)
    else:
        vindent = 0

    if node.hasAttribute("hindent"):
        hindent = int(node.attributes["hindent"].value)
    else:
        hindent = 0

    lines = intelliDraw(draw, text, font, width - (hindent * 2))
    j = 0

    # work out what the line spacing should be
    textImage = font.drawText(lines[0])
    h = int(textImage.size[1] * 1.1)

    for i in lines:
        if (j * h) < (height - (vindent * 2) - h):
            textImage = font.drawText(i, color)
            write( "Wrapped text  = " + i.encode("ascii", "replace"), False)

            if halign == "left":
                xoffset = hindent
            elif  halign == "center" or halign == "centre":
                xoffset = (width / 2) - (textImage.size[0] / 2)
            elif  halign == "right":
                xoffset = width - textImage.size[0] - hindent
            else:
                xoffset = hindent

            if valign == "top":
                yoffset = vindent
            elif  valign == "center" or halign == "centre":
                yoffset = (height / 2) - (textImage.size[1] / 2)
            elif  valign == "bottom":
                yoffset = height - textImage.size[1] - vindent
            else:
                yoffset = vindent

            image.paste(textImage, (x + xoffset,y + yoffset + j * h), textImage)
        else:
            write( "Truncated text = " + i.encode("ascii", "replace"), False)
        #Move to next line
        j = j + 1

#############################################################
# Paint an image on the background image

def paintImage(filename, maskfilename, imageDom, destimage, stretch=True):
    """Paste the image specified in the filename into the specified image"""

    if not doesFileExist(filename):
        write("Image file (%s) does not exist" % filename)
        return False

    picture = Image.open(filename, "r")
    xpos = getScaledAttribute(imageDom, "x")
    ypos = getScaledAttribute(imageDom, "y")
    w = getScaledAttribute(imageDom, "w")
    h = getScaledAttribute(imageDom, "h")
    (imgw, imgh) = picture.size
    write("Image (%s, %s) into space of (%s, %s) at (%s, %s)" % (imgw, imgh, w, h, xpos, ypos), False)

    # the theme can override the default stretch behaviour 
    if imageDom.hasAttribute("stretch"): 
        if imageDom.attributes["stretch"].value == "True":
            stretch = True
        else:
            stretch = False

    if stretch == True:
        imgw = w;
        imgh = h;
    else:
        if float(w)/imgw < float(h)/imgh:
            # Width is the constraining dimension
            imgh = imgh*w/imgw
            imgw = w
            if imageDom.hasAttribute("valign"):
                valign = imageDom.attributes["valign"].value
            else:
                valign = "center"

            if valign == "bottom":
                ypos += h - imgh
            if valign == "center":
                ypos += (h - imgh)/2
        else:
            # Height is the constraining dimension
            imgw = imgw*h/imgh
            imgh = h
            if imageDom.hasAttribute("halign"):
                halign = imageDom.attributes["halign"].value
            else:
                halign = "center"

            if halign == "right":
                xpos += w - imgw
            if halign == "center":
                xpos += (w - imgw)/2

    write("Image resized to (%s, %s) at (%s, %s)" % (imgw, imgh, xpos, ypos), False)
    picture = picture.resize((imgw, imgh))
    picture = picture.convert("RGBA")

    if maskfilename <> None and doesFileExist(maskfilename):
        maskpicture = Image.open(maskfilename, "r").resize((imgw, imgh))
        maskpicture = maskpicture.convert("RGBA")
    else:
        maskpicture = picture

    destimage.paste(picture, (xpos, ypos), maskpicture)
    del picture
    if maskfilename <> None and doesFileExist(maskfilename):
        del maskpicture

    write ("Added image %s" % filename)

    return True


#############################################################
# Check if boundary box need adjusting

def checkBoundaryBox(boundarybox, node):
    # We work out how much space all of our graphics and text are taking up
    # in a bounding rectangle so that we can use this as an automatic highlight
    # on the DVD menu   
    if getText(node.attributes["static"]) == "False":
        if getScaledAttribute(node, "x") < boundarybox[0]:
            boundarybox = getScaledAttribute(node, "x"), boundarybox[1], boundarybox[2], boundarybox[3]

        if getScaledAttribute(node, "y") < boundarybox[1]:
            boundarybox = boundarybox[0], getScaledAttribute(node, "y"), boundarybox[2], boundarybox[3]

        if (getScaledAttribute(node, "x") + getScaledAttribute(node, "w")) > boundarybox[2]:
            boundarybox = boundarybox[0], boundarybox[1], getScaledAttribute(node, "x") + \
                          getScaledAttribute(node, "w"), boundarybox[3]

        if (getScaledAttribute(node, "y") + getScaledAttribute(node, "h")) > boundarybox[3]:
            boundarybox = boundarybox[0], boundarybox[1], boundarybox[2], \
                          getScaledAttribute(node, "y") + getScaledAttribute(node, "h")

    return boundarybox

#############################################################
# Load the font defintions from a DVD theme file

def loadFonts(themeDOM):
    global themeFonts

    #Find all the fonts
    nodelistfonts = themeDOM.getElementsByTagName("font")

    fontnumber = 0
    for node in nodelistfonts:
        filename = getText(node)

        if node.hasAttribute("name"):
            name = node.attributes["name"].value
        else:
            name = str(fontnumber)

        fontsize = getScaledAttribute(node, "size")

        if node.hasAttribute("color"):
            color = node.attributes["color"].value
        else:
            color = "white"

        if node.hasAttribute("effect"):
            effect = node.attributes["effect"].value
        else:
            effect = "normal"

        if node.hasAttribute("shadowsize"):
            shadowsize = int(node.attributes["shadowsize"].value)
        else:
            shadowsize = 0

        if node.hasAttribute("shadowcolor"):
            shadowcolor = node.attributes["shadowcolor"].value
        else:
            shadowcolor = "black"

        themeFonts[name] = FontDef(name, getFontPathName(filename),
                           fontsize, color, effect, shadowcolor, shadowsize)

        write( "Loading font %s, %s size %s" % (fontnumber,getFontPathName(filename),fontsize) )
        fontnumber+=1

#############################################################
# Creates an info xml file from details in the job file or from the DB

def getFileInformation(file, folder):
    outputfile = os.path.join(folder, "info.xml")
    impl = xml.dom.minidom.getDOMImplementation()
    infoDOM = impl.createDocument(None, "fileinfo", None)
    top_element = infoDOM.documentElement

    # if the jobfile has amended file details use them
    details = file.getElementsByTagName("details")
    if details.length > 0:
        node = infoDOM.createElement("type")
        node.appendChild(infoDOM.createTextNode(file.attributes["type"].value))
        top_element.appendChild(node)

        node = infoDOM.createElement("filename")
        node.appendChild(infoDOM.createTextNode(file.attributes["filename"].value))
        top_element.appendChild(node)   

        node = infoDOM.createElement("title")
        node.appendChild(infoDOM.createTextNode(details[0].attributes["title"].value))
        top_element.appendChild(node)

        node = infoDOM.createElement("recordingdate")
        node.appendChild(infoDOM.createTextNode(details[0].attributes["startdate"].value))
        top_element.appendChild(node)

        node = infoDOM.createElement("recordingtime")
        node.appendChild(infoDOM.createTextNode(details[0].attributes["starttime"].value))
        top_element.appendChild(node)   

        node = infoDOM.createElement("subtitle")
        node.appendChild(infoDOM.createTextNode(details[0].attributes["subtitle"].value))
        top_element.appendChild(node)   

        node = infoDOM.createElement("description")
        node.appendChild(infoDOM.createTextNode(getText(details[0])))
        top_element.appendChild(node)   

        node = infoDOM.createElement("rating")
        node.appendChild(infoDOM.createTextNode(""))
        top_element.appendChild(node)   

        node = infoDOM.createElement("coverfile")
        node.appendChild(infoDOM.createTextNode(""))
        top_element.appendChild(node)

        #FIXME: add cutlist to details?
        node = infoDOM.createElement("cutlist")
        node.appendChild(infoDOM.createTextNode(""))
        top_element.appendChild(node)

        # if this a myth recording we still need to find the chanid, starttime and hascutlist
        if file.attributes["type"].value=="recording":
            basename = os.path.basename(file.attributes["filename"].value)
            sqlstatement  = """SELECT starttime, chanid FROM recorded 
                               WHERE basename = '%s'""" % basename.replace("'", "\\'")

            db = getDatabaseConnection()
            cursor = db.cursor()
            cursor.execute(sqlstatement)
            result = cursor.fetchall()
            numrows = int(cursor.rowcount)

            #We must have exactly 1 row returned for this recording
            if numrows!=1:
                fatalError("Failed to get recording details from the DB for %s" % file.attributes["filename"].value)

            # iterate through resultset
            for record in result:
                node = infoDOM.createElement("chanid")
                node.appendChild(infoDOM.createTextNode("%s" % record[1]))
                top_element.appendChild(node)

                #date time is returned as 2005-12-19 00:15:00 
                recdate = time.strptime(str(record[0])[0:19], "%Y-%m-%d %H:%M:%S")
                node = infoDOM.createElement("starttime")
                node.appendChild(infoDOM.createTextNode( time.strftime("%Y-%m-%dT%H:%M:%S", recdate)))
                top_element.appendChild(node)

                starttime = record[0]
                chanid = record[1]

                # find the cutlist if available
                sqlstatement  = """SELECT mark, type FROM recordedmarkup 
                                WHERE chanid = '%s' AND starttime = '%s' 
                                AND type IN (0,1) ORDER BY mark""" % (chanid, starttime)
                cursor = db.cursor()
                # execute SQL statement
                cursor.execute(sqlstatement)
                if cursor.rowcount > 0:
                    node = infoDOM.createElement("hascutlist")
                    node.appendChild(infoDOM.createTextNode("yes"))
                    top_element.appendChild(node)
                else:
                    node = infoDOM.createElement("hascutlist")
                    node.appendChild(infoDOM.createTextNode("no"))
                    top_element.appendChild(node)

                # find the cut list end marks if available to use as chapter marks
                if file.attributes["usecutlist"].value == "0" and addCutlistChapters == False:
                    sqlstatement  = """SELECT mark, type FROM recordedmarkup 
                                    WHERE chanid = '%s' AND starttime = '%s' 
                                    AND type = 0 ORDER BY mark""" % (chanid, starttime)
                    cursor = db.cursor()
                    # execute SQL statement
                    cursor.execute(sqlstatement)
                    # get the resultset as a tuple
                    result = cursor.fetchall()
                    if cursor.rowcount > 0:
                        res, fps, ar = getVideoParams(folder)
                        chapterlist="00:00:00"
                        #iterate through marks, adding to chapterlist
                        for record in result:
                            chapterlist += "," + frameToTime(int(record[0]), float(fps))

                        node = infoDOM.createElement("chapterlist")
                        node.appendChild(infoDOM.createTextNode(chapterlist))
                        top_element.appendChild(node)

                    db.close()
                    del db
                    del cursor

    elif file.attributes["type"].value=="recording":
        basename = os.path.basename(file.attributes["filename"].value)
        sqlstatement  = """SELECT progstart, stars, cutlist, category, description, subtitle, 
                           title, starttime, chanid
                           FROM recorded WHERE basename = '%s'""" % basename.replace("'", "\\'")

        # connect
        db = getDatabaseConnection()
        # create a cursor
        cursor = db.cursor()
        # execute SQL statement
        cursor.execute(sqlstatement)
        # get the resultset as a tuple
        result = cursor.fetchall()
        # get the number of rows in the resultset
        numrows = int(cursor.rowcount)
        #We must have exactly 1 row returned for this recording
        if numrows!=1:
            fatalError("Failed to get recording details from the DB for %s" % file.attributes["filename"].value)

        # iterate through resultset
        for record in result:
            #write( record[0] , "-->", record[1], record[2], record[3])
            write( "          " + record[6])
            #Create an XML DOM to hold information about this video file

            node = infoDOM.createElement("type")
            node.appendChild(infoDOM.createTextNode(file.attributes["type"].value))
            top_element.appendChild(node)

            node = infoDOM.createElement("filename")
            node.appendChild(infoDOM.createTextNode(file.attributes["filename"].value))
            top_element.appendChild(node)   

            node = infoDOM.createElement("title")
            node.appendChild(infoDOM.createTextNode(unicode(record[6], "UTF-8")))
            top_element.appendChild(node)

            #date time is returned as 2005-12-19 00:15:00            
            recdate = time.strptime(str(record[0])[0:19], "%Y-%m-%d %H:%M:%S")
            node = infoDOM.createElement("recordingdate")
            node.appendChild(infoDOM.createTextNode( time.strftime(dateformat,recdate)  ))
            top_element.appendChild(node)

            node = infoDOM.createElement("recordingtime")
            node.appendChild(infoDOM.createTextNode( time.strftime(timeformat,recdate)))
            top_element.appendChild(node)   

            node = infoDOM.createElement("subtitle")
            node.appendChild(infoDOM.createTextNode(unicode(record[5], "UTF-8")))
            top_element.appendChild(node)   

            node = infoDOM.createElement("description")
            node.appendChild(infoDOM.createTextNode(unicode(record[4], "UTF-8")))
            top_element.appendChild(node)   

            node = infoDOM.createElement("rating")
            node.appendChild(infoDOM.createTextNode("%s" % record[1]))
            top_element.appendChild(node)   

            node = infoDOM.createElement("coverfile")
            node.appendChild(infoDOM.createTextNode(""))
            #node.appendChild(infoDOM.createTextNode(record[8]))
            top_element.appendChild(node)

            node = infoDOM.createElement("chanid")
            node.appendChild(infoDOM.createTextNode("%s" % record[8]))
            top_element.appendChild(node)

            #date time is returned as 2005-12-19 00:15:00 
            recdate = time.strptime(str(record[7])[0:19], "%Y-%m-%d %H:%M:%S")
            node = infoDOM.createElement("starttime")
            node.appendChild(infoDOM.createTextNode( time.strftime("%Y-%m-%dT%H:%M:%S", recdate)))
            top_element.appendChild(node)

            starttime = record[7]
            chanid = record[8]

            # find the cutlist if available
            sqlstatement  = """SELECT mark, type FROM recordedmarkup 
                               WHERE chanid = '%s' AND starttime = '%s' 
                               AND type IN (0,1) ORDER BY mark""" % (chanid, starttime)
            cursor = db.cursor()
            # execute SQL statement
            cursor.execute(sqlstatement)
            if cursor.rowcount > 0:
                node = infoDOM.createElement("hascutlist")
                node.appendChild(infoDOM.createTextNode("yes"))
                top_element.appendChild(node)
            else:
                node = infoDOM.createElement("hascutlist")
                node.appendChild(infoDOM.createTextNode("no"))
                top_element.appendChild(node)

            if file.attributes["usecutlist"].value == "0" and addCutlistChapters == True:
                # find the cut list end marks if available
                sqlstatement  = """SELECT mark, type FROM recordedmarkup 
                                    WHERE chanid = '%s' AND starttime = '%s' 
                                    AND type = 0 ORDER BY mark""" % (chanid, starttime)
                cursor = db.cursor()
                # execute SQL statement
                cursor.execute(sqlstatement)
                # get the resultset as a tuple
                result = cursor.fetchall()
                if cursor.rowcount > 0:
                    res, fps, ar = getVideoParams(folder)
                    chapterlist="00:00:00"
                    #iterate through marks, adding to chapterlist
                    for record in result:
                        chapterlist += "," + frameToTime(int(record[0]), float(fps))

                    node = infoDOM.createElement("chapterlist")
                    node.appendChild(infoDOM.createTextNode(chapterlist))
                    top_element.appendChild(node)

        db.close()
        del db
        del cursor

    elif file.attributes["type"].value=="video":
        filename = os.path.join(videopath, file.attributes["filename"].value.replace("'", "\\'"))
        sqlstatement="""select title, director, plot, rating, inetref, year, 
                        userrating, length, coverfile from videometadata 
                        where filename='%s'""" % filename

        # connect
        db = getDatabaseConnection()
        # create a cursor
        cursor = db.cursor()
        # execute SQL statement
        cursor.execute(sqlstatement)
        # get the resultset as a tuple
        result = cursor.fetchall()
        # get the number of rows in the resultset
        numrows = int(cursor.rowcount)

        #title,director,plot,rating,inetref,year,userrating,length,coverfile
        #We must have exactly 1 row returned for this recording
        if numrows<>1:
            #Theres no record in the database so use a dummy row so we dont die!
            #title,director,plot,rating,inetref,year,userrating,length,coverfile
            record = file.attributes["filename"].value, "","",0,"","",0,0,""

        for record in result:
            write( "          " + record[0])

            node = infoDOM.createElement("type")
            node.appendChild(infoDOM.createTextNode(file.attributes["type"].value))
            top_element.appendChild(node)

            node = infoDOM.createElement("filename")
            node.appendChild(infoDOM.createTextNode(file.attributes["filename"].value))
            top_element.appendChild(node)   

            node = infoDOM.createElement("title")
            node.appendChild(infoDOM.createTextNode(unicode(record[0], "UTF-8")))
            top_element.appendChild(node)   

            node = infoDOM.createElement("recordingdate")
            date = int(record[5])
            if date != 1895:
                node.appendChild(infoDOM.createTextNode("%s" % record[5]))
            else:
                node.appendChild(infoDOM.createTextNode(""))

            top_element.appendChild(node)

            node = infoDOM.createElement("recordingtime")
            #node.appendChild(infoDOM.createTextNode(""))
            top_element.appendChild(node)   

            node = infoDOM.createElement("subtitle")
            #node.appendChild(infoDOM.createTextNode(""))
            top_element.appendChild(node)   

            node = infoDOM.createElement("description")
            desc = unicode(record[2], "UTF-8")
            if desc != "None":
                node.appendChild(infoDOM.createTextNode(desc))
            else:
                node.appendChild(infoDOM.createTextNode(""))

            top_element.appendChild(node)

            node = infoDOM.createElement("rating")
            node.appendChild(infoDOM.createTextNode("%s" % record[6]))
            top_element.appendChild(node)   

            node = infoDOM.createElement("cutlist")
            #node.appendChild(infoDOM.createTextNode(record[2]))
            top_element.appendChild(node)   

            node = infoDOM.createElement("coverfile")
            if doesFileExist(record[8]):
                node.appendChild(infoDOM.createTextNode(record[8]))
            else:
                node.appendChild(infoDOM.createTextNode(""))
            top_element.appendChild(node)

        db.close()
        del db
        del cursor

    elif file.attributes["type"].value=="file":

        node = infoDOM.createElement("type")
        node.appendChild(infoDOM.createTextNode(file.attributes["type"].value))
        top_element.appendChild(node)

        node = infoDOM.createElement("filename")
        node.appendChild(infoDOM.createTextNode(file.attributes["filename"].value))
        top_element.appendChild(node)

        node = infoDOM.createElement("title")
        node.appendChild(infoDOM.createTextNode(file.attributes["filename"].value))
        top_element.appendChild(node)

        node = infoDOM.createElement("recordingdate")
        node.appendChild(infoDOM.createTextNode(""))
        top_element.appendChild(node)

        node = infoDOM.createElement("recordingtime")
        node.appendChild(infoDOM.createTextNode(""))
        top_element.appendChild(node)   

        node = infoDOM.createElement("subtitle")
        node.appendChild(infoDOM.createTextNode(""))
        top_element.appendChild(node)   

        node = infoDOM.createElement("description")
        node.appendChild(infoDOM.createTextNode(""))
        top_element.appendChild(node)   

        node = infoDOM.createElement("rating")
        node.appendChild(infoDOM.createTextNode(""))
        top_element.appendChild(node)   

        node = infoDOM.createElement("cutlist")
        node.appendChild(infoDOM.createTextNode(""))
        top_element.appendChild(node)   

        node = infoDOM.createElement("coverfile")
        node.appendChild(infoDOM.createTextNode(""))
        top_element.appendChild(node)   

    # if the jobfile has thumb image details copy the images to the work dir
    thumbs = file.getElementsByTagName("thumbimages")
    if thumbs.length > 0:
        thumbs = thumbs[0]
        thumbs = file.getElementsByTagName("thumb")
        thumblist = ""
        res, fps, ar = getVideoParams(folder)

        for thumb in thumbs:
            caption = thumb.attributes["caption"].value
            frame = thumb.attributes["frame"].value
            filename = thumb.attributes["filename"].value
            if caption != "Title":
                if thumblist != "":
                    thumblist += "," + frameToTime(int(frame), float(fps))
                else:
                    thumblist += frameToTime(int(frame), float(fps))

            # copy thumb file to work dir
            copy(filename, folder)

        node = infoDOM.createElement("thumblist")
        node.appendChild(infoDOM.createTextNode(thumblist))
        top_element.appendChild(node)

        #top_element.appendChild(thumbs)

    WriteXMLToFile (infoDOM, outputfile)

#############################################################
# Write an xml file to disc

def WriteXMLToFile(myDOM, filename):
    #Save the XML file to disk for use later on
    f=open(filename, 'w')
    f.write(myDOM.toxml("UTF-8"))
    f.close()


#############################################################
# Pre-process a single video/recording file

def preProcessFile(file, folder):
    """Pre-process a single video/recording file."""

    write( "Pre-processing file '" + file.attributes["filename"].value + \
           "' of type '"+ file.attributes["type"].value+"'")

    #As part of this routine we need to pre-process the video:
    #1. check the file actually exists
    #2. extract information from mythtv for this file in xml file
    #3. Extract a single frame from the video to use as a thumbnail and resolution check
    mediafile=""

    if file.attributes["type"].value == "recording":
        mediafile = file.attributes["filename"].value
    elif file.attributes["type"].value == "video":
        mediafile = os.path.join(videopath, file.attributes["filename"].value)
    elif file.attributes["type"].value == "file":
        mediafile = file.attributes["filename"].value
    else:
        fatalError("Unknown type of video file it must be 'recording', 'video' or 'file'.")

    if doesFileExist(mediafile) == False:
        fatalError("Source file does not exist: " + mediafile)

    if file.hasAttribute("localfilename"):
        mediafile = file.attributes["localfilename"].value

    getStreamInformation(mediafile, os.path.join(folder, "streaminfo.xml"), 0)
    copy(os.path.join(folder, "streaminfo.xml"), os.path.join(folder, "streaminfo_orig.xml"))

    getFileInformation(file, folder)

    videosize = getVideoSize(os.path.join(folder, "streaminfo.xml"))

    write( "Video resolution is %s by %s" % (videosize[0], videosize[1]))

#############################################################
# Re-encodes an audio stream to ac3

def encodeAudio(format, sourcefile, destinationfile, deletesourceafterencode):
    write( "Encoding audio to "+format)
    if format == "ac3":
        cmd = path_ffmpeg[0] + " -v 0 -y "

        if cpuCount > 1:
            cmd += "-threads %d " % cpuCount

        cmd += "-i '%s' -f ac3 -ab 192k -ar 48000 '%s'" % (sourcefile, destinationfile)
        result = runCommand(cmd)

        if result != 0:
            fatalError("Failed while running ffmpeg to re-encode the audio to ac3\n"
                       "Command was %s" % cmd)
    else:
        fatalError("Unknown encodeAudio format " + format)

    if deletesourceafterencode==True:
        os.remove(sourcefile)

#############################################################
# Recombines a video and one or two audio streams back together
# adding in the NAV packets required to create a DVD

def multiplexMPEGStream(video, audio1, audio2, destination, syncOffset):
    """multiplex one video and one or two audio streams together"""

    write("Multiplexing MPEG stream to %s" % destination)

    if useSyncOffset == True:
        write("Adding sync offset of %dms" % syncOffset)
    else:
        write("Using sync offset is disabled - it would be %dms" % syncOffset)
        syncOffset = 0

    if doesFileExist(destination)==True:
        os.remove(destination)

    # figure out what audio files to use
    if doesFileExist(audio1 + ".ac3"):
        audio1 = audio1 + ".ac3"
    elif doesFileExist(audio1 + ".mp2"):
        audio1 = audio1 + ".mp2"
    else:
        fatalError("No audio stream available!")

    if doesFileExist(audio2 + ".ac3"):
        audio2 = audio2 + ".ac3"
    elif doesFileExist(audio2 + ".mp2"):
        audio2 = audio2 + ".mp2"

    if useFIFO==True:
        os.mkfifo(destination)
        mode=os.P_NOWAIT
    else:
        mode=os.P_WAIT

    checkCancelFlag()

    if not doesFileExist(audio2):
        write("Available streams - video and one audio stream")
        result=os.spawnlp(mode, path_mplex[0], path_mplex[1],
                    '-f', '8',
                    '-v', '0',
                    '--sync-offset', '%sms' % syncOffset,
                    '-o', destination,
                    video,
                    audio1)
    else:
        write("Available streams - video and two audio streams")
        result=os.spawnlp(mode, path_mplex[0], path_mplex[1],
                    '-f', '8',
                    '-v', '0',
                    '--sync-offset', '%sms' % syncOffset,
                    '-o', destination,
                    video,
                    audio1,
                    audio2)

    if useFIFO==True:
        write( "Multiplex started PID=%s" % result)
        return result
    else:
        if result != 0:
            fatalError("mplex failed with result %d" % result)

#############################################################
# Creates a stream xml file for a video file

def getStreamInformation(filename, xmlFilename, lenMethod):
    """create a stream.xml file for filename"""
    filename = quoteFilename(filename)
    command = "mytharchivehelper -i %s %s %d" % (filename, xmlFilename, lenMethod)

    result = runCommand(command)

    if result <> 0:
        fatalError("Failed while running mytharchivehelper to get stream information from %s" % filename)

    # print out the streaminfo.xml file to the log
    infoDOM = xml.dom.minidom.parse(xmlFilename)
    write("streaminfo.xml :-\n" + infoDOM.toprettyxml("    ", ""))

#############################################################
# Gets the video width and height from a file's stream xml file

def getVideoSize(xmlFilename):
    """Get video width and height from stream.xml file"""

    #open the XML containing information about this file
    infoDOM = xml.dom.minidom.parse(xmlFilename)
    #error out if its the wrong XML

    if infoDOM.documentElement.tagName != "file":
        fatalError("This info file doesn't look right (%s)." % xmlFilename)
    nodes = infoDOM.getElementsByTagName("video")
    if nodes.length == 0:
        fatalError("Didn't find any video elements in stream info file. (%s)" % xmlFilename)

    if nodes.length > 1:
        write("Found more than one video element in stream info file.!!!")
    node = nodes[0]
    width = int(node.attributes["width"].value)
    height = int(node.attributes["height"].value)

    return (width, height)

#############################################################
# Run a file though the lossless encoder optionally removing commercials

def runMythtranscode(chanid, starttime, destination, usecutlist, localfile):
    """Use mythtrancode to cut commercials and/or clean up an mpeg2 file"""

    if localfile != "":
        localfile = quoteFilename(localfile)
        if usecutlist == True:
            command = "mythtranscode --mpeg2 --honorcutlist -i %s -o %s" % (localfile, destination)
        else:
            command = "mythtranscode --mpeg2 -i %s -o %s" % (localfile, destination)
    else:
        if usecutlist == True:
            command = "mythtranscode --mpeg2 --honorcutlist -c %s -s %s -o %s" % (chanid, starttime, destination)
        else:
            command = "mythtranscode --mpeg2 -c %s -s %s -o %s" % (chanid, starttime, destination)

    result = runCommand(command)

    if (result != 0):
        write("Failed while running mythtranscode to cut commercials and/or clean up an mpeg2 file.\n"
              "Result: %d, Command was %s" % (result, command))
        return False;

    return True

#############################################################
# Grabs a sequence of consecutive frames from a file

def extractVideoFrame(source, destination, seconds):
    write("Extracting thumbnail image from %s at position %s" % (source, seconds))
    write("Destination file %s" % destination)

    if doesFileExist(destination) == False:

        if videomode=="pal":
            fr=frameratePAL
        else:
            fr=framerateNTSC

        source = quoteFilename(source)

        command = "mytharchivehelper -t  %s '%s' %s" % (source, seconds, destination)
        result = runCommand(command)
        if result <> 0:
            fatalError("Failed while running mytharchivehelper to get thumbnails.\n"
                       "Result: %d, Command was %s" % (result, command))
    try:
        myimage=Image.open(destination,"r")

        if myimage.format <> "JPEG":
            write( "Something went wrong with thumbnail capture - " + myimage.format)
            return (0L,0L)
        else:
            return myimage.size
    except IOError:
        return (0L, 0L)

#############################################################
# Grabs a list of single frames from a file

def extractVideoFrames(source, destination, thumbList):
    write("Extracting thumbnail images from: %s - at %s" % (source, thumbList))
    write("Destination file %s" % destination)

    source = quoteFilename(source)

    command = "mytharchivehelper -v important -t %s '%s' %s" % (source, thumbList, destination)
    result = runCommand(command)
    if result <> 0:
        fatalError("Failed while running mytharchivehelper to get thumbnails")

#############################################################
# Re-encodes a file to mpeg2

def encodeVideoToMPEG2(source, destvideofile, video, audio1, audio2, aspectratio, profile):
    """Encodes an unknown video source file eg. AVI to MPEG2 video and AC3 audio, use ffmpeg"""

    profileNode = findEncodingProfile(profile)

    passes = int(getText(profileNode.getElementsByTagName("passes")[0]))

    command = path_ffmpeg[0]

    if cpuCount > 1:
        command += " -threads %d" % cpuCount

    parameters = profileNode.getElementsByTagName("parameter")

    for param in parameters:
        name = param.attributes["name"].value
        value = param.attributes["value"].value

        # do some parameter substitution
        if value == "%inputfile":
            value = quoteFilename(source)
        if value == "%outputfile":
            value = quoteFilename(destvideofile)
        if value == "%aspect":
            value = aspectratio

        # only re-encode the audio if it is not already in AC3 format
        if audio1[AUDIO_CODEC] == "AC3":
            if name == "-acodec":
                value = "copy"
            if name == "-ar" or name == "-ab" or name == "-ac":
                name = ""
                value = ""

        if name != "":
            command += " " + name

        if value != "":
            command += " " + value


    #add second audio track if required
    if audio2[AUDIO_ID] != -1:
        command += " -newaudio" 

    #make sure we get the correct stream(s) that we want
    command += " -map 0:%d -map 0:%d " % (video[VIDEO_INDEX], audio1[AUDIO_INDEX])
    if audio2[AUDIO_ID] != -1:
        command += "-map 0:%d" % (audio2[AUDIO_INDEX])

    if passes == 1:
        write(command)
        result = runCommand(command)
        if result!=0:
            fatalError("Failed while running ffmpeg to re-encode video.\n"
                       "Command was %s" % command)

    else:
        passLog = os.path.join(getTempPath(), 'pass')

        pass1 = string.replace(command, "%passno","1")
        pass1 = string.replace(pass1, "%passlogfile", passLog)
        write("Pass 1 - " + pass1)
        result = runCommand(pass1)

        if result!=0:
            fatalError("Failed while running ffmpeg (Pass 1) to re-encode video.\n"
                       "Command was %s" % command)

        if os.path.exists(destvideofile):
            os.remove(destvideofile)

        pass2 = string.replace(command, "%passno","2")
        pass2 = string.replace(pass2, "%passlogfile", passLog)
        write("Pass 2 - " + pass2)
        result = runCommand(pass2)

        if result!=0:
            fatalError("Failed while running ffmpeg (Pass 2) to re-encode video.\n"
                       "Command was %s" % command)
#############################################################
# Re-encodes a nuv file to mpeg2 optionally removing commercials

def encodeNuvToMPEG2(chanid, starttime, mediafile, destvideofile, folder, profile, usecutlist):
    """Encodes a nuv video source file to MPEG2 video and AC3 audio, using mythtranscode & ffmpeg"""

    # make sure mythtranscode hasn't left some stale fifos hanging around
    if ((doesFileExist(os.path.join(folder, "audout")) or doesFileExist(os.path.join(folder, "vidout")))):
        fatalError("Something is wrong! Found one or more stale fifo's from mythtranscode\n"
                   "Delete the fifos in '%s' and start again" % folder)

    profileNode = findEncodingProfile(profile)
    parameters = profileNode.getElementsByTagName("parameter")

    # default values - will be overriden by values from the profile 
    outvideobitrate = 5000
    if videomode == "ntsc":
        outvideores = "720x480"
    else:
        outvideores = "720x576"

    outaudiochannels = 2
    outaudiobitrate = 384
    outaudiosamplerate = 48000
    outaudiocodec = "ac3"

    for param in parameters:
        name = param.attributes["name"].value
        value = param.attributes["value"].value

        # we only support a subset of the parameter for the moment
        if name == "-acodec":
            outaudiocodec = value
        if name == "-ac":
            outaudiochannels = value
        if name == "-ab":
            outaudiobitrate = value
        if name == "-ar":
            outaudiosamplerate = value
        if name == "-b":
            outvideobitrate = value
        if name == "-s":
            outvideores = value

    if chanid != -1:
        if (usecutlist == True):
            PID=os.spawnlp(os.P_NOWAIT, "mythtranscode", "mythtranscode",
                        '-p', '27',
                        '-c', chanid,
                        '-s', starttime,
                        '--honorcutlist',
                        '-f', folder)
            write("mythtranscode started (using cut list) PID = %s" % PID)
        else:
            PID=os.spawnlp(os.P_NOWAIT, "mythtranscode", "mythtranscode",
                        '-p', '27',
                        '-c', chanid,
                        '-s', starttime,
                        '-f', folder)

            write("mythtranscode started PID = %s" % PID)
    elif mediafile != -1:
        PID=os.spawnlp(os.P_NOWAIT, "mythtranscode", "mythtranscode",
                '-p', '27',
                '-i', mediafile,
                '-f', folder)
        write("mythtranscode started (using file) PID = %s" % PID)
    else:
        fatalError("no video source passed to encodeNuvToMPEG2.\n")


    samplerate, channels = getAudioParams(folder)
    videores, fps, aspectratio = getVideoParams(folder)

    command =  path_ffmpeg[0] + " -y "

    if cpuCount > 1:
        command += "-threads %d " % cpuCount

    command += "-f s16le -ar %s -ac %s -i %s " % (samplerate, channels, os.path.join(folder, "audout")) 
    command += "-f rawvideo -pix_fmt yuv420p -s %s -aspect %s -r %s " % (videores, aspectratio, fps)
    command += "-i %s " % os.path.join(folder, "vidout")
    command += "-aspect %s -r %s -s %s -b %s " % (aspectratio, fps, outvideores, outvideobitrate)
    command += "-vcodec mpeg2video -qmin 5 "
    command += "-ab %s -ar %s -acodec %s " % (outaudiobitrate, outaudiosamplerate, outaudiocodec)
    command += "-f dvd %s" % quoteFilename(destvideofile)

    #wait for mythtranscode to create the fifos
    tries = 30
    while (tries and not(doesFileExist(os.path.join(folder, "audout")) and
                         doesFileExist(os.path.join(folder, "vidout")))):
        tries -= 1
        write("Waiting for mythtranscode to create the fifos")
        time.sleep(1)

    if (not(doesFileExist(os.path.join(folder, "audout")) and doesFileExist(os.path.join(folder, "vidout")))):
        fatalError("Waited too long for mythtranscode to create the fifos - giving up!!")

    write("Running ffmpeg")
    result = runCommand(command)
    if result != 0:
        os.kill(PID, signal.SIGKILL)
        fatalError("Failed while running ffmpeg to re-encode video.\n"
                   "Command was %s" % command)

#############################################################
# Runs DVDAuthor to create a DVD file structure

def runDVDAuthor():
    write( "Starting dvdauthor")
    checkCancelFlag()
    result=os.spawnlp(os.P_WAIT, path_dvdauthor[0],path_dvdauthor[1],'-x',os.path.join(getTempPath(),'dvdauthor.xml'))
    if result<>0:
        fatalError("Failed while running dvdauthor. Result: %d" % result)
    write( "Finished  dvdauthor")

#############################################################
# Creates an ISO image from the contents of a directory

def CreateDVDISO():
    write("Creating ISO image")
    checkCancelFlag()
    result = os.spawnlp(os.P_WAIT, path_mkisofs[0], path_mkisofs[1], '-dvd-video', \
        '-V','MythTV BurnDVD','-o',os.path.join(getTempPath(),'mythburn.iso'), \
        os.path.join(getTempPath(),'dvd'))

    if result<>0:
        fatalError("Failed while running mkisofs.")

    write("Finished creating ISO image")

#############################################################
# Burns the contents of a directory to create a DVD 

def BurnDVDISO():
    write( "Burning ISO image to %s" % dvddrivepath)
    checkCancelFlag()

    finished = False
    tries = 0
    while not finished and tries < 10:
        f = os.open(dvddrivepath, os.O_RDONLY | os.O_NONBLOCK)
        drivestatus = ioctl(f,CDROM.CDROM_DRIVE_STATUS, 0)
        os.close(f);

        if drivestatus == CDROM.CDS_DISC_OK or drivestatus == CDROM.CDS_NO_INFO:
            if mediatype == DVD_RW and erasedvdrw == True:
                command = path_growisofs[0] + " -dvd-compat "
                if drivespeed != 0:
                    command += "-speed=%d " % drivespeed
                command += " -use-the-force-luke -Z " + dvddrivepath 
                command += " -dvd-video -V 'MythTV DVD' "
                command += os.path.join(getTempPath(),'dvd')
            else:
                command = path_growisofs[0] + " -dvd-compat "
                if drivespeed != 0:
                    command += "-speed=%d " % drivespeed
                command += " -Z " + dvddrivepath + " -dvd-video -V 'MythTV DVD' " 
                command += os.path.join(getTempPath(),'dvd')

            write(command)
            write("Running growisofs to burn DVD")

            result = runCommand(command)
            if result != 0:
                write("-"*60)
                write("ERROR: Failed while running growisofs.")
                write("Result %d, Command was: %s" % (result, command))
                write("Please check the troubleshooting section of the README for ways to fix this error")
                write("-"*60)
                write("")
                sys.exit(1)
            finished = True

            try:
                # eject the burned disc
                f = os.open(dvddrivepath, os.O_RDONLY | os.O_NONBLOCK)
                r = ioctl(f,CDROM.CDROMEJECT, 0)
                os.close(f)
            except:
                write("Failed to eject the disc! "
                      "Maybe the media monitor has mounted it")

        elif drivestatus == CDROM.CDS_TRAY_OPEN:
            # Give the user 10secs to close the Tray
            write("Waiting for tray to close.")
            time.sleep(10)
        elif drivestatus == CDROM.CDS_NO_DISC:
            # Open the Tray, if there is one.
            write("Opening tray to get it fed with a DVD.")
            f = os.open(dvddrivepath, os.O_RDONLY | os.O_NONBLOCK)
            ioctl(f,CDROM.CDROMEJECT, 0)
            os.close(f);
        elif drivestatus == CDROM.CDS_DRIVE_NOT_READY:
            # Try a hard reset
            write("Trying a hard-reset of the device")
            f = os.open(dvddrivepath, os.O_RDONLY | os.O_NONBLOCK)
            ioctl(f,CDROM.CDROMEJECT, 0)
            os.close(f);

        time.sleep(1)
        tries += 1

    if not finished:
        fatalError("Tried 10 times to get a good status from DVD drive - Giving up!")

    write("Finished burning ISO image")

#############################################################
# Splits a file into the separate audio and video streams

def deMultiplexMPEG2File(folder, mediafile, video, audio1, audio2):

    if getFileType(folder) == "mpegts":
        command = "mythreplex --demux --fix_sync -t TS -o %s " % (folder + "/stream")
        command += "-v %d " % (video[VIDEO_ID])

        if audio1[AUDIO_ID] != -1: 
            if audio1[AUDIO_CODEC] == 'MP2':
                command += "-a %d " % (audio1[AUDIO_ID])
            elif audio1[AUDIO_CODEC] == 'AC3':
                command += "-c %d " % (audio1[AUDIO_ID])

        if audio2[AUDIO_ID] != -1: 
            if audio2[AUDIO_CODEC] == 'MP2':
                command += "-a %d " % (audio2[AUDIO_ID])
            elif audio2[AUDIO_CODEC] == 'AC3':
                command += "-c %d " % (audio2[AUDIO_ID])

    else:
        command = "mythreplex --demux --fix_sync -o %s " % (folder + "/stream")
        command += "-v %d " % (video[VIDEO_ID] & 255)

        if audio1[AUDIO_ID] != -1: 
            if audio1[AUDIO_CODEC] == 'MP2':
                command += "-a %d " % (audio1[AUDIO_ID] & 255)
            elif audio1[AUDIO_CODEC] == 'AC3':
                command += "-c %d " % (audio1[AUDIO_ID] & 255)

        if audio2[AUDIO_ID] != -1: 
            if audio2[AUDIO_CODEC] == 'MP2':
                command += "-a %d " % (audio2[AUDIO_ID] & 255)
            elif audio2[AUDIO_CODEC] == 'AC3':
                command += "-c %d " % (audio2[AUDIO_ID] & 255)

    mediafile = quoteFilename(mediafile)
    command += mediafile
    write("Running: " + command)

    result = runCommand(command)

    if result<>0:
        fatalError("Failed while running mythreplex. Command was %s" % command)

#############################################################
# Run tcrequant

def runTcrequant(source,destination,percentage):
    checkCancelFlag()

    write (path_tcrequant[0] + " %s %s %s" % (source,destination,percentage))
    result=os.spawnlp(os.P_WAIT, path_tcrequant[0],path_tcrequant[1],
            "-i",source,
            "-o",destination,
            "-d","2",
            "-f","%s" % percentage)
    if result<>0:
        fatalError("Failed while running tcrequant")

#############################################################
# Calculates the total size of all the video, audio and menu files 

def calculateFileSizes(files):
    """ Returns the sizes of all video, audio and menu files"""
    filecount=0
    totalvideosize=0
    totalaudiosize=0
    totalmenusize=0

    for node in files:
        filecount+=1
        #Generate a temp folder name for this file
        folder=getItemTempPath(filecount)
        #Process this file
        file=os.path.join(folder,"stream.mv2")
        #Get size of video in MBytes
        totalvideosize+=os.path.getsize(file) / 1024 / 1024

        #Get size of audio track 1
        if doesFileExist(os.path.join(folder,"stream0.ac3")):
            totalaudiosize+=os.path.getsize(os.path.join(folder,"stream0.ac3")) / 1024 / 1024
        if doesFileExist(os.path.join(folder,"stream0.mp2")):
            totalaudiosize+=os.path.getsize(os.path.join(folder,"stream0.mp2")) / 1024 / 1024

        #Get size of audio track 2 if available 
        if doesFileExist(os.path.join(folder,"stream1.ac3")):
            totalaudiosize+=os.path.getsize(os.path.join(folder,"stream1.ac3")) / 1024 / 1024
        if doesFileExist(os.path.join(folder,"stream1.mp2")):
            totalaudiosize+=os.path.getsize(os.path.join(folder,"stream1.mp2")) / 1024 / 1024

        # add chapter menu if available
        if doesFileExist(os.path.join(getTempPath(),"chaptermenu-%s.mpg" % filecount)):
            totalmenusize+=os.path.getsize(os.path.join(getTempPath(),"chaptermenu-%s.mpg" % filecount)) / 1024 / 1024

        # add details page if available
        if doesFileExist(os.path.join(getTempPath(),"details-%s.mpg" % filecount)):
            totalmenusize+=os.path.getsize(os.path.join(getTempPath(),"details-%s.mpg" % filecount)) / 1024 / 1024

    filecount=1
    while doesFileExist(os.path.join(getTempPath(),"menu-%s.mpg" % filecount)):
        totalmenusize+=os.path.getsize(os.path.join(getTempPath(),"menu-%s.mpg" % filecount)) / 1024 / 1024
        filecount+=1

    return totalvideosize,totalaudiosize,totalmenusize

#############################################################
# Uses tcrequant if available to shrink the video streams so 
# they will fit on a DVD

def performMPEG2Shrink(files,dvdrsize):
    checkCancelFlag()

    totalvideosize,totalaudiosize,totalmenusize=calculateFileSizes(files)

    #Report findings
    write( "Total size of video files, before multiplexing, is %s Mbytes, audio is %s MBytes, menus are %s MBytes." % (totalvideosize,totalaudiosize,totalmenusize))

    #Subtract the audio and menus from the size of the disk (we cannot shrink this further)
    dvdrsize-=totalaudiosize
    dvdrsize-=totalmenusize

    #Add a little bit for the multiplexing stream data
    totalvideosize=totalvideosize*1.05

    if dvdrsize<0:
        fatalError("Audio and menu files are greater than the size of a recordable DVD disk.  Giving up!")

    if totalvideosize>dvdrsize:
        write( "Need to shrink MPEG2 video files to fit onto recordable DVD, video is %s MBytes too big." % (totalvideosize - dvdrsize))
        scalepercentage=totalvideosize/dvdrsize
        write( "Need to scale by %s" % scalepercentage)

        if scalepercentage>3:
            write( "Large scale to shrink, may not work!")

        #tcrequant (transcode) is an optional install so may not be available
        if path_tcrequant[0] == "":
            fatalError("tcrequant is not available to resize the files.  Giving up!")

        filecount=0
        for node in files:
            filecount+=1
            runTcrequant(os.path.join(getItemTempPath(filecount),"stream.mv2"),os.path.join(getItemTempPath(filecount),"video.small.m2v"),scalepercentage)
            os.remove(os.path.join(getItemTempPath(filecount),"stream.mv2"))
            os.rename(os.path.join(getItemTempPath(filecount),"video.small.m2v"),os.path.join(getItemTempPath(filecount),"stream.mv2"))

        totalvideosize,totalaudiosize,totalmenusize=calculateFileSizes(files)        
        write( "Total DVD size AFTER TCREQUANT is %s MBytes" % (totalaudiosize + totalmenusize + (totalvideosize*1.05)))

    else:
        dvdrsize-=totalvideosize
        write( "Video will fit onto DVD. %s MBytes of space remaining on recordable DVD." % dvdrsize)


#############################################################
# Creates the DVDAuthor xml file used to create a standard DVD with menus

def createDVDAuthorXML(screensize, numberofitems):
    """Creates the xml file for dvdauthor to use the MythBurn menus."""

    #Get the main menu node (we must only have 1)
    menunode=themeDOM.getElementsByTagName("menu")
    if menunode.length!=1:
        fatalError("Cannot find the menu element in the theme file")
    menunode=menunode[0]

    menuitems=menunode.getElementsByTagName("item")
    #Total number of video items on a single menu page (no less than 1!)
    itemsperpage = menuitems.length
    write( "Menu items per page %s" % itemsperpage)
    autoplaymenu = 2 + ((numberofitems + itemsperpage - 1)/itemsperpage)

    if wantChapterMenu:
        #Get the chapter menu node (we must only have 1)
        submenunode=themeDOM.getElementsByTagName("submenu")
        if submenunode.length!=1:
            fatalError("Cannot find the submenu element in the theme file")

        submenunode=submenunode[0]

        chapteritems=submenunode.getElementsByTagName("chapter")
        #Total number of video items on a single menu page (no less than 1!)
        chapters = chapteritems.length
        write( "Chapters per recording %s" % chapters)

        del chapteritems
        del submenunode

    #Page number counter
    page=1

    #Item counter to indicate current video item
    itemnum=1

    write( "Creating DVD XML file for dvd author")

    dvddom = xml.dom.minidom.parseString(
                '''<dvdauthor>
                <vmgm>
                <menus lang="en">
                <pgc entry="title">
                </pgc>
                </menus>
                </vmgm>
                </dvdauthor>''')

    dvdauthor_element=dvddom.documentElement
    menus_element = dvdauthor_element.childNodes[1].childNodes[1]

    dvdauthor_element.insertBefore( dvddom.createComment("""
    DVD Variables
    g0=not used
    g1=not used
    g2=title number selected on current menu page (see g4)
    g3=1 if intro movie has played
    g4=last menu page on display
    g5=next title to autoplay (0 or > # titles means no more autoplay)
    """), dvdauthor_element.firstChild )
    dvdauthor_element.insertBefore(dvddom.createComment("dvdauthor XML file created by MythBurn script"), dvdauthor_element.firstChild )

    menus_element.appendChild( dvddom.createComment("Title menu used to hold intro movie") )

    dvdauthor_element.setAttribute("dest",os.path.join(getTempPath(),"dvd"))

    video = dvddom.createElement("video")
    video.setAttribute("format",videomode)

    # set aspect ratio
    if mainmenuAspectRatio == "4:3":
        video.setAttribute("aspect", "4:3")
    else:
        video.setAttribute("aspect", "16:9")
        video.setAttribute("widescreen", "nopanscan")

    menus_element.appendChild(video)

    pgc=menus_element.childNodes[1]

    if wantIntro:
        #code to skip over intro if its already played
        pre = dvddom.createElement("pre")
        pgc.appendChild(pre)
        vmgm_pre_node=pre
        del pre

        node = themeDOM.getElementsByTagName("intro")[0]
        introFile = node.attributes["filename"].value

        #Pick the correct intro movie based on video format ntsc/pal
        vob = dvddom.createElement("vob")
        vob.setAttribute("pause","")
        vob.setAttribute("file",os.path.join(getThemeFile(themeName, videomode + '_' + introFile)))
        pgc.appendChild(vob)
        del vob

        #We use g3 to indicate that the intro has been played at least once
        #default g2 to point to first recording
        post = dvddom.createElement("post")
        post .appendChild(dvddom.createTextNode("{g3=1;g2=1;jump menu 2;}"))
        pgc.appendChild(post)
        del post

    while itemnum <= numberofitems:
        write( "Menu page %s" % page)

        #For each menu page we need to create a new PGC structure
        menupgc = dvddom.createElement("pgc")
        menus_element.appendChild(menupgc)
        menupgc.setAttribute("pause","inf")

        menupgc.appendChild( dvddom.createComment("Menu Page %s" % page) )

        #Make sure the button last highlighted is selected
        #g4 holds the menu page last displayed
        pre = dvddom.createElement("pre")
        pre.appendChild(dvddom.createTextNode("{button=g2*1024;g4=%s;}" % page))
        menupgc.appendChild(pre)    

        vob = dvddom.createElement("vob")
        vob.setAttribute("file",os.path.join(getTempPath(),"menu-%s.mpg" % page))
        menupgc.appendChild(vob)    

        #Loop menu forever
        post = dvddom.createElement("post")
        post.appendChild(dvddom.createTextNode("jump cell 1;"))
        menupgc.appendChild(post)

        #Default settings for this page

        #Number of video items on this menu page
        itemsonthispage=0

        endbuttons = []
        #Loop through all the items on this menu page
        while itemnum <= numberofitems and itemsonthispage < itemsperpage:
            menuitem=menuitems[ itemsonthispage ]

            itemsonthispage+=1

            #Get the XML containing information about this item
            infoDOM = xml.dom.minidom.parse( os.path.join(getItemTempPath(itemnum),"info.xml") )
            #Error out if its the wrong XML
            if infoDOM.documentElement.tagName != "fileinfo":
                fatalError("The info.xml file (%s) doesn't look right" % os.path.join(getItemTempPath(itemnum),"info.xml"))

            #write( themedom.toprettyxml())

            #Add this recording to this page's menu...
            button=dvddom.createElement("button")
            button.setAttribute("name","%s" % itemnum)
            button.appendChild(dvddom.createTextNode("{g2=" + "%s" % itemsonthispage + "; g5=0; jump title %s;}" % itemnum))
            menupgc.appendChild(button)
            del button

            #Create a TITLESET for each item
            titleset = dvddom.createElement("titleset")
            dvdauthor_element.appendChild(titleset)

            #Comment XML file with title of video
            titleset.appendChild( dvddom.createComment( getText( infoDOM.getElementsByTagName("title")[0]) ) )

            menus= dvddom.createElement("menus")
            titleset.appendChild(menus)

            video = dvddom.createElement("video")
            video.setAttribute("format",videomode)

            # set the right aspect ratio
            if chaptermenuAspectRatio == "4:3":
                video.setAttribute("aspect", "4:3")
            elif chaptermenuAspectRatio == "16:9":
                video.setAttribute("aspect", "16:9")
                video.setAttribute("widescreen", "nopanscan")
            else: 
                # use same aspect ratio as the video
                if getAspectRatioOfVideo(itemnum) > aspectRatioThreshold:
                    video.setAttribute("aspect", "16:9")
                    video.setAttribute("widescreen", "nopanscan")
                else:
                    video.setAttribute("aspect", "4:3")

            menus.appendChild(video)

            if wantChapterMenu:
                mymenupgc = dvddom.createElement("pgc")
                menus.appendChild(mymenupgc)
                mymenupgc.setAttribute("pause","inf")

                pre = dvddom.createElement("pre")
                mymenupgc.appendChild(pre)
                if wantDetailsPage: 
                    pre.appendChild(dvddom.createTextNode("{button=s7 - 1 * 1024;}"))
                else:
                    pre.appendChild(dvddom.createTextNode("{button=s7 * 1024;}"))

                vob = dvddom.createElement("vob")
                vob.setAttribute("file",os.path.join(getTempPath(),"chaptermenu-%s.mpg" % itemnum))
                mymenupgc.appendChild(vob)    

                #Loop menu forever
                post = dvddom.createElement("post")
                post.appendChild(dvddom.createTextNode("jump cell 1;"))
                mymenupgc.appendChild(post)

                # the first chapter MUST be 00:00:00 if its not dvdauthor adds it which 
                # throws of the chapter selection - so make sure we add it if needed so we
                # can compensate for it in the chapter selection menu 
                firstChapter = 0
                thumbNode = infoDOM.getElementsByTagName("thumblist")
                if thumbNode.length > 0:
                    thumblist = getText(thumbNode[0])
                    chapterlist = string.split(thumblist, ",")
                    if chapterlist[0] != '00:00:00':
                        firstChapter = 1
                x=1
                while x<=chapters:
                    #Add this recording to this page's menu...
                    button=dvddom.createElement("button")
                    button.setAttribute("name","%s" % x)
                    if wantDetailsPage: 
                        button.appendChild(dvddom.createTextNode("jump title %s chapter %s;" % (1, firstChapter + x + 1)))
                    else:
                        button.appendChild(dvddom.createTextNode("jump title %s chapter %s;" % (1, firstChapter + x)))

                    mymenupgc.appendChild(button)
                    del button
                    x+=1

                #add the titlemenu button if required
                submenunode = themeDOM.getElementsByTagName("submenu")
                submenunode = submenunode[0]
                titlemenunodes = submenunode.getElementsByTagName("titlemenu")
                if titlemenunodes.length > 0:
                    button = dvddom.createElement("button")
                    button.setAttribute("name","titlemenu")
                    button.appendChild(dvddom.createTextNode("{jump vmgm menu;}"))
                    mymenupgc.appendChild(button)
                    del button

            titles = dvddom.createElement("titles")
            titleset.appendChild(titles)

            # set the right aspect ratio
            title_video = dvddom.createElement("video")
            title_video.setAttribute("format",videomode)

            if getAspectRatioOfVideo(itemnum) > aspectRatioThreshold:
                title_video.setAttribute("aspect", "16:9")
                title_video.setAttribute("widescreen", "nopanscan")
            else:
                title_video.setAttribute("aspect", "4:3")

            titles.appendChild(title_video)

            #set right audio format
            if doesFileExist(os.path.join(getItemTempPath(itemnum), "stream0.mp2")):
                title_audio = dvddom.createElement("audio")
                title_audio.setAttribute("format", "mp2")
            else:
                title_audio = dvddom.createElement("audio")
                title_audio.setAttribute("format", "ac3")

            titles.appendChild(title_audio)

            pgc = dvddom.createElement("pgc")
            titles.appendChild(pgc)
            #pgc.setAttribute("pause","inf")

            if wantDetailsPage:
                #add the detail page intro for this item
                vob = dvddom.createElement("vob")
                vob.setAttribute("file",os.path.join(getTempPath(),"details-%s.mpg" % itemnum))
                pgc.appendChild(vob)

            vob = dvddom.createElement("vob")
            if wantChapterMenu:
                vob.setAttribute("chapters",
                    createVideoChapters(itemnum,
                                        chapters,
                                        getLengthOfVideo(itemnum),
                                        False))
            else:
                vob.setAttribute("chapters", 
                    createVideoChaptersFixedLength(itemnum,
                                                   chapterLength, 
                                                   getLengthOfVideo(itemnum)))

            vob.setAttribute("file",os.path.join(getItemTempPath(itemnum),"final.mpg"))
            pgc.appendChild(vob)

            post = dvddom.createElement("post")
            post.appendChild(dvddom.createTextNode("if (g5 eq %s) call vmgm menu %s; call vmgm menu %s;" % (itemnum + 1, autoplaymenu, page + 1)))
            pgc.appendChild(post)

            #Quick variable tidy up (not really required under Python)
            del titleset
            del titles
            del menus
            del video
            del pgc
            del vob
            del post

            #Loop through all the nodes inside this menu item and pick previous / next buttons
            for node in menuitem.childNodes:

                if node.nodeName=="previous":
                    if page>1:
                        button=dvddom.createElement("button")
                        button.setAttribute("name","previous")
                        button.appendChild(dvddom.createTextNode("{g2=1;jump menu %s;}" % page ))
                        endbuttons.append(button)


                elif node.nodeName=="next":
                    if itemnum < numberofitems:
                        button=dvddom.createElement("button")
                        button.setAttribute("name","next")
                        button.appendChild(dvddom.createTextNode("{g2=1;jump menu %s;}" % (page + 2)))
                        endbuttons.append(button)

                elif node.nodeName=="playall":
                   button=dvddom.createElement("button")
                   button.setAttribute("name","playall")
                   button.appendChild(dvddom.createTextNode("{g5=1; jump menu %s;}" % autoplaymenu))
                   endbuttons.append(button)

            #On to the next item
            itemnum+=1

        #Move on to the next page
        page+=1

        for button in endbuttons:
            menupgc.appendChild(button)
            del button

    menupgc = dvddom.createElement("pgc")
    menus_element.appendChild(menupgc)
    menupgc.setAttribute("pause","inf")
    menupgc.appendChild( dvddom.createComment("Autoplay hack") )

    dvdcode = ""
    while (itemnum > 1):
        itemnum-=1
        dvdcode += "if (g5 eq %s) {g5 = %s; jump title %s;} " % (itemnum, itemnum + 1, itemnum)
    dvdcode += "g5 = 0; jump menu 1;"

    pre = dvddom.createElement("pre")
    pre.appendChild(dvddom.createTextNode(dvdcode))
    menupgc.appendChild(pre)    

    if wantIntro:
        #Menu creation is finished so we know how many pages were created
        #add to to jump to the correct one automatically
        dvdcode="if (g3 eq 1) {"
        while (page>1):
            page-=1;
            dvdcode+="if (g4 eq %s) " % page
            dvdcode+="jump menu %s;" % (page + 1)
            if (page>1):
                dvdcode+=" else "
        dvdcode+="}"       
        vmgm_pre_node.appendChild(dvddom.createTextNode(dvdcode))

    #write(dvddom.toprettyxml())
    #Save xml to file
    WriteXMLToFile (dvddom,os.path.join(getTempPath(),"dvdauthor.xml"))

    #Destroy the DOM and free memory
    dvddom.unlink()   

#############################################################
# Creates the DVDAuthor xml file used to create a DVD with no main menu

def createDVDAuthorXMLNoMainMenu(screensize, numberofitems):
    """Creates the xml file for dvdauthor to use the MythBurn menus."""

    # creates a simple DVD with only a chapter menus shown before each video
    # can contain an intro movie and each title can have a details page
    # displayed before each title

    write( "Creating DVD XML file for dvd author (No Main Menu)")
    #FIXME:
    assert False

#############################################################
# Creates the DVDAuthor xml file used to create an Autoplay DVD

def createDVDAuthorXMLNoMenus(screensize, numberofitems):
    """Creates the xml file for dvdauthor containing no menus."""

    # creates a simple DVD with no menus that chains the videos one after the other
    # can contain an intro movie and each title can have a details page
    # displayed before each title

    write( "Creating DVD XML file for dvd author (No Menus)")

    dvddom = xml.dom.minidom.parseString(
                '''
                <dvdauthor>
                    <vmgm>
                        <menus lang="en">
                            <pgc entry="title" pause="0">
                            </pgc>
                        </menus>
                    </vmgm>
                </dvdauthor>''')

    dvdauthor_element = dvddom.documentElement
    menus = dvdauthor_element.childNodes[1].childNodes[1]
    menu_pgc = menus.childNodes[1]

    dvdauthor_element.insertBefore(dvddom.createComment("dvdauthor XML file created by MythBurn script"), dvdauthor_element.firstChild )
    dvdauthor_element.setAttribute("dest",os.path.join(getTempPath(),"dvd"))

    # create pgc for menu 1 holds the intro if required, blank mpg if not
    if wantIntro:
        video = dvddom.createElement("video")
        video.setAttribute("format", videomode)

        # set aspect ratio
        if mainmenuAspectRatio == "4:3":
            video.setAttribute("aspect", "4:3")
        else:
            video.setAttribute("aspect", "16:9")
            video.setAttribute("widescreen", "nopanscan")
        menus.appendChild(video)

        pre = dvddom.createElement("pre")
        pre.appendChild(dvddom.createTextNode("if (g2==1) jump menu 2;"))
        menu_pgc.appendChild(pre)

        node = themeDOM.getElementsByTagName("intro")[0]
        introFile = node.attributes["filename"].value

        vob = dvddom.createElement("vob")
        vob.setAttribute("file", getThemeFile(themeName, videomode + '_' + introFile))
        menu_pgc.appendChild(vob)

        post = dvddom.createElement("post")
        post.appendChild(dvddom.createTextNode("g2=1; jump menu 2;"))
        menu_pgc.appendChild(post)
        del menu_pgc
        del post
        del pre
        del vob
    else:
        pre = dvddom.createElement("pre")
        pre.appendChild(dvddom.createTextNode("g2=1;jump menu 2;"))
        menu_pgc.appendChild(pre)

        vob = dvddom.createElement("vob")
        vob.setAttribute("file", getThemeFile(themeName, videomode + '_' + "blank.mpg"))
        menu_pgc.appendChild(vob)

        del menu_pgc
        del pre
        del vob

    # create menu 2 - dummy menu that allows us to jump to each titleset in sequence
    menu_pgc = dvddom.createElement("pgc")
    menu_pgc.setAttribute("pause", "0")

    preText = "if (g1==0) g1=1;"
    for i in range(numberofitems):
        preText += "if (g1==%d) jump titleset %d menu;" % (i + 1, i + 1)

    pre = dvddom.createElement("pre")
    pre.appendChild(dvddom.createTextNode(preText))
    menu_pgc.appendChild(pre)

    vob = dvddom.createElement("vob")
    vob.setAttribute("file", getThemeFile(themeName, videomode + '_' + "blank.mpg"))
    menu_pgc.appendChild(vob)
    menus.appendChild(menu_pgc)

    # for each title add a <titleset> section
    itemNum = 1
    while itemNum <= numberofitems:
        write( "Adding item %s" % itemNum)

        titleset = dvddom.createElement("titleset")
        dvdauthor_element.appendChild(titleset)

        # create menu
        menu = dvddom.createElement("menus")
        menupgc = dvddom.createElement("pgc")
        menu.appendChild(menupgc)
        menupgc.setAttribute("pause","0")
        titleset.appendChild(menu)

        if wantDetailsPage:
            #add the detail page intro for this item
            vob = dvddom.createElement("vob")
            vob.setAttribute("file", os.path.join(getTempPath(),"details-%s.mpg" % itemNum))
            menupgc.appendChild(vob)

            post = dvddom.createElement("post")
            post.appendChild(dvddom.createTextNode("jump title 1;"))
            menupgc.appendChild(post)
            del post
        else:
            #add dummy menu for this item
            pre = dvddom.createElement("pre")
            pre.appendChild(dvddom.createTextNode("jump title 1;"))
            menupgc.appendChild(pre)
            del pre

            vob = dvddom.createElement("vob")
            vob.setAttribute("file", getThemeFile(themeName, videomode + '_' + "blank.mpg"))
            menupgc.appendChild(vob)

        titles = dvddom.createElement("titles")

        # set the right aspect ratio
        title_video = dvddom.createElement("video")
        title_video.setAttribute("format", videomode)

        # use aspect ratio of video
        if getAspectRatioOfVideo(itemNum) > aspectRatioThreshold:
            title_video.setAttribute("aspect", "16:9")
            title_video.setAttribute("widescreen", "nopanscan")
        else:
            title_video.setAttribute("aspect", "4:3")

        titles.appendChild(title_video)

        pgc = dvddom.createElement("pgc")

        vob = dvddom.createElement("vob")
        vob.setAttribute("file", os.path.join(getItemTempPath(itemNum), "final.mpg"))
        vob.setAttribute("chapters", createVideoChaptersFixedLength(itemNum,
                                                                    chapterLength,
                                                                    getLengthOfVideo(itemNum)))
        pgc.appendChild(vob)

        del vob
        del menupgc

        post = dvddom.createElement("post")
        if itemNum == numberofitems:
            post.appendChild(dvddom.createTextNode("exit;"))
        else:
            post.appendChild(dvddom.createTextNode("g1=%d;call vmgm menu 2;" % (itemNum + 1)))

        pgc.appendChild(post)

        titles.appendChild(pgc)
        titleset.appendChild(titles)

        del pgc
        del titles
        del title_video
        del post
        del titleset

        itemNum +=1

    #Save xml to file
    WriteXMLToFile (dvddom,os.path.join(getTempPath(),"dvdauthor.xml"))

    #Destroy the DOM and free memory
    dvddom.unlink()

#############################################################
# Creates the directory to hold the preview images for an animated menu 

def createEmptyPreviewFolder(videoitem):
    previewfolder = os.path.join(getItemTempPath(videoitem), "preview")
    if os.path.exists(previewfolder):
        deleteAllFilesInFolder(previewfolder)
        os.rmdir (previewfolder)
    os.makedirs(previewfolder)
    return previewfolder

#############################################################
# Generates the thumbnail images used to create animated menus

def generateVideoPreview(videoitem, itemonthispage, menuitem, starttime, menulength, previewfolder):
    """generate thumbnails for a preview in a menu"""

    positionx = 9999
    positiony = 9999
    width = 0
    height = 0
    maskpicture = None

    #run through the theme items and find any graphics that is using a movie identifier
    for node in menuitem.childNodes:
        if node.nodeName=="graphic":
            if node.attributes["filename"].value == "%movie":
                #This is a movie preview item so we need to generate the thumbnails
                inputfile = os.path.join(getItemTempPath(videoitem),"stream.mv2")
                outputfile = os.path.join(previewfolder, "preview-i%d-t%%1-f%%2.jpg" % itemonthispage)
                width = getScaledAttribute(node, "w")
                height = getScaledAttribute(node, "h")
                frames = int(secondsToFrames(menulength))

                command = "mytharchivehelper -t  %s '%s' '%s' %d" % (inputfile, starttime, outputfile, frames)
                result = runCommand(command)
                if (result != 0):
                    write( "mytharchivehelper failed with code %d. Command = %s" % (result, command) )

                positionx = getScaledAttribute(node, "x")
                positiony = getScaledAttribute(node, "y")

                #see if this graphics item has a mask
                if node.hasAttribute("mask"):
                    imagemaskfilename = getThemeFile(themeName, node.attributes["mask"].value)
                    if node.attributes["mask"].value <> "" and doesFileExist(imagemaskfilename):
                        maskpicture = Image.open(imagemaskfilename,"r").resize((width, height))
                        maskpicture = maskpicture.convert("RGBA")

    return (positionx, positiony, width, height, maskpicture)

#############################################################
# Draws text and graphics onto a dvd menu

def drawThemeItem(page, itemsonthispage, itemnum, menuitem, bgimage, draw,
                  bgimagemask, drawmask, highlightcolor, spumuxdom, spunode,
                  numberofitems, chapternumber, chapterlist):
    """Draws text and graphics onto a dvd menu, called by 
       createMenu and createChapterMenu"""

    #Get the XML containing information about this item
    infoDOM = xml.dom.minidom.parse(os.path.join(getItemTempPath(itemnum), "info.xml"))

    #Error out if its the wrong XML
    if infoDOM.documentElement.tagName != "fileinfo":
        fatalError("The info.xml file (%s) doesn't look right" %
                    os.path.join(getItemTempPath(itemnum),"info.xml"))

    #boundarybox holds the max and min dimensions for this item 
    #so we can auto build a menu highlight box
    boundarybox = 9999,9999,0,0
    wantHighlightBox = True

    #Loop through all the nodes inside this menu item
    for node in menuitem.childNodes:

        #Process each type of item to add it onto the background image
        if node.nodeName=="graphic":
            #Overlay graphic image onto background

            # draw background if required
            paintBackground(bgimage, node)

            # if this graphic item is a movie thumbnail then we dont process it here
            if node.attributes["filename"].value == "%movie":
                # this is a movie item but we must still update the boundary box
                boundarybox = checkBoundaryBox(boundarybox, node)
            else:
                imagefilename = expandItemText(infoDOM,
                                               node.attributes["filename"].value,
                                               itemnum, page, itemsonthispage,
                                               chapternumber, chapterlist)

                if doesFileExist(imagefilename) == False:
                    if imagefilename == node.attributes["filename"].value:
                        imagefilename = getThemeFile(themeName,
                                        node.attributes["filename"].value)

                # see if an image mask exists
                maskfilename = None
                if node.hasAttribute("mask") and node.attributes["mask"].value <> "":
                    maskfilename = getThemeFile(themeName, node.attributes["mask"].value)

                # if this is a thumb image and is a MythVideo coverart image then preserve 
                # its aspect ratio unless overriden later by the theme
                if (node.attributes["filename"].value == "%thumbnail"
                  and getText(infoDOM.getElementsByTagName("coverfile")[0]) !=""):
                    stretch = False
                else:
                    stretch = True

                if paintImage(imagefilename, maskfilename, node, bgimage, stretch):
                    boundarybox = checkBoundaryBox(boundarybox, node)
                else:
                    write("Image file does not exist '%s'" % imagefilename)

        elif node.nodeName == "text":
            # Apply some text to the background, including wordwrap if required.

            # draw background if required
            paintBackground(bgimage, node)

            text = expandItemText(infoDOM,node.attributes["value"].value,
                                  itemnum, page, itemsonthispage,
                                  chapternumber, chapterlist)

            if text>"":
                paintText(draw, bgimage, text, node)

            boundarybox = checkBoundaryBox(boundarybox, node)
            del text

        elif node.nodeName=="previous":
            if page>1:
                #Overlay previous graphic button onto background

                # draw background if required
                paintBackground(bgimage, node)

                paintButton(draw, bgimage, bgimagemask, node, infoDOM,
                            itemnum, page, itemsonthispage, chapternumber,
                            chapterlist)

                button = spumuxdom.createElement("button")
                button.setAttribute("name","previous")
                button.setAttribute("x0","%s" % getScaledAttribute(node, "x"))
                button.setAttribute("y0","%s" % getScaledAttribute(node, "y"))
                button.setAttribute("x1","%s" % (getScaledAttribute(node, "x") + 
                                                getScaledAttribute(node, "w")))
                button.setAttribute("y1","%s" % (getScaledAttribute(node, "y") +
                                                getScaledAttribute(node, "h")))
                spunode.appendChild(button)

                write( "Added previous page button")


        elif node.nodeName == "next":
            if itemnum < numberofitems:
                #Overlay next graphic button onto background

                # draw background if required
                paintBackground(bgimage, node)

                paintButton(draw, bgimage, bgimagemask, node, infoDOM,
                            itemnum, page, itemsonthispage, chapternumber,
                            chapterlist)

                button = spumuxdom.createElement("button")
                button.setAttribute("name","next")
                button.setAttribute("x0","%s" % getScaledAttribute(node, "x"))
                button.setAttribute("y0","%s" % getScaledAttribute(node, "y"))
                button.setAttribute("x1","%s" % (getScaledAttribute(node, "x") + 
                                                 getScaledAttribute(node, "w")))
                button.setAttribute("y1","%s" % (getScaledAttribute(node, "y") + 
                                                 getScaledAttribute(node, "h")))
                spunode.appendChild(button)

                write("Added next page button")

        elif node.nodeName=="playall":
            #Overlay playall graphic button onto background

            # draw background if required
            paintBackground(bgimage, node)

            paintButton(draw, bgimage, bgimagemask, node, infoDOM, itemnum, page,
                        itemsonthispage, chapternumber, chapterlist)

            button = spumuxdom.createElement("button")
            button.setAttribute("name","playall")
            button.setAttribute("x0","%s" % getScaledAttribute(node, "x"))
            button.setAttribute("y0","%s" % getScaledAttribute(node, "y"))
            button.setAttribute("x1","%s" % (getScaledAttribute(node, "x") + 
                                             getScaledAttribute(node, "w")))
            button.setAttribute("y1","%s" % (getScaledAttribute(node, "y") +
                                             getScaledAttribute(node, "h")))
            spunode.appendChild(button)

            write("Added playall button")

        elif node.nodeName == "titlemenu":
            if itemnum < numberofitems:
                #Overlay next graphic button onto background

                # draw background if required
                paintBackground(bgimage, node)

                paintButton(draw, bgimage, bgimagemask, node, infoDOM, 
                            itemnum, page, itemsonthispage, chapternumber, 
                            chapterlist)

                button = spumuxdom.createElement("button")
                button.setAttribute("name","titlemenu")
                button.setAttribute("x0","%s" % getScaledAttribute(node, "x"))
                button.setAttribute("y0","%s" % getScaledAttribute(node, "y"))
                button.setAttribute("x1","%s" % (getScaledAttribute(node, "x") +
                                                getScaledAttribute(node, "w")))
                button.setAttribute("y1","%s" % (getScaledAttribute(node, "y") +
                                                getScaledAttribute(node, "h")))
                spunode.appendChild(button)

                write( "Added titlemenu button")

        elif node.nodeName=="button":
            #Overlay item graphic/text button onto background

            # draw background if required
            paintBackground(bgimage, node)

            wantHighlightBox = False

            paintButton(draw, bgimage, bgimagemask, node, infoDOM, itemnum, page,
                        itemsonthispage, chapternumber, chapterlist)

            boundarybox = checkBoundaryBox(boundarybox, node)


        elif node.nodeName=="#text" or node.nodeName=="#comment":
            #Do nothing
            assert True
        else:
            write( "Dont know how to process %s" % node.nodeName)

    if drawmask == None:
        return

    #Draw the selection mask for this item
    if wantHighlightBox == True:
        # Make the boundary box bigger than the content to avoid over writing it
        boundarybox=boundarybox[0]-1,boundarybox[1]-1,boundarybox[2]+1,boundarybox[3]+1
        drawmask.rectangle(boundarybox,outline=highlightcolor)

        # Draw another line to make the box thicker - PIL does not support linewidth
        boundarybox=boundarybox[0]-1,boundarybox[1]-1,boundarybox[2]+1,boundarybox[3]+1
        drawmask.rectangle(boundarybox,outline=highlightcolor)

    node = spumuxdom.createElement("button")
    #Fiddle this for chapter marks....
    if chapternumber>0:
        node.setAttribute("name","%s" % chapternumber)
    else:
        node.setAttribute("name","%s" % itemnum)
    node.setAttribute("x0","%d" % int(boundarybox[0]))
    node.setAttribute("y0","%d" % int(boundarybox[1]))
    node.setAttribute("x1","%d" % int(boundarybox[2] + 1))
    node.setAttribute("y1","%d" % int(boundarybox[3] + 1))
    spunode.appendChild(node)

#############################################################
# creates the main menu for a DVD

def createMenu(screensize, screendpi, numberofitems):
    """Creates all the necessary menu images and files for the MythBurn menus."""

    #Get the main menu node (we must only have 1)
    menunode=themeDOM.getElementsByTagName("menu")
    if menunode.length!=1:
        fatalError("Cannot find menu element in theme file")
    menunode=menunode[0]

    menuitems=menunode.getElementsByTagName("item")
    #Total number of video items on a single menu page (no less than 1!)
    itemsperpage = menuitems.length
    write( "Menu items per page %s" % itemsperpage)

    #Get background image filename
    backgroundfilename = menunode.attributes["background"].value
    if backgroundfilename=="":
        fatalError("Background image is not set in theme file")

    backgroundfilename = getThemeFile(themeName,backgroundfilename)
    write( "Background image file is %s" % backgroundfilename)
    if not doesFileExist(backgroundfilename):
        fatalError("Background image not found (%s)" % backgroundfilename)

    #Get highlight color
    highlightcolor = "red"
    if menunode.hasAttribute("highlightcolor"):
        highlightcolor = menunode.attributes["highlightcolor"].value

    #Get menu music
    menumusic = "menumusic.ac3"
    if menunode.hasAttribute("music"):
        menumusic = menunode.attributes["music"].value

    #Get menu length
    menulength = 15
    if menunode.hasAttribute("length"):
        menulength = int(menunode.attributes["length"].value)

    write("Music is %s, length is %s seconds" % (menumusic, menulength))

    #Page number counter
    page=1

    #Item counter to indicate current video item
    itemnum=1

    write("Creating DVD menus")

    while itemnum <= numberofitems:
        write("Menu page %s" % page)

        #need to check if any of the videos are flaged as movies
        #and if so generate the required preview

        write("Creating Preview Video")
        previewitem = itemnum
        itemsonthispage = 0
        haspreview = False

        previewx = []
        previewy = []
        previeww = []
        previewh = []
        previewmask = []

        while previewitem <= numberofitems and itemsonthispage < itemsperpage:
            menuitem=menuitems[ itemsonthispage ]
            itemsonthispage+=1

            #make sure the preview folder is empty and present
            previewfolder = createEmptyPreviewFolder(previewitem)

            #and then generate the preview if required (px=9999 means not required)
            px, py, pw, ph, maskimage = generateVideoPreview(previewitem, itemsonthispage, menuitem, 0, menulength, previewfolder)
            previewx.append(px)
            previewy.append(py)
            previeww.append(pw)
            previewh.append(ph)
            previewmask.append(maskimage)
            if px != 9999:
                haspreview = True

            previewitem+=1

        #previews generated but need to save where we started from
        savedpreviewitem = itemnum

        #Number of video items on this menu page
        itemsonthispage=0

        #instead of loading the background image and drawing on it we now
        #make a transparent image and draw all items on it. This overlay
        #image is then added to the required background image when the
        #preview items are added (the reason for this is it will assist
        #if the background image is actually a video)

        overlayimage=Image.new("RGBA",screensize)
        draw=ImageDraw.Draw(overlayimage)

        #Create image to hold button masks (same size as background)
        bgimagemask=Image.new("RGBA",overlayimage.size)
        drawmask=ImageDraw.Draw(bgimagemask)

        spumuxdom = xml.dom.minidom.parseString('<subpictures><stream><spu force="yes" start="00:00:00.0" highlight="" select="" ></spu></stream></subpictures>')
        spunode = spumuxdom.documentElement.firstChild.firstChild

        #Loop through all the items on this menu page
        while itemnum <= numberofitems and itemsonthispage < itemsperpage:
            menuitem=menuitems[ itemsonthispage ]

            itemsonthispage+=1

            drawThemeItem(page, itemsonthispage,
                        itemnum, menuitem, overlayimage,
                        draw, bgimagemask, drawmask, highlightcolor,
                        spumuxdom, spunode, numberofitems, 0,"")

            #On to the next item
            itemnum+=1

        #Paste the overlay image onto the background
        bgimage=Image.open(backgroundfilename,"r").resize(screensize)
        bgimage.paste(overlayimage, (0,0), overlayimage)

        #Save this menu image and its mask
        bgimage.save(os.path.join(getTempPath(),"background-%s.jpg" % page),"JPEG", quality=99)
        bgimagemask.save(os.path.join(getTempPath(),"backgroundmask-%s.png" % page),"PNG",quality=99,optimize=0,dpi=screendpi)

        #now that the base background has been made and all the previews generated
        #we need to add the previews to the background
        #Assumption: We assume that there is nothing in the location of where the items go 
        #(ie, no text on the images)

        itemsonthispage = 0

        #numframes should be the number of preview images that have been created
        numframes=secondsToFrames(menulength)

        # only generate the preview video if required.
        if haspreview == True:
            write( "Generating the preview images" )
            framenum = 0
            while framenum < numframes:
                previewitem = savedpreviewitem
                itemsonthispage = 0
                while previewitem <= numberofitems and itemsonthispage < itemsperpage:
                    itemsonthispage+=1
                    if previewx[itemsonthispage-1] != 9999:
                        previewpath = os.path.join(getItemTempPath(previewitem), "preview")
                        previewfile = "preview-i%d-t1-f%d.jpg" % (itemsonthispage, framenum)
                        imagefile = os.path.join(previewpath, previewfile)

                        if doesFileExist(imagefile):
                            picture = Image.open(imagefile, "r").resize((previeww[itemsonthispage-1], previewh[itemsonthispage-1]))
                            picture = picture.convert("RGBA")
                            imagemaskfile = os.path.join(previewpath, "mask-i%d.png" % itemsonthispage)
                            if previewmask[itemsonthispage-1] != None:
                                bgimage.paste(picture, (previewx[itemsonthispage-1], previewy[itemsonthispage-1]), previewmask[itemsonthispage-1])
                            else:
                                bgimage.paste(picture, (previewx[itemsonthispage-1], previewy[itemsonthispage-1]))
                            del picture
                    previewitem+=1
                #bgimage.save(os.path.join(getTempPath(),"background-%s-f%06d.png" % (page, framenum)),"PNG",quality=100,optimize=0,dpi=screendpi)
                bgimage.save(os.path.join(getTempPath(),"background-%s-f%06d.jpg" % (page, framenum)),"JPEG",quality=99)
                framenum+=1

        spumuxdom.documentElement.firstChild.firstChild.setAttribute("select",os.path.join(getTempPath(),"backgroundmask-%s.png" % page))
        spumuxdom.documentElement.firstChild.firstChild.setAttribute("highlight",os.path.join(getTempPath(),"backgroundmask-%s.png" % page))

        #Release large amounts of memory ASAP !
        del draw
        del bgimage
        del drawmask
        del bgimagemask
        del overlayimage
        del previewx
        del previewy
        del previewmask

        WriteXMLToFile (spumuxdom,os.path.join(getTempPath(),"spumux-%s.xml" % page))

        if mainmenuAspectRatio == "4:3":
            aspect_ratio = 2
        else:
            aspect_ratio = 3

        write("Encoding Menu Page %s using aspect ratio '%s'" % (page, mainmenuAspectRatio))
        if haspreview == True:
            encodeMenu(os.path.join(getTempPath(),"background-%s-f%%06d.jpg" % page),
                        os.path.join(getTempPath(),"temp.m2v"),
                        getThemeFile(themeName,menumusic),
                        menulength,
                        os.path.join(getTempPath(),"temp.mpg"),
                        os.path.join(getTempPath(),"spumux-%s.xml" % page),
                        os.path.join(getTempPath(),"menu-%s.mpg" % page),
                        aspect_ratio)
        else:
            encodeMenu(os.path.join(getTempPath(),"background-%s.jpg" % page),
                        os.path.join(getTempPath(),"temp.m2v"),
                        getThemeFile(themeName,menumusic),
                        menulength,
                        os.path.join(getTempPath(),"temp.mpg"),
                        os.path.join(getTempPath(),"spumux-%s.xml" % page),
                        os.path.join(getTempPath(),"menu-%s.mpg" % page),
                        aspect_ratio)

        #Move on to the next page
        page+=1

#############################################################
# creates a chapter menu for a file on a DVD

def createChapterMenu(screensize, screendpi, numberofitems):
    """Creates all the necessary menu images and files for the MythBurn menus."""

    #Get the main menu node (we must only have 1)
    menunode=themeDOM.getElementsByTagName("submenu")
    if menunode.length!=1:
        fatalError("Cannot find submenu element in theme file")
    menunode=menunode[0]

    menuitems=menunode.getElementsByTagName("chapter")
    #Total number of video items on a single menu page (no less than 1!)
    itemsperpage = menuitems.length
    write( "Chapter items per page %s " % itemsperpage)

    #Get background image filename
    backgroundfilename = menunode.attributes["background"].value
    if backgroundfilename=="":
        fatalError("Background image is not set in theme file")
    backgroundfilename = getThemeFile(themeName,backgroundfilename)
    write( "Background image file is %s" % backgroundfilename)
    if not doesFileExist(backgroundfilename):
        fatalError("Background image not found (%s)" % backgroundfilename)

    #Get highlight color
    highlightcolor = "red"
    if menunode.hasAttribute("highlightcolor"):
        highlightcolor = menunode.attributes["highlightcolor"].value

    #Get menu music
    menumusic = "menumusic.ac3"
    if menunode.hasAttribute("music"):
        menumusic = menunode.attributes["music"].value

    #Get menu length
    menulength = 15
    if menunode.hasAttribute("length"):
        menulength = int(menunode.attributes["length"].value)

    write("Music is %s, length is %s seconds" % (menumusic, menulength))

    #Page number counter
    page=1

    write( "Creating DVD sub-menus")

    while page <= numberofitems:
        write( "Sub-menu %s " % page)

        #instead of loading the background image and drawing on it we now
        #make a transparent image and draw all items on it. This overlay
        #image is then added to the required background image when the
        #preview items are added (the reason for this is it will assist
        #if the background image is actually a video)

        overlayimage=Image.new("RGBA",screensize, (0,0,0,0))
        draw=ImageDraw.Draw(overlayimage)

        #Create image to hold button masks (same size as background)
        bgimagemask=Image.new("RGBA",overlayimage.size, (0,0,0,0))
        drawmask=ImageDraw.Draw(bgimagemask)

        spumuxdom = xml.dom.minidom.parseString('<subpictures><stream><spu force="yes" start="00:00:00.0" highlight="" select="" ></spu></stream></subpictures>')
        spunode = spumuxdom.documentElement.firstChild.firstChild

        #Extract the thumbnails
        chapterlist=createVideoChapters(page,itemsperpage,getLengthOfVideo(page),True)
        chapterlist=string.split(chapterlist,",")

        #now need to preprocess the menu to see if any preview videos are required
        #This must be done on an individual basis since we do the resize as the
        #images are extracted.

        #first make sure the preview folder is empty and present
        previewfolder = createEmptyPreviewFolder(page)

        haspreview = False

        previewsegment=int(getLengthOfVideo(page) / itemsperpage)
        previewtime = 0
        previewchapter = 0
        previewx = []
        previewy = []
        previeww = []
        previewh = []
        previewmask = []

        while previewchapter < itemsperpage:
            menuitem=menuitems[ previewchapter ]

            #generate the preview if required (px=9999 means not required)
            px, py, pw, ph, maskimage = generateVideoPreview(page, previewchapter, menuitem, previewtime, menulength, previewfolder)
            previewx.append(px)
            previewy.append(py)
            previeww.append(pw)
            previewh.append(ph)
            previewmask.append(maskimage)

            if px != 9999:
                haspreview = True

            previewchapter+=1
            previewtime+=previewsegment

        #Loop through all the items on this menu page
        chapter=0
        while chapter < itemsperpage:  # and itemsonthispage < itemsperpage:
            menuitem=menuitems[ chapter ]
            chapter+=1

            drawThemeItem(page, itemsperpage, page, menuitem,
                        overlayimage, draw, 
                        bgimagemask, drawmask, highlightcolor,
                        spumuxdom, spunode,
                        999, chapter, chapterlist)

        #Save this menu image and its mask
        bgimage=Image.open(backgroundfilename,"r").resize(screensize)
        bgimage.paste(overlayimage, (0,0), overlayimage)
        bgimage.save(os.path.join(getTempPath(),"chaptermenu-%s.jpg" % page),"JPEG", quality=99)

        bgimagemask.save(os.path.join(getTempPath(),"chaptermenumask-%s.png" % page),"PNG",quality=90,optimize=0)

        if haspreview == True:
            numframes=secondsToFrames(menulength)

            #numframes should be the number of preview images that have been created

            write( "Generating the preview images" )
            framenum = 0
            while framenum < numframes:
                previewchapter = 0
                while previewchapter < itemsperpage:
                    if previewx[previewchapter] != 9999:
                        previewpath = os.path.join(getItemTempPath(page), "preview")
                        previewfile = "preview-i%d-t1-f%d.jpg" % (previewchapter, framenum)
                        imagefile = os.path.join(previewpath, previewfile)

                        if doesFileExist(imagefile):
                            picture = Image.open(imagefile, "r").resize((previeww[previewchapter], previewh[previewchapter]))
                            picture = picture.convert("RGBA")
                            imagemaskfile = os.path.join(previewpath, "mask-i%d.png" % previewchapter)
                            if previewmask[previewchapter] != None:
                                bgimage.paste(picture, (previewx[previewchapter], previewy[previewchapter]), previewmask[previewchapter])
                            else:
                                bgimage.paste(picture, (previewx[previewchapter], previewy[previewchapter]))
                            del picture
                    previewchapter+=1
                bgimage.save(os.path.join(getTempPath(),"chaptermenu-%s-f%06d.jpg" % (page, framenum)),"JPEG",quality=99)
                framenum+=1

        spumuxdom.documentElement.firstChild.firstChild.setAttribute("select",os.path.join(getTempPath(),"chaptermenumask-%s.png" % page))
        spumuxdom.documentElement.firstChild.firstChild.setAttribute("highlight",os.path.join(getTempPath(),"chaptermenumask-%s.png" % page))

        #Release large amounts of memory ASAP !
        del draw
        del bgimage
        del drawmask
        del bgimagemask
        del overlayimage
        del previewx
        del previewy
        del previewmask

        #write( spumuxdom.toprettyxml())
        WriteXMLToFile (spumuxdom,os.path.join(getTempPath(),"chapterspumux-%s.xml" % page))

        if chaptermenuAspectRatio == "4:3":
            aspect_ratio = '2'
        elif chaptermenuAspectRatio == "16:9":
            aspect_ratio = '3'
        else: 
            if getAspectRatioOfVideo(page) > aspectRatioThreshold:
                aspect_ratio = '3'
            else:
                aspect_ratio = '2'

        write("Encoding Chapter Menu Page %s using aspect ratio '%s'" % (page, chaptermenuAspectRatio))

        if haspreview == True:
            encodeMenu(os.path.join(getTempPath(),"chaptermenu-%s-f%%06d.jpg" % page),
                        os.path.join(getTempPath(),"temp.m2v"),
                        getThemeFile(themeName,menumusic),
                        menulength,
                        os.path.join(getTempPath(),"temp.mpg"),
                        os.path.join(getTempPath(),"chapterspumux-%s.xml" % page),
                        os.path.join(getTempPath(),"chaptermenu-%s.mpg" % page),
                        aspect_ratio)
        else:
            encodeMenu(os.path.join(getTempPath(),"chaptermenu-%s.jpg" % page),
                        os.path.join(getTempPath(),"temp.m2v"),
                        getThemeFile(themeName,menumusic),
                        menulength,
                        os.path.join(getTempPath(),"temp.mpg"),
                        os.path.join(getTempPath(),"chapterspumux-%s.xml" % page),
                        os.path.join(getTempPath(),"chaptermenu-%s.mpg" % page),
                        aspect_ratio)

        #Move on to the next page
        page+=1

#############################################################
# creates the details page for a file on a DVD

def createDetailsPage(screensize, screendpi, numberofitems):
    """Creates all the necessary images and files for the details page."""

    write( "Creating details pages")

    #Get the detailspage node (we must only have 1)
    detailnode=themeDOM.getElementsByTagName("detailspage")
    if detailnode.length!=1:
        fatalError("Cannot find detailspage element in theme file")
    detailnode=detailnode[0]

    #Get background image filename
    backgroundfilename = detailnode.attributes["background"].value
    if backgroundfilename=="":
        fatalError("Background image is not set in theme file")
    backgroundfilename = getThemeFile(themeName,backgroundfilename)
    write( "Background image file is %s" % backgroundfilename)
    if not doesFileExist(backgroundfilename):
        fatalError("Background image not found (%s)" % backgroundfilename)

    #Get menu music
    menumusic = "menumusic.ac3"
    if detailnode.hasAttribute("music"):
        menumusic = detailnode.attributes["music"].value

    #Get menu length
    menulength = 15
    if detailnode.hasAttribute("length"):
        menulength = int(detailnode.attributes["length"].value)

    write("Music is %s, length is %s seconds" % (menumusic, menulength))

    #Item counter to indicate current video item
    itemnum=1

    while itemnum <= numberofitems:
        write( "Creating details page for %s" % itemnum)

        #make sure the preview folder is empty and present
        previewfolder = createEmptyPreviewFolder(itemnum)
        haspreview = False

        #and then generate the preview if required (px=9999 means not required)
        previewx, previewy, previeww, previewh, previewmask = generateVideoPreview(itemnum, 1, detailnode, 0, menulength, previewfolder)
        if previewx != 9999:
            haspreview = True

        #instead of loading the background image and drawing on it we now
        #make a transparent image and draw all items on it. This overlay
        #image is then added to the required background image when the
        #preview items are added (the reason for this is it will assist
        #if the background image is actually a video)

        overlayimage=Image.new("RGBA",screensize, (0,0,0,0))
        draw=ImageDraw.Draw(overlayimage)

        spumuxdom = xml.dom.minidom.parseString('<subpictures><stream><spu force="yes" start="00:00:00.0" highlight="" select="" ></spu></stream></subpictures>')
        spunode = spumuxdom.documentElement.firstChild.firstChild

        drawThemeItem(0, 0, itemnum, detailnode, overlayimage, draw, None, None,
                      "", spumuxdom, spunode, numberofitems, 0, "")

        #Save this details image
        bgimage=Image.open(backgroundfilename,"r").resize(screensize)
        bgimage.paste(overlayimage, (0,0), overlayimage)
        bgimage.save(os.path.join(getTempPath(),"details-%s.jpg" % itemnum),"JPEG", quality=99)

        if haspreview == True:
            numframes=secondsToFrames(menulength)

            #numframes should be the number of preview images that have been created
            write( "Generating the detail preview images" )
            framenum = 0
            while framenum < numframes:
                if previewx != 9999:
                    previewpath = os.path.join(getItemTempPath(itemnum), "preview")
                    previewfile = "preview-i%d-t1-f%d.jpg" % (1, framenum)
                    imagefile = os.path.join(previewpath, previewfile)

                    if doesFileExist(imagefile):
                        picture = Image.open(imagefile, "r").resize((previeww, previewh))
                        picture = picture.convert("RGBA")
                        imagemaskfile = os.path.join(previewpath, "mask-i%d.png" % 1)
                        if previewmask != None:
                            bgimage.paste(picture, (previewx, previewy), previewmask)
                        else:
                            bgimage.paste(picture, (previewx, previewy))
                        del picture
                bgimage.save(os.path.join(getTempPath(),"details-%s-f%06d.jpg" % (itemnum, framenum)),"JPEG",quality=99)
                framenum+=1


        #Release large amounts of memory ASAP !
        del draw
        del bgimage

        # always use the same aspect ratio as the video
        aspect_ratio='2'
        if getAspectRatioOfVideo(itemnum) > aspectRatioThreshold:
            aspect_ratio='3'

        #write( spumuxdom.toprettyxml())
        WriteXMLToFile (spumuxdom,os.path.join(getTempPath(),"detailsspumux-%s.xml" % itemnum))

        write("Encoding Details Page %s" % itemnum)
        if haspreview == True:
            encodeMenu(os.path.join(getTempPath(),"details-%s-f%%06d.jpg" % itemnum),
                        os.path.join(getTempPath(),"temp.m2v"),
                        getThemeFile(themeName,menumusic),
                        menulength,
                        os.path.join(getTempPath(),"temp.mpg"),
                        "",
                        os.path.join(getTempPath(),"details-%s.mpg" % itemnum),
                        aspect_ratio)
        else:
            encodeMenu(os.path.join(getTempPath(),"details-%s.jpg" % itemnum),
                        os.path.join(getTempPath(),"temp.m2v"),
                        getThemeFile(themeName,menumusic),
                        menulength,
                        os.path.join(getTempPath(),"temp.mpg"),
                        "",
                        os.path.join(getTempPath(),"details-%s.mpg" % itemnum),
                        aspect_ratio)

        #On to the next item
        itemnum+=1

#############################################################
# checks if a file is an avi file

def isMediaAVIFile(file):
    fh = open(file, 'rb')
    Magic = fh.read(4)
    fh.close()
    return Magic=="RIFF"

#############################################################
# checks to see if an audio stream need to be converted to ac3 

def processAudio(folder):
    """encode audio to ac3 for better compression and compatability with NTSC players"""

    # process track 1
    if not encodetoac3 and doesFileExist(os.path.join(folder,'stream0.mp2')):
        #don't re-encode to ac3 if the user doesn't want it
        write( "Audio track 1 is in mp2 format - NOT re-encoding to ac3")
    elif doesFileExist(os.path.join(folder,'stream0.mp2'))==True:
        write( "Audio track 1 is in mp2 format - re-encoding to ac3")
        encodeAudio("ac3",os.path.join(folder,'stream0.mp2'), os.path.join(folder,'stream0.ac3'),True)
    elif doesFileExist(os.path.join(folder,'stream0.mpa'))==True:
        write( "Audio track 1 is in mpa format - re-encoding to ac3")
        encodeAudio("ac3",os.path.join(folder,'stream0.mpa'), os.path.join(folder,'stream0.ac3'),True)
    elif doesFileExist(os.path.join(folder,'stream0.ac3'))==True:
        write( "Audio is already in ac3 format")
    else:
        fatalError("Track 1 - Unknown audio format or de-multiplex failed!")

    # process track 2
    if not encodetoac3 and doesFileExist(os.path.join(folder,'stream1.mp2')):
        #don't re-encode to ac3 if the user doesn't want it
        write( "Audio track 1 is in mp2 format - NOT re-encoding to ac3")
    elif doesFileExist(os.path.join(folder,'stream1.mp2'))==True:
        write( "Audio track 2 is in mp2 format - re-encoding to ac3")
        encodeAudio("ac3",os.path.join(folder,'stream1.mp2'), os.path.join(folder,'stream1.ac3'),True)
    elif doesFileExist(os.path.join(folder,'stream1.mpa'))==True:
        write( "Audio track 2 is in mpa format - re-encoding to ac3")
        encodeAudio("ac3",os.path.join(folder,'stream1.mpa'), os.path.join(folder,'stream1.ac3'),True)
    elif doesFileExist(os.path.join(folder,'stream1.ac3'))==True:
        write( "Audio is already in ac3 format")

#############################################################
# chooses which streams from a file to include on the DVD

# tuple index constants
VIDEO_INDEX = 0
VIDEO_CODEC = 1
VIDEO_ID    = 2

AUDIO_INDEX = 0
AUDIO_CODEC = 1
AUDIO_ID    = 2
AUDIO_LANG  = 3

def selectStreams(folder):
    """Choose the streams we want from the source file"""

    video    = (-1, 'N/A', -1)         # index, codec, ID
    audio1   = (-1, 'N/A', -1, 'N/A')  # index, codec, ID, lang
    audio2   = (-1, 'N/A', -1, 'N/A')

    #open the XML containing information about this file
    infoDOM = xml.dom.minidom.parse(os.path.join(folder, 'streaminfo.xml'))
    #error out if its the wrong XML
    if infoDOM.documentElement.tagName != "file":
        fatalError("This does not look like a stream info file (%s)" % os.path.join(folder, 'streaminfo.xml'))


    #get video ID, CODEC
    nodes = infoDOM.getElementsByTagName("video")
    if nodes.length == 0:
        write("Didn't find any video elements in stream info file.!!!")
        write("");
        sys.exit(1)
    if nodes.length > 1:
        write("Found more than one video element in stream info file.!!!")
    node = nodes[0]
    video = (int(node.attributes["ffmpegindex"].value), node.attributes["codec"].value, int(node.attributes["id"].value))

    #get audioID's - we choose the best 2 audio streams using this algorithm
    # 1. if there is one or more stream(s) using the 1st preferred language we use that
    # 2. if there is one or more stream(s) using the 2nd preferred language we use that
    # 3. if we still haven't found a stream we use the stream with the lowest PID
    # 4. we prefer ac3 over mp2
    # 5. if there are more that one stream with the chosen language we use the one with the lowest PID

    write("Preferred audio languages %s and %s" % (preferredlang1, preferredlang2))

    nodes = infoDOM.getElementsByTagName("audio")

    if nodes.length == 0:
        write("Didn't find any audio elements in stream info file.!!!")
        write("");
        sys.exit(1)

    found = False
    # first try to find a stream with ac3 and preferred language 1
    for node in nodes:
        index = int(node.attributes["ffmpegindex"].value)
        lang = node.attributes["language"].value
        format = string.upper(node.attributes["codec"].value)
        pid = int(node.attributes["id"].value)
        if lang == preferredlang1 and format == "AC3":
            if found:
                if pid < audio1[AUDIO_ID]:
                    audio1 = (index, format, pid, lang)
            else:
                audio1 = (index, format, pid, lang)
            found = True

    # second try to find a stream with mp2 and preferred language 1
    if not found:
        for node in nodes:
            index = int(node.attributes["ffmpegindex"].value)
            lang = node.attributes["language"].value
            format = string.upper(node.attributes["codec"].value)
            pid = int(node.attributes["id"].value)
            if lang == preferredlang1 and format == "MP2":
                if found:
                    if pid < audio1[AUDIO_ID]:
                        audio1 = (index, format, pid, lang)
                else:
                    audio1 = (index, format, pid, lang)
                found = True

    # finally use the stream with the lowest pid, prefer ac3 over mp2
    if not found:
        for node in nodes:
            index = int(node.attributes["ffmpegindex"].value)
            format = string.upper(node.attributes["codec"].value)
            pid = int(node.attributes["id"].value)
            if not found:
                audio1 = (index, format, pid, lang)
                found = True
            else:
                if format == "AC3" and audio1[AUDIO_CODEC] == "MP2":
                    audio1 = (index, format, pid, lang)
                else:
                    if pid < audio1[AUDIO_ID]:
                        audio1 = (index, format, pid, lang)

    # do we need to find a second audio stream?
    if preferredlang1 != preferredlang2 and nodes.length > 1:
        found = False
        # first try to find a stream with ac3 and preferred language 2
        for node in nodes:
            index = int(node.attributes["ffmpegindex"].value)
            lang = node.attributes["language"].value
            format = string.upper(node.attributes["codec"].value)
            pid = int(node.attributes["id"].value)
            if lang == preferredlang2 and format == "AC3":
                if found:
                    if pid < audio2[AUDIO_ID]:
                        audio2 = (index, format, pid, lang)
                else:
                    audio2 = (index, format, pid, lang)
                found = True

        # second try to find a stream with mp2 and preferred language 2
        if not found:
            for node in nodes:
                index = int(node.attributes["ffmpegindex"].value)
                lang = node.attributes["language"].value
                format = string.upper(node.attributes["codec"].value)
                pid = int(node.attributes["id"].value)
                if lang == preferredlang2 and format == "MP2":
                    if found:
                        if pid < audio2[AUDIO_ID]:
                            audio2 = (index, format, pid, lang)
                    else:
                        audio2 = (index, format, pid, lang)
                    found = True

        # finally use the stream with the lowest pid, prefer ac3 over mp2
        if not found:
            for node in nodes:
                index = int(node.attributes["ffmpegindex"].value)
                format = string.upper(node.attributes["codec"].value)
                pid = int(node.attributes["id"].value)
                if not found:
                    # make sure we don't choose the same stream as audio1
                    if pid != audio1[AUDIO_ID]:
                        audio2 = (index, format, pid, lang)
                        found = True
                else:
                    if format == "AC3" and audio2[AUDIO_CODEC] == "MP2" and pid != audio1[AUDIO_ID]:
                        audio2 = (index, format, pid, lang)
                    else:
                        if pid < audio2[AUDIO_ID] and pid != audio1[AUDIO_ID]:
                            audio2 = (index, format, pid, lang)

    write("Video id: 0x%x, Audio1: [%d] 0x%x (%s, %s), Audio2: [%d] - 0x%x (%s, %s)" % \
        (video[VIDEO_ID], audio1[AUDIO_INDEX], audio1[AUDIO_ID], audio1[AUDIO_CODEC], audio1[AUDIO_LANG], \
         audio2[AUDIO_INDEX], audio2[AUDIO_ID], audio2[AUDIO_CODEC], audio2[AUDIO_LANG]))

    return (video, audio1, audio2)

#############################################################
# gets the video aspect ratio from the stream info xml file

def selectAspectRatio(folder):
    """figure out what aspect ratio we want from the source file"""

    #this should be smarter and look though the file for any AR changes
    #at the moment it just uses the AR found at the start of the file

    #open the XML containing information about this file
    infoDOM = xml.dom.minidom.parse(os.path.join(folder, 'streaminfo.xml'))
    #error out if its the wrong XML
    if infoDOM.documentElement.tagName != "file":
        fatalError("This does not look like a stream info file (%s)" % os.path.join(folder, 'streaminfo.xml'))


    #get aspect ratio
    nodes = infoDOM.getElementsByTagName("video")
    if nodes.length == 0:
        write("Didn't find any video elements in stream info file.!!!")
        write("");
        sys.exit(1)
    if nodes.length > 1:
        write("Found more than one video element in stream info file.!!!")
    node = nodes[0]
    try:
        ar = float(node.attributes["aspectratio"].value)
        if ar > float(4.0/3.0 - 0.01) and ar < float(4.0/3.0 + 0.01):
            aspectratio = "4:3"
            write("Aspect ratio is 4:3")
        elif ar > float(16.0/9.0 - 0.01) and ar < float(16.0/9.0 + 0.01):
            aspectratio = "16:9"
            write("Aspect ratio is 16:9")
        else:
            write("Unknown aspect ratio %f - Using 16:9" % ar)
            aspectratio = "16:9"
    except:
        aspectratio = "16:9"

    return aspectratio

#############################################################
# gets video stream codec from the stream info xml file

def getVideoCodec(folder):
    """Get the video codec from the streaminfo.xml for the file"""

    #open the XML containing information about this file
    infoDOM = xml.dom.minidom.parse(os.path.join(folder, 'streaminfo.xml'))
    #error out if its the wrong XML
    if infoDOM.documentElement.tagName != "file":
        fatalError("This does not look like a stream info file (%s)" % os.path.join(folder, 'streaminfo.xml'))

    nodes = infoDOM.getElementsByTagName("video")
    if nodes.length == 0:
        write("Didn't find any video elements in stream info file!!!")
        write("");
        sys.exit(1)
    if nodes.length > 1:
        write("Found more than one video element in stream info file!!!")
    node = nodes[0]
    return node.attributes["codec"].value

#############################################################
# gets file container type from the stream info xml file

def getFileType(folder):
    """Get the overall file type from the streaminfo.xml for the file"""

    #open the XML containing information about this file
    infoDOM = xml.dom.minidom.parse(os.path.join(folder, 'streaminfo.xml'))
    #error out if its the wrong XML
    if infoDOM.documentElement.tagName != "file":
        fatalError("This does not look like a stream info file (%s)" % os.path.join(folder, 'streaminfo.xml'))

    nodes = infoDOM.getElementsByTagName("file")
    if nodes.length == 0:
        write("Didn't find any file elements in stream info file!!!")
        write("");
        sys.exit(1)
    if nodes.length > 1:
        write("Found more than one file element in stream info file!!!")
    node = nodes[0]

    return node.attributes["type"].value

#############################################################
# check if file is DVD compliant

def isFileOkayForDVD(file, folder):
    """return true if the file is dvd compliant"""

    if string.lower(getVideoCodec(folder)) != "mpeg2video":
        return False

#    if string.lower(getAudioCodec(folder)) != "ac3" and encodeToAC3:
#        return False

    videosize = getVideoSize(os.path.join(folder, "streaminfo.xml"))

    # has the user elected to re-encode the file
    if file.hasAttribute("encodingprofile"):
        if file.attributes["encodingprofile"].value != "NONE":
            write("File will be re-encoded using profile %s" % file.attributes["encodingprofile"].value)
            return False

    if not isResolutionOkayForDVD(videosize):
        # file does not have a dvd resolution
        if file.hasAttribute("encodingprofile"):
            if file.attributes["encodingprofile"].value == "NONE":
                write("WARNING: File does not have a DVD compliant resolution but "
                      "you have selected not to re-encode the file")
                return True
        else:
            return False

    return True

#############################################################
# process a single file ready for burning

def processFile(file, folder):
    """Process a single video/recording file ready for burning."""

    write( "*************************************************************")
    write( "Processing file " + file.attributes["filename"].value + " of type " + file.attributes["type"].value)
    write( "*************************************************************")

    #As part of this routine we need to pre-process the video this MAY mean:
    #1. removing commercials/cleaning up mpeg2 stream
    #2. encoding to mpeg2 (if its an avi for instance or isn't DVD compatible)
    #3. selecting audio track to use and encoding audio from mp2 into ac3
    #4. de-multiplexing into video and audio steams)

    mediafile=""

    if file.hasAttribute("localfilename"):
        mediafile=file.attributes["localfilename"].value
    elif file.attributes["type"].value=="recording":
        mediafile = file.attributes["filename"].value
    elif file.attributes["type"].value=="video":
        mediafile=os.path.join(videopath, file.attributes["filename"].value)
    elif file.attributes["type"].value=="file":
        mediafile=file.attributes["filename"].value
    else:
        fatalError("Unknown type of video file it must be 'recording', 'video' or 'file'.")

    #Get the XML containing information about this item
    infoDOM = xml.dom.minidom.parse( os.path.join(folder,"info.xml") )
    #Error out if its the wrong XML
    if infoDOM.documentElement.tagName != "fileinfo":
        fatalError("The info.xml file (%s) doesn't look right" % os.path.join(folder,"info.xml"))

    #If this is an mpeg2 myth recording and there is a cut list available and the user wants to use it
    #run mythtranscode to cut out commercials etc
    if file.attributes["type"].value == "recording":
        #can only use mythtranscode to cut commercials on mpeg2 files
        write("File type is '%s'" % getFileType(folder))
        write("Video codec is '%s'" % getVideoCodec(folder))
        if string.lower(getVideoCodec(folder)) == "mpeg2video": 
            if file.attributes["usecutlist"].value == "1" and getText(infoDOM.getElementsByTagName("hascutlist")[0]) == "yes":
                # Run from local file?
                if file.hasAttribute("localfilename"):
                    localfile = file.attributes["localfilename"].value
                else:
                    localfile = ""
                write("File has a cut list - running mythtrancode to remove unwanted segments")
                chanid = getText(infoDOM.getElementsByTagName("chanid")[0])
                starttime = getText(infoDOM.getElementsByTagName("starttime")[0])
                if runMythtranscode(chanid, starttime, os.path.join(folder,'tmp'), True, localfile):
                    mediafile = os.path.join(folder,'tmp')
                else:
                    write("Failed to run mythtranscode to remove unwanted segments")
            else:
                #does the user always want to run recordings through mythtranscode?
                #may help to fix any errors in the file 
                if (alwaysRunMythtranscode == True or 
                        (getFileType(folder) == "mpegts" and isFileOkayForDVD(file, folder))):
                    # Run from local file?
                    if file.hasAttribute("localfilename"):
                        localfile = file.attributes["localfilename"].value
                    else:
                        localfile = ""
                    write("Running mythtranscode --mpeg2 to fix any errors")
                    chanid = getText(infoDOM.getElementsByTagName("chanid")[0])
                    starttime = getText(infoDOM.getElementsByTagName("starttime")[0])
                    if runMythtranscode(chanid, starttime, os.path.join(folder, 'newfile.mpg'), False, localfile):
                        mediafile = os.path.join(folder, 'newfile.mpg')
                    else:
                        write("Failed to run mythtrancode to fix any errors")
    else:
        #does the user always want to run mpeg2 files through mythtranscode?
        #may help to fix any errors in the file 
        write("File type is '%s'" % getFileType(folder))
        write("Video codec is '%s'" % getVideoCodec(folder))

        if (alwaysRunMythtranscode == True and 
                string.lower(getVideoCodec(folder)) == "mpeg2video" and
                isFileOkayForDVD(file, folder)):
            if file.hasAttribute("localfilename"):
                localfile = file.attributes["localfilename"].value
            else:
                localfile = file.attributes["filename"].value
            write("Running mythtranscode --mpeg2 to fix any errors")
            chanid = -1
            starttime = -1
            if runMythtranscode(chanid, starttime, os.path.join(folder, 'newfile.mpg'), False, localfile):
                mediafile = os.path.join(folder, 'newfile.mpg')
            else:
                write("Failed to run mythtrancode to fix any errors")

    #do we need to re-encode the file to make it DVD compliant?
    if not isFileOkayForDVD(file, folder):
        if getFileType(folder) == 'nuv':
            #file is a nuv file which ffmpeg has problems reading so use mythtranscode to pass
            #the video and audio streams to ffmpeg to do the reencode

            #we need to re-encode the file, make sure we get the right video/audio streams
            #would be good if we could also split the file at the same time
            getStreamInformation(mediafile, os.path.join(folder, "streaminfo.xml"), 0)

            #choose which streams we need
            video, audio1, audio2 = selectStreams(folder)

            #choose which aspect ratio we should use
            aspectratio = selectAspectRatio(folder)

            write("Re-encoding audio and video from nuv file")

            # what encoding profile should we use
            if file.hasAttribute("encodingprofile"):
                profile = file.attributes["encodingprofile"].value
            else:
                profile = defaultEncodingProfile

            if file.hasAttribute("localfilename"):
                mediafile = file.attributes["localfilename"].value
                chanid = -1
                starttime = -1
                usecutlist = -1
            elif file.attributes["type"].value == "recording":
                mediafile = -1
                chanid = getText(infoDOM.getElementsByTagName("chanid")[0])
                starttime = getText(infoDOM.getElementsByTagName("starttime")[0])
                usecutlist = (file.attributes["usecutlist"].value == "1" and 
                            getText(infoDOM.getElementsByTagName("hascutlist")[0]) == "yes")
            else:
                chanid = -1
                starttime = -1
                usecutlist = -1

            encodeNuvToMPEG2(chanid, starttime, mediafile, os.path.join(folder, "newfile2.mpg"), folder,
                         profile, usecutlist)
            mediafile = os.path.join(folder, 'newfile2.mpg')
        else:
            #we need to re-encode the file, make sure we get the right video/audio streams
            #would be good if we could also split the file at the same time
            getStreamInformation(mediafile, os.path.join(folder, "streaminfo.xml"), 0)

            #choose which streams we need
            video, audio1, audio2 = selectStreams(folder)

            #choose which aspect ratio we should use
            aspectratio = selectAspectRatio(folder)

            write("Re-encoding audio and video")

            # Run from local file?
            if file.hasAttribute("localfilename"):
                mediafile = file.attributes["localfilename"].value

            # what encoding profile should we use
            if file.hasAttribute("encodingprofile"):
                profile = file.attributes["encodingprofile"].value
            else:
                profile = defaultEncodingProfile

            #do the re-encode 
            encodeVideoToMPEG2(mediafile, os.path.join(folder, "newfile2.mpg"), video,
                            audio1, audio2, aspectratio, profile)
            mediafile = os.path.join(folder, 'newfile2.mpg')

    #remove an intermediate file
    if os.path.exists(os.path.join(folder, "newfile1.mpg")):
        os.remove(os.path.join(folder,'newfile1.mpg'))

    # the file is now DVD compliant split it into video and audio parts

    # find out what streams we have available now
    getStreamInformation(mediafile, os.path.join(folder, "streaminfo.xml"), 1)

    # choose which streams we need
    video, audio1, audio2 = selectStreams(folder)

    # now attempt to split the source file into video and audio parts
    write("Splitting MPEG stream into audio and video parts")
    deMultiplexMPEG2File(folder, mediafile, video, audio1, audio2)

    if os.path.exists(os.path.join(folder, "newfile2.mpg")):
        os.remove(os.path.join(folder,'newfile2.mpg'))

    # we now have a video stream and one or more audio streams
    # check if we need to convert any of the audio streams to ac3
    processAudio(folder)

    # if we don't already have one find a title thumbnail image
    titleImage = os.path.join(folder, "title.jpg")
    if not os.path.exists(titleImage):
        # if the file is a recording try to use its preview image for the thumb
        if file.attributes["type"].value == "recording":
            previewImage = file.attributes["filename"].value + ".png"
            if usebookmark == True and os.path.exists(previewImage):
                copy(previewImage, titleImage)
            else:
                extractVideoFrame(os.path.join(folder, "stream.mv2"), titleImage, thumboffset)
        else:
            extractVideoFrame(os.path.join(folder, "stream.mv2"), titleImage, thumboffset)

    write( "*************************************************************")
    write( "Finished processing file " + file.attributes["filename"].value)
    write( "*************************************************************")

#############################################################
# copy files on remote filesystems to the local filesystem

def copyRemote(files, tmpPath):
    '''go through the list of files looking for files on remote filesytems
       and copy them to a local file for quicker processing'''
    localTmpPath = os.path.join(tmpPath, "localcopy")
    for node in files:
        tmpfile = node.attributes["filename"].value
        filename = os.path.basename(tmpfile)

        res = runCommand("mytharchivehelper -r " + quoteFilename(tmpfile))
        if res == 2:
            # file is on a remote filesystem so copy it to a local file
            write("Copying file from " + tmpfile)
            write("to " + os.path.join(localTmpPath, filename))

            # Copy file
            if not doesFileExist(os.path.join(localTmpPath, filename)):
                copy(tmpfile, os.path.join(localTmpPath, filename))

            # update node
            node.setAttribute("localfilename", os.path.join(localTmpPath, filename))
    return files

#############################################################
# processes one job

def processJob(job):
    """Starts processing a MythBurn job, expects XML nodes to be passed as input."""
    global wantIntro, wantMainMenu, wantChapterMenu, wantDetailsPage
    global themeDOM, themeName, themeFonts


    media=job.getElementsByTagName("media")

    if media.length==1:

        themeName=job.attributes["theme"].value

        #Check theme exists
        if not validateTheme(themeName):
            fatalError("Failed to validate theme (%s)" % themeName)
        #Get the theme XML
        themeDOM = getThemeConfigurationXML(themeName)

        #Pre generate all the fonts we need
        loadFonts(themeDOM)

        #Update the global flags
        nodes=themeDOM.getElementsByTagName("intro")
        wantIntro = (nodes.length > 0)

        nodes=themeDOM.getElementsByTagName("menu")
        wantMainMenu = (nodes.length > 0)

        nodes=themeDOM.getElementsByTagName("submenu")
        wantChapterMenu = (nodes.length > 0)

        nodes=themeDOM.getElementsByTagName("detailspage")
        wantDetailsPage = (nodes.length > 0)

        write( "wantIntro: %d, wantMainMenu: %d, wantChapterMenu:%d, wantDetailsPage: %d" \
                % (wantIntro, wantMainMenu, wantChapterMenu, wantDetailsPage))

        if videomode=="ntsc":
            format=dvdNTSC
            dpi=dvdNTSCdpi
        elif videomode=="pal":
            format=dvdPAL
            dpi=dvdPALdpi
        else:
            fatalError("Unknown videomode is set (%s)" % videomode)

        write( "Final DVD Video format will be " + videomode)

        #Ensure the destination dvd folder is empty
        if doesFileExist(os.path.join(getTempPath(),"dvd")):
            deleteAllFilesInFolder(os.path.join(getTempPath(),"dvd"))

        #Loop through all the files
        files=media[0].getElementsByTagName("file")
        filecount=0
        if files.length > 0:
            write( "There are %s files to process" % files.length)

            if debug_secondrunthrough==False:
                #Delete all the temporary files that currently exist
                deleteAllFilesInFolder(getTempPath())

            #If User wants to, copy remote files to a tmp dir
            if copyremoteFiles==True:
                if debug_secondrunthrough==False:
                    localCopyFolder=os.path.join(getTempPath(),"localcopy")
                    #If it already exists destroy it to remove previous debris
                    if os.path.exists(localCopyFolder):
                        #Remove all the files first
                        deleteAllFilesInFolder(localCopyFolder)
                        #Remove the folder
                        os.rmdir (localCopyFolder)
                    os.makedirs(localCopyFolder)
                files=copyRemote(files,getTempPath())

            #First pass through the files to be recorded - sense check
            #we dont want to find half way through this long process that
            #a file does not exist, or is the wrong format!!
            for node in files:
                filecount+=1

                #Generate a temp folder name for this file
                folder=getItemTempPath(filecount)

                if debug_secondrunthrough==False:
                    #If it already exists destroy it to remove previous debris
                    if os.path.exists(folder):
                        #Remove all the files first
                        deleteAllFilesInFolder(folder)
                        previewfolder = os.path.join(folder, "preview")
                        if os.path.exists(previewfolder):
                            deleteAllFilesInFolder(previewfolder)
                            os.rmdir(previewfolder)
                        #Remove the folder
                        os.rmdir (folder)
                    os.makedirs(folder)
                #Do the pre-process work
                preProcessFile(node,folder)

            if debug_secondrunthrough==False:
                #Loop through all the files again but this time do more serious work!
                filecount=0
                for node in files:
                    filecount+=1
                    folder=getItemTempPath(filecount)

                    #Process this file
                    processFile(node,folder)

            #We can only create the menus after the videos have been processed
            #and the commercials cut out so we get the correct run time length
            #for the chapter marks and thumbnails.
            #create the DVD menus...
            if wantMainMenu:
                createMenu(format, dpi, files.length)

            #Submenus are visible when you select the chapter menu while the recording is playing
            if wantChapterMenu:
                createChapterMenu(format, dpi, files.length)

            #Details Page are displayed just before playing each recording
            if wantDetailsPage:
                createDetailsPage(format, dpi, files.length)

            #DVD Author file
            if not wantMainMenu and not wantChapterMenu:
                createDVDAuthorXMLNoMenus(format, files.length)
            elif not wantMainMenu:
                createDVDAuthorXMLNoMainMenu(format, files.length)
            else:
                createDVDAuthorXML(format, files.length)

            #Check all the files will fit onto a recordable DVD
            if mediatype == DVD_DL:
                # dual layer
                performMPEG2Shrink(files, dvdrsize[1])
            else:
                #single layer
                performMPEG2Shrink(files, dvdrsize[0])

            filecount=0
            for node in files:
                filecount+=1
                folder=getItemTempPath(filecount)
                #Multiplex this file
                #(This also removes non-required audio feeds inside mpeg streams 
                #(through re-multiplexing) we only take 1 video and 1 or 2 audio streams)
                pid=multiplexMPEGStream(os.path.join(folder,'stream.mv2'),
                        os.path.join(folder,'stream0'),
                        os.path.join(folder,'stream1'),
                        os.path.join(folder,'final.mpg'),
                        calcSyncOffset(filecount))

            #Now all the files are completed and ready to be burnt
            runDVDAuthor()

            #Create the DVD ISO image
            if docreateiso == True or mediatype == FILE:
                CreateDVDISO()

            #Burn the DVD ISO image
            if doburn == True and mediatype != FILE:
                BurnDVDISO()

            #Move the created iso image to the given location
            if mediatype == FILE and savefilename != "":
                write("Moving ISO image to: %s" % savefilename)
                try:
                    os.rename(os.path.join(getTempPath(), 'mythburn.iso'), savefilename)
                except:
                    f1 = open(os.path.join(getTempPath(), 'mythburn.iso'), 'rb')
                    f2 = open(savefilename, 'wb')
                    data = f1.read(1024 * 1024)
                    while data:
                        f2.write(data)
                        data = f1.read(1024 * 1024)
                    f1.close()
                    f2.close()
                    os.unlink(os.path.join(getTempPath(), 'mythburn.iso'))
        else:
            write( "Nothing to do! (files)")
    else:
        write( "Nothing to do! (media)")
    return

#############################################################
# show usage

def usage():
    write("""
    -h/--help               (Show this usage)
    -j/--jobfile file       (use file as the job file)
    -l/--progresslog file   (log file to output progress messages)

    """)

#############################################################
# The main starting point for mythburn.py

def main():
    global sharepath, scriptpath, cpuCount, videopath, gallerypath, musicpath
    global videomode, temppath, logpath, dvddrivepath, dbVersion, preferredlang1
    global preferredlang2, useFIFO, encodetoac3, alwaysRunMythtranscode
    global copyremoteFiles, mainmenuAspectRatio, chaptermenuAspectRatio, dateformat
    global timeformat, clearArchiveTable, nicelevel, drivespeed, path_mplex, path_ffmpeg
    global path_dvdauthor, path_mkisofs, path_growisofs, path_tcrequant
    global path_jpeg2yuv, path_spumux, path_mpeg2enc, progresslog
    global progressfile, jobfile

    write( "mythburn.py (%s) starting up..." % VERSION)

    #Ensure we are running at least python 2.3.5
    if not hasattr(sys, "hexversion") or sys.hexversion < 0x20305F0:
        sys.stderr.write("Sorry, your Python is too old. Please upgrade at least to 2.3.5\n")
        sys.exit(1)

    # figure out where this script is located
    scriptpath = os.path.dirname(sys.argv[0])
    scriptpath = os.path.abspath(scriptpath)
    write("script path:" + scriptpath)

    # figure out where the myth share directory is located
    sharepath = os.path.split(scriptpath)[0]
    sharepath = os.path.split(sharepath)[0]
    write("myth share path:" + sharepath)

    # process any command line options
    try:
        opts, args = getopt.getopt(sys.argv[1:], "j:hl:", ["jobfile=", "help", "progresslog="])
    except getopt.GetoptError:
        # print usage and exit
        usage()
        sys.exit(2)

    for o, a in opts:
        if o in ("-h", "--help"):
            usage()
            sys.exit()
        if o in ("-j", "--jobfile"):
            jobfile = str(a)
            write("passed job file: " + a)
        if o in ("-l", "--progresslog"):
            progresslog = str(a)
            write("passed progress log file: " + a)

    #if we have been given a progresslog filename to write to open it
    if progresslog != "":
        if os.path.exists(progresslog):
            os.remove(progresslog)
        progressfile = open(progresslog, 'w')
        write( "mythburn.py (%s) starting up..." % VERSION)

    #Get mysql database parameters
    getMysqlDBParameters()

    saveSetting("MythArchiveLastRunStart", time.strftime("%Y-%m-%d %H:%M:%S "))
    saveSetting("MythArchiveLastRunType", "DVD")
    saveSetting("MythArchiveLastRunStatus", "Running")

    cpuCount = getCPUCount()

    #if the script is run from the web interface the PATH environment variable does not include
    #many of the bin locations we need so just append a few likely locations where our required
    #executables may be
    if not os.environ['PATH'].endswith(':'):
        os.environ['PATH'] += ":"
    os.environ['PATH'] += "/bin:/sbin:/usr/local/bin:/usr/bin:/opt/bin:" + installPrefix +"/bin:"

    #Get defaults from MythTV database
    defaultsettings = getDefaultParametersFromMythTVDB()
    videopath = defaultsettings.get("VideoStartupDir", None)
    gallerypath = defaultsettings.get("GalleryDir", None)
    musicpath = defaultsettings.get("MusicLocation", None)
    videomode = string.lower(defaultsettings["MythArchiveVideoFormat"])
    temppath = os.path.join(defaultsettings["MythArchiveTempDir"], "work")
    logpath = os.path.join(defaultsettings["MythArchiveTempDir"], "logs")
    write("temppath: " + temppath)
    write("logpath:  " + logpath)
    dvddrivepath = defaultsettings["MythArchiveDVDLocation"]
    dbVersion = defaultsettings["DBSchemaVer"]
    preferredlang1 = defaultsettings["ISO639Language0"]
    preferredlang2 = defaultsettings["ISO639Language1"]
    useFIFO = (defaultsettings["MythArchiveUseFIFO"] == '1')
    encodetoac3 = (defaultsettings["MythArchiveEncodeToAc3"] == '1')
    alwaysRunMythtranscode = (defaultsettings["MythArchiveAlwaysUseMythTranscode"] == '1')
    copyremoteFiles = (defaultsettings["MythArchiveCopyRemoteFiles"] == '1')
    mainmenuAspectRatio = defaultsettings["MythArchiveMainMenuAR"]
    chaptermenuAspectRatio = defaultsettings["MythArchiveChapterMenuAR"]
    dateformat = defaultsettings.get("MythArchiveDateFormat", "%a %d %b %Y")
    timeformat = defaultsettings.get("MythArchiveTimeFormat", "%I:%M %p")
    drivespeed = int(defaultsettings.get("MythArchiveDriveSpeed", "0"))
    if "MythArchiveClearArchiveTable" in defaultsettings:
        clearArchiveTable = (defaultsettings["MythArchiveClearArchiveTable"] == '1')
    nicelevel = defaultsettings.get("JobQueueCPU", "0")

    # external commands
    path_mplex = [defaultsettings["MythArchiveMplexCmd"], os.path.split(defaultsettings["MythArchiveMplexCmd"])[1]]
    path_ffmpeg = [defaultsettings["MythArchiveFfmpegCmd"], os.path.split(defaultsettings["MythArchiveFfmpegCmd"])[1]]
    path_dvdauthor = [defaultsettings["MythArchiveDvdauthorCmd"], os.path.split(defaultsettings["MythArchiveDvdauthorCmd"])[1]]
    path_mkisofs = [defaultsettings["MythArchiveMkisofsCmd"], os.path.split(defaultsettings["MythArchiveMkisofsCmd"])[1]]
    path_growisofs = [defaultsettings["MythArchiveGrowisofsCmd"], os.path.split(defaultsettings["MythArchiveGrowisofsCmd"])[1]]
    path_tcrequant = [defaultsettings["MythArchiveTcrequantCmd"], os.path.split(defaultsettings["MythArchiveTcrequantCmd"])[1]]
    path_jpeg2yuv = [defaultsettings["MythArchiveJpeg2yuvCmd"], os.path.split(defaultsettings["MythArchiveJpeg2yuvCmd"])[1]]
    path_spumux = [defaultsettings["MythArchiveSpumuxCmd"], os.path.split(defaultsettings["MythArchiveSpumuxCmd"])[1]]
    path_mpeg2enc = [defaultsettings["MythArchiveMpeg2encCmd"], os.path.split(defaultsettings["MythArchiveMpeg2encCmd"])[1]]

    if nicelevel == '1':
        nicelevel = 10
    elif nicelevel == '2':
        nicelevel = 0
    else:
        nicelevel = 17

    nicelevel = os.nice(nicelevel)
    write( "Setting process priority to %s" % nicelevel)

    import errno

    try:
        # Attempt to create a lock file so any UI knows we are running.
        # Testing for and creation of the lock is one atomic operation.
        lckpath = os.path.join(logpath, "mythburn.lck")
        try:
            fd = os.open(lckpath, os.O_WRONLY | os.O_CREAT | os.O_EXCL)
            try:
                os.write(fd, "%d\n" % os.getpid())
                os.close(fd)
            except:
                os.remove(lckpath)
                raise
        except OSError, e:
            if e.errno == errno.EEXIST:
                write("Lock file exists -- already running???")
                sys.exit(1)
            else:
                fatalError("cannot create lockfile: %s" % e)
        # if we get here, we own the lock

        try:
            #Load XML input file from disk
            jobDOM = xml.dom.minidom.parse(jobfile)

            #Error out if its the wrong XML
            if jobDOM.documentElement.tagName != "mythburn":
                fatalError("Job file doesn't look right!")

            #process each job
            jobcount=0
            jobs=jobDOM.getElementsByTagName("job")
            for job in jobs:
                jobcount+=1
                write( "Processing Mythburn job number %s." % jobcount)

                #get any options from the job file if present
                options = job.getElementsByTagName("options")
                if options.length > 0:
                    getOptions(options)

                processJob(job)

            jobDOM.unlink()

            # clear the archiveitems table
            if clearArchiveTable == True:
                clearArchiveItems()

            saveSetting("MythArchiveLastRunStatus", "Success")
            saveSetting("MythArchiveLastRunEnd", time.strftime("%Y-%m-%d %H:%M:%S "))
            write("Finished processing jobs!!!")
        finally:
            # remove our lock file
            os.remove(lckpath)

            # make sure the files we created are read/writable by all 
            os.system("chmod -R a+rw-x+X %s" % defaultsettings["MythArchiveTempDir"])
    except SystemExit:
        write("Terminated")
    except:
        write('-'*60)
        traceback.print_exc(file=sys.stdout)
        if progresslog != "":
            traceback.print_exc(file=progressfile)
        write('-'*60)
        saveSetting("MythArchiveLastRunStatus", "Failed")
        saveSetting("MythArchiveLastRunEnd", time.strftime("%Y-%m-%d %H:%M:%S "))

if __name__ == "__main__":
    main()
