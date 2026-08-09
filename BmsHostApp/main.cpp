#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSettings>
#include <QTranslator>
#include <QLocale>
#include <QThread>
#include "can/CanWorker.h"
#include "model/BmsDataModel.h"
#include "protocol/CsvLogger.h"
#include "model/CommunicationMonitor.h"

// QSettings::value/setValue 是普通 C++ 方法 (非 slot/Q_INVOKABLE)，QML 无法直接调用，
// 故用薄封装桥接，向 QML 暴露 settings 上下文属性。
class SettingsBridge : public QObject {
    Q_OBJECT
public:
    explicit SettingsBridge(QSettings *s, QObject *parent = nullptr)
        : QObject(parent), m_settings(s) {}

    Q_INVOKABLE QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const
    { return m_settings->value(key, defaultValue); }

    Q_INVOKABLE void setValue(const QString &key, const QVariant &value)
    { m_settings->setValue(key, value); }

private:
    QSettings *m_settings;
};

int main(int argc, char *argv[])
{
    // 注意: 必须用 QApplication 而非 QGuiApplication —
    // QtCharts 的 QChart 继承 QGraphicsWidget (QWidget)，ChartView 创建时
    // QWidgetTextControl 会调用 QApplication::style()，QGuiApplication 下为
    // nullptr → 段错误 (实测 Qt 6.11.1 MinGW)。
    QApplication app(argc, argv);
    app.setOrganizationName("BMS");
    app.setApplicationName("BmsHostApp");

    // 加载翻译
    QTranslator translator;
    if (translator.load(QLocale::system(), "BmsHostApp", "_",
                        ":/translations")) {
        app.installTranslator(&translator);
    }

    QQmlApplicationEngine engine;

    // 队列连接 (CanWorker 跨线程) 需要注册自定义 metatype — 必须在线程装配之前
    qRegisterMetaType<CanFrame>("CanFrame");
    qRegisterMetaType<QVector<CanFrame>>("QVector<CanFrame>");

    CanWorker canWorker;
    BmsDataModel bmsModel;
    CsvLogger csvLogger;

    bmsModel.setCanWorker(&canWorker);
    bmsModel.setCsvLogger(&csvLogger);

    CommunicationMonitor commMonitor;
    commMonitor.setCanWorker(&canWorker);
    commMonitor.setBmsModel(&bmsModel);

    QObject::connect(&canWorker, &CanWorker::batchReady,
                     &bmsModel, &BmsDataModel::onBatchReady);
    QObject::connect(&canWorker, &CanWorker::connectionStatusChanged,
                     &bmsModel, &BmsDataModel::onConnectionChanged);
    QObject::connect(&canWorker, &CanWorker::deviceConnectedChanged,
                     &bmsModel, &BmsDataModel::onDeviceConnected);
    QObject::connect(&canWorker, &CanWorker::errorOccurred,
                     &bmsModel, &BmsDataModel::onCanError);

    engine.rootContext()->setContextProperty("bms", &bmsModel);
    engine.rootContext()->setContextProperty("commMonitor", &commMonitor);

    // 窗口几何等持久化设置 (main.qml 通过 settings.value/setValue 读写)
    QSettings *settings = new QSettings(&app);
    engine.rootContext()->setContextProperty("settings", new SettingsBridge(settings, &app));

    // CanWorker 线程
    QThread *canThread = new QThread(&app);
    canWorker.moveToThread(canThread);
    QObject::connect(canThread, &QThread::started, &canWorker, [&]() {
        canWorker.start("peakcan", "usb0", 500000);
    });
    canThread->start();

    const QUrl url("qrc:/qt/qml/com/bms/host/qml/main.qml");
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);
    engine.load(url);

    int ret = app.exec();
    // 优雅停止 CAN 线程并等待其退出，避免 QThread 析构时线程仍在运行导致崩溃
    canThread->quit();
    canThread->wait();
    return ret;
}

#include "main.moc"
