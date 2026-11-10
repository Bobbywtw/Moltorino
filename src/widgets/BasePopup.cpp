// SPDX-FileCopyrightText: 2019 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/BasePopup.hpp"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QKeyEvent>

namespace chatterino {

BasePopup::BasePopup(FlagsEnum<Flags> _flags, QWidget *parent)
    : BaseWindow(_flags | Dialog, parent)
    , openingAnimation_(this, "windowOpacity")
{
    this->openingAnimation_.setDuration(110);
    this->openingAnimation_.setEasingCurve(QEasingCurve::OutCubic);
}

void BasePopup::showEvent(QShowEvent *event)
{
    BaseWindow::showEvent(event);
    this->openingAnimation_.stop();
    this->openingAnimation_.setStartValue(0.96);
    this->openingAnimation_.setEndValue(1.0);
    this->openingAnimation_.start();
}

void BasePopup::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape)
    {
        this->close();
        return;
    }

    BaseWindow::keyPressEvent(e);
}

bool BasePopup::handleEscape(QKeyEvent *e, QDialogButtonBox *buttonBox)
{
    assert(buttonBox != nullptr);

    if (e->key() == Qt::Key_Escape)
    {
        auto buttons = buttonBox->buttons();
        for (auto *button : buttons)
        {
            if (auto role = buttonBox->buttonRole(button);
                role == QDialogButtonBox::ButtonRole::RejectRole)
            {
                button->click();
                return true;
            }
        }
    }

    return false;
}

bool BasePopup::handleEnter(QKeyEvent *e, QDialogButtonBox *buttonBox)
{
    assert(buttonBox != nullptr);

    if (!e->modifiers() ||
        (e->modifiers() & Qt::KeypadModifier && e->key() == Qt::Key_Enter))
    {
        switch (e->key())
        {
            case Qt::Key_Enter:
            case Qt::Key_Return: {
                auto buttons = buttonBox->buttons();
                QAbstractButton *acceptButton = nullptr;
                for (auto *button : buttons)
                {
                    if (button->hasFocus())
                    {
                        button->click();
                        return true;
                    }

                    if (auto role = buttonBox->buttonRole(button);
                        role == QDialogButtonBox::ButtonRole::AcceptRole)
                    {
                        acceptButton = button;
                    }
                }

                if (acceptButton != nullptr)
                {
                    acceptButton->click();
                    return true;
                }
            }
            break;
        }
    }

    return false;
}

}
