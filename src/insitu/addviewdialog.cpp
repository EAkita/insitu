#include "addviewdialog.hpp"

namespace insitu {

AddViewDialog::AddViewDialog(QWidget * parent) : QDialog(parent)
{
    tabmanager = parent->findChild<QTabWidget *>("tabmanager");

    // View name text input
    nameEdit = new QLineEdit;

    // Mode name selector
    modeBox = new QComboBox();

    // Topic name selector
    topicBox = new QComboBox();

    // cancel button
    cancelButton = new QPushButton(tr("Cancel"));
    createButton = new QPushButton(tr("Create"));
    createButton->setDefault(true);

    // callbacks
    QObject::connect(createButton, SIGNAL(clicked()), SLOT(AddView()));
    QObject::connect(cancelButton, SIGNAL(clicked()), SLOT(reject()));

    // layout
    buttonHBox = new QHBoxLayout();
    buttonHBox->addWidget(cancelButton);
    buttonHBox->addWidget(createButton);

    form = new QFormLayout();
    form->addRow(tr("View Name"), nameEdit);
    form->addRow(tr("Add to Mode"), modeBox);
    form->addRow(tr("View Topic"), topicBox);
    form->addRow(buttonHBox);

    setLayout(form);

    setWindowTitle(tr("Add View"));
}

void AddViewDialog::AddView()
{
    if (modeBox->count() > 0) {
        QMdiArea * mdiarea = tabmanager->findChild<QMdiArea *>(modeBox->currentText());

        QString topic = nameEdit->text();

        FilteredView * view = new FilteredView(topic);
        view->setObjectName(topic);
        view->setWindowTitle(topic);
        mdiarea->addSubWindow(view);
        view->show();
        mdiarea->tileSubWindows();
    } else {
        // TODO, inform user mode box cannot be empty
    }
    accept();
}

void AddViewDialog::open()
{
    modeBox->clear();
    modeBox->addItems(getModeList());
    modeBox->setCurrentIndex(tabmanager->currentIndex());

    topicBox->clear();
    topicBox->addItems(getTopicList());

    QDialog::open();
}

QList<QString> AddViewDialog::getModeList()
{
    QList<QString> modes;
    int modenum = tabmanager->count();
    for (int i = 0; i < modenum; ++i) {
        modes.append(tabmanager->tabText(i));
    }
    return modes;
}

} // END NAMESPACE INSITU