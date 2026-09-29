#ifndef BROWSEPAGE_H
#define BROWSEPAGE_H

#include <QGridLayout>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>
#include <vector>
#include "../io/shape_db.hpp"

#include "../components/browsercard.h"

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
	void addMeshCard(const MeshData& mesh);
	void loadSampleMeshes(); // TODO: Create a proper structure to load the model db

	std::unique_ptr<ShapeDb> database;

	QScrollArea* scrollArea;
	QWidget* containerWidget;
	QGridLayout* containerLayout;
	std::vector<BrowserCard*> meshCards;

private slots:
	void onMeshCardClicked(const QString& meshName);
};

#endif // BROWSEPAGE_H