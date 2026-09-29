#ifndef BROWSEPAGE_H
#define BROWSEPAGE_H

#include <QComboBox>
#include <QGridLayout>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>
#include <vector>

#include "../components/browsercard.h"
#include "../io/shape_db.hpp"

struct MeshData
{
	QString name;
	QString source;
};

class BrowsePage : public QWidget
{
	Q_OBJECT

public:
	BrowsePage(std::unique_ptr<ShapeDb> database, QWidget* parent = nullptr);
	~BrowsePage();

signals:
	void meshSelected(const QString& source);

private:
	void setupUI();
	void setupClassList();
	void addMeshCard(const MeshData& mesh);
	void loadSelectedClass();
	void clearMeshCards();

	std::unique_ptr<ShapeDb> database;

	QScrollArea* scrollArea;
	QWidget* containerWidget;
	QGridLayout* containerLayout;

	QComboBox* classComboBox;

	std::vector<BrowserCard*> meshCards;

private slots:
	void onMeshCardClicked(const QString& meshName);
};

#endif // BROWSEPAGE_H