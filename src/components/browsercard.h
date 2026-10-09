#ifndef MESHCARD_H
#define MESHCARD_H

#include <QFrame>
#include <QString>

#include "../geometry/mesh.hpp"

class BrowserCard : public QFrame
{
	Q_OBJECT

public:
	BrowserCard(const QString& name, const std::filesystem::path& meshSource, QWidget* parent = nullptr);
	~BrowserCard() override;

signals:
	void meshClicked(const QString& meshName);

private:
	void setupUI();

	QString meshName;
	std::optional<Mesh> object;
};

#endif // MESHCARD_H