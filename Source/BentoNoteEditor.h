/*
  ==============================================================================

    NoteEditor.h
    Created: 23 Jan 2022 10:58:42am
    Author:  Halil Kleinmann

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace te = tracktion_engine;

namespace bento
{

class Instrument;

//==============================================================================
struct NoteEditor   : public juce::Component,
                      private te::SelectableListener
{
    //==============================================================================
    NoteEditor (te::MidiClip& sc, bento::Instrument* instrument)
        : clip (sc), transport (sc.edit.getTransport())
    {
        for(int i = 127; i > 0; i--)
            visibleNotes.add(i);
        
        for (auto n : visibleNotes)
            addAndMakeVisible (laneConfigs.add (new LaneConfig (*this, n, instrument)));

        addAndMakeVisible (patternEditor);

        timer.setCallback ([this] { patternEditor.updatePaths(); });
        timer.startTimerHz (10);

        clip.addSelectableListener (this);
    }

    ~NoteEditor()
    {
        clip.cancelAnyPendingUpdates();
        clip.removeSelectableListener (this);
        timer.stopTimer();
    }
    
    te::MidiList& getPattern() const
    {
        return clip.getSequence();
    }

    //==============================================================================
    struct LaneConfig  : public Component
    {
        LaneConfig (NoteEditor& se, int noteNumber, bento::Instrument* instrument)
            : editor (se)
        {
            keyButton.setColour(TextButton::ColourIds::textColourOffId , juce::Colours::grey);
            keyButton.setColour(TextButton::ColourIds::textColourOnId , juce::Colours::grey);
            keyButton.setColour(TextButton::ColourIds::buttonColourId, Colour::fromString( (MidiMessage::isMidiNoteBlack(noteNumber)) ? "FF000000" : "FFCCCCCC") );
            keyButton.setColour(TextButton::ColourIds::buttonOnColourId , juce::Colours::lightgrey);
            keyButton.setColour(ComboBox::outlineColourId, Colour::fromString("FF000000"));
            keyButton.setButtonText( MidiMessage::getMidiNoteName(noteNumber, true, true, 3) + " (" + juce::String(noteNumber) + ")" );
            keyButton.onStateChange = [this, noteNumber]
            {
                if(keyButton.isDown())
                {
                    auto note = juce::MidiMessage::noteOn(1, noteNumber, (juce::uint8) 100);
                    editor.clip.getAudioTrack()->injectLiveMidiMessage(note, tracktion_engine::MidiMessageArray::notMPE);
                }
            };
            keyButton.onClick = [this, noteNumber]
            {
                auto note = juce::MidiMessage::noteOff(1, noteNumber, (juce::uint8) 100);
                editor.clip.getAudioTrack()->injectLiveMidiMessage(note, tracktion_engine::MidiMessageArray::notMPE);
            };
            addAndMakeVisible (&keyButton);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            keyButton.setBounds (r.removeFromLeft (r.getWidth()));
        }

        NoteEditor& editor;
        TextButton keyButton;
    };

    //==============================================================================
    struct PatternEditor    : public Component
    {
        PatternEditor (NoteEditor& se)
            : editor (se)
        {
        }
        
        struct NoteMatrix
        {
            float cellWidth;
            float cellHeight;
            
            int cellColNumber;
            int cellRowNumber;
            
            Array<float> cellXes;
            Array<float> cellYes;
            
            int cellSelectStart = -1;
            int cellSelectEnd = -1;
            
            int getCellSelectLength()
            {
                return cellSelectEnd - cellSelectStart;
            }
        };

        void updatePaths()
        {
            playheadRect = {};
            gridNote.clear();
            gridBeat.clear();
            gridBar.clear();
            activeCells.clear();
            playingCells.clear();

            auto& sequence = editor.clip.getSequence();
            auto notes = sequence.getNotes();
            const float indent = 2.0f;

            const bool isPlaying = editor.transport.isPlaying();
            const float playheadX = getPlayheadX();
            
            updateNoteMatrix();
            //DBG("noteMatrix.cellWidth: " + String(noteMatrix.cellWidth) + " noteMatrix.cellHeight: " + String(noteMatrix.cellHeight));

            //------------------------------------------
            // Add Note Selection
            //------------------------------------------
            const float x = noteMatrix.cellWidth * ( int( noteMatrix.cellSelectStart % noteMatrix.cellColNumber ) - 1 );
            const float y = noteMatrix.cellHeight * ( int( noteMatrix.cellSelectStart / noteMatrix.cellColNumber ) );
            const int selectedCellNumber = noteMatrix.cellSelectEnd - noteMatrix.cellSelectStart + 1;
            
            selectionRect.setX ( x );
            selectionRect.setY ( y );
            
            selectionRect.setWidth ( noteMatrix.cellWidth * selectedCellNumber );
            selectionRect.setHeight ( noteMatrix.cellHeight );
            
            //DBG("noteMatrix.cellSelectStart: " + juce::String ( noteMatrix.cellSelectStart ) + " noteMatrix.cellSelectEnd: " + juce::String ( noteMatrix.cellSelectEnd ));

            //------------------------------------------
            // Add NoteCells
            //------------------------------------------
            
            for (auto visibleNote : editor.visibleNotes)
            {
                for(auto note : notes)
                {
                    if(visibleNote == note->getNoteNumber() && note->getStartBeat() < editor.clip.getLengthInBeats())
                    {
                        int noteNumber = note->getNoteNumber();
                        double noteStart = note->getStartBeat();
                        double noteLength = note->getLengthBeats();
                        
                        int visibleNoteNumberStart = editor.visibleNotes.getFirst();

                        float x = noteMatrix.cellWidth * (noteStart * 4);
                        float y = noteMatrix.cellHeight * (visibleNoteNumberStart - noteNumber);
                        
                        //DBG("Note # " + String(i) + " noteNumber: " + String(noteNumber) + " noteStart: " + String(noteStart) + " noteLength: " + String(noteLength));
                        //DBG("X: " + String(x) + " Y: " + String(y));

                        Path& path = (isPlaying && playheadX >= x && playheadX < x + (noteMatrix.cellWidth * noteLength * 4)) ? playingCells : activeCells;
                        
                        const Rectangle<float> rect (x, y, (float) noteMatrix.cellWidth * noteLength * 4, noteMatrix.cellHeight);
                        path.addRoundedRectangle (rect.reduced (jlimit (0.5f, indent, rect.getWidth()  / 8.0f), jlimit (0.5f, indent, rect.getHeight() / 8.0f)), 5.0f);
                    }
                        
                }
            }

            //------------------------------------------
            // Add horizontal lines
            //------------------------------------------
            for(int i = 0; i < editor.visibleNotes.size(); i++)
            {
                gridNote.addWithoutMerging ({ 0.0f, (float) noteMatrix.cellHeight * i, (float) getWidth(), (float) 0.5f });
                //gridNote.addWithoutMerging ({ 0.0f, y.getStart() - 0.25f, (float) getWidth(), 0.5f });
            }

            //------------------------------------------
            // Add the vertical lines (notes)
            //------------------------------------------
            for (float x : noteMatrix.cellXes)
                gridNote.addWithoutMerging ({ x - 0.25f, 0.0f, 0.5f, (float) getHeight() });
            
            //------------------------------------------
            // Add the vertical lines (beats)
            //------------------------------------------
            auto gridBeatOffset = noteMatrix.cellXes[0];
            for (float i = 0; i < noteMatrix.cellXes.size(); i += 4)
            {
                gridBeat.addWithoutMerging ({ noteMatrix.cellXes[i] - gridBeatOffset - 0.25f, 0.0f, 0.5f, (float) getHeight() });
            }

            //------------------------------------------
            // Add the vertical lines (bar)
            //------------------------------------------
            auto gridBarOffset = noteMatrix.cellXes[0];
            for (float i = 0; i < noteMatrix.cellXes.size(); i += 16)
            {
                gridBar.addWithoutMerging ({ noteMatrix.cellXes[i] - gridBarOffset - 0.25f, 0.0f, 0.5f, (float) getHeight() });
                gridBar.addWithoutMerging ({ noteMatrix.cellXes[i] - gridBarOffset - 1.50f, 0.0f, 1.75f, (float) getHeight() });
                gridBar.addWithoutMerging ({ noteMatrix.cellXes[i] - gridBarOffset - 2.75f, 0.0f, 3.00f, (float) getHeight() });
            }
                

            // Add the missing right and bottom edges (notes)
            {
                auto r = getLocalBounds().toFloat();
                gridNote.addWithoutMerging (r.removeFromBottom (0.5f).translated (0.0f, -0.25f));
                gridNote.addWithoutMerging (r.removeFromLeft (0.5f).translated (-0.25f, 0.0f));
            }
            
            // Add the missing right and bottom edges (beats)
            {
                auto r = getLocalBounds().toFloat();
                gridBeat.addWithoutMerging (r.removeFromBottom (0.5f).translated (0.0f, -0.25f));
                gridBeat.addWithoutMerging (r.removeFromLeft (0.5f).translated (-0.25f, 0.0f));
                gridBeat.addWithoutMerging (r.removeFromTop (0.5f).translated (0.25f, 0.0f));
                gridBeat.addWithoutMerging (r.removeFromRight (0.5f).translated (0.25f, 0.0f));
            }

            //------------------------------------------
            // Calculate playhead rect
            //------------------------------------------
            {
                auto r = getLocalBounds().toFloat();
                float lastX = 0.0f;

                for (float x : noteMatrix.cellXes)
                {
                    if (playheadX >= lastX && playheadX < x)
                    {
                        playheadRect = r.withX (lastX).withRight (x);
                        break;
                    }

                    lastX = x;
                }
            }

            repaint();
        }

        void paint (Graphics& g) override
        {
            g.setColour (Colours::white.withMultipliedAlpha (0.5f));
            g.fillRectList (gridNote);
            
            g.setColour (Colours::white.withMultipliedAlpha (1.0f));
            g.fillRectList (gridBeat);
            
            g.setColour (Colours::white.withMultipliedAlpha (1.0f));
            g.fillRectList (gridBar);

            g.setColour (Colours::darkorange.withMultipliedAlpha (0.7f));
            g.fillPath (activeCells);

            g.setColour (Colours::orange);
            g.fillPath (playingCells);

            g.setColour (Colours::white.withMultipliedAlpha (0.2f));
            g.fillRect (playheadRect);

            g.setColour (Colours::orange.withMultipliedAlpha (0.5f));
            g.fillRect (selectionRect);
            
        }

        void resized() override
        {
            updateNoteMatrix();
            updatePaths();
        }

        void mouseEnter (const MouseEvent& e) override {}
        
        void mouseExit (const MouseEvent&) override {}

        void mouseMove (const MouseEvent& e) override {}

        void mouseDown (const MouseEvent& e) override
        {
            auto cellIndex = pointToNoteMatrixCellIndex ( { (float) e.x, (float) e.y } );
            //DBG("mouseDown - CellIndex: " + juce::String(cellIndex));
            
            if( auto note = checkNoteAtCellIndex (cellIndex) )
            {
                //DBG ("Note Found: " + juce::String( note->getNoteNumber() ) );
                editor.clip.getSequence().removeNote (*note, nullptr);
                noteMatrix.cellSelectStart = -1;
            }
            else
            {
                noteMatrix.cellSelectStart = cellIndex;
            }
            
        }

        void mouseDrag (const MouseEvent& e) override
        {
            if( noteMatrix.cellSelectStart == -1 )
                return;
            
            int cellIndex = pointToNoteMatrixCellIndex({(float) e.x, (float) e.y});
            int maxCellSelectIndex = ( int (noteMatrix.cellSelectStart / noteMatrix.cellColNumber) + 1) * noteMatrix.cellColNumber;
            noteMatrix.cellSelectEnd = ( cellIndex > maxCellSelectIndex ) ? maxCellSelectIndex : cellIndex;
            //DBG("mouseDrag - CellIndex: " + juce::String(cellIndex) + " cellSelectEnd: " + juce::String(noteMatrix.cellSelectEnd) + " maxCellSelectIndex: " + juce::String(maxCellSelectIndex));
        }

        void mouseUp (const MouseEvent&) override
        {
            if( noteMatrix.cellSelectStart == -1 )
                return;
            
            int noteNumber = convertCellIndexToNoteNumber ( noteMatrix.cellSelectStart );
            double notePos = convertCellIndexToNotePosition ( noteMatrix.cellSelectStart );
            double noteLength = double ( noteMatrix.cellSelectEnd - noteMatrix.cellSelectStart + 1) / 4.0;
            DBG(
                " mouseUp - NoteNumber: "   + juce::String( noteNumber ) +
                " SelectLength: "           + juce::String( (double) noteLength ) +
                " notePos: "                + juce::String( (double) notePos )
                );
            editor.clip.getSequence().addNote(noteNumber, notePos, noteLength, 127, 0, nullptr);
            
            noteMatrix.cellSelectStart = -1;
            noteMatrix.cellSelectEnd = -1;
        }

    private:

        void updateNoteMatrix()
        {
            noteMatrix.cellXes.clearQuick();
            noteMatrix.cellYes.clearQuick();
            
            auto r = getLocalBounds().toFloat();
            noteMatrix.cellColNumber = editor.clip.getLengthInBeats() * 4;
            noteMatrix.cellRowNumber = editor.visibleNotes.size();
            noteMatrix.cellWidth = r.getWidth() / noteMatrix.cellColNumber;
            noteMatrix.cellHeight = r.getHeight() / noteMatrix.cellRowNumber;
            
            for (int i = 0; i < noteMatrix.cellColNumber; ++i)
                noteMatrix.cellXes.add ((i + 1) * noteMatrix.cellWidth);
            
            for (int i = 0; i < noteMatrix.cellRowNumber; ++i)
                noteMatrix.cellYes.add ((i + 1) * noteMatrix.cellHeight);
        }
        
        te::MidiNote* checkNoteAtCellIndex( int cellIndex )
        {
            int noteNumber = convertCellIndexToNoteNumber ( cellIndex );
            double notePos = convertCellIndexToNotePosition ( cellIndex ) + 0.001;
            auto notes = editor.clip.getSequence().getNotes();
            
            for ( auto note : notes )
            {
                //DBG ( "Note searching: notePos: " + juce::String( double(notePos) ) + " Start: " + juce::String( double (note->getStartBeat()) ) + " End: " + juce::String( double (note->getEndBeat()) ) );
                if ( note->getNoteNumber() == noteNumber && note->getStartBeat() <= notePos && note->getEndBeat() >= notePos )
                {
                    return note;
                    /*
                    for ( double pos = notePos; pos >= 0.0; pos -= 0.25 )
                    {
                        //DBG ( "Note searching: " + juce::String( (double) pos ) );
                        if( note->getBeatPosition() == pos )
                        {
                            return note;
                        }
                    }
                     */
                }
            }
            return nullptr;
        }

        float getPlayheadX() const
        {
            auto clipRange = editor.clip.getEditTimeRange();
            if (clipRange.isEmpty())
                return 0.0f;
            
            double position = editor.transport.position;
            double proportion = position / clipRange.getLength();
            auto r = getLocalBounds().toFloat();
            double offset = floor(proportion) * clipRange.getLength();
            
            proportion = (position - offset) / clipRange.getLength();
            /*
            auto factor = editor.transport.getLoopRange().getLength() / clipRange.getLength();
            const double clipLength = clipRange.getLength();
            const double position = editor.transport.position - (clipLength * factor) ;
            */
             
            /*
            DBG("Offset: " + juce::String(offset));
            DBG("position: " + juce::String(position));
            DBG("position in Bar: " + juce::String( editor.clip.edit.tempoSequence.timeToBarsBeats(position).bars ));
            DBG("proportion: " + juce::String(proportion));
            DBG("r.getWidth(): " + juce::String(r.getWidth()));
            DBG("clipRange.getLength(): " + juce::String(clipRange.getLength()));
            DBG("ClipLength in Bars: " + juce::String(editor.clip.edit.tempoSequence.timeToBarsBeats(clipRange.getLength()).bars ));
            DBG("r.getWidth() * float (proportion) = " + juce::String(r.getWidth() * float (proportion)));
            DBG("-----------------------------------------------");
            */
            
            return r.getWidth() * float (proportion);
        }

        int xToSequenceIndex (float x) const
        {
            if (x >= 0)
                for (int i = 0; i < noteMatrix.cellXes.size(); ++i)
                    if (x < noteMatrix.cellXes.getUnchecked (i))
                        return i;

            return -1;
        }
        
        int pointToNoteMatrixCellIndex(Point<float> point)
        {
            //DBG("pointToNoteMatrixCellIndex: cellColNumber: " + juce::String(noteMatrix.cellColNumber) + " noteMatrix.cellRowNumber: " + juce::String(noteMatrix.cellRowNumber));
            //DBG("pointToNoteMatrixCellIndex: X: " + juce::String(point.getX()) + " Y: " + juce::String(point.getY()));
            
            int colCellPos = -1;
            if (point.getX() >= 0)
            {
                for(int i = 0; i < noteMatrix.cellColNumber; ++i)
                {
                    //DBG("Check --- point.getX(): " + juce::String(point.getX()) + " cellXes: " + juce::String(noteMatrix.cellXes.getUnchecked(i)));
                    if (point.getX() < noteMatrix.cellXes.getUnchecked(i))
                    {
                        colCellPos = i;
                        break;
                    }
                }
            }

            int rowCellPos = -1;
            if (point.getY() >= 0)
            {
                for(int i = 0; i < noteMatrix.cellRowNumber; ++i)
                {
                    if (point.getY() < noteMatrix.cellYes.getUnchecked(i))
                    {
                        rowCellPos = i;
                        break;
                    }
                }
            }

            //DBG("pointToNoteMatrixCellIndex: colCellPos: " + juce::String(colCellPos) + " rowCellPos: " + juce::String(rowCellPos));
                
            if(colCellPos >= 0 && rowCellPos >= 0)
            {
                return (rowCellPos * noteMatrix.cellColNumber) + colCellPos + 1;
            }
            else
                return -1;
        }
        
        int convertCellIndexToNoteNumber(int cellIndex)
        {
            int noteIndex = ( cellIndex - 1 ) / noteMatrix.cellColNumber;
            return editor.visibleNotes[noteIndex];
        }
        
        double convertCellIndexToNotePosition(int cellIndex)
        {
            double notePos = ( ( cellIndex - 1 ) % noteMatrix.cellColNumber );
            double noteTimePos = notePos / 4.0;
            return noteTimePos;
        }

        Range<float> getSequenceIndexXRange (int index) const
        {
            if (! isPositiveAndBelow (index, noteMatrix.cellXes.size()) || noteMatrix.cellXes.size() <= 2)
                return {};

            if (index == 0)
                return { 0.0f, noteMatrix.cellXes.getUnchecked (index) };

            return { noteMatrix.cellXes.getUnchecked (index - 1), noteMatrix.cellXes.getUnchecked (index) };
        }

        NoteEditor& editor;
        RectangleList<float> gridNote, gridBeat, gridBar;
        Path activeCells, playingCells;
        Rectangle<float> playheadRect, selectionRect;
        int mouseOverCellIndex = -1, mouseOverChannel = -1;
        bool paintSolidCells = true;
        double playheadXPosition = 0.0;
        
        NoteMatrix noteMatrix;
        int mouseOverNoteCellIndex = -1;

    };

    
    void paint (Graphics&) override
    {
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto configR = r.removeFromLeft (150);

        int index = 0;
        for (auto c : laneConfigs)
        {
            auto channelR = configR.toFloat();
            channelR.setVerticalRange (getChannelYRange (index));
            c->setBounds (channelR.getSmallestIntegerContainer());
            index++;
        }

        patternEditor.setBounds (r);
    }
    
    PatternEditor& getEditor()
    {
        return patternEditor;
    }
    
    void setOctave(int octave)
    {
        currentOctave = octave;
        setVisibleNotes({octave * 12, (octave * 12) + 12 });
    }

private:
    te::MidiClip& clip;
    te::TransportControl& transport;
    te::LambdaTimer timer;

    juce::OwnedArray<LaneConfig> laneConfigs;
    PatternEditor patternEditor { *this };

    Array<int> visibleNotes;
    int currentOctave = 3;

    //==============================================================================
    
    Array<int> getVisibleNotes()
    {
        return visibleNotes;
    }
    
    Array<int> setVisibleNotes(Range<int> noteRange)
    {
        if(noteRange.getStart() > noteRange.getEnd())
            return {};
        
        visibleNotes.clear();
        for(int i = noteRange.getEnd(); i > noteRange.getStart(); --i)
        {
            visibleNotes.add(i);
        }
        return visibleNotes;
    }

    Range<float> getChannelYRange (int channelIndex) const
    {
        auto r = getLocalBounds().toFloat();
        const int numChans = visibleNotes.size();

        if (numChans == 0)
            return {};

        const float h = r.getHeight() / numChans;

        return Range<float>::withStartAndLength (channelIndex * h, h);
    }

    void selectableObjectChanged (te::Selectable*) override
    {
        // This is our Clip telling us that one of it's properties has changed
        patternEditor.updatePaths();
    }

    void selectableObjectAboutToBeDeleted (te::Selectable*) override
    {
        jassertfalse;
    }
};

}  // namespace bento

