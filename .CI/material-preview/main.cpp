// SPDX-License-Identifier: MIT
#include "common/UiPalette.hpp"
#include "widgets/helper/GlassMaterial.hpp"
#include "widgets/helper/GlassStyle.hpp"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QMenu>
#include <QPainter>
#include <QTimer>
#include <QWidget>

#include <cmath>
#include <iostream>

using namespace chatterino;

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setStyle(new GlassStyle);
    auto palette = app.palette();
    palette.setColor(QPalette::Window, QColor(20, 20, 22));
    palette.setColor(QPalette::WindowText, QColor(235, 235, 239));
    palette.setColor(QPalette::Text, QColor(235, 235, 239));
    app.setPalette(palette);
    if (argc != 2)
    {
        return 2;
    }
    QDir().mkpath(QString::fromLocal8Bit(argv[1]));
    const QString output = QString::fromLocal8Bit(argv[1]);
    QImage source(640, 80, QImage::Format_RGB32);
    for (int y = 0; y < source.height(); ++y)
    {
        auto *row = reinterpret_cast<QRgb *>(source.scanLine(y));
        for (int x = 0; x < source.width(); ++x)
        {
            const int checker = ((x / 12 + y / 12) % 2) * 100;
            row[x] = qRgb(30 + checker, 30 + checker, 32 + checker);
        }
    }
    {
        QPainter painter(&source);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(92, 158, 127));
        painter.drawEllipse(QRect(32, 10, 90, 65));
        painter.setBrush(QColor(175, 134, 97));
        painter.drawEllipse(QRect(400, -20, 150, 90));
    }
    const auto blurred = GlassBackdrop::diffuse(source);
    GlassOptions options{.role = GlassRole::Composer, .radius = 8};
    const auto glass = GlassSurface::render(blurred, {640, 48}, 1, options);
    auto unrefractedOptions = options;
    unrefractedOptions.refraction = 0;
    const auto unrefracted =
        GlassSurface::render(blurred, {640, 48}, 1, unrefractedOptions);
    int refractedPixels = 0;
    for (int y = 0; y < glass.height(); ++y)
    {
        for (int x = 0; x < glass.width(); ++x)
        {
            refractedPixels += glass.pixel(x, y) != unrefracted.pixel(x, y);
        }
    }
    if (refractedPixels < 20 ||
        glass.pixel(320, 24) != unrefracted.pixel(320, 24) ||
        qAlpha(glass.pixel(0, 0)) != 0)
    {
        std::cerr << "Optical refraction/masking checks failed\n";
        return 1;
    }
    QWidget owner;
    QImage target(640, 48, QImage::Format_ARGB32_Premultiplied);
    QPainter cached(&target);
    GlassSurface surface;
    for (int i = 0; i < 100; ++i)
    {
        surface.paint(cached, &owner, QRectF(0, 0, 640, 48), options);
    }
    cached.end();
    if (surface.renderCount() != 1)
    {
        std::cerr << "Stable chrome was recomputed during repeated painting\n";
        return 1;
    }
    QElapsedTimer elapsed;
    elapsed.start();
    for (int i = 0; i < 60; ++i)
    {
        const auto frame =
            GlassSurface::render(blurred, {1000, 36}, 1, options);
        if (frame.isNull())
        {
            return 1;
        }
    }
    std::cout
        << "1000x36 optical layer: " << elapsed.nsecsElapsed() / 60000000.0
        << " ms per rebuild; unchanged surface: 1 rebuild for 100 paints; "
        << refractedPixels << " pixels displaced near the rim\n";
    QImage review(720, 430, QImage::Format_RGB32);
    review.fill(QColor(20, 20, 22));
    QPainter painter(&review);
    painter.setFont(QFont("Sans Serif", 11));
    painter.setPen(QColor(235, 235, 239));
    painter.drawText(30, 32,
                     "Moltorino material — native Qt optical reference");
    painter.drawImage(QRect(30, 55, 660, 80), source);
    painter.drawText(30, 160,
                     "Diffusion + bounded edge refraction + Fresnel rim");
    painter.drawImage(QRect(30, 180, 660, 50), glass);
    painter.drawText(46, 211, "Message composer");
    auto popover = options;
    popover.role = GlassRole::Popover;
    popover.radius = 9;
    painter.drawImage(QRect(30, 255, 250, 140),
                      GlassSurface::render(blurred, {250, 140}, 1, popover));
    painter.drawText(47, 287, "Open channel");
    painter.drawText(47, 320, "Copy channel address");
    painter.drawText(47, 369, "Channel settings");
    painter.setPen(QColor(150, 150, 159));
    painter.drawText(310, 290, "Source stays in process memory.");
    painter.drawText(310, 319, "No desktop capture or network I/O.");
    painter.drawText(310, 348, "No per-message effects.");
    painter.end();
    if (!review.save(output + "/material-reference.png"))
    {
        return 1;
    }
    class ChatPattern final : public QWidget
    {
    public:
        explicit ChatPattern(QWidget *parent)
            : QWidget(parent)
        {
        }
        int paints = 0;

    protected:
        void paintEvent(QPaintEvent *) override
        {
            ++this->paints;
            QPainter paint(this);
            paint.fillRect(this->rect(), QColor(20, 20, 22));
            paint.setFont(QFont("Sans Serif", 10));
            for (int y = 20; y < this->height(); y += 24)
            {
                paint.setPen(QColor(145, 181, 161));
                paint.drawText(12, y, "mira:");
                paint.setPen(QColor(235, 235, 239));
                paint.drawText(56, y,
                               "Good evening everyone. Thanks for the stream!");
            }
        }
    };
    QWidget window;
    window.resize(640, 380);
    ChatPattern chat(&window);
    chat.setGeometry(0, 0, 640, 320);
    QWidget consumer(&window);
    consumer.setGeometry(0, 320, 640, 40);
    window.show();
    auto *sampler = GlassBackdrop::watch(&chat);
    sampler->addConsumer(&consumer, consumer.rect());
    QEventLoop loop;
    QTimer updates;
    QObject::connect(&updates, &QTimer::timeout, &chat, [&chat] {
        chat.update();
    });
    updates.start(10);
    QTimer::singleShot(1250, &loop, &QEventLoop::quit);
    loop.exec();
    updates.stop();
    if (chat.paints < 30 || sampler->captureCount() > 5)
    {
        std::cerr << "Backdrop capture budget failed: paints=" << chat.paints
                  << ", captures=" << sampler->captureCount() << '\n';
        return 1;
    }
    std::cout << "Capture budget: " << sampler->captureCount()
              << " samples for " << chat.paints
              << " chat paints in 1.25 seconds\n";
    QMenu menu(&window);
    menu.addAction("Open channel");
    menu.addAction("Copy channel address");
    menu.addSeparator();
    menu.addAction("Channel settings");
    menu.addAction("Unavailable action")->setEnabled(false);
    menu.popup(chat.mapToGlobal(QPoint(60, 72)));
    menu.setActiveAction(menu.actions()[1]);
    QTimer::singleShot(180, &loop, &QEventLoop::quit);
    loop.exec();
    if (!menu.grab().save(output + "/material-menu.png"))
        return 1;
    menu.close();
    window.close();
    return 0;
}
