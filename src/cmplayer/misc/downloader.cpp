#include "downloader.hpp"

auto reg_downloader() -> void
{
    qmlRegisterType<Downloader>();
}

struct Downloader::Data {
    Downloader *p = nullptr;
    QUrl url;
    QNetworkAccessManager *nam = nullptr;
    bool running = false, canceled = false;
    QByteArray data;
    qint64 written = -1, total = -1;
    qreal rate = -1.0;
    QNetworkReply *reply = nullptr;
};

Downloader::Downloader(QObject *parent)
    : QObject(parent)
    , d(new Data)
{
    d->p = this;
    d->nam = new QNetworkAccessManager;
}

Downloader::~Downloader()
{
    if (d->reply)
        cancel();
    delete d->nam;
    delete d;
}

auto Downloader::writtenSize() const -> qint64
{
    return d->written;
}

auto Downloader::totalSize() const -> qint64
{
    return d->total;
}

auto Downloader::isCanceled() const -> bool
{
    return d->canceled;
}

auto Downloader::cancel() -> void
{
    if (d->reply) {
        d->canceled = true;
        d->reply->abort();
        if (d->reply)
            _Delete(d->reply);
        if (_Change(d->running, false))
            emit runningChanged();
        emit canceledChanged();
    }
}

auto Downloader::start(const QUrl &url) -> bool
{
    if (d->running)
        return false;
    if (_Change(d->url, url))
        emit urlChanged();
    d->running = true;
    if (_Change(d->canceled, false))
        emit canceledChanged();
    emit started();
    emit runningChanged();
    progress(-1, -1);
    d->reply = d->nam->get(QNetworkRequest(url));
    connect(d->reply, &QNetworkReply::downloadProgress,
            this, &Downloader::progress);
    connect(d->reply, &QNetworkReply::finished, [this] () {
        d->data = d->reply->readAll();
        d->running = false;
        emit finished();
        emit runningChanged();
        d->reply->deleteLater();
        d->reply = nullptr;
    });
    return true;
}

auto Downloader::progress(qint64 written, qint64 total) -> void
{
    if (_Change(d->total, total))
        emit totalSizeChanged(total);
    if (_Change(d->written, written))
        emit writtenSizeChanged(written);
    qreal rate = -1.0;
    if (total > 0)
        rate = written/(double)total;
    if (_Change(d->rate, rate))
        emit rateChanged();
    emit progressed(written, total);
}

auto Downloader::url() const -> QUrl
{
    return d->url;
}

auto Downloader::isRunning() const -> bool
{
    return d->running;
}

auto Downloader::rate() const -> qreal
{
    return d->rate;
}

auto Downloader::takeData() -> QByteArray
{
    auto data = d->data;
    d->data = QByteArray();
    return data;
}

auto Downloader::data() const -> QByteArray
{
    return d->data;
}

//void Downloader::downloadFinished(QNetworkReply *reply)
//{
//    QUrl url = reply->url();
//    if (reply->error()) {
//        fprintf(stderr, "Download of %s failed: %s\n",
//                url.toEncoded().constData(),
//                qPrintable(reply->errorString()));
//    } else {
//        QString filename = saveFileName(url);
//        if (saveToDisk(filename, reply))
//            printf("Download of %s succeeded (saved to %s)\n",
//                   url.toEncoded().constData(), qPrintable(filename));
//    }

//    d->currentDownloads.removeAll(reply);
//    reply->deleteLater();

//    if (d->currentDownloads.isEmpty())
//        // all downloads finished
//        QCoreApplication::instance()->quit();
//}


//QString Downloader::saveFileName(const QUrl &url)
//{
//    QString path = url.path();
//    QString basename = QFileInfo(path).fileName();

//    if (basename.isEmpty())
//        basename = "download";

//    if (QFile::exists(basename)) {
//        // already exists, don't overwrite
//        int i = 0;
//        basename += '.';
//        while (QFile::exists(basename + QString::number(i)))
//            ++i;

//        basename += QString::number(i);
//    }

//    return basename;
//}

//bool Downloader::saveToDisk(const QString &filename, QIODevice *data)
//{
//    QFile file(filename);
//    if (!file.open(QIODevice::WriteOnly)) {
//        fprintf(stderr, "Could not open %s for writing: %s\n",
//                qPrintable(filename),
//                qPrintable(file.errorString()));
//        return false;
//    }

//    file.write(data->readAll());
//    file.close();

//    return true;
//}

//void Downloader::execute()
//{
//    QStringList args = QCoreApplication::instance()->arguments();
//    args.takeFirst();           // skip the first argument, which is the program's name
//    if (args.isEmpty()) {
//        printf("Qt Download example - downloads all URLs in parallel\n"
//               "Usage: download url1 [url2... urlN]\n"
//               "\n"
//               "Downloads the URLs passed in the command-line to the local directory\n"
//               "If the target file already exists, a .0, .1, .2, etc. is appended to\n"
//               "differentiate.\n");
//        QCoreApplication::instance()->quit();
//        return;
//    }

//    foreach (QString arg, args) {
//        QUrl url = QUrl::fromEncoded(arg.toLocal8Bit());
//        doDownload(url);
//    }
//}

