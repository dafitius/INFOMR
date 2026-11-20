#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "io/shape_db.hpp"


#include <QMainWindow>
#include <QTabWidget>

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(std::shared_ptr<ShapeDb> database, QWidget* parent = nullptr);
	~MainWindow() override;

private:
	void setupUI();
	void openModel(std::filesystem::path source);

	std::shared_ptr<ShapeDb> database_; //database is shared_ptr, so it shouldn't ever get deleted.
	QTabWidget* tabWidget;
};

#endif // MAINWINDOW_H