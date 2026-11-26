// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QCheckBox>

class QPropertyAnimation;

/*
 * A QCheckBox that is drawn as an on/off switch: the label on the left, a sliding pill on the right.
 * It is a normal QCheckBox otherwise (isChecked(), toggled(), stateChanged(), mnemonics, tab focus ...), so it can
 * replace a checkbox in a .ui file by promoting it to "ToggleSwitch".
 * The whole row is clickable, not just the pill.
 */
class ToggleSwitch : public QCheckBox {
    Q_OBJECT
    Q_PROPERTY(qreal knobPosition READ knobPosition WRITE setKnobPosition)

   public:
    explicit ToggleSwitch(QWidget* parent = nullptr);
    explicit ToggleSwitch(const QString& text, QWidget* parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    qreal knobPosition() const { return m_knobPosition; }
    void setKnobPosition(qreal position);

   protected:
    void paintEvent(QPaintEvent* event) override;
    bool hitButton(const QPoint& pos) const override;

   private slots:
    void animateTo(bool checked);

   private:
    void init();
    QString plainText() const;

    qreal m_knobPosition = 0.0;  // 0 = off (knob on the left), 1 = on (knob on the right)
    QPropertyAnimation* m_animation = nullptr;
};
