#ifndef PLAYERCOMMAND_H_
#define PLAYERCOMMAND_H_

class VideoPlayerCommand
{
  public:
    static VideoPlayerCommand PlayerFor(const class Metadata *item);
    static VideoPlayerCommand PlayerFor(const QString &filename);

  public:
    VideoPlayerCommand();
    ~VideoPlayerCommand();

    VideoPlayerCommand(const VideoPlayerCommand &other);
    VideoPlayerCommand &operator=(const VideoPlayerCommand &rhs);

    void Play() const;

    /// Returns the player command suitable for display to the user.
    QString GetCommandDisplayName() const;

  private:
    class VideoPlayerCommandPrivate *m_d;
};

#endif // PLAYERCOMMAND_H_
