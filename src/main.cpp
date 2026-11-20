
#include "io/shape_db.hpp"
#include "mainwindow.h"
#include "theme.h"
#include <qquickwindow.h>

int main(int argc, char* argv[])
{
	// constexpr auto theme = theme::Mode::Dark;
	constexpr auto theme = theme::Mode::Light;

	// auto shapeDb = ShapeDb::open(TEST_SHAPE_DB_ROOT);
	auto shapeDb = ShapeDb::open(SHAPE_DB_ROOT);
	// auto shapeDb = ShapeDb::open(CACHED_DB_ROOT);

	if (!shapeDb)
	{
		std::cerr << "Failed to create ShapeDB\n";
		return 1;
	}

	QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

	QSurfaceFormat format;
	format.setDepthBufferSize(24);
	format.setStencilBufferSize(8);
	format.setVersion(3, 3);
	format.setProfile(QSurfaceFormat::CompatibilityProfile);
	QSurfaceFormat::setDefaultFormat(format);

	QApplication app(argc, argv);
	theme::apply(app, theme);

	MainWindow window(std::make_unique<ShapeDb>(*shapeDb));
	window.setWindowTitle("Geometry Retrieval");
	window.resize(1000, 700);
	window.show();

	return QApplication::exec();
}
