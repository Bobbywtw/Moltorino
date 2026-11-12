// SPDX-License-Identifier: MIT
#pragma once
#include <QJsonObject>
#include <QRectF>
class QWidget;
namespace chatterino::glass {
// GPU chrome with compositor-owned desktop acrylic on supported Windows.
// No screen capture or account storage access.
void installChrome();
// Explicitly tear down QQuickWidget scene graphs before QApplication quits.
void shutdownChrome();
bool enabled();
bool available(const QWidget *widget);
bool surface(QWidget *widget, const QRectF &rect, const char *kind,
             qreal emphasis = 0.0);
void reviewOptics(QWidget *window, bool enabled);
QJsonObject diagnostics(QWidget *window);
}  // namespace chatterino::glass
