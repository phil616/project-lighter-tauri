#include "window.h"
#include "editor.h"
#include "theme.h"
#include "lighter/version.h"
#include <QtWidgets>
#include <QtConcurrent>
#include <QFutureWatcher>

namespace {
QLabel *label(const QString &text, const char *name = nullptr) {
    auto *widget = new QLabel(text); widget->setTextFormat(Qt::PlainText);
    if (name) widget->setObjectName(QString::fromLatin1(name));
    return widget;
}
QPushButton *button(const QString &text, const char *name = nullptr, const QString &icon = {}) {
    auto *widget = new QPushButton(text); widget->setCursor(Qt::PointingHandCursor);
    if (name) widget->setObjectName(QString::fromLatin1(name));
    if (!icon.isEmpty()) { widget->setProperty("iconName", icon); widget->setIcon(uiIcon(icon)); widget->setIconSize(QSize(18, 18)); }
    return widget;
}
constexpr int RunningRole = Qt::UserRole + 1;
constexpr int SummaryRole = Qt::UserRole + 2;
class ProjectDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override { return {190, 76}; }
    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        const auto c = themeColors(qApp->property("lighterDark").toBool());
        const bool selected = option.state & QStyle::State_Selected;
        const bool running = index.data(RunningRole).toBool();
        const auto rect = option.rect.adjusted(2, 3, -2, -3);
        p->save(); p->setClipRect(option.rect); p->setRenderHint(QPainter::Antialiasing);
        p->setPen(Qt::NoPen); p->setBrush(selected ? c.selected : option.state & QStyle::State_MouseOver ? c.hover : c.surface);
        p->drawRoundedRect(rect, 8, 8);
        if (option.state & QStyle::State_HasFocus) { p->setPen(QPen(c.accent, 1, Qt::DotLine)); p->setBrush(Qt::NoBrush); p->drawRoundedRect(rect.adjusted(1, 1, -1, -1), 8, 8); }
        const QRect tile(rect.x() + 10, rect.y() + 16, 34, 34);
        p->setPen(Qt::NoPen); p->setBrush(selected ? c.surface : c.subtle); p->drawRoundedRect(tile, 8, 8);
        auto font = option.font; font.setPixelSize(14); font.setWeight(QFont::DemiBold); p->setFont(font); p->setPen(selected ? c.accent : c.muted);
        p->drawText(tile, Qt::AlignCenter, index.data().toString().left(1).toUpper());
        const int x = tile.right() + 11, width = qMax(0, rect.right() - x - 12);
        p->setPen(c.text); font.setPixelSize(13); p->setFont(font);
        p->drawText(QRect(x, rect.y() + 12, width, 23), Qt::AlignVCenter, QFontMetrics(font).elidedText(index.data().toString(), Qt::ElideRight, width));
        font.setWeight(QFont::Normal); font.setPixelSize(11); p->setFont(font);
        p->setPen(Qt::NoPen); p->setBrush(running ? c.accent : c.muted); p->drawEllipse(QPointF(x + 3, rect.y() + 46), 3, 3);
        p->setPen(running ? c.accent : c.muted);
        const QString summary = (running ? QStringLiteral("运行中") : QStringLiteral("已停止")) + " · " + index.data(SummaryRole).toString();
        p->drawText(QRect(x + 12, rect.y() + 36, qMax(0, width - 12), 20), Qt::AlignVCenter, QFontMetrics(font).elidedText(summary, Qt::ElideRight, qMax(0, width - 12)));
        p->restore();
    }
};

void pageHeading(QVBoxLayout *layout, const QString &title, const QString &description) {
    layout->addWidget(label(title, "pageTitle")); layout->addWidget(label(description, "pageDescription")); layout->addSpacing(8);
}
QString styleCredits(bool dark) {
    return QStringLiteral("界面样式基于 <a style=\"color:%1\" href=\"https://github.com/ColinDuquesnoy/QDarkStyleSheet\">QDarkStyleSheet 3.2.3</a>。<br>Colin Duquesnoy、Daniel Cosmo Pizetta · 代码 MIT / 图像 <a style=\"color:%1\" href=\"https://creativecommons.org/licenses/by/4.0/\">CC BY 4.0</a>。").arg(themeColors(dark).accent.name());
}
}
Window::Window(QString configPath, QWidget *parent) : QMainWindow(parent), store_(std::move(configPath)), runner_(this), batch_(runner_, this) {
    setWindowTitle(QStringLiteral("Lighter · 项目启动器")); resize(1240, 820); setMinimumSize(920, 660);
    QSettings settings(store_.path() + ".ui.ini", QSettings::IniFormat);
    dark_ = settings.value("dark", false).toBool(); applyTheme(*qApp, dark_);
    restoreGeometry(settings.value("geometry").toByteArray());
    auto *root = new QWidget; auto *layout = new QHBoxLayout(root); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(0); setCentralWidget(root);
    auto *sidebar = new QFrame; sidebar->setObjectName("sidebar"); sidebar->setFixedWidth(176);
    auto *side = new QVBoxLayout(sidebar); side->setContentsMargins(14, 24, 14, 16); side->setSpacing(8);
    auto *brandRow = new QHBoxLayout; brandRow->setSpacing(10);
    auto *mark = label({}, "brandMark"); mark->setFixedSize(36, 36); mark->setAlignment(Qt::AlignCenter); mark->setPixmap(uiIcon("brand").pixmap(24, 24));
    brandRow->addWidget(mark); brandRow->addWidget(label("Lighter", "brand")); brandRow->addStretch(); side->addLayout(brandRow);
    side->addWidget(label(QStringLiteral("项目便捷启动器"), "pageDescription")); side->addSpacing(24);
    side->addWidget(label(QStringLiteral("工作空间"), "eyebrow"));
    auto *group = new QButtonGroup(this); group->setExclusive(true);
    pages_ = new QStackedWidget;
    const QStringList names = {QStringLiteral("项目工作台"), QStringLiteral("端口工具"), QStringLiteral("配置管理")};
    const QStringList icons = {"projects", "ports", "settings"};
    for (int i = 0; i < names.size(); ++i) {
        auto *nav = button(names[i], "nav", icons[i]); nav->setCheckable(true); group->addButton(nav, i); side->addWidget(nav);
        connect(nav, &QPushButton::clicked, this, [this, i] { pages_->setCurrentIndex(i); });
        if (!i) nav->setChecked(true);
    }
    connect(pages_, &QStackedWidget::currentChanged, this, [group](int index) { if (auto *nav = group->button(index)) nav->setChecked(true); });
    side->addStretch();
    side->addWidget(label(QStringLiteral("Ctrl+N  新建项目\nCtrl+F  快速搜索"), "pageDescription")); side->addSpacing(12);
    auto *theme = button(QStringLiteral("切换明暗主题"), "quiet", "theme"); side->addWidget(theme);
    connect(theme, &QPushButton::clicked, this, [this] {
        dark_ = !dark_; applyTheme(*qApp, dark_);
        findChild<QLabel *>("styleCredits")->setText(styleCredits(dark_));
        QSettings settings(store_.path() + ".ui.ini", QSettings::IniFormat); settings.setValue("dark", dark_);
    });
    auto *version = label(QStringLiteral("v%1").arg(LIGHTER_VERSION), "muted");
    version->setToolTip(QStringLiteral("Qt 6 · %1").arg(LIGHTER_GIT_COMMIT)); side->addWidget(version);
    layout->addWidget(sidebar); layout->addWidget(pages_, 1);
    pages_->addWidget(projectPage()); pages_->addWidget(portPage()); pages_->addWidget(settingsPage());
    statusBar()->addPermanentWidget(label(QStringLiteral("Lighter  ·  本地工作空间"), "muted"));
    auto *shortcut = new QShortcut(QKeySequence::New, this); connect(shortcut, &QShortcut::activated, this, [this] { editProject(true); });
    shortcut = new QShortcut(QKeySequence::Find, this); connect(shortcut, &QShortcut::activated, this, [this] { pages_->setCurrentIndex(0); search_->setFocus(); search_->selectAll(); });
    shortcut = new QShortcut(QKeySequence::Refresh, this); connect(shortcut, &QShortcut::activated, this, &Window::load);
    connect(&batch_, &BatchLauncher::changed, this, [this] {
        updateStats(); updateDetail();
        statusBar()->showMessage(batch_.busy() ? (batch_.stopping() ? QStringLiteral("取消启动…") : QStringLiteral("检查 / 释放端口…")) : QString());
        if (closing_ && !batch_.busy() && runner_.count() == 0) QTimer::singleShot(0, this, &QWidget::close);
    });
    connect(&batch_, &BatchLauncher::error, this, &Window::showError);
    connect(&batch_, &BatchLauncher::confirmationNeeded, this, [this](const QString &details) {
        batch_.confirmPorts(confirm(QStringLiteral("结束以下端口占用进程并启动所有项目？\n\n%1").arg(details)));
    });
    connect(&batch_, &BatchLauncher::projectStarting, this, [this](const QString &id) {
        if (logs_.contains(id)) logs_[id]->clear();
        renderers_[id].reset();
    });
    connect(&runner_, &Runner::output, this, &Window::appendOutput);
    connect(&runner_, &Runner::error, this, &Window::showError);
    connect(&runner_, &Runner::stateChanged, this, [this] {
        refreshList();
        if (closing_ && !batch_.busy() && runner_.count() == 0) QTimer::singleShot(0, this, &QWidget::close);
    });
    // Initial errors are shown only after the main window has entered the event loop.
    QTimer::singleShot(0, this, &Window::load);
}
QWidget *Window::projectPage() {
    auto *page = new QWidget; page->setObjectName("page"); auto *layout = new QVBoxLayout(page); layout->setContentsMargins(24, 22, 24, 18); layout->setSpacing(12);
    auto *head = new QHBoxLayout;
    auto *heading = new QVBoxLayout; heading->setSpacing(3); heading->addWidget(label(QStringLiteral("项目工作台"), "pageTitle")); heading->addWidget(label(QStringLiteral("由AltasCI出品的项目进程管理工具。"), "pageDescription"));
    head->addLayout(heading); head->addStretch();
    startupPortsButton_ = button(QStringLiteral("启动端口")); startupPortsButton_->setObjectName("startupPortsButton");
    head->addWidget(startupPortsButton_); connect(startupPortsButton_, &QPushButton::clicked, this, &Window::editStartupPorts);
    batchButton_ = button(QStringLiteral("一键启动"), "primary"); batchButton_->setObjectName("batchButton"); batchButton_->setProperty("primary", true); head->addWidget(batchButton_);
    connect(batchButton_, &QPushButton::clicked, this, [this] {
        if (batch_.busy() || runner_.count()) batch_.stop();
        else if (loaded_) batch_.start(config_);
    });
    new_ = button(QStringLiteral("新建项目"), nullptr, "plus"); new_->setToolTip("Ctrl+N"); head->addWidget(new_); layout->addLayout(head);
    connect(new_, &QPushButton::clicked, this, [this] { editProject(true); });
    auto *stats = new QHBoxLayout; stats->setSpacing(12);
    auto stat = [stats](const QString &caption, const char *tone) {
        auto *card = new QFrame; card->setObjectName("statCard"); auto *row = new QHBoxLayout(card); row->setContentsMargins(16, 10, 16, 10);
        row->addWidget(label(caption, "muted")); row->addStretch(); auto *value = label("0"); value->setProperty("stat", true); value->setProperty("tone", tone); row->addWidget(value); stats->addWidget(card, 1); return value;
    };
    total_ = stat(QStringLiteral("全部项目"), "total"); active_ = stat(QStringLiteral("运行中"), "active"); idle_ = stat(QStringLiteral("已停止"), "idle"); layout->addLayout(stats);
    auto *toolbar = new QHBoxLayout;
    search_ = new QLineEdit; search_->setObjectName("projectSearch"); search_->setPlaceholderText(QStringLiteral("搜索项目、命令或目录…")); search_->setToolTip(QStringLiteral("搜索项目、命令或目录（Ctrl+F）")); search_->setClearButtonEnabled(true); search_->addAction(uiIcon("search"), QLineEdit::LeadingPosition); toolbar->addWidget(search_, 1);
    filter_ = new QComboBox; filter_->setObjectName("projectFilter"); filter_->addItems({QStringLiteral("全部状态"), QStringLiteral("运行中"), QStringLiteral("已停止")}); toolbar->addWidget(filter_);
    auto *refresh = button(QStringLiteral("刷新"), nullptr, "refresh"); refresh->setToolTip("F5"); toolbar->addWidget(refresh); layout->addLayout(toolbar);
    connect(search_, &QLineEdit::textChanged, this, [this] { refreshList(); }); connect(filter_, &QComboBox::currentIndexChanged, this, [this] { refreshList(); }); connect(refresh, &QPushButton::clicked, this, &Window::load);
    auto *splitter = new QSplitter; splitter->setChildrenCollapsible(false); splitter->setHandleWidth(12);
    projects_ = new QListWidget; projects_->setObjectName("projectList"); projects_->setMinimumWidth(190); projects_->setItemDelegate(new ProjectDelegate(projects_)); projects_->setMouseTracking(true); projects_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); projects_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel); splitter->addWidget(projects_);
    detailStack_ = new QStackedWidget;
    auto *empty = new QFrame; empty->setObjectName("panel"); auto *emptyLayout = new QVBoxLayout(empty); emptyLayout->addStretch();
    auto *emptyMark = label(QStringLiteral("›_"), "emptyMark"); emptyMark->setFixedSize(64, 64); emptyMark->setAlignment(Qt::AlignCenter); emptyLayout->addWidget(emptyMark, 0, Qt::AlignHCenter); emptyLayout->addSpacing(12);
    auto *emptyTitle = label(QStringLiteral("暂无项目"), "emptyState"); emptyTitle->setAlignment(Qt::AlignCenter); emptyLayout->addWidget(emptyTitle);
    auto *emptyDescription = label(QStringLiteral("添加第一个项目，集中管理命令、脚本与运行日志。"), "emptyDescription"); emptyDescription->setWordWrap(true); emptyDescription->setAlignment(Qt::AlignCenter); emptyLayout->addWidget(emptyDescription);
    auto *emptyCreate = button(QStringLiteral("新建第一个项目"), "primary"); emptyCreate->setObjectName("emptyCreate"); emptyCreate->setProperty("primary", true); emptyCreate->setEnabled(false); emptyLayout->addSpacing(12); emptyLayout->addWidget(emptyCreate, 0, Qt::AlignHCenter); connect(emptyCreate, &QPushButton::clicked, this, [this] { editProject(true); });
    auto *reset = button(QStringLiteral("清除搜索与筛选")); reset->setObjectName("resetFilters"); reset->hide(); emptyLayout->addWidget(reset, 0, Qt::AlignHCenter); connect(reset, &QPushButton::clicked, this, [this] { search_->clear(); filter_->setCurrentIndex(0); });
    emptyLayout->addStretch(); detailStack_->addWidget(empty);
    auto *detail = new QWidget; detail->setObjectName("detail"); auto *content = new QVBoxLayout(detail); content->setContentsMargins(0, 0, 0, 0); content->setSpacing(12);
    auto *summary = new QFrame; summary->setObjectName("projectSummary"); auto *summaryLayout = new QVBoxLayout(summary); summaryLayout->setContentsMargins(16, 14, 16, 14); summaryLayout->setSpacing(8);
    auto *top = new QHBoxLayout; title_ = label({}, "sectionTitle"); title_->setWordWrap(true); top->addWidget(title_, 1); state_ = label({}, "badge"); top->addWidget(state_); summaryLayout->addLayout(top);
    directory_ = label({}, "muted"); directory_->setWordWrap(true); directory_->setTextInteractionFlags(Qt::TextSelectableByMouse); summaryLayout->addWidget(directory_);
    commands_ = label({}, "commandPreview"); commands_->setWordWrap(true); commands_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *commandScroll = new QScrollArea; commandScroll->setObjectName("commandScroll"); commandScroll->setWidgetResizable(true); commandScroll->setFrameShape(QFrame::NoFrame); commandScroll->setMaximumHeight(100); commandScroll->setWidget(commands_); summaryLayout->addWidget(commandScroll);
    auto *actions = new QHBoxLayout; start_ = button(QStringLiteral("▶ 启动"), "primary"); stop_ = button(QStringLiteral("■ 停止"), "danger"); edit_ = button(QStringLiteral("编辑")); remove_ = button(QStringLiteral("删除"));
    remove_->setProperty("danger", true);
    actions->addWidget(start_); actions->addWidget(stop_); actions->addStretch(); actions->addWidget(edit_); actions->addWidget(remove_); summaryLayout->addLayout(actions); content->addWidget(summary);
    connect(start_, &QPushButton::clicked, this, &Window::startProject); connect(stop_, &QPushButton::clicked, this, [this] { runner_.stop(selectedId()); });
    connect(edit_, &QPushButton::clicked, this, [this] { editProject(false); }); connect(remove_, &QPushButton::clicked, this, &Window::removeProject);
    auto *logPanel = new QFrame; logPanel->setObjectName("logPanel"); auto *logLayout = new QVBoxLayout(logPanel); logLayout->setContentsMargins(1, 1, 1, 1); logLayout->setSpacing(0);
    auto *logToolbar = new QWidget; logToolbar->setObjectName("logToolbar"); auto *logbar = new QHBoxLayout(logToolbar); logbar->setContentsMargins(14, 7, 10, 7); logbar->addWidget(label(QStringLiteral("运行日志"))); logbar->addStretch(); follow_ = new QCheckBox(QStringLiteral("自动滚动")); follow_->setChecked(true); logbar->addWidget(follow_);
    auto *clear = button(QStringLiteral("清空")); auto *exportLog = button(QStringLiteral("保存日志")); logbar->addWidget(clear); logbar->addWidget(exportLog); logLayout->addWidget(logToolbar);
    terminal_ = new QTextEdit; terminal_->setObjectName("terminal"); terminal_->setReadOnly(true); terminal_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    terminal_->setPlaceholderText(QStringLiteral("等待项目启动…\n运行输出将在这里实时显示。"));
    emptyLog_ = new QTextDocument(this); terminal_->setDocument(emptyLog_); logLayout->addWidget(terminal_, 1); content->addWidget(logPanel, 1);
    connect(clear, &QPushButton::clicked, this, [this] { const auto id = selectedId(); if (logs_.contains(id)) logs_[id]->clear(); renderers_[id].reset(); });
    connect(exportLog, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getSaveFileName(this, QStringLiteral("保存日志"), "project-output.txt", "Text (*.txt)");
        if (path.isEmpty()) return;
        try { ConfigStore::write(path, terminal_->toPlainText().toUtf8()); statusBar()->showMessage(QStringLiteral("日志已保存"), 4000); } catch (const std::exception &e) { showError(QString::fromUtf8(e.what())); }
    });
    detailStack_->addWidget(detail); splitter->addWidget(detailStack_); splitter->setStretchFactor(0, 1); splitter->setStretchFactor(1, 3); splitter->setSizes({250, 650}); layout->addWidget(splitter, 1);
    connect(projects_, &QListWidget::currentItemChanged, this, [this] { updateDetail(); });
    connect(projects_, &QListWidget::itemDoubleClicked, this, [this] { startProject(); });
    return page;
}
QWidget *Window::portPage() {
    auto *page = new QWidget; page->setObjectName("page"); auto *layout = new QVBoxLayout(page); layout->setContentsMargins(24, 22, 24, 18); layout->setSpacing(12);
    pageHeading(layout, QStringLiteral("端口工具"), QStringLiteral("查看 TCP 端口占用，快速定位启动冲突。"));
    auto *panel = new QFrame; panel->setObjectName("panel"); auto *box = new QVBoxLayout(panel); box->setContentsMargins(24, 22, 24, 22); box->setSpacing(18);
    box->addWidget(label(QStringLiteral("查询端口占用"), "sectionTitle"));
    auto *hint = label(QStringLiteral("输入端口号，查看占用它的进程名称与 PID。"), "muted"); hint->setWordWrap(true); box->addWidget(hint);
    auto *row = new QHBoxLayout;
    port_ = new QSpinBox; port_->setRange(1, 65535); port_->setValue(8080); port_->setPrefix("TCP  "); row->addWidget(port_, 1);
    portQuery_ = button(QStringLiteral("查询端口"), "primary"); portKill_ = button(QStringLiteral("结束占用进程"), "danger"); portKill_->setEnabled(false); row->addWidget(portQuery_); row->addWidget(portKill_); box->addLayout(row);
    portResult_ = label(QStringLiteral("等待查询 · 支持 TCP / IPv4 / IPv6"), "muted"); portResult_->setMinimumHeight(70); portResult_->setWordWrap(true); portResult_->setTextInteractionFlags(Qt::TextSelectableByMouse); box->addWidget(portResult_); layout->addWidget(panel); layout->addStretch();
    connect(portQuery_, &QPushButton::clicked, this, &Window::queryPort); connect(portKill_, &QPushButton::clicked, this, &Window::killPort);
    connect(port_, &QSpinBox::valueChanged, this, [this] { portKill_->setEnabled(false); });
    auto *enter = new QShortcut(QKeySequence(Qt::Key_Return), port_); enter->setContext(Qt::WidgetWithChildrenShortcut); connect(enter, &QShortcut::activated, this, &Window::queryPort);
    return page;
}
QWidget *Window::settingsPage() {
    auto *page = new QWidget; page->setObjectName("page"); auto *layout = new QVBoxLayout(page); layout->setContentsMargins(24, 22, 24, 18); layout->setSpacing(12);
    pageHeading(layout, QStringLiteral("配置管理"), QStringLiteral("备份、迁移和维护你的项目工作空间。"));
    auto *panel = new QFrame; panel->setObjectName("panel"); auto *box = new QVBoxLayout(panel); box->setContentsMargins(24, 22, 24, 22); box->setSpacing(16);
    box->addWidget(label(QStringLiteral("配置文件"), "sectionTitle"));
    auto *path = label(store_.path(), "muted"); path->setWordWrap(true); path->setTextInteractionFlags(Qt::TextSelectableByMouse); box->addWidget(path);
    path->setToolTip(QStringLiteral("首次保存备份：%1.pre-qt.bak").arg(store_.path()));
    auto *backup = label(QStringLiteral("首次保存会自动备份原文件。导出配置可用于备份或迁移到其他设备。"), "muted"); backup->setWordWrap(true); box->addWidget(backup);
    auto *row = new QHBoxLayout; auto *import = button(QStringLiteral("导入配置"), "primary"), *exportButton = button(QStringLiteral("导出配置")), *json = button(QStringLiteral("编辑 JSON"));
    row->addWidget(import); row->addWidget(exportButton); row->addWidget(json); row->addStretch(); box->addLayout(row); layout->addWidget(panel);
    connect(import, &QPushButton::clicked, this, &Window::importConfig); connect(exportButton, &QPushButton::clicked, this, &Window::exportConfig); connect(json, &QPushButton::clicked, this, &Window::editJson);
    auto *about = new QFrame; about->setObjectName("panel"); auto *aboutLayout = new QVBoxLayout(about); aboutLayout->setContentsMargins(24, 22, 24, 22); aboutLayout->setSpacing(12);
    aboutLayout->addWidget(label(QStringLiteral("关于 Lighter"), "sectionTitle"));
    aboutLayout->addWidget(label(QStringLiteral("轻量的本地项目启动器 · v%1").arg(LIGHTER_VERSION), "muted"));
    auto *credits = new QLabel(styleCredits(dark_)); credits->setObjectName("styleCredits"); credits->setWordWrap(true); credits->setOpenExternalLinks(true); aboutLayout->addWidget(credits);
    auto *github = button(QStringLiteral("在 GitHub 查看项目"), "quiet"); aboutLayout->addWidget(github, 0, Qt::AlignLeft); layout->addWidget(about);
    connect(github, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl("https://github.com/phil616/project-lighter-tauri")); });
    layout->addStretch(); return page;
}
QString Window::selectedId() const { return projects_->currentItem() ? projects_->currentItem()->data(Qt::UserRole).toString() : QString(); }
const Project *Window::selected() const { const auto id = selectedId(); for (const auto &p : config_.projects) if (p.id == id) return &p; return nullptr; }
void Window::updateStats() {
    total_->setText(QString::number(config_.projects.size())); active_->setText(QString::number(runner_.count()));
    int stopped = 0; for (const auto &p : config_.projects) if (!runner_.running(p.id)) ++stopped;
    idle_->setText(QString::number(stopped)); new_->setEnabled(loaded_ && !batch_.busy());
    findChild<QPushButton *>("emptyCreate")->setEnabled(loaded_ && !batch_.busy());
    batchButton_->setText(batch_.busy() || runner_.count() ? QStringLiteral("一键关闭") : QStringLiteral("一键启动"));
    batchButton_->setEnabled(!batch_.stopping() && (batch_.busy() || runner_.count() || (loaded_ && !config_.projects.isEmpty())));
    startupPortsButton_->setEnabled(loaded_ && !batch_.busy());
    startupPortsButton_->setText(QStringLiteral("启动端口 (%1)").arg(config_.startupPorts.size()));
}
void Window::refreshList(const QString &select) {
    const auto old = select.isEmpty() ? selectedId() : select;
    QSignalBlocker blocker(projects_); projects_->clear();
    for (const auto &p : config_.projects) {
        QString haystack = p.name + " " + p.workingDir + " " + p.script;
        for (const auto &c : p.commands) haystack += " " + c.name + " " + c.command;
        if (!haystack.contains(search_->text(), Qt::CaseInsensitive)) continue;
        const bool running = runner_.running(p.id);
        if ((filter_->currentIndex() == 1 && !running) || (filter_->currentIndex() == 2 && running)) continue;
        auto *item = new QListWidgetItem(p.name, projects_);
        const auto summary = p.type == "script" ? QStringLiteral("脚本") : QStringLiteral("%1 个步骤").arg(p.commands.size());
        item->setData(RunningRole, running); item->setData(SummaryRole, summary);
        item->setData(Qt::AccessibleTextRole, p.name + " · " + (running ? QStringLiteral("运行中") : QStringLiteral("已停止")) + " · " + summary);
        item->setData(Qt::UserRole, p.id); item->setToolTip(p.workingDir);
        if (p.id == old) projects_->setCurrentItem(item);
    }
    if (!projects_->currentItem() && projects_->count()) projects_->setCurrentRow(0);
    updateStats(); updateDetail();
    QSet<QString> retained;
    for (const auto &project : config_.projects) retained.insert(project.id);
    for (const auto &id : logs_.keys()) {
        if (!retained.contains(id) && !runner_.running(id)) {
            auto *document = logs_.take(id);
            if (terminal_->document() == document) terminal_->setDocument(emptyLog_);
            document->deleteLater(); renderers_.remove(id);
        }
    }
}
void Window::updateDetail() {
    const auto *p = selected(); detailStack_->setCurrentIndex(p ? 1 : 0);
    if (!p) {
        const bool empty = config_.projects.isEmpty();
        findChild<QLabel *>("emptyState")->setText(empty ? QStringLiteral("从第一个项目开始") : QStringLiteral("没有找到匹配的项目"));
        findChild<QLabel *>("emptyDescription")->setText(empty ? QStringLiteral("添加项目的命令或脚本，集中查看每一次运行的输出。") : QStringLiteral("试试其他关键词，或清除当前的搜索与状态筛选。"));
        findChild<QPushButton *>("emptyCreate")->setVisible(empty);
        findChild<QPushButton *>("resetFilters")->setVisible(!empty);
        terminal_->setDocument(emptyLog_); return;
    }
    title_->setText(p->name); directory_->setText(p->workingDir.isEmpty() ? QDir::currentPath() : p->workingDir);
    QStringList commands; for (int i = 0; i < p->commands.size(); ++i) commands << QStringLiteral("%1  %2  ›  %3").arg(i + 1).arg(p->commands[i].name, p->commands[i].command);
    commands_->setText(p->type == "script" ? p->script : commands.join('\n'));
    findChild<QScrollArea *>("commandScroll")->setFixedHeight(qBound(42, (p->type == "script" ? int(p->script.count('\n')) + 1 : int(commands.size())) * 30 + 12, 100));
    const bool running = runner_.running(p->id);
    state_->setText(running ? QStringLiteral("● 运行中") : QStringLiteral("○ 已停止"));
    if (state_->property("running").toBool() != running) { state_->setProperty("running", running); state_->style()->unpolish(state_); state_->style()->polish(state_); }
    start_->setEnabled(!running && !batch_.busy()); stop_->setEnabled(running);
    edit_->setEnabled(!running && !batch_.busy()); remove_->setEnabled(!batch_.busy()); edit_->setToolTip(running ? QStringLiteral("停止项目后可编辑") : QString());
    auto *document = logs_.value(p->id, emptyLog_); if (terminal_->document() != document) terminal_->setDocument(document);
}
void Window::load() {
    if (batch_.busy()) return;
    try {
        const auto config = store_.load();
        if (runner_.count() && config.json() != config_.json()) {
            if (!confirm(QStringLiteral("配置已变化。重新加载会停止当前运行的项目，是否继续？"))) return;
            runner_.stopAll();
        }
        config_ = config; loaded_ = true; refreshList(); statusBar()->showMessage(QStringLiteral("配置已加载"), 3000);
    } catch (const std::exception &e) { showError(QStringLiteral("加载配置失败：%1\n可在配置管理中修复 JSON 或导入配置。原文件未修改。").arg(QString::fromUtf8(e.what()))); }
}
bool Window::save(const Config &config, bool stopRunning) {
    if (batch_.busy()) { showError(QStringLiteral("请先取消一键启动。")); return false; }
    try {
        store_.save(config);
        if (stopRunning) runner_.stopAll();
        config_ = config; loaded_ = true; refreshList(); statusBar()->showMessage(QStringLiteral("配置已保存"), 4000); return true;
    } catch (const std::exception &e) { showError(QString::fromUtf8(e.what())); return false; }
}
void Window::editProject(bool create) {
    if (batch_.busy()) return;
    if (!loaded_) { showError(QStringLiteral("请先修复或导入有效配置。")); return; }
    const auto *p = selected(); if (!create && (!p || runner_.running(p->id))) return;
    ProjectEditor editor(create ? Project{} : *p, this);
    if (editor.exec() != QDialog::Accepted) return;
    const auto project = editor.project(); auto config = config_; bool found = false;
    for (auto &item : config.projects) if (item.id == project.id) { item = project; found = true; break; }
    if (!found) config.projects.append(project);
    if (save(config, false)) { search_->clear(); filter_->setCurrentIndex(0); refreshList(project.id); }
}
void Window::removeProject() {
    if (batch_.busy()) return;
    const auto *p = selected(); if (!p) return; const auto id = p->id;
    if (!confirm(QStringLiteral("删除项目“%1”？运行中的项目会停止，项目目录和文件不会删除。").arg(p->name))) return;
    auto config = config_; for (int i = 0; i < config.projects.size(); ++i) if (config.projects[i].id == id) { config.projects.removeAt(i); break; }
    if (save(config, false)) runner_.stop(id);
}
void Window::startProject() {
    if (batch_.busy()) return;
    const auto *p = selected(); if (!p || runner_.running(p->id)) return;
    const auto project = *p; if (logs_.contains(project.id)) logs_[project.id]->clear(); renderers_[project.id].reset(); runner_.start(project);
}
void Window::appendOutput(const QString &id, const QString &text) {
    if (!logs_.contains(id)) { auto *document = new QTextDocument(this); document->setMaximumBlockCount(10000); document->setDefaultFont(QFontDatabase::systemFont(QFontDatabase::FixedFont)); logs_.insert(id, document); }
    renderers_[id].append(logs_[id], text);
    if (selectedId() == id) {
        if (terminal_->document() != logs_[id]) terminal_->setDocument(logs_[id]);
        if (follow_->isChecked()) terminal_->verticalScrollBar()->setValue(terminal_->verticalScrollBar()->maximum());
    }
}
void Window::editStartupPorts() {
    if (!loaded_ || batch_.busy()) return;
    QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("启动端口")); dialog.resize(400, 360);
    auto *layout = new QVBoxLayout(&dialog); layout->setContentsMargins(24, 24, 24, 24); layout->setSpacing(14);
    auto *hint = label(QStringLiteral("一键启动前检查并释放这些 TCP 端口。"), "muted"); layout->addWidget(hint);
    auto *list = new QListWidget; list->setObjectName("startupPortList"); layout->addWidget(list);
    for (const auto port : config_.startupPorts) new QListWidgetItem(QString::number(port), list);
    auto *row = new QHBoxLayout; auto *port = new QSpinBox; port->setRange(1, 65535); port->setValue(3000); row->addWidget(port);
    auto *add = button(QStringLiteral("添加")); auto *remove = button(QStringLiteral("移除")); row->addWidget(add); row->addWidget(remove); layout->addLayout(row);
    connect(add, &QPushButton::clicked, &dialog, [list, port] {
        const auto text = QString::number(port->value()); if (list->findItems(text, Qt::MatchExactly).isEmpty()) new QListWidgetItem(text, list);
    });
    connect(remove, &QPushButton::clicked, &dialog, [list] { delete list->takeItem(list->currentRow()); });
    auto *buttons = new QDialogButtonBox; buttons->addButton(QStringLiteral("保存"), QDialogButtonBox::AcceptRole)->setObjectName("primary"); buttons->addButton(QStringLiteral("取消"), QDialogButtonBox::RejectRole); layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&, this] {
        auto config = config_; config.startupPorts.clear();
        for (int i = 0; i < list->count(); ++i) config.startupPorts.append(list->item(i)->text().toUShort());
        if (save(config, false)) dialog.accept();
    }); dialog.exec();
}
void Window::editJson() {
    QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("编辑 JSON 配置")); dialog.resize(820, 640);
    auto *layout = new QVBoxLayout(&dialog); layout->setContentsMargins(24, 24, 24, 24); layout->setSpacing(14); auto *editor = new QPlainTextEdit;
    editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont)); editor->setPlainText(QString::fromUtf8(config_.json()));
    if (!loaded_) { QFile file(store_.path()); if (file.open(QIODevice::ReadOnly)) editor->setPlainText(QString::fromUtf8(file.readAll())); }
    layout->addWidget(editor); auto *error = label({}, "error"); error->setWordWrap(true); layout->addWidget(error);
    auto *buttons = new QDialogButtonBox; buttons->addButton(QStringLiteral("取消"), QDialogButtonBox::RejectRole); buttons->addButton(QStringLiteral("保存配置"), QDialogButtonBox::AcceptRole)->setObjectName("primary"); layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&, this] {
        try {
            const auto config = Config::parse(editor->toPlainText().toUtf8());
            if (runner_.count() && !confirm(QStringLiteral("保存配置会停止所有运行中的项目，是否继续？"))) return;
            if (save(config, true)) dialog.accept();
        } catch (const std::exception &e) { error->setText(QString::fromUtf8(e.what())); }
    }); dialog.exec();
}
void Window::importConfig() {
    const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("导入配置"), {}, "JSON (*.json)"); if (path.isEmpty()) return;
    try {
        QFile file(path); if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error(file.errorString().toStdString());
        const auto config = Config::parse(file.readAll());
        if (confirm(QStringLiteral("导入将替换当前配置并停止运行中的项目。是否继续？"))) save(config, true);
    } catch (const std::exception &e) { showError(QString::fromUtf8(e.what())); }
}
void Window::exportConfig() {
    if (!loaded_) { showError(QStringLiteral("当前配置无效，请先修复。")); return; }
    const auto path = QFileDialog::getSaveFileName(this, QStringLiteral("导出配置"), "project-config.json", "JSON (*.json)"); if (path.isEmpty()) return;
    try { ConfigStore::write(path, config_.json()); statusBar()->showMessage(QStringLiteral("配置已导出"), 4000); } catch (const std::exception &e) { showError(QString::fromUtf8(e.what())); }
}
void Window::queryPort() {
    if (!portQuery_->isEnabled()) return;
    queriedPort_ = quint16(port_->value()); port_->setEnabled(false); portQuery_->setEnabled(false); portKill_->setEnabled(false); portResult_->setText(QStringLiteral("正在查询…"));
    auto *watcher = new QFutureWatcher<PortResult>(this);
    connect(watcher, &QFutureWatcher<PortResult>::finished, this, [this, watcher] {
        lastPort_ = watcher->result(); watcher->deleteLater(); port_->setEnabled(true); portQuery_->setEnabled(true);
        if (!lastPort_.error.isEmpty()) { portResult_->setText(lastPort_.error); return; }
        QStringList lines;
        for (const auto &owner : lastPort_.owners) lines << QStringLiteral("%1  ·  PID %2").arg(owner.name).arg(owner.pid);
        portResult_->setText(lastPort_.occupied ? QStringLiteral("端口 %1 正在使用\n%2").arg(queriedPort_).arg(lines.isEmpty() ? QStringLiteral("无法读取进程信息，可能需要更高权限。") : lines.join('\n')) : QStringLiteral("端口 %1 空闲").arg(queriedPort_));
        portKill_->setEnabled(!lastPort_.owners.isEmpty());
    });
    watcher->setFuture(QtConcurrent::run([port = queriedPort_] { return Ports::query(port); }));
}
void Window::killPort() {
    if (quint16(port_->value()) != queriedPort_ || lastPort_.owners.isEmpty()) return;
    if (!confirm(QStringLiteral("结束端口 %1 上列出的进程？未保存的数据可能丢失。").arg(queriedPort_))) return;
    port_->setEnabled(false); portQuery_->setEnabled(false); portKill_->setEnabled(false);
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher] {
        const auto error = watcher->result(); watcher->deleteLater(); port_->setEnabled(true); portQuery_->setEnabled(true);
        if (!error.isEmpty()) { portResult_->setText(error); return; }
        statusBar()->showMessage(QStringLiteral("已结束占用进程"), 5000); queryPort();
    }); watcher->setFuture(QtConcurrent::run([port = queriedPort_, owners = lastPort_.owners] { return Ports::terminate(port, owners); }));
}
bool Window::confirm(const QString &text) { return QMessageBox::question(this, QStringLiteral("请确认"), text, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes; }
void Window::showError(const QString &message) { statusBar()->showMessage(message, 10000); QMessageBox::warning(this, QStringLiteral("操作未完成"), message); }
void Window::closeEvent(QCloseEvent *event) {
    if (runner_.count() || batch_.busy()) {
        event->ignore();
        if (!closing_ && confirm(QStringLiteral("关闭并停止所有项目？待执行的启动操作也会取消。"))) { closing_ = true; setEnabled(false); batch_.stop(); }
        return;
    }
    QSettings settings(store_.path() + ".ui.ini", QSettings::IniFormat); settings.setValue("geometry", saveGeometry()); event->accept();
}
