// SPDX-License-Identifier: MIT
#include "widgets/glass/GlassChrome.hpp"
#ifdef MOLTORINO_WITH_QTQUICK_GLASS_PREVIEW

#    include "Application.hpp"
#    include "singletons/Theme.hpp"
#    include "singletons/ThemeCustomization.hpp"
#    include "singletons/ThemeVideo.hpp"
#    include "singletons/ThemeVideoDecoder.hpp"
#    include "widgets/AccountSwitchPopup.hpp"
#    include "widgets/BasePopup.hpp"
#    include "widgets/BaseWindow.hpp"
#    include "widgets/buttons/Button.hpp"
#    include "widgets/dialogs/EmotePopup.hpp"
#    include "widgets/dialogs/SettingsDialog.hpp"
#    include "widgets/helper/ChannelView.hpp"
#    include "widgets/helper/NotebookTab.hpp"
#    include "widgets/Label.hpp"
#    include "widgets/splits/Split.hpp"

#    include <QAbstractButton>
#    include <QAbstractItemModel>
#    include <QAbstractScrollArea>
#    include <QApplication>
#    include <QComboBox>
#    include <QCryptographicHash>
#    include <QDebug>
#    include <QElapsedTimer>
#    include <QEvent>
#    include <QFile>
#    include <QImage>
#    include <QJsonArray>
#    include <QLabel>
#    include <QLineEdit>
#    include <QMenu>
#    include <QMouseEvent>
#    include <QOperatingSystemVersion>
#    include <QPainter>
#    include <QPlainTextEdit>
#    include <QPointer>
#    include <QQmlContext>
#    include <QQmlEngine>
#    include <QQuickImageProvider>
#    include <QQuickItem>
#    include <QQuickWindow>
#    include <QRegularExpression>
#    include <QScrollArea>
#    include <QSGRendererInterface>
#    include <QStyle>
#    include <QTabBar>
#    include <QTextEdit>
#    include <QTimer>
#    include <QtQuickWidgets/QQuickWidget>
#    include <QWindow>

#    include <algorithm>
#    include <atomic>
#    include <memory>
#    include <optional>

#    ifdef USEWINSDK
#        include <dwmapi.h>
#        include <qt_windows.h>
#        pragma comment(lib, "Dwmapi.lib")
#    endif

namespace chatterino::glass {
namespace {
struct Panel {
    QPointer<QWidget> owner;
    QRectF local;
    QString kind;
    qreal emphasis = 0;
};
class Panels final : public QAbstractListModel
{
public:
    enum {
        Bounds = Qt::UserRole + 1,
        Kind,
        Emphasis,
        Hovered,
        Pressed,
        Disabled,
        Pointer
    };
    QList<Panel> entries;
    QPoint cursor;
    QWidget *window = nullptr;
    int rowCount(const QModelIndex & = {}) const override
    {
        return entries.size();
    }
    QHash<int, QByteArray> roleNames() const override
    {
        return {{Bounds, "bounds"},     {Kind, "kind"},
                {Emphasis, "emphasis"}, {Hovered, "hovered"},
                {Pressed, "pressed"},   {Disabled, "disabled"},
                {Pointer, "pointer"}};
    }
    QVariant data(const QModelIndex &i, int role) const override
    {
        if (!i.isValid() || i.row() >= entries.size())
            return {};
        const auto &p = entries[i.row()];
        if (!p.owner)
            return {};
        const auto rect =
            p.owner->window() == window
                ? QRectF(p.owner->mapTo(window, p.local.topLeft()),
                         p.local.size())
                : QRectF();
        const bool shown = p.owner->isVisibleTo(window) && !rect.isEmpty();
        const bool hover = shown && rect.contains(cursor);
        switch (role)
        {
            case Bounds:
                return shown ? rect : QRectF();
            case Kind:
                return p.kind;
            case Emphasis: {
                auto *focus = QApplication::focusWidget();
                if (p.kind == "composer")
                    return focus && (focus == p.owner ||
                                     p.owner->isAncestorOf(focus))
                               ? 1.
                               : 0.;
                return p.emphasis;
            }
            case Hovered:
                return hover;
            case Pressed: {
                if (auto *b = dynamic_cast<Button *>(p.owner.data()))
                    return b->leftMouseButtonDown() || b->menuVisible();
                if (auto *b = qobject_cast<QAbstractButton *>(p.owner.data()))
                    return b->isDown();
                return hover && (QApplication::mouseButtons() & Qt::LeftButton);
            }
            case Disabled: {
                auto *button = dynamic_cast<Button *>(p.owner.data());
                return !p.owner->isEnabled() || (button && !button->enabled());
            }
            case Pointer:
                return hover ? QPointF((cursor.x() - rect.x()) /
                                           std::max(1., rect.width()),
                                       (cursor.y() - rect.y()) /
                                           std::max(1., rect.height()))
                             : QPointF(-2, -2);
        }
        return {};
    }
    void put(QWidget *owner, QRectF rect, const QString &kind, qreal emphasis)
    {
        for (int i = 0; i < entries.size(); ++i)
        {
            auto &p = entries[i];
            if (p.owner == owner && p.kind == kind)
            {
                if (p.local != rect || p.emphasis != emphasis)
                {
                    p.local = rect;
                    p.emphasis = emphasis;
                    Q_EMIT dataChanged(index(i), index(i));
                }
                return;
            }
        }
        beginInsertRows({}, entries.size(), entries.size());
        entries.push_back({owner, rect, kind, emphasis});
        endInsertRows();
    }
    void refresh()
    {
        // Remove destroyed controls; animations and delegates are reused.
        for (int i = entries.size() - 1; i >= 0; --i)
            if (!entries[i].owner)
            {
                beginRemoveRows({}, i, i);
                entries.removeAt(i);
                endRemoveRows();
            }
        cursor = window->mapFromGlobal(QCursor::pos());
        if (!entries.isEmpty())
            Q_EMIT dataChanged(index(0), index(entries.size() - 1));
    }
};
class Backdrop final : public QQuickImageProvider
{
public:
    QImage image;
    Backdrop()
        : QQuickImageProvider(QQuickImageProvider::Image)
    {
    }
    QImage requestImage(const QString &, QSize *size, const QSize &) override
    {
        if (size)
            *size = image.size();
        return image;
    }
};
class Chrome;
QList<Chrome *> layers;
bool constructing = false;

bool settingsWindow(const QWidget *window)
{
    return dynamic_cast<const SettingsDialog *>(window) != nullptr;
}

void prepareDesktopBackdrop(QWidget *window)
{
#    ifdef USEWINSDK
    if (!window->isWindow() || window->inherits("chatterino::Window") ||
        settingsWindow(window) ||
        window->property("moltorinoDesktopBackdrop").toBool() ||
        qEnvironmentVariableIsSet("MOLTORINO_GLASS_DISABLE_DESKTOP") ||
        QOperatingSystemVersion::current().microVersion() < 22621)
        return;
    // Qt requires a frameless alpha surface. BaseWindow's own caption and
    // native hit testing remain intact; never remove an OS-owned title bar.
    auto *base = dynamic_cast<BaseWindow *>(window);
    if (!(window->windowFlags() & Qt::FramelessWindowHint) &&
        !(base && base->hasCustomWindowFrame()))
        return;
    window->setProperty("moltorinoDesktopBackdrop", true);
    window->setWindowFlag(Qt::FramelessWindowHint);
    window->setAttribute(Qt::WA_TranslucentBackground);
#    else
    Q_UNUSED(window);
#    endif
}

// Preserve semantic colors (badges, outcomes, selections) and all metrics.
// Only dark opaque stylesheet fills become readable translucent panels.
QString panelStyles(QString sheet)
{
    static const QRegularExpression fills(QStringLiteral(
        "(?<![-\\w])background(?:-color)?\\s*:\\s*(#[0-9a-fA-F]{3,8})\\s*;"));
    static const QRegularExpression semantic(
        QStringLiteral(
            "Swatch|ColorPreview|ColorPicker|Badge|Progress|Indicator"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression shell(QStringLiteral(
        "#\\w*(?:Root|Content|Header|Footer|ScrollArea)(?:\\s|,|$)"));
    auto matches = fills.globalMatch(sheet);
    struct Replacement {
        qsizetype start;
        qsizetype length;
        QString color;
    };
    QList<Replacement> replacements;
    while (matches.hasNext())
    {
        const auto match = matches.next();
        const auto open = sheet.lastIndexOf('{', match.capturedStart());
        const auto previous = sheet.lastIndexOf('}', open);
        const auto selector = sheet.mid(previous + 1, open - previous - 1);
        if (semantic.match(selector).hasMatch())
            continue;
        QColor color(match.captured(1));
        if (!color.isValid() || color.alpha() < 240 ||
            color.lightnessF() > 0.3 || color.hsvSaturationF() > 0.25)
            continue;
        // The GPU already owns the shell. Repeated translucent fills on a
        // scroll area's nested containers otherwise add up to opaque black.
        color.setAlpha(shell.match(selector).hasMatch() ? 0 : 85);
        replacements.append({match.capturedStart(1), match.capturedLength(1),
                             QString("rgba(%1,%2,%3,%4)")
                                 .arg(color.red())
                                 .arg(color.green())
                                 .arg(color.blue())
                                 .arg(color.alpha())});
    }
    for (auto i = replacements.crbegin(); i != replacements.crend(); ++i)
        sheet.replace(i->start, i->length, i->color);
    return sheet;
}

// QMenu and Qt's tooltip label paint foreground directly on their window.
// Replay that native paint ABOVE the GPU child without rendering children or
// replacing actions, shortcuts, accessibility, submenu logic or hit testing.
class NativeForeground final : public QWidget
{
    bool rendering_ = false;

public:
    explicit NativeForeground(QWidget *parent)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
    }
    bool rendering() const
    {
        return rendering_;
    }
    void paintEvent(QPaintEvent *) override
    {
        if (rendering_)
            return;
        rendering_ = true;
        QImage text(size() * devicePixelRatioF(),
                    QImage::Format_ARGB32_Premultiplied);
        text.setDevicePixelRatio(devicePixelRatioF());
        text.fill(Qt::transparent);
        parentWidget()->render(&text, {}, QRegion(),
                               QWidget::DrawWindowBackground);
        QPainter painter(this);
        painter.drawImage(QPoint(), text);
        rendering_ = false;
    }
};
class Chrome final : public QQuickWidget
{
public:
    Panels panels;
    Backdrop *backdrop = nullptr;
    QPointer<NativeForeground> foreground;
    QTimer synchronize;
    int revision = 0;
    bool failed = false;
    QSize backdropSize;
    QImage popupBackdrop;
    QPointer<QWidget> popupOwner;
    QTimer popupRefresh;
    QPoint popupBackdropOrigin;
    QString backdropKey;
    std::optional<ThemeCustomizationProfile> backdropProfile;
    QString videoSource;
    std::shared_ptr<ThemeVideo> video;
    QElapsedTimer videoThrottle;
    std::atomic<int> frames{0};
    bool capturing = false;
    bool popupDirty = true;
    bool ownedPopup = false;
    bool desktopBackdrop = false;
    bool desktopDark = true;
    WId backdropWindowId = 0;
    QHash<QWidget *, QPair<QString, QString>> styled;
    QList<QPointer<QWidget>> clearedBackgrounds;
    QList<QPointer<ChannelView>> transparentViews;
    QList<QRectF> wallpaperGeometry;
    explicit Chrome(QWidget *parent)
        : QQuickWidget(parent)
    {
        // Windows 11 supplies the live desktop through DWM. The owner capture
        // is the fallback on older systems; neither path captures the screen.
        if (!parent->inherits("chatterino::Window") && !settingsWindow(parent))
        {
            ownedPopup = true;
        }
        if (ownedPopup || parent->windowType() == Qt::Popup ||
            parent->windowType() == Qt::ToolTip ||
            qobject_cast<QMenu *>(parent) ||
            dynamic_cast<BasePopup *>(parent) ||
            dynamic_cast<AccountSwitchPopup *>(parent))
        {
            auto *owner = parent->parentWidget()
                              ? parent->parentWidget()->window()
                              : QApplication::activeWindow();
            // Show events often arrive after the popup became active. Resolve
            // its transient owner rather than capturing the popup itself.
            if ((!owner || owner == parent) && parent->windowHandle() &&
                parent->windowHandle()->transientParent())
                owner = QWidget::find(
                    parent->windowHandle()->transientParent()->winId());
            if (!owner || owner == parent)
                for (auto *candidate : QApplication::topLevelWidgets())
                    if (candidate != parent && candidate->isVisible() &&
                        candidate->inherits("chatterino::Window"))
                    {
                        owner = candidate;
                        break;
                    }
            if (owner && owner != parent && owner->isVisible())
            {
                popupOwner = owner;
                capturePopupBackdrop();
            }
        }
        popupRefresh.setInterval(125);
        connect(&popupRefresh, &QTimer::timeout, this, [this] {
            if (isVisible() && !failed)
                capturePopupBackdrop();
        });
        if (popupOwner)
            popupRefresh.start();
        layers.push_back(this);
        setObjectName("moltorinoGpuChrome");
        videoThrottle.start();
        connect(
            quickWindow(), &QQuickWindow::afterRendering, this,
            [this] {
                ++frames;
            },
            Qt::DirectConnection);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFocusPolicy(Qt::NoFocus);
        setClearColor(Qt::transparent);
        setResizeMode(QQuickWidget::SizeRootObjectToView);
        quickWindow()->setPersistentSceneGraph(false);
        connect(
            quickWindow(), &QQuickWindow::sceneGraphInitialized, this, [this] {
                if (quickWindow()->rendererInterface()->graphicsApi() ==
                    QSGRendererInterface::Software)
                    failRenderer(QStringLiteral("Software Qt Quick cannot "
                                                "execute the optical shaders"));
                else
                    parentWidget()->update();
            });
        panels.window = parent;
        backdrop = new Backdrop;
        engine()->addImageProvider("ownedBackdrop", backdrop);
        rootContext()->setContextProperty("chromePanels", &panels);
        connect(this, &QQuickWidget::sceneGraphError, this,
                [this](auto, const QString &message) {
                    failRenderer(message);
                });
        setSource(QUrl("qrc:/moltorino-glass/GlassChrome.qml"));
        if (status() == QQuickWidget::Error)
        {
            failRenderer(
                QStringLiteral("Production material QML failed to load"));
            qWarning() << "GLASS_QML_ERROR" << errors();
        }
        synchronize.setSingleShot(true);
        synchronize.setInterval(0);
        connect(&synchronize, &QTimer::timeout, this, [this] {
            sync();
        });
        setGeometry(parent->rect());
        lower();
        show();
        if (qobject_cast<QMenu *>(parent) || parent->inherits("QTipLabel"))
        {
            foreground = new NativeForeground(parent);
            foreground->setGeometry(parent->rect());
            foreground->raise();
            foreground->show();
        }
        request();
    }
    void updateDesktopBackdrop()
    {
#    ifdef USEWINSDK
        auto *window = parentWidget();
        const bool darkTheme = !getTheme()->isLightTheme();
        if (!window->property("moltorinoDesktopBackdrop").toBool() ||
            !window->internalWinId() ||
            (backdropWindowId == window->internalWinId() &&
             desktopDark == darkTheme))
            return;
        backdropWindowId = window->internalWinId();
        desktopDark = darkTheme;
        const auto hwnd = reinterpret_cast<HWND>(backdropWindowId);
        // Values are documented in the Windows 11 SDK. Runtime HRESULT checks
        // also allow builds using older Windows SDK headers.
        constexpr auto systemBackdrop = static_cast<DWMWINDOWATTRIBUTE>(38);
        const int acrylic =
            3;  // DWMSBT_TRANSIENTWINDOW, actual desktop acrylic
        const BOOL dark = darkTheme ? TRUE : FALSE;
        DwmSetWindowAttribute(hwnd, static_cast<DWMWINDOWATTRIBUTE>(20), &dark,
                              sizeof(dark));
        const MARGINS margins{-1, -1, -1, -1};
        desktopBackdrop =
            SUCCEEDED(DwmExtendFrameIntoClientArea(hwnd, &margins)) &&
            SUCCEEDED(DwmSetWindowAttribute(hwnd, systemBackdrop, &acrylic,
                                            sizeof(acrylic)));
        if (rootObject())
            rootObject()->setProperty("systemBackdrop", desktopBackdrop);
        popupBackdrop = {};
        backdropKey.clear();
        if (desktopBackdrop)
            popupRefresh.stop();
        else if (popupOwner)
        {
            popupDirty = true;
            popupRefresh.start();
        }
#    endif
    }
    void capturePopupBackdrop()
    {
        if (desktopBackdrop || capturing || !popupDirty || isAppAboutToQuit() ||
            !tryGetApp())
            return;
        popupDirty = false;
        if (!popupOwner || !popupOwner->isVisible() ||
            popupOwner->isMinimized())
        {
            if (!popupBackdrop.isNull())
            {
                popupBackdrop = {};
                backdropKey.clear();
                request();
            }
            return;
        }
        const auto origin =
            popupOwner->mapFromGlobal(parentWidget()->mapToGlobal(QPoint()));
        const auto region = QRect(origin, parentWidget()->size())
                                .intersected(popupOwner->rect());
        if (region.isEmpty())
        {
            if (!popupBackdrop.isNull())
            {
                popupBackdrop = {};
                backdropKey.clear();
                request();
            }
            return;
        }
        const auto offset = parentWidget()->mapFromGlobal(
            popupOwner->mapToGlobal(region.topLeft()));
        capturing = true;
        auto pixels = popupOwner->grab(region).toImage();
        capturing = false;
        if (pixels == popupBackdrop && offset == popupBackdropOrigin)
            return;
        popupBackdrop = std::move(pixels);
        popupBackdropOrigin = offset;
        backdropKey.clear();
        if (rootObject())
            request();
    }
    void failRenderer(const QString &message)
    {
        failed = true;
        popupRefresh.stop();
#    ifdef USEWINSDK
        if (desktopBackdrop && parentWidget()->internalWinId())
        {
            const int none = 1;  // DWMSBT_NONE
            DwmSetWindowAttribute(
                reinterpret_cast<HWND>(parentWidget()->internalWinId()),
                static_cast<DWMWINDOWATTRIBUTE>(38), &none, sizeof(none));
            desktopBackdrop = false;
        }
#    endif
        qWarning() << "GLASS_GPU_ERROR" << message;
        QTimer::singleShot(0, this, [this] {
            hide();
            if (foreground)
                foreground->hide();
            for (auto i = styled.cbegin(); i != styled.cend(); ++i)
                if (i.key()->styleSheet() == i.value().second)
                    i.key()->setStyleSheet(i.value().first);
            for (const auto &w : clearedBackgrounds)
                if (w)
                    w->setAutoFillBackground(true);
            for (const auto &view : transparentViews)
                if (view)
                    view->setTransparentBackground(false);
            for (auto *w : parentWidget()->findChildren<QWidget *>())
            {
                w->setProperty("liquidGlassSurface", false);
                w->style()->unpolish(w);
                w->style()->polish(w);
                w->update();
            }
            parentWidget()->setAutoFillBackground(true);
            const auto bg = getTheme()->window.background.name();
            parentWidget()->setStyleSheet(
                parentWidget()->styleSheet() +
                QString("QMenu,QToolTip,QPushButton,QToolButton,QLineEdit,"
                        "QComboBox,QTextEdit,QPlainTextEdit {background:%1;}")
                    .arg(bg));
            parentWidget()->update();
        });
    }
    ~Chrome() override
    {
        disconnect(quickWindow(), nullptr, this, nullptr);
        if (video)
            video->detach(this);
        layers.removeAll(this);
    }
    void request()
    {
        if (!synchronize.isActive())
            synchronize.start();
    }
    void updateBackdrop()
    {
        const auto &profile = getTheme()->customization;
        const auto source = ownedPopup ? QString() : profile.wallpaperSource;
        if (videoSource != source)
        {
            if (video)
                video->detach(this);
            video.reset();
            videoSource = source;
            if (isThemeVideo(videoSource))
            {
                video = ThemeVideo::acquire(videoSource, this);
                connect(video.get(), &ThemeVideo::frameChanged, this, [this] {
                    if (videoThrottle.elapsed() >= 125)
                    {
                        videoThrottle.restart();
                        backdropKey.clear();
                        request();
                    }
                });
            }
        }
        const auto key = profile.wallpaperSource +
                         QString::number(profile.wallpaperOpacity) +
                         QString::number(profile.wallpaperOverlayOpacity) +
                         QString::number(profile.wallpaperBlur) +
                         getTheme()->window.background.name() +
                         QString::number(devicePixelRatioF());
        QList<QRectF> mapping;
        for (auto *view : parentWidget()->findChildren<ChannelView *>())
        {
            if (auto *split = dynamic_cast<Split *>(view->parentWidget());
                split && view->isVisible() && view->window() == parentWidget())
            {
                mapping << view->glassWallpaperTarget();
                mapping << QRectF(split->mapTo(parentWidget(), QPointF()),
                                  QSizeF(split->size()));
            }
        }
        if (backdropSize == size() && wallpaperGeometry == mapping &&
            backdropKey == key && backdropProfile &&
            *backdropProfile == profile)
            return;
        backdropSize = size();
        backdropKey = key;
        backdropProfile = profile;
        wallpaperGeometry = mapping;
        QImage image(size() * devicePixelRatioF(),
                     QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(devicePixelRatioF());
        const auto canvas = profile.hasWallpaper()
                                ? profile.chatBackground
                                : getTheme()->window.background;
        image.fill(canvas);
        QPainter p(&image);
        auto wallpaper =
            ownedPopup || profile.wallpaperSource.isEmpty() || video
                ? QImage()
                : loadThemeWallpaper(profile.wallpaperSource);
        if (!wallpaper.isNull())
            wallpaper =
                blurThemeWallpaper(std::move(wallpaper), profile.wallpaperBlur);
        if (video || !wallpaper.isNull())
        {
            p.setRenderHint(QPainter::SmoothPixmapTransform);
            p.setOpacity(profile.wallpaperOpacity / 100.);
            auto drawWallpaper = [&](const QRectF &target) {
                if (video)
                {
                    video->paint(p, target, profile);
                    return;
                }
                if (profile.wallpaperMode == ThemeWallpaperMode::Tile)
                {
                    const auto zoom =
                        std::clamp(profile.wallpaperZoom, 100, 300) / 100.;
                    p.save();
                    p.translate(target.topLeft());
                    p.scale(zoom, zoom);
                    const auto clip = p.clipBoundingRect();
                    // The tile origin stays identical to ChannelView's mapping.
                    const auto area = clip.isEmpty()
                                          ? QRectF(-target.topLeft() / zoom,
                                                   QSizeF(size()) / zoom)
                                          : clip;
                    p.drawTiledPixmap(area, QPixmap::fromImage(wallpaper),
                                      area.topLeft());
                    p.restore();
                    return;
                }
                const auto layout = layoutThemeWallpaper(
                    wallpaper.size(), target, profile.wallpaperMode,
                    profile.wallpaperFocalX, profile.wallpaperFocalY,
                    profile.wallpaperZoom);
                if (layout.source.isEmpty())
                    return;
                const qreal sx =
                    layout.destination.width() / layout.source.width();
                const qreal sy =
                    layout.destination.height() / layout.source.height();
                // Extend the same photo transform under the chrome, rather than
                // stretching a screenshot or re-cropping it on every surface.
                const QRectF extended(
                    layout.destination.topLeft() -
                        QPointF(layout.source.x() * sx, layout.source.y() * sy),
                    QSizeF(wallpaper.width() * sx, wallpaper.height() * sy));
                p.drawImage(extended, wallpaper);
            };
            drawWallpaper(mapping.isEmpty() ? QRectF(rect()) : mapping.front());
            for (int i = 0; i + 1 < mapping.size(); i += 2)
            {
                p.save();
                p.setClipRect(mapping[i + 1]);
                p.setOpacity(1.);
                p.fillRect(mapping[i + 1], canvas);
                p.setOpacity(profile.wallpaperOpacity / 100.);
                drawWallpaper(mapping[i]);
                p.restore();
            }
            p.setOpacity(1.);
            auto overlay = profile.wallpaperOverlayColor;
            overlay.setAlphaF(overlay.alphaF() *
                              profile.wallpaperOverlayOpacity / 100.);
            p.fillRect(rect(), overlay);
        }
        if (!popupBackdrop.isNull())
        {
            // The owner capture can contain live chat text. Diffuse that
            // content before it reaches the shared optical shader so it cannot
            // compete with the popup's native foreground labels and messages.
            // Half-resolution filtering follows cached backdrop revisions,
            // so hover events do not repeat it. The GPU shaders stay intact.
            const QRectF captured(popupBackdropOrigin,
                                  QSizeF(popupBackdrop.size()) /
                                      popupBackdrop.devicePixelRatio());
            auto diffuse = popupBackdrop.scaled(
                QSize(std::max(1, popupBackdrop.width() / 2),
                      std::max(1, popupBackdrop.height() / 2)),
                Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            diffuse = blurThemeWallpaper(
                std::move(diffuse),
                qRound(8 * popupBackdrop.devicePixelRatio()));
            p.drawImage(captured, diffuse, diffuse.rect());
            // Tint only the captured portion; do not invent wallpaper outside
            // the owner window or retain stale pixels after moving off it.
            p.fillRect(captured, QColor(0, 0, 0, 55));
        }
        p.end();
        backdrop->image = std::move(image);
        if (rootObject())
            rootObject()->setProperty(
                "backdropUrl",
                QString("image://ownedBackdrop/%1").arg(++revision));
    }
    void sync()
    {
        // Timers queued before shutdown must not fetch Themes/Windows after
        // Application::stop() has released their owning singletons.
        if (isAppAboutToQuit() || !tryGetApp())
            return;
        if (failed || !parentWidget()->isVisible())
            return;
        updateDesktopBackdrop();
        if (geometry() != parentWidget()->rect())
            setGeometry(parentWidget()->rect());
        updateBackdrop();
        if (!parentWidget()->inherits("chatterino::Window"))
            panels.put(parentWidget(),
                       parentWidget()->rect().adjusted(1, 1, -1, -1), "window",
                       0.);
        const bool settings = settingsWindow(parentWidget());
        auto controls = parentWidget()->findChildren<QWidget *>();
        controls.prepend(parentWidget());
        for (auto *w : controls)
        {
            if (w->window() != parentWidget() || !w->isVisible() || w == this ||
                w == foreground || isAncestorOf(w))
                continue;
            // ChannelView owns native chat text, fonts, colors and badges.
            // Popout histories use the GPU shell's mapped owner backdrop.
            if (auto *view = dynamic_cast<ChannelView *>(w))
            {
                if (ownedPopup && !view->getTransparentBackground())
                {
                    transparentViews.append(view);
                    view->setTransparentBackground(true);
                }
                continue;
            }
            bool chatChild = false;
            for (auto *p = w->parentWidget(); p && p != parentWidget();
                 p = p->parentWidget())
                if (dynamic_cast<ChannelView *>(p))
                    chatChild = true;
            if (chatChild)
                continue;
            if (auto *label = qobject_cast<QLabel *>(w))
            {
                auto font = label->font();
                if (font.weight() < QFont::DemiBold)
                {
                    font.setWeight(QFont::DemiBold);
                    label->setFont(font);
                }
            }
            if (auto *label = dynamic_cast<Label *>(w);
                label && label->getFontStyle() == FontStyle::UiMedium)
                label->setFontStyle(FontStyle::UiMediumBold);
            if (settings)
                continue;
            // Clear inherited opaque palette fills only where a real GPU
            // layer is available. Do not modify semantic palette colors.
            if (w->autoFillBackground())
            {
                clearedBackgrounds.append(w);
                w->setAutoFillBackground(false);
            }
            const auto current = w->styleSheet();
            if (!current.isEmpty() &&
                (!styled.contains(w) || styled.value(w).second != current))
            {
                const auto material = panelStyles(current);
                if (!styled.contains(w))
                    connect(w, &QObject::destroyed, this, [this, w] {
                        styled.remove(w);
                    });
                styled[w] = {current, material};
                if (material != current)
                    w->setStyleSheet(material);
            }
            if (auto *tabs = qobject_cast<QTabBar *>(w))
            {
                // Native tabs retain their selected/hover states and keys.
                panels.put(tabs, tabs->rect(), "toolbar", 0.);
            }
            else if (w->objectName().endsWith("Header") ||
                     w->objectName().endsWith("Footer"))
            {
                panels.put(w, w->rect(), "toolbar", 0.);
            }
            if (auto *scroll = qobject_cast<QScrollArea *>(w))
            {
                scroll->viewport()->setAutoFillBackground(false);
                if (scroll->widget())
                    scroll->widget()->setAutoFillBackground(false);
            }
            if (dynamic_cast<Button *>(w) && !dynamic_cast<NotebookTab *>(w))
            {
                panels.put(w, QRectF(w->rect()).adjusted(2, 2, -2, -2),
                           "button", 0.);
            }
            else if (qobject_cast<QAbstractButton *>(w) &&
                     !w->inherits("QCheckBox") && !w->inherits("QRadioButton"))
            {
                panels.put(w, QRectF(w->rect()).adjusted(1, 1, -1, -1),
                           "button", 0.);
                if (!w->property("liquidGlassSurface").toBool())
                {
                    w->setProperty("liquidGlassSurface", true);
                    w->style()->unpolish(w);
                    w->style()->polish(w);
                }
            }
            else if (qobject_cast<QLineEdit *>(w) ||
                     qobject_cast<QComboBox *>(w) ||
                     qobject_cast<QTextEdit *>(w) ||
                     qobject_cast<QPlainTextEdit *>(w))
            {
                panels.put(w, QRectF(w->rect()).adjusted(1, 1, -1, -1), "field",
                           w->hasFocus() ? 1. : 0.);
                if (!w->property("liquidGlassSurface").toBool())
                {
                    w->setProperty("liquidGlassSurface", true);
                    w->style()->unpolish(w);
                    w->style()->polish(w);
                }
            }
        }
        if (auto *menu = qobject_cast<QMenu *>(parentWidget()))
        {
            auto *active = menu->activeAction();
            panels.put(menu, active ? menu->actionGeometry(active) : QRectF(),
                       "selection", 1.);
        }
        panels.refresh();
        lower();
        if (foreground)
        {
            foreground->setGeometry(parentWidget()->rect());
            foreground->raise();
            foreground->update();
        }
    }
};
Chrome *layer(const QWidget *widget)
{
    if (!widget)
        return nullptr;
    for (auto *c : layers)
        if (c->parentWidget() == widget->window())
            return c;
    return nullptr;
}
class ChromeEvents final : public QObject
{
    bool eventFilter(QObject *object, QEvent *event) override
    {
        // Do not install additional GPU windows or schedule material refresh
        // while the underlying Qt application is already closing.
        if (isAppAboutToQuit() || !tryGetApp())
            return false;
        auto *w = qobject_cast<QWidget *>(object);
        if (!w || constructing || w->inherits("QQuickWidget") ||
            w->objectName() == "moltorinoGpuChrome" ||
            w->objectName() == "moltorinoReviewBackdrop")
            return false;
        if (event->type() == QEvent::Polish && w->isWindow())
            prepareDesktopBackdrop(w);
        if (event->type() == QEvent::Paint || event->type() == QEvent::Move ||
            event->type() == QEvent::Resize || event->type() == QEvent::Show ||
            event->type() == QEvent::Hide)
            for (auto *popup : layers)
                if (!popup->capturing && (popup->popupOwner == w->window() ||
                                          popup->parentWidget() == w))
                    popup->popupDirty = true;
        if (event->type() == QEvent::Show && w->isWindow() && !layer(w) &&
            !w->inherits("QDesktopWidget") &&
            (w->windowFlags() & Qt::WindowType_Mask) != Qt::ForeignWindow)
        {
            // Never change composition of embedded browser/OS-owned windows.
            constructing = true;
            new Chrome(w);
            constructing = false;
        }
        auto *c = layer(w);
        if (!c || c->isAncestorOf(w) || w == c->foreground)
            return false;
        if (w == c->parentWidget())
        {
            if (event->type() == QEvent::WinIdChange ||
                event->type() == QEvent::ThemeChange)
            {
                c->backdropWindowId = 0;
                c->request();
            }
            if (event->type() == QEvent::Hide)
                c->popupRefresh.stop();
            if (event->type() == QEvent::Show && c->popupOwner &&
                !c->desktopBackdrop)
                c->popupRefresh.start();
        }
        switch (event->type())
        {
            case QEvent::Resize:
            case QEvent::Move:
            case QEvent::Show:
            case QEvent::Hide:
            case QEvent::EnabledChange:
            case QEvent::FocusIn:
            case QEvent::FocusOut:
            case QEvent::Enter:
            case QEvent::Leave:
            case QEvent::MouseMove:
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonRelease:
            case QEvent::KeyPress:
            case QEvent::KeyRelease:
            case QEvent::ActionChanged:
            case QEvent::Wheel:
            case QEvent::LayoutRequest:
            case QEvent::PaletteChange:
                c->request();
                break;
            default:
                break;
        }
        return false;
    }
};
}  // namespace
bool enabled()
{
#    ifdef MOLTORINO_WITH_QTQUICK_GLASS_PREVIEW
    // The software scene graph cannot execute the compiled optical shaders.
    // Do not install a transparent Quick underlay in that mode: keep the
    // existing native Qt Widgets backgrounds, controls and input instead.
    return !qEnvironmentVariableIsSet("MOLTORINO_GLASS_DISABLE") &&
           qEnvironmentVariable("QT_QUICK_BACKEND").trimmed().compare(
               QStringLiteral("software"), Qt::CaseInsensitive) != 0;
#    else
    return false;
#    endif
}
void shutdownChrome()
{
    // Main Moltorino windows aren't WA_DeleteOnClose. Release GPU resources
    // deterministically while Qt Quick/Gui are still initialized, instead
    // of leaving QQuickWidgets alive until QApplication destruction.
    const auto active = layers;
    for (auto *chrome : active)
        delete chrome;  // ~Chrome() removes itself from layers.
}
void installChrome()
{
    if (!enabled())
        return;
    auto *events = new ChromeEvents;
    events->setParent(qApp);
    qApp->installEventFilter(events);
}
bool available(const QWidget *widget)
{
    auto *c = layer(widget);
    return enabled() && c && !c->failed;
}
bool surface(QWidget *widget, const QRectF &rect, const char *kind,
             qreal emphasis)
{
    auto *c = layer(widget);
    if (!available(widget))
        return false;
    c->panels.put(widget, rect, QString::fromLatin1(kind), emphasis);
    return true;
}
void reviewOptics(QWidget *window, bool active)
{
    if (qEnvironmentVariableIsEmpty("MOLTORINO_UI_REVIEW_DIR"))
        return;
    if (auto *c = layer(window); c && c->rootObject())
        c->rootObject()->setProperty("opticsEnabled", active);
}
QJsonObject diagnostics(QWidget *window)
{
    auto *c = layer(window);
    QJsonObject result{{"enabled", enabled()},
                       {"available", available(window)}};
    if (!c)
        return result;
    QJsonArray entries;
    for (int i = 0; i < c->panels.entries.size(); ++i)
    {
        const auto index = c->panels.index(i);
        const auto rect = c->panels.data(index, Panels::Bounds).toRectF();
        if (rect.isEmpty())
            continue;
        entries.append(QJsonObject{{"kind", c->panels.entries[i].kind},
                                   {"x", rect.x()},
                                   {"y", rect.y()},
                                   {"width", rect.width()},
                                   {"height", rect.height()}});
    }
    result["surfaces"] = entries;
    result["graphicsApi"] =
        static_cast<int>(c->quickWindow()->rendererInterface()->graphicsApi());
    result["qmlReady"] = c->status() == QQuickWidget::Ready;
    result["backdropRevision"] = c->revision;
    result["renderedFrames"] = c->frames.load();
    result["backdropSource"] = c->desktopBackdrop ? "system-desktop"
                               : c->ownedPopup    ? "owner-window"
                                                  : "chat-wallpaper";
    result["desktopBackdrop"] = c->desktopBackdrop;
    result["ownerCaptureValid"] = !c->popupBackdrop.isNull();
    result["settingsControlsPreserved"] = settingsWindow(window);
    QJsonObject baked;
    for (const auto &name : {"glass.vert.qsb", "glass.frag.qsb"})
    {
        QFile file(QString(":/shaders/") + name);
        if (file.open(QIODevice::ReadOnly))
            baked[name] = QString::fromLatin1(
                QCryptographicHash::hash(file.readAll(),
                                         QCryptographicHash::Sha256)
                    .toHex());
    }
    result["bakedShaderSha256"] = baked;
    return result;
}
}  // namespace chatterino::glass

#else
namespace chatterino::glass {
void shutdownChrome()
{
}
void installChrome()
{
}
bool enabled()
{
    return false;
}
bool available(const QWidget *)
{
    return false;
}
bool surface(QWidget *, const QRectF &, const char *, qreal)
{
    return false;
}
void reviewOptics(QWidget *, bool)
{
}
QJsonObject diagnostics(QWidget *)
{
    return {{"enabled", false}};
}
}  // namespace chatterino::glass
#endif
