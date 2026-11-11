// SPDX-License-Identifier: MIT
#pragma once
#include <QJsonObject>
#include <QRectF>
class QWidget;
namespace chatterino::glass {
// Application-owned backdrop only; never reads the desktop or account storage.
void installChrome();
bool enabled();
bool available(const QWidget *widget);
bool surface(QWidget *widget, const QRectF &rect, const char *kind,
             qreal emphasis = 0.0);
void reviewOptics(QWidget *window, bool enabled);
QJsonObject diagnostics(QWidget *window);
}  // namespace chatterino::glass
