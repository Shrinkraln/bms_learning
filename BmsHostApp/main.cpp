#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSettings>
#include <QTranslator>
#include <QLocale>
#include <QThread>
#include "can/CanWorker.h"
#include "model/BmsDataModel.h"
#include "protocol/CsvLogger.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
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

    QObject::connect(&canWorker, &CanWorker::batchReady,
                     &bmsModel, &BmsDataModel::onBatchReady);
    QObject::connect(&canWorker, &CanWorker::connectionStatusChanged,
                     &bmsModel, &BmsDataModel::onConnectionChanged);

    engine.rootContext()->setContextProperty("bms", &bmsModel);

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

    return app.exec();
}
