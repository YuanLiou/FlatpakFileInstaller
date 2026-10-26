// SPDX-License-Identifier: Apache-2.0
#include "filedroparea.h"

#include "packagefile.h"

#include <KUrlMimeData>

#include <QMimeData>

#include <algorithm>

FileDropArea::FileDropArea(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemAcceptsDrops);
}

bool FileDropArea::containsDrag() const
{
    return m_containsDrag;
}

bool FileDropArea::offersInstallableFile(const QMimeData *mimeData)
{
    // Only peeks at the file names; the portal is asked on drop.
    const QList<QUrl> urls = mimeData->urls();
    return std::ranges::any_of(urls, [](const QUrl &url) {
        return PackageFile::isInstallableFileName(url.fileName());
    });
}

void FileDropArea::dragEnterEvent(QDragEnterEvent *event)
{
    if (!offersInstallableFile(event->mimeData())) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
    setContainsDrag(true);
}

void FileDropArea::dragMoveEvent(QDragMoveEvent *event)
{
    if (m_containsDrag) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void FileDropArea::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event)
    setContainsDrag(false);
}

void FileDropArea::dropEvent(QDropEvent *event)
{
    setContainsDrag(false);
    const QList<QUrl> urls = KUrlMimeData::urlsFromMimeData(event->mimeData(), KUrlMimeData::PreferLocalUrls);
    const auto file = std::ranges::find_if(urls, [](const QUrl &url) {
        return url.isLocalFile() && PackageFile::isInstallableFileName(url.fileName());
    });
    if (file == urls.end()) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
    Q_EMIT fileDropped(*file);
}

void FileDropArea::setContainsDrag(bool containsDrag)
{
    if (m_containsDrag != containsDrag) {
        m_containsDrag = containsDrag;
        Q_EMIT containsDragChanged();
    }
}
