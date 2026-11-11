// SPDX-License-Identifier: MIT
#include "util/UiReview.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "common/enums/MessageContext.hpp"
#include "messages/Image.hpp"
#include "messages/Message.hpp"
#include "messages/MessageBuilder.hpp"
#include "singletons/Theme.hpp"
#include "singletons/WindowManager.hpp"
#include "widgets/buttons/Button.hpp"
#include "widgets/dialogs/EmotePopup.hpp"
#include "widgets/dialogs/SettingsDialog.hpp"
#include "widgets/glass/GlassChrome.hpp"
#include "widgets/helper/NotebookTab.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"
#include "widgets/splits/SplitHeader.hpp"
#include "widgets/splits/SplitInput.hpp"
#include "widgets/Window.hpp"

#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

#include <functional>

namespace chatterino {
namespace {
class ApplicationReview final : public QObject
{
    Window *window_;
    Split *split_;
    QString output_;
    QJsonObject measurements_;
    bool valid_ = true;
    QImage on_;
    void later(int ms, std::function<void()> next)
    {
        QTimer::singleShot(ms, this, std::move(next));
    }
    void save(QWidget *widget, const QString &name)
    {
        valid_ &= widget && widget->grab().save(output_ + "/" + name + ".png");
        if (widget && glass::enabled())
        {
            const auto info = glass::diagnostics(widget);
            valid_ &= info["available"].toBool() && info["qmlReady"].toBool() &&
                      info["graphicsApi"].toInt() >= 3;
            write(name + "-gpu.json", info);
        }
    }
    void write(const QString &name, const QJsonObject &object)
    {
        QFile file(output_ + "/" + name);
        valid_ &= file.open(QIODevice::WriteOnly) &&
                  file.write(QJsonDocument(object).toJson()) > 0;
    }
    void menu()
    {
        auto *header = split_->findChild<SplitHeader *>();
        if (!header)
        {
            qApp->exit(1);
            return;
        }
        const auto point = QPointF(30, header->height() / 2);
        QMouseEvent press(QEvent::MouseButtonPress, point,
                          header->mapToGlobal(point.toPoint()), Qt::RightButton,
                          Qt::RightButton, Qt::NoModifier);
        QApplication::sendEvent(header, &press);
        later(350, [this] {
            auto *popup =
                qobject_cast<QMenu *>(QApplication::activePopupWidget());
            if (!popup)
            {
                qApp->exit(1);
                return;
            }
            save(popup, "menu");
            popup->close();
            SettingsDialog::showDialog();
            later(800, [this] {
                QWidget *settings = nullptr;
                for (auto *w : QApplication::topLevelWidgets())
                    if (dynamic_cast<SettingsDialog *>(w) && w->isVisible())
                        settings = w;
                if (!settings)
                {
                    qApp->exit(1);
                    return;
                }
                save(settings, "settings");
                settings->hide();
                auto *emotes = new EmotePopup(window_);
                emotes->loadChannel(std::make_shared<Channel>(
                    "community", Channel::Type::None));
                emotes->resize(660, 480);
                emotes->show();
                later(700, [this, emotes] {
                    save(emotes, "emotes");
                    emotes->hide();
                    emotes->deleteLater();
                    auto *dialog = new QMessageBox(
                        QMessageBox::Information, "Moltorino",
                        "Native dialog controls remain functional.",
                        QMessageBox::Ok, window_);
                    dialog->setAttribute(Qt::WA_DeleteOnClose);
                    dialog->show();
                    later(400, [this, dialog] {
                        save(dialog, "dialog");
                        dialog->close();
                        const auto text = split_->getInput().getInputText() ==
                                          "Thanks for the stream!";
                        measurements_["textPreserved"] = text;
                        valid_ &= text;
                        write("composer.json", measurements_);
                        qApp->exit(valid_ ? 0 : 1);
                    });
                });
            });
        });
    }

    void interactions(std::function<void()> next)
    {
        Button *target = nullptr;
        for (auto *button : split_->getInput().findChildren<Button *>())
            if (button->isVisible() && button->enabled() &&
                button->width() <= 36 && button->height() <= 36 &&
                !button->menu())
            {
                target = button;
                break;
            }
        if (!target)
        {
            valid_ = false;
            next();
            return;
        }
        const auto point = QPointF(target->rect().center());
        QCursor::setPos(target->mapToGlobal(point.toPoint()));
        QEnterEvent enter(point, target->mapTo(window_, point),
                          target->mapToGlobal(point));
        QApplication::sendEvent(target, &enter);
        later(180, [this, target, point, next] {
            save(window_, "chat-hover");
            QMouseEvent press(QEvent::MouseButtonPress, point,
                              target->mapToGlobal(point), Qt::LeftButton,
                              Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(target, &press);
            later(55, [this, target, next] {
                save(window_, "chat-pressed");
                // Release outside the real control: exercise its native pressed
                // state without invoking a chat, moderation or navigation action.
                const QPointF outside(-10, -10);
                QMouseEvent release(QEvent::MouseButtonRelease, outside,
                                    target->mapToGlobal(outside),
                                    Qt::LeftButton, Qt::NoButton,
                                    Qt::NoModifier);
                QApplication::sendEvent(target, &release);
                QEvent leave(QEvent::Leave);
                QApplication::sendEvent(target, &leave);
                QCursor::setPos(window_->mapToGlobal(QPoint(250, 250)));
                later(200, std::move(next));
            });
        });
    }

public:
    ApplicationReview(Window *window, Split *split, QString output)
        : QObject(qApp)
        , window_(window)
        , split_(split)
        , output_(std::move(output))
    {
        QDir().mkpath(output_);
        later(1300, [this] {
            save(window_, "chat");
            on_ = window_->grab().toImage();
            const auto info = glass::diagnostics(window_);
            write("chrome.json", info);
            if (glass::enabled())
            {
                QStringList kinds;
                for (const auto &item : info["surfaces"].toArray())
                    kinds << item.toObject()["kind"].toString();
                for (const auto &kind : {"tab", "header", "composer", "button"})
                    valid_ &= kinds.contains(kind);
            }
            interactions([this] {
                measurements_["singleLineHeight"] = split_->getInput().height();
                split_->getInput().setInputText(
                    "First line\nSecond line\nThird line");
                later(200, [this] {
                    measurements_["multilineHeight"] =
                        split_->getInput().height();
                    split_->getInput().setInputText("Thanks for the stream!");
                    window_->resize(400, 680);
                    later(300, [this] {
                        measurements_["narrowHeight"] =
                            split_->getInput().height();
                        save(window_, "chat-resized");
                        window_->resize(1180, 760);
                        later(300, [this] {
                            measurements_["restoredHeight"] =
                                split_->getInput().height();
                            valid_ &= measurements_["multilineHeight"].toInt() >
                                      measurements_["singleLineHeight"].toInt();
                            valid_ &=
                                qAbs(measurements_["restoredHeight"].toInt() -
                                     measurements_["singleLineHeight"]
                                         .toInt()) <= 2;
                            if (!glass::enabled())
                            {
                                menu();
                                return;
                            }
                            on_ = window_->grab().toImage();
                            glass::reviewOptics(window_, false);
                            later(200, [this] {
                                save(window_, "chat-optics-off");
                                auto off = window_->grab().toImage();
                                int changed = 0;
                                for (int y = 0; y < off.height(); ++y)
                                    for (int x = 0; x < off.width(); ++x)
                                        if (off.pixel(x, y) != on_.pixel(x, y))
                                            ++changed;
                                measurements_["opticsChangedPixels"] = changed;
                                valid_ &= changed > 500;
                                glass::reviewOptics(window_, true);
                                later(200, [this] {
                                    menu();
                                });
                            });
                        });
                    });
                });
            });
        });
    }
};
}  // namespace
void scheduleUiReview()
{
    const auto output = qEnvironmentVariable("MOLTORINO_UI_REVIEW_DIR");
    if (output.isEmpty())
    {
        return;
    }
    // Use the real layout/renderer and disconnected, unlogged channels. Never
    // synthesize provider traffic or send messages during the review.
    QTimer::singleShot(500, qApp, [output] {
        if (!qEnvironmentVariableIsEmpty("MOLTORINO_UI_REVIEW_WALLPAPER"))
        {
            // Only the disconnected CI fixture uses this existing bundled image.
            // The user's settings file is never changed.
            getTheme()->customization.wallpaperSource =
                qEnvironmentVariable("MOLTORINO_UI_REVIEW_WALLPAPER");
            getTheme()->customization.wallpaperOverlayOpacity = 42;
            getTheme()->updated.invoke();
        }
        auto &window = getApp()->getWindows()->getMainWindow();
        window.resize(1180, 760);
        auto &notebook = window.getNotebook();
        auto *page = notebook.getOrAddSelectedPage();
        page->getTab()->setCustomTitle("Community");
        auto *split = page->getSelectedSplit();
        if (!split)
        {
            split = page->appendNewSplit(false);
        }
        auto channel =
            std::make_shared<Channel>("community", Channel::Type::None);
        split->setChannel(IndirectChannel(channel));
        const QStringList names{"mira", "Alex", "pixelpilot", "river"};
        const QStringList texts{
            "Good evening everyone!", "The new layout feels really clear.",
            "Longer conversations should wrap naturally, with room for names, "
            "badges and emotes without turning every message into a separate "
            "card.",
            "@mira I agree — keeping the chat easy to read matters most."};
        for (int i = 0; i < 22; ++i)
        {
            MessageBuilder builder;
            builder->id = QString::number(i);
            builder->displayName = names[i % names.size()];
            builder->messageText = texts[i % texts.size()];
            builder.emplace<TimestampElement>(QTime(20, 14, i));
            if (i % 4 == 0)
            {
                QPixmap badge(18, 18);
                badge.fill(QColor("#438562"));
                QPainter painter(&badge);
                painter.setPen(Qt::white);
                painter.drawText(badge.rect(), Qt::AlignCenter, "★");
                painter.end();
                builder.emplace<ImageElement>(
                    Image::fromResourcePixmap(badge),
                    MessageElementFlag::BadgeSubscription);
            }
            if (i == 18)
            {
                builder->flags.set(MessageFlag::ReplyMessage);
                builder.emplace<TextElement>(
                    "Replying to mira: Good evening everyone!",
                    MessageElementFlag::Text, MessageColor::System);
                builder.emplace<LinebreakElement>(MessageElementFlag::Text);
            }
            builder.emplace<TextElement>(
                builder->displayName + ":", MessageElementFlag::Username,
                MessageColor(QColor(i % 2 ? "#cfaa78" : "#a99ad8")),
                FontStyle::ChatUsername);
            builder.emplace<TextElement>(builder->messageText,
                                         MessageElementFlag::Text);
            if (i == 19)
            {
                builder->flags.set(MessageFlag::Highlighted);
                builder->highlightColor =
                    std::make_shared<QColor>(116, 91, 45, 55);
            }
            if (i == 20)
            {
                builder->flags.set(MessageFlag::Disabled);
            }
            channel->addMessage(builder.release(), MessageContext::Original);
        }
        channel->addMessage(
            makeSystemMessage(
                "Stream title updated • Welcome to the community"),
            MessageContext::Original);
        split->getInput().setInputText("Thanks for the stream!");
        auto *other = notebook.addPage(false);
        other->getTab()->setCustomTitle("Mentions");
        other->getTab()->setHighlightState(HighlightState::Highlighted);
        notebook.addPage(false)->getTab()->setCustomTitle("Moderation");
        notebook.select(page);
        getApp()->getWindows()->forceLayoutChannelViews();
        window.show();
        new ApplicationReview(&window, split, output);
    });
}
}  // namespace chatterino
