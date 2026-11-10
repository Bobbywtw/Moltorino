// SPDX-License-Identifier: MIT
#include "util/UiReview.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "common/enums/MessageContext.hpp"
#include "messages/Image.hpp"
#include "messages/Message.hpp"
#include "messages/MessageBuilder.hpp"
#include "singletons/WindowManager.hpp"
#include "widgets/helper/NotebookTab.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"
#include "widgets/splits/SplitInput.hpp"
#include "widgets/Window.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
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
        QTimer::singleShot(700, qApp, [output, &window] {
            QDir().mkpath(output);
            const auto saved = window.grab().save(output + "/chat.png");
            QMenu menu;
            menu.addAction("Open channel");
            menu.addAction("Copy channel address");
            menu.addSeparator();
            menu.addAction("Channel settings");
            menu.addAction("Unavailable action")->setEnabled(false);
            menu.ensurePolished();
            menu.resize(menu.sizeHint());
            const auto menuSaved = menu.grab().save(output + "/menu.png");
            qApp->exit(saved && menuSaved ? 0 : 1);
        });
    });
}
}  // namespace chatterino
