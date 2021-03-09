#include "filtered_view.hpp"
#include "addfilterdialog.hpp"
#include "filter_card.hpp"

namespace insitu {

/*
    Constructor / Destructor
*/
FilteredView::FilteredView(const ros::NodeHandle& parent_, QString _name, 
    QString _topic, QWidget * parent) : QWidget(parent)
{
    // Topic name selector
    topicBox = new QComboBox();
    topicBox->addItems(getTopicList());
    topicBox->setCurrentIndex(topicBox->findText(_topic));

    // buttons
    refreshTopicButton = new QPushButton(tr("Refresh"));
    addFilterButton = new QPushButton(tr("Add Filter"));
    rmFilterButton = new QPushButton(tr("Delete Filter"));

    // checkboxes
    showFilterPaneCheckBox = new QCheckBox(tr("Show Filter Pane"));
    showFilterPaneCheckBox->setChecked(true);
    republishCheckBox = new QCheckBox(tr("Republish"));

    // frame to preserve image aspect ratio
    imgFrame = new RosImageFrame();
    imgFrame->setFrameStyle(QFrame::Plain | QFrame::Box);
    imgFrame->setBackgroundRole(QPalette::Base);

    // label to hold image
    imgLabel = new QLabel(imgFrame);
    imgLabel->setBackgroundRole(QPalette::Base);
    imgLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    imgLabel->setScaledContents(true);

    // display statistics in lower status bar
    fpsLabel = new QLabel();
    fpsLabel->setText(tr("FPS: "));

    // panel for filters
    filterList = new QListWidget();
    filterList->setDragDropMode(QAbstractItemView::DragDrop);
    filterList->setDefaultDropAction(Qt::DropAction::MoveAction);

    // layout
    filterPaneLayout = new QGridLayout();
    filterPaneLayout->addWidget(addFilterButton, 0, 0);
    filterPaneLayout->addWidget(rmFilterButton, 0, 1);
    filterPaneLayout->addWidget(filterList, 1, 0, 1, 2);
    filterPaneWidget = new QWidget();
    filterPaneWidget->setLayout(filterPaneLayout);

    imagePane = new QHBoxLayout();
    imagePane->addWidget(imgFrame, 1);
    imagePane->addWidget(filterPaneWidget);

    layout = new QGridLayout();
    layout->addWidget(topicBox, 0, 0);
    layout->addWidget(refreshTopicButton, 0, 1);
    layout->addWidget(republishCheckBox, 0, 2);
    layout->addWidget(showFilterPaneCheckBox, 0, 3);
    layout->addLayout(imagePane, 1, 0, 1, 4);
    layout->addWidget(fpsLabel, 2, 0);
    layout->setColumnStretch(0, 1);
    layout->setRowStretch(1, 1);

    lastFrameTime = ros::Time::now();
    frames = 0;

    setLayout(layout);

    QObject::connect(topicBox, SIGNAL(currentIndexChanged(const QString&)),
                     SLOT(onTopicChange(const QString&)));
    QObject::connect(refreshTopicButton, SIGNAL(clicked()),
                     SLOT(refreshTopics()));
    QObject::connect(addFilterButton, SIGNAL(clicked()),
                     SLOT(openFilterDialog()));
    QObject::connect(rmFilterButton, SIGNAL(clicked()), SLOT(rmFilter()));
    QObject::connect(showFilterPaneCheckBox, SIGNAL(stateChanged(int)), 
                     SLOT(onToggleFilterPane()));
    QObject::connect(republishCheckBox, SIGNAL(stateChanged(int)),
                      SLOT(onToggleRepublish()));

    name = _name.toStdString();
    nh = new ros::NodeHandle(parent_, name);
    nh->setCallbackQueue(&viewQueue);
    spinner = new ros::AsyncSpinner(1, &viewQueue);
    spinner->start();

    onTopicChange(topicBox->currentText());
}

FilteredView::~FilteredView(void)
{
    sub.shutdown();
    spinner->stop();
    delete spinner;
    delete nh;
}

/*
    Slots
*/
void FilteredView::refreshTopics(void)
{
    QString save_topic = topicBox->currentText();
    topicBox->clear();
    topicBox->addItems(getTopicList());
    int idx = topicBox->findText(save_topic);
    idx = idx == -1 ? 0 : idx;
    topicBox->setCurrentIndex(idx);
}

void FilteredView::openFilterDialog(void)
{
    AddFilterDialog * afd = (AddFilterDialog *) getNamedWidget("addfilterdialog");
    afd->setActiveView(this);
    afd->open();
}

void FilteredView::onTopicChange(QString topic_transport)
{
    // qDebug("topic changed");

    // imgMat.release();

    QList<QString> l = topic_transport.split(" ");
    std::string topic = l.first().toStdString();
    std::string transport = l.length() == 2 ? l.last().toStdString() : "raw";

    if (!topic.empty()) {
        if(sub.getNumPublishers()) sub.shutdown();
        
        image_transport::ImageTransport it(*nh);
        image_transport::TransportHints hints(transport);

        sub = it.subscribe(topic, 1, &FilteredView::callbackImg, this, hints);
    } else {
        // TODO error message
    }

    // qDebug("Subscribed to topic %s / %s", sub.getTopic().c_str(), sub.getTransport().c_str());
}

void FilteredView::rmFilter(void)
{
    QListWidgetItem * item = filterList->currentItem();
    if (item == nullptr) return;
    FilterCard * fc = (FilterCard *) filterList->itemWidget(item);
    filters[fc->getFilterName()].reset();
    filters.erase(fc->getFilterName());

    AddFilterDialog * afd = (AddFilterDialog *) getNamedWidget("addfilterdialog");
    afd->unloadFilter(fc->getFilterName());

    delete fc;
    delete item;
}

void FilteredView::onToggleFilterPane(void)
{
    filterPaneWidget->setVisible(showFilterPaneCheckBox->isChecked());
}

void FilteredView::onToggleRepublish(void)
{
    if (republishCheckBox->isChecked()) {
        topicBox->setDisabled(true);
        image_transport::ImageTransport it(*nh);
        pub = it.advertise("republish", 1);
    } else {
        pub.shutdown();
        topicBox->setDisabled(false);
    }
}

/*
    Public Functions
*/
void FilteredView::addFilter(boost::shared_ptr<insitu::Filter> filter)
{
    std::string name = filter->name();

    filters[name] = filter;

    QListWidgetItem * item = new QListWidgetItem();
    FilterCard * fc = new FilterCard(name, filter);
    item->setSizeHint(fc->sizeHint());

    filterList->addItem(item);
    filterList->setItemWidget(item, fc);
}

const std::string & FilteredView::getViewName(void)
{
    return name;
}

const ros::NodeHandle& FilteredView::getNodeHandle(void)
{
    return *nh;
}

/*
    Private Functions
*/

void FilteredView::callbackImg(const sensor_msgs::Image::ConstPtr& msg)
{
    // qDebug("cb in");
    // track frames per second
    ros::Time now = ros::Time::now();
    ++frames;
    if (now - lastFrameTime > ros::Duration(1)) {
        fpsLabel->setText(QString("FPS: %1").arg(frames));
        frames = 0;
        lastFrameTime = now;
    }

    // convert sensor_msgs::Image to cv matrix
    cv_bridge::CvImageConstPtr cv_ptr;
    try {
        cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::RGB8);
    } catch (cv_bridge::Exception& e) {
        qWarning("Failed to convert image: %s", e.what());
        return;
    }
    imgMat = cv_ptr->image;

    // apply filters
    for (int i = filterList->count() - 1; i >= 0; --i) {
        // seems awfully verbose for what we're trying to do but at this point
        // I've just accepted that this is just how QT is designed
        QListWidgetItem * it = filterList->item(i);
        FilterCard * fc = (FilterCard *) filterList->itemWidget(it);
        imgMat = filters[fc->getFilterName()]->apply(imgMat);
    }

    // republish
    if (pub.getNumSubscribers() > 0) {
        pub.publish(cv_ptr->toImageMsg());
    }

    // convert cv matrix to qpixmap
    QImage image(imgMat.data, imgMat.cols, imgMat.rows, imgMat.step[0], 
                 QImage::Format_RGB888);
    
    imgFrame->setImage(image);
    // qDebug("cb out");
}

}
