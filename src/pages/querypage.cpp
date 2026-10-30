#include "querypage.h"

#include <QComboBox>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QVBoxLayout>
#include <QVector>
#include <filesystem>
#include <iostream>
#include <qquickwindow.h>
#include <utility>

QueryPage::QueryPage(QWidget* parent)
	: QWidget(parent)
{
	setupUI();

	// Important: Do not change the order of the labels, this should be the same order as the feature vector
	setWeightLabels(
		{"Volume", "Surface area", "Compactness", "3D rectangularity", "Diameter", "Convexity", "Eccentricity", "A3", "D1", "D2", "D3", "D4"});
}

QueryPage::~QueryPage() {}

void QueryPage::setupUI()
{
	QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

	auto* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(16, 16, 16, 16);

	auto* splitter = new QSplitter(Qt::Horizontal, this);
	splitter->setChildrenCollapsible(true);
	mainLayout->addWidget(splitter);

	// Sidebar
	auto* sidebar = new QWidget(splitter);
	sidebar->setObjectName("querySidebar");
	sidebar->setMinimumWidth(240);

	auto* sidebarLayout = new QVBoxLayout(sidebar);
	sidebarLayout->setContentsMargins(8, 8, 8, 8);
	sidebarLayout->setSpacing(12);

	// Model upload
	auto* fileLabel = new QLabel("Upload a model", sidebar);
	sidebarLayout->addWidget(fileLabel);

	auto* fileLayout = new QHBoxLayout();

	modelPathInput = new QLineEdit(sidebar);
	modelPathInput->setPlaceholderText("Path");
	fileLayout->addWidget(modelPathInput, 1);

	browseButton = new QPushButton("Browse", sidebar);
	fileLayout->addWidget(browseButton);

	sidebarLayout->addLayout(fileLayout);

	// Weights
	auto* weightsTitle = new QLabel("Weights", sidebar);
	sidebarLayout->addWidget(weightsTitle);

	weightsWidget = new QWidget(sidebar);
	weightsLayout = new QVBoxLayout(weightsWidget);
	weightsLayout->setContentsMargins(0, 0, 0, 0);
	weightsLayout->setSpacing(8);
	sidebarLayout->addWidget(weightsWidget);

	auto* topKLabel = new QLabel("Number of results (K)", sidebar);
	sidebarLayout->addWidget(topKLabel);

	topKInput = new QSpinBox(sidebar);
	topKInput->setRange(1, 100);
	topKInput->setValue(10);
	sidebarLayout->addWidget(topKInput);

	// Search button
	sidebarLayout->addStretch();

	searchButton = new QPushButton("Start search", sidebar);
	searchButton->setObjectName("primaryButton");
	searchButton->setMinimumHeight(40);
	sidebarLayout->addWidget(searchButton);

	// Content
	auto* contentWidget = new QWidget(splitter);
	auto* contentLayout = new QVBoxLayout(contentWidget);
	contentLayout->setContentsMargins(0, 0, 0, 0);
	contentLayout->setSpacing(12);

	auto* title = new QLabel("Search results", contentWidget);
	title->setObjectName("pageTitle");
	contentLayout->addWidget(title);

	scrollArea = new QScrollArea(contentWidget);
	scrollArea->setWidgetResizable(true);
	scrollArea->setFrameShape(QFrame::NoFrame);

	containerWidget = new QWidget(scrollArea);
	containerLayout = new QGridLayout(containerWidget);
	containerLayout->setContentsMargins(0, 0, 0, 0);
	containerLayout->setHorizontalSpacing(14);
	containerLayout->setVerticalSpacing(14);
	containerLayout->setColumnStretch(0, 1);
	containerLayout->setColumnStretch(1, 1);
	containerLayout->setAlignment(Qt::AlignTop);

	scrollArea->setWidget(containerWidget);
	contentLayout->addWidget(scrollArea, 1);

	splitter->setStretchFactor(0, 1);
	splitter->setStretchFactor(1, 2);
	splitter->setSizes({300, 700});

	connect(searchButton, &QPushButton::clicked, this, &QueryPage::startSearch);
	connect(
		browseButton, &QPushButton::clicked, this,
		[this]()
		{
			const QString path = QFileDialog::getOpenFileName(
				this, "Select 3D model", QString(),
				"3D models (*.obj);;"
				"All files (*)");

			if (!path.isEmpty())
				modelPathInput->setText(path);
		});
}

void QueryPage::setWeightLabels(const QStringList& labels)
{
	weightInputs.clear();

	while (auto* item = weightsLayout->takeAt(0))
	{
		delete item->widget();
		delete item;
	}

	for (const QString& label : labels)
	{
		auto* row = new QWidget(weightsWidget);
		auto* rowLayout = new QHBoxLayout(row);
		rowLayout->setContentsMargins(0, 0, 0, 0);
		rowLayout->setSpacing(8);

		auto* nameLabel = new QLabel(label, row);
		rowLayout->addWidget(nameLabel, 1);

		auto* weightInput = new QDoubleSpinBox(row);
		weightInput->setRange(0.0, 1.0);
		weightInput->setDecimals(3);
		weightInput->setSingleStep(0.01);
		weightInput->setValue(0.5);
		weightInput->setFixedWidth(90);

		rowLayout->addWidget(weightInput);

		weightsLayout->addWidget(row);
		weightInputs.push_back(weightInput);
	}

	weightsLayout->addStretch();
}

std::filesystem::path QueryPage::getSelectedMeshPath() const
{
	const QString uploadedPath = modelPathInput->text().trimmed();
	return std::filesystem::path(uploadedPath.toStdString());
}

std::vector<double> QueryPage::getWeights() const
{
	std::vector<double> weights;
	weights.reserve(static_cast<std::size_t>(weightInputs.size()));

	for (const auto* input : weightInputs)
		weights.push_back(input->value());

	return weights;
}

void QueryPage::startSearch()
{
	const auto meshPath = getSelectedMeshPath();

	if (meshPath.empty())
	{
		QMessageBox::warning(this, "Search", "Please select a model.");
		return;
	}

	if (!std::filesystem::exists(meshPath))
	{
		QMessageBox::warning(this, "Search", "The selected file does not exist.");
		return;
	}

	currentQueryMesh = meshPath;
	const auto weights = getWeights();
	const auto topK = static_cast<std::size_t>(topKInput->value());
	Query query(meshPath, weights);
	const auto results = query.search(topK);

	displayResults(results);
}

void QueryPage::displayResults(const std::vector<QueryResult>& results)
{
	clearResultCards();

	// Display the query model first
	const QString queryName = QString::fromStdString(currentQueryMesh.filename().string());
	auto* queryCard = createModelCard(queryName, currentQueryMesh, "Query model");

	containerLayout->addWidget(queryCard, 0, 0);
	resultCards.push_back(queryCard);

	for (std::size_t i = 0; i < results.size(); ++i)
	{
		const auto& result = results[i];
		const QString name = QString::fromStdString(result.name);
		const QString subtitle = QString("Distance: %1").arg(result.distance, 0, 'f', 5);

		auto* card = createModelCard(name, result.meshPath, subtitle);

		const int index = static_cast<int>(i) + 1;
		const int row = index / 2;
		const int column = index % 2;

		containerLayout->addWidget(card, row, column);
		resultCards.push_back(card);
	}
}

QWidget* QueryPage::createModelCard(const QString& name, const std::filesystem::path path, const QString& subtitle)
{
	auto* card = new QFrame(containerWidget);
	card->setObjectName("modelCard");
	card->setFrameShape(QFrame::StyledPanel);

	auto* layout = new QVBoxLayout(card);
	layout->setContentsMargins(10, 10, 10, 10);
	layout->setSpacing(8);

	auto* title = new QLabel(name, card);
	title->setWordWrap(true);
	title->setObjectName("cardTitle");
	layout->addWidget(title);

	auto* preview = new BrowserCard(name, path, card);

	layout->addWidget(preview, 1);

	auto* info = new QLabel(subtitle, card);
	info->setWordWrap(true);
	info->setObjectName("cardSubtitle");
	layout->addWidget(info);

	return card;
}

void QueryPage::clearResultCards()
{
	for (auto* card : resultCards)
	{
		containerLayout->removeWidget(card);
		delete card;
	}

	resultCards.clear();
}