#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>

//==============================================================================
/**
    DGUTTA RadioReady - whole-mix / master-bus processor.

    Chain (stereo only):
        Input trim
        -> Clean stage   (35 Hz high-pass + mud cut ~300 Hz, depth = Clean)
        -> Glue comp     (Punch = threshold/ratio blend, slow attack)
        -> Presence/Air  (4 kHz presence + 10 kHz shelf, amount = Air)
        -> Saturation    (tanh soft clip, drive tied to Loud)
        -> Maximizer     (makeup gain = Loud, linked peak limiter,
                          -1.0 dBFS ceiling, enforced again at the very end)
        -> Output trim

    Listen (Auto) mode runs a filter-bank analysis over the input and adds
    slow, bounded corrections on top of the Clean / Air / Loud knob values.
*/
class RadioReadyProcessor final : public juce::AudioProcessor
{
public:
    RadioReadyProcessor();
    ~RadioReadyProcessor() override = default;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==========================================================================
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==========================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }

    // Metering data for the editor (all linear amplitudes except GR, which
    // is in positive dB of reduction).
    float getInputLevel (int channel) const noexcept;
    float getOutputLevel (int channel) const noexcept;
    float getGainReductionDb() const noexcept { return gainReductionDb.load (std::memory_order_relaxed); }

    // Listen (Auto) mode readouts for the editor. Band figures are dB
    // relative to the overall signal level; loudness is dBFS RMS, smoothed.
    bool isListening() const noexcept { return listenState.load (std::memory_order_relaxed); }
    float getDetectedLoudnessDb() const noexcept { return detectedLoudnessDb.load (std::memory_order_relaxed); }
    float getMudRelativeDb() const noexcept { return mudRelativeDb.load (std::memory_order_relaxed); }
    float getPresenceRelativeDb() const noexcept { return presenceRelativeDb.load (std::memory_order_relaxed); }
    float getAirRelativeDb() const noexcept { return airRelativeDb.load (std::memory_order_relaxed); }
    float getAutoCleanPoints() const noexcept { return autoCleanPoints.load (std::memory_order_relaxed); }
    float getAutoAirPoints() const noexcept { return autoAirPoints.load (std::memory_order_relaxed); }
    float getAutoLoudPoints() const noexcept { return autoLoudPoints.load (std::memory_order_relaxed); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Fixed radio/streaming ceiling: -1.0 dBFS.
    static constexpr float ceilingDb = -1.0f;

private:
    //==========================================================================
    using Filter = juce::dsp::IIR::Filter<float>;
    using FilterChain = juce::dsp::ProcessorDuplicator<Filter, juce::dsp::IIR::Coefficients<float>>;

    void updateCleanFilters (float clean01);
    void updateAirFilters (float air01);

    // Listen mode: run the analysis filters over a copy of the (trimmed)
    // input, update the smoothed band/loudness estimates and, when Listen
    // is on, the automatic correction amounts.
    void runAnalysis (const juce::AudioBuffer<float>& buffer, int numChannels);

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float>* inputGainParam = nullptr;
    std::atomic<float>* cleanParam = nullptr;
    std::atomic<float>* punchParam = nullptr;
    std::atomic<float>* airParam = nullptr;
    std::atomic<float>* loudParam = nullptr;
    std::atomic<float>* outputGainParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;
    std::atomic<float>* listenParam = nullptr;

    // DSP stages
    FilterChain highPassFilter;    // 35 Hz clean-up
    FilterChain mudFilter;         // ~300 Hz peaking cut, depth from Clean
    FilterChain presenceFilter;    // ~4 kHz peaking boost, from Air
    FilterChain airShelfFilter;    // ~10 kHz high shelf, from Air
    juce::dsp::Compressor<float> glueCompressor;

    // Per-sample maximizer state (stereo-linked)
    float limiterEnvelope = 0.0f;
    float limiterGain = 1.0f;

    // Smoothed gain values (linear)
    juce::SmoothedValue<float> inputGainSmooth;
    juce::SmoothedValue<float> outputGainSmooth;
    juce::SmoothedValue<float> loudDriveSmooth;   // makeup gain into limiter
    float saturationDriveSmooth = 1.0f;           // one-pole smoothed tanh drive

    // Cached knob values so filter coefficients only update on change
    float lastClean = -1.0f;
    float lastAir = -1.0f;

    // Meters
    std::atomic<float> inputLevel[2] { 0.0f, 0.0f };
    std::atomic<float> outputLevel[2] { 0.0f, 0.0f };
    std::atomic<float> gainReductionDb { 0.0f };
    float gainReductionSmooth = 0.0f;

    //==========================================================================
    // Listen (Auto) analysis: a small filter bank run over a copy of the
    // input. Band energies are tracked as mean-square estimates smoothed
    // over a few seconds, then compared against a fixed radio-balance
    // reference to derive gentle, bounded corrections.
    enum class Band { sub = 0, mud, mids, presence, air, numBands };
    static constexpr int numAnalysisBands = (int) Band::numBands;

    FilterChain analysisFilters[numAnalysisBands]; // low/band/high-pass per band
    juce::AudioBuffer<float> analysisBuffer;       // scratch copy of the input
    double bandMeanSquare[numAnalysisBands] {};    // slow-smoothed energies
    double overallMeanSquare = 0.0;                // slow-smoothed full-band energy
    double momentaryMeanSquare = 0.0;              // faster loudness estimate
    float peakEnvelope = 0.0f;                     // for crest estimation

    // Automatic corrections (in knob points, 0-100 scale), heavily smoothed.
    // They are added on top of the user's knob settings while Listen is on
    // and decay back to zero when it is switched off.
    float cleanCorrection = 0.0f;
    float airCorrection = 0.0f;
    float loudCorrection = 0.0f;

    // Readouts published for the editor.
    std::atomic<bool> listenState { false };
    std::atomic<float> detectedLoudnessDb { -70.0f };
    std::atomic<float> mudRelativeDb { 0.0f };
    std::atomic<float> presenceRelativeDb { 0.0f };
    std::atomic<float> airRelativeDb { 0.0f };
    std::atomic<float> autoCleanPoints { 0.0f };
    std::atomic<float> autoAirPoints { 0.0f };
    std::atomic<float> autoLoudPoints { 0.0f };

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RadioReadyProcessor)
};
