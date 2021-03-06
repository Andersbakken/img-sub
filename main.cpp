#include <QtGui>
#include <memory>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

int minSize = 10;
int verbose = 0;
float threshold = 0;
bool imageMagickFormat = false;

static inline QByteArray toString(const QRect &rect)
{
    char buf[1024];
    if (imageMagickFormat) {
        snprintf(buf, sizeof(buf), "%dx%d+%d+%d", rect.width(), rect.height(), rect.x(), rect.y());
    } else {
        snprintf(buf, sizeof(buf), "%d,%d+%dx%d", rect.x(), rect.y(), rect.width(), rect.height());
    }
    return buf;
}

struct Color
{
    Color(const QColor &col = QColor())
        : red(col.red()), green(col.green()), blue(col.blue()), alpha(col.alpha())
    {}

    QString toString() const
    {
        char buf[1024];
        snprintf(buf, sizeof(buf), "%02x%02x%02x%02x", red, green, blue, alpha);
        return QString::fromLocal8Bit(buf);
    }

    bool compare(const Color &other) const
    {
        const float r = powf(red - other.red, 2);
        const float g = powf(green - other.green, 2);
        const float b = powf(blue - other.blue, 2);

        float distance = sqrtf(r + g + b);
        const float alphaDistance = abs(alpha - other.alpha);
        // if (verbose >= 2) {
        //     fprintf(stderr, "%s to %s => %f/%f (%f) at %d,%d (%d,%d)\n",
        //             qPrintable(toString()),
        //             qPrintable(other.toString()),
        //             distance,
        //             alphaDistance,
        //             threshold,
        //             needleX, needleY,
        //             haystackX, haystackY);
        // }
        if (alphaDistance > distance)
            distance = alphaDistance;

        // static float highest = 0;
        const bool ret = distance <= threshold;
        // if (verbose >= 1 && ret && distance > highest) {
        //     fprintf(stderr, "Allowed %f distance for threshold %f at %d,%d (%d,%d) (%s vs %s)\n",
        //             distance, threshold,
        //             needleX, needleY,
        //             haystackX, haystackY,
        //             qPrintable(needle.toString()),
        //             qPrintable(haystack.toString()));
        //     highest = distance;
        // }

        // need to compare alpha
        return ret;
    }

    bool operator==(const Color &other) const
    {
        return compare(other);
    }
    bool operator!=(const Color &other) const
    {
        return !compare(other);
    }


    quint8 red, green, blue, alpha;
};

class Image;
class Chunk
{
public:
    Chunk(const std::shared_ptr<const Image> &i = std::shared_ptr<const Image>(), const QRect &r = QRect());
    Chunk(const Chunk &other)
        : mImage(other.mImage), mRect(other.mRect), mFlags(other.mFlags)
    {}
    int x() const { return mRect.x(); }
    int y() const { return mRect.y(); }
    int height() const { return mRect.height(); }
    int width() const { return mRect.width(); }
    QSize size() const { return mRect.size(); }
    QRect rect() const { return mRect; }

    inline Color color(int x, int y) const;
    bool compare(const Chunk &other) const;
    bool operator==(const Chunk &other) const { return compare(other); }
    bool operator!=(const Chunk &other) const { return !compare(other); }
    std::shared_ptr<const Image> image() const { return mImage; }
    bool isNull() const { return !mImage; }
    bool isValid() const { return mImage.get(); }
    void adopt(const Chunk &other);
    Qt::Alignment isAligned(const Chunk &other) const;
    enum Flag {
        None = 0x0,
        AllTransparent = 0x1
    };

    quint32 flags() const { return mFlags; }
    void save(const QString &fileName) const;
private:
    std::shared_ptr<const Image> mImage;
    QRect mRect;
    quint32 mFlags;
};

class Image : public std::enable_shared_from_this<Image>
{
public:
    static std::shared_ptr<Image> load(const QString &fileName)
    {
        QImage image(fileName);
        if (image.isNull())
            return std::shared_ptr<Image>();

        const int w = image.width();
        const int h = image.height();
        std::shared_ptr<Image> ret(new Image);
        ret->mFileName = fileName;
        ret->mSize = image.size();
        ret->mImage = image;
        ret->mColors.resize(w * h);
        for (int y=0; y<h; ++y) {
            for (int x=0; x<w; ++x) {
                ret->mColors[x + (y * w)] = QColor::fromRgba(image.pixel(x, y));
            }
        }
        return ret;
    }

    Chunk chunk(const QRect &rect) const { return Chunk(shared_from_this(), rect); }

    QVector<Chunk> chunks(int count, const QRegion &filter = QRegion()) const
    {
        if (count == 1) {
            Q_ASSERT(filter.isEmpty());
            QVector<Chunk> ret;
            ret.push_back(chunk(rect()));
            return ret;
        }
        Q_ASSERT(count > 1);
        const int w = width() / count;
        const int wextra = width() - (w * count);
        const int h = height() / count;
        if (w < minSize || h < minSize)
            return QVector<Chunk>();
        const int hextra = height() - (h * count);
        QVector<Chunk> ret(count * count);
        for (int y=0; y<count; ++y) {
            for (int x=0; x<count; ++x) {
                const QRect r(x * w,
                              y * h,
                              w + (x + 1 == count ? wextra : 0),
                              h + (y + 1 == count ? hextra : 0));
                // const QRect r(x * w, y * h, w, h);
                if (!filter.intersects(r)) {
                    ret[(y * count) + x] = chunk(r);
                }
            }
        }
        return ret;
    }

    Color color(int x, int y) const
    {
        Q_ASSERT(x >= 0);
        Q_ASSERT(y >= 0);
        Q_ASSERT(x < mSize.width());
        Q_ASSERT(y < mSize.height());
        return mColors.at((y * mSize.width()) + x);
    }

    QSize size() const { return mSize; }
    int width() const { return mSize.width(); }
    int height() const { return mSize.height(); }
    QString fileName() const { return mFileName; }
    QRect rect() const { return QRect(0, 0, width(), height()); }
    QImage image() const { return mImage; }
private:
    Image()
    {}

    QString mFileName;
    QImage mImage;
    QSize mSize;
    QVector<Color> mColors;

};

QDebug &operator<<(QDebug &debug, const Chunk &chunk)
{
    debug << "Chunk(" << chunk.image()->fileName() << chunk.rect() << ")";
    return debug;
}

Chunk::Chunk(const std::shared_ptr<const Image> &i, const QRect &r)
    : mImage(i), mRect(r), mFlags(0)
{
    if (mImage) {
        Q_ASSERT(!r.isNull());
        Q_ASSERT(r.bottom() < i->height());
        Q_ASSERT(r.right() < i->width()); // bottom/right are off-by-one
        mFlags |= AllTransparent;
        ([this]() {
            const int h = height();
            const int w = width();
            for (int y = 0; y<h; ++y) {
                for (int x = 0; x<w; ++x) {
                    if (color(x, y).alpha) {
                        mFlags &= ~AllTransparent;
                        return;
                    }
                }
            }
        })();
    }
    Q_ASSERT(!mImage.get() == r.isNull());
}

inline Color Chunk::color(int x, int y) const // x and y is in Chunk coordinates
{
    Q_ASSERT(mImage);
    Q_ASSERT(x < mRect.width());
    Q_ASSERT(y < mRect.height());
    return mImage->color(mRect.x() + x, mRect.y() + y);
}

bool Chunk::compare(const Chunk &other) const
{
    if ((mFlags & AllTransparent) && (other.mFlags & AllTransparent))
        return true;
    Q_ASSERT(other.mRect.size() == mRect.size());
    const int h = height();
    const int w = width();
    for (int y = 0; y<h; ++y) {
        for (int x = 0; x<w; ++x) {
            if (color(x, y) != other.color(x, y)) {
                return false;
            }
        }
    }
    return true;
}

Qt::Alignment Chunk::isAligned(const Chunk &other) const
{
    Qt::Alignment ret;
    if (height() == other.height()) {
        if (x() + width() == other.x()) {
            ret |= Qt::AlignRight;
        } else if (other.x() + other.width() == x()) {
            ret |= Qt::AlignLeft;
        }
    } else if (width() == other.width()) {
        if (y() + height() == other.y()) {
            ret |= Qt::AlignBottom;
        } else if (other.y() + other.height() == y()) {
            ret |= Qt::AlignTop;
        }
    }
    return ret;
}

void Chunk::save(const QString &fileName) const
{
    Q_ASSERT(mImage);
    const QImage image = mImage->image().copy(mRect);
    image.save(fileName);
}

void Chunk::adopt(const Chunk &other)
{
    Q_ASSERT(isAligned(other));
    QRegion region;
    region |= rect();
    region |= other.rect();
    mRect = region.boundingRect();
    Q_ASSERT(mRect.bottom() < mImage->height());
    Q_ASSERT(mRect.right() < mImage->width()); // bottom/right are off-by-one
}

void usage(FILE *f)
{
    fprintf(f,
            "img-diff [options...] imga imgb\n"
            "  --verbose|-v                       Be verbose\n"
            "  --range=[range]                    The range?\n"
            "  --min-size=[min-size]              The min-size?\n"
            "  --same                             Only display the areas that are identical\n"
            "  --no-join                          Don't join chunks\n"
            "  --dump-images                      Dump images to /tmp/img-sub_%%d[a|b].png\n"
            "  --imagemagick                      Douchy rects\n"
            "  --threshold=[threshold]            Set threshold value\n");
}

static void joinChunks(QVector<std::pair<Chunk, Chunk> > &chunks)
{
    bool modified;
    do {
        modified = false;
        for (int i=0; !modified && i<chunks.size(); ++i) {
            Chunk &chunk = chunks[i].first;
            Chunk &otherChunk = chunks[i].second;
            if (chunk.rect() == otherChunk.rect())
                continue;
            for (int j=i + 1; j<chunks.size(); ++j) {
                const Chunk &maybeChunk = chunks.at(j).first;
                if (maybeChunk.isNull())
                    continue;
                if ((chunk.flags() & Chunk::AllTransparent) != (maybeChunk.flags() & Chunk::AllTransparent))
                    continue;
                const Qt::Alignment aligned = chunk.isAligned(maybeChunk);
                if (verbose >= 2) {
                    qDebug() << "comparing" << chunk.rect() << maybeChunk.rect() << aligned
                             << otherChunk.rect() << chunks.at(j).second.rect()
                             << otherChunk.isAligned(chunks.at(j).second);
                }
                if (aligned && otherChunk.isAligned(chunks.at(j).second) == aligned) {
                    modified = true;
                    chunk.adopt(maybeChunk);
                    otherChunk.adopt(chunks.at(j).second);
                    chunks.remove(j, 1);
                    if (verbose)
                        qDebug() << "chunk" << i << chunk.rect() << "was joined with chunk" << j << maybeChunk.rect();
                    break;
                }
            }
        }
    } while (modified);
}

inline bool operator<(const QPoint &l, const QPoint &r)
{
    if (l.y() < r.y())
        return true;
    if (l.y() == r.y())
        return l.x() < r.x();
    return false;
}

int main(int argc, char **argv)
{
    QApplication a(argc, argv);
    QImage dump;
    std::shared_ptr<Image> oldImage, newImage;
    float threshold = 0;
    bool same = false;
    bool nojoin = false;
    bool dumpImages = false;
    int range = 2;
    for (int i=1; i<argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "--help" || arg == "-h") {
            usage(stdout);
            return 0;
        } else if (arg == "-v" || arg == "--verbose") {
            ++verbose;
        } else if (arg == "--imagemagick") {
            imageMagickFormat = true;
        } else if (arg == "--dump-images") {
            dumpImages = true;
        } else if (arg == "--no-join") {
            nojoin = true;
        } else if (arg.startsWith("--threshold=")) {
            bool ok;
            QString t = arg.mid(12);
            bool percent = false;
            if (t.endsWith("%")) {
                t.chop(1);
                percent = true;
            }
            threshold = t.toFloat(&ok);
            if (!ok || threshold < .0) {
                fprintf(stderr, "Invalid threshold (%s), must be positive float value\n",
                        qPrintable(arg.mid(12)));
                return 1;
            }
            if (percent) {
                threshold /= 100;
                threshold *= 256;
            }
            if (verbose)
                qDebug() << "threshold:" << threshold;
        } else if (arg.startsWith("--min-size=")) {
            bool ok;
            QString t = arg.mid(11);
            minSize = t.toInt(&ok);
            if (!ok || minSize <= 0) {
                fprintf(stderr, "Invalid --min-size (%s), must be positive integer value\n",
                        qPrintable(arg.mid(12)));
                return 1;
            }
            if (verbose)
                qDebug() << "min-size:" << minSize;
        } else if (arg == "--same") {
            same = true;
        } else if (arg.startsWith("--range=")) {
            bool ok;
            QString t = arg.mid(11);
            range = t.toInt(&ok);
            if (!ok || range <= 0) {
                fprintf(stderr, "Invalid --range (%s), must be positive integer value\n",
                        qPrintable(arg.mid(12)));
                return 1;
            }
            if (verbose)
                qDebug() << "range:" << range;
        } else if (!oldImage) {
            oldImage = Image::load(arg);
            if (!oldImage) {
                fprintf(stderr, "Failed to decode %s\n", qPrintable(arg));
                return 1;
            }
        } else if (!newImage) {
            newImage = Image::load(arg);
            if (!newImage) {
                fprintf(stderr, "Failed to decode %s\n", qPrintable(arg));
                return 1;
            }
        } else {
            usage(stderr);
            fprintf(stderr, "Too many args\n");
            return 1;
        }
    }
    if (!newImage) {
        usage(stderr);
        fprintf(stderr, "Not enough args\n");
        return 1;
    }

    if (oldImage->size() != newImage->size()) {
        fprintf(stderr, "Images have different sizes: %dx%d vs %dx%d\n",
                oldImage->width(), oldImage->height(),
                newImage->width(), newImage->height());
        return 1;
    }

    auto chunkIndexes = [range](int count, int idx) {
        QVector<int> indexes;
        const int y = (idx / count);
        const int x = idx % count;
        auto add = [&](int xadd, int yadd) {
            const int xx = x + xadd;
            if (xx < 0 || xx >= count)
                return;
            const int yy = y + yadd;
            if (yy < 0 || yy >= count)
                return;
            indexes.push_back((yy * count) + xx);
        };
        add(0, 0);
        for (int y=-range; y<=range; ++y) {
            for (int x=-range; x<=range; ++x) {
                if (x || y)
                    add(x, y);
            }
        }

        return indexes;
    };

    // qDebug() << chunkIndexes(10, 0);
    // return 0;
    QVector<std::pair<Chunk, Chunk> > matches;
    QRegion used;
    int count = 1;
    QPainter p;
    if (dumpImages) {
        dump = newImage->image();
        p.begin(&dump);
        p.setOpacity(.3);
        p.drawImage(0, 0, oldImage->image());
        p.setOpacity(.5);
        p.setPen(Qt::black);
    }
    QMap<QPoint, QString> texts;
    while (true) {
        const QVector<Chunk> newChunks = newImage->chunks(count, used);
        if (newChunks.isEmpty())
            break;
        const QVector<Chunk> oldChunks = oldImage->chunks(count);
        for (int i=0; i<newChunks.size(); ++i) {
            const Chunk &newChunk = newChunks.at(i);
            if (newChunk.isNull())
                continue;
            Q_ASSERT(newChunk.width() >= minSize && newChunk.height() >= minSize);

            for (int idx : chunkIndexes(count, i)) {
                const Chunk &oldChunk = oldChunks.at(idx);
                if (verbose >= 2) {
                    qDebug() << "comparing chunks" << newChunk << oldChunk;
                }

                if (oldChunk.size() == newChunk.size() && newChunk == oldChunk) {
                    used |= newChunk.rect();
                    matches.push_back(std::make_pair(newChunk, oldChunk));
                    break;
                }
            }
        }

        ++count;
    }
    if (!matches.isEmpty()) {
        if (!nojoin)
            joinChunks(matches);
        int i = 0;
        for (const auto &match : matches) {
            if (verbose) {
                QString str;
                QDebug dbg(&str);
                dbg << "Match" << i << toString(match.first.rect()) << (match.first.flags() & Chunk::AllTransparent ? "transparent" : "");
                if (match.first.rect() == match.second.rect()) {
                    dbg << "SAME";
                } else {
                    dbg << "FOUND AT" << toString(match.second.rect());
                }
                fprintf(stderr, "%s\n", qPrintable(str));
            }
            if (dumpImages) {
                /* char buf[1024]; */
                /* snprintf(buf, sizeof(buf), "/tmp/img-sub_%04d_%s_a%s.png", i, toString(match.first.rect()).constData(), */
                /*          match.first.flags() & Chunk::AllTransparent ? "_alpha" : ""); */
                /* match.first.save(buf); */
                /* if (verbose) */
                /*     fprintf(stderr, "Dumped %s\n", buf); */
                /* snprintf(buf, sizeof(buf), "/tmp/img-sub_%04d_%s_b%s.png", i, toString(match.second.rect()).constData(), */
                /*          match.second.flags() & Chunk::AllTransparent ? "_alpha" : ""); */
                /* match.second.save(buf); */
                /* if (verbose) */
                /*     fprintf(stderr, "Dumped %s\n", buf); */

                if (match.first.rect() == match.second.rect()) {
                    // p.fillRect(match.first.rect(), Qt::green);
                    // p.drawRect(match.first.rect());
                    // p.drawText(chunk.rect().topLeft() + QPoint(2, 10),
                    //            toString(chunk.rect()) + "\ndid not move");
                } else {
                    p.fillRect(match.first.rect(), Qt::yellow);
                    p.drawRect(match.first.rect());
                    texts[match.first.rect().topLeft() + QPoint(2, 10)] = toString(match.second.rect()) + " moved to " + toString(match.first.rect());
                }

            }
            if (match.first.rect() != match.second.rect()) {
                if (!same) {
                    printf("%s %s\n", toString(match.second.rect()).constData(), toString(match.first.rect()).constData());
                }
            } else if (same) {
                printf("%s\n", toString(match.first.rect()).constData());
            }
            ++i;
        }
        QRegion all;
        all |= oldImage->rect();
        all -= used;
        if (dumpImages) {
            for (const QRect &r : all.rects()) {
                p.fillRect(r, Qt::green);
                p.drawRect(r);
            }
        }
        if (!same) {
            for (const QRect &rect : all.rects()) {
                printf("%s\n", toString(rect).constData());
            }
        }
    } else if (!same) {
        printf("%s\n", toString(oldImage->rect()).constData());
    }
    if (dumpImages) {
        p.setOpacity(1);
        QFont f;
        f.setPixelSize(8);
        p.setFont(f);
        p.setPen(Qt::blue);
        // qDebug() << texts;
        for (QMap<QPoint, QString>::const_iterator it = texts.begin(); it != texts.end(); ++it) {
            p.drawText(it.key(), it.value());
        }
        p.end();
        dump.save("/tmp/img-sub.png");
    }

    return 0;
}
