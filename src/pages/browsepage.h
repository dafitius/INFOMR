#ifndef BROWSEPAGE_H
#define BROWSEPAGE_H

#include <QComboBox>
#include <QGridLayout>
#include <QScrollArea>
#include <QWidget>
#include <vector>

#include "../components/browsercard.h"
#include "../io/shape_db.hpp"

struct MeshData
{
	QString name;
	std::filesystem::path source;
};

class BrowsePage : public QWidget
{
	Q_OBJECT

public:
	explicit BrowsePage(std::shared_ptr<ShapeDb> database, QWidget* parent = nullptr);
	~BrowsePage() override;

signals:
	void meshSelected(const std::filesystem::path& source);

private:
	void setupUI();
	void setupClassList();
	void addMeshCard(const MeshData& mesh);
	void loadSelectedClass();
	void clearMeshCards();

	std::shared_ptr<ShapeDb> database;

	QScrollArea* scrollArea;
	QWidget* containerWidget;
	QGridLayout* containerLayout;

	QComboBox* classComboBox;

	std::vector<BrowserCard*> meshCards;

private slots:
	static void onMeshCardClicked(const QString& meshName);
};

#endif // BROWSEPAGE_H