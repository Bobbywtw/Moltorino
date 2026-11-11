// SPDX-License-Identifier: MIT
#pragma once
class QWidget;
namespace chatterino::glass {
// Opens a preview only; cannot read account data or intercept chat input.
void openGlassQuickPreview(QWidget *parent);
}
