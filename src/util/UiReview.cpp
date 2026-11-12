// SPDX-License-Identifier: MIT
#include "util/UiReview.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "common/enums/MessageContext.hpp"
#include "messages/Image.hpp"
#include "messages/Message.hpp"
#include "messages/MessageBuilder.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "singletons/WindowManager.hpp"
#include "widgets/buttons/Button.hpp"
#include "widgets/ChatterListWidget.hpp"
#include "widgets/dialogs/ChannelManagementDialog.hpp"
#include "widgets/dialogs/EmotePopup.hpp"
#include "widgets/dialogs/PollDialog.hpp"
#include "widgets/dialogs/PredictionDialog.hpp"
#include "widgets/dialogs/SettingsDialog.hpp"
#include "widgets/dialogs/UserInfoPopup.hpp"
#include "widgets/dialogs/VanityDialog.hpp"
#include "widgets/glass/GlassChrome.hpp"
#include "widgets/helper/ChannelView.hpp"
#include "widgets/helper/NotebookTab.hpp"
#include "widgets/Label.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"
#include "widgets/splits/SplitHeader.hpp"
#include "widgets/splits/SplitInput.hpp"
#include "widgets/Window.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCursor>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QTabWidget>
#include <QTimer>

#include <functional>

namespace chatterino {
class ApplicationReview final : public QObject
{
    Window *window_;
    Split *split_;
    QString output_;
    QJsonObject measurements_;
    bool valid_ = true;
    QImage on_;
    std::shared_ptr<TwitchChannel> toolsChannel_;
    void finish()
    {
        write("composer.json", measurements_);
        qInfo().noquote()
            << "GLASS_UI_REVIEW"
            << QJsonDocument(measurements_).toJson(QJsonDocument::Compact);
        qApp->exit(valid_ ? 0 : 1);
    }
    void toolsReview(int step = 0)
    {
        // No connection, room ID, token or mutating provider action. Exercise
        // the real dialogs with deterministic data and native input widgets.
        if (!toolsChannel_)
        {
            toolsChannel_ = std::make_shared<TwitchChannel>("community", true);
            toolsChannel_->setMod(true);
            for (const auto &name : {"mira", "Alex", "pixelpilot", "river"})
                toolsChannel_->addRecentChatter(name);
            getSettings()->chatterListDataMode = "local";
        }
        QWidget *tool = nullptr;
        QString name;
        switch (step)
        {
            case 0: {
                ChannelManagementState state;
                state.access = ChannelManagementAccess::Editor;
                state.metadata.title =
                    "Community night — games and conversation";
                state.metadata.language = "en";
                state.metadata.category = {
                    "509658", "Just Chatting", "Just Chatting", {}};
                state.metadata.tags = {"English", "Community"};
                auto *editor =
                    new ChannelManagementDialog(toolsChannel_, window_, state);
                valid_ &= editor->titleEdit_->isEnabled();
                editor->titleEdit_->setPlainText(QString(150, 'x'));
                valid_ &= editor->hasInvalidTitle() &&
                          !editor->saveButton_->isEnabled();
                editor->titleEdit_->setPlainText(
                    "Community night — welcome everyone!");
                editor->tagInput_->setText("Friendly");
                editor->addTagButton_->click();
                const auto update = editor->buildUpdate();
                const bool edited = update.title.has_value() && update.tags &&
                                    update.tags->contains("Friendly") &&
                                    editor->saveButton_->isEnabled();
                measurements_["editorValidationAndDraft"] = edited;
                valid_ &= edited;
                tool = editor;
                name = "stream-info";
                break;
            }
            case 1:
                tool = new ChatterListWidget(toolsChannel_, window_);
                tool->resize(440, 500);
                name = "chatters";
                break;
            case 2:
                tool = new VanityDialog(toolsChannel_, window_);
                name = "vanity";
                break;
            case 3:
            case 4: {
                auto *poll = new PollDialog(toolsChannel_.get(), window_);
                if (step == 4)
                {
                    TwitchChannel::PollEvent event;
                    event.id = "review-poll";
                    event.title = "What should we play next?";
                    event.status = "ACTIVE";
                    event.choices = {{"a", "Adventure", 32},
                                     {"b", "Puzzle", 18},
                                     {"c", "Racing", 12}};
                    event.totalVotes = 62;
                    event.durationSeconds = 300;
                    event.remainingDurationMilliseconds = 240000;
                    event.createdAt = QDateTime::currentDateTimeUtc();
                    poll->setPoll(event);
                }
                tool = poll;
                name = step == 3 ? "poll-create" : "poll-active";
                break;
            }
            case 5:
            case 6: {
                auto *prediction =
                    new PredictionDialog(toolsChannel_.get(), window_);
                if (step == 6)
                {
                    TwitchChannel::PredictionEvent event;
                    event.id = "review-prediction";
                    event.title = "Will we finish the challenge?";
                    event.status = "LOCKED";
                    event.outcomes = {{"a", "Yes", "BLUE", 3200, 32},
                                      {"b", "Not this time", "PINK", 1800, 18}};
                    event.createdAt = QDateTime::currentDateTimeUtc();
                    prediction->setPrediction(event);
                }
                tool = prediction;
                name = step == 5 ? "prediction-create" : "prediction-manage";
                break;
            }
            default:
                finish();
                return;
        }
        tool->show();
        tool->move(window_->mapToGlobal(QPoint(130, 70)));
        tool->raise();
        tool->activateWindow();
        later(650, [this, tool, name, step] {
            save(tool, name);
            if (step == 1)
            {
                auto *search = tool->findChild<QLineEdit *>();
                auto *list = tool->findChild<QListView *>();
                const int before = list ? list->model()->rowCount() : 0;
                if (search)
                    search->setText("mira");
                later(250, [this, tool, list, before, step] {
                    const bool filtered = list &&
                                          before > list->model()->rowCount() &&
                                          list->model()->rowCount() > 0;
                    valid_ &= filtered;
                    measurements_["chatterSearch"] = filtered;
                    save(tool, "chatters-search");
                    tool->close();
                    toolsReview(step + 1);
                });
                return;
            }
            if (step == 2)
            {
                vanityTabs(tool, 0);
                return;
            }
            if (step == 3 || step == 5)
            {
                auto fields = tool->findChildren<QLineEdit *>();
                int edited = 0;
                for (auto *field : fields)
                    if (field->isVisibleTo(tool) && field->isEnabled() &&
                        !field->isReadOnly())
                    {
                        field->setText(edited++ ? "Example option"
                                                : "Community challenge");
                    }
                valid_ &= edited >= 3;
                measurements_[name + "EditableFields"] = edited;
                save(tool, name + "-edited");
            }
            tool->close();
            toolsReview(step + 1);
        });
    }
    void vanityTabs(QWidget *tool, int index)
    {
        auto *tabs = tool->findChild<QTabWidget *>();
        if (!tabs || index >= tabs->count())
        {
            valid_ &= tabs && tabs->count() >= 5;
            measurements_["vanityTabs"] = tabs ? tabs->count() : 0;
            tool->close();
            toolsReview(3);
            return;
        }
        tabs->setCurrentIndex(index);
        later(250, [this, tool, index] {
            save(tool, QString("vanity-tab-%1").arg(index));
            vanityTabs(tool, index + 1);
        });
    }
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
    void popout()
    {
        // No setData(): this exercises the real usercard shell without
        // requesting a Twitch profile, logs, badges or moderation state.
        auto *card = new UserInfoPopup(false, split_);
        card->show();
        card->move(window_->mapToGlobal(QPoint(80, 80)));
        later(500, [this, card] {
            save(card, "usercard");
            if (glass::enabled())
            {
                const auto info = glass::diagnostics(card);
                valid_ &=
                    info["desktopBackdrop"].toBool() ||
                    (info["backdropSource"].toString() == "owner-window" &&
                     info["ownerCaptureValid"].toBool());
                // The empty history area overlaps the fixture's dense chat.
                // Sharp background glyphs previously passed the mapping check
                // while making the card unreadable. Measure their edge contrast
                // in an area with no foreground controls or card messages.
                const auto pixels = card->grab().toImage();
                const int left = int(pixels.width() * 0.35);
                const int right = int(pixels.width() * 0.95);
                const int top = int(pixels.height() * 0.55);
                const int bottom = int(pixels.height() * 0.85);
                double contrast = 0;
                int samples = 0;
                for (int y = top; y < bottom; ++y)
                    for (int x = left; x + 1 < right; ++x)
                    {
                        const auto a = pixels.pixel(x, y);
                        const auto b = pixels.pixel(x + 1, y);
                        contrast += qAbs(qRed(a) - qRed(b)) +
                                    qAbs(qGreen(a) - qGreen(b)) +
                                    qAbs(qBlue(a) - qBlue(b));
                        samples += 3;
                    }
                const double average = samples ? contrast / samples : 255.;
                measurements_["usercardBackdropEdgeContrast"] = average;
                valid_ &= average < 2.;
            }
            card->move(window_->mapToGlobal(QPoint(210, 100)));
            later(300, [this, card] {
                save(card, "usercard-moved");
                // Move completely beyond its owner. Old captured pixels and
                // custom wallpaper must not be retained in the glass shell.
                card->move(window_->mapToGlobal(
                    QPoint(window_->width() + 80, window_->height() + 80)));
                later(300, [this, card] {
                    if (glass::enabled())
                        valid_ &= !glass::diagnostics(card)["ownerCaptureValid"]
                                       .toBool();
                    card->move(window_->mapToGlobal(QPoint(80, 80)));
                    card->ui_.nameLabel->setText("Mira");
                    card->ui_.bioLabel->setText(
                        "Community member • offline review fixture");
                    card->ui_.latestMessages->setChannel(split_->getChannel());
                    card->ui_.noMessagesLabel->hide();
                    later(250, [this, card] {
                        save(card, "usercard-populated");
                        card->close();
                        toolsReview();
                    });
                });
            });
        });
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
                if (glass::enabled())
                    for (auto *w : settings->findChildren<QWidget *>())
                        valid_ &= !w->property("liquidGlassSurface").toBool();
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
                        popout();
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
            // Match the transparent message rows generated for a real
            // wallpaper theme, rather than leaving the old Dark row fills.
            getTheme()->messages.backgrounds.regular.setAlpha(0);
            getTheme()->messages.backgrounds.alternate.setAlpha(0);
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
