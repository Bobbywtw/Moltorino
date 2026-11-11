// SPDX-License-Identifier: MIT
#include "widgets/glass/GlassQuickPreview.hpp"
#include <QColor>
#include <QDialog>
#include <QLabel>
#include <QMessageBox>
#include <QQuickView>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace chatterino::glass {
void openGlassQuickPreview(QWidget *parent)
{
    auto *dialog = new QDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QStringLiteral("Moltorino GPU Liquid Glass Preview"));
    dialog->resize(900, 560);

    auto *layout = new QVBoxLayout(dialog);
    auto *note = new QLabel(
        QStringLiteral("Test preview only: existing Moltorino chats, "
                       "emotes, settings and moderation stay unchanged."),
        dialog);
    note->setWordWrap(true);
    layout->addWidget(note);

    auto *quickView = new QQuickView();
    quickView->setColor(QColor(QStringLiteral("#18191a")));
    quickView->setResizeMode(QQuickView::SizeRootObjectToView);
    quickView->setSource(
        QUrl(QStringLiteral("qrc:/moltorino-glass/GlassIntegrationPreview.qml")));

    if (quickView->status() == QQuickView::Error)
    {
        delete quickView;
        delete dialog;
        QMessageBox::warning(
            parent, QStringLiteral("Moltorino GPU Preview"),
            QStringLiteral("Qt Quick preview could not load. "
                           "The normal Moltorino interface is unchanged."));
        return;
    }
    auto *viewport = QWidget::createWindowContainer(quickView, dialog);
    viewport->setMinimumSize(480, 270);
    layout->addWidget(viewport, 1);
    dialog->show();
}
}
