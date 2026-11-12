#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace defeedback
{

/** A clean segmented selector, used for the protection mode. */
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
        const auto b = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colour (0xff1a1f2b));
        g.fillRoundedRectangle (b, 6.0f);
        g.setColour (juce::Colour (0xff242b3a));
        g.drawRoundedRectangle (b, 6.0f, 1.0f);

        if (items_.isEmpty())
            return;

        const auto n = items_.size();
        const auto w = b.getWidth() / (float) n;

        for (int i = 0; i < n; ++i)
        {
            auto seg = b.withWidth (w).withX (b.getX() + w * (float) i).reduced (3.0f, 3.0f);

            if (i == selected_)
            {
                g.setColour (juce::Colour (0xff35e0c3));
                g.fillRoundedRectangle (seg, 4.0f);
            }
            else if (i == hover_)
            {
                g.setColour (juce::Colour (0xff242b3a));
                g.fillRoundedRectangle (seg, 4.0f);
            }

            g.setColour (i == selected_ ? juce::Colour (0xff0a0c11) : juce::Colour (0xff8993a8));
            g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
            g.drawText (items_[i], seg.toNearestInt(), juce::Justification::centred, false);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int idx = segmentAt (e.getPosition());
        if (idx >= 0)
            setSelectedIndex (idx);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int idx = segmentAt (e.getPosition());
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
    int segmentAt (juce::Point<int> p) const
    {
        if (items_.isEmpty())
            return -1;

        const auto w = getWidth() / (float) items_.size();
        const int idx = juce::jlimit (0, items_.size() - 1, (int) ((float) p.x / juce::jmax (1.0f, w)));
        return idx;
    }

    juce::StringArray items_;
    int selected_ = 0;
    int hover_ = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SegmentedControl)
};

} // namespace defeedback
