#ifndef MESHCARD_H
#define MESHCARD_H

#include <QFrame>
#include <QString>

#include "../io/mesh.hpp"

class BrowserCard : public QFrame
{
	Q_OBJECT

public:
	BrowserCard(const QString& name, QString mesh_source, QWidget* parent = nullptr);
	~BrowserCard() override;

signals:
	void meshClicked(const QString& meshName);

private:
	void setupUI();

	QString meshName;
	std::optional<Mesh> object;
};

#endif // MESHCARD_H