#include "backend.h"
#include "curveplotprovider.h"
#include "logger.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSurfaceFormat>

int main(int argc, char *argv[])
{
    QSurfaceFormat format;
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QGuiApplication app(argc, argv);
    Logger::initialize(QCoreApplication::applicationDirPath());
    Logger::installQtMessageHandler();//qt信息处理器，将信息重定向到日志
    Logger::info(QStringLiteral("qthmi 启动"));

    qmlRegisterType<CurvePlotProvider>("Industrial", 1, 0, "CurvePlotProvider");

    QQmlApplicationEngine engine;
    Backend *backend = new Backend(QCoreApplication::applicationDirPath(), &engine);
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), backend);
    QObject::connect(&app, &QCoreApplication::aboutToQuit,
                     backend, &Backend::shutdown,
                     Qt::DirectConnection);

    const QUrl url(QStringLiteral("qrc:/qt/qml/Qthmi/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
