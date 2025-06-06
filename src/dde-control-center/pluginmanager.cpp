// SPDX-FileCopyrightText: 2024 - 2027 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pluginmanager.h"

#include "dccfactory.h"
#include "dccmanager.h"

#include <DIconTheme>

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QPluginLoader>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QRunnable>
#include <QSet>
#include <QSettings>
#include <QtConcurrent>
#include <QtConcurrentRun>

namespace dccV25 {
static Q_LOGGING_CATEGORY(dccLog, "dde.dcc.plugin");

const static QString TranslateReadDir = QStringLiteral(TRANSLATE_READ_DIR);

enum PluginStatus {
    // metaData 0xFF000000
    PluginBegin = 0x10000000,
    PluginEnd = 0x20000000,
    MetaDataEnd = 0x04000000,
    MetaDataErr = 0x08000000,
    // module 0x00FF0000
    ModuleLoad = 0x00010000,
    ModuleCreate = 0x00020000,
    ModuleEnd = 0x00400000,
    ModuleErr = 0x00800000,
    // data 0x0000FF00
    DataBegin = 0x00000100,
    DataLoad = 0x00000200,
    DataEnd = 0x00004000,
    DataErr = 0x00008000,
    // mainObj 0x000000FF
    MainObjLoad = 0x00000001,
    MainObjCreate = 0x00000002,
    MainObjAdd = 0x00000004,
    MainObjEnd = 0x00000040,
    MainObjErr = 0x00000080,

    PluginErrMask = MetaDataErr | ModuleErr | DataErr | MainObjErr,
    PluginEndMask = PluginEnd | MetaDataEnd | ModuleEnd | DataEnd | MainObjEnd,
};

struct PluginData
{
    QString name;
    QString path;
    DccObject *module;
    DccObject *mainObj;
    DccObject *soObj;
    QObject *data;
    QThread *thread;
    uint status;

    PluginData(const QString &_name, const QString &_path)
        : name(_name)
        , path(_path)
        , module(nullptr)
        , mainObj(nullptr)
        , soObj(nullptr)
        , data(nullptr)
        , thread(nullptr)
        , status(PluginBegin)
    {
    }
};

class LoadPluginTask : public QRunnable
{
public:
    explicit LoadPluginTask(PluginData *data, PluginManager *pManager)
        : QRunnable()
        , m_pManager(pManager)
        , m_data(data)
    {
    }

protected:
    void run() override;
    void doRun();

protected:
    PluginManager *m_pManager;
    PluginData *m_data;
};

void LoadPluginTask::run()
{
    m_data->thread = QThread::currentThread();
    doRun();
    m_data->thread = nullptr;
}

void LoadPluginTask::doRun()
{
    Q_EMIT m_pManager->updatePluginStatus(m_data, DataBegin, "load plugin begin");
    // {main.qml}
    const QString soPath = m_data->path + "/" + m_data->name + ".so";
    QElapsedTimer timer;
    timer.start();
    QObject *dataObj = nullptr;
    DccObject *soObj = nullptr;
    if (QFile::exists(soPath)) {
        if (m_pManager->isDeleting()) {
            return;
        }
        QPluginLoader loader(soPath);
        Q_EMIT m_pManager->updatePluginStatus(m_data, DataLoad, QString());
        loader.load();
        if (m_pManager->isDeleting()) {
            return;
        }
        if (!loader.isLoaded()) {
            Q_EMIT m_pManager->updatePluginStatus(m_data, DataErr, "Load the plugin failed." + loader.errorString());
        } else {
            const auto &meta = loader.metaData();
            do {
                const auto iid = meta["IID"].toString();
                if (iid.isEmpty() || iid != QString(qobject_interface_iid<DccFactory *>())) {
                    Q_EMIT m_pManager->updatePluginStatus(m_data, DataErr, "Error iid:" + iid);
                    break;
                }

                if (!loader.instance()) {
                    Q_EMIT m_pManager->updatePluginStatus(m_data, DataErr, "instance() failed." + loader.errorString());
                    break;
                }
                DccFactory *factory = qobject_cast<DccFactory *>(loader.instance());
                if (!factory) {
                    Q_EMIT m_pManager->updatePluginStatus(m_data, DataErr, "The plugin isn't a DccFactory." + soPath);
                    loader.unload();
                    break;
                }
                dataObj = factory->create();
                if (dataObj && dataObj->parent()) {
                    dataObj->setParent(nullptr);
                }
                soObj = factory->dccObject();
                if (soObj && soObj->parent()) {
                    soObj->setParent(nullptr);
                }
            } while (false);
        }
    } else {
        Q_EMIT m_pManager->updatePluginStatus(m_data, DataErr, "File does not exist:" + soPath);
    }
    if (dataObj) {
        m_data->data = dataObj;
    }
    if (soObj) {
        m_data->soObj = soObj;
    }
    if (m_data->data) {
        m_data->data->moveToThread(m_pManager->thread());
        m_data->data->setParent(m_pManager->parent());
    }
    if (m_data->soObj) {
        m_data->soObj->moveToThread(m_pManager->thread());
        m_data->soObj->setParent(m_pManager->parent());
    }
    Q_EMIT m_pManager->updatePluginStatus(m_data, DataEnd, ": load plugin finished. elasped time :" + QString::number(timer.elapsed()));
}

PluginManager::PluginManager(DccManager *parent)
    : QObject(parent)
    , m_manager(parent)
    , m_rootModule(nullptr)
    , m_threadPool(nullptr)
    , m_isDeleting(false)
{
    qRegisterMetaType<PluginData>("PluginData");
    connect(this, &PluginManager::pluginEndStatusChanged, this, &PluginManager::loadPlugin);
    connect(this, &PluginManager::updatePluginStatus, this, &PluginManager::onUpdatePluginStatus);
    connect(m_manager, &DccManager::hideModuleChanged, this, &PluginManager::onHideModuleChanged);
}

PluginManager::~PluginManager()
{
    for (auto &&data : m_plugins) {
        if (data->data && !data->thread) {
            qCDebug(dccLog()) << "delete so" << data->name;
            delete data->data;
            data->data = nullptr;
        }
    }
    cancelLoad();
    for (auto &&data : m_plugins) {
        if (data->data) {
            qCDebug(dccLog()) << "delete so" << data->name;
            delete data->data;
            data->data = nullptr;
        }
        delete data;
    }
    m_plugins.clear();
}

bool PluginManager::compareVersion(const QString &targetVersion, const QString &baseVersion)
{
    QStringList version1 = baseVersion.split(".");
    QStringList version2 = targetVersion.split(".");

    if (version1.size() != version2.size()) {
        return false;
    }

    for (int i = 0; i < version1.size(); ++i) {
        // 相等判断下一个子版本号
        if (version1[i] == version2[i])
            continue;

        // 转成整形比较
        if (version1[i].toInt() > version2[i].toInt()) {
            return false;
        } else {
            return true;
        }
    }

    return true;
}

QThreadPool *PluginManager::threadPool()
{
    if (!m_threadPool) {
        m_threadPool = new QThreadPool(this);
    }
    return m_threadPool;
}

void PluginManager::loadPlugin(PluginData *plugin)
{
    if (isDeleting()) {
        return;
    }
    if (plugin->status & PluginEnd) {
        if (loadFinished()) {
            Q_EMIT loadAllFinished();
            cancelLoad();
        }
    } else if (plugin->status & MainObjEnd) {
        addMainObject(plugin);
        Q_EMIT updatePluginStatus(plugin, PluginEnd, QString());
    } else if ((plugin->status & (DataEnd | MainObjLoad)) == DataEnd) {
        loadMain(plugin);
    } else if ((plugin->status & (ModuleEnd | DataBegin)) == ModuleEnd) {
        if (plugin->module) {
            disconnect(plugin->module, nullptr, this, nullptr);
            if (plugin->module->isVisibleToApp()) {
                threadPool()->start(new LoadPluginTask(plugin, this));
            } else {
                connect(plugin->module, &DccObject::visibleToAppChanged, this, &PluginManager::onVisibleToAppChanged);
                Q_EMIT updatePluginStatus(plugin, PluginEnd, QString());
            }
        } else {
            threadPool()->start(new LoadPluginTask(plugin, this));
        }
    } else if ((plugin->status & (MetaDataEnd | ModuleLoad)) == MetaDataEnd) {
        DccManager::installTranslator(plugin->name);
        loadModule(plugin);
    } else {
        loadMetaData(plugin);
    }
}

void PluginManager::onUpdatePluginStatus(PluginData *plugin, uint status, const QString &log)
{
    if (isDeleting()) {
        return;
    }
    uint oldStatus = plugin->status;
    plugin->status |= status;
    if (status & PluginErrMask) {
        qCWarning(dccLog()) << plugin->name << ": status" << QString::number(plugin->status, 16) << log;
    } else {
        qCDebug(dccLog()) << plugin->name << ": status" << QString::number(plugin->status, 16) << log;
    }
    if ((oldStatus != plugin->status) && (status & PluginEndMask)) {
        Q_EMIT pluginEndStatusChanged(plugin);
    }
}

void PluginManager::loadMetaData(PluginData *plugin)
{
    if (isDeleting()) {
        return;
    }
    if (m_manager->hideModule().contains(plugin->name)) {
        // 跳过隐藏的模块,需要动态加载回来
        Q_EMIT updatePluginStatus(plugin, PluginEnd | MetaDataEnd, QString());
        return;
    }
    // metadata
#if 0 // 文件夹有版本信息，不需要用metadata.json判断
    const QString metadataPath = plugin->path + "/metadata.json";
    QFile metadataFile(metadataPath);
    if (!metadataFile.open(QIODevice::ReadOnly)) {
        Q_EMIT updatePluginStatus(plugin, MetaDataErr | PluginEnd, "Couldn't open " + metadataFile.fileName());
        return;
    }
    QJsonParseError error;
    const QJsonObject metaData = QJsonDocument::fromJson(metadataFile.readAll()).object();
    metadataFile.close();
    if (error.error) {
        Q_EMIT updatePluginStatus(plugin, MetaDataErr | PluginEnd, "error parsing json data:" + error.errorString());
        return;
    }
    if (!compareVersion(metaData.value("Version").toString(), "1.0")) {
        Q_EMIT updatePluginStatus(plugin, MetaDataErr | PluginEnd, "plugin's version is too low:" + metaData.value("Version").toString());
        return;
    }
#endif
    Q_EMIT updatePluginStatus(plugin, MetaDataEnd, QString());
}

void PluginManager::loadModule(PluginData *plugin)
{
    if (isDeleting()) {
        return;
    }
    const QString qmlPath = plugin->path + "/" + plugin->name + ".qml";
    Q_EMIT updatePluginStatus(plugin, ModuleLoad, ": load module " + qmlPath);
    if (QFile::exists(qmlPath)) {
        QQmlComponent *component = new QQmlComponent(m_manager->engine(), m_manager->engine());
        component->setProperty("PluginData", QVariant::fromValue(plugin));
        component->loadUrl(qmlPath, QQmlComponent::Asynchronous);
        if (component->isLoading())
            connect(component, &QQmlComponent::statusChanged, this, &PluginManager::moduleLoading);
        else
            createModule(component);
    } else {
        Q_EMIT updatePluginStatus(plugin, ModuleErr | ModuleEnd, "module qml not exists");
    }
}

void PluginManager::loadMain(PluginData *plugin)
{
    if (isDeleting()) {
        return;
    }
    Q_EMIT updatePluginStatus(plugin, MainObjLoad, "load Main");
    QString qmlPath = plugin->path + "/" + plugin->name + "Main.qml";
    if (!QFile::exists(qmlPath)) {
        qmlPath = plugin->path + "/main.qml";
        if (!QFile::exists(qmlPath)) {
            qmlPath.clear();
        }
    }
    if (!qmlPath.isEmpty()) {
        QQmlComponent *component = new QQmlComponent(m_manager->engine(), m_manager->engine());
        component->setProperty("PluginData", QVariant::fromValue(plugin));
        component->loadUrl(qmlPath, QQmlComponent::Asynchronous);
        if (component->isLoading()) {
            connect(component, &QQmlComponent::statusChanged, this, &PluginManager::mainLoading);
        } else {
            createMain(component);
        }
    } else {
        Q_EMIT updatePluginStatus(plugin, MainObjErr | MainObjEnd, "Main.qml not exists");
    }
}

void PluginManager::createModule(QQmlComponent *component)
{
    if (isDeleting()) {
        return;
    }
    PluginData *plugin = component->property("PluginData").value<PluginData *>();
    Q_EMIT updatePluginStatus(plugin, ModuleCreate, "create module");
    if (component->isError()) {
        Q_EMIT updatePluginStatus(plugin, ModuleErr | ModuleEnd, " component create module object error:" + component->errorString());
    } else {
        QObject *object = component->create();
        if (!object) {
            Q_EMIT updatePluginStatus(plugin, ModuleErr | ModuleEnd, " component create module object is null:" + component->errorString());
            return;
        }
        plugin->module = qobject_cast<DccObject *>(object);
        Q_EMIT updatePluginStatus(plugin, ModuleEnd, "create module finished");
        m_manager->addObject(plugin->module);
    }
}

void PluginManager::createMain(QQmlComponent *component)
{
    if (isDeleting()) {
        return;
    }
    PluginData *plugin = component->property("PluginData").value<PluginData *>();
    Q_EMIT updatePluginStatus(plugin, MainObjCreate, "create main");
    if (component->isError()) {
        Q_EMIT updatePluginStatus(plugin, MainObjErr | MainObjEnd, " component create main object error:" + component->errorString());
    } else {
        QQmlContext *context = new QQmlContext(component->engine(), component);
        context->setContextProperties({ { "dccData", QVariant::fromValue(plugin->data) }, { "dccModule", QVariant::fromValue(plugin->module) } });
        QObject *object = component->create(context);
        // component->createWithInitialProperties({}, context);
        if (!object) {
            Q_EMIT updatePluginStatus(plugin, MainObjErr, " component create main object is null:" + component->errorString());
            return;
        }
        plugin->mainObj = qobject_cast<DccObject *>(object);
        Q_EMIT updatePluginStatus(plugin, MainObjEnd, ": create main finished");
    }
    Q_EMIT updatePluginStatus(plugin, MainObjEnd, QString());
}

void PluginManager::addMainObject(PluginData *plugin)
{
    if (isDeleting()) {
        return;
    }
    Q_EMIT updatePluginStatus(plugin, MainObjAdd, "add main object");
    if (!plugin->mainObj) {
        plugin->mainObj = plugin->soObj;
    }
    if (plugin->mainObj) {
        if (plugin->mainObj->name().isEmpty() || (plugin->module && plugin->mainObj->name() == plugin->module->name())) {
            // 插件根项name为空时，关联{name}.qml,不加树
            if (plugin->module) {
                QQmlComponent *page = plugin->mainObj->page();
                if (page) {
                    plugin->module->setPage(page);
                }
                connect(plugin->mainObj, &DccObject::pageChanged, plugin->module, &DccObject::setPage);
                connect(plugin->mainObj, &DccObject::displayNameChanged, plugin->module, &DccObject::setDisplayName);
                connect(plugin->mainObj, &DccObject::descriptionChanged, plugin->module, &DccObject::setDescription);
                connect(plugin->mainObj, &DccObject::iconChanged, plugin->module, &DccObject::setIcon);
                connect(plugin->mainObj, &DccObject::badgeChanged, plugin->module, &DccObject::setBadge);
                connect(plugin->mainObj, &DccObject::visibleChanged, plugin->module, &DccObject::setVisible);
                connect(plugin->mainObj, &DccObject::active, plugin->module, &DccObject::active);
                connect(plugin->mainObj, &DccObject::deactive, plugin->module, &DccObject::deactive);
            }
        } else {
        }
    } else {
        Q_EMIT updatePluginStatus(plugin, MainObjErr, "The plugin isn't main DccObject");
    }
    Q_EMIT updatePluginStatus(plugin, MainObjEnd | PluginEnd, "add main object finished");
    if (plugin->mainObj) {
        Q_EMIT addObject(plugin->mainObj);
    }
    if (plugin->soObj) {
        Q_EMIT addObject(plugin->soObj);
    }
}

void PluginManager::moduleLoading()
{
    QQmlComponent *component = qobject_cast<QQmlComponent *>(sender());
    if (component)
        createModule(component);
}

void PluginManager::mainLoading()
{
    QQmlComponent *component = qobject_cast<QQmlComponent *>(sender());
    if (component) {
        createMain(component);
    }
}

void PluginManager::onHideModuleChanged(const QSet<QString> &hideModule)
{
    for (auto &&plugin : m_plugins) {
        if ((plugin->status & PluginEnd) && (((plugin->status & (MetaDataEnd | MetaDataErr | ModuleLoad)) == MetaDataEnd) && (!hideModule.contains(plugin->name)))) {
            // 加载完成，没检查MetaData也没错误，不在hideModule中，则需要重新加载
            plugin->status &= ~PluginEnd;
            loadPlugin(plugin);
        }
    }
}

void PluginManager::onVisibleToAppChanged(bool visibleToApp)
{
    if (!visibleToApp) {
        return;
    }
    DccObject *obj = qobject_cast<DccObject *>(sender());
    if (!obj) {
        return;
    }
    for (auto &&plugin : m_plugins) {
        if (plugin->module == obj && (plugin->status & PluginEnd) && (!(plugin->status & (DataEnd | DataErr)))) {
            // 加载完成，没检查MetaData也没错误，不在hideModule中，则需要重新加载
            plugin->status &= ~PluginEnd;
            loadPlugin(plugin);
        }
    }
}

void PluginManager::loadModules(DccObject *root, bool async, const QStringList &dirs)
{
    Q_UNUSED(async)
    if (!root)
        return;
    m_rootModule = root;
    qCDebug(dccLog()) << "plugin dir:" << dirs;

    QFileInfoList pluginList;
    for (const auto &dir : dirs) {
        QDir plugindir(dir);
        if (plugindir.exists()) {
            pluginList += plugindir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        }
    }
    QStringList paths = Dtk::Gui::DIconTheme::dciThemeSearchPaths();
    for (auto &lib : pluginList) {
        const QString &filepath = lib.absoluteFilePath();
        auto filename = lib.fileName();
        PluginData *plugin = new PluginData(lib.baseName(), filepath);
        m_plugins.append(plugin);
        loadPlugin(plugin);
        paths.prepend(filepath);
    }
    Dtk::Gui::DIconTheme::setDciThemeSearchPaths(paths);
}

void PluginManager::cancelLoad()
{
    if (m_threadPool) {
        m_threadPool->clear();
        for (auto &&plugin : m_plugins) {
            if (plugin->thread) {
                qCWarning(dccLog()) << plugin->name << ": status" << QString::number(plugin->status, 16) << "thread exit timeout";
            }
        }
        qCWarning(dccLog()) << "delete threadPool";
        delete m_threadPool;
        qCWarning(dccLog()) << "delete threadPool finish";
        m_threadPool = nullptr;
    }
}

bool PluginManager::loadFinished() const
{
    uint status = PluginEnd;
    for (auto &&plugin : m_plugins) {
        status &= plugin->status;
    }

    return (status & PluginEnd) && (!m_plugins.isEmpty());
}

void PluginManager::beginDelete()
{
    m_isDeleting = true;
}
}; // namespace dccV25
Q_DECLARE_METATYPE(dccV25::PluginData *)
