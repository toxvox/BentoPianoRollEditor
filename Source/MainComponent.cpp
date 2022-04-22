#include "MainComponent.h"
#include "Utilities.h"

//==============================================================================
MainComponent::MainComponent()
{
    auto editFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile( "MySong.tracktionedit" );
    edit = te::createEmptyEdit(engine, editFile);
    
    auto audioTrack = EngineHelpers::getOrInsertAudioTrackAt (*edit, 0);
    const te::EditTimeRange editTimeRange (0, edit->tempoSequence.barsBeatsToTime ({ 1, 0.0 }));
    auto clip = dynamic_cast<te::MidiClip*>(audioTrack->insertNewClip (te::TrackItem::Type::midi, "MidiClip", editTimeRange, nullptr));
    
    noteEditor = std::make_unique<bento::NoteEditor>(*clip, nullptr);
    noteEditor->setSize(600, 2400);

    viewPort.setViewedComponent(noteEditor.get(), false);
    viewPort.setSize(getWidth(), getHeight());
    viewPort.setViewPosition(0, 1000);
    addAndMakeVisible(&viewPort);
 
    setSize (600, 400);
}

MainComponent::~MainComponent()
{
}

void MainComponent::paint (juce::Graphics& g)
{
}

void MainComponent::resized()
{
    viewPort.setBounds(getLocalBounds());
}
