#ifndef QUERYPAGE_H
#define QUERYPAGE_H

#include "../components/browsercard.h"
#include "../querying/query.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>
#include <QVector>
#include <QWidget>
#include <filesystem>
#include <vector>

class QueryPage : public QWidget
{
	Q_OBJECT

public:
	explicit QueryPage(QWidget* parent = nullptr);
	~QueryPage() override;

	void setWeightLabels(const QStringList& labels);

signals:
	void searchRequested();

private:
	void setupUI();

	void clearResultCards();
	QWidget* createModelCard(const QString& name, const std::filesystem::path path, const QString& subtitle);
	void startSearch();

	std::filesystem::path getSelectedMeshPath() const;
	std::vector<double> getWeights() const;

	void displayResults(const std::vector<QueryResult>& results);

	QSpinBox* topKInput = nullptr;
	QVector<QDoubleSpinBox*> weightInputs;
	std::filesystem::path currentQueryMesh;

	QLineEdit* modelPathInput = nullptr;
	QPushButton* browseButton = nullptr;
	QPushButton* searchButton = nullptr;

	QWidget* sidebar = nullptr;
	QVBoxLayout* sidebarLayout = nullptr;
	QWidget* weightsWidget = nullptr;
	QVBoxLayout* weightsLayout = nullptr;

	QScrollArea* scrollArea = nullptr;
	QWidget* containerWidget = nullptr;
	QGridLayout* containerLayout = nullptr;

	QVector<QWidget*> resultCards;
};

#endif // QUERYPAGE_H
