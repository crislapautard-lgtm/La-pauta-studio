#pragma once
#include <JuceHeader.h>

class VoiceRecorder
{
public:
    VoiceRecorder() { buffer.setSize(2, 44100*60*10); buffer.clear(); }
    void start() { isRecording=true; writePos=0; takeCount++; }
    void stop() { isRecording=false; }
    void addSample(float l, float r) { if(!isRecording) return; if(writePos<buffer.getNumSamples()) { buffer.setSample(0, writePos, l); buffer.setSample(1, writePos, r); writePos++; } }
    bool saveToWav(juce::File file) { juce::WavAudioFormat fmt; std::unique_ptr<juce::AudioFormatWriter> w(fmt.createWriterFor(file.createOutputStream().release(), 44100, 2, 16, {}, 0)); if(w==nullptr) return false; w->writeFromAudioSampleBuffer(buffer, 0, writePos); return true; }
    int getTakeCount() { return takeCount; }
    int getRecordedSamples() { return writePos; }
    bool isRecordingNow() { return isRecording; }
    void clear() { buffer.clear(); writePos=0; }
private:
    juce::AudioBuffer<float> buffer; int writePos=0; int takeCount=0; bool isRecording=false;
};
