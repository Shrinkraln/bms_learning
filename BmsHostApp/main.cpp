#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSettings>
#include <QTranslator>
#include <QLocale>

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

    // Model 将在后续任务中创建并注册到 context
    // BmsDataModel model;
    // engine.rootContext()->setContextProperty("bms", &model);

    const QUrl url("qrc:/qt/qml/com/bms/host/qml/main.qml");
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
