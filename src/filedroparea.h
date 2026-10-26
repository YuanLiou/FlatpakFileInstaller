// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QQuickItem>
#include <QUrl>
#include <QtQmlIntegration>

class QMimeData;

// Accepts a .flatpak or .flatpakref file dragged in from a file manager or
// browser. Unlike QML's DropArea it decodes the drop with KUrlMimeData,
// which goes through the FileTransfer portal inside the Flatpak sandbox, so
// we get a path we are allowed to read.
class FileDropArea : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool containsDrag READ containsDrag NOTIFY containsDragChanged)

public:
    explicit FileDropArea(QQuickItem *parent = nullptr);

    bool containsDrag() const;

Q_SIGNALS:
    void containsDragChanged();
    void fileDropped(const QUrl &file);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    static bool offersInstallableFile(const QMimeData *mimeData);
    void setContainsDrag(bool containsDrag);

    bool m_containsDrag = false;
};
