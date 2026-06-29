#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr std::array<int, 7> kMajorScale { 0, 2, 4, 5, 7, 9, 11 };
constexpr std::array<int, 7> kMinorScale { 0, 2, 3, 5, 7, 8, 10 };

const std::array<int, 7>& getScale (int mode)
{
    return mode == 1 ? kMinorScale : kMajorScale;
}

float dbFromLinear (float value)
{
    return juce::Decibels::gainToDecibels (juce::jmax (value, 0.0000001f), -100.0f);
}

float onePoleCoefficient (double sampleRate, float frequency)
{
    return std::exp (-2.0f * kPi * frequency / static_cast<float> (sampleRate));
}
} // namespace

CrystalVoiceAudioProcessor::CrystalVoiceAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

CrystalVoiceAudioProcessor::APVTS::ParameterLayout CrystalVoiceAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;

    auto addFloat = [&layout] (const juce::String& id, const juce::String& name,
                               float min, float max, float interval, float defaultValue,
                               const juce::String& suffix = {})
    {
        auto attributes = juce::AudioParameterFloatAttributes {};
        if (suffix.isNotEmpty())
            attributes = attributes.withLabel (suffix);

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float> { min, max, interval }, defaultValue, attributes));
    };

    auto addChoice = [&layout] (const juce::String& id, const juce::String& name,
                                juce::StringArray choices, int defaultIndex)
    {
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { id, 1 }, name, choices, defaultIndex));
    };

    addFloat ("inputGain",  "Input",          -18.0f, 18.0f, 0.1f, 0.0f, " dB");
    addFloat ("tuneAmount", "Pitch correction", 0.0f, 1.0f, 0.01f, 0.68f);
    addFloat ("retuneMs",   "Correction speed", 8.0f, 150.0f, 1.0f, 48.0f, " ms");
    addChoice ("key", "Key", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0);
    addChoice ("scale", "Scale", { "Major", "Minor" }, 0);
    addChoice ("harmonyMode", "Harmony", { "Off", "3rd above", "3rd below", "Both thirds", "Double" }, 0);
    addFloat ("harmonyMix", "Harmony level", 0.0f, 0.85f, 0.01f, 0.42f);
    addFloat ("deEss", "De-esser", 0.0f, 1.0f, 0.01f, 0.48f);
    addFloat ("air", "Air", 0.0f, 1.0f, 0.01f, 0.42f);
    addFloat ("compress", "Compression", 0.0f, 1.0f, 0.01f, 0.34f);
    addFloat ("reverb", "Space", 0.0f, 1.0f, 0.01f, 0.20f);
    addFloat ("delay", "Delay", 0.0f, 0.75f, 0.01f, 0.0f);
    addFloat ("outputGain", "Output", -18.0f, 6.0f, 0.1f, 0.0f, " dB");

    return layout;
}

void CrystalVoiceAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;

    for (auto& shifter : leadShifters)        shifter.prepare (sampleRate);
    for (auto& shifter : harmonyUpShifters)   shifter.prepare (sampleRate);
    for (auto& shifter : harmonyDownShifters) shifter.prepare (sampleRate);

    pitchTracker.prepare (sampleRate);
    for (auto& processor : deEssers)      processor.reset();
    for (auto& processor : airEnhancers)  processor.reset();
    for (auto& processor : compressors)   processor.reset();

    stereoDelay.prepare (sampleRate);
    reverb.reset();
    smoothedLeadRatio = smoothedUpRatio = smoothedDownRatio = 1.0f;

    // This latency comes from the short overlap-add grain buffer. Hosts such as
    // FL Studio can compensate it automatically on mixer insert tracks.
    setLatencySamples (leadShifters[0].getLatencySamples());
}

void CrystalVoiceAudioProcessor::releaseResources()
{
}

bool CrystalVoiceAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    return mainIn == juce::AudioChannelSet::mono()
        || mainIn == juce::AudioChannelSet::stereo();
}

void CrystalVoiceAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalInputChannels = getTotalNumInputChannels();
    const auto totalOutputChannels = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    for (auto channel = totalInputChannels; channel < totalOutputChannels; ++channel)
        buffer.clear (channel, 0, numSamples);

    if (totalInputChannels == 0 || numSamples == 0)
        return;

    const float inputGain = juce::Decibels::decibelsToGain (getParameterValue ("inputGain"));
    const float outputGain = juce::Decibels::decibelsToGain (getParameterValue ("outputGain"));
    const float tuneAmount = getParameterValue ("tuneAmount");
    const float retuneMs = getParameterValue ("retuneMs");
    const int root = static_cast<int> (std::round (getParameterValue ("key")));
    const int scaleMode = static_cast<int> (std::round (getParameterValue ("scale")));
    const int harmonyMode = static_cast<int> (std::round (getParameterValue ("harmonyMode")));
    const float harmonyMix = getParameterValue ("harmonyMix");
    const float deEssAmount = getParameterValue ("deEss");
    const float airAmount = getParameterValue ("air");
    const float compression = getParameterValue ("compress");
    const float reverbAmount = getParameterValue ("reverb");
    const float delayAmount = getParameterValue ("delay");

    const auto smoothing = 1.0f - std::exp (-1.0f / (static_cast<float> (currentSampleRate) * (retuneMs * 0.001f)));

    auto* left = buffer.getWritePointer (0);
    auto* right = totalInputChannels > 1 ? buffer.getWritePointer (1) : nullptr;
    float inputEnergy = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float inL = left[sample] * inputGain;
        float inR = right != nullptr ? right[sample] * inputGain : inL;
        const float mono = 0.5f * (inL + inR);
        inputEnergy += 0.5f * (inL * inL + inR * inR);

        pitchTracker.pushSample (mono);

        const float detectedHz = pitchTracker.getFrequencyHz();
        const float confidence = pitchTracker.getConfidence();
        const bool hasPitch = detectedHz > 60.0f && confidence > 0.58f;

        float targetLeadRatio = 1.0f;
        float targetUpRatio = 1.0f;
        float targetDownRatio = 1.0f;
        float cents = 0.0f;

        if (hasPitch)
        {
            const float rawMidi = frequencyToMidi (detectedHz);
            const int quantisedMidi = quantiseToScale (rawMidi, root, scaleMode);
            const float correctedMidi = rawMidi + (static_cast<float> (quantisedMidi) - rawMidi) * tuneAmount;

            targetLeadRatio = midiToFrequency (correctedMidi) / detectedHz;
            const int upMidi = getDiatonicThird (quantisedMidi, root, scaleMode, +1);
            const int downMidi = getDiatonicThird (quantisedMidi, root, scaleMode, -1);
            targetUpRatio = midiToFrequency (static_cast<float> (upMidi)) / detectedHz;
            targetDownRatio = midiToFrequency (static_cast<float> (downMidi)) / detectedHz;
            cents = (correctedMidi - rawMidi) * 100.0f;
        }

        targetLeadRatio = juce::jlimit (0.72f, 1.45f, targetLeadRatio);
        targetUpRatio = juce::jlimit (0.60f, 1.65f, targetUpRatio);
        targetDownRatio = juce::jlimit (0.60f, 1.65f, targetDownRatio);

        smoothedLeadRatio += (targetLeadRatio - smoothedLeadRatio) * smoothing;
        smoothedUpRatio += (targetUpRatio - smoothedUpRatio) * smoothing;
        smoothedDownRatio += (targetDownRatio - smoothedDownRatio) * smoothing;

        const float upRatio = harmonyMode == 4 ? smoothedLeadRatio * 1.004f : smoothedUpRatio;
        const float downRatio = harmonyMode == 4 ? smoothedLeadRatio * 0.996f : smoothedDownRatio;
        const float leadL = leadShifters[0].process (inL, smoothedLeadRatio);
        const float leadR = leadShifters[1].process (inR, smoothedLeadRatio);
        const float upL = harmonyUpShifters[0].process (inL, upRatio);
        const float upR = harmonyUpShifters[1].process (inR, upRatio);
        const float downL = harmonyDownShifters[0].process (inL, downRatio);
        const float downR = harmonyDownShifters[1].process (inR, downRatio);

        float outL = leadL;
        float outR = leadR;

        switch (harmonyMode)
        {
            case 1: // 3rd above
                if (hasPitch)
                {
                    outL += upL * harmonyMix;
                    outR += upR * harmonyMix;
                }
                break;
            case 2: // 3rd below
                if (hasPitch)
                {
                    outL += downL * harmonyMix;
                    outR += downR * harmonyMix;
                }
                break;
            case 3: // both thirds
                if (hasPitch)
                {
                    outL += (upL + downL) * (harmonyMix * 0.62f);
                    outR += (upR + downR) * (harmonyMix * 0.62f);
                }
                break;
            case 4: // double: does not need an active note detection
                outL += (upL + downL) * (harmonyMix * 0.45f);
                outR += (upR + downR) * (harmonyMix * 0.45f);
                break;
            default:
                break;
        }

        outL = deEssers[0].process (outL, deEssAmount, currentSampleRate);
        outR = deEssers[1].process (outR, deEssAmount, currentSampleRate);
        outL = airEnhancers[0].process (outL, airAmount, currentSampleRate);
        outR = airEnhancers[1].process (outR, airAmount, currentSampleRate);
        outL = compressors[0].process (outL, compression, currentSampleRate);
        outR = compressors[1].process (outR, compression, currentSampleRate);

        left[sample] = outL * outputGain;
        if (right != nullptr)
            right[sample] = outR * outputGain;

        correctionCents.store (cents);
    }

    juce::Reverb::Parameters reverbParameters;
    reverbParameters.roomSize = 0.25f + reverbAmount * 0.45f;
    reverbParameters.damping = 0.52f;
    reverbParameters.wetLevel = reverbAmount * 0.34f;
    reverbParameters.dryLevel = 1.0f;
    reverbParameters.width = 1.0f;
    reverbParameters.freezeMode = 0.0f;
    reverb.setParameters (reverbParameters);

    if (right != nullptr)
        reverb.processStereo (left, right, numSamples);
    else
        reverb.processMono (left, numSamples);

    stereoDelay.process (buffer, delayAmount, 0.26f + delayAmount * 0.26f);

    inputMeterDb.store (dbFromLinear (std::sqrt (inputEnergy / static_cast<float> (juce::jmax (1, numSamples)))));
    outputMeterDb.store (calculateRmsDb (buffer));
    gainReductionDb.store (0.5f * (compressors[0].getReductionDb() + compressors[1].getReductionDb()));
}

juce::AudioProcessorEditor* CrystalVoiceAudioProcessor::createEditor()
{
    return new CrystalVoiceAudioProcessorEditor (*this);
}

void CrystalVoiceAudioProcessor::setCurrentProgram (int index)
{
    applyFactoryPreset (index);
}

const juce::String CrystalVoiceAudioProcessor::getProgramName (int index)
{
    return getFactoryPresetName (index);
}

void CrystalVoiceAudioProcessor::applyFactoryPreset (int index)
{
    currentProgram = juce::jlimit (0, getNumPrograms() - 1, index);

    struct Preset
    {
        float input, tune, speed, key, scale, harmony, harmonyMix, deEss, air, compress, reverb, delay, output;
    };

    static const std::array<Preset, 4> presets
    {{
        { 0.0f, 0.66f, 52.0f, 0.0f, 0.0f, 0.0f, 0.40f, 0.48f, 0.48f, 0.36f, 0.22f, 0.00f, 0.0f }, // Crystal clear
        { 1.0f, 0.78f, 32.0f, 0.0f, 0.0f, 0.0f, 0.48f, 0.55f, 0.58f, 0.45f, 0.18f, 0.05f, -0.5f }, // Idol pop
        { 0.0f, 0.52f, 72.0f, 0.0f, 1.0f, 0.0f, 0.35f, 0.42f, 0.32f, 0.28f, 0.36f, 0.10f, 0.0f }, // Soft ballad
        { 0.0f, 0.90f, 18.0f, 0.0f, 0.0f, 0.0f, 0.45f, 0.56f, 0.60f, 0.52f, 0.16f, 0.03f, -1.0f }  // Pop correction
    }};

    const auto& p = presets[static_cast<size_t> (currentProgram)];
    setParameterValue ("inputGain", p.input);
    setParameterValue ("tuneAmount", p.tune);
    setParameterValue ("retuneMs", p.speed);
    setParameterValue ("key", p.key);
    setParameterValue ("scale", p.scale);
    setParameterValue ("harmonyMode", p.harmony);
    setParameterValue ("harmonyMix", p.harmonyMix);
    setParameterValue ("deEss", p.deEss);
    setParameterValue ("air", p.air);
    setParameterValue ("compress", p.compress);
    setParameterValue ("reverb", p.reverb);
    setParameterValue ("delay", p.delay);
    setParameterValue ("outputGain", p.output);
}

juce::String CrystalVoiceAudioProcessor::getFactoryPresetName (int index) const
{
    static const juce::StringArray names { "Crystal Clear", "Idol Pop", "Soft Ballad", "Pop Correction" };
    return names[juce::jlimit (0, names.size() - 1, index)];
}

void CrystalVoiceAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = parameters.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void CrystalVoiceAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

void CrystalVoiceAudioProcessor::GranularPitchShifter::prepare (double sampleRate)
{
    grainSamples = juce::jlimit (320, 512, static_cast<int> (sampleRate * 0.0080));
    baseDelaySamples = grainSamples + grainSamples / 2;
    delayBuffer.assign (static_cast<size_t> (baseDelaySamples + grainSamples * 3 + 8), 0.0f);
    reset();
}

void CrystalVoiceAudioProcessor::GranularPitchShifter::reset()
{
    std::fill (delayBuffer.begin(), delayBuffer.end(), 0.0f);
    writeIndex = 0;
    phaseA = 0.0f;
    phaseB = 0.5f;
}

float CrystalVoiceAudioProcessor::GranularPitchShifter::readInterpolated (float position) const
{
    const auto size = static_cast<int> (delayBuffer.size());
    while (position < 0.0f) position += static_cast<float> (size);
    while (position >= static_cast<float> (size)) position -= static_cast<float> (size);

    const int index0 = static_cast<int> (position);
    const int index1 = (index0 + 1) % size;
    const float frac = position - static_cast<float> (index0);
    return delayBuffer[static_cast<size_t> (index0)] + frac * (delayBuffer[static_cast<size_t> (index1)] - delayBuffer[static_cast<size_t> (index0)]);
}

float CrystalVoiceAudioProcessor::GranularPitchShifter::readGrain (float phase, float ratio) const
{
    const float offset = (phase - 0.5f) * static_cast<float> (grainSamples) * (1.0f - ratio);
    const float readPosition = static_cast<float> (writeIndex - baseDelaySamples) - offset;
    const float window = 0.5f - 0.5f * std::cos (2.0f * kPi * phase);
    return readInterpolated (readPosition) * window;
}

float CrystalVoiceAudioProcessor::GranularPitchShifter::process (float input, float ratio)
{
    if (delayBuffer.empty())
        return input;

    ratio = juce::jlimit (0.55f, 1.85f, ratio);
    delayBuffer[static_cast<size_t> (writeIndex)] = input;

    const float result = readGrain (phaseA, ratio) + readGrain (phaseB, ratio);
    const float increment = 1.0f / static_cast<float> (grainSamples);
    phaseA += increment;
    phaseB += increment;
    if (phaseA >= 1.0f) phaseA -= 1.0f;
    if (phaseB >= 1.0f) phaseB -= 1.0f;

    writeIndex = (writeIndex + 1) % static_cast<int> (delayBuffer.size());
    return result;
}

void CrystalVoiceAudioProcessor::PitchTracker::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    samples.assign (2048, 0.0f);
    analysisHop = 512;
    reset();
}

void CrystalVoiceAudioProcessor::PitchTracker::reset()
{
    std::fill (samples.begin(), samples.end(), 0.0f);
    writeIndex = 0;
    sampleCounter = 0;
    frequencyHz = 0.0f;
    confidence = 0.0f;
}

void CrystalVoiceAudioProcessor::PitchTracker::pushSample (float sample)
{
    if (samples.empty())
        return;

    samples[static_cast<size_t> (writeIndex)] = sample;
    writeIndex = (writeIndex + 1) % static_cast<int> (samples.size());

    if (++sampleCounter >= analysisHop)
    {
        sampleCounter = 0;
        analyse();
    }
}

void CrystalVoiceAudioProcessor::PitchTracker::analyse()
{
    const int size = static_cast<int> (samples.size());
    const int minLag = juce::jlimit (16, size / 3, static_cast<int> (sampleRate / 900.0));
    const int maxLag = juce::jlimit (minLag + 2, size / 2, static_cast<int> (sampleRate / 75.0));

    float bestScore = -1.0f;
    int bestLag = 0;

    // Evaluate a normalised autocorrelation on every other sample. This keeps
    // the pitch tracker light enough for live use while remaining stable on voice.
    for (int lag = minLag; lag <= maxLag; lag += 2)
    {
        float correlation = 0.0f;
        float energyA = 0.0f;
        float energyB = 0.0f;

        for (int i = 0; i < size - lag; i += 2)
        {
            const int indexA = (writeIndex + i) % size;
            const int indexB = (writeIndex + i + lag) % size;
            const float a = samples[static_cast<size_t> (indexA)];
            const float b = samples[static_cast<size_t> (indexB)];
            correlation += a * b;
            energyA += a * a;
            energyB += b * b;
        }

        const float denominator = std::sqrt (energyA * energyB) + 0.000001f;
        const float score = correlation / denominator;
        if (score > bestScore)
        {
            bestScore = score;
            bestLag = lag;
        }
    }

    if (bestLag > 0 && bestScore > 0.32f)
    {
        const float candidate = static_cast<float> (sampleRate / static_cast<double> (bestLag));
        if (candidate >= 75.0f && candidate <= 1000.0f)
        {
            frequencyHz += (candidate - frequencyHz) * 0.28f;
            confidence += (bestScore - confidence) * 0.35f;
            return;
        }
    }

    confidence *= 0.82f;
}

void CrystalVoiceAudioProcessor::DeEsser::reset()
{
    lowState = 0.0f;
    envelope = 0.0f;
}

float CrystalVoiceAudioProcessor::DeEsser::process (float input, float amount, double sampleRate)
{
    const float coefficient = onePoleCoefficient (sampleRate, 4700.0f);
    lowState = (1.0f - coefficient) * input + coefficient * lowState;
    const float high = input - lowState;

    const float absolute = std::abs (high);
    const float attack = 0.45f;
    const float release = 0.992f;
    envelope = absolute > envelope ? envelope + (absolute - envelope) * attack : envelope * release;

    const float threshold = 0.028f + (1.0f - amount) * 0.10f;
    const float excess = juce::jlimit (0.0f, 1.0f, (envelope - threshold) / 0.18f);
    const float highGain = 1.0f - amount * 0.78f * excess;
    return lowState + high * highGain;
}

void CrystalVoiceAudioProcessor::AirEnhancer::reset()
{
    lowState = 0.0f;
}

float CrystalVoiceAudioProcessor::AirEnhancer::process (float input, float amount, double sampleRate)
{
    const float coefficient = onePoleCoefficient (sampleRate, 6800.0f);
    lowState = (1.0f - coefficient) * input + coefficient * lowState;
    const float high = input - lowState;
    return input + high * (amount * 0.42f);
}

void CrystalVoiceAudioProcessor::Compressor::reset()
{
    envelope = 0.0f;
    reductionDb = 0.0f;
}

float CrystalVoiceAudioProcessor::Compressor::process (float input, float amount, double)
{
    const float absolute = std::abs (input);
    const float attack = 0.16f + amount * 0.18f;
    const float release = 0.997f - amount * 0.003f;
    envelope = absolute > envelope ? envelope + (absolute - envelope) * attack : envelope * release;

    const float threshold = juce::Decibels::decibelsToGain (-4.0f - amount * 19.0f);
    const float ratio = 1.0f + amount * 5.0f;
    float gain = 1.0f;

    if (envelope > threshold)
        gain = std::pow (envelope / threshold, -(ratio - 1.0f) / ratio);

    reductionDb = -dbFromLinear (gain);
    return input * gain;
}

void CrystalVoiceAudioProcessor::StereoDelay::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    const int bufferSize = static_cast<int> (sampleRate * 2.2);
    for (auto& buffer : buffers)
        buffer.assign (static_cast<size_t> (bufferSize), 0.0f);
    delaySamples = juce::jlimit (1, bufferSize - 1, static_cast<int> (sampleRate * 0.285));
    reset();
}

void CrystalVoiceAudioProcessor::StereoDelay::reset()
{
    for (auto& buffer : buffers)
        std::fill (buffer.begin(), buffer.end(), 0.0f);
    writeIndex = 0;
}

void CrystalVoiceAudioProcessor::StereoDelay::process (juce::AudioBuffer<float>& buffer, float mix, float feedback)
{
    if (mix <= 0.0001f || buffers[0].empty())
        return;

    const int numChannels = juce::jmin (2, buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();
    const int size = static_cast<int> (buffers[0].size());

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const int readIndex = (writeIndex - delaySamples + size) % size;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            const float input = data[sample];
            const float delayed = buffers[static_cast<size_t> (channel)][static_cast<size_t> (readIndex)];
            buffers[static_cast<size_t> (channel)][static_cast<size_t> (writeIndex)] = input + delayed * feedback;
            data[sample] = input + delayed * mix;
        }

        writeIndex = (writeIndex + 1) % size;
    }
}

int CrystalVoiceAudioProcessor::positiveModulo (int value, int modulus) noexcept
{
    const int result = value % modulus;
    return result < 0 ? result + modulus : result;
}

int CrystalVoiceAudioProcessor::floorDivide (int value, int divisor) noexcept
{
    int quotient = value / divisor;
    const int remainder = value % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0)))
        --quotient;
    return quotient;
}

float CrystalVoiceAudioProcessor::frequencyToMidi (float frequencyHz) noexcept
{
    return 69.0f + 12.0f * std::log2 (juce::jmax (frequencyHz, 1.0f) / 440.0f);
}

float CrystalVoiceAudioProcessor::midiToFrequency (float midiNote) noexcept
{
    return 440.0f * std::pow (2.0f, (midiNote - 69.0f) / 12.0f);
}

int CrystalVoiceAudioProcessor::quantiseToScale (float midiNote, int root, int scaleMode) noexcept
{
    const auto& scale = getScale (scaleMode);
    const int rounded = static_cast<int> (std::round (midiNote));
    int bestNote = rounded;
    float bestDistance = std::numeric_limits<float>::max();

    for (int note = rounded - 7; note <= rounded + 7; ++note)
    {
        const int pitchClass = positiveModulo (note - root, 12);
        const bool allowed = std::find (scale.begin(), scale.end(), pitchClass) != scale.end();
        if (! allowed)
            continue;

        const float distance = std::abs (static_cast<float> (note) - midiNote);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestNote = note;
        }
    }

    return bestNote;
}

int CrystalVoiceAudioProcessor::getScaleDegree (int quantisedMidi, int root, int scaleMode) noexcept
{
    const auto& scale = getScale (scaleMode);
    const int pitchClass = positiveModulo (quantisedMidi - root, 12);

    for (int degree = 0; degree < static_cast<int> (scale.size()); ++degree)
        if (scale[static_cast<size_t> (degree)] == pitchClass)
            return degree;

    return 0;
}

int CrystalVoiceAudioProcessor::getDiatonicThird (int quantisedMidi, int root, int scaleMode, int direction) noexcept
{
    const auto& scale = getScale (scaleMode);
    const int degree = getScaleDegree (quantisedMidi, root, scaleMode);
    const int octave = floorDivide (quantisedMidi - root, 12);
    const int targetDegreeRaw = degree + direction * 2;
    const int octaveOffset = floorDivide (targetDegreeRaw, 7);
    const int targetDegree = positiveModulo (targetDegreeRaw, 7);
    return root + (octave + octaveOffset) * 12 + scale[static_cast<size_t> (targetDegree)];
}

float CrystalVoiceAudioProcessor::calculateRmsDb (const juce::AudioBuffer<float>& buffer) noexcept
{
    float sum = 0.0f;
    const int samples = buffer.getNumSamples();
    const int channels = buffer.getNumChannels();

    for (int channel = 0; channel < channels; ++channel)
    {
        const auto* data = buffer.getReadPointer (channel);
        for (int sample = 0; sample < samples; ++sample)
            sum += data[sample] * data[sample];
    }

    const float denominator = static_cast<float> (juce::jmax (1, samples * channels));
    return dbFromLinear (std::sqrt (sum / denominator));
}

void CrystalVoiceAudioProcessor::setParameterValue (const juce::String& id, float plainValue)
{
    if (auto* parameter = parameters.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
}

float CrystalVoiceAudioProcessor::getParameterValue (const juce::String& id) const noexcept
{
    if (auto* value = parameters.getRawParameterValue (id))
        return value->load();
    return 0.0f;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CrystalVoiceAudioProcessor();
}
