#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "io/shape_db.hpp"


#include <QMainWindow>
#include <QTabWidget>

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(std::unique_ptr<ShapeDb> database, QWidget* parent = nullptr);
	~MainWindow() override;

private:
	void setupUI(std::unique_ptr<ShapeDb> database);

	QTabWidget* tabWidget;
};

#endif // MAINWINDOW_H