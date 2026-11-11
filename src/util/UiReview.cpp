// SPDX-License-Identifier: MIT
#include "util/UiReview.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "common/enums/MessageContext.hpp"
#include "messages/Image.hpp"
#include "messages/Message.hpp"
#include "messages/MessageBuilder.hpp"
#include "singletons/WindowManager.hpp"
#include "util/NativeGlass.hpp"
#include "widgets/buttons/SvgButton.hpp"
#include "widgets/helper/ChannelView.hpp"
#include "widgets/helper/NotebookTab.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"
#include "widgets/splits/SplitInput.hpp"
#include "widgets/Window.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QPainter>
#include <QTimer>

namespace chatterino {
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
        QTimer::singleShot(700, qApp, [output, &window, split] {
            QDir().mkpath(output);
            const auto saved = window.grab().save(output + "/chat.png");
            auto &input = split->getInput();
            const auto singleHeight = input.height();
            const auto scale = input.scale();
            QJsonArray buttons;
            for (auto *button : input.findChildren<SvgButton *>())
            {
                if (!button->isVisible())
                {
                    continue;
                }
                buttons.append(QJsonObject{
                    {"hitWidth", button->width() / scale},
                    {"hitHeight", button->height() / scale},
                    {"glyphWidth",
                     button->width() / scale - 2 * button->padding().width()}});
            }
            QJsonObject state{
                {"nativeSupported", nativeglass::supported()},
                {"nativeActive", nativeglass::active(&window)},
                {"effectsAllowed", nativeglass::effectsAllowed()},
                {"dwmResult",
                 window.property("_moltorino_dwmResult").toString()},
                {"dwmType", window.property("_moltorino_dwmType").toInt()},
                {"singleLineComposerHeight", singleHeight / scale},
                {"buttons", buttons}};
            input.setInputText("First line\nSecond line\nThird line");
            QTimer::singleShot(
                250, qApp,
                [output, &window, split, singleHeight, state, saved]() mutable {
                    state.insert(
                        "multilineComposerHeight",
                        split->getInput().height() / split->getInput().scale());
                    const auto grew = split->getInput().height() > singleHeight;
                    split->getInput().setInputText("Thanks for the stream!");
                    auto *menu = new QMenu(&window);
                    menu->addAction("Open channel");
                    menu->addAction("Copy channel address");
                    menu->addSeparator();
                    menu->addAction("Channel settings");
                    menu->addAction("Unavailable action")->setEnabled(false);
                    menu->popup(
                        split->getChannelView().mapToGlobal(QPoint(80, 90)));
                    menu->setActiveAction(menu->actions()[1]);
                    QTimer::singleShot(
                        250, qApp, [output, menu, state, saved, grew] {
                            const auto menuSaved =
                                menu->grab().save(output + "/menu.png");
                            QFile file(output + "/material-state.json");
                            const auto written =
                                file.open(QIODevice::WriteOnly) &&
                                file.write(QJsonDocument(state).toJson()) > 0;
                            file.close();
                            menu->close();
                            menu->deleteLater();
                            qApp->exit(
                                saved && menuSaved && written && grew ? 0 : 1);
                        });
                });
        });
    });
}
}  // namespace chatterino
