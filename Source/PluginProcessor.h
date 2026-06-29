#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

class CrystalVoiceAudioProcessor final : public juce::AudioProcessor
{
public:
    CrystalVoiceAudioProcessor();
    ~CrystalVoiceAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 4; }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    using APVTS = juce::AudioProcessorValueTreeState;
    APVTS parameters;

    void applyFactoryPreset (int index);
    juce::String getFactoryPresetName (int index) const;

    float getInputMeterDb() const noexcept { return inputMeterDb.load(); }
    float getOutputMeterDb() const noexcept { return outputMeterDb.load(); }
    float getCorrectionCents() const noexcept { return correctionCents.load(); }
    float getGainReductionDb() const noexcept { return gainReductionDb.load(); }

private:
    static APVTS::ParameterLayout createParameterLayout();

    // A lightweight time-domain pitch shifter. It is deliberately optimised for
    // monophonic vocal material and short grains, rather than offline mastering.
    class GranularPitchShifter
    {
    public:
        void prepare (double newSampleRate);
        void reset();
        float process (float input, float ratio);
        int getLatencySamples() const noexcept { return baseDelaySamples; }

    private:
        float readInterpolated (float position) const;
        float readGrain (float phase, float ratio) const;

        std::vector<float> delayBuffer;
        int writeIndex = 0;
        int grainSamples = 512;
        int baseDelaySamples = 768;
        float phaseA = 0.0f;
        float phaseB = 0.5f;
    };

    class PitchTracker
    {
    public:
        void prepare (double newSampleRate);
        void reset();
        void pushSample (float sample);
        float getFrequencyHz() const noexcept { return frequencyHz; }
        float getConfidence() const noexcept { return confidence; }

    private:
        void analyse();

        std::vector<float> samples;
        int writeIndex = 0;
        int sampleCounter = 0;
        int analysisHop = 512;
        double sampleRate = 48000.0;
        float frequencyHz = 0.0f;
        float confidence = 0.0f;
    };

    struct DeEsser
    {
        void reset();
        float process (float input, float amount, double sampleRate);

        float lowState = 0.0f;
        float envelope = 0.0f;
    };

    struct AirEnhancer
    {
        void reset();
        float process (float input, float amount, double sampleRate);

        float lowState = 0.0f;
    };

    struct Compressor
    {
        void reset();
        float process (float input, float amount, double sampleRate);
        float getReductionDb() const noexcept { return reductionDb; }

        float envelope = 0.0f;
        float reductionDb = 0.0f;
    };

    struct StereoDelay
    {
        void prepare (double newSampleRate);
        void reset();
        void process (juce::AudioBuffer<float>& buffer, float mix, float feedback);

        std::array<std::vector<float>, 2> buffers;
        int writeIndex = 0;
        int delaySamples = 12000;
        double sampleRate = 48000.0;
    };

    static int positiveModulo (int value, int modulus) noexcept;
    static int floorDivide (int value, int divisor) noexcept;
    static float frequencyToMidi (float frequencyHz) noexcept;
    static float midiToFrequency (float midiNote) noexcept;
    static int quantiseToScale (float midiNote, int root, int scaleMode) noexcept;
    static int getScaleDegree (int quantisedMidi, int root, int scaleMode) noexcept;
    static int getDiatonicThird (int quantisedMidi, int root, int scaleMode, int direction) noexcept;
    static float calculateRmsDb (const juce::AudioBuffer<float>& buffer) noexcept;

    void setParameterValue (const juce::String& id, float plainValue);
    float getParameterValue (const juce::String& id) const noexcept;

    std::array<GranularPitchShifter, 2> leadShifters;
    std::array<GranularPitchShifter, 2> harmonyUpShifters;
    std::array<GranularPitchShifter, 2> harmonyDownShifters;
    PitchTracker pitchTracker;
    std::array<DeEsser, 2> deEssers;
    std::array<AirEnhancer, 2> airEnhancers;
    std::array<Compressor, 2> compressors;
    StereoDelay stereoDelay;
    juce::Reverb reverb;

    double currentSampleRate = 48000.0;
    float smoothedLeadRatio = 1.0f;
    float smoothedUpRatio = 1.0f;
    float smoothedDownRatio = 1.0f;
    int currentProgram = 0;

    std::atomic<float> inputMeterDb { -100.0f };
    std::atomic<float> outputMeterDb { -100.0f };
    std::atomic<float> correctionCents { 0.0f };
    std::atomic<float> gainReductionDb { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrystalVoiceAudioProcessor)
};
