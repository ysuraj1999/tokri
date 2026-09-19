#include "helpwindow.h"

#include <QFormLayout>
#include <QLabel>

namespace {

struct Shortcut {
    const char *action;
    const char *keys;
};

const Shortcut kShortcuts[] = {
#ifdef Q_OS_MACOS
    { "Open item",  "\u2318O" },
    { "Copy",       "\u2318C" },
    { "Paste",      "\u2318V" },
    { "Rename",     "\u21A9" },
    { "Delete",     "\u2318\u232B" },
    { "Select all", "\u2318A" },
    { "Hide basket", "Esc" },
#else
    { "Open item",  "Enter" },
    { "Copy",       "Ctrl+C" },
    { "Paste",      "Ctrl+V" },
    { "Rename",     "F2" },
    { "Delete",     "Delete" },
    { "Select all", "Ctrl+A" },
    { "Hide basket", "Esc" },
#endif
};

} // namespace

HelpWindow::HelpWindow(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Keyboard Shortcuts"));
    setModal(true);

    auto *layout = new QFormLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setHorizontalSpacing(32);
    layout->setVerticalSpacing(8);

    for (const Shortcut &shortcut : kShortcuts) {
        auto *action = new QLabel(tr(shortcut.action), this);
        auto *keys = new QLabel(tr(shortcut.keys), this);
        keys->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        QFont font = keys->font();
        font.setBold(true);
        keys->setFont(font);

        layout->addRow(action, keys);
    }
}
