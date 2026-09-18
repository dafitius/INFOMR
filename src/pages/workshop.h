//
// Created by dafitius on 18-09-2026.
//

#ifndef QTTEST_WORKSHOP_H
#define QTTEST_WORKSHOP_H

#include <QWidget>
#include <memory>

class QQuickWidget;

class Workshop : public QWidget
{
	Q_OBJECT

public:
	explicit Workshop(QWidget* parent = nullptr);
	~Workshop() override;
	void loadMesh(const class OffFile& mesh);

private:
	void setupUI();
	std::unique_ptr<QQuickWidget> viewWidget;
};

#endif // QTTEST_WORKSHOP_H