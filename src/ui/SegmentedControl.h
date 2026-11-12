#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/LookAndFeel.h"

#include <functional>

namespace defeedback
{

/** A vertically stacked selector, used for the protection mode. */
class SegmentedControl : public juce::Component
{
public:
    explicit SegmentedControl (juce::StringArray items) : items_ (std::move (items)) {}

    void setSelectedIndex (int index, juce::NotificationType notify = juce::sendNotification)
    {
        index = juce::jlimit (0, juce::jmax (0, items_.size() - 1), index);
        if (index == selected_)
            return;

        selected_ = index;
        repaint();
        if (notify != juce::dontSendNotification && onChange)
            onChange (selected_);
    }

    int getSelectedIndex() const noexcept { return selected_; }

    std::function<void (int)> onChange;

    void paint (juce::Graphics& g) override
    {
        if (items_.isEmpty())
            return;

        const auto b = getLocalBounds().toFloat();
        const auto rowH = b.getHeight() / (float) items_.size();

        g.setFont (DeFeedbackLookAndFeel::caps (11.5f));

        for (int i = 0; i < items_.size(); ++i)
        {
            const auto row = b.withHeight (rowH).withY (b.getY() + rowH * (float) i).reduced (0.0f, 2.0f);

            if (i == selected_)
            {
                g.setColour (Palette::accent);
                g.fillRoundedRectangle (row, 7.0f);
            }
            else if (i == hover_)
            {
                g.setColour (Palette::surfaceHi);
                g.fillRoundedRectangle (row, 7.0f);
            }

            g.setColour (i == selected_ ? Palette::background : (i == hover_ ? Palette::text : Palette::textDim));
            g.drawText (items_[i], row.toNearestInt(), juce::Justification::centred, false);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { setSelectedIndex (rowAt (e.getPosition())); }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int idx = rowAt (e.getPosition());
        if (idx != hover_)
        {
            hover_ = idx;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hover_ = -1;
        repaint();
    }

private:
    int rowAt (juce::Point<int> p) const
    {
        const auto rowH = (float) getHeight() / (float) juce::jmax (1, items_.size());
        return juce::jlimit (0, juce::jmax (0, items_.size() - 1), (int) ((float) p.y / juce::jmax (1.0f, rowH)));
    }

    juce::StringArray items_;
    int selected_ = 0;
    int hover_ = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SegmentedControl)
};

} // namespace defeedback
