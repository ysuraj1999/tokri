#include "aboutwindow.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

AboutWindow::AboutWindow(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About Tokri"));
    setModal(true);
    setMinimumWidth(380);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);

    auto *title = new QLabel(tr("Tokri"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 6);
    title->setFont(titleFont);

    auto *version = new QLabel(
        tr("Version %1").arg(QCoreApplication::applicationVersion()),
        this);
    version->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto *tagline = new QLabel(
        tr("A desktop basket to drag and drop text, URLs, images, and files."),
        this);
    tagline->setWordWrap(true);

    auto *link = new QLabel(this);
    link->setText(
        tr("<a href=\"https://github.com/ysuraj1999/tokri\">"
           "github.com/ysuraj1999/tokri</a>"));
    link->setOpenExternalLinks(true);
    link->setTextInteractionFlags(Qt::TextBrowserInteraction);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);

    layout->addWidget(title);
    layout->addWidget(version);
    layout->addWidget(tagline);
    layout->addWidget(link);
    layout->addWidget(buttons);
}
