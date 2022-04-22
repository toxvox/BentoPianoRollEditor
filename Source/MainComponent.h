#pragma once

#include <JuceHeader.h>
#include "BentoNoteEditor.h"

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent  : public juce::Component
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    //==============================================================================
    // Your private member variables go here...
    
    te::Engine engine { ProjectInfo::projectName };
    
    std::unique_ptr<te::Edit> edit;

    std::unique_ptr<bento::NoteEditor> noteEditor;
    juce::Viewport viewPort;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
