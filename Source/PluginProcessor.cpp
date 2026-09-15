/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SlewClipAudioProcessor::SlewClipAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), apvts (*this, nullptr, "Parameters", createParameters())
#endif
{
}

SlewClipAudioProcessor::~SlewClipAudioProcessor()
{
}

//==============================================================================
const juce::String SlewClipAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SlewClipAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SlewClipAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SlewClipAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SlewClipAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SlewClipAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SlewClipAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SlewClipAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SlewClipAudioProcessor::getProgramName (int index)
{
    return {};
}

void SlewClipAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void SlewClipAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    oversamplers.clear();
    for (size_t factor = 1; factor <= 4; ++factor)
    {
        auto os = std::make_unique<juce::dsp::Oversampling<float>>(
            2,                                                          
            factor,                                                   
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, 
            true                                                        
        );
        
        os->initProcessing(static_cast<size_t>(samplesPerBlock));
        os->reset();
        
        oversamplers.push_back(std::move(os));
    }

    size_t osIndex = static_cast<size_t> (apvts.getRawParameterValue ("OS")->load());
    prevOsIndex = osIndex;
    if (osIndex > 0)
    {
        const float latency = oversamplers[osIndex - 1]->getLatencyInSamples();
        setLatencySamples (juce::roundToInt (latency));
    }
    else
    {
        setLatencySamples (0);
    }       

    float initialThresh = apvts.getRawParameterValue("THRESH")->load();
    dynamicThresholds.assign(getTotalNumInputChannels(), initialThresh);
}

void SlewClipAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
 
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SlewClipAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void SlewClipAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
   
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    size_t osIndex = static_cast<size_t> (apvts.getRawParameterValue ("OS")->load());
    

    float osMultiplier = static_cast<float>(1 << osIndex); 
    float currentSampleRate = static_cast<float>(getSampleRate()) * osMultiplier;
    float driveDb = apvts.getRawParameterValue("DRIVE")->load();
    float driveGain = juce::Decibels::decibelsToGain(driveDb);
    float bias = apvts.getRawParameterValue("BIAS")->load();

    float baseThresh = apvts.getRawParameterValue("THRESH")->load();
    float slopePerSec = apvts.getRawParameterValue("SLEW")->load(); 

    float deltaPerSample = slopePerSec / currentSampleRate;   

    juce::dsp::AudioBlock<float> mainBlock(buffer);
    juce::dsp::AudioBlock<float> blockToProcess = mainBlock; 

    if (prevOsIndex != osIndex)
    {
        prevOsIndex = osIndex;
        if (osIndex > 0)
        {
            const float latency = oversamplers[osIndex - 1]->getLatencyInSamples();
            setLatencySamples (juce::roundToInt (latency));
        }
        else
        {
            setLatencySamples (0);
        }           
    }
 
    if (osIndex > 0)
    {
        blockToProcess = oversamplers[osIndex - 1]->processSamplesUp(mainBlock);
    }

    for (size_t ch = 0; ch < blockToProcess.getNumChannels(); ++ch)
    {
        auto* data = blockToProcess.getChannelPointer(ch);

        for (size_t i = 0; i < blockToProcess.getNumSamples(); ++i)
        {
            float x = data[i] * driveGain + bias;

            float absX = std::abs(x);

            if (absX > dynamicThresholds[ch]) 
            {
                float sign = (x > 0.0f) ? 1.0f : -1.0f;
                data[i] = dynamicThresholds[ch] * sign;
                
                dynamicThresholds[ch] += deltaPerSample;
            } 
            else 
            {
                data[i] = x;
                
                if (absX < baseThresh)
                {
                    dynamicThresholds[ch] = baseThresh;
                }
                else
                {
                    dynamicThresholds[ch] = absX;
                }
                
            }           
        }       
    }

    if (osIndex > 0)
    {
        oversamplers[osIndex - 1]->processSamplesDown(mainBlock);
    }    
}

//==============================================================================
bool SlewClipAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SlewClipAudioProcessor::createEditor()
{
    //return new SlewClipAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor (*this); 
}

//==============================================================================
void SlewClipAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);    
}

void SlewClipAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SlewClipAudioProcessor();
}


juce::AudioProcessorValueTreeState::ParameterLayout SlewClipAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        
    juce::StringArray osChoices { "1x (Off)", "2x", "4x", "8x", "16x" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>("OS", "Oversampling", osChoices, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("DRIVE", "Drive (dB)", 0.0f, 40.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("BIAS", "Bias", -2.5f, 2.5f, 0.0f));
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>("THRESH", "Threshold", 0.01f, 1.0f, 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "SLEW", 1 },
        "Slew Rate",
        juce::NormalisableRange<float> (0.0f, 1000.0f, 0.1f),
        10.0f
    ));

    return { params.begin(), params.end() };
}