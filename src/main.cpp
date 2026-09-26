
#include "mainwindow.h"
#include "theme.h"
#include <QSurfaceFormat>
#include <qquickwindow.h>

#include "quick/workshopmodel.h"

int main(int argc, char* argv[])
{
	QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
	
	QSurfaceFormat format;
	format.setDepthBufferSize(24);
	format.setStencilBufferSize(8);
	format.setVersion(3, 3);
	format.setProfile(QSurfaceFormat::CompatibilityProfile);
	QSurfaceFormat::setDefaultFormat(format);

	QApplication app(argc, argv);
	Theme::apply(app);

	qmlRegisterType<WorkshopModel>("MultimediaRetrieval.Types", 1, 0, "WorkshopModel");

	MainWindow window;
	window.setWindowTitle("Geometry Retrieval");
	window.resize(1000, 700);
	window.show();

	return app.exec();
}
