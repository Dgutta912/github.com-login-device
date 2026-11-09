#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
RadioReadyProcessor::RadioReadyProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    inputGainParam = apvts.getRawParameterValue ("inputGain");
    cleanParam = apvts.getRawParameterValue ("clean");
    punchParam = apvts.getRawParameterValue ("punch");
    airParam = apvts.getRawParameterValue ("air");
    loudParam = apvts.getRawParameterValue ("loud");
    outputGainParam = apvts.getRawParameterValue ("outputGain");
    bypassParam = apvts.getRawParameterValue ("bypass");
    listenParam = apvts.getRawParameterValue ("listen");
}

juce::AudioProcessorValueTreeState::ParameterLayout
RadioReadyProcessor::createParameterLayout()
{
    using Range = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto percentRange = [] { return Range (0.0f, 100.0f, 0.1f); };
    auto dbRange = [] { return Range (-24.0f, 24.0f, 0.1f); };

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("inputGain", 1), "Input", dbRange(), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("clean", 1), "Clean", percentRange(), 30.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("punch", 1), "Punch", percentRange(), 35.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("air", 1), "Air", percentRange(), 30.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("loud", 1), "Loud", percentRange(), 40.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("outputGain", 1), "Output", dbRange(), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID ("bypass", 1), "Bypass", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID ("listen", 1), "Listen", false));

    return { params.begin(), params.end() };
}

//==============================================================================
void RadioReadyProcessor::updateCleanFilters (float clean01)
{
    // Always-on subsonic clean-up for a master bus.
    *highPassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (
        currentSampleRate, 35.0, 0.707);

    // Mud cut: 0 dB at Clean = 0, down to -4 dB at Clean = 100.
    const auto mudDb = -4.0f * clean01;
    *mudFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        currentSampleRate, 300.0, 0.9, juce::Decibels::decibelsToGain (mudDb));
}

void RadioReadyProcessor::updateAirFilters (float air01)
{
    // Presence: up to +4 dB at 4 kHz.
    *presenceFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        currentSampleRate, 4000.0, 0.8,
        juce::Decibels::decibelsToGain (4.0f * air01));

    // Air: up to +6 dB high shelf at 10 kHz.
    *airShelfFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        currentSampleRate, 10000.0, 0.7,
        juce::Decibels::decibelsToGain (6.0f * air01));
}

//==============================================================================
// Listen (Auto) analysis.
//
// A five-band filter bank (sub / mud / mids / presence / air) runs over a
// copy of the input every block. Band energies and the overall level are
// smoothed over a few seconds, then compared with a fixed radio-balance
// reference. When Listen is on, the differences become gentle corrections
// (in knob points) on top of the user's Clean / Air / Loud settings:
//
//     mud band hot          -> more Clean cut
//     presence / air weak   -> more Air boost
//     mix quiet vs target   -> more Loud drive (less if already hot, and
//                              backed off further if the crest factor says
//                              the mix is already squashed)
//     hot peaks             -> handled sample-by-sample by the limiter
//
// All corrections are bounded, heavily smoothed (about 2 s), and have a
// deadband, so the auto logic itself can never pump.
void RadioReadyProcessor::runAnalysis (const juce::AudioBuffer<float>& buffer,
                                       int numChannels)
{
    const auto numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || analysisBuffer.getNumSamples() < numSamples)
        return;

    const double blockSeconds = (double) numSamples / currentSampleRate;
    const double slowAlpha = 1.0 - std::exp (-blockSeconds / 2.5);  // band energies
    const double fastAlpha = 1.0 - std::exp (-blockSeconds / 0.4);  // loudness feel

    auto meanSquareOf = [&] (const juce::AudioBuffer<float>& b)
    {
        double sum = 0.0;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto* data = b.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
                sum += (double) data[i] * (double) data[i];
        }
        return sum / (double) (numSamples * numChannels);
    };

    // Overall energy + peak (from the untouched input copy).
    const double overallBlock = meanSquareOf (buffer);
    overallMeanSquare += slowAlpha * (overallBlock - overallMeanSquare);
    momentaryMeanSquare += fastAlpha * (overallBlock - momentaryMeanSquare);

    float blockPeak = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
        blockPeak = juce::jmax (blockPeak, buffer.getMagnitude (ch, 0, numSamples));

    const float peakDecay = (float) std::exp (-blockSeconds / 1.0);
    peakEnvelope = juce::jmax (blockPeak, peakEnvelope * peakDecay);

    // Band energies through the filter bank (each pass needs a fresh copy,
    // because the filters process in place).
    for (int b = 0; b < numAnalysisBands; ++b)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            analysisBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        juce::dsp::AudioBlock<float> analysisBlock (analysisBuffer);
        auto subBlock = analysisBlock.getSubBlock (0, (size_t) numSamples)
                            .getSubsetChannelBlock (0, (size_t) numChannels);
        juce::dsp::ProcessContextReplacing<float> ctx (subBlock);
        analysisFilters[b].process (ctx);

        const double bandBlock = meanSquareOf (analysisBuffer);
        bandMeanSquare[b] += slowAlpha * (bandBlock - bandMeanSquare[b]);
    }

    //--------------------------------------------------------------------------
    auto rmsDb = [] (double meanSquare)
    {
        return juce::Decibels::gainToDecibels (
            (float) std::sqrt (std::max (meanSquare, 1.0e-10)), -70.0f);
    };

    const float overallDbSlow = rmsDb (overallMeanSquare);
    const float overallDbFast = rmsDb (momentaryMeanSquare);
    const float mudRel = rmsDb (bandMeanSquare[(int) Band::mud]) - overallDbSlow;
    const float presRel = rmsDb (bandMeanSquare[(int) Band::presence]) - overallDbSlow;
    const float airRel = rmsDb (bandMeanSquare[(int) Band::air]) - overallDbSlow;

    //--------------------------------------------------------------------------
    // Map analysis -> correction targets (knob points). Targets are zero
    // whenever Listen is off or the input is effectively silent, so the
    // corrections always relax back to the pure manual sound.
    const bool listening = listenParam->load (std::memory_order_relaxed) > 0.5f;
    listenState.store (listening, std::memory_order_relaxed);

    float targetClean = 0.0f, targetAir = 0.0f, targetLoud = 0.0f;

    if (listening && overallDbSlow > -55.0f)
    {
        // Reference balance (band RMS relative to overall RMS, dB).
        constexpr float mudTargetRel = -13.0f;
        constexpr float presenceTargetRel = -11.0f;
        constexpr float airTargetRel = -15.0f;
        constexpr float deadband = 1.5f;

        const float mudExcess = mudRel - mudTargetRel;
        if (mudExcess > deadband)
            targetClean = juce::jmin (35.0f, (mudExcess - deadband) * 5.0f);

        const float deficit = 0.5f * juce::jmax (0.0f, presenceTargetRel - presRel)
                            + 0.5f * juce::jmax (0.0f, airTargetRel - airRel);
        if (deficit > deadband)
            targetAir = juce::jmin (35.0f, (deficit - deadband) * 5.0f);

        // Loudness: aim the slow RMS near -13 dBFS.
        const float loudDeficit = -13.0f - overallDbSlow;
        if (loudDeficit > 1.0f)
            targetLoud = juce::jmin (30.0f, loudDeficit * 3.0f);
        else if (loudDeficit < -4.0f)
            targetLoud = juce::jmax (-15.0f, (loudDeficit + 4.0f) * 2.5f);

        // Crest safety: an already-squashed mix gets less extra push.
        const float crestDb = juce::Decibels::gainToDecibels (juce::jmax (peakEnvelope, 1.0e-5f))
                            - overallDbFast;
        if (crestDb < 6.0f && targetLoud > 0.0f)
            targetLoud *= 0.5f;
    }

    // Heavy smoothing (~2 s) so corrections drift, never jump.
    const double corrAlpha = 1.0 - std::exp (-blockSeconds / 2.0);
    cleanCorrection += (float) corrAlpha * (targetClean - cleanCorrection);
    airCorrection += (float) corrAlpha * (targetAir - airCorrection);
    loudCorrection += (float) corrAlpha * (targetLoud - loudCorrection);

    // Publish readouts for the editor.
    detectedLoudnessDb.store (overallDbFast, std::memory_order_relaxed);
    mudRelativeDb.store (mudRel, std::memory_order_relaxed);
    presenceRelativeDb.store (presRel, std::memory_order_relaxed);
    airRelativeDb.store (airRel, std::memory_order_relaxed);
    autoCleanPoints.store (cleanCorrection, std::memory_order_relaxed);
    autoAirPoints.store (airCorrection, std::memory_order_relaxed);
    autoLoudPoints.store (loudCorrection, std::memory_order_relaxed);
}

//==============================================================================
void RadioReadyProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32> (getTotalNumOutputChannels());

    highPassFilter.prepare (spec);
    mudFilter.prepare (spec);
    presenceFilter.prepare (spec);
    airShelfFilter.prepare (spec);
    glueCompressor.prepare (spec);

    highPassFilter.reset();
    mudFilter.reset();
    presenceFilter.reset();
    airShelfFilter.reset();
    glueCompressor.reset();

    // Listen analysis filter bank.
    *analysisFilters[(int) Band::sub].state =
        *juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 120.0, 0.707);
    *analysisFilters[(int) Band::mud].state =
        *juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, 300.0, 1.1);
    *analysisFilters[(int) Band::mids].state =
        *juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, 1000.0, 0.5);
    *analysisFilters[(int) Band::presence].state =
        *juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, 4000.0, 0.8);
    *analysisFilters[(int) Band::air].state =
        *juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 9000.0, 0.707);

    for (auto& filter : analysisFilters)
    {
        filter.prepare (spec);
        filter.reset();
    }

    analysisBuffer.setSize (getTotalNumOutputChannels(), samplesPerBlock);
    analysisBuffer.clear();

    for (auto& ms : bandMeanSquare)
        ms = 0.0;
    overallMeanSquare = 0.0;
    momentaryMeanSquare = 0.0;
    peakEnvelope = 0.0f;
    cleanCorrection = 0.0f;
    airCorrection = 0.0f;
    loudCorrection = 0.0f;

    inputGainSmooth.reset (sampleRate, 0.02);
    outputGainSmooth.reset (sampleRate, 0.02);
    loudDriveSmooth.reset (sampleRate, 0.05);
    inputGainSmooth.setCurrentAndTargetValue (1.0f);
    outputGainSmooth.setCurrentAndTargetValue (1.0f);
    loudDriveSmooth.setCurrentAndTargetValue (1.0f);
    saturationDriveSmooth = 1.0f;

    limiterEnvelope = 0.0f;
    limiterGain = 1.0f;
    gainReductionSmooth = 0.0f;

    lastClean = -1.0f; // force coefficient update on first block
    lastAir = -1.0f;
}

void RadioReadyProcessor::releaseResources() {}

bool RadioReadyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Stereo mix / master bus only, by design.
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================
void RadioReadyProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numChannels = juce::jmin (2, buffer.getNumChannels());
    const auto numSamples = buffer.getNumSamples();
    if (numChannels <= 0 || numSamples <= 0)
        return;

    const bool bypassed = bypassParam->load (std::memory_order_relaxed) > 0.5f;

    if (bypassed)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto peak = buffer.getMagnitude (ch, 0, numSamples);
            inputLevel[ch].store (peak, std::memory_order_relaxed);
            outputLevel[ch].store (peak, std::memory_order_relaxed);
        }
        gainReductionDb.store (0.0f, std::memory_order_relaxed);
        gainReductionSmooth = 0.0f;
        return;
    }

    // Effective knob values: user's settings plus Listen corrections
    // (corrections are zero when Listen is off, so manual behaviour is
    // exactly as before).
    const auto clean01 = juce::jlimit (0.0f, 1.0f,
        (cleanParam->load (std::memory_order_relaxed) + cleanCorrection) / 100.0f);
    const auto punch01 = juce::jlimit (0.0f, 1.0f, punchParam->load (std::memory_order_relaxed) / 100.0f);
    const auto air01 = juce::jlimit (0.0f, 1.0f,
        (airParam->load (std::memory_order_relaxed) + airCorrection) / 100.0f);
    const auto loud01 = juce::jlimit (0.0f, 1.0f,
        (loudParam->load (std::memory_order_relaxed) + loudCorrection) / 100.0f);

    //--------------------------------------------------------------------------
    // 1. Input trim (smoothed ramp)
    {
        const auto target = juce::Decibels::decibelsToGain (
            inputGainParam->load (std::memory_order_relaxed));
        const auto start = inputGainSmooth.getCurrentValue();
        inputGainSmooth.setTargetValue (target);
        const auto end = inputGainSmooth.skip (numSamples);
        buffer.applyGainRamp (0, numSamples, start, end);
    }

    for (int ch = 0; ch < numChannels; ++ch)
        inputLevel[ch].store (buffer.getMagnitude (ch, 0, numSamples),
                              std::memory_order_relaxed);

    // Listen analysis runs on the trimmed input, before the chain, and
    // refreshes the correction amounts used by the stages below.
    runAnalysis (buffer, numChannels);

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    //--------------------------------------------------------------------------
    // 2. Clean stage: 35 Hz high-pass + mud cut
    if (std::abs (clean01 - lastClean) > 0.001f)
    {
        updateCleanFilters (clean01);
        lastClean = clean01;
    }
    highPassFilter.process (context);
    mudFilter.process (context);

    //--------------------------------------------------------------------------
    // 3. Glue compressor: Punch blends threshold (-4..-18 dB) and
    //    ratio (2:1 .. 4:1). Slow attack, medium release.
    glueCompressor.setThreshold (-4.0f - 14.0f * punch01);
    glueCompressor.setRatio (2.0f + 2.0f * punch01);
    glueCompressor.setAttack (30.0f);
    glueCompressor.setRelease (150.0f);

    float prePeak = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
        prePeak = juce::jmax (prePeak, buffer.getMagnitude (ch, 0, numSamples));

    glueCompressor.process (context);

    float postPeak = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
        postPeak = juce::jmax (postPeak, buffer.getMagnitude (ch, 0, numSamples));

    float compGrDb = 0.0f;
    if (prePeak > 1.0e-5f && postPeak > 1.0e-5f && postPeak < prePeak)
        compGrDb = juce::jlimit (0.0f, 24.0f,
                                 juce::Decibels::gainToDecibels (prePeak / postPeak));

    //--------------------------------------------------------------------------
    // 4. Presence + Air EQ
    if (std::abs (air01 - lastAir) > 0.001f)
    {
        updateAirFilters (air01);
        lastAir = air01;
    }
    presenceFilter.process (context);
    airShelfFilter.process (context);

    //--------------------------------------------------------------------------
    // 5 + 6. Saturation (drive tied to Loud) -> maximizer (makeup gain from
    // Loud, stereo-linked peak limiter, -1.0 dBFS ceiling) -> output trim
    // -> final ceiling safety clamp.
    const float ceiling = juce::Decibels::decibelsToGain (ceilingDb);

    // Loud at 0 = no makeup; Loud at 100 = +18 dB push (gentle curve).
    const auto makeupTarget = juce::Decibels::decibelsToGain (18.0f * loud01 * loud01);
    loudDriveSmooth.setTargetValue (makeupTarget);

    // Saturation drive: 1.0 (subtle) rising to ~2.2 as Loud increases.
    const float satDriveTarget = 1.0f + 1.2f * loud01;
    const float satSmoothCoef = 1.0f - std::exp (-1.0f / (0.03f * (float) currentSampleRate));

    // Limiter envelope / gain-smoothing coefficients.
    const float sr = (float) currentSampleRate;
    const float envAttack = std::exp (-1.0f / (0.0001f * sr));   // ~0.1 ms
    const float envRelease = std::exp (-1.0f / (0.100f * sr));   // ~100 ms
    const float gainAttack = std::exp (-1.0f / (0.0002f * sr));  // ~0.2 ms
    const float gainRelease = std::exp (-1.0f / (0.060f * sr));  // ~60 ms

    auto* channelData0 = buffer.getWritePointer (0);
    auto* channelData1 = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    float limiterGrDb = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        // Saturation (per channel, same drive) with 1/drive level compensation
        saturationDriveSmooth += satSmoothCoef * (satDriveTarget - saturationDriveSmooth);
        const float drive = saturationDriveSmooth;

        float l = std::tanh (drive * channelData0[i]) / drive;
        float r = channelData1 != nullptr ? std::tanh (drive * channelData1[i]) / drive : l;

        // Maximizer makeup gain (smoothed)
        const float makeup = loudDriveSmooth.getNextValue();
        l *= makeup;
        r *= makeup;

        // Stereo-linked peak envelope
        const float peak = juce::jmax (std::abs (l), std::abs (r));
        if (peak > limiterEnvelope)
            limiterEnvelope = peak + envAttack * (limiterEnvelope - peak);
        else
            limiterEnvelope = peak + envRelease * (limiterEnvelope - peak);

        const float targetGain = limiterEnvelope > ceiling ? ceiling / limiterEnvelope : 1.0f;
        if (targetGain < limiterGain)
            limiterGain = targetGain + gainAttack * (limiterGain - targetGain);
        else
            limiterGain = targetGain + gainRelease * (limiterGain - targetGain);

        l *= limiterGain;
        r *= limiterGain;

        if (limiterGain < 1.0f)
            limiterGrDb = juce::jmax (limiterGrDb,
                                      juce::Decibels::gainToDecibels (1.0f / limiterGain));

        channelData0[i] = l;
        if (channelData1 != nullptr)
            channelData1[i] = r;
    }

    //--------------------------------------------------------------------------
    // Output trim (smoothed ramp), then a final hard safety clamp so the
    // -1.0 dBFS ceiling holds even if the output trim is pushed up.
    {
        const auto target = juce::Decibels::decibelsToGain (
            outputGainParam->load (std::memory_order_relaxed));
        const auto start = outputGainSmooth.getCurrentValue();
        outputGainSmooth.setTargetValue (target);
        const auto end = outputGainSmooth.skip (numSamples);
        buffer.applyGainRamp (0, numSamples, start, end);
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = juce::jlimit (-ceiling, ceiling, data[i]);

        outputLevel[ch].store (buffer.getMagnitude (ch, 0, numSamples),
                               std::memory_order_relaxed);
    }

    //--------------------------------------------------------------------------
    // Combined gain-reduction meter (compressor + limiter), with fall-off.
    const float totalGr = compGrDb + limiterGrDb;
    gainReductionSmooth = totalGr > gainReductionSmooth
                              ? totalGr
                              : gainReductionSmooth * 0.9f;
    gainReductionDb.store (gainReductionSmooth, std::memory_order_relaxed);
}

//==============================================================================
float RadioReadyProcessor::getInputLevel (int channel) const noexcept
{
    return inputLevel[juce::jlimit (0, 1, channel)].load (std::memory_order_relaxed);
}

float RadioReadyProcessor::getOutputLevel (int channel) const noexcept
{
    return outputLevel[juce::jlimit (0, 1, channel)].load (std::memory_order_relaxed);
}

//==============================================================================
juce::AudioProcessorEditor* RadioReadyProcessor::createEditor()
{
    return new RadioReadyEditor (*this);
}

void RadioReadyProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void RadioReadyProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// Standard JUCE plugin entry point.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RadioReadyProcessor();
}
