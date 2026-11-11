// SPDX-License-Identifier: MIT
#pragma once
#include "widgets/glass/GlassSkinInterface.hpp"
namespace chatterino::glass {
// False means the trusted bundled plugin is unavailable: use normal painting.
bool paint(QPainter &painter, const QRectF &rect, Surface surface,
           qreal emphasis, qreal scale);
}
