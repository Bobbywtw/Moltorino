// SPDX-License-Identifier: MIT
#pragma once
#include "widgets/glass/GlassSkinInterface.hpp"
namespace chatterino::glass {
// Register native control geometry with the integrated GPU material.
// False requests normal painting when the renderer is unavailable.
bool paint(QPainter &painter, const QRectF &rect, Surface surface,
           qreal emphasis, qreal scale);
}
