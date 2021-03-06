

#include "mainwindow.h"

// Tnz6 includes
#include "menubar.h"
#include "menubarcommandids.h"
#include "xsheetviewer.h"
#include "viewerpane.h"
#include "flipbook.h"
#include "messagepanel.h"
#include "iocommand.h"
#include "tapp.h"
#include "viewerpane.h"
#include "startuppopup.h"
#include "statusbar.h"
#include "aboutpopup.h"

// TnzTools includes
#include "tools/toolcommandids.h"
#include "tools/toolhandle.h"

// TnzQt includes
#include "toonzqt/gutil.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/viewcommandids.h"
#include "toonzqt/updatechecker.h"
#include "toonzqt/paletteviewer.h"

// TnzLib includes
#include "toonz/toonzfolders.h"
#include "toonz/stage2.h"
#include "toonz/stylemanager.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/txshleveltypes.h"
#include "toonz/tproject.h"

// TnzBase includes
#include "tenv.h"

// TnzCore includes
#include "tsystem.h"
#include "timagecache.h"
#include "tthread.h"

// Qt includes
#include <QStackedWidget>
#include <QSettings>
#include <QApplication>
#include <QGLPixelBuffer>
#include <QDebug>
#include <QDesktopServices>
#include <QButtonGroup>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>

TEnv::IntVar ViewCameraToggleAction("ViewCameraToggleAction", 1);
TEnv::IntVar ViewTableToggleAction("ViewTableToggleAction", 0);
TEnv::IntVar FieldGuideToggleAction("FieldGuideToggleAction", 0);
TEnv::IntVar ViewBBoxToggleAction("ViewBBoxToggleAction1", 1);
TEnv::IntVar EditInPlaceToggleAction("EditInPlaceToggleAction", 0);
TEnv::IntVar RasterizePliToggleAction("RasterizePliToggleAction", 0);
TEnv::IntVar SafeAreaToggleAction("SafeAreaToggleAction", 0);
TEnv::IntVar ViewColorcardToggleAction("ViewColorcardToggleAction", 1);
TEnv::IntVar ViewGuideToggleAction("ViewGuideToggleAction", 1);
TEnv::IntVar ViewRulerToggleAction("ViewRulerToggleAction", 1);
TEnv::IntVar TCheckToggleAction("TCheckToggleAction", 0);
TEnv::IntVar ICheckToggleAction("ICheckToggleAction", 0);
TEnv::IntVar Ink1CheckToggleAction("Ink1CheckToggleAction", 0);
TEnv::IntVar PCheckToggleAction("PCheckToggleAction", 0);
TEnv::IntVar IOnlyToggleAction("IOnlyToggleAction", 0);
TEnv::IntVar BCheckToggleAction("BCheckToggleAction", 0);
TEnv::IntVar GCheckToggleAction("GCheckToggleAction", 0);
TEnv::IntVar ACheckToggleAction("ACheckToggleAction", 0);
TEnv::IntVar LinkToggleAction("LinkToggleAction", 0);
TEnv::IntVar ShowStatusBarAction("ShowStatusBarAction", 1);
// TEnv::IntVar DockingCheckToggleAction("DockingCheckToggleAction", 1);
TEnv::IntVar ShiftTraceToggleAction("ShiftTraceToggleAction", 0);
TEnv::IntVar EditShiftToggleAction("EditShiftToggleAction", 0);
TEnv::IntVar NoShiftToggleAction("NoShiftToggleAction", 0);
TEnv::IntVar TouchGestureControl("TouchGestureControl", 0);
TEnv::IntVar TransparencySliderValue("TransparencySliderValue", 50);

//=============================================================================
namespace {
//=============================================================================

// layout file name may be overwritten by the argument
std::string layoutsFileName           = "layouts.txt";
const std::string currentRoomFileName = "currentRoom.txt";
bool scrambledRooms                   = false;

//=============================================================================

bool readRoomList(std::vector<TFilePath> &roomPaths,
                  const QString &argumentLayoutFileName) {
  bool argumentLayoutFileLoaded = false;

  TFilePath fp;
  /*-レイアウトファイルが指定されている場合--*/
  if (!argumentLayoutFileName.isEmpty()) {
    fp = ToonzFolder::getRoomsFile(argumentLayoutFileName.toStdString());
    if (!TFileStatus(fp).doesExist()) {
      DVGui::warning("Room layout file " + argumentLayoutFileName +
                     " not found!");
      fp = ToonzFolder::getRoomsFile(layoutsFileName);
      if (!TFileStatus(fp).doesExist()) return false;
    } else {
      argumentLayoutFileLoaded = true;
      layoutsFileName          = argumentLayoutFileName.toStdString();
    }
  } else {
    fp = ToonzFolder::getRoomsFile(layoutsFileName);
    if (!TFileStatus(fp).doesExist()) return false;
  }

  Tifstream is(fp);
  for (;;) {
    char buffer[1024];
    is.getline(buffer, sizeof(buffer));
    if (is.eof()) break;
    char *s = buffer;
    while (*s == ' ' || *s == '\t') s++;
    char *t = s;
    while (*t && *t != '\r' && *t != '\n') t++;
    while (t > s && (t[-1] == ' ' || t[-1] == '\t')) t--;
    t[0] = '\0';
    if (s[0] == '\0') continue;
    TFilePath roomPath = fp.getParentDir() + s;
    roomPaths.push_back(roomPath);
  }

  return argumentLayoutFileLoaded;
}

//-----------------------------------------------------------------------------

void writeRoomList(std::vector<TFilePath> &roomPaths) {
  TFilePath fp = ToonzFolder::getMyRoomsDir() + layoutsFileName;
  TSystem::touchParentDir(fp);
  Tofstream os(fp);
  if (!os) return;
  for (int i = 0; i < (int)roomPaths.size(); i++) {
    TFilePath roomPath = roomPaths[i];
    assert(roomPath.getParentDir() == fp.getParentDir());
    os << roomPath.withoutParentDir() << "\n";
  }
}

//-----------------------------------------------------------------------------

void writeRoomList(std::vector<Room *> &rooms) {
  std::vector<TFilePath> roomPaths;
  for (int i = 0; i < (int)rooms.size(); i++)
    roomPaths.push_back(rooms[i]->getPath());
  writeRoomList(roomPaths);
}

//-----------------------------------------------------------------------------

void makePrivate(Room *room) {
  TFilePath layoutDir = ToonzFolder::getMyRoomsDir();
  TFilePath roomPath  = room->getPath();
  if (roomPath == TFilePath() || roomPath.getParentDir() != layoutDir) {
    int count = 1;
    for (;;) {
      roomPath = layoutDir + ("room" + std::to_string(count++) + ".ini");
      if (!TFileStatus(roomPath).doesExist()) break;
    }
    room->setPath(roomPath);
    TSystem::touchParentDir(roomPath);
    room->save();
  }
}

//-----------------------------------------------------------------------------

void makePrivate(std::vector<Room *> &rooms) {
  for (int i = 0; i < (int)rooms.size(); i++) makePrivate(rooms[i]);
}

// major version  :  7 bits
// minor version  :  8 bits
// revision number: 16 bits
int get_version_code_from(std::string ver) {
  int version = 0;

  // major version: assume that the major version is less than 127.
  std::string::size_type const a = ver.find('.');
  std::string const major = (a == std::string::npos) ? ver : ver.substr(0, a);
  version += std::stoi(major) << 24;
  if ((a == std::string::npos) || (a + 1 == ver.length())) {
    return version;
  }

  // minor version: assume that the minor version is less than 255.
  std::string::size_type const b = ver.find('.', a + 1);
  std::string const minor        = (b == std::string::npos)
                                ? ver.substr(a + 1)
                                : ver.substr(a + 1, b - a - 1);
  version += std::stoi(minor) << 16;
  if ((b == std::string::npos) || (b + 1 == ver.length())) {
    return version;
  }

  // revision number: assume that the revision number is less than 32767.
  version += std::stoi(ver.substr(b + 1));

  return version;
}

}  // namespace
//=============================================================================

//=============================================================================
// Room
//-----------------------------------------------------------------------------

void Room::save() {
  DockLayout *layout = dockLayout();

  // Now save layout state
  DockLayout::State state        = layout->saveState();
  std::vector<QRect> &geometries = state.first;

  QSettings settings(toQString(getPath()), QSettings::IniFormat);
  settings.remove("");

  settings.beginGroup("room");

  int i;
  for (i = 0; i < layout->count(); ++i) {
    settings.beginGroup("pane_" + QString::number(i));
    TPanel *pane = static_cast<TPanel *>(layout->itemAt(i)->widget());
    settings.setValue("name", pane->objectName());
    settings.setValue("geometry", geometries[i]);  // Use passed geometry
    if (SaveLoadQSettings *persistent =
            dynamic_cast<SaveLoadQSettings *>(pane->widget()))
      persistent->save(settings);
    if (pane->getViewType() != -1)
      // If panel has different viewtypes, store current one
      settings.setValue("viewtype", pane->getViewType());
    if (pane->objectName() == "FlipBook") {
      // Store flipbook's identification number
      FlipBook *flip = static_cast<FlipBook *>(pane->widget());
      settings.setValue("index", flip->getPoolIndex());
    }
    settings.endGroup();
  }

  // Adding hierarchy string
  settings.setValue("hierarchy", state.second);
  settings.setValue("name", QVariant(QString(m_name)));

  settings.endGroup();
}

//-----------------------------------------------------------------------------

std::pair<DockLayout *, DockLayout::State> Room::load(const TFilePath &fp) {
  QSettings settings(toQString(fp), QSettings::IniFormat);

  setPath(fp);

  DockLayout *layout = dockLayout();

  settings.beginGroup("room");
  QStringList itemsList = settings.childGroups();

  std::vector<QRect> geometries;
  unsigned int i;
  for (i = 0; i < itemsList.size(); i++) {
    // Panel i
    // NOTE: Panels have to be retrieved in the precise order they were saved.
    // settings.beginGroup(itemsList[i]);  //NO! itemsList has lexicographical
    // ordering!!
    settings.beginGroup("pane_" + QString::number(i));

    TPanel *pane = 0;
    QString paneObjectName;

    // Retrieve panel name
    QVariant name = settings.value("name");
    if (name.canConvert(QVariant::String)) {
      // Allocate panel
      paneObjectName          = name.toString();
      std::string paneStrName = paneObjectName.toStdString();
      pane = TPanelFactory::createPanel(this, paneObjectName);
      if (SaveLoadQSettings *persistent =
              dynamic_cast<SaveLoadQSettings *>(pane->widget()))
        persistent->load(settings);
    }

    if (!pane) {
      // Allocate a message panel
      MessagePanel *message = new MessagePanel(this);
      message->setWindowTitle(name.toString());
      message->setMessage(
          "This panel is not supported by the currently set license!");

      pane = message;
      pane->setPanelType(paneObjectName.toStdString());
      pane->setObjectName(paneObjectName);
    }

    pane->setObjectName(paneObjectName);

    // Add panel to room
    addDockWidget(pane);

    // Store its geometry
    geometries.push_back(settings.value("geometry").toRect());

    // Restore view type if present
    if (settings.contains("viewtype"))
      pane->setViewType(settings.value("viewtype").toInt());

    // Restore flipbook pool indices
    if (paneObjectName == "FlipBook") {
      int index = settings.value("index").toInt();
      dynamic_cast<FlipBook *>(pane->widget())->setPoolIndex(index);
    }

    settings.endGroup();
  }

  // resolve resize events here to avoid unwanted minimize of floating viewer
  qApp->processEvents();

  DockLayout::State state(geometries, settings.value("hierarchy").toString());

  layout->restoreState(state);

  setName(settings.value("name").toString());
  return std::make_pair(layout, state);
}

//-----------------------------------------------------------------------------

void Room::reload() {
  TFilePath fp = getPath();

  QSettings settings(toQString(fp), QSettings::IniFormat);

  DockLayout *layout = dockLayout();
  std::vector<QRect> geometries;

  hide();
  QRect lgeo = layout->geometry();

  for (int i = layout->count() - 1; i >= 0; i--) {
    TPanel *pane = static_cast<TPanel *>(layout->itemAt(i)->widget());
    removeDockWidget(pane);
  }

  DockLayout::State state(geometries, "-1 ");
  layout->restoreState(state);

  load(fp);

  layout->setGeometry(lgeo);
  layout->redistribute();

  show();
}

//=============================================================================
// MainWindow
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
MainWindow::MainWindow(const QString &argumentLayoutFileName, QWidget *parent,
                       Qt::WindowFlags flags)
#else
MainWindow::MainWindow(const QString &argumentLayoutFileName, QWidget *parent,
                       Qt::WFlags flags)
#endif
    : QMainWindow(parent, flags)
    , m_saveSettingsOnQuit(true)
    , m_oldRoomIndex(0)
    , m_layoutName("") {
  // store a main window pointer in advance of making its contents
  TApp::instance()->setMainWindow(this);

  m_toolsActionGroup = new QActionGroup(this);
  m_toolsActionGroup->setExclusive(true);
  m_currentRoomsChoice = Preferences::instance()->getCurrentRoomChoice();
  makeTransparencyDialog();
  defineActions();
  // user defined shortcuts will be loaded here
  CommandManager::instance()->loadShortcuts();
  TApp::instance()->getCurrentScene()->setDirtyFlag(false);

  // La menuBar altro non è che una toolbar
  // in cui posso inserire quanti custom widget voglio.
  m_topBar = new TopBar(this);

  addToolBar(m_topBar);
  addToolBarBreak(Qt::TopToolBarArea);

  m_stackedWidget = new QStackedWidget(this);

  // For the style sheet
  m_stackedWidget->setObjectName("MainStackedWidget");
  m_stackedWidget->setFrameStyle(QFrame::StyledPanel);

  // To give a border to the stackedWidget.
  /*QFrame *centralWidget = new QFrame(this);
centralWidget->setFrameStyle(QFrame::StyledPanel);
centralWidget->setObjectName("centralWidget");
QHBoxLayout *centralWidgetLayout = new QHBoxLayout;
centralWidgetLayout->setMargin(3);
centralWidgetLayout->addWidget(m_stackedWidget);
centralWidget->setLayout(centralWidgetLayout);*/

  setCentralWidget(m_stackedWidget);

  m_statusBar = new StatusBar(this);
  setStatusBar(m_statusBar);
  m_statusBar->setVisible(ShowStatusBarAction == 1 ? true : false);
  TApp::instance()->setStatusBar(m_statusBar);

  m_aboutPopup = new AboutPopup(this);

  // Leggo i settings
  readSettings(argumentLayoutFileName);

  // Setto le stanze
  QTabBar *roomTabWidget = m_topBar->getRoomTabWidget();
  connect(m_stackedWidget, SIGNAL(currentChanged(int)),
          SLOT(onCurrentRoomChanged(int)));
  connect(roomTabWidget, SIGNAL(currentChanged(int)), m_stackedWidget,
          SLOT(setCurrentIndex(int)));

  /*-- タイトルバーにScene名を表示する --*/
  connect(TApp::instance()->getCurrentScene(), SIGNAL(nameSceneChanged()), this,
          SLOT(changeWindowTitle()));

  changeWindowTitle();

  // Connetto i comandi che sono in RoomTabWidget
  connect(roomTabWidget, SIGNAL(indexSwapped(int, int)),
          SLOT(onIndexSwapped(int, int)));
  connect(roomTabWidget, SIGNAL(insertNewTabRoom()), SLOT(insertNewRoom()));
  connect(roomTabWidget, SIGNAL(deleteTabRoom(int)), SLOT(deleteRoom(int)));
  connect(roomTabWidget, SIGNAL(renameTabRoom(int, const QString)),
          SLOT(renameRoom(int, const QString)));

  setCommandHandler("MI_Quit", this, &MainWindow::onQuit);
  setCommandHandler("MI_Undo", this, &MainWindow::onUndo);
  setCommandHandler("MI_Redo", this, &MainWindow::onRedo);
  setCommandHandler("MI_NewScene", this, &MainWindow::onNewScene);
  setCommandHandler("MI_LoadScene", this, &MainWindow::onLoadScene);
  setCommandHandler("MI_LoadSubSceneFile", this, &MainWindow::onLoadSubScene);
  setCommandHandler("MI_ResetRoomLayout", this, &MainWindow::resetRoomsLayout);
  setCommandHandler(MI_AutoFillToggle, this, &MainWindow::autofillToggle);

  /*-- Animate tool + mode switching shortcuts --*/
  setCommandHandler(MI_EditNextMode, this, &MainWindow::toggleEditNextMode);
  setCommandHandler(MI_EditPosition, this, &MainWindow::toggleEditPosition);
  setCommandHandler(MI_EditRotation, this, &MainWindow::toggleEditRotation);
  setCommandHandler(MI_EditScale, this, &MainWindow::toggleEditNextScale);
  setCommandHandler(MI_EditShear, this, &MainWindow::toggleEditNextShear);
  setCommandHandler(MI_EditCenter, this, &MainWindow::toggleEditNextCenter);
  setCommandHandler(MI_EditAll, this, &MainWindow::toggleEditNextAll);

  /*-- Selection tool + type switching shortcuts --*/
  setCommandHandler(MI_SelectionNextType, this,
                    &MainWindow::toggleSelectionNextType);
  setCommandHandler(MI_SelectionRectangular, this,
                    &MainWindow::toggleSelectionRectangular);
  setCommandHandler(MI_SelectionFreehand, this,
                    &MainWindow::toggleSelectionFreehand);
  setCommandHandler(MI_SelectionPolyline, this,
                    &MainWindow::toggleSelectionPolyline);

  /*-- Geometric tool + shape switching shortcuts --*/
  setCommandHandler(MI_GeometricNextShape, this,
                    &MainWindow::toggleGeometricNextShape);
  setCommandHandler(MI_GeometricRectangle, this,
                    &MainWindow::toggleGeometricRectangle);
  setCommandHandler(MI_GeometricCircle, this,
                    &MainWindow::toggleGeometricCircle);
  setCommandHandler(MI_GeometricEllipse, this,
                    &MainWindow::toggleGeometricEllipse);
  setCommandHandler(MI_GeometricLine, this, &MainWindow::toggleGeometricLine);
  setCommandHandler(MI_GeometricPolyline, this,
                    &MainWindow::toggleGeometricPolyline);
  setCommandHandler(MI_GeometricArc, this, &MainWindow::toggleGeometricArc);
  setCommandHandler(MI_GeometricMultiArc, this,
                    &MainWindow::toggleGeometricMultiArc);
  setCommandHandler(MI_GeometricPolygon, this,
                    &MainWindow::toggleGeometricPolygon);

  /*-- Type tool + style switching shortcuts --*/
  setCommandHandler(MI_TypeNextStyle, this, &MainWindow::toggleTypeNextStyle);
  setCommandHandler(MI_TypeOblique, this, &MainWindow::toggleTypeOblique);
  setCommandHandler(MI_TypeRegular, this, &MainWindow::toggleTypeRegular);
  setCommandHandler(MI_TypeBoldOblique, this,
                    &MainWindow::toggleTypeBoldOblique);
  setCommandHandler(MI_TypeBold, this, &MainWindow::toggleTypeBold);

  /*-- Fill tool + type/mode switching shortcuts --*/
  setCommandHandler(MI_FillNextType, this, &MainWindow::toggleFillNextType);
  setCommandHandler(MI_FillNormal, this, &MainWindow::toggleFillNormal);
  setCommandHandler(MI_FillRectangular, this,
                    &MainWindow::toggleFillRectangular);
  setCommandHandler(MI_FillFreehand, this, &MainWindow::toggleFillFreehand);
  setCommandHandler(MI_FillPolyline, this, &MainWindow::toggleFillPolyline);
  setCommandHandler(MI_FillNextMode, this, &MainWindow::toggleFillNextMode);
  setCommandHandler(MI_FillAreas, this, &MainWindow::toggleFillAreas);
  setCommandHandler(MI_FillLines, this, &MainWindow::toggleFillLines);
  setCommandHandler(MI_FillLinesAndAreas, this,
                    &MainWindow::toggleFillLinesAndAreas);

  /*-- Eraser tool + type switching shortcuts --*/
  setCommandHandler(MI_EraserNextType, this, &MainWindow::toggleEraserNextType);
  setCommandHandler(MI_EraserNormal, this, &MainWindow::toggleEraserNormal);
  setCommandHandler(MI_EraserRectangular, this,
                    &MainWindow::toggleEraserRectangular);
  setCommandHandler(MI_EraserFreehand, this, &MainWindow::toggleEraserFreehand);
  setCommandHandler(MI_EraserPolyline, this, &MainWindow::toggleEraserPolyline);
  setCommandHandler(MI_EraserSegment, this, &MainWindow::toggleEraserSegment);

  /*-- Tape tool + type/mode switching shortcuts --*/
  setCommandHandler(MI_TapeNextType, this, &MainWindow::toggleTapeNextType);
  setCommandHandler(MI_TapeNormal, this, &MainWindow::toggleTapeNormal);
  setCommandHandler(MI_TapeRectangular, this,
                    &MainWindow::toggleTapeRectangular);
  setCommandHandler(MI_TapeNextMode, this, &MainWindow::toggleTapeNextMode);
  setCommandHandler(MI_TapeEndpointToEndpoint, this,
                    &MainWindow::toggleTapeEndpointToEndpoint);
  setCommandHandler(MI_TapeEndpointToLine, this,
                    &MainWindow::toggleTapeEndpointToLine);
  setCommandHandler(MI_TapeLineToLine, this, &MainWindow::toggleTapeLineToLine);

  /*-- Style Picker tool + mode switching shortcuts --*/
  setCommandHandler(MI_PickStyleNextMode, this,
                    &MainWindow::togglePickStyleNextMode);
  setCommandHandler(MI_PickStyleAreas, this, &MainWindow::togglePickStyleAreas);
  setCommandHandler(MI_PickStyleLines, this, &MainWindow::togglePickStyleLines);
  setCommandHandler(MI_PickStyleLinesAndAreas, this,
                    &MainWindow::togglePickStyleLinesAndAreas);

  /*-- RGB Picker tool + type switching shortcuts --*/
  setCommandHandler(MI_RGBPickerNextType, this,
                    &MainWindow::toggleRGBPickerNextType);
  setCommandHandler(MI_RGBPickerNormal, this,
                    &MainWindow::toggleRGBPickerNormal);
  setCommandHandler(MI_RGBPickerRectangular, this,
                    &MainWindow::toggleRGBPickerRectangular);
  setCommandHandler(MI_RGBPickerFreehand, this,
                    &MainWindow::toggleRGBPickerFreehand);
  setCommandHandler(MI_RGBPickerPolyline, this,
                    &MainWindow::toggleRGBPickerPolyline);

  /*-- Skeleton tool + mode switching shortcuts --*/
  setCommandHandler(MI_SkeletonNextMode, this,
                    &MainWindow::ToggleSkeletonNextMode);
  setCommandHandler(MI_SkeletonBuildSkeleton, this,
                    &MainWindow::ToggleSkeletonBuildSkeleton);
  setCommandHandler(MI_SkeletonAnimate, this,
                    &MainWindow::ToggleSkeletonAnimate);
  setCommandHandler(MI_SkeletonInverseKinematics, this,
                    &MainWindow::ToggleSkeletonInverseKinematics);

  /*-- Plastic tool + mode switching shortcuts --*/
  setCommandHandler(MI_PlasticNextMode, this,
                    &MainWindow::TogglePlasticNextMode);
  setCommandHandler(MI_PlasticEditMesh, this,
                    &MainWindow::TogglePlasticEditMesh);
  setCommandHandler(MI_PlasticPaintRigid, this,
                    &MainWindow::TogglePlasticPaintRigid);
  setCommandHandler(MI_PlasticBuildSkeleton, this,
                    &MainWindow::TogglePlasticBuildSkeleton);
  setCommandHandler(MI_PlasticAnimate, this, &MainWindow::TogglePlasticAnimate);

  /*-- Brush tool + mode switching shortcuts --*/
  setCommandHandler(MI_BrushAutoFillOff, this,
                    &MainWindow::ToggleBrushAutoFillOff);
  setCommandHandler(MI_BrushAutoFillOn, this,
                    &MainWindow::ToggleBrushAutoFillOn);

  setCommandHandler(MI_About, this, &MainWindow::onAbout);
  setCommandHandler(MI_OpenOnlineManual, this, &MainWindow::onOpenOnlineManual);
  setCommandHandler(MI_SupportTahoma2D, this, &MainWindow::onSupportTahoma2D);
  setCommandHandler(MI_OpenWhatsNew, this, &MainWindow::onOpenWhatsNew);
  setCommandHandler(MI_OpenCommunityForum, this,
                    &MainWindow::onOpenCommunityForum);
  setCommandHandler(MI_OpenReportABug, this, &MainWindow::onOpenReportABug);

  setCommandHandler(MI_MaximizePanel, this, &MainWindow::maximizePanel);
  setCommandHandler(MI_FullScreenWindow, this, &MainWindow::fullScreenWindow);
  setCommandHandler("MI_NewVectorLevel", this,
                    &MainWindow::onNewVectorLevelButtonPressed);
  setCommandHandler("MI_NewToonzRasterLevel", this,
                    &MainWindow::onNewToonzRasterLevelButtonPressed);
  setCommandHandler("MI_NewRasterLevel", this,
                    &MainWindow::onNewRasterLevelButtonPressed);
  setCommandHandler(MI_ClearCacheFolder, this, &MainWindow::clearCacheFolder);
  // remove ffmpegCache if still exists from crashed exit
  QString ffmpegCachePath =
      ToonzFolder::getCacheRootFolder().getQString() + "//ffmpeg";
  if (TSystem::doesExistFileOrLevel(TFilePath(ffmpegCachePath))) {
    TSystem::rmDirTree(TFilePath(ffmpegCachePath));
  }
}

//-----------------------------------------------------------------------------

MainWindow::~MainWindow() {
  TEnv::saveAllEnvVariables();
  // cleanup ffmpeg cache
  QString ffmpegCachePath =
      ToonzFolder::getCacheRootFolder().getQString() + "//ffmpeg";
  if (TSystem::doesExistFileOrLevel(TFilePath(ffmpegCachePath))) {
    TSystem::rmDirTree(TFilePath(ffmpegCachePath));
  }
}

//-----------------------------------------------------------------------------

void MainWindow::changeWindowTitle() {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;

  TProject *project   = scene->getProject();
  QString projectName = QString::fromStdString(project->getName().getName());

  QString sceneName = QString::fromStdWString(scene->getSceneName());

  if (sceneName.isEmpty()) sceneName = tr("Untitled");
  if (app->getCurrentScene()->getDirtyFlag()) sceneName += QString("*");

  /*--- レイアウトファイル名を頭に表示させる ---*/
  if (!m_layoutName.isEmpty()) sceneName.prepend(m_layoutName + " : ");

  QString name = sceneName + " [" + projectName + "] : " +
                 QString::fromStdString(TEnv::getApplicationFullName());

  setWindowTitle(name);
}

//-----------------------------------------------------------------------------

void MainWindow::changeWindowTitle(QString &str) { setWindowTitle(str); }

//-----------------------------------------------------------------------------

void MainWindow::startupFloatingPanels() {
  // Show all floating panels
  DockLayout *currDockLayout = getCurrentRoom()->dockLayout();
  int i;
  for (i = 0; i < currDockLayout->count(); ++i) {
    TPanel *currPanel = static_cast<TPanel *>(currDockLayout->widgetAt(i));
    if (currPanel->isFloating()) currPanel->show();
  }
}

//-----------------------------------------------------------------------------

Room *MainWindow::getRoom(int index) const {
  assert(index >= 0 && index < getRoomCount());
  return dynamic_cast<Room *>(m_stackedWidget->widget(index));
}

//-----------------------------------------------------------------------------
/*! Roomを名前から探す
 */
Room *MainWindow::getRoomByName(QString &roomName) {
  for (int i = 0; i < getRoomCount(); i++) {
    Room *room = dynamic_cast<Room *>(m_stackedWidget->widget(i));
    if (room) {
      if (room->getName() == roomName) return room;
    }
  }
  return 0;
}

//-----------------------------------------------------------------------------

int MainWindow::getRoomCount() const { return m_stackedWidget->count(); }

//-----------------------------------------------------------------------------

void MainWindow::refreshWriteSettings() { writeSettings(); }

//-----------------------------------------------------------------------------

void MainWindow::readSettings(const QString &argumentLayoutFileName) {
  QTabBar *roomTabWidget = m_topBar->getRoomTabWidget();

  m_topBar->loadMenubar();

  std::vector<Room *> rooms;

  // leggo l'elenco dei layout
  std::vector<TFilePath> roomPaths;

  if (readRoomList(roomPaths, argumentLayoutFileName)) {
    if (!argumentLayoutFileName.isEmpty()) {
      /*--
       * タイトルバーに表示するレイアウト名を作る：_layoutがあればそこから省く。無ければ.txtのみ省く
       * --*/
      int pos = (argumentLayoutFileName.indexOf("_layout") == -1)
                    ? argumentLayoutFileName.indexOf(".txt")
                    : argumentLayoutFileName.indexOf("_layout");
      m_layoutName = argumentLayoutFileName.left(pos);
    }
  }

  int i;
  for (i = 0; i < (int)roomPaths.size(); i++) {
    TFilePath roomPath = roomPaths[i];
    if (TFileStatus(roomPath).doesExist()) {
      Room *room = new Room(this);
      m_panelStates.push_back(room->load(roomPath));
      m_stackedWidget->addWidget(room);
      roomTabWidget->addTab(room->getName());

      // room->setDockOptions(QMainWindow::DockOptions(
      //  (QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks) &
      //  ~QMainWindow::AllowTabbedDocks));
      rooms.push_back(room);
    }
  }

  // Read the flipbook history
  FlipBookPool::instance()->load(ToonzFolder::getMyModuleDir() +
                                 TFilePath("fliphistory.ini"));

  if (rooms.empty()) {
    // 2D Room
    Room *room2D = create2DRoom();
    m_stackedWidget->addWidget(room2D);
    rooms.push_back(room2D);

    // Stop Motion Room
    Room *roomStopMotion = createStopMotionRoom();
    m_stackedWidget->addWidget(roomStopMotion);
    rooms.push_back(roomStopMotion);

    // Timing Room
    Room *roomTiming = createTimingRoom();
    m_stackedWidget->addWidget(roomTiming);
    rooms.push_back(roomTiming);

    // FX Room
    Room *roomFX = createFXRoom();
    m_stackedWidget->addWidget(roomFX);
    rooms.push_back(roomFX);

    // BrowserRoom
    Room *browserRoom = createBrowserRoom();
    m_stackedWidget->addWidget(browserRoom);
    rooms.push_back(browserRoom);
  }

  /*- If the layout files were loaded from template, then save them as private
   * ones -*/
  makePrivate(rooms);
  writeRoomList(rooms);

  // Imposto la stanza corrente
  TFilePath fp = ToonzFolder::getRoomsFile(currentRoomFileName);
  Tifstream is(fp);
  std::string currentRoomName;
  is >> currentRoomName;
  if (currentRoomName != "") {
    int count = m_stackedWidget->count();
    int index;
    for (index = 0; index < count; index++)
      if (getRoom(index)->getName().toStdString() == currentRoomName) break;
    if (index < count) {
      m_oldRoomIndex = index;
      roomTabWidget->setCurrentIndex(index);
      m_stackedWidget->setCurrentIndex(index);
    }
  }

  RecentFiles::instance()->loadRecentFiles();
}

//-----------------------------------------------------------------------------

void MainWindow::writeSettings() {
  std::vector<Room *> rooms;
  int i;

  // Flipbook history
  DockLayout *currRoomLayout(getCurrentRoom()->dockLayout());
  for (i = 0; i < currRoomLayout->count(); ++i) {
    // Remove all floating flipbooks from current room and return them into
    // the flipbook pool.
    TPanel *panel = static_cast<TPanel *>(currRoomLayout->itemAt(i)->widget());
    if (panel->isFloating() && panel->getPanelType() == "FlipBook") {
      currRoomLayout->removeWidget(panel);
      FlipBook *flipbook = static_cast<FlipBook *>(panel->widget());
      FlipBookPool::instance()->push(flipbook);
      --i;
    }
  }

  FlipBookPool::instance()->save();

  // Room layouts
  for (i = 0; i < m_stackedWidget->count(); i++) {
    Room *room = getRoom(i);
    rooms.push_back(room);
    room->save();
  }
  if (m_currentRoomsChoice == Preferences::instance()->getCurrentRoomChoice()) {
    writeRoomList(rooms);
  }

  // Current room settings
  Tofstream os(ToonzFolder::getMyRoomsDir() + currentRoomFileName);
  os << getCurrentRoom()->getName().toStdString();

  // Main window settings
  TFilePath fp = ToonzFolder::getMyModuleDir() + TFilePath("mainwindow.ini");
  QSettings settings(toQString(fp), QSettings::IniFormat);

  settings.setValue("MainWindowGeometry", saveGeometry());
}

//-----------------------------------------------------------------------------

Room *MainWindow::create2DRoom() {
  Room *room = new Room(this);
  room->setName("2D");
  room->setObjectName("2DRoom");

  m_topBar->getRoomTabWidget()->addTab("2D");

  DockLayout *layout = room->dockLayout();

  std::vector<QRect> geometries;

  // StyleEditor
  TPanel *styleEditorPane = TPanelFactory::createPanel(room, "StyleEditor");
  if (styleEditorPane) {
    styleEditorPane->setGeometry(QRect(34, 30, 259, 358));
    geometries.push_back(styleEditorPane->geometry());
    room->addDockWidget(styleEditorPane);
    layout->dockItem(styleEditorPane);
  }

  // Timeline
  TPanel *timelinePane = TPanelFactory::createPanel(room, "Timeline");
  if (timelinePane) {
    timelinePane->setGeometry(QRect(34, 764, 1886, 211));
    geometries.push_back(timelinePane->geometry());
    room->addDockWidget(timelinePane);
    layout->dockItem(timelinePane);
  }

  // SceneViewer
  TPanel *sceneViewerPane = TPanelFactory::createPanel(room, "SceneViewer");
  if (sceneViewerPane) {
    sceneViewerPane->setGeometry(QRect(297, 30, 1623, 730));
    geometries.push_back(sceneViewerPane->geometry());
    room->addDockWidget(sceneViewerPane);
    layout->dockItem(sceneViewerPane);
  }

  // ToolBar
  TPanel *toolBarPane = TPanelFactory::createPanel(room, "ToolBar");
  if (toolBarPane) {
    toolBarPane->setGeometry(QRect(0, 30, 30, 945));
    geometries.push_back(toolBarPane->geometry());
    room->addDockWidget(toolBarPane);
    layout->dockItem(toolBarPane);
  }

  // ToolOptions
  TPanel *toolOptionsPane = TPanelFactory::createPanel(room, "ToolOptions");
  if (toolOptionsPane) {
    toolOptionsPane->setGeometry(QRect(0, 0, 1920, 26));
    geometries.push_back(toolOptionsPane->geometry());
    room->addDockWidget(toolOptionsPane);
    layout->dockItem(toolOptionsPane);
  }

  // LevelPalette
  TPanel *levelPalettePane = TPanelFactory::createPanel(room, "LevelPalette");
  if (levelPalettePane) {
    levelPalettePane->setGeometry(QRect(34, 392, 259, 368));
    geometries.push_back(levelPalettePane->geometry());
    room->addDockWidget(levelPalettePane);
    layout->dockItem(levelPalettePane);
  }

  DockLayout::State state(geometries, "-1 1 [ 4 [ 3 [ [ [ 0 5 ] 2 ] 1 ] ] ] ");

  layout->restoreState(state);

  m_panelStates.push_back(std::make_pair(layout, state));

  return room;
}

//-----------------------------------------------------------------------------

Room *MainWindow::createStopMotionRoom() {
  Room *room = new Room(this);
  room->setName("StopMotion");
  room->setObjectName("StopMotionRoom");

  m_topBar->getRoomTabWidget()->addTab("StopMotion");

  DockLayout *layout = room->dockLayout();

  std::vector<QRect> geometries;

  // SceneViewer
  TPanel *sceneViewerPane = TPanelFactory::createPanel(room, "SceneViewer");
  if (sceneViewerPane) {
    sceneViewerPane->setGeometry(QRect(0, 0, 1529, 774));
    geometries.push_back(sceneViewerPane->geometry());
    room->addDockWidget(sceneViewerPane);
    layout->dockItem(sceneViewerPane);
  }

  // Timeline
  TPanel *timelinePane = TPanelFactory::createPanel(room, "Timeline");
  if (timelinePane) {
    timelinePane->setGeometry(QRect(0, 778, 1920, 197));
    geometries.push_back(timelinePane->geometry());
    room->addDockWidget(timelinePane);
    layout->dockItem(timelinePane);
  }

  // Stop Motion Controller
  TPanel *stopMotionControllerPane =
      TPanelFactory::createPanel(room, "StopMotionController");
  if (stopMotionControllerPane) {
    stopMotionControllerPane->setGeometry(QRect(1533, 0, 387, 774));
    geometries.push_back(stopMotionControllerPane->geometry());
    room->addDockWidget(stopMotionControllerPane);
    layout->dockItem(stopMotionControllerPane);
  }

  DockLayout::State state(geometries, "-1 1 [ [ 0 2 ] 1 ] ");

  layout->restoreState(state);

  m_panelStates.push_back(std::make_pair(layout, state));

  return room;
}

//-----------------------------------------------------------------------------

Room *MainWindow::createTimingRoom() {
  Room *room = new Room(this);
  room->setName("Timing");
  room->setObjectName("TimingRoom");

  m_topBar->getRoomTabWidget()->addTab("Timing");

  DockLayout *layout = room->dockLayout();

  std::vector<QRect> geometries;

  // SceneViewer
  TPanel *sceneViewerPane = TPanelFactory::createPanel(room, "SceneViewer");
  if (sceneViewerPane) {
    sceneViewerPane->setGeometry(QRect(995, 30, 925, 710));
    geometries.push_back(sceneViewerPane->geometry());
    room->addDockWidget(sceneViewerPane);
    layout->dockItem(sceneViewerPane);
  }

  // ToolBar
  TPanel *toolBarPane = TPanelFactory::createPanel(room, "ToolBar");
  if (toolBarPane) {
    toolBarPane->setGeometry(QRect(0, 0, 35, 995));
    geometries.push_back(toolBarPane->geometry());
    room->addDockWidget(toolBarPane);
    layout->dockItem(toolBarPane);
  }

  // ToolOptions
  TPanel *toolOptionsPane = TPanelFactory::createPanel(room, "ToolOptions");
  if (toolOptionsPane) {
    toolOptionsPane->setGeometry(QRect(39, 0, 1881, 26));
    geometries.push_back(toolOptionsPane->geometry());
    room->addDockWidget(toolOptionsPane);
    layout->dockItem(toolOptionsPane);
  }

  // Timeline
  TPanel *timelinePane = TPanelFactory::createPanel(room, "Timeline");
  if (timelinePane) {
    timelinePane->setGeometry(QRect(39, 744, 1881, 251));
    geometries.push_back(timelinePane->geometry());
    room->addDockWidget(timelinePane);
    layout->dockItem(timelinePane);
  }

  // Function Editor
  TPanel *functionEditorPane =
      TPanelFactory::createPanel(room, "FunctionEditor");
  if (functionEditorPane) {
    functionEditorPane->setGeometry(QRect(39, 30, 952, 710));
    geometries.push_back(functionEditorPane->geometry());
    room->addDockWidget(functionEditorPane);
    layout->dockItem(functionEditorPane);
  }

  DockLayout::State state(geometries, "-1 0 [ 1 [ 2 [ 4 0 ] 3 ] ] ");

  layout->restoreState(state);

  m_panelStates.push_back(std::make_pair(layout, state));

  return room;
}

//-----------------------------------------------------------------------------

Room *MainWindow::createFXRoom() {
  Room *room = new Room(this);
  room->setName("FX");
  room->setObjectName("FXRoom");

  m_topBar->getRoomTabWidget()->addTab("FX");

  DockLayout *layout = room->dockLayout();

  std::vector<QRect> geometries;

  // Timeline
  TPanel *timelinePane = TPanelFactory::createPanel(room, "Timeline");
  if (timelinePane) {
    timelinePane->setGeometry(QRect(0, 743, 1920, 252));
    geometries.push_back(timelinePane->geometry());
    room->addDockWidget(timelinePane);
    layout->dockItem(timelinePane);
  }

  // SceneViewer
  TPanel *sceneViewerPane = TPanelFactory::createPanel(room, "SceneViewer");
  if (sceneViewerPane) {
    sceneViewerPane->setGeometry(QRect(849, 0, 1071, 739));
    geometries.push_back(sceneViewerPane->geometry());
    room->addDockWidget(sceneViewerPane);
    layout->dockItem(sceneViewerPane);
  }

  // Schematic
  TPanel *schematicPane = TPanelFactory::createPanel(room, "Schematic");
  if (schematicPane) {
    schematicPane->setGeometry(QRect(0, 0, 845, 739));
    schematicPane->setViewType(0);
    geometries.push_back(schematicPane->geometry());
    room->addDockWidget(schematicPane);
    layout->dockItem(schematicPane);
  }

  DockLayout::State state(geometries, "-1 1 [ [ 2 1 ] 0 ] ");

  layout->restoreState(state);

  m_panelStates.push_back(std::make_pair(layout, state));

  return room;
}

//-----------------------------------------------------------------------------

Room *MainWindow::createBrowserRoom() {
  Room *browserRoom = new Room(this);
  browserRoom->setName("Browser");
  browserRoom->setObjectName("BrowserRoom");

  m_topBar->getRoomTabWidget()->addTab("Browser");

  DockLayout *layout = browserRoom->dockLayout();

  std::vector<QRect> geometries;

  // Browser
  TPanel *browserPane = TPanelFactory::createPanel(browserRoom, "Browser");
  if (browserPane) {
    browserPane->setGeometry(QRect(0, 0, 1920, 497));
    geometries.push_back(browserPane->geometry());
    browserRoom->addDockWidget(browserPane);
    layout->dockItem(browserPane);
  }

  // Scene Cast
  TPanel *sceneCastPanel = TPanelFactory::createPanel(browserRoom, "SceneCast");
  if (sceneCastPanel) {
    sceneCastPanel->setGeometry(QRect(0, 501, 1920, 494));
    geometries.push_back(sceneCastPanel->geometry());
    browserRoom->addDockWidget(sceneCastPanel);
    layout->dockItem(sceneCastPanel, browserPane, Region::bottom);
  }

  DockLayout::State state(geometries, "-1 1 [ 0 1 ] ");

  layout->restoreState(state);

  m_panelStates.push_back(std::make_pair(layout, state));

  return browserRoom;
}

//-----------------------------------------------------------------------------

Room *MainWindow::getCurrentRoom() const {
  return getRoom(m_stackedWidget->currentIndex());
}

//-----------------------------------------------------------------------------

void MainWindow::onUndo() {
  bool ret = TUndoManager::manager()->undo();
  if (!ret) DVGui::error(QObject::tr("No more Undo operations available."));
}

//-----------------------------------------------------------------------------

void MainWindow::onRedo() {
  bool ret = TUndoManager::manager()->redo();
  if (!ret) DVGui::error(QObject::tr("No more Redo operations available."));
}

//-----------------------------------------------------------------------------

void MainWindow::onNewScene() {
  IoCmd::newScene();
  CommandManager *cm = CommandManager::instance();
  cm->setChecked(MI_ShiftTrace, false);
  cm->setChecked(MI_EditShift, false);
  cm->setChecked(MI_NoShift, false);
  cm->setChecked(MI_VectorGuidedDrawing, false);
}

//-----------------------------------------------------------------------------

void MainWindow::onLoadScene() { IoCmd::loadScene(); }

//-----------------------------------------------------------------------------

void MainWindow::onLoadSubScene() { IoCmd::loadSubScene(); }
//-----------------------------------------------------------------------------

void MainWindow::onUpgradeTabPro() {}

//-----------------------------------------------------------------------------

void MainWindow::onAbout() { m_aboutPopup->exec(); }

//-----------------------------------------------------------------------------

void MainWindow::onOpenOnlineManual() {
  QDesktopServices::openUrl(QUrl(tr("http://tahoma2d.readthedocs.io")));
}

//-----------------------------------------------------------------------------

void MainWindow::onSupportTahoma2D() {
  QDesktopServices::openUrl(QUrl("http://patreon.com/jeremybullock"));
}

//-----------------------------------------------------------------------------

void MainWindow::onOpenWhatsNew() {
  QDesktopServices::openUrl(
      QUrl(tr("https://tahoma.readthedocs.io/en/latest/whats_new.html")));
}

//-----------------------------------------------------------------------------

void MainWindow::onOpenCommunityForum() {
  QDesktopServices::openUrl(QUrl(tr("https://groups.google.com/g/tahoma2d")));
}

//-----------------------------------------------------------------------------

void MainWindow::onOpenReportABug() {
  QString str = QString(
      tr("To report a bug, click on the button below to open a web browser "
         "window for Tahoma2D's Issues page on https://github.com.  Click on "
         "the 'New issue' button and fill out the form."));

  std::vector<QString> buttons = {QObject::tr("Report a Bug"),
                                  QObject::tr("Close")};
  int ret = DVGui::MsgBox(DVGui::INFORMATION, str, buttons, 1);
  if (ret == 1)
    QDesktopServices::openUrl(
        QUrl("https://github.com/tahoma2d/tahoma2d/issues"));
}
//-----------------------------------------------------------------------------

void MainWindow::autofillToggle() {
  TPaletteHandle *h = TApp::instance()->getCurrentPalette();
  h->toggleAutopaint();
}

void MainWindow::resetRoomsLayout() {
  QString question(
      tr("Are you sure you want to reload and restore default rooms?\nCustom "
         "rooms will not be touched."));
  int ret = DVGui::MsgBox(question, QObject::tr("Yes"), QObject::tr("No"));
  if (ret == 0 || ret == 2) return;

  TFilePath layoutDir = ToonzFolder::getMyRoomsDir();
  if (layoutDir != TFilePath()) {
    TFilePath layoutTemplateDir = ToonzFolder::getTemplateRoomsDir();
    TFilePathSet room_fpset;
    try {
      TSystem::readDirectory(room_fpset, layoutTemplateDir, false, true);
      TFilePathSet::iterator it = room_fpset.begin();
      for (int i = 0; it != room_fpset.end(); it++, i++) {
        TFilePath defaultfp = *it;
        if (defaultfp.getType() != "ini") continue;
        TFilePath fp = layoutDir + defaultfp.getLevelName();
        if (TFileStatus(fp).doesExist()) {
          for (i = 0; i < m_stackedWidget->count(); i++) {
            Room *room = getRoom(i);
            if (room->getPath() == fp) {
              room->reload();
              break;
            }
          }
        } else {
          TSystem::copyFile(fp, defaultfp);

          QTabBar *roomTabWidget = m_topBar->getRoomTabWidget();
          Room *room             = new Room(this);
          room->hide();
          m_panelStates.push_back(room->load(fp));
          m_stackedWidget->addWidget(room);
          roomTabWidget->addTab(room->getName());
          room->show();
        }
      }
    } catch (...) {
    }
  }
}

void MainWindow::maximizePanel() {
  DockLayout *currDockLayout = getCurrentRoom()->dockLayout();
  if (currDockLayout->getMaximized() &&
      currDockLayout->getMaximized()->isMaximized()) {
    currDockLayout->getMaximized()->maximizeDock();  // release maximization
    return;
  }

  QPoint p            = mapFromGlobal(QCursor::pos());
  QWidget *currWidget = currDockLayout->containerOf(p);
  DockWidget *currW   = dynamic_cast<DockWidget *>(currWidget);
  if (currW) currW->maximizeDock();
}

void MainWindow::fullScreenWindow() {
  if (isFullScreen())
    setWindowState(Qt::WindowMaximized);
  else
    setWindowState(Qt::WindowFullScreen);
}

//-----------------------------------------------------------------------------

void MainWindow::onCurrentRoomChanged(int newRoomIndex) {
  Room *oldRoom            = getRoom(m_oldRoomIndex);
  Room *newRoom            = getRoom(newRoomIndex);
  QList<TPanel *> paneList = oldRoom->findChildren<TPanel *>();

  // Change the parent of all the floating dockWidgets
  for (int i = 0; i < paneList.size(); i++) {
    TPanel *pane = paneList.at(i);
    if (pane->isFloating() && !pane->isHidden()) {
      QRect oldGeometry = pane->geometry();
      // Just setting the new parent is not enough for the new layout manager.
      // Must be removed from the old and added to the new.
      oldRoom->removeDockWidget(pane);
      newRoom->addDockWidget(pane);
      // pane->setParent(newRoom);

      // Some flags are lost when the parent changes.
      pane->setFloating(true);
      pane->setGeometry(oldGeometry);
      pane->show();
    }
  }
  m_oldRoomIndex = newRoomIndex;
  TSelection::setCurrent(0);
}

//-----------------------------------------------------------------------------

void MainWindow::onIndexSwapped(int firstIndex, int secondIndex) {
  assert(firstIndex >= 0 && secondIndex >= 0);
  Room *mainWindow = getRoom(firstIndex);
  m_stackedWidget->removeWidget(mainWindow);
  m_stackedWidget->insertWidget(secondIndex, mainWindow);
}

//-----------------------------------------------------------------------------

void MainWindow::insertNewRoom() {
  Room *room = new Room(this);
  room->setName("room");
  if (m_saveSettingsOnQuit) makePrivate(room);
  m_stackedWidget->insertWidget(0, room);

  // Finally, old room index is increased by 1
  m_oldRoomIndex++;
}

//-----------------------------------------------------------------------------

void MainWindow::deleteRoom(int index) {
  Room *room = getRoom(index);

  TFilePath fp = room->getPath();
  try {
    TSystem::deleteFile(fp);
  } catch (...) {
    DVGui::error(tr("Cannot delete") + toQString(fp));
    // Se non ho rimosso la stanza devo rimettere il tab!!
    m_topBar->getRoomTabWidget()->insertTab(index, room->getName());
    return;
  }

  // The old room index must be updated if index < of it
  if (index < m_oldRoomIndex) m_oldRoomIndex--;

  m_stackedWidget->removeWidget(room);
  m_panelStates.erase(m_panelStates.begin() + index);
  delete room;
}

//-----------------------------------------------------------------------------

void MainWindow::renameRoom(int index, const QString name) {
  Room *room = getRoom(index);
  room->setName(name);
  if (m_saveSettingsOnQuit) room->save();
}

//-----------------------------------------------------------------------------

void MainWindow::onMenuCheckboxChanged() {
  QAction *action = qobject_cast<QAction *>(sender());

  int isChecked = action->isChecked();

  CommandManager *cm = CommandManager::instance();

  if (cm->getAction(MI_ViewCamera) == action)
    ViewCameraToggleAction = isChecked;
  else if (cm->getAction(MI_ViewTable) == action)
    ViewTableToggleAction = isChecked;
  else if (cm->getAction(MI_ToggleEditInPlace) == action)
    EditInPlaceToggleAction = isChecked;
  else if (cm->getAction(MI_ViewBBox) == action)
    ViewBBoxToggleAction = isChecked;
  else if (cm->getAction(MI_FieldGuide) == action)
    FieldGuideToggleAction = isChecked;
  else if (cm->getAction(MI_RasterizePli) == action) {
    if (!QGLPixelBuffer::hasOpenGLPbuffers()) isChecked = 0;
    RasterizePliToggleAction                            = isChecked;
  } else if (cm->getAction(MI_SafeArea) == action)
    SafeAreaToggleAction = isChecked;
  else if (cm->getAction(MI_ViewColorcard) == action)
    ViewColorcardToggleAction = isChecked;
  else if (cm->getAction(MI_ViewGuide) == action)
    ViewGuideToggleAction = isChecked;
  else if (cm->getAction(MI_ViewRuler) == action)
    ViewRulerToggleAction = isChecked;
  else if (cm->getAction(MI_TCheck) == action)
    TCheckToggleAction = isChecked;
  // else if (cm->getAction(MI_DockingCheck) == action)
  //  DockingCheckToggleAction = isChecked;
  else if (cm->getAction(MI_ICheck) == action)
    ICheckToggleAction = isChecked;
  else if (cm->getAction(MI_Ink1Check) == action)
    Ink1CheckToggleAction = isChecked;
  else if (cm->getAction(MI_PCheck) == action)
    PCheckToggleAction = isChecked;
  else if (cm->getAction(MI_BCheck) == action)
    BCheckToggleAction = isChecked;
  else if (cm->getAction(MI_GCheck) == action)
    GCheckToggleAction = isChecked;
  else if (cm->getAction(MI_ACheck) == action)
    ACheckToggleAction = isChecked;
  else if (cm->getAction(MI_IOnly) == action)
    IOnlyToggleAction = isChecked;
  else if (cm->getAction(MI_Link) == action)
    LinkToggleAction = isChecked;
  else if (cm->getAction(MI_ShiftTrace) == action)
    ShiftTraceToggleAction = isChecked;
  else if (cm->getAction(MI_EditShift) == action)
    EditShiftToggleAction = isChecked;
  else if (cm->getAction(MI_NoShift) == action)
    NoShiftToggleAction = isChecked;
  else if (cm->getAction(MI_TouchGestureControl) == action)
    TouchGestureControl = isChecked;
}

//-----------------------------------------------------------------------------

void MainWindow::showEvent(QShowEvent *event) {
  //  QTimer *nt = new QTimer(this);
  //
  //  nt->setSingleShot(true);
  //  nt->setInterval(100);
  //
  //  connect(nt, &QTimer::timeout, [=]() {
  //#ifdef WIN32
  //    if (!m_shownOnce && windowState() == Qt::WindowMaximized) {
  //      int roomsSize = m_panelStates.size();
  //      for (auto iter : m_panelStates) {
  //        iter.first->restoreState(iter.second);
  //      }
  //      m_shownOnce = true;
  //    }
  //#endif
  //  });
  //  nt->connect(nt, SIGNAL(timeout()), SLOT(deleteLater()));
  //
  //  nt->start();

  getCurrentRoom()->layout()->setEnabled(true);  // See main function in
                                                 // main.cpp
  if (Preferences::instance()->isStartupPopupEnabled() &&
      !m_startupPopupShown) {
    StartupPopup *startupPopup = new StartupPopup();
    startupPopup->show();
    m_startupPopupShown = true;
  }
}
extern const char *applicationName;
extern const char *applicationVersion;
//-----------------------------------------------------------------------------
void MainWindow::checkForUpdates() {
  // Since there is only a single version of Tahoma, we can do a simple check
  // against a string
  QString updateUrl("https://tahoma2d.org/files/tahoma-version.txt");

  m_updateChecker = new UpdateChecker(updateUrl);
  connect(m_updateChecker, SIGNAL(done(bool)), this,
          SLOT(onUpdateCheckerDone(bool)));
}
//-----------------------------------------------------------------------------

void MainWindow::onUpdateCheckerDone(bool error) {
  if (error) {
    // Get the last update date
    return;
  }

  int const software_version =
      get_version_code_from(TEnv::getApplicationVersion());
  int const latest_version =
      get_version_code_from(m_updateChecker->getLatestVersion().toStdString());
  if (software_version < latest_version) {
    QStringList buttons;
    buttons.push_back(QObject::tr("Visit Web Site"));
    buttons.push_back(QObject::tr("Cancel"));
    DVGui::MessageAndCheckboxDialog *dialog = DVGui::createMsgandCheckbox(
        DVGui::INFORMATION,
        QObject::tr("An update is available for this software.\nVisit the Web "
                    "site for more information."),
        QObject::tr("Check for the latest version on launch."), buttons, 0,
        Qt::Checked);
    int ret = dialog->exec();
    if (dialog->getChecked() == Qt::Unchecked)
      Preferences::instance()->setValue(latestVersionCheckEnabled, false);
    dialog->deleteLater();
    if (ret == 1) {
      // Write the new last date to file
      QDesktopServices::openUrl(QObject::tr(
          "https://github.com/tahoma2d/tahoma2d/releases/latest"));
    }
  }

  disconnect(m_updateChecker);
  m_updateChecker->deleteLater();
}
//-----------------------------------------------------------------------------

void MainWindow::closeEvent(QCloseEvent *event) {
  // il riferimento alla variabile booleana doExit viene passato a tutti gli
  // slot che catturano l'emissione
  // del segnale. Impostando a false tale variabile, l'applicazione non viene
  // chiusa!
  bool doExit = true;
  emit exit(doExit);
  if (!doExit) {
    event->ignore();
    return;
  }

  if (!IoCmd::saveSceneIfNeeded(QApplication::tr("Quit"))) {
    event->ignore();
    return;
  }

  // sto facendo quit. interrompo (il prima possibile) le threads
  IconGenerator::instance()->clearRequests();

  if (m_saveSettingsOnQuit) writeSettings();

  // svuoto la directory di output
  TFilePath outputsDir = TEnv::getStuffDir() + "outputs";
  TFilePathSet fps;
  if (TFileStatus(outputsDir).isDirectory()) {
    TSystem::readDirectory(fps, outputsDir);
    TFilePathSet::iterator fpsIt;
    for (fpsIt = fps.begin(); fpsIt != fps.end(); ++fpsIt) {
      TFilePath fp = *fpsIt;
      if (TSystem::doesExistFileOrLevel(fp)) {
        try {
          TSystem::removeFileOrLevel(fp);
        } catch (...) {
        }
      }
    }
  }

  TImageCache::instance()->clear(true);

  event->accept();
  TThread::shutdown();
  qApp->quit();
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createAction(const char *id, const QString &name,
                                  const QString &defaultShortcut,
                                  QString newStatusTip, CommandType type) {
  QAction *action = new DVAction(name, this);
  action->setIconVisibleInMenu(false);  // Hide icons
  addAction(action);
#ifdef MACOSX
  // To prevent the wrong menu items (due to MacOS menu naming conventions),
  // from
  // taking Preferences, Quit, or About roles (sometimes happens unexpectedly in
  // translations) - all menu items should have "NoRole"
  //  except for Preferences, Quit, and About
  if (strcmp(id, MI_Preferences) == 0)
    action->setMenuRole(QAction::PreferencesRole);
  else if (strcmp(id, MI_Quit) == 0)
    action->setMenuRole(QAction::QuitRole);
  else if (strcmp(id, MI_About) == 0)
    action->setMenuRole(QAction::AboutRole);
  else
    action->setMenuRole(QAction::NoRole);
#endif
  CommandManager::instance()->define(id, type, defaultShortcut.toStdString(),
                                     action);
  action->setStatusTip(newStatusTip);
  return action;
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createRightClickMenuAction(const char *id,
                                                const QString &name,
                                                const QString &defaultShortcut,
                                                QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      RightClickMenuCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuFileAction(const char *id, const QString &name,
                                          const QString &defaultShortcut,
                                          QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      MenuFileCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuEditAction(const char *id, const QString &name,
                                          const QString &defaultShortcut,
                                          QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      MenuEditCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuScanCleanupAction(const char *id,
                                                 const QString &name,
                                                 const QString &defaultShortcut,
                                                 QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      MenuScanCleanupCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuLevelAction(const char *id, const QString &name,
                                           const QString &defaultShortcut,
                                           QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      MenuLevelCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuXsheetAction(const char *id, const QString &name,
                                            const QString &defaultShortcut,
                                            QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      MenuXsheetCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuCellsAction(const char *id, const QString &name,
                                           const QString &defaultShortcut,
                                           QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      MenuCellsCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuViewAction(const char *id, const QString &name,
                                          const QString &defaultShortcut,
                                          QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      MenuViewCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuWindowsAction(const char *id,
                                             const QString &name,
                                             const QString &defaultShortcut,
                                             QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      MenuWindowsCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuPlayAction(const char *id, const QString &name,
                                          const QString &defaultShortcut,
                                          QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      MenuPlayCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuRenderAction(const char *id, const QString &name,
                                            const QString &defaultShortcut,
                                            QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      MenuRenderCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuHelpAction(const char *id, const QString &name,
                                          const QString &defaultShortcut,
                                          QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      MenuHelpCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createRGBAAction(const char *id, const QString &name,
                                      const QString &defaultShortcut,
                                      QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip, RGBACommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createFillAction(const char *id, const QString &name,
                                      const QString &defaultShortcut,
                                      QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip, FillCommandType);
}
//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuAction(const char *id, const QString &name,
                                      QList<QString> list,
                                      QString newStatusTip) {
  QMenu *menu     = new DVMenuAction(name, this, list);
  QAction *action = menu->menuAction();
  action->setStatusTip(newStatusTip);
  CommandManager::instance()->define(id, MenuCommandType, "", action);
  return action;
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createViewerAction(const char *id, const QString &name,
                                        const QString &defaultShortcut,
                                        QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip, ZoomCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createVisualizationButtonAction(const char *id,
                                                     const QString &name,
                                                     QString newStatusTip) {
  return createAction(id, name, "", newStatusTip,
                      VisualizationButtonCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMiscAction(const char *id, const QString &name,
                                      const char *defaultShortcut,
                                      QString newStatusTip) {
  QAction *action = new DVAction(name, this);
  action->setStatusTip(newStatusTip);
  CommandManager::instance()->define(id, MiscCommandType, defaultShortcut,
                                     action);
  return action;
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createToolOptionsAction(const char *id,
                                             const QString &name,
                                             const QString &defaultShortcut,
                                             QString newStatusTip) {
  QAction *action = new DVAction(name, this);
  addAction(action);
  action->setStatusTip(newStatusTip);
  CommandManager::instance()->define(id, ToolModifierCommandType,
                                     defaultShortcut.toStdString(), action);
  return action;
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createStopMotionAction(const char *id, const QString &name,
                                            const QString &defaultShortcut,
                                            QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      StopMotionCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createToggle(const char *id, const QString &name,
                                  const QString &defaultShortcut,
                                  bool startStatus, CommandType type,
                                  QString newStatusTip) {
  QAction *action = createAction(id, name, defaultShortcut, newStatusTip, type);
  action->setCheckable(true);
  if (startStatus == true) action->trigger();
  bool ret =
      connect(action, SIGNAL(changed()), this, SLOT(onMenuCheckboxChanged()));
  assert(ret);
  return action;
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createToolAction(const char *id, const char *iconName,
                                      const QString &name,
                                      const QString &defaultShortcut,
                                      QString newStatusTip) {
  QIcon icon      = createQIcon(iconName);
  QAction *action = new DVAction(icon, name, this);
  action->setCheckable(true);
  action->setActionGroup(m_toolsActionGroup);
  action->setIconVisibleInMenu(true);
  action->setStatusTip(newStatusTip);

  // When the viewer is maximized (not fullscreen) the toolbar is hided and the
  // action are disabled,
  // so the tool shortcuts don't work.
  // Adding tool actions to the main window solve this problem!
  addAction(action);

  CommandManager::instance()->define(id, ToolCommandType,
                                     defaultShortcut.toStdString(), action);
  return action;
}

//-----------------------------------------------------------------------------

void MainWindow::defineActions() {
  QString separator = "                    ";
  QAction *menuAct  = createMenuFileAction(MI_NewScene, tr("&New Scene"),
                                          "Ctrl+N", tr("Create a new scene."));
  menuAct->setIcon(createQIcon("new_scene"));
  menuAct = createMenuFileAction(MI_LoadScene, tr("&Load Scene..."), "Ctrl+L",
                                 tr("Load an existing scene."));
  menuAct->setIcon(createQIcon("load_scene"));
  menuAct =
      createMenuFileAction(MI_SaveScene, tr("&Save Scene"), "Ctrl+Shift+S",
                           tr("Save ONLY the scene.") + separator +
                               tr("This does NOT save levels or images."));
  menuAct->setIcon(createQIcon("save_scene"));
  menuAct = createMenuFileAction(
      MI_SaveSceneAs, tr("&Save Scene As..."), "",
      tr("Save ONLY the scene with a new name.") + separator +
          tr("This does NOT save levels or images."));
  menuAct->setIcon(createQIcon("save_scene_as"));
  menuAct = createMenuFileAction(
      MI_SaveAll, tr("&Save All"), "Ctrl+S",
      tr("Save the scene info and the levels and images.") + separator +
          tr("Saves everything."));
  menuAct->setIconText(tr("Save All"));
  menuAct->setIcon(createQIcon("saveall"));
  menuAct = createMenuFileAction(
      MI_RevertScene, tr("&Revert Scene"), "",
      tr("Revert the scene to its previously saved state."));
  menuAct->setIcon(createQIcon("revert_scene"));

  QAction *act = CommandManager::instance()->getAction(MI_RevertScene);
  if (act) act->setEnabled(false);
  QList<QString> files;
  menuAct = createMenuFileAction(
      MI_LoadFolder, tr("&Load Folder..."), "",
      tr("Load the contents of a folder into the current scene."));
  menuAct->setIcon(createQIcon("load_folder"));
  createMenuFileAction(
      MI_LoadSubSceneFile, tr("&Load As Sub-Scene..."), "",
      tr("Load an existing scene into the current scene as a sub-scene"));
  createMenuAction(MI_OpenRecentScene, tr("&Open Recent Scene File"), files,
                   tr("Load a recently used scene."));
  createMenuAction(MI_OpenRecentLevel, tr("&Open Recent Level File"), files,
                   tr("Load a recently used level."));
  createMenuFileAction(MI_ClearRecentScene, tr("&Clear Recent Scene File List"),
                       "", tr("Remove everything from the recent scene list."));
  createMenuFileAction(MI_ClearRecentLevel, tr("&Clear Recent level File List"),
                       "", tr("Remove everything from the recent level list."));
  createMenuFileAction(MI_ClearRecentProject, tr("&Clear Recent Project List"),
                       "",
                       tr("Remove everything from the recent project list."));

  menuAct = createMenuLevelAction(MI_NewLevel, tr("&New Level..."), "Alt+N",
                                  tr("Create a new drawing layer."));
  menuAct->setIcon(createQIcon("new_document"));
  menuAct =
      createMenuLevelAction(MI_NewVectorLevel, tr("&New Vector Level"), "",
                            tr("Create a new vector level.") + separator +
                                tr("Vectors can be manipulated easily and have "
                                   "some extra tools and features."));
  menuAct->setIcon(createQIcon("new_vector_level"));
  menuAct = createMenuLevelAction(
      MI_NewToonzRasterLevel, tr("&New Smart Raster Level"), "",
      tr("Create a new Smart Raster level.") + separator +
          tr("Smart Raster levels are color mapped making the colors easier to "
             "adjust at any time."));
  menuAct->setIcon(createQIcon("new_toonz_raster_level"));
  menuAct = createMenuLevelAction(
      MI_NewRasterLevel, tr("&New Raster Level"), "",
      tr("Create a new raster level") + separator +
          tr("Raster levels are traditonal drawing levels") + separator +
          tr("Imported images will be imported as raster levels."));
  menuAct->setIcon(createQIcon("new_raster_level"));

  menuAct = createMenuLevelAction(
      MI_NewSpline, tr("&New Motion Path"), "",
      tr("Create a new motion path.") + separator +
          tr("Motion paths can be used as animation guides, or you can animate "
             "objects along a motion path."));
  menuAct->setIcon(createQIcon("menu_toggle"));

  menuAct = createMenuLevelAction(MI_LoadLevel, tr("&Load Level..."), "",
                                  tr("Load an existing level."));
  menuAct->setIcon(createQIcon("load_level"));
  menuAct = createMenuLevelAction(MI_SaveLevel, tr("&Save Level"), "",
                                  tr("Save the current level.") + separator +
                                      tr("This does not save the scene info."));
  menuAct->setIcon(createQIcon("save_level"));
  menuAct = createMenuLevelAction(MI_SaveAllLevels, tr("&Save All Levels"), "",
                                  tr("Save all levels loaded into the scene.") +
                                      separator +
                                      tr("This does not save the scene info."));
  menuAct->setIcon(createQIcon("save_all_levels"));
  menuAct = createMenuLevelAction(
      MI_SaveLevelAs, tr("&Save Level As..."), "",
      tr("Save the current level as a different name.") + separator +
          tr("This does not save the scene info."));
  menuAct->setIcon(createQIcon("save_level_as"));
  menuAct = createMenuLevelAction(
      MI_ExportLevel, tr("&Export Level..."), "",
      tr("Export the current level as an image sequence."));
  menuAct->setIcon(createQIcon("export_level"));
  menuAct = createMenuFileAction(
      MI_ConvertFileWithInput, tr("&Convert File..."), "",
      tr("Convert an existing file or image sequnce to another format."));
  menuAct->setIcon(createQIcon("convert"));
  createRightClickMenuAction(
      MI_SavePaletteAs, tr("&Save Palette As..."), "",
      tr("Save the current style palette as a separate file with a new name."));
  createRightClickMenuAction(MI_SaveStudioPalette, tr("&Save Studio Palette"),
                             "", tr("Save the current Studio Palette."));
  createRightClickMenuAction(
      MI_OverwritePalette, tr("&Save Palette"), "",
      tr("Save the current style palette as a separate file."));
  createRightClickMenuAction(MI_SaveAsDefaultPalette,
                             tr("&Save As Default Palette"), "",
                             tr("Save the current style palette as the default "
                                "for new levels of the current level type."));
  menuAct = createMenuFileAction(MI_LoadColorModel, tr("&Load Color Model..."),
                                 "", tr("Load an image as a color guide."));
  menuAct->setIcon(createQIcon("load_colormodel"));
  createMenuFileAction(MI_ImportMagpieFile,
                       tr("&Import Toonz Lip Sync File..."), "",
                       tr("Import a lip sync file to be applied to a level."));
  createMenuFileAction(MI_NewProject, tr("&New Project..."), "",
                       tr("Create a new project.") + separator +
                           tr("A project is a container for a collection of "
                              "related scenes and drawings."));
  createMenuFileAction(MI_ProjectSettings, tr("&Switch Project"), "");
  createMenuFileAction(MI_SaveDefaultSettings, tr("&Save Default Settings"), "",
                       tr("Use the current scene's settings as a template for "
                          "all new scenes in the current project."));
  menuAct = createMenuRenderAction(
      MI_OutputSettings, tr("&Output Settings..."), "Ctrl+O",
      tr("Control the output settings for the current scene.") + separator +
          tr("You can render from the output settings window also."));
  menuAct->setIcon(createQIcon("output_settings"));
  menuAct = createMenuRenderAction(
      MI_PreviewSettings, tr("&Preview Settings..."), "",
      tr("Control the settings that will be used to preview the scene."));
  menuAct->setIcon(createQIcon("preview_settings"));
  menuAct = createMenuRenderAction(MI_Render, tr("&Render"), "Ctrl+Shift+R",
                                   tr("Renders according to the settings and "
                                      "location set in Output Settings."));
  menuAct->setIcon(createQIcon("render"));
  menuAct = createMenuRenderAction(
      MI_SaveAndRender, tr("&Save and Render"), "",
      tr("Saves the current scene and renders according to the settings and "
         "location set in Output Settings."));
  menuAct->setIcon(createQIcon("render"));
  menuAct = createMenuRenderAction(
      MI_FastRender, tr("&Fast Render to MP4"), "Alt+R",
      tr("Exports an MP4 file to the location specified in the preferences.") +
          separator + tr("This is quicker than going into the Output Settings "
                         "and setting up an MP4 render."));
  menuAct->setIcon(createQIcon("fast_render_mp4"));
  menuAct = createMenuRenderAction(
      MI_Preview, tr("&Preview"), "Ctrl+R",
      tr("Previews the current scene with all effects applied."));
  menuAct->setIcon(createQIcon("preview"));
  createMenuFileAction(
      MI_SoundTrack, tr("&Export Soundtrack"), "",
      tr("Exports the soundtrack to the current scene as a wav file."));
  createStopMotionAction(
      MI_StopMotionExportImageSequence,
      tr("&Export Stop Motion Image Sequence"), "",
      tr("Exports the full resolution stop motion image sequence.") +
          separator + tr("This is especially useful if using a DSLR camera."));
  menuAct = createMenuRenderAction(
      MI_SavePreviewedFrames, tr("&Save Previewed Frames"), "",
      tr("Save the images created during preview to a specified location."));
  menuAct->setIcon(createQIcon("save_previewed_frames"));
  createRightClickMenuAction(MI_RegeneratePreview, tr("&Regenerate Preview"),
                             "", tr("Recreates a set of preview images."));
  createRightClickMenuAction(MI_RegenerateFramePr,
                             tr("&Regenerate Frame Preview"), "",
                             tr("Regenerate the frame preview."));
  createRightClickMenuAction(MI_ClonePreview, tr("&Clone Preview"), "",
                             tr("Creates a clone of the previewed images."));
  createRightClickMenuAction(MI_FreezePreview, tr("&Freeze//Unfreeze Preview"),
                             "", tr("Prevent the preview from being updated."));
  CommandManager::instance()->setToggleTexts(
      MI_FreezePreview, tr("Freeze Preview"), tr("Unfreeze Preview"));
  // createAction(MI_SavePreview,         "&Save Preview",		"");
  createRightClickMenuAction(MI_SavePreset, tr("&Save As Preset"), "");
  menuAct = createMenuFileAction(MI_Preferences, tr("&Preferences..."),
                                 "Ctrl+U", tr("Change Tahoma2D's settings."));
  menuAct->setIcon(createQIcon("gear"));
  createMenuFileAction(MI_ShortcutPopup, tr("&Configure Shortcuts..."), "",
                       tr("Change the shortcuts of Tahoma2D."));

  menuAct = createMenuFileAction(MI_PrintXsheet, tr("&Print Xsheet"), "",
                                 tr("Print the scene's exposure sheet."));
  menuAct->setIcon(createQIcon("printer"));

  createMenuFileAction(MI_ExportXDTS,
                       tr("Export Exchange Digital Time Sheet (XDTS)"), "");

  menuAct = createMenuFileAction(
      "MI_RunScript", tr("Run Script..."), "",
      tr("Run a script to perform a series of actions on a scene."));
  menuAct->setIcon(createQIcon("run_script"));

  menuAct = createMenuFileAction(
      "MI_OpenScriptConsole", tr("Open Script Console..."), "",
      tr("Open a console window where you can enter script commands."));
  menuAct->setIcon(createQIcon("console"));

  menuAct =
      createMenuFileAction(MI_Print, tr("&Print Current Frame..."), "Ctrl+P");
  menuAct->setIcon(createQIcon("printer"));

  createMenuFileAction(MI_Quit, tr("&Quit"), "Ctrl+Q", tr("Bye."));
#ifndef NDEBUG
  createMenuFileAction("MI_ReloadStyle", tr("Reload qss"), "");
#endif
  createMenuAction(MI_LoadRecentImage, tr("&Load Recent Image Files"), files);
  createMenuFileAction(MI_ClearRecentImage,
                       tr("&Clear Recent Flipbook Image List"), "");
  createMenuFileAction(MI_ClearCacheFolder, tr("&Clear Cache Folder"), "");

  createRightClickMenuAction(MI_PreviewFx, tr("Preview Fx"), "");

  menuAct = createMenuEditAction(MI_SelectAll, tr("&Select All"), "Ctrl+A");
  menuAct->setIcon(createQIcon("select_all"));

  menuAct =
      createMenuEditAction(MI_InvertSelection, tr("&Invert Selection"), "");
  menuAct->setIcon(createQIcon("invert_selection"));

  menuAct = createMenuEditAction(MI_Undo, tr("&Undo"), "Ctrl+Z");
  menuAct->setIcon(createQIcon("undo"));

  menuAct = createMenuEditAction(MI_Redo, tr("&Redo"), "Ctrl+Y");
  menuAct->setIcon(createQIcon("redo"));

  menuAct = createMenuEditAction(MI_Cut, tr("&Cut"), "Ctrl+X");
  menuAct->setIcon(createQIcon("cut"));

  menuAct = createMenuEditAction(MI_Copy, tr("&Copy"), "Ctrl+C");
  menuAct->setIcon(createQIcon("content_copy"));

  menuAct = createMenuEditAction(MI_Paste, tr("&Paste Insert"), "Ctrl+V");
  menuAct->setIcon(createQIcon("paste"));
  menuAct = createMenuEditAction(
      MI_PasteBelow, tr("&Paste Insert Below/Before"), "Ctrl+Shift+V");
  menuAct->setIcon(createQIcon("paste_above_after"));
  menuAct = createMenuEditAction(MI_PasteDuplicate, tr("&Paste as a Copy"), "");
  menuAct->setIcon(createQIcon("paste_duplicate"));
  menuAct = createMenuEditAction(MI_PasteInto, tr("&Paste Into"), "");
  menuAct->setIcon(createQIcon("paste_into"));
  createRightClickMenuAction(MI_PasteValues, tr("&Paste Color && Name"), "");
  createRightClickMenuAction(MI_PasteColors, tr("Paste Color"), "");
  createRightClickMenuAction(MI_PasteNames, tr("Paste Name"), "");
  createRightClickMenuAction(MI_GetColorFromStudioPalette,
                             tr("Get Color from Studio Palette"), "");
  createRightClickMenuAction(MI_ToggleLinkToStudioPalette,
                             tr("Toggle Link to Studio Palette"), "");
  createRightClickMenuAction(MI_RemoveReferenceToStudioPalette,
                             tr("Remove Reference to Studio Palette"), "");
  // createMenuEditAction(MI_PasteNew,     tr("&Paste New"),  "");
  createMenuCellsAction(MI_MergeFrames, tr("&Merge"), "");

  menuAct = createMenuEditAction(MI_Clear, tr("&Delete"), "Del");
  menuAct->setIcon(createQIcon("delete"));
  createMenuEditAction(MI_ClearFrames, tr("&Clear Frames"), "");
  menuAct = createMenuEditAction(MI_Insert, tr("&Insert"), "Ins");
  menuAct->setIcon(createQIcon("insert"));
  menuAct = createMenuEditAction(MI_InsertBelow, tr("&Insert Below/Before"),
                                 "Shift+Ins");
  menuAct->setIcon(createQIcon("insert_above_after"));
  menuAct = createMenuEditAction(MI_Group, tr("&Group"), "Ctrl+G");
  menuAct->setIcon(createQIcon("group"));
  menuAct = createMenuEditAction(MI_Ungroup, tr("&Ungroup"), "Ctrl+Shift+G");
  menuAct->setIcon(createQIcon("ungroup"));
  menuAct = createMenuEditAction(MI_EnterGroup, tr("&Enter Group"), "");
  menuAct->setIcon(createQIcon("enter_group"));
  menuAct = createMenuEditAction(MI_ExitGroup, tr("&Exit Group"), "");
  menuAct->setIcon(createQIcon("leave_group"));

  menuAct = createMenuEditAction(MI_SendBack, tr("&Move to Back"), "Ctrl+[");
  menuAct->setIcon(createQIcon("move_to_back"));
  menuAct = createMenuEditAction(MI_SendBackward, tr("&Move Back One"), "[");
  menuAct->setIcon(createQIcon("move_back_one"));
  menuAct = createMenuEditAction(MI_BringForward, tr("&Move Forward One"), "]");
  menuAct->setIcon(createQIcon("move_forward_one"));
  menuAct =
      createMenuEditAction(MI_BringToFront, tr("&Move to Front"), "Ctrl+]");
  menuAct->setIcon(createQIcon("move_to_front"));

  menuAct = createMenuLevelAction(MI_RemoveEndpoints,
                                  tr("&Remove Vector Overflow"), "");
  menuAct->setIcon(createQIcon("remove_vector_overflow"));
  QAction *toggle =
      createToggle(MI_TouchGestureControl, tr("&Touch Gesture Control"), "",
                   TouchGestureControl ? 1 : 0, MiscCommandType);
  toggle->setEnabled(true);
  toggle->setIcon(QIcon(":Resources/touch.svg"));

  QAction *cleanupSettingsAction = createMenuScanCleanupAction(
      MI_CleanupSettings, tr("&Cleanup Settings..."), "");
  cleanupSettingsAction->setIcon(createQIcon("cleanup_settings"));
  toggle = createToggle(MI_CleanupPreview, tr("&Preview Cleanup"), "", 0,
                        MenuScanCleanupCommandType);
  toggle->setIcon(createQIcon("cleanup_preview"));
  CleanupPreviewCheck::instance()->setToggle(toggle);
  toggle = createToggle(MI_CameraTest, tr("&Camera Test"), "", 0,
                        MenuScanCleanupCommandType);
  CameraTestCheck::instance()->setToggle(toggle);

  menuAct = createToggle(MI_OpacityCheck, tr("&Opacity Check"), "Alt+1", false,
                         MenuScanCleanupCommandType);
  menuAct->setIcon(createQIcon("opacity_check"));

  menuAct = createMenuScanCleanupAction(MI_Cleanup, tr("&Cleanup"), "");
  menuAct->setIcon(createQIcon("cleanup"));

  menuAct = createMenuLevelAction(MI_AddFrames, tr("&Add Frames..."), "");
  menuAct->setIcon(createQIcon("add_cells"));

  menuAct = createMenuLevelAction(MI_Renumber, tr("&Renumber..."), "");
  menuAct->setIcon(createQIcon("renumber"));
  menuAct = createMenuLevelAction(MI_ReplaceLevel, tr("&Replace Level..."), "");
  menuAct->setIcon(createQIcon("replace_level"));
  menuAct = createMenuLevelAction(MI_RevertToCleanedUp,
                                  tr("&Revert to Cleaned Up"), "");
  menuAct->setIcon(createQIcon("revert_level_to_cleanup"));
  menuAct = createMenuLevelAction(MI_RevertToLastSaved, tr("&Reload"), "");
  menuAct->setIcon(createQIcon("reload_level"));
  createMenuLevelAction(MI_ExposeResource, tr("&Expose in Scene"), "");
  createMenuLevelAction(MI_EditLevel, tr("&Display in Level Strip"), "");
  menuAct =
      createMenuLevelAction(MI_LevelSettings, tr("&Level Settings..."), "");
  menuAct->setIcon(createQIcon("level_settings"));
  menuAct = createMenuLevelAction(MI_AdjustLevels, tr("Adjust Levels..."), "");
  menuAct->setIcon(createQIcon("histograms"));
  menuAct =
      createMenuLevelAction(MI_AdjustThickness, tr("Adjust Thickness..."), "");
  menuAct->setIcon(createQIcon("thickness"));
  menuAct = createMenuLevelAction(MI_Antialias, tr("&Antialias..."), "");
  menuAct->setIcon(createQIcon("antialias"));
  menuAct = createMenuLevelAction(MI_Binarize, tr("&Binarize..."), "");
  menuAct->setIcon(createQIcon("binarize"));
  menuAct = createMenuLevelAction(MI_BrightnessAndContrast,
                                  tr("&Brightness and Contrast..."), "");
  menuAct->setIcon(createQIcon("brightness_contrast"));
  menuAct = createMenuLevelAction(MI_LinesFade, tr("&Color Fade..."), "");
  menuAct->setIcon(createQIcon("colorfade"));

  menuAct = createMenuLevelAction(MI_CanvasSize, tr("&Canvas Size..."), "");
  if (menuAct) menuAct->setDisabled(true);
  menuAct->setIcon(createQIcon("resize"));

  menuAct = createMenuLevelAction(MI_FileInfo, tr("&Info..."), "");
  menuAct->setIcon(createQIcon("level_info"));
  createRightClickMenuAction(MI_ViewFile, tr("&View..."), "");
  menuAct = createMenuLevelAction(MI_RemoveUnused,
                                  tr("&Remove All Unused Levels"), "");
  menuAct->setIcon(createQIcon("remove_unused_levels"));
  createMenuLevelAction(MI_ReplaceParentDirectory,
                        tr("&Replace Parent Directory..."), "");
  menuAct =
      createMenuXsheetAction(MI_SceneSettings, tr("&Scene Settings..."), "");
  menuAct->setIcon(createQIcon("scene_settings"));
  menuAct =
      createMenuXsheetAction(MI_CameraSettings, tr("&Camera Settings..."), "");
  menuAct->setIcon(createQIcon("camera_settings"));
  createMiscAction(MI_CameraStage, tr("&Camera Settings..."), "");

  menuAct = createMenuXsheetAction(MI_OpenChild, tr("&Open Sub-Scene"), "");
  menuAct->setIcon(createQIcon("sub_enter"));
  menuAct = createMenuXsheetAction(MI_CloseChild, tr("&Close Sub-Scene"), "");
  menuAct->setIcon(createQIcon("sub_leave"));
  menuAct =
      createMenuXsheetAction(MI_ExplodeChild, tr("Explode Sub-Scene"), "");
  menuAct->setIcon(createQIcon("sub_explode"));
  menuAct = createMenuXsheetAction(MI_Collapse, tr("Collapse"), "");
  menuAct->setIcon(createQIcon("sub_collapse"));
  toggle = createToggle(MI_ToggleEditInPlace, tr("&Toggle Edit In Place"), "",
                        EditInPlaceToggleAction ? 1 : 0, MenuXsheetCommandType);
  toggle->setIconText(tr("Toggle Edit in Place"));
  toggle->setIcon(createQIcon("sub_edit_in_place"));
  menuAct = createMenuXsheetAction(MI_SaveSubxsheetAs,
                                   tr("&Save Sub-Scene As..."), "");
  menuAct->setIcon(createQIcon("saveas"));
  menuAct = createMenuXsheetAction(MI_Resequence, tr("Resequence"), "");
  menuAct->setIcon(createQIcon("resequence"));
  menuAct = createMenuXsheetAction(MI_CloneChild, tr("Clone Sub-Scene"), "");
  menuAct->setIcon(createQIcon("sub_clone"));

  menuAct = createMenuXsheetAction(MI_ApplyMatchLines,
                                   tr("&Apply Match Lines..."), "");
  menuAct->setIcon(createQIcon("apply_match_lines"));
  menuAct =
      createMenuXsheetAction(MI_MergeCmapped, tr("&Merge Tlv Levels..."), "");
  menuAct->setIcon(createQIcon("merge_levels_tlv"));
  menuAct = createMenuXsheetAction(MI_DeleteMatchLines,
                                   tr("&Delete Match Lines"), "");
  menuAct->setIcon(createQIcon("delete_match_lines"));
  menuAct = createMenuXsheetAction(MI_DeleteInk, tr("&Delete Lines..."), "");
  menuAct->setIcon(createQIcon("delete_lines"));

  menuAct = createMenuXsheetAction(MI_MergeColumns, tr("&Merge Levels"), "");
  menuAct->setIcon(createQIcon("merge_levels"));

  menuAct = createMenuXsheetAction(MI_InsertFx, tr("&New FX..."), "Ctrl+F");
  menuAct->setIcon(createQIcon("fx_new"));

  menuAct = createMenuXsheetAction(MI_NewOutputFx, tr("&New Output"), "Alt+O");
  menuAct->setIcon(createQIcon("output"));

  menuAct = createMenuXsheetAction(MI_InsertSceneFrame, tr("Insert Frame"), "");
  menuAct->setIcon(createQIcon("insert_frame"));

  menuAct = createMenuXsheetAction(MI_RemoveSceneFrame, tr("Remove Frame"), "");
  menuAct->setIcon(createQIcon("remove_frame"));

  menuAct = createMenuXsheetAction(MI_InsertGlobalKeyframe,
                                   tr("Insert Multiple Keys"), "");
  menuAct->setIcon(createQIcon("insert_multiple_keys"));

  menuAct = createMenuXsheetAction(MI_RemoveGlobalKeyframe,
                                   tr("Remove Multiple Keys"), "");
  menuAct->setIcon(createQIcon("remove_multiple_keys"));

  menuAct =
      createMenuXsheetAction(MI_SetStartMarker, tr("Set Start Marker"), "");
  menuAct = createMenuXsheetAction(MI_SetStopMarker, tr("Set Stop Marker"), "");
  menuAct = createMenuXsheetAction(MI_ClearMarkers, tr("Remove Markers"), "");
  menuAct =
      createMenuXsheetAction(MI_SetAutoMarkers, tr("Set Auto Markers"), "");
  menuAct = createMenuXsheetAction(MI_PreviewThis,
                                   tr("Set Markers to Current Frame"), "");

  menuAct = createMenuLevelAction(MI_NewNoteLevel, tr("New Note Level"), "");
  menuAct->setIcon(createQIcon("new_note_level"));

  menuAct = createMenuXsheetAction(MI_RemoveEmptyColumns,
                                   tr("Remove Empty Columns"), "");
  menuAct->setIcon(createQIcon("clear"));
  createMenuXsheetAction(MI_LipSyncPopup, tr("&Apply Lip Sync to Column"),
                         "Alt+L");
  createRightClickMenuAction(MI_ToggleQuickToolbar, tr("Toggle Quick Toolbar"),
                             "");
  createRightClickMenuAction(MI_ToggleXsheetCameraColumn,
                             tr("Show/Hide Camera Column"), "");

  menuAct = createMenuCellsAction(MI_Reverse, tr("&Reverse"), "");
  menuAct->setIcon(createQIcon("reverse"));
  menuAct = createMenuCellsAction(MI_Swing, tr("&Swing"), "");
  menuAct->setIcon(createQIcon("swing"));
  menuAct = createMenuCellsAction(MI_Random, tr("&Random"), "");
  menuAct->setIcon(createQIcon("random"));
  menuAct = createMenuCellsAction(MI_Increment, tr("&Autoexpose"), "");
  menuAct->setIcon(createQIcon("autoexpose"));
  menuAct = createMenuCellsAction(MI_Dup, tr("&Repeat..."), "");
  menuAct->setIcon(createQIcon("repeat"));
  createMenuCellsAction(MI_ResetStep, tr("&Reset Step"), "");
  menuAct = createMenuCellsAction(MI_IncreaseStep, tr("&Increase Step"), "'");
  menuAct->setIcon(createQIcon("step_plus"));
  menuAct = createMenuCellsAction(MI_DecreaseStep, tr("&Decrease Step"), ";");
  menuAct->setIcon(createQIcon("step_minus"));
  menuAct = createMenuCellsAction(MI_Step2, tr("&Step 2"), "");
  menuAct->setIcon(createQIcon("step_2"));
  menuAct = createMenuCellsAction(MI_Step3, tr("&Step 3"), "");
  menuAct->setIcon(createQIcon("step_3"));
  menuAct = createMenuCellsAction(MI_Step4, tr("&Step 4"), "");
  menuAct->setIcon(createQIcon("step_4"));
  createMenuCellsAction(MI_Each2, tr("&Each 2"), "");
  createMenuCellsAction(MI_Each3, tr("&Each 3"), "");
  createMenuCellsAction(MI_Each4, tr("&Each 4"), "");
  menuAct = createMenuCellsAction(MI_Rollup, tr("&Roll Up"), "");
  menuAct->setIcon(createQIcon("rollup"));
  menuAct = createMenuCellsAction(MI_Rolldown, tr("&Roll Down"), "");
  menuAct->setIcon(createQIcon("rolldown"));
  menuAct = createMenuCellsAction(MI_TimeStretch, tr("&Time Stretch..."), "");
  menuAct->setIcon(createQIcon("time_stretch"));
  menuAct = createMenuCellsAction(MI_CreateBlankDrawing,
                                  tr("&Create Blank Drawing"), "Alt+D");
  menuAct->setIcon(createQIcon("add_cell"));
  menuAct =
      createMenuCellsAction(MI_Duplicate, tr("&Duplicate Drawing  "), "D");
  menuAct->setIcon(createQIcon("duplicate_drawing"));
  menuAct = createMenuCellsAction(MI_Autorenumber, tr("&Autorenumber"), "");
  menuAct->setIcon(createQIcon("renumber"));
  menuAct = createMenuCellsAction(MI_CloneLevel, tr("&Clone Cells"), "");
  menuAct->setIcon(createQIcon("clone_cells"));
  createMenuCellsAction(MI_DrawingSubForward,
                        tr("Drawing Substitution Forward"), "W");
  createMenuCellsAction(MI_DrawingSubBackward,
                        tr("Drawing Substitution Backward"), "Q");
  createMenuCellsAction(MI_DrawingSubGroupForward,
                        tr("Similar Drawing Substitution Forward"), "Alt+W");
  createMenuCellsAction(MI_DrawingSubGroupBackward,
                        tr("Similar Drawing Substitution Backward"), "Alt+Q");
  menuAct = createMenuCellsAction(MI_Reframe1, tr("Reframe on 1's"), "");
  menuAct->setIcon(createQIcon("on_1s"));
  menuAct = createMenuCellsAction(MI_Reframe2, tr("Reframe on 2's"), "");
  menuAct->setIcon(createQIcon("on_2s"));
  menuAct = createMenuCellsAction(MI_Reframe3, tr("Reframe on 3's"), "");
  menuAct->setIcon(createQIcon("on_3s"));
  menuAct = createMenuCellsAction(MI_Reframe4, tr("Reframe on 4's"), "");
  menuAct->setIcon(createQIcon("on_4s"));
  menuAct = createMenuCellsAction(MI_ReframeWithEmptyInbetweens,
                                  tr("Reframe with Empty Inbetweens..."), "");
  menuAct->setIcon(createQIcon("on_with_empty"));
  menuAct = createMenuCellsAction(MI_AutoInputCellNumber,
                                  tr("Auto Input Cell Number..."), "");
  menuAct->setIcon(createQIcon("auto_input_cell_number"));
  menuAct =
      createMenuCellsAction(MI_FillEmptyCell, tr("&Fill In Empty Cells"), "");
  menuAct->setIcon(createQIcon("fill_empty_cells"));

  menuAct = createRightClickMenuAction(MI_SetKeyframes, tr("&Set Key"), "Z");
  menuAct->setIcon(createQIcon("set_key"));
  menuAct = createRightClickMenuAction(MI_ShiftKeyframesDown,
                                       tr("&Shift Keys Down"), "");
  menuAct->setIcon(createQIcon("shift_keys_down"));
  menuAct =
      createRightClickMenuAction(MI_ShiftKeyframesUp, tr("&Shift Keys Up"), "");
  menuAct->setIcon(createQIcon("shift_keys_up"));

  createRightClickMenuAction(MI_PasteNumbers, tr("&Paste Numbers"), "");

  createToggle(MI_ViewCamera, tr("&Camera Box"), "",
               ViewCameraToggleAction ? 1 : 0, MenuViewCommandType);
  createToggle(MI_ViewTable, tr("&Table"), "", ViewTableToggleAction ? 1 : 0,
               MenuViewCommandType);
  createToggle(MI_FieldGuide, tr("&Grids and Overlays"), "Shift+G",
               FieldGuideToggleAction ? 1 : 0, MenuViewCommandType);
  createToggle(MI_ViewBBox, tr("&Raster Bounding Box"), "",
               ViewBBoxToggleAction ? 1 : 0, MenuViewCommandType);
  createToggle(MI_SafeArea, tr("&Safe Area"), "", SafeAreaToggleAction ? 1 : 0,
               MenuViewCommandType);
  createToggle(MI_ViewColorcard, tr("&Camera BG Color"), "",
               ViewColorcardToggleAction ? 1 : 0, MenuViewCommandType);
  createToggle(MI_ViewGuide, tr("&Guide"), "", ViewGuideToggleAction ? 1 : 0,
               MenuViewCommandType);
  createToggle(MI_ViewRuler, tr("&Ruler"), "", ViewRulerToggleAction ? 1 : 0,
               MenuViewCommandType);
  menuAct = createToggle(MI_TCheck, tr("&Transparency Check  "), "",
                         TCheckToggleAction ? 1 : 0, MenuViewCommandType);
  menuAct->setIcon(createQIcon("transparency_check"));

  QAction *inkCheckAction =
      createToggle(MI_ICheck, tr("&Ink Check"), "", ICheckToggleAction ? 1 : 0,
                   MenuViewCommandType);
  inkCheckAction->setIcon(createQIcon("ink_check"));
  QAction *ink1CheckAction =
      createToggle(MI_Ink1Check, tr("&Ink#1 Check"), "",
                   Ink1CheckToggleAction ? 1 : 0, MenuViewCommandType);
  ink1CheckAction->setIcon(createQIcon("ink_no1_check"));
  /*-- Ink Check と Ink1Checkを排他的にする --*/
  connect(inkCheckAction, SIGNAL(triggered(bool)), this,
          SLOT(onInkCheckTriggered(bool)));
  connect(ink1CheckAction, SIGNAL(triggered(bool)), this,
          SLOT(onInk1CheckTriggered(bool)));

  QAction *paintCheckAction =
      createToggle(MI_PCheck, tr("&Paint Check"), "",
                   PCheckToggleAction ? 1 : 0, MenuViewCommandType);
  paintCheckAction->setIcon(createQIcon("paint_check"));
  QAction *checkModesAction =
      createToggle(MI_IOnly, tr("Inks &Only"), "", IOnlyToggleAction ? 1 : 0,
                   MenuViewCommandType);
  checkModesAction->setIcon(createQIcon("inks_only"));
  QAction *fillCheckAction =
      createToggle(MI_GCheck, tr("&Fill Check"), "", GCheckToggleAction ? 1 : 0,
                   MenuViewCommandType);
  fillCheckAction->setIcon(createQIcon("fill_check"));
  QAction *blackBgCheckAction =
      createToggle(MI_BCheck, tr("&Black BG Check"), "",
                   BCheckToggleAction ? 1 : 0, MenuViewCommandType);
  blackBgCheckAction->setIcon(createQIcon("blackbg_check"));
  QAction *gapCheckAction =
      createToggle(MI_ACheck, tr("&Gap Check"), "", ACheckToggleAction ? 1 : 0,
                   MenuViewCommandType);
  gapCheckAction->setIcon(createQIcon("gap_check"));

  QAction *showStatusBarAction =
      createToggle(MI_ShowStatusBar, tr("&Show Status Bar"), "",
                   ShowStatusBarAction ? 1 : 0, MenuViewCommandType);
  connect(showStatusBarAction, SIGNAL(triggered(bool)), this,
          SLOT(toggleStatusBar(bool)));

  QAction *toggleTransparencyAction =
      createToggle(MI_ToggleTransparent, tr("&Toggle Transparency"), "", 0,
                   MenuViewCommandType);
  connect(toggleTransparencyAction, SIGNAL(triggered(bool)), this,
          SLOT(toggleTransparency(bool)));
  connect(m_transparencyTogglerWindow, &QDialog::finished, [=](int result) {
    toggleTransparency(false);
    toggleTransparencyAction->setChecked(false);
  });

  toggle = createToggle(MI_ShiftTrace, tr("Shift and Trace"), "", false,
                        MenuViewCommandType);
  toggle->setIcon(createQIcon("shift_and_trace"));

  toggle = createToggle(MI_EditShift, tr("Edit Shift"), "", false,
                        MenuViewCommandType);
  toggle->setIcon(createQIcon("shift_and_trace_edit"));
  toggle =
      createToggle(MI_NoShift, tr("No Shift"), "", false, MenuViewCommandType);
  toggle->setIcon(createQIcon("shift_and_trace_no_shift"));
  CommandManager::instance()->enable(MI_EditShift, false);
  CommandManager::instance()->enable(MI_NoShift, false);
  menuAct = createAction(MI_ResetShift, tr("Reset Shift"), "", "",
                         MenuViewCommandType);
  menuAct->setIcon(createQIcon("shift_and_trace_reset"));

  toggle = createToggle(MI_VectorGuidedDrawing, tr("Vector Guided Drawing"), "",
                        Preferences::instance()->isGuidedDrawingEnabled(),
                        MenuViewCommandType);

  if (QGLPixelBuffer::hasOpenGLPbuffers())
    createToggle(MI_RasterizePli, tr("&Visualize Vector As Raster"), "",
                 RasterizePliToggleAction ? 1 : 0, MenuViewCommandType);
  else
    RasterizePliToggleAction = 0;

  createRightClickMenuAction(MI_Histogram, tr("&Histogram"), "");

  // createToolOptionsAction("A_ToolOption_Link", tr("Link"), "");
  menuAct = createToggle(MI_Link, tr("Link Flipbooks"), "",
                         LinkToggleAction ? 1 : 0, MenuPlayCommandType);
  menuAct->setIcon(createQIcon("flipbook_link"));

  menuAct = createMenuPlayAction(MI_Play, tr("Play"), "P");
  menuAct->setIcon(createQIcon("play"));
  createMenuPlayAction(MI_ShortPlay, tr("Short Play"), "Alt+P");
  menuAct = createMenuPlayAction(MI_Loop, tr("Loop"), "L");
  menuAct->setIcon(createQIcon("loop"));
  menuAct = createMenuPlayAction(MI_Pause, tr("Pause"), "");
  menuAct->setIcon(createQIcon("pause"));
  menuAct = createMenuPlayAction(MI_FirstFrame, tr("First Frame"), "Alt+,");
  menuAct->setIcon(createQIcon("framefirst"));
  menuAct = createMenuPlayAction(MI_LastFrame, tr("Last Frame"), "Alt+.");
  menuAct->setIcon(createQIcon("framelast"));
  menuAct = createMenuPlayAction(MI_PrevFrame, tr("Previous Frame"), "Shift+,");
  menuAct->setIcon(createQIcon("frameprev"));
  menuAct = createMenuPlayAction(MI_NextFrame, tr("Next Frame"), "Shift+.");
  menuAct->setIcon(createQIcon("framenext"));
  menuAct = createMenuPlayAction(MI_NextDrawing, tr("Next Drawing"), ".");
  menuAct->setIcon(createQIcon("next_drawing"));
  menuAct = createMenuPlayAction(MI_PrevDrawing, tr("Previous Drawing"), ",");
  menuAct->setIcon(createQIcon("prev_drawing"));
  menuAct = createMenuPlayAction(MI_NextStep, tr("Next Step"), "");
  menuAct->setIcon(createQIcon("nextstep"));
  menuAct = createMenuPlayAction(MI_PrevStep, tr("Previous Step"), "");
  menuAct->setIcon(createQIcon("prevstep"));
  menuAct = createMenuPlayAction(MI_NextKeyframe, tr("Next Key"), "Ctrl+.");
  menuAct->setIcon(createQIcon("nextkey"));
  menuAct = createMenuPlayAction(MI_PrevKeyframe, tr("Previous Key"), "Ctrl+,");
  menuAct->setIcon(createQIcon("prevkey"));

  createRGBAAction(MI_RedChannel, tr("Red Channel"), "");
  createRGBAAction(MI_GreenChannel, tr("Green Channel"), "");
  createRGBAAction(MI_BlueChannel, tr("Blue Channel"), "");
  createRGBAAction(MI_MatteChannel, tr("Alpha Channel"), "");
  createRGBAAction(MI_RedChannelGreyscale, tr("Red Channel Greyscale"), "");
  createRGBAAction(MI_GreenChannelGreyscale, tr("Green Channel Greyscale"), "");
  createRGBAAction(MI_BlueChannelGreyscale, tr("Blue Channel Greyscale"), "");
  /*-- Viewer下部のCompareToSnapshotボタンのトグル --*/
  createViewerAction(MI_CompareToSnapshot, tr("Compare to Snapshot"), "");
  createFillAction(MI_AutoFillToggle,
                   tr("Toggle Autofill on Current Palette Color"), "Shift+A");

  // toggle =
  //     createToggle(MI_DockingCheck, tr("&Lock Room Panes"), "",
  //                  DockingCheckToggleAction ? 1 : 0, MenuWindowsCommandType);
  // DockingCheck::instance()->setToggle(toggle);

  // createRightClickMenuAction(MI_OpenCurrentScene,   tr("&Current Scene"),
  // "");

  createMenuWindowsAction(MI_OpenExport, tr("&Export"), "");

  menuAct =
      createMenuWindowsAction(MI_OpenFileBrowser, tr("&File Browser"), "");
  menuAct->setIcon(createQIcon("filebrowser"));
  menuAct = createMenuWindowsAction(MI_OpenFileViewer, tr("&Flipbook"), "");
  menuAct->setIcon(createQIcon("flipbook"));
  menuAct = createMenuWindowsAction(MI_OpenFunctionEditor,
                                    tr("&Function Editor"), "");
  menuAct->setIcon(createQIcon("function_editor"));
  createMenuWindowsAction(MI_OpenFilmStrip, tr("&Level Strip"), "");
  menuAct = createMenuWindowsAction(MI_OpenPalette, tr("&Palette"), "");
  menuAct->setIcon(createQIcon("palette"));
  menuAct =
      createRightClickMenuAction(MI_OpenPltGizmo, tr("&Palette Gizmo"), "");
  menuAct->setIcon(createQIcon("palettegizmo"));
  createRightClickMenuAction(MI_EraseUnusedStyles, tr("&Delete Unused Styles"),
                             "");
  menuAct = createMenuWindowsAction(MI_OpenTasks, tr("&Tasks"), "");
  menuAct->setIcon(createQIcon("tasks"));
  menuAct =
      createMenuWindowsAction(MI_OpenBatchServers, tr("&Batch Servers"), "");
  menuAct->setIcon(createQIcon("batchservers"));
  menuAct = createMenuWindowsAction(MI_OpenTMessage, tr("&Message Center"), "");
  menuAct->setIcon(createQIcon("messagecenter"));
  menuAct = createMenuWindowsAction(MI_OpenColorModel, tr("&Color Model"), "");
  menuAct->setIcon(createQIcon("colormodel"));
  menuAct =
      createMenuWindowsAction(MI_OpenStudioPalette, tr("&Studio Palette"), "");
  menuAct->setIcon(createQIcon("studiopalette"));

  menuAct = createMenuWindowsAction(MI_OpenSchematic, tr("&Schematic"), "");
  menuAct->setIcon(createQIcon("schematic"));
  menuAct =
      createMenuWindowsAction(MI_FxParamEditor, tr("&FX Editor"), "Ctrl+K");
  menuAct->setIcon(createQIcon("fx_settings"));
  menuAct = createMenuWindowsAction(MI_OpenCleanupSettings,
                                    tr("&Cleanup Settings"), "");
  menuAct->setIcon(createQIcon("cleanup_settings"));

  menuAct = createMenuWindowsAction(MI_OpenFileBrowser2, tr("&Scene Cast"), "");
  menuAct->setIcon(createQIcon("scenecast"));
  menuAct =
      createMenuWindowsAction(MI_OpenStyleControl, tr("&Style Editor"), "");
  menuAct->setIcon(createQIcon("styleeditor"));
  createMenuWindowsAction(MI_OpenToolbar, tr("&Toolbar"), "");
  createMenuWindowsAction(MI_OpenToolOptionBar, tr("&Tool Option Bar"), "");
  createMenuWindowsAction(MI_OpenCommandToolbar, tr("&Command Bar"), "");
  menuAct = createMenuWindowsAction(MI_OpenStopMotionPanel,
                                    tr("&Stop Motion Controls"), "");
  menuAct->setIcon(createQIcon("camera_capture"));
  createMenuWindowsAction(MI_OpenMotionPathPanel, tr("&Motion Paths"), "");

  menuAct = createMenuWindowsAction(MI_OpenLevelView, tr("&Viewer"), "");
  menuAct->setIcon(createQIcon("viewer"));
  menuAct = createMenuWindowsAction(MI_OpenXshView, tr("&Xsheet"), "");
  menuAct->setIcon(createQIcon("xsheet"));
  menuAct = createMenuWindowsAction(MI_OpenTimelineView, tr("&Timeline"), "");
  menuAct->setIcon(createQIcon("timeline"));
  //  createAction(MI_TestAnimation,     "Test Animation",   "Ctrl+Return");
  //  createAction(MI_Export,            "Export",           "Ctrl+E");

  menuAct = createMenuWindowsAction(MI_OpenComboViewer, tr("&ComboViewer"), "");
  menuAct->setIcon(createQIcon("viewer"));
  menuAct =
      createMenuWindowsAction(MI_OpenHistoryPanel, tr("&History"), "Ctrl+H");
  menuAct->setIcon(createQIcon("history"));
  menuAct =
      createMenuWindowsAction(MI_AudioRecording, tr("Record Audio"), "Alt+A");
  menuAct->setIcon(createQIcon("recordaudio"));
  createMenuWindowsAction(MI_ResetRoomLayout, tr("&Reset All Default Rooms"),
                          "");
  menuAct = createMenuWindowsAction(MI_MaximizePanel,
                                    tr("Toggle Maximize Panel"), "`");
  menuAct->setIcon(createQIcon("fit_to_window"));
  menuAct = createMenuWindowsAction(MI_FullScreenWindow,
                                    tr("Toggle Main Window's Full Screen Mode"),
                                    "Ctrl+`");
  menuAct->setIcon(createQIcon("toggle_fullscreen"));

  menuAct = createMenuHelpAction(MI_About, tr("&About Tahoma2D..."), "");
  menuAct->setIconText(tr("About Tahoma2D..."));
  menuAct->setIcon(createQIcon("info"));

  menuAct = createMenuWindowsAction(MI_StartupPopup, tr("&Startup Popup..."),
                                    "Alt+S");
  // menuAct->setIcon(createQIcon("opentoonz"));

  menuAct =
      createMenuHelpAction(MI_OpenOnlineManual, tr("&Online Manual..."), "F1");
  menuAct->setIconText(tr("Online Manual..."));
  menuAct->setIcon(createQIcon("manual"));

  menuAct = createMenuHelpAction(MI_OpenWhatsNew, tr("&What's New..."), "");
  menuAct->setIconText(tr("What's New..."));
  menuAct->setIcon(createQIcon("web"));

  menuAct =
      createMenuHelpAction(MI_SupportTahoma2D, tr("&Support Tahoma2D..."), "");
  menuAct->setIconText(tr("Support Tahoma2D"));
  menuAct->setIcon(createQIcon("web"));

  menuAct = createMenuHelpAction(MI_OpenCommunityForum,
                                 tr("&Community Forum..."), "");
  menuAct->setIconText(tr("Community Forum..."));
  menuAct->setIcon(createQIcon("web"));

  menuAct = createMenuHelpAction(MI_OpenReportABug, tr("&Report a Bug..."), "");
  menuAct->setIconText(tr("Report a Bug..."));
  menuAct->setIcon(createQIcon("web"));

  createMenuWindowsAction(MI_OpenGuidedDrawingControls,
                          tr("Guided Drawing Controls"), "");

  createRightClickMenuAction(MI_BlendColors, tr("&Blend colors"), "");

  toggle = createToggle(MI_OnionSkin, tr("Onion Skin Toggle"), "/", false,
                        RightClickMenuCommandType);
  toggle->setIcon(createQIcon("onionskin_toggle"));

  createToggle(MI_ZeroThick, tr("Zero Thick Lines"), "Shift+/", false,
               RightClickMenuCommandType);
  createToggle(MI_CursorOutline, tr("Toggle Cursor Size Outline"), "", false,
               RightClickMenuCommandType);

  createRightClickMenuAction(MI_ToggleCurrentTimeIndicator,
                             tr("Toggle Current Time Indicator"), "");

  // createRightClickMenuAction(MI_LoadSubSceneFile,     tr("Load As
  // Sub-xsheet"),   "");
  // createRightClickMenuAction(MI_LoadResourceFile,     tr("Load"),
  // "");

  menuAct = createRightClickMenuAction(MI_DuplicateFile, tr("Duplicate"), "");
  menuAct->setIcon(createQIcon("duplicate"));

  createRightClickMenuAction(MI_ShowFolderContents, tr("Show Folder Contents"),
                             "");
  createRightClickMenuAction(MI_ConvertFiles, tr("Convert..."), "");
  createRightClickMenuAction(MI_CollectAssets, tr("Collect Assets"), "");
  createRightClickMenuAction(MI_ImportScenes, tr("Import Scene"), "");
  createRightClickMenuAction(MI_ExportScenes, tr("Export Scene..."), "");

  // createRightClickMenuAction(MI_PremultiplyFile,      tr("Premultiply"),
  // "");
  createMenuLevelAction(MI_ConvertToVectors, tr("Convert to Vectors..."), "");
  createMenuLevelAction(MI_ConvertToToonzRaster, tr("Vectors to Smart Raster"),
                        "");
  createMenuLevelAction(MI_ConvertVectorToVector, tr("Simplify Vectors"), "");

  menuAct = createMenuLevelAction(MI_Tracking, tr("Tracking..."), "");
  menuAct->setIcon(createQIcon("tracking_options"));

  menuAct = createRightClickMenuAction(MI_RemoveLevel, tr("Remove Level"), "");
  menuAct->setIcon(createQIcon("remove_level"));

  menuAct = createRightClickMenuAction(MI_AddToBatchRenderList,
                                       tr("Add As Render Task"), "");
  menuAct->setIcon(createQIcon("render_add"));
  menuAct = createRightClickMenuAction(MI_AddToBatchCleanupList,
                                       tr("Add As Cleanup Task"), "");
  menuAct->setIcon(createQIcon("cleanup_add"));

  createRightClickMenuAction(MI_SelectRowKeyframes,
                             tr("Select All Keys in this Frame"), "");
  createRightClickMenuAction(MI_SelectColumnKeyframes,
                             tr("Select All Keys in this Column"), "");
  createRightClickMenuAction(MI_SelectAllKeyframes, tr("Select All Keys"), "");
  createRightClickMenuAction(MI_SelectAllKeyframesNotBefore,
                             tr("Select All Following Keys"), "");
  createRightClickMenuAction(MI_SelectAllKeyframesNotAfter,
                             tr("Select All Previous Keys"), "");
  createRightClickMenuAction(MI_SelectPreviousKeysInColumn,
                             tr("Select Previous Keys in this Column"), "");
  createRightClickMenuAction(MI_SelectFollowingKeysInColumn,
                             tr("Select Following Keys in this Column"), "");
  createRightClickMenuAction(MI_SelectPreviousKeysInRow,
                             tr("Select Previous Keys in this Frame"), "");
  createRightClickMenuAction(MI_SelectFollowingKeysInRow,
                             tr("Select Following Keys in this Frame"), "");
  createRightClickMenuAction(MI_InvertKeyframeSelection,
                             tr("Invert Key Selection"), "");

  createRightClickMenuAction(MI_SetAcceleration, tr("Set Acceleration"), "");
  createRightClickMenuAction(MI_SetDeceleration, tr("Set Deceleration"), "");
  createRightClickMenuAction(MI_SetConstantSpeed, tr("Set Constant Speed"), "");
  createRightClickMenuAction(MI_ResetInterpolation, tr("Reset Interpolation"),
                             "");

  createRightClickMenuAction(MI_UseLinearInterpolation,
                             tr("Linear Interpolation"), "");
  createRightClickMenuAction(MI_UseSpeedInOutInterpolation,
                             tr("Speed In / Speed Out Interpolation"), "");
  createRightClickMenuAction(MI_UseEaseInOutInterpolation,
                             tr("Ease In / Ease Out Interpolation"), "");
  createRightClickMenuAction(MI_UseEaseInOutPctInterpolation,
                             tr("Ease In / Ease Out (%) Interpolation"), "");
  createRightClickMenuAction(MI_UseExponentialInterpolation,
                             tr("Exponential Interpolation"), "");
  createRightClickMenuAction(MI_UseExpressionInterpolation,
                             tr("Expression Interpolation"), "");
  createRightClickMenuAction(MI_UseFileInterpolation, tr("File Interpolation"),
                             "");
  createRightClickMenuAction(MI_UseConstantInterpolation,
                             tr("Constant Interpolation"), "");

  menuAct = createRightClickMenuAction(MI_FoldColumns, tr("Fold Column"), "");
  menuAct->setIcon(createQIcon("fold_column"));

  createRightClickMenuAction(MI_ActivateThisColumnOnly, tr("Show This Only"),
                             "");
  createRightClickMenuAction(MI_ActivateSelectedColumns, tr("Show Selected"),
                             "");
  createRightClickMenuAction(MI_ActivateAllColumns, tr("Show All"), "");
  createRightClickMenuAction(MI_DeactivateSelectedColumns, tr("Hide Selected"),
                             "");
  createRightClickMenuAction(MI_DeactivateAllColumns, tr("Hide All"), "");
  createRightClickMenuAction(MI_ToggleColumnsActivation, tr("Toggle Show/Hide"),
                             "");
  createRightClickMenuAction(MI_EnableThisColumnOnly, tr("ON This Only"), "");
  createRightClickMenuAction(MI_EnableSelectedColumns, tr("ON Selected"), "");
  createRightClickMenuAction(MI_EnableAllColumns, tr("ON All"), "");
  createRightClickMenuAction(MI_DisableAllColumns, tr("OFF All"), "");
  createRightClickMenuAction(MI_DisableSelectedColumns, tr("OFF Selected"), "");
  createRightClickMenuAction(MI_SwapEnabledColumns, tr("Swap ON/OFF"), "");
  createRightClickMenuAction(MI_LockThisColumnOnly, tr("Lock This Only"),
                             "Shift+L");
  createRightClickMenuAction(MI_LockSelectedColumns, tr("Lock Selected"),
                             "Ctrl+Shift+L");
  createRightClickMenuAction(MI_LockAllColumns, tr("Lock All"),
                             "Ctrl+Alt+Shift+L");
  createRightClickMenuAction(MI_UnlockSelectedColumns, tr("Unlock Selected"),
                             "Ctrl+Shift+U");
  createRightClickMenuAction(MI_UnlockAllColumns, tr("Unlock All"),
                             "Ctrl+Alt+Shift+U");
  createRightClickMenuAction(MI_ToggleColumnLocks, tr("Swap Lock/Unlock"), "");
  /*-- カレントカラムの右側のカラムを全て非表示にするコマンド --*/
  createRightClickMenuAction(MI_DeactivateUpperColumns,
                             tr("Hide Upper Columns"), "");

  createRightClickMenuAction(MI_SeparateColors, tr("Separate Colors..."), "");

  createToolAction(T_Edit, "animate", tr("Animate Tool"), "A",
                   tr("Animate Tool: Modifies the position, rotation and size "
                      "of the current column"));
  createToolAction(
      T_Selection, "selection", tr("Selection Tool"), "S",
      tr("Selection Tool: Select parts of your image to transform it."));
  createToolAction(T_Brush, "brush", tr("Brush Tool"), "B",
                   tr("Brush Tool: Draws in the work area freehand"));
  createToolAction(T_Geometric, "geometric", tr("Geometry Tool"), "G",
                   tr("Geometry Tool: Draws geometric shapes"));
  createToolAction(T_Type, "type", tr("Type Tool"), "Y",
                   tr("Type Tool: Adds text"));
  createToolAction(T_Fill, "fill", tr("Fill Tool"), "F",
                   tr("Fill Tool: Fills drawing areas with the current style"));
  createToolAction(
      T_PaintBrush, "paintbrush", tr("Smart Raster Paint Tool"), "",
      tr("Smart Raster Paint: Paints areas in Smart Raster levels"));
  createToolAction(T_Eraser, "eraser", tr("Eraser Tool"), "E",
                   tr("Eraser Tool: Erases lines and areas"));
  createToolAction(
      T_Tape, "tape", tr("Tape Tool"), "T",
      tr("Tape Tool: Closes gaps in raster, joins edges in vector"));
  createToolAction(T_StylePicker, "stylepicker", tr("Style Picker Tool"), "K",
                   tr("Style Picker: Selects style on current drawing"));
  createToolAction(
      T_RGBPicker, "rgbpicker", tr("RGB Picker Tool"), "R",
      tr("RBG Picker: Picks color on screen and applies to current style"));
  createToolAction(T_ControlPointEditor, "controlpointeditor",
                   tr("Control Point Editor Tool"), "C",
                   tr("Control Point Editor: Modifies vector lines by editing "
                      "its control points"));
  createToolAction(T_Pinch, "pinch", tr("Pinch Tool"), "M",
                   tr("Pinch Tool: Pulls vector drawings"));
  createToolAction(T_Pump, "pump", tr("Pump Tool"), "",
                   tr("Pump Tool: Changes vector thickness"));
  createToolAction(T_Magnet, "magnet", tr("Magnet Tool"), "",
                   tr("Magnet Tool: Deforms vector lines"));
  createToolAction(
      T_Bender, "bender", tr("Bender Tool"), "",
      tr("Bender Tool: Bends vector shapes around the first click"));
  createToolAction(T_Iron, "iron", tr("Iron Tool"), "",
                   tr("Iron Tool: Smooths out vector lines"));
  createToolAction(T_Cutter, "cutter", tr("Cutter Tool"), "",
                   tr("Cutter Tool: Splits vector lines"));
  createToolAction(T_Skeleton, "skeleton", tr("Skeleton Tool"), "V",
                   tr("Skeleton Tool: Allows to build a skeleton and animate "
                      "in a cut-out workflow"));
  createToolAction(
      T_Tracker, "radar", tr("Tracker Tool"), "",
      tr("Tracker Tool: Tracks specific regions in a sequence of images"));
  createToolAction(T_Hook, "hook", tr("Hook Tool"), "O");
  createToolAction(T_Zoom, "zoom", tr("Zoom Tool"), "",
                   tr("Zoom Tool: Zooms viewer"));
  createToolAction(T_Rotate, "rotate", tr("Rotate Tool"), "",
                   tr("Rotate Tool: Rotate the viewer"));
  createToolAction(T_Hand, "hand", tr("Hand Tool"), "",
                   tr("Hand Tool: Pans the workspace (Space)"));
  createToolAction(T_Plastic, "plastic", tr("Plastic Tool"), "X",
                   tr("Plastic Tool: Builds a mesh that allows to deform and "
                      "animate a level"));
  createToolAction(T_Ruler, "ruler", tr("Ruler Tool"), "",
                   tr("Ruler Tool: Measure distances on the canvas"));
  createToolAction(T_Finger, "finger", tr("Finger Tool"), "",
                   tr("Finger Tool: Smudges small areas to cover with line"));

  createViewerAction(V_ZoomIn, tr("Zoom In"), "+");
  createViewerAction(V_ZoomOut, tr("Zoom Out"), "-");
  createViewerAction(V_ViewReset, tr("Reset View"), "Alt+0");
  createViewerAction(V_ZoomFit, tr("Fit to Window"), "Alt+9");
  createViewerAction(V_ZoomReset, tr("Reset Zoom"), "");
  createViewerAction(V_RotateReset, tr("Reset Rotation"), "");
  createViewerAction(V_PositionReset, tr("Reset Position"), "");

  createViewerAction(V_ActualPixelSize, tr("Actual Pixel Size"), "N");
  createViewerAction(V_FlipX, tr("Flip Viewer Horizontally"), "");
  createViewerAction(V_FlipY, tr("Flip Viewer Vertically"), "");
  createViewerAction(V_ShowHideFullScreen, tr("Show//Hide Full Screen"),
                     "Alt+F");
  CommandManager::instance()->setToggleTexts(V_ShowHideFullScreen,
                                             tr("Full Screen Mode"),
                                             tr("Exit Full Screen Mode"));
  createToolOptionsAction(MI_SelectNextGuideStroke,
                          tr("Select Next Frame Guide Stroke"), "");
  createToolOptionsAction(MI_SelectPrevGuideStroke,
                          tr("Select Previous Frame Guide Stroke"), "");
  createToolOptionsAction(MI_SelectBothGuideStrokes,
                          tr("Select Prev && Next Frame Guide Strokes"), "");
  createToolOptionsAction(MI_SelectGuideStrokeReset,
                          tr("Reset Guide Stroke Selections"), "");
  createToolOptionsAction(MI_TweenGuideStrokes,
                          tr("Tween Selected Guide Strokes"), "");
  createToolOptionsAction(MI_TweenGuideStrokeToSelected,
                          tr("Tween Guide Strokes to Selected"), "");
  createToolOptionsAction(MI_SelectGuidesAndTweenMode,
                          tr("Select Guide Strokes && Tween Mode"), "");
  createToolOptionsAction(MI_FlipNextGuideStroke,
                          tr("Flip Next Guide Stroke Direction"), "");
  createToolOptionsAction(MI_FlipPrevGuideStroke,
                          tr("Flip Previous Guide Stroke Direction"), "");

  // Following actions are for adding "Visualization" menu items to the command
  // bar. They are separated from the original actions in order to avoid
  // assigning shortcut keys. They must be triggered only from pressing buttons
  // in the command bar. Assinging shortcut keys and registering as MenuItem
  // will break a logic of ShortcutZoomer. So here we register separate items
  // and bypass the command.
  menuAct = createVisualizationButtonAction(VB_ViewReset, tr("Reset View"));
  menuAct->setIcon(createQIcon("reset"));

  menuAct = createVisualizationButtonAction(VB_ZoomFit, tr("Fit to Window"));
  menuAct->setIcon(createQIcon("fit_to_window"));

  createVisualizationButtonAction(VB_ZoomReset, tr("Reset Zoom"));
  createVisualizationButtonAction(VB_RotateReset, tr("Reset Rotation"));
  createVisualizationButtonAction(VB_PositionReset, tr("Reset Position"));

  menuAct = createVisualizationButtonAction(VB_ActualPixelSize,
                                            tr("Actual Pixel Size"));
  menuAct->setIcon(createQIcon("actual_pixel_size"));

  menuAct =
      createVisualizationButtonAction(VB_FlipX, tr("Flip Viewer Horizontally"));
  menuAct->setIcon(createQIcon("fliphoriz_off"));

  menuAct =
      createVisualizationButtonAction(VB_FlipY, tr("Flip Viewer Vertically"));
  menuAct->setIcon(createQIcon("flipvert_off"));

  menuAct = createMiscAction(MI_RefreshTree, tr("Refresh Folder Tree"), "");
  menuAct->setIconText(tr("Refresh"));
  menuAct->setIcon(createQIcon("refresh"));

  createToolOptionsAction("A_ToolOption_GlobalKey", tr("Global Key"), "");

  createToolOptionsAction("A_IncreaseMaxBrushThickness",
                          tr("Brush size - Increase max"), "I");
  createToolOptionsAction("A_DecreaseMaxBrushThickness",
                          tr("Brush size - Decrease max"), "U");
  createToolOptionsAction("A_IncreaseMinBrushThickness",
                          tr("Brush size - Increase min"), "J");
  createToolOptionsAction("A_DecreaseMinBrushThickness",
                          tr("Brush size - Decrease min"), "H");
  createToolOptionsAction("A_IncreaseBrushHardness",
                          tr("Brush hardness - Increase"), "");
  createToolOptionsAction("A_DecreaseBrushHardness",
                          tr("Brush hardness - Decrease"), "");
  createToolOptionsAction("A_ToolOption_SnapSensitivity",
                          tr("Snap Sensitivity"), "");
  createToolOptionsAction("A_ToolOption_AutoGroup", tr("Auto Group"), "");
  createToolOptionsAction("A_ToolOption_BreakSharpAngles",
                          tr("Break sharp angles"), "");
  createToolOptionsAction("A_ToolOption_FrameRange", tr("Frame range"), "F6");
  createToolOptionsAction("A_ToolOption_IK", tr("Inverse Kinematics"), "");
  createToolOptionsAction("A_ToolOption_Invert", tr("Invert"), "");
  createToolOptionsAction("A_ToolOption_Manual", tr("Manual"), "");
  createToolOptionsAction("A_ToolOption_OnionSkin", tr("Onion skin"), "");
  createToolOptionsAction("A_ToolOption_Orientation", tr("Orientation"), "");
  createToolOptionsAction("A_ToolOption_PencilMode", tr("Pencil Mode"), "");
  createToolOptionsAction("A_ToolOption_PreserveThickness",
                          tr("Preserve Thickness"), "");
  createToolOptionsAction("A_ToolOption_PressureSensitivity",
                          tr("Pressure Sensitivity"), "Shift+P");
  createToolOptionsAction("A_ToolOption_SegmentInk", tr("Segment Ink"), "F8");
  createToolOptionsAction("A_ToolOption_Selective", tr("Selective"), "F7");
  createToolOptionsAction("A_ToolOption_DrawOrder",
                          tr("Brush Tool - Draw Order"), "");
  createToolOptionsAction("A_ToolOption_Smooth", tr("Smooth"), "");
  createToolOptionsAction("A_ToolOption_Snap", tr("Snap"), "");
  createToolOptionsAction("A_ToolOption_AutoClose", tr("Auto Close"), "");
  createToolOptionsAction("A_ToolOption_DrawUnder", tr("Draw Under"), "");
  createToolOptionsAction("A_ToolOption_AutoSelectDrawing",
                          tr("Auto Select Drawing"), "");
  createToolOptionsAction("A_ToolOption_Autofill", tr("Auto Fill"), "");
  createToolOptionsAction("A_ToolOption_JoinVectors", tr("Join Vectors"), "");
  createToolOptionsAction("A_ToolOption_ShowOnlyActiveSkeleton",
                          tr("Show Only Active Skeleton"), "");
  createToolOptionsAction("A_ToolOption_RasterEraser",
                          tr("Brush Tool - Eraser (Raster option)"), "");
  createToolOptionsAction("A_ToolOption_LockAlpha",
                          tr("Brush Tool - Lock Alpha"), "");

  // Option Menu
  createToolOptionsAction("A_ToolOption_BrushPreset", tr("Brush Preset"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape", tr("Geometric Shape"),
                          "");
  createToolOptionsAction("A_ToolOption_GeometricShape:Rectangle",
                          tr("Geometric Shape Rectangle"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape:Circle",
                          tr("Geometric Shape Circle"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape:Ellipse",
                          tr("Geometric Shape Ellipse"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape:Line",
                          tr("Geometric Shape Line"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape:Polyline",
                          tr("Geometric Shape Polyline"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape:Arc",
                          tr("Geometric Shape Arc"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape:MultiArc",
                          tr("Geometric Shape MultiArc"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape:Polygon",
                          tr("Geometric Shape Polygon"), "");
  createToolOptionsAction("A_ToolOption_GeometricEdge", tr("Geometric Edge"),
                          "");
  createToolOptionsAction("A_ToolOption_Mode", tr("Mode"), "");
  menuAct = createToolOptionsAction("A_ToolOption_Mode:Areas",
                                    tr("Mode - Areas"), "");
  menuAct->setIcon(createQIcon("mode_areas"));
  menuAct = createToolOptionsAction("A_ToolOption_Mode:Lines",
                                    tr("Mode - Lines"), "");
  menuAct->setIcon(createQIcon("mode_lines"));
  menuAct = createToolOptionsAction("A_ToolOption_Mode:Lines & Areas",
                                    tr("Mode - Lines && Areas"), "");
  menuAct->setIcon(createQIcon("mode_areas_lines"));
  createToolOptionsAction("A_ToolOption_Mode:Endpoint to Endpoint",
                          tr("Mode - Endpoint to Endpoint"), "");
  createToolOptionsAction("A_ToolOption_Mode:Endpoint to Line",
                          tr("Mode - Endpoint to Line"), "");
  createToolOptionsAction("A_ToolOption_Mode:Line to Line",
                          tr("Mode - Line to Line"), "");
  createToolOptionsAction("A_ToolOption_Type", tr("Type"), "");

  menuAct = createToolOptionsAction("A_ToolOption_Type:Normal",
                                    tr("Type - Normal"), "");
  menuAct->setIcon(createQIcon("type_normal"));

  menuAct = createToolOptionsAction("A_ToolOption_Type:Rectangular",
                                    tr("Type - Rectangular"), "F5");
  menuAct->setIcon(createQIcon("type_rectangular"));

  menuAct = createToolOptionsAction("A_ToolOption_Type:Freehand",
                                    tr("Type - Freehand"), "");
  menuAct->setIcon(createQIcon("type_lasso"));

  menuAct = createToolOptionsAction("A_ToolOption_Type:Polyline",
                                    tr("Type - Polyline"), "");
  menuAct->setIcon(createQIcon("type_polyline"));
  menuAct = createToolOptionsAction("A_ToolOption_Type:Segment",
                                    tr("Type - Segment"), "");
  menuAct->setIcon(createQIcon("type_erase_segment"));

  createToolOptionsAction("A_ToolOption_TypeFont", tr("TypeTool Font"), "");
  createToolOptionsAction("A_ToolOption_TypeSize", tr("TypeTool Size"), "");
  createToolOptionsAction("A_ToolOption_TypeStyle", tr("TypeTool Style"), "");
  createToolOptionsAction("A_ToolOption_TypeStyle:Oblique",
                          tr("TypeTool Style - Oblique"), "");
  createToolOptionsAction("A_ToolOption_TypeStyle:Regular",
                          tr("TypeTool Style - Regular"), "");
  createToolOptionsAction("A_ToolOption_TypeStyle:Bold Oblique",
                          tr("TypeTool Style - Bold Oblique"), "");
  createToolOptionsAction("A_ToolOption_TypeStyle:Bold",
                          tr("TypeTool Style - Bold"), "");

  createToolOptionsAction("A_ToolOption_EditToolActiveAxis", tr("Active Axis"),
                          "");
  createToolOptionsAction("A_ToolOption_EditToolActiveAxis:Position",
                          tr("Active Axis - Position"), "");
  createToolOptionsAction("A_ToolOption_EditToolActiveAxis:Rotation",
                          tr("Active Axis - Rotation"), "");
  createToolOptionsAction("A_ToolOption_EditToolActiveAxis:Scale",
                          tr("Active Axis - Scale"), "");
  createToolOptionsAction("A_ToolOption_EditToolActiveAxis:Shear",
                          tr("Active Axis - Shear"), "");
  createToolOptionsAction("A_ToolOption_EditToolActiveAxis:Center",
                          tr("Active Axis - Center"), "");
  createToolOptionsAction("A_ToolOption_EditToolActiveAxis:All",
                          tr("Active Axis - All"), "");

  createToolOptionsAction("A_ToolOption_SkeletonMode", tr("Skeleton Mode"), "");
  createToolOptionsAction("A_ToolOption_SkeletonMode:Edit Mesh",
                          tr("Edit Mesh Mode"), "");
  createToolOptionsAction("A_ToolOption_SkeletonMode:Paint Rigid",
                          tr("Paint Rigid Mode"), "");
  createToolOptionsAction("A_ToolOption_SkeletonMode:Build Skeleton",
                          tr("Build Skeleton Mode"), "");
  createToolOptionsAction("A_ToolOption_SkeletonMode:Animate",
                          tr("Animate Mode"), "");
  createToolOptionsAction("A_ToolOption_SkeletonMode:Inverse Kinematics",
                          tr("Inverse Kinematics Mode"), "");
  createToolOptionsAction("A_ToolOption_AutoSelect:None", tr("None Pick Mode"),
                          "");
  createToolOptionsAction("A_ToolOption_AutoSelect:Column",
                          tr("Column Pick Mode"), "");
  createToolOptionsAction("A_ToolOption_AutoSelect:Pegbar",
                          tr("Pegbar Pick Mode"), "");
  menuAct =
      createToolOptionsAction("A_ToolOption_PickScreen", tr("Pick Screen"), "");
  menuAct->setIcon(createQIcon("pickscreen"));
  createToolOptionsAction("A_ToolOption_Meshify", tr("Create Mesh"), "");

  menuAct = createToolOptionsAction("A_ToolOption_AutopaintLines",
                                    tr("Fill Tool - Autopaint Lines"), "");
  menuAct->setIcon(createQIcon("fill_auto"));

  /*-- Animate tool + mode switching shortcuts --*/
  createAction(MI_EditNextMode, tr("Animate Tool - Next Mode"), "", "",
               ToolCommandType);
  createAction(MI_EditPosition, tr("Animate Tool - Position"), "", "",
               ToolCommandType);
  createAction(MI_EditRotation, tr("Animate Tool - Rotation"), "", "",
               ToolCommandType);
  createAction(MI_EditScale, tr("Animate Tool - Scale"), "", "",
               ToolCommandType);
  createAction(MI_EditShear, tr("Animate Tool - Shear"), "", "",
               ToolCommandType);
  createAction(MI_EditCenter, tr("Animate Tool - Center"), "", "",
               ToolCommandType);
  createAction(MI_EditAll, tr("Animate Tool - All"), "", "", ToolCommandType);

  /*-- Selection tool + type switching shortcuts --*/
  createAction(MI_SelectionNextType, tr("Selection Tool - Next Type"), "", "",
               ToolCommandType);
  createAction(MI_SelectionRectangular, tr("Selection Tool - Rectangular"), "",
               "", ToolCommandType);
  createAction(MI_SelectionFreehand, tr("Selection Tool - Freehand"), "", "",
               ToolCommandType);
  createAction(MI_SelectionPolyline, tr("Selection Tool - Polyline"), "", "",
               ToolCommandType);

  /*-- Geometric tool + shape switching shortcuts --*/
  createAction(MI_GeometricNextShape, tr("Geometric Tool - Next Shape"), "", "",
               ToolCommandType);
  createAction(MI_GeometricRectangle, tr("Geometric Tool - Rectangle"), "", "",
               ToolCommandType);
  createAction(MI_GeometricCircle, tr("Geometric Tool - Circle"), "", "",
               ToolCommandType);
  createAction(MI_GeometricEllipse, tr("Geometric Tool - Ellipse"), "", "",
               ToolCommandType);
  createAction(MI_GeometricLine, tr("Geometric Tool - Line"), "", "",
               ToolCommandType);
  createAction(MI_GeometricPolyline, tr("Geometric Tool - Polyline"), "", "",
               ToolCommandType);
  createAction(MI_GeometricArc, tr("Geometric Tool - Arc"), "", "",
               ToolCommandType);
  createAction(MI_GeometricMultiArc, tr("Geometric Tool - MultiArc"), "", "",
               ToolCommandType);
  createAction(MI_GeometricPolygon, tr("Geometric Tool - Polygon"), "", "",
               ToolCommandType);

  /*-- Type tool + style switching shortcuts --*/
  createAction(MI_TypeNextStyle, tr("Type Tool - Next Style"), "", "",
               ToolCommandType);
  createAction(MI_TypeOblique, tr("Type Tool - Oblique"), "", "",
               ToolCommandType);
  createAction(MI_TypeRegular, tr("Type Tool - Regular"), "", "",
               ToolCommandType);
  createAction(MI_TypeBoldOblique, tr("Type Tool - Bold Oblique"), "", "",
               ToolCommandType);
  createAction(MI_TypeBold, tr("Type Tool - Bold"), "", "", ToolCommandType);

  /*-- Fill tool + type/mode switching shortcuts --*/
  createAction(MI_FillNextType, tr("Fill Tool - Next Type"), "", "",
               ToolCommandType);

  menuAct = createAction(MI_FillNormal, tr("Fill Tool - Normal"), "", "",
                         ToolCommandType);
  menuAct->setIcon(createQIcon("fill_normal"));
  menuAct = createAction(MI_FillRectangular, tr("Fill Tool - Rectangular"), "",
                         "", ToolCommandType);
  menuAct->setIcon(createQIcon("fill_rectangular"));
  menuAct = createAction(MI_FillFreehand, tr("Fill Tool - Freehand"), "", "",
                         ToolCommandType);
  menuAct->setIcon(createQIcon("fill_freehand"));
  menuAct = createAction(MI_FillPolyline, tr("Fill Tool - Polyline"), "", "",
                         ToolCommandType);
  menuAct->setIcon(createQIcon("fill_polyline"));
  createAction(MI_FillNextMode, tr("Fill Tool - Next Mode"), "", "",
               ToolCommandType);
  createAction(MI_FillAreas, tr("Fill Tool - Areas"), "", "", ToolCommandType);
  createAction(MI_FillLines, tr("Fill Tool - Lines"), "", "", ToolCommandType);
  createAction(MI_FillLinesAndAreas, tr("Fill Tool - Lines & Areas"), "", "",
               ToolCommandType);

  /*-- Eraser tool + type switching shortcuts --*/
  createAction(MI_EraserNextType, tr("Eraser Tool - Next Type"), "", "",
               ToolCommandType);
  createAction(MI_EraserNormal, tr("Eraser Tool - Normal"), "", "",
               ToolCommandType);
  createAction(MI_EraserRectangular, tr("Eraser Tool - Rectangular"), "", "",
               ToolCommandType);
  createAction(MI_EraserFreehand, tr("Eraser Tool - Freehand"), "", "",
               ToolCommandType);
  createAction(MI_EraserPolyline, tr("Eraser Tool - Polyline"), "", "",
               ToolCommandType);
  createAction(MI_EraserSegment, tr("Eraser Tool - Segment"), "", "",
               ToolCommandType);

  /*-- Tape tool + type/mode switching shortcuts --*/
  createAction(MI_TapeNextType, tr("Tape Tool - Next Type"), "", "",
               ToolCommandType);
  createAction(MI_TapeNormal, tr("Tape Tool - Normal"), "", "",
               ToolCommandType);
  createAction(MI_TapeRectangular, tr("Tape Tool - Rectangular"), "", "",
               ToolCommandType);
  createAction(MI_TapeNextMode, tr("Tape Tool - Next Mode"), "", "",
               ToolCommandType);
  createAction(MI_TapeEndpointToEndpoint,
               tr("Tape Tool - Endpoint to Endpoint"), "", "", ToolCommandType);
  createAction(MI_TapeEndpointToLine, tr("Tape Tool - Endpoint to Line"), "",
               "", ToolCommandType);
  createAction(MI_TapeLineToLine, tr("Tape Tool - Line to Line"), "", "",
               ToolCommandType);

  /*-- Style Picker tool + mode switching shortcuts --*/
  createAction(MI_PickStyleNextMode, tr("Style Picker Tool - Next Mode"), "",
               "", ToolCommandType);
  createAction(MI_PickStyleAreas, tr("Style Picker Tool - Areas"), "", "",
               ToolCommandType);
  createAction(MI_PickStyleLines, tr("Style Picker Tool - Lines"), "", "",
               ToolCommandType);
  createAction(MI_PickStyleLinesAndAreas,
               tr("Style Picker Tool - Lines & Areas"), "", "",
               ToolCommandType);

  /*-- RGB Picker tool + type switching shortcuts --*/
  createAction(MI_RGBPickerNextType, tr("RGB Picker Tool - Next Type"), "", "",
               ToolCommandType);
  createAction(MI_RGBPickerNormal, tr("RGB Picker Tool - Normal"), "", "",
               ToolCommandType);
  createAction(MI_RGBPickerRectangular, tr("RGB Picker Tool - Rectangular"), "",
               "", ToolCommandType);
  createAction(MI_RGBPickerFreehand, tr("RGB Picker Tool - Freehand"), "", "",
               ToolCommandType);
  createAction(MI_RGBPickerPolyline, tr("RGB Picker Tool - Polyline"), "", "",
               ToolCommandType);

  /*-- Skeleton tool + mode switching shortcuts --*/
  createAction(MI_SkeletonNextMode, tr("Skeleton Tool - Next Mode"), "", "",
               ToolCommandType);
  createAction(MI_SkeletonBuildSkeleton, tr("Skeleton Tool - Build Skeleton"),
               "", "", ToolCommandType);
  createAction(MI_SkeletonAnimate, tr("Skeleton Tool - Animate"), "", "",
               ToolCommandType);
  createAction(MI_SkeletonInverseKinematics,
               tr("Skeleton Tool - Inverse Kinematics"), "", "",
               ToolCommandType);

  /*-- Plastic tool + mode switching shortcuts --*/
  createAction(MI_PlasticNextMode, tr("Plastic Tool - Next Mode"), "", "",
               ToolCommandType);
  createAction(MI_PlasticEditMesh, tr("Plastic Tool - Edit Mesh"), "", "",
               ToolCommandType);
  createAction(MI_PlasticPaintRigid, tr("Plastic Tool - Paint Rigid"), "", "",
               ToolCommandType);
  createAction(MI_PlasticBuildSkeleton, tr("Plastic Tool - Build Skeleton"), "",
               "", ToolCommandType);
  createAction(MI_PlasticAnimate, tr("Plastic Tool - Animate"), "", "",
               ToolCommandType);

  /*-- Brush tool + mode switching shortcuts --*/
  createAction(MI_BrushAutoFillOn, tr("Brush Tool - Auto Fill On"), "", "",
               ToolCommandType);
  createAction(MI_BrushAutoFillOff, tr("Brush Tool - Auto Fill Off"), "", "",
               ToolCommandType);

  createMiscAction("A_FxSchematicToggle", tr("Toggle FX/Stage schematic"), "");

  createStopMotionAction(MI_StopMotionCapture, tr("Capture Stop Motion Frame"),
                         "");
  createStopMotionAction(MI_StopMotionRaiseOpacity,
                         tr("Raise Stop Motion Opacity"), "");
  createStopMotionAction(MI_StopMotionLowerOpacity,
                         tr("Lower Stop Motion Opacity"), "");
  createStopMotionAction(MI_StopMotionToggleLiveView,
                         tr("Toggle Stop Motion Live View"), "");
#ifdef WITH_CANON
  createStopMotionAction(MI_StopMotionToggleZoom, tr("Toggle Stop Motion Zoom"),
                         "");
  createStopMotionAction(MI_StopMotionPickFocusCheck,
                         tr("Pick Focus Check Location"), "");
#endif
  createStopMotionAction(MI_StopMotionLowerSubsampling,
                         tr("Lower Stop Motion Level Subsampling"), "");
  createStopMotionAction(MI_StopMotionRaiseSubsampling,
                         tr("Raise Stop Motion Level Subsampling"), "");
  createStopMotionAction(MI_StopMotionJumpToCamera,
                         tr("Go to Stop Motion Insert Frame"), "");
  createStopMotionAction(MI_StopMotionRemoveFrame,
                         tr("Remove frame before Stop Motion Camera"), "");
  createStopMotionAction(MI_StopMotionNextFrame,
                         tr("Next Frame including Stop Motion Camera"), "");
  createStopMotionAction(MI_StopMotionToggleUseLiveViewImages,
                         tr("Show original live view images."), "");
}

//-----------------------------------------------------------------------------

void MainWindow::onInkCheckTriggered(bool on) {
  if (!on) return;
  QAction *ink1CheckAction =
      CommandManager::instance()->getAction(MI_Ink1Check);
  if (ink1CheckAction) ink1CheckAction->setChecked(false);
}

//-----------------------------------------------------------------------------

void MainWindow::onInk1CheckTriggered(bool on) {
  if (!on) return;
  QAction *inkCheckAction = CommandManager::instance()->getAction(MI_ICheck);
  if (inkCheckAction) inkCheckAction->setChecked(false);
}

//-----------------------------------------------------------------------------
/*-- Animate tool + mode switching shortcuts --*/
void MainWindow::toggleEditNextMode() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_Edit)
    CommandManager::instance()
        ->getAction("A_ToolOption_EditToolActiveAxis")
        ->trigger();
  else
    CommandManager::instance()->getAction(T_Edit)->trigger();
}

void MainWindow::toggleEditPosition() {
  CommandManager::instance()->getAction(T_Edit)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_EditToolActiveAxis:Position")
      ->trigger();
}

void MainWindow::toggleEditRotation() {
  CommandManager::instance()->getAction(T_Edit)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_EditToolActiveAxis:Rotation")
      ->trigger();
}

void MainWindow::toggleEditNextScale() {
  CommandManager::instance()->getAction(T_Edit)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_EditToolActiveAxis:Scale")
      ->trigger();
}

void MainWindow::toggleEditNextShear() {
  CommandManager::instance()->getAction(T_Edit)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_EditToolActiveAxis:Shear")
      ->trigger();
}

void MainWindow::toggleEditNextCenter() {
  CommandManager::instance()->getAction(T_Edit)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_EditToolActiveAxis:Center")
      ->trigger();
}

void MainWindow::toggleEditNextAll() {
  CommandManager::instance()->getAction(T_Edit)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_EditToolActiveAxis:All")
      ->trigger();
}

//---------------------------------------------------------------------------------------
/*-- Selection tool + type switching shortcuts --*/
void MainWindow::toggleSelectionNextType() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_Selection)
    CommandManager::instance()->getAction("A_ToolOption_Type")->trigger();
  else
    CommandManager::instance()->getAction(T_Selection)->trigger();
}

void MainWindow::toggleSelectionRectangular() {
  CommandManager::instance()->getAction(T_Selection)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Rectangular")
      ->trigger();
}

void MainWindow::toggleSelectionFreehand() {
  CommandManager::instance()->getAction(T_Selection)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Freehand")
      ->trigger();
}

void MainWindow::toggleSelectionPolyline() {
  CommandManager::instance()->getAction(T_Selection)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Polyline")
      ->trigger();
}

//---------------------------------------------------------------------------------------
/*-- Geometric tool + shape switching shortcuts --*/
void MainWindow::toggleGeometricNextShape() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_Geometric)
    CommandManager::instance()
        ->getAction("A_ToolOption_GeometricShape")
        ->trigger();
  else
    CommandManager::instance()->getAction(T_Geometric)->trigger();
}

void MainWindow::toggleGeometricRectangle() {
  CommandManager::instance()->getAction(T_Geometric)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_GeometricShape:Rectangle")
      ->trigger();
}

void MainWindow::toggleGeometricCircle() {
  CommandManager::instance()->getAction(T_Geometric)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_GeometricShape:Circle")
      ->trigger();
}

void MainWindow::toggleGeometricEllipse() {
  CommandManager::instance()->getAction(T_Geometric)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_GeometricShape:Ellipse")
      ->trigger();
}

void MainWindow::toggleGeometricLine() {
  CommandManager::instance()->getAction(T_Geometric)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_GeometricShape:Line")
      ->trigger();
}

void MainWindow::toggleGeometricPolyline() {
  CommandManager::instance()->getAction(T_Geometric)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_GeometricShape:Polyline")
      ->trigger();
}

void MainWindow::toggleGeometricArc() {
  CommandManager::instance()->getAction(T_Geometric)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_GeometricShape:Arc")
      ->trigger();
}

void MainWindow::toggleGeometricMultiArc() {
  CommandManager::instance()->getAction(T_Geometric)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_GeometricShape:MultiArc")
      ->trigger();
}

void MainWindow::toggleGeometricPolygon() {
  CommandManager::instance()->getAction(T_Geometric)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_GeometricShape:Polygon")
      ->trigger();
}
//---------------------------------------------------------------------------------------
/*-- Type tool + mode switching shortcuts --*/
void MainWindow::toggleTypeNextStyle() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_Type)
    CommandManager::instance()->getAction("A_ToolOption_TypeStyle")->trigger();
  else
    CommandManager::instance()->getAction(T_Type)->trigger();
}

void MainWindow::toggleTypeOblique() {
  CommandManager::instance()->getAction(T_Type)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_TypeStyle:Oblique")
      ->trigger();
}

void MainWindow::toggleTypeRegular() {
  CommandManager::instance()->getAction(T_Type)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_TypeStyle:Regular")
      ->trigger();
}

void MainWindow::toggleTypeBoldOblique() {
  CommandManager::instance()->getAction(T_Type)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_TypeStyle:Bold Oblique")
      ->trigger();
}

void MainWindow::toggleTypeBold() {
  CommandManager::instance()->getAction(T_Type)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_TypeStyle:Bold")
      ->trigger();
}

//---------------------------------------------------------------------------------------
/*-- Fill tool + type/mode switching shortcuts --*/
void MainWindow::toggleFillNextType() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_Fill)
    CommandManager::instance()->getAction("A_ToolOption_Type")->trigger();
  else
    CommandManager::instance()->getAction(T_Fill)->trigger();
}

void MainWindow::toggleFillNormal() {
  CommandManager::instance()->getAction(T_Fill)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
}

void MainWindow::toggleFillRectangular() {
  CommandManager::instance()->getAction(T_Fill)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Rectangular")
      ->trigger();
}

void MainWindow::toggleFillFreehand() {
  CommandManager::instance()->getAction(T_Fill)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Freehand")
      ->trigger();
}

void MainWindow::toggleFillPolyline() {
  CommandManager::instance()->getAction(T_Fill)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Polyline")
      ->trigger();
}

void MainWindow::toggleFillNextMode() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_Fill)
    CommandManager::instance()->getAction("A_ToolOption_Mode")->trigger();
  else
    CommandManager::instance()->getAction(T_Fill)->trigger();
}

void MainWindow::toggleFillAreas() {
  CommandManager::instance()->getAction(T_Fill)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Mode:Areas")->trigger();
}

void MainWindow::toggleFillLines() {
  CommandManager::instance()->getAction(T_Fill)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Mode:Lines")->trigger();
}

void MainWindow::toggleFillLinesAndAreas() {
  CommandManager::instance()->getAction(T_Fill)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Mode:Lines & Areas")
      ->trigger();
}

//---------------------------------------------------------------------------------------
/*-- Eraser tool + type switching shortcuts --*/
void MainWindow::toggleEraserNextType() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_Eraser)
    CommandManager::instance()->getAction("A_ToolOption_Type")->trigger();
  else
    CommandManager::instance()->getAction(T_Eraser)->trigger();
}

void MainWindow::toggleEraserNormal() {
  CommandManager::instance()->getAction(T_Eraser)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
}

void MainWindow::toggleEraserRectangular() {
  CommandManager::instance()->getAction(T_Eraser)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Rectangular")
      ->trigger();
}

void MainWindow::toggleEraserFreehand() {
  CommandManager::instance()->getAction(T_Eraser)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Freehand")
      ->trigger();
}

void MainWindow::toggleEraserPolyline() {
  CommandManager::instance()->getAction(T_Eraser)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Polyline")
      ->trigger();
}

void MainWindow::toggleEraserSegment() {
  CommandManager::instance()->getAction(T_Eraser)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Segment")->trigger();
}
//---------------------------------------------------------------------------------------
/*-- Tape tool + type/mode switching shortcuts --*/
void MainWindow::toggleTapeNextType() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_Tape)
    CommandManager::instance()->getAction("A_ToolOption_Type")->trigger();
  else
    CommandManager::instance()->getAction(T_Tape)->trigger();
}

void MainWindow::toggleTapeNormal() {
  CommandManager::instance()->getAction(T_Tape)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
}

void MainWindow::toggleTapeRectangular() {
  CommandManager::instance()->getAction(T_Tape)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Rectangular")
      ->trigger();
}

void MainWindow::toggleTapeNextMode() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_Tape)
    CommandManager::instance()->getAction("A_ToolOption_Mode")->trigger();
  else
    CommandManager::instance()->getAction(T_Tape)->trigger();
}

void MainWindow::toggleTapeEndpointToEndpoint() {
  CommandManager::instance()->getAction(T_Tape)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Mode:Endpoint to Endpoint")
      ->trigger();
}

void MainWindow::toggleTapeEndpointToLine() {
  CommandManager::instance()->getAction(T_Tape)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Mode:Endpoint to Line")
      ->trigger();
}

void MainWindow::toggleTapeLineToLine() {
  CommandManager::instance()->getAction(T_Tape)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Mode:Line to Line")
      ->trigger();
}

//---------------------------------------------------------------------------------------
/*-- Style Picker tool + mode switching shortcuts --*/
void MainWindow::togglePickStyleNextMode() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_StylePicker)
    CommandManager::instance()->getAction("A_ToolOption_Mode")->trigger();
  else
    CommandManager::instance()->getAction(T_StylePicker)->trigger();
}

void MainWindow::togglePickStyleAreas() {
  CommandManager::instance()->getAction(T_StylePicker)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Mode:Areas")->trigger();
}

void MainWindow::togglePickStyleLines() {
  CommandManager::instance()->getAction(T_StylePicker)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Mode:Lines")->trigger();
}

void MainWindow::togglePickStyleLinesAndAreas() {
  CommandManager::instance()->getAction(T_StylePicker)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Mode:Lines & Areas")
      ->trigger();
}
//-----------------------------------------------------------------------------
/*-- RGB Picker tool + type switching shortcuts --*/
void MainWindow::toggleRGBPickerNextType() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_RGBPicker)
    CommandManager::instance()->getAction("A_ToolOption_Type")->trigger();
  else
    CommandManager::instance()->getAction(T_RGBPicker)->trigger();
}

void MainWindow::toggleRGBPickerNormal() {
  CommandManager::instance()->getAction(T_RGBPicker)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
}

void MainWindow::toggleRGBPickerRectangular() {
  CommandManager::instance()->getAction(T_RGBPicker)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Rectangular")
      ->trigger();
}

void MainWindow::toggleRGBPickerFreehand() {
  CommandManager::instance()->getAction(T_RGBPicker)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Freehand")
      ->trigger();
}

void MainWindow::toggleRGBPickerPolyline() {
  CommandManager::instance()->getAction(T_RGBPicker)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Type:Normal")->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_Type:Polyline")
      ->trigger();
}
//-----------------------------------------------------------------------------
/*-- Skeleton tool + type switching shortcuts --*/
void MainWindow::ToggleSkeletonNextMode() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_Skeleton)
    CommandManager::instance()
        ->getAction("A_ToolOption_SkeletonMode")
        ->trigger();
  else
    CommandManager::instance()->getAction(T_Skeleton)->trigger();
}

void MainWindow::ToggleSkeletonBuildSkeleton() {
  CommandManager::instance()->getAction(T_Skeleton)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_SkeletonMode:Build Skeleton")
      ->trigger();
}

void MainWindow::ToggleSkeletonAnimate() {
  CommandManager::instance()->getAction(T_Skeleton)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_SkeletonMode:Animate")
      ->trigger();
}

void MainWindow::ToggleSkeletonInverseKinematics() {
  CommandManager::instance()->getAction(T_Skeleton)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_SkeletonMode:Inverse Kinematics")
      ->trigger();
}

//-----------------------------------------------------------------------------
/*-- Plastic tool + mode switching shortcuts --*/
void MainWindow::TogglePlasticNextMode() {
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_Plastic)
    CommandManager::instance()
        ->getAction("A_ToolOption_SkeletonMode")
        ->trigger();
  else
    CommandManager::instance()->getAction(T_Plastic)->trigger();
}

void MainWindow::TogglePlasticEditMesh() {
  CommandManager::instance()->getAction(T_Plastic)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_SkeletonMode:Edit Mesh")
      ->trigger();
}

void MainWindow::TogglePlasticPaintRigid() {
  CommandManager::instance()->getAction(T_Plastic)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_SkeletonMode:Paint Rigid")
      ->trigger();
}

void MainWindow::TogglePlasticBuildSkeleton() {
  CommandManager::instance()->getAction(T_Plastic)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_SkeletonMode:Build Skeleton")
      ->trigger();
}

void MainWindow::TogglePlasticAnimate() {
  CommandManager::instance()->getAction(T_Plastic)->trigger();
  CommandManager::instance()
      ->getAction("A_ToolOption_SkeletonMode:Animate")
      ->trigger();
}

//-----------------------------------------------------------------------------
/*-- Brush tool + mode switching shortcuts --*/
void MainWindow::ToggleBrushAutoFillOff() {
  CommandManager::instance()->getAction(T_Brush)->trigger();
  QAction *ac = CommandManager::instance()->getAction("A_ToolOption_AutoClose");
  if (ac->isChecked()) {
    ac->trigger();
  }
}

void MainWindow::ToggleBrushAutoFillOn() {
  CommandManager::instance()->getAction(T_Brush)->trigger();
  QAction *ac = CommandManager::instance()->getAction("A_ToolOption_Autofill");
  if (!ac->isChecked()) {
    ac->trigger();
  }
}

//-----------------------------------------------------------------------------

void MainWindow::onNewVectorLevelButtonPressed() {
  int defaultLevelType = Preferences::instance()->getDefLevelType();
  Preferences::instance()->setValue(DefLevelType, PLI_XSHLEVEL);
  CommandManager::instance()->execute("MI_NewLevel");
  Preferences::instance()->setValue(DefLevelType, defaultLevelType);
}

//-----------------------------------------------------------------------------

void MainWindow::onNewToonzRasterLevelButtonPressed() {
  int defaultLevelType = Preferences::instance()->getDefLevelType();
  Preferences::instance()->setValue(DefLevelType, TZP_XSHLEVEL);
  CommandManager::instance()->execute("MI_NewLevel");
  Preferences::instance()->setValue(DefLevelType, defaultLevelType);
}

//-----------------------------------------------------------------------------

void MainWindow::onNewRasterLevelButtonPressed() {
  int defaultLevelType = Preferences::instance()->getDefLevelType();
  Preferences::instance()->setValue(DefLevelType, OVL_XSHLEVEL);
  CommandManager::instance()->execute("MI_NewLevel");
  Preferences::instance()->setValue(DefLevelType, defaultLevelType);
}

//-----------------------------------------------------------------------------
// delete unused files / folders in the cache
void MainWindow::clearCacheFolder() {
  // currently cache folder is used for following purposes
  // 1. $CACHE/[ProcessID] : for disk swap of image cache.
  //    To be deleted on exit. Remains on crash.
  // 2. $CACHE/ffmpeg : ffmpeg cache.
  //    To be cleared on the end of rendering, on exist and on launch.
  // 3. $CACHE/temp : untitled scene data.
  //    To be deleted on switching or exiting scenes. Remains on crash.

  // So, this function will delete all files / folders in $CACHE
  // except the following items:
  // 1. $CACHE/[Current ProcessID]
  // 2. $CACHE/temp/[Current scene folder] if the current scene is untitled

  TFilePath cacheRoot                = ToonzFolder::getCacheRootFolder();
  if (cacheRoot.isEmpty()) cacheRoot = TEnv::getStuffDir() + "cache";

  TFilePathSet filesToBeRemoved;

  TSystem::readDirectory(filesToBeRemoved, cacheRoot, false);

  // keep the imagecache folder
  filesToBeRemoved.remove(cacheRoot + std::to_string(TSystem::getProcessId()));
  // keep the untitled scene data folder
  if (TApp::instance()->getCurrentScene()->getScene()->isUntitled()) {
    filesToBeRemoved.remove(cacheRoot + "temp");
    TFilePathSet untitledData =
        TSystem::readDirectory(cacheRoot + "temp", false);
    untitledData.remove(TApp::instance()
                            ->getCurrentScene()
                            ->getScene()
                            ->getScenePath()
                            .getParentDir());
    filesToBeRemoved.insert(filesToBeRemoved.end(), untitledData.begin(),
                            untitledData.end());
  }

  // return if there is no files/folders to be deleted
  if (filesToBeRemoved.size() == 0) {
    QMessageBox::information(
        this, tr("Clear Cache Folder"),
        tr("There are no unused items in the cache folder."));
    return;
  }

  QString message(tr("Deleting the following items:\n"));
  int count = 0;
  for (const auto &fileToBeRemoved : filesToBeRemoved) {
    QString dirPrefix =
        (TFileStatus(fileToBeRemoved).isDirectory()) ? tr("<DIR> ") : "";
    message +=
        "   " + dirPrefix + (fileToBeRemoved - cacheRoot).getQString() + "\n";
    count++;
    if (count == 5) break;
  }
  if (filesToBeRemoved.size() > 5)
    message +=
        tr("   ... and %1 more items\n").arg(filesToBeRemoved.size() - 5);

  message +=
      tr("\nAre you sure?\n\nN.B. Make sure you are not running another "
         "process of Tahoma2D,\nor you may delete necessary files for it.");

  QMessageBox::StandardButton ret = QMessageBox::question(
      this, tr("Clear Cache Folder"), message,
      QMessageBox::StandardButtons(QMessageBox::Ok | QMessageBox::Cancel));

  if (ret != QMessageBox::Ok) return;

  for (const auto &fileToBeRemoved : filesToBeRemoved) {
    try {
      if (TFileStatus(fileToBeRemoved).isDirectory())
        TSystem::rmDirTree(fileToBeRemoved);
      else
        TSystem::deleteFile(fileToBeRemoved);
    } catch (TException &e) {
      QMessageBox::warning(
          this, tr("Clear Cache Folder"),
          tr("Can't delete %1 : ").arg(fileToBeRemoved.getQString()) +
              QString::fromStdWString(e.getMessage()));
    } catch (...) {
      QMessageBox::warning(
          this, tr("Clear Cache Folder"),
          tr("Can't delete %1 : ").arg(fileToBeRemoved.getQString()));
    }
  }
}

//-----------------------------------------------------------------------------

void MainWindow::toggleStatusBar(bool on) {
  if (!on) {
    m_statusBar->hide();
    ShowStatusBarAction = 0;
  } else {
    m_statusBar->show();
    ShowStatusBarAction = 1;
  }
}

//-----------------------------------------------------------------------------

void MainWindow::toggleTransparency(bool on) {
  if (!on) {
    this->setProperty("windowOpacity", 1.0);
  } else {
    this->setProperty("windowOpacity", (double)TransparencySliderValue / 100);
    m_transparencyTogglerWindow->show();
  }
}

//-----------------------------------------------------------------------------

void MainWindow::makeTransparencyDialog() {
  m_transparencyTogglerWindow = new QDialog();
  m_transparencyTogglerWindow->setWindowFlags(Qt::WindowStaysOnTopHint |
                                              Qt::WindowCloseButtonHint);

  m_transparencyTogglerWindow->setFixedHeight(100);
  m_transparencyTogglerWindow->setFixedWidth(250);
  m_transparencyTogglerWindow->setWindowTitle(tr("Tahoma2D Transparency"));
  QPushButton *toggleButton = new QPushButton(this);
  toggleButton->setText(tr("Close to turn off Transparency."));
  connect(toggleButton, &QPushButton::clicked,
          [=]() { m_transparencyTogglerWindow->accept(); });
  m_transparencySlider = new QSlider(this);
  m_transparencySlider->setRange(-100, -30);
  m_transparencySlider->setValue(TransparencySliderValue * -1);
  m_transparencySlider->setOrientation(Qt::Horizontal);
  connect(m_transparencySlider, &QSlider::valueChanged, [=](int value) {
    TransparencySliderValue = value * -1;
    toggleTransparency(true);
  });

  QVBoxLayout *togglerLayout       = new QVBoxLayout(this);
  QHBoxLayout *togglerSliderLayout = new QHBoxLayout(this);
  togglerSliderLayout->addWidget(new QLabel(tr("Amount: "), this));
  togglerSliderLayout->addWidget(m_transparencySlider);
  togglerLayout->addLayout(togglerSliderLayout);
  togglerLayout->addWidget(toggleButton);

  m_transparencyTogglerWindow->setLayout(togglerLayout);
}

//-----------------------------------------------------------------------------

class ToggleStatusBar final : public MenuItemHandler {
public:
  ToggleStatusBar() : MenuItemHandler("MI_ShowStatusBar") {}
  void execute() override {}
} toggleStatusBar;

//-----------------------------------------------------------------------------

class ToggleTransparency final : public MenuItemHandler {
public:
  ToggleTransparency() : MenuItemHandler("MI_ToggleTransparent") {}
  void execute() override {}
} toggleTransparency;

//-----------------------------------------------------------------------------

class ReloadStyle final : public MenuItemHandler {
public:
  ReloadStyle() : MenuItemHandler("MI_ReloadStyle") {}
  void execute() override {
    QString currentStyle = Preferences::instance()->getCurrentStyleSheetPath();
    QFile file(currentStyle);
    file.open(QFile::ReadOnly);
    QString styleSheet = QString(file.readAll());
    qApp->setStyleSheet(styleSheet);
    file.close();
  }
} reloadStyle;

void MainWindow::onQuit() { close(); }

//=============================================================================
// RecentFiles
//=============================================================================

RecentFiles::RecentFiles()
    : m_recentScenes()
    , m_recentSceneProjects()
    , m_recentLevels()
    , m_recentProjects() {}

//-----------------------------------------------------------------------------

RecentFiles *RecentFiles::instance() {
  static RecentFiles _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

RecentFiles::~RecentFiles() {}

//-----------------------------------------------------------------------------

void RecentFiles::addFilePath(QString path, FileType fileType,
                              QString projectName) {
  QList<QString> files =
      (fileType == Scene) ? m_recentScenes : (fileType == Level)
                                                 ? m_recentLevels
                                                 : (fileType == Project)
                                                       ? m_recentProjects
                                                       : m_recentFlipbookImages;
  int i;
  for (i = 0; i < files.size(); i++)
    if (files.at(i) == path) {
      files.removeAt(i);
      if (fileType == Scene) m_recentSceneProjects.removeAt(i);
    }
  files.insert(0, path);
  if (fileType == Scene) m_recentSceneProjects.insert(0, projectName);
  int maxSize = 10;
  if (files.size() > maxSize) {
    files.removeAt(maxSize);
    if (fileType == Scene) m_recentSceneProjects.removeAt(maxSize);
  }

  if (fileType == Scene)
    m_recentScenes = files;
  else if (fileType == Level)
    m_recentLevels = files;
  else if (fileType == Project)
    m_recentProjects = files;
  else
    m_recentFlipbookImages = files;

  refreshRecentFilesMenu(fileType);
  saveRecentFiles();
}

//-----------------------------------------------------------------------------

void RecentFiles::moveFilePath(int fromIndex, int toIndex, FileType fileType) {
  if (fileType == Scene) {
    m_recentScenes.move(fromIndex, toIndex);
    m_recentSceneProjects.move(fromIndex, toIndex);
  } else if (fileType == Level)
    m_recentLevels.move(fromIndex, toIndex);
  else if (fileType == Project)
    m_recentProjects.move(fromIndex, toIndex);
  else
    m_recentFlipbookImages.move(fromIndex, toIndex);
  saveRecentFiles();
}

//-----------------------------------------------------------------------------

void RecentFiles::removeFilePath(int index, FileType fileType) {
  if (fileType == Scene) {
    m_recentScenes.removeAt(index);
    m_recentSceneProjects.removeAt(index);
  } else if (fileType == Level)
    m_recentLevels.removeAt(index);
  else if (fileType == Project)
    m_recentProjects.removeAt(index);
  saveRecentFiles();
}

//-----------------------------------------------------------------------------

QString RecentFiles::getFilePath(int index, FileType fileType) const {
  return (fileType == Scene)
             ? m_recentScenes[index]
             : (fileType == Level)
                   ? m_recentLevels[index]
                   : (fileType == Project) ? m_recentProjects[index]
                                           : m_recentFlipbookImages[index];
}

//-----------------------------------------------------------------------------

QString RecentFiles::getFileProject(int index) const {
  if (index >= m_recentScenes.size() || index >= m_recentSceneProjects.size())
    return "-";
  return m_recentSceneProjects[index];
}

QString RecentFiles::getFileProject(QString fileName) const {
  for (int index = 0; index < m_recentScenes.size(); index++)
    if (m_recentScenes[index] == fileName) return m_recentSceneProjects[index];

  return "-";
}

//-----------------------------------------------------------------------------

void RecentFiles::clearRecentFilesList(FileType fileType) {
  if (fileType == Scene) {
    m_recentScenes.clear();
    m_recentSceneProjects.clear();
  } else if (fileType == Level)
    m_recentLevels.clear();
  else if (fileType == Project)
    m_recentProjects.clear();
  else
    m_recentFlipbookImages.clear();

  refreshRecentFilesMenu(fileType);
  saveRecentFiles();
}

//-----------------------------------------------------------------------------

void RecentFiles::clearAllRecentFilesList(bool saveNow) {
  m_recentScenes.clear();
  m_recentSceneProjects.clear();
  m_recentLevels.clear();
  m_recentFlipbookImages.clear();
  if (saveNow) saveRecentFiles();
}

//-----------------------------------------------------------------------------

void RecentFiles::loadRecentFiles() {
  TFilePath fp = ToonzFolder::getMyModuleDir() + TFilePath("RecentFiles.ini");
  QSettings settings(toQString(fp), QSettings::IniFormat);
  int i;

  QList<QVariant> scenes = settings.value(QString("Scenes")).toList();
  if (!scenes.isEmpty()) {
    for (i = 0; i < scenes.size(); i++)
      m_recentScenes.append(scenes.at(i).toString());
  } else {
    QString scene = settings.value(QString("Scenes")).toString();
    if (!scene.isEmpty()) m_recentScenes.append(scene);
  }

  // Load scene's projects info. This is for display purposes only. For
  // backwards compatibility it is stored and maintained separately.
  QList<QVariant> sceneProjects =
      settings.value(QString("SceneProjects")).toList();
  if (!sceneProjects.isEmpty()) {
    for (i = 0; i < sceneProjects.size(); i++)
      m_recentSceneProjects.append(sceneProjects.at(i).toString());
  } else {
    QString sceneProject = settings.value(QString("SceneProjects")).toString();
    if (!sceneProject.isEmpty()) m_recentSceneProjects.append(sceneProject);
  }
  // Should be 1-to-1. If we're short, append projects list with "-".
  while (m_recentSceneProjects.size() < m_recentScenes.size())
    m_recentSceneProjects.append("-");

  QList<QVariant> levels = settings.value(QString("Levels")).toList();
  if (!levels.isEmpty()) {
    for (i = 0; i < levels.size(); i++) {
      QString path = levels.at(i).toString();
      m_recentLevels.append(path);
    }
  } else {
    QString level = settings.value(QString("Levels")).toString();
    if (!level.isEmpty()) m_recentLevels.append(level);
  }

  QList<QVariant> projects = settings.value(QString("Projects")).toList();
  if (!projects.isEmpty()) {
    for (i = 0; i < projects.size(); i++)
      m_recentProjects.append(projects.at(i).toString());
  } else {
    QString project = settings.value(QString("Projects")).toString();
    if (!project.isEmpty()) m_recentProjects.append(project);
  }

  QList<QVariant> flipImages =
      settings.value(QString("FlipbookImages")).toList();
  if (!flipImages.isEmpty()) {
    for (i = 0; i < flipImages.size(); i++)
      m_recentFlipbookImages.append(flipImages.at(i).toString());
  } else {
    QString flipImage = settings.value(QString("FlipbookImages")).toString();
    if (!flipImage.isEmpty()) m_recentFlipbookImages.append(flipImage);
  }

  refreshRecentFilesMenu(Scene);
  refreshRecentFilesMenu(Level);
  refreshRecentFilesMenu(Flip);
  refreshRecentFilesMenu(Project);
}

//-----------------------------------------------------------------------------

void RecentFiles::saveRecentFiles() {
  TFilePath fp = ToonzFolder::getMyModuleDir() + TFilePath("RecentFiles.ini");
  QSettings settings(toQString(fp), QSettings::IniFormat);
  settings.setValue(QString("Scenes"), QVariant(m_recentScenes));
  settings.setValue(QString("SceneProjects"), QVariant(m_recentSceneProjects));
  settings.setValue(QString("Levels"), QVariant(m_recentLevels));
  settings.setValue(QString("Projects"), QVariant(m_recentProjects));
  settings.setValue(QString("FlipbookImages"),
                    QVariant(m_recentFlipbookImages));
}

//-----------------------------------------------------------------------------

void RecentFiles::updateStuffPath(QString oldPath, QString newPath) {
  m_recentScenes.replaceInStrings(oldPath, newPath);
  m_recentLevels.replaceInStrings(oldPath, newPath);
  m_recentFlipbookImages.replaceInStrings(oldPath, newPath);
}

//-----------------------------------------------------------------------------

QList<QString> RecentFiles::getFilesNameList(FileType fileType) {
  QList<QString> files =
      (fileType == Scene) ? m_recentScenes : (fileType == Level)
                                                 ? m_recentLevels
                                                 : (fileType == Project)
                                                       ? m_recentProjects
                                                       : m_recentFlipbookImages;
  QList<QString> names;
  int i;
  for (i = 0; i < files.size(); i++) {
    TFilePath path(files.at(i).toStdWString());
    QString str, number;
    if (fileType != Project)
      names.append(number.number(i + 1) + QString(". ") +
                   str.fromStdWString(path.getWideString()));
    else
      names.append(str.fromStdWString(path.getWideString()));
  }
  return names;
}

//-----------------------------------------------------------------------------

void RecentFiles::refreshRecentFilesMenu(FileType fileType) {
  CommandId id = (fileType == Scene) ? MI_OpenRecentScene
                                     : (fileType == Level) ? MI_OpenRecentLevel
                                                           : MI_LoadRecentImage;
  QAction *act = CommandManager::instance()->getAction(id);
  if (!act) return;
  DVMenuAction *menu = dynamic_cast<DVMenuAction *>(act->menu());
  if (!menu) return;
  QList<QString> names = getFilesNameList(fileType);
  if (names.isEmpty())
    menu->setEnabled(false);
  else {
    CommandId clearActionId =
        (fileType == Scene) ? MI_ClearRecentScene : (fileType == Level)
                                                        ? MI_ClearRecentLevel
                                                        : MI_ClearRecentImage;
    menu->setActions(names);
    menu->addSeparator();
    QAction *clearAction = CommandManager::instance()->getAction(clearActionId);
    assert(clearAction);
    menu->addAction(clearAction);
    if (!menu->isEnabled()) menu->setEnabled(true);
  }
}
