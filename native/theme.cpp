#include "theme.h"
#include <QAbstractButton>
#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QIconEngine>
#include <QMap>
#include <QPainter>
#include <QPalette>

ThemeColors themeColors(bool dark) {
    if (dark) return {"#11161d", "#191f28", "#151b23", "#e5eaf1", "#9ba8b9",
                      "#303946", "#242e3b", "#183d38", "#65d6b4", "#f18c8c"};
    return {"#f5f7fa", "#ffffff", "#f9fafc", "#202b3c", "#637287",
            "#dfe5ed", "#edf1f6", "#e7f5ef", "#137c65", "#c44747"};
}

namespace {
// Vector icons stay sharp at fractional desktop scale factors without Qt SVG.
class LineIcon final : public QIconEngine {
public:
    explicit LineIcon(QString name) : name_(std::move(name)) {}
    QIconEngine *clone() const override { return new LineIcon(name_); }
    void paint(QPainter *p, const QRect &rect, QIcon::Mode mode, QIcon::State state) override {
        const auto c = themeColors(qApp->property("lighterDark").toBool());
        p->save(); p->setRenderHint(QPainter::Antialiasing);
        p->translate(rect.x(), rect.y()); p->scale(rect.width() / 24.0, rect.height() / 24.0);
        const QColor color = mode == QIcon::Disabled ? c.muted : name_ == "brand" ? QColor("#ffffff") : state == QIcon::On ? c.accent : c.muted;
        p->setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p->setBrush(Qt::NoBrush);
        if (name_ == "projects") {
            for (const auto &r : {QRectF(4, 4, 6, 6), QRectF(14, 4, 6, 6), QRectF(4, 14, 6, 6), QRectF(14, 14, 6, 6)}) p->drawRoundedRect(r, 1.5, 1.5);
        } else if (name_ == "ports") {
            p->drawRoundedRect(QRectF(7, 3, 10, 6), 1.5, 1.5); p->drawLine(12, 9, 12, 15); p->drawLine(5, 15, 19, 15);
            p->drawLine(5, 15, 5, 18); p->drawLine(19, 15, 19, 18);
            p->drawRect(QRectF(2, 18, 6, 4)); p->drawRect(QRectF(16, 18, 6, 4));
        } else if (name_ == "settings") {
            p->drawLine(4, 6, 20, 6); p->drawLine(4, 12, 20, 12); p->drawLine(4, 18, 20, 18);
            p->setBrush(c.surface); p->drawEllipse(QPointF(9, 6), 2, 2); p->drawEllipse(QPointF(16, 12), 2, 2); p->drawEllipse(QPointF(9, 18), 2, 2);
        } else if (name_ == "theme") {
            p->drawEllipse(QPointF(12, 12), 4, 4);
            for (int i = 0; i < 8; ++i) { p->drawLine(12, 2, 12, 4); p->translate(12, 12); p->rotate(45); p->translate(-12, -12); }
        } else if (name_ == "refresh") {
            p->drawArc(QRectF(5, 5, 14, 14), 30 * 16, 295 * 16); p->drawLine(19, 3, 19, 8); p->drawLine(14, 8, 19, 8);
        } else if (name_ == "search") {
            p->drawEllipse(QPointF(10, 10), 6, 6); p->drawLine(15, 15, 21, 21);
        } else if (name_ == "plus") {
            p->drawLine(12, 5, 12, 19); p->drawLine(5, 12, 19, 12);
        } else if (name_ == "terminal" || name_ == "brand") {
            QPolygonF arrow; arrow << QPointF(5, 7) << QPointF(10, 12) << QPointF(5, 17); p->drawPolyline(arrow); p->drawLine(13, 17, 19, 17);
        }
        p->restore();
    }
    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override {
        QPixmap result(size); result.fill(Qt::transparent); QPainter painter(&result);
        paint(&painter, QRect(QPoint(), size), mode, state); return result;
    }
private:
    QString name_;
};

QString readStyle(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) qFatal("Missing embedded style: %s", qPrintable(path));
    return QString::fromUtf8(file.readAll());
}
}

QIcon uiIcon(const QString &name) { return QIcon(new LineIcon(name)); }

void applyTheme(QApplication &application, bool dark) {
    // Explicit initialization also links resources into consumers of the static UI library (tests).
    static const bool resourcesInitialized = [] {
        Q_INIT_RESOURCE(styles);
        Q_INIT_RESOURCE(darkstyle);
        Q_INIT_RESOURCE(lightstyle);
        return true;
    }();
    Q_UNUSED(resourcesInitialized);
    if (application.property("lighterDark").isNull()) application.setStyle("Fusion");
    application.setProperty("lighterDark", dark);
    auto font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
#ifdef Q_OS_WIN
    font.setFamilies({"Segoe UI", "Microsoft YaHei UI"});
#else
    font.setFamilies({"Noto Sans", "Noto Sans CJK SC", "sans-serif"});
#endif
    font.setPixelSize(13); application.setFont(font);
    const auto c = themeColors(dark);
    QPalette palette;
    palette.setColor(QPalette::Window, c.window); palette.setColor(QPalette::WindowText, c.text);
    palette.setColor(QPalette::Base, c.surface); palette.setColor(QPalette::AlternateBase, c.subtle);
    palette.setColor(QPalette::Text, c.text); palette.setColor(QPalette::Button, c.surface);
    palette.setColor(QPalette::ButtonText, c.text); palette.setColor(QPalette::Highlight, c.selected);
    palette.setColor(QPalette::HighlightedText, c.accent); palette.setColor(QPalette::PlaceholderText, c.muted);
    palette.setColor(QPalette::ToolTipBase, c.surface); palette.setColor(QPalette::ToolTipText, c.text);
    palette.setColor(QPalette::Link, c.accent);
    for (const auto role : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText}) palette.setColor(QPalette::Disabled, role, c.muted);
    application.setPalette(palette);

    auto base = readStyle(dark ? ":/qdarkstyle/dark/darkstyle.qss" : ":/qdarkstyle/light/lightstyle.qss");
    // Recolor the upstream palette without modifying the vendored stylesheet.
    const QStringList source = dark
        ? QStringList{"#19232D", "#37414F", "#455364", "#54687A", "#60798B", "#788D9C", "#9DA9B5", "#DFE1E2", "#26486B", "#346792", "#1A72BB", "#259AE9"}
        : QStringList{"#FAFAFA", "#DAEDFF", "#D2D5D8", "#C0C4C8", "#B4B8BC", "#ACB1B6", "#9DA9B5", "#19232D", "#9FCBFF", "#73C7FF", "#37AEFE", "#788D9C"};
    const QList<QColor> target = {c.surface, c.hover, c.border, c.border, c.muted, c.muted, c.muted, c.text, c.selected, c.selected, c.accent, c.accent};
    for (int i = 0; i < source.size(); ++i) base.replace(source[i], QString("@upstream%1@").arg(i));
    for (int i = 0; i < source.size(); ++i) base.replace(QString("@upstream%1@").arg(i), target[i].name());
    auto custom = readStyle(":/styles/lighter.qss");
    const QMap<QString, QString> tokens = {
        {"window", c.window.name()}, {"surface", c.surface.name()}, {"subtle", c.subtle.name()},
        {"text", c.text.name()}, {"muted", c.muted.name()}, {"border", c.border.name()},
        {"hover", c.hover.name()}, {"selected", c.selected.name()}, {"accent", c.accent.name()},
        {"danger", c.danger.name()}, {"primary", dark ? "#21896f" : "#137c65"},
        {"primaryHover", dark ? "#279d7f" : "#106953"},
        {"dangerBg", dark ? "#3b252b" : "#fff0f0"}
    };
    for (auto i = tokens.cbegin(); i != tokens.cend(); ++i) custom.replace("@" + i.key() + "@", i.value());
    application.setStyleSheet(base + '\n' + custom);
    for (auto *widget : application.allWidgets()) {
        if (auto *button = qobject_cast<QAbstractButton *>(widget); button && button->property("iconName").isValid())
            button->setIcon(uiIcon(button->property("iconName").toString()));
        widget->update();
    }
}
