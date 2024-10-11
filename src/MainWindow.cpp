#include "MainWindow.hpp"

#define CONNECT_TO_IMAGE_LOADER(signal_slot_name)                              \
  connect(this, &MainWindow::signal_slot_name, imageLoader,                    \
          &ImageLoader::signal_slot_name, Qt::QueuedConnection);

MainWindow::MainWindow(const QString& path) : QMainWindow() {

  // Create a fixed-size QPixmap on startup
  QRect primaryScreenGeometry = QApplication::primaryScreen()->geometry();
  setGeometry(primaryScreenGeometry);

  m_preferences = new Preferences();
  connect(m_preferences, &Preferences::settingChangedSlideShowPeriod, this,
          &MainWindow::settingChangedSlideShowPeriod);
  connect(m_preferences, &Preferences::settingChangedSlideShowLoop, this,
          &MainWindow::settingChangedSlideShowLoop);
  connect(m_preferences, &Preferences::settingChangedBackgroundColor, this,
          &MainWindow::settingChangedBackgroundColor, Qt::QueuedConnection);
  connect(m_preferences, &Preferences::rawSettingChanged, this,
          &MainWindow::onRawSettingChanged, Qt::QueuedConnection);

  // Create the image loader and move it to a separate thread
  imageLoader = new ImageLoader;
  imageLoaderThread = new QThread;
  imageLoader->moveToThread(imageLoaderThread);

  // Connect signals and slots for image loading
  CONNECT_TO_IMAGE_LOADER(resetImageFilePaths);
  CONNECT_TO_IMAGE_LOADER(loadImage);
  CONNECT_TO_IMAGE_LOADER(goToStart);
  CONNECT_TO_IMAGE_LOADER(goBackward);
  CONNECT_TO_IMAGE_LOADER(previousImage);
  CONNECT_TO_IMAGE_LOADER(nextImage);
  CONNECT_TO_IMAGE_LOADER(goForward);
  CONNECT_TO_IMAGE_LOADER(deleteCurrentImage);
  CONNECT_TO_IMAGE_LOADER(changeSortOrder);
  CONNECT_TO_IMAGE_LOADER(changeSortBy);
  CONNECT_TO_IMAGE_LOADER(copyCurrentImageFullResToClipboard);
  CONNECT_TO_IMAGE_LOADER(slideShowNext);
  CONNECT_TO_IMAGE_LOADER(reloadCurrentImage);
  CONNECT_TO_IMAGE_LOADER(goToFirstImage);
  CONNECT_TO_IMAGE_LOADER(goToLastImage);

  connect(imageLoader, &ImageLoader::noMoreImagesLeft, this,
          &MainWindow::onNoMoreImagesLeft, Qt::QueuedConnection);
  connect(imageLoader, &ImageLoader::imageLoaded, this,
          &MainWindow::onImageLoaded, Qt::QueuedConnection);

  // Start the thread
  imageLoaderThread->start();

  // Create a menu bar
  //QMenuBar *menuBar = new QMenuBar(this);
  //setMenuBar(menuBar);

  // Create a "File" menu
  ;

  slideshowTimer = new QTimer(this);

  auto interval =
      m_preferences->get(Preferences::SETTING_SLIDESHOW_PERIOD, 2500).toInt();
  m_preferences->set(Preferences::SETTING_SLIDESHOW_PERIOD, interval);
  slideshowTimer->setInterval(interval);
  connect(slideshowTimer, &QTimer::timeout, this,
          &MainWindow::slideshowTimerCallback);

  m_slideshowLoop =
      m_preferences->get(Preferences::SETTING_SLIDESHOW_LOOP, false).toBool();

  //createSortByMenu(viewMenu);
  //createSortOrderMenu(viewMenu);

  // Create a imageViewer to display the image
  imageViewer = new ImageViewer(this);

  m_centralWidget = new QWidget(this);
  auto vstackLayout = new QVBoxLayout();
  vstackLayout->addWidget(imageViewer);
  m_centralWidget->setLayout(vstackLayout);

  auto savedColor = Preferences::get(Preferences::SETTING_BACKGROUND_COLOR,
                                     QRect(25, 25, 25, 255))
                        .toRect();
  auto r = savedColor.x();
  auto g = savedColor.y();
  auto b = savedColor.width();
  auto a = savedColor.height();
  settingChangedBackgroundColor(QColor(r, g, b, a));

  setCentralWidget(m_centralWidget);
  
  m_path = path;

  openImage();

}

void MainWindow::createSortOrderMenu(QMenu *viewMenu) {
  // Create the "Sort Order" menu
  QMenu *sortOrderMenu = viewMenu->addMenu(tr("Sort Order"));

  // Create action group
  QActionGroup *sortGroup = new QActionGroup(this);

  // Create actions for "Ascending" and "Descending"
  QAction *ascendingAction = new QAction(tr("Ascending"), this);
  QAction *descendingAction = new QAction(tr("Descending"), this);

  // Set checkable property and add actions to the group
  ascendingAction->setCheckable(true);
  descendingAction->setCheckable(true);
  sortGroup->addAction(ascendingAction);
  sortGroup->addAction(descendingAction);

  // Connect actions to slots
  connect(ascendingAction, &QAction::triggered, this,
          [this]() { emit changeSortOrder(SortOrder::ascending); });
  connect(descendingAction, &QAction::triggered, this,
          [this]() { emit changeSortOrder(SortOrder::descending); });

  // By default, set "Ascending" as checked
  ascendingAction->setChecked(true);

  // Add actions to the "Sort" menu
  sortOrderMenu->addAction(ascendingAction);
  sortOrderMenu->addAction(descendingAction);
}

void MainWindow::createSortByMenu(QMenu *viewMenu) {
  // Create the "Sort By" menu
  QMenu *sortByMenu = viewMenu->addMenu(tr("Sort By"));

  // Create action group
  QActionGroup *sortGroup = new QActionGroup(this);

  // Create actions for "Ascending" and "Descending"
  QAction *nameAction = new QAction(tr("Name"), this);
  QAction *sizeAction = new QAction(tr("Size"), this);
  QAction *dateModifiedAction = new QAction(tr("Date Modified"), this);

  // Set checkable property and add actions to the group
  nameAction->setCheckable(true);
  sizeAction->setCheckable(true);
  dateModifiedAction->setCheckable(true);
  sortGroup->addAction(nameAction);
  sortGroup->addAction(sizeAction);
  sortGroup->addAction(dateModifiedAction);

  // Connect actions to slots
  connect(nameAction, &QAction::triggered, this,
          [this]() { emit changeSortBy(SortBy::name); });
  connect(sizeAction, &QAction::triggered, this,
          [this]() { emit changeSortBy(SortBy::size); });
  connect(dateModifiedAction, &QAction::triggered, this,
          [this]() { emit changeSortBy(SortBy::date_modified); });

  // By default, set "Ascending" as checked
  nameAction->setChecked(true);

  // Add actions to the "Sort" menu
  sortByMenu->addAction(nameAction);
  sortByMenu->addAction(sizeAction);
  sortByMenu->addAction(dateModifiedAction);
}

void MainWindow::openImage() {
  // Open a file dialog to select an image
  QString fileFilter = "Images (*.png *.jpg *.jpeg *.heic *.nef "
                       "*.tiff *.webp)";

  QString previousOpenPath =
      m_preferences->get(Preferences::SETTING_PREVIOUS_OPEN_PATH, "")
          .toString();

  //QString imagePath = QFileDialog::getOpenFileName(
  //    this, "Open Image", previousOpenPath, fileFilter);
  //QString imagePath = "/home/marek/Downloads/IMG_4631.jpg";
  qDebug() << "m_path:" << m_path;
  if (!m_path.isEmpty()) {
    // Emit a signal to load the image in a separate thread

    emit resetImageFilePaths();
    emit loadImage(m_path);

    QFileInfo fileInfo(m_path);

    /// Save the open path
    m_preferences->set(Preferences::SETTING_PREVIOUS_OPEN_PATH, "/home/marek/Downloads");
                       //fileInfo.dir().absolutePath());
  }
}

void MainWindow::copyToClipboard() {
  emit copyCurrentImageFullResToClipboard();
}

void MainWindow::copyImagePathToClipboard() {
  QClipboard *clipboard = QGuiApplication::clipboard();
  clipboard->setText(m_currentFileInfo.absoluteFilePath());
}

QString getLastDestination() {
  QSettings settings("p-ranav", "ImageViewer");
  return settings.value("lastCopyDestination", QDir::homePath()).toString();
}

void setLastDestination(const QString &path) {
  QSettings settings("p-ranav", "ImageViewer");
  settings.setValue("lastCopyDestination", path);
}

void MainWindow::copyToLocation() {
  QFile sourceFile(m_currentFileInfo.absoluteFilePath());
  if (!sourceFile.open(QIODevice::ReadOnly)) {
    qWarning() << "Failed to open source file:" << sourceFile.errorString();
    return;
  }

  QString lastUsedDestination = getLastDestination();
  QDir candidateDir(lastUsedDestination);
  QFileInfo candidateFileInfo(candidateDir, m_currentFileInfo.fileName());
  auto candidateSaveLocation = candidateFileInfo.filePath();

  QString destinationFilePath = QFileDialog::getSaveFileName(
      this, tr("Save Image"), candidateSaveLocation,
      tr("Images (*.png *.jpg *.jpeg *.heic *.nef *.tiff *.webp);;All Files "
         "(*)"));
  if (!destinationFilePath.isEmpty()) {

    // Open the destination file for writing
    QFile destinationFile(destinationFilePath);
    if (!destinationFile.open(QIODevice::WriteOnly)) {
      qWarning() << "Failed to open destination file:"
                 << destinationFile.errorString();
      sourceFile.close(); // Close the source file before returning
      return;
    }

    // Copy the contents of the source file to the destination file
    QByteArray data = sourceFile.readAll();
    destinationFile.write(data);

    // Close both files
    sourceFile.close();
    destinationFile.close();

    setLastDestination(destinationFilePath);
  }
}

void MainWindow::onImageLoaded(const QFileInfo &fileInfo,
                               const QPixmap &imagePixmap,
                               const ImageInfo &imageInfo) {

  m_currentFileInfo = fileInfo;

  // Set the resized image to the QLabel
  imageViewer->setPixmap(imagePixmap, width() * getScaleFactor(),
                         height() * getScaleFactor());

  setWindowTitle(fileInfo.fileName() +
                 QString(" (%1 x %2) [%3]")
                     .arg(imageInfo.width)
                     .arg(imageInfo.height)
                     .arg(prettyPrintSize(fileInfo.size())));
}

void MainWindow::onNoMoreImagesLeft() {
  auto emptyImage = QImage(0, 0, QImage::Format_ARGB32);
  auto emptyPixmap = QPixmap::fromImage(emptyImage);
  imageViewer->setPixmap(emptyPixmap, 0, 0);
}

void MainWindow::confirmAndDeleteCurrentImage() {

  // Display a confirmation dialog
  QMessageBox::StandardButton reply = QMessageBox::question(
      this, "Delete Confirmation",
      "Do you really want to delete " + m_currentFileInfo.fileName(),
      QMessageBox::Yes | QMessageBox::No);

  grabKeyboard();
  setFocus();
  activateWindow();

  // Check the user's response
  if (reply == QMessageBox::Yes) {
    emit deleteCurrentImage(m_currentFileInfo);
  } else {
    // User clicked 'No' or closed the dialog
    qDebug() << "Deletion canceled.";
  }
}

void MainWindow::closeEvent(QCloseEvent *event) {
  // Clean up the thread when the main window is closed
  imageLoaderThread->quit();
  imageLoaderThread->wait();
  event->accept();
}

void MainWindow::resizeEvent(QResizeEvent *event) {
  auto desiredWidth = width() * getScaleFactor();
  auto desiredHeight = height() * getScaleFactor();
  imageViewer->resize(desiredWidth, desiredHeight);

  QMainWindow::resizeEvent(event);
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    // Handle left mouse button double click
    QPointF scenePos = imageViewer->mapToScene(event->pos());
    qDebug() << "Double click at scene coordinates:" << scenePos;

    auto desiredWidth = width() * getScaleFactor();
    auto desiredHeight = height() * getScaleFactor();
    imageViewer->resize(desiredWidth, desiredHeight);
  }

  // Call the base class implementation
  QMainWindow::mouseDoubleClickEvent(event);
}

void MainWindow::zoomIn() { imageViewer->zoomIn(); }

void MainWindow::zoomOut() { imageViewer->zoomOut(); }

void MainWindow::slideshowTimerCallback() {
  emit slideShowNext(imageViewer->pixmap(), m_slideshowLoop);
}

void MainWindow::startSlideshow() { slideshowTimer->start(); }

qreal MainWindow::getScaleFactor() const { return 1; }

void MainWindow::keyPressEvent(QKeyEvent *event) {

  if (slideshowTimer->isActive()) {
    slideshowTimer->stop();
    /// TODO: Add reachedEnd signal to stop slideshow
    /// when no more images left
    ///
    /// this can also be used to restart the slidehow from
    /// the start
  }

  switch (event->key()) {
  case Qt::Key_Up:
    /// std::cout << "UP\n";
    break;
  case Qt::Key_Down:
    /// std::cout << "DOWN\n";
    break;
  case Qt::Key_Right:
    emit nextImage(imageViewer->pixmap());
    break;
  case Qt::Key_Left:
    emit previousImage(imageViewer->pixmap());
    break;
  }
  /// QMainWindow::keyPressEvent(event);
}

void MainWindow::showPreferences() { m_preferences->show(); }

void MainWindow::settingChangedBackgroundColor(const QColor &color) {
  QString styleSheet =
      QString("background-color: %1; padding: 0px;").arg(color.name());
  m_centralWidget->setStyleSheet(styleSheet);
}

void MainWindow::settingChangedSlideShowPeriod() {
  slideshowTimer->setInterval(
      m_preferences->get(Preferences::SETTING_SLIDESHOW_PERIOD, 2500).toInt());
}

void MainWindow::settingChangedSlideShowLoop() {
  m_slideshowLoop =
      m_preferences->get(Preferences::SETTING_SLIDESHOW_LOOP, false).toBool();
}

void MainWindow::onRawSettingChanged() { emit reloadCurrentImage(); }