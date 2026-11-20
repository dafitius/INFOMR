#pragma once

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QStyleFactory>

namespace theme
{
	enum class Mode { Dark, Light };

	struct Colors {
		QColor window;
		QColor windowText;
		QColor base;
		QColor alternateBase;
		QColor text;
		QColor button;
		QColor buttonText;
		QColor highlight;
		QColor placeholder;
		QColor tabBar;
		QColor tabText;
		QColor tabHover;
		QColor tabSelected;
		QColor border;
		QColor card;
		QColor cardHover;
		QColor preview;
		QColor muted;
		QColor scrollbar;
		QColor scrollbarHover;
		QColor input;
		QColor inputBorder;
		QColor primary;
		QColor primaryHover;
	};

	inline Colors colors(Mode mode) {
		if (mode == Mode::Dark) {
			return {
				QColor("#111318"), // window
				QColor("#E6EAF0"), // windowText
				QColor("#181B22"), // base
				QColor("#20242D"), // alternateBase
				QColor("#E6EAF0"), // text
				QColor("#252A34"), // button
				QColor("#E6EAF0"), // buttonText
				QColor("#4F8CFF"), // highlight
				QColor("#788296"), // placeholder
				QColor("#151820"), // tabBar
				QColor("#8E98AA"), // tabText
				QColor("#202530"), // tabHover
				QColor("#2A3140"), // tabSelected
				QColor("#2D3440"), // border
				QColor("#1A1E26"), // card
				QColor("#1E2430"), // cardHover
				QColor("#0D0F13"), // preview
				QColor("#8994A7"), // muted
				QColor("#3A4252"), // scrollbar
				QColor("#536078"), // scrollbarHover
				QColor("#181C24"), // input
				QColor("#343C4B"), // inputBorder
				QColor("#3978E8"), // primary
				QColor("#4F8CFF"), // primaryHover
			};
		}

		return {
			QColor("#FFFFFF"), // window
			QColor("#202733"), // windowText
			QColor("#FFFFFF"), // base
			QColor("#F0F3F7"), // alternateBase
			QColor("#202733"), // text
			QColor("#E9EDF3"), // button
			QColor("#202733"), // buttonText
			QColor("#3978E8"), // highlight
			QColor("#8994A7"), // placeholder
			QColor("#EDF0F5"), // tabBar
			QColor("#626F82"), // tabText
			QColor("#E0E6EF"), // tabHover
			QColor("#FFFFFF"), // tabSelected
			QColor("#D8DEE8"), // border
			QColor("#FFFFFF"), // card
			QColor("#F5F8FC"), // cardHover
			QColor("#E9EDF3"), // preview
			QColor("#69778C"), // muted
			QColor("#C5CDD9"), // scrollbar
			QColor("#9AA7BA"), // scrollbarHover
			QColor("#FFFFFF"), // input
			QColor("#C8D0DC"), // inputBorder
			QColor("#3978E8"), // primary
			QColor("#2867D5"), // primaryHover
		};
	}

	inline void apply(QApplication& app, const Mode mode) {
		const auto colorMode = colors(mode);
		QApplication::setStyle(QStyleFactory::create("Fusion"));

		QPalette palette;
		palette.setColor(QPalette::Window, colorMode.window);
		palette.setColor(QPalette::WindowText, colorMode.windowText);
		palette.setColor(QPalette::Base, colorMode.base);
		palette.setColor(QPalette::AlternateBase, colorMode.alternateBase);
		palette.setColor(QPalette::Text, colorMode.text);
		palette.setColor(QPalette::Button, colorMode.button);
		palette.setColor(QPalette::ButtonText, colorMode.buttonText);
		palette.setColor(QPalette::BrightText, QColor("#FFFFFF"));
		palette.setColor(QPalette::Highlight, colorMode.highlight);
		palette.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
		palette.setColor(QPalette::PlaceholderText, colorMode.placeholder);

		QApplication::setPalette(palette);

		const QString cssQWidget = R"(
    QWidget {
        font-family: "Inter", "Noto Sans", sans-serif;
        font-size: 13px;
    }

    QScrollArea {
        background: transparent;
        border: none;
    }
)";

		const QString cssQMainWindow = QString(R"(
    QMainWindow {
        background: %1;
    }

    QTabWidget::pane {
        background: %1;
        border: none;
    }
)")
										   .arg(colorMode.window.name());

		const QString cssQTabBar = QString(R"(
    QTabBar {
        background: %1;
    }

    QTabBar::tab {
        color: %2;
        background: transparent;
        padding: 12px 20px;
        margin-right: 2px;
        border: none;
    }

    QTabBar::tab:hover {
        color: %3;
        background: %4;
    }

    QTabBar::tab:selected {
        color: %5;
        background: %6;
        border-bottom: 2px solid %7;
    }
)")
									   .arg(colorMode.tabBar.name())
									   .arg(colorMode.tabText.name())
									   .arg(colorMode.windowText.name())
									   .arg(colorMode.tabHover.name())
									   .arg(colorMode.text.name())
									   .arg(colorMode.tabSelected.name())
									   .arg(colorMode.highlight.name());

		const QString cssQLabel = QString(R"(
    QLabel#pageTitle {
        color: %1;
        font-size: 24px;
        font-weight: 700;
    }

    QLabel#pageSubtitle {
        color: %2;
        font-size: 13px;
    }

    QLabel#meshName {
        color: %1;
        font-size: 15px;
        font-weight: 600;
    }

    QLabel#meshStats {
        color: %2;
        font-size: 12px;
    }
)")
									  .arg(colorMode.text.name())
									  .arg(colorMode.muted.name());

		const QString cssQScrollBar = QString(R"(
    QScrollBar:vertical {
        background: transparent;
        width: 10px;
        margin: 2px;
    }

    QScrollBar::handle:vertical {
        background: %1;
        border-radius: 5px;
        min-height: 32px;
    }

    QScrollBar::handle:vertical:hover {
        background: %2;
    }

    QScrollBar::add-line:vertical,
    QScrollBar::sub-line:vertical {
        height: 0px;
    }
)")
										  .arg(colorMode.scrollbar.name())
										  .arg(colorMode.scrollbarHover.name());

		const QString cssQPushButton = QString(R"(
    QPushButton {
        color: %1;
        background: %2;
        border: 1px solid %3;
        border-radius: 6px;
        padding: 8px 14px;
    }

    QPushButton:hover {
        background: %4;
        border-color: %5;
    }

    QPushButton:pressed {
        background: %6;
    }

    QPushButton[primary="true"] {
        color: white;
        background: %7;
        border: 1px solid %8;
    }

    QPushButton[primary="true"]:hover {
        background: %9;
    }
)")
										   .arg(colorMode.buttonText.name())
										   .arg(colorMode.button.name())
										   .arg(colorMode.border.name())
										   .arg(colorMode.tabHover.name())
										   .arg(colorMode.scrollbarHover.name())
										   .arg(colorMode.tabSelected.name())
										   .arg(colorMode.primary.name())
										   .arg(colorMode.highlight.name())
										   .arg(colorMode.primaryHover.name());

		const QString cssQFrame = QString(R"(
    QFrame#meshCard {
        background: %1;
        border: 1px solid %2;
        border-radius: 10px;
    }

    QFrame#meshCard:hover {
        background: %3;
        border: 1px solid %4;
    }

    QFrame#previewContainer {
        background: %5;
        border: 1px solid %2;
        border-radius: 7px;
    }
)")
									  .arg(colorMode.card.name())
									  .arg(colorMode.border.name())
									  .arg(colorMode.cardHover.name())
									  .arg(colorMode.scrollbarHover.name())
									  .arg(colorMode.preview.name());

		const QString cssQLineEdit = QString(R"(
    QLineEdit {
        color: %1;
        background: %2;
        border: 1px solid %3;
        border-radius: 6px;
        padding: 9px 11px;
        selection-background-color: %4;
    }

    QLineEdit:focus {
        border: 1px solid %5;
    }
)")
										 .arg(colorMode.text.name())
										 .arg(colorMode.input.name())
										 .arg(colorMode.inputBorder.name())
										 .arg(colorMode.primary.name())
										 .arg(colorMode.highlight.name());

		const QString css = cssQWidget + cssQMainWindow + cssQTabBar + cssQLabel + cssQScrollBar + cssQPushButton + cssQFrame + cssQLineEdit;

		app.setStyleSheet(css);
	}
} // namespace theme