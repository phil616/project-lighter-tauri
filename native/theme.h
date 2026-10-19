#pragma once
#include <QColor>
#include <QIcon>
class QApplication;
struct ThemeColors {
    QColor window, surface, subtle, text, muted, border, hover, selected, accent, danger;
};
ThemeColors themeColors(bool dark);
QIcon uiIcon(const QString &name);
void applyTheme(QApplication &application, bool dark);
