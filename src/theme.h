#pragma once

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QStyleFactory>

namespace Theme
{
	inline void apply(QApplication& app)
	{
		QApplication::setStyle(QStyleFactory::create("Fusion"));

		QPalette palette;

		palette.setColor(QPalette::Window, QColor(0x111318));
		palette.setColor(QPalette::WindowText, QColor(0xE6EAF0));

		palette.setColor(QPalette::Base, QColor(0x181B22));
		palette.setColor(QPalette::AlternateBase, QColor(0x20242D));
		palette.setColor(QPalette::Text, QColor(0xE6EAF0));

		palette.setColor(QPalette::Button, QColor(0x252A34));
		palette.setColor(QPalette::ButtonText, QColor(0xE6EAF0));

		palette.setColor(QPalette::BrightText, QColor(0xFFFFFF));
		palette.setColor(QPalette::Highlight, QColor(0x4F8CFF));
		palette.setColor(QPalette::HighlightedText, QColor(0xFFFFFF));

		palette.setColor(QPalette::PlaceholderText, QColor(0x788296));

		QApplication::setPalette(palette);

		app.setStyleSheet(R"(
         QWidget {
             font-family: "Inter", "Noto Sans", sans-serif;
             font-size: 13px;
         }

         QMainWindow {
             background: #111318;
         }

         QTabWidget::pane {
             background: #111318;
             border: none;
         }

         QTabBar {
             background: #151820;
         }

         QTabBar::tab {
             color: #8E98AA;
             background: transparent;
             padding: 12px 20px;
             margin-right: 2px;
             border: none;
         }

         QTabBar::tab:hover {
             color: #D9E1EF;
             background: #202530;
         }

         QTabBar::tab:selected {
             color: #FFFFFF;
             background: #2A3140;
             border-bottom: 2px solid #4F8CFF;
         }

         QLabel#pageTitle {
             color: #F4F7FB;
             font-size: 24px;
             font-weight: 700;
         }

         QLabel#pageSubtitle {
             color: #8E98AA;
             font-size: 13px;
         }

         QScrollArea {
             background: transparent;
             border: none;
         }

         QScrollBar:vertical {
             background: transparent;
             width: 10px;
             margin: 2px;
         }

         QScrollBar::handle:vertical {
             background: #3A4252;
             border-radius: 5px;
             min-height: 32px;
         }

         QScrollBar::handle:vertical:hover {
             background: #536078;
         }

         QScrollBar::add-line:vertical,
         QScrollBar::sub-line:vertical {
             height: 0px;
         }

         QPushButton {
             color: #DCE4F2;
             background: #252B36;
             border: 1px solid #374151;
             border-radius: 6px;
             padding: 8px 14px;
         }

         QPushButton:hover {
             background: #303948;
             border-color: #536A8C;
         }

         QPushButton:pressed {
             background: #1D2430;
         }

         QPushButton[primary="true"] {
             color: white;
             background: #3978E8;
             border: 1px solid #4F8CFF;
         }

         QPushButton[primary="true"]:hover {
             background: #4F8CFF;
         }

         QLineEdit {
             color: #E6EAF0;
             background: #181C24;
             border: 1px solid #343C4B;
             border-radius: 6px;
             padding: 9px 11px;
             selection-background-color: #3978E8;
         }

         QLineEdit:focus {
             border: 1px solid #4F8CFF;
         }

         QFrame#meshCard {
             background: #1A1E26;
             border: 1px solid #2D3440;
             border-radius: 10px;
         }

         QFrame#meshCard:hover {
             background: #1E2430;
             border: 1px solid #46546A;
         }

         QFrame#previewContainer {
             background: #0D0F13;
             border: 1px solid #29303B;
             border-radius: 7px;
         }

         QLabel#meshName {
             color: #F4F7FB;
             font-size: 15px;
             font-weight: 600;
         }

         QLabel#meshStats {
             color: #8994A7;
             font-size: 12px;
         }
     )");
	}
} // namespace Theme
