#ifndef RESULTCARD_H
#define RESULTCARD_H

#include "../geometry/mesh.hpp"

#include <QFrame>
#include <QString>
#include <filesystem>
#include <optional>

class ResultCard : public QFrame {
public:
	ResultCard(const QString& name, const std::filesystem::path& meshSource, const QString& subtitle, QWidget* parent = nullptr);
	ResultCard(const QString& name, const Mesh& mesh, const QString& subtitle, QWidget* parent = nullptr);

private:
	void setupUI(const QString& name, const QString& subtitle);

	std::optional<Mesh> mesh_;
};

#endif // RESULTCARD_H
