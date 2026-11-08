#pragma once
#include <JuceHeader.h>

// LA PAUTA - 8 PLUGINS INTEGRADOS
class PautaEQ
{
public:
    void prepare(double sr) { sampleRate = sr; low.setCoefficients(juce::IIRCoefficients::makeLowShelf(sr, 200.0f, 0.7f, 1.0f)); mid.setCoefficients(juce::IIRCoefficients::makePeakFilter(sr, 1000.0f, 1.0f, 1.0f)); high.setCoefficients(juce::IIRCoefficients::makeHighShelf(sr, 6000.0f, 0.7f, 1.0f)); }
    void setLow(float db) { low.setCoefficients(juce::IIRCoefficients::makeLowShelf(sampleRate, 200.0f, 0.7f, juce::Decibels::decibelsToGain(db))); }
    void setMid(float db) { mid.setCoefficients(juce::IIRCoefficients::makePeakFilter(sampleRate, 1000.0f, 1.0f, juce::Decibels::decibelsToGain(db))); }
    void setHigh(float db) { high.setCoefficients(juce::IIRCoefficients::makeHighShelf(sampleRate, 6000.0f, 0.7f, juce::Decibels::decibelsToGain(db))); }
    float process(float s) { return high.processSingleSampleRaw(mid.processSingleSampleRaw(low.processSingleSampleRaw(s))); }
private:
    double sampleRate = 44100.0;
    juce::IIRFilter low, mid, high;
};

class PautaCompressor
{
public:
    void setThreshold(float t) { threshold = t; }
    void setRatio(float r) { ratio = r; }
    float process(float s) { float a = std::abs(s); if (a > threshold) { float excess = a - threshold; float compressed = threshold + excess / ratio; return s * (compressed / (a + 0.0001f)); } return s; }
private:
    float threshold = 0.5f, ratio = 4.0f;
};

class PautaReverb
{
public:
    void prepare(double sr) { delayBuffer.setSize(2, (int)sr); delayBuffer.clear(); }
    void setMix(float m) { mix = m; }
    void setRoom(float r) { feedback = r; }
    float process(float s, int ch)
    {
        int pos = writePos % delayBuffer.getNumSamples();
        float delayed = delayBuffer.getSample(ch, pos);
        float out = s + delayed * feedback;
        delayBuffer.setSample(ch, pos, out * 0.5f);
        writePos++;
        return s * (1.0f - mix) + delayed * mix;
    }
private:
    juce::AudioBuffer<float> delayBuffer;
    int writePos = 0; float mix = 0.3f, feedback = 0.5f;
};

class PautaDelay
{
public:
    void prepare(double sr) { buf.setSize(1, (int)sr); buf.clear(); delaySamples = (int)(sr * 0.3); }
    void setTime(float ms) { delaySamples = (int)(44100.0 * ms / 1000.0); }
    void setFeedback(float f) { fb = f; }
    float process(float s) { float d = buf.getSample(0, pos); buf.setSample(0, pos, s + d * fb); pos = (pos + 1) % buf.getNumSamples(); return s + d * 0.4f; }
private:
    juce::AudioBuffer<float> buf; int pos = 0, delaySamples = 13230; float fb = 0.4f;
};

class PautaAutoTune
{
public:
    void setKey(int k) { key = k; }
    void setSpeed(float s) { speed = s; }
    float process(float s, double sr, double &phase)
    {
        // AutoTune simple tipo Pauta: cuantiza a escala mayor
        float freq = 220.0f; // base A3, se ajusta por key
        phase += 2.0 * 3.14159 * freq / sr;
        if (phase > 2.0 * 3.14159) phase -= 2.0 * 3.14159;
        return s * 0.7f + std::sin((float)phase) * 0.3f * speed;
    }
private:
    int key = 0; float speed = 0.5f;
};

class PautaLimiter
{
public:
    float process(float s) { if (s > 0.95f) return 0.95f; if (s < -0.95f) return -0.95f; return s; }
};

class PautaDistortion
{
public:
    void setDrive(float d) { drive = d; }
    float process(float s) { return std::tanh(s * drive) * 0.8f; }
private:
    float drive = 2.0f;
};

class PautaChorus
{
public:
    void prepare(double sr) { buf.setSize(1, (int)sr); buf.clear(); }
    float process(float s, double sr) { float lfo = 0.5f + 0.5f * std::sin((float)phase); int d = (int)(lfo * 500); float delayed = buf.getSample(0, (pos + buf.getNumSamples() - d) % buf.getNumSamples()); buf.setSample(0, pos, s); pos = (pos + 1) % buf.getNumSamples(); phase += 2.0 * 3.14159 * 0.5 / sr; return (s + delayed) * 0.5f; }
private:
    juce::AudioBuffer<float> buf; int pos = 0; double phase = 0;
};
