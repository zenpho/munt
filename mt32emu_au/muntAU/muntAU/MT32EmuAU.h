//
//  MT32EmuAU.h
//  muntAU
//
//  Updated 2023 by Zenpho
//  added CoreMidi virtual ports for timbre dump data
//  to work around Ableton Live limitations when hosting
//  multitimbral plugins and using MIDI Sysex messages
//
//  added multiple audio outputs for individual parts
//
//  Orginal code by Ivan Safrin on 1/17/14.
//  Copyright (c) 2014 Ivan Safrin. All rights reserved.
//

#include "MusicDeviceBase.h"
#include "mt32emu.h"

#pragma once

// parallel MT32Emu synthesis engines are hosted here
#define NUM_PARALLEL_SYNTHS 5

#define TIMBRE1_DICTIONARY_KEY   CFSTR("timbre1.syx")
#define TIMBRE2_DICTIONARY_KEY   CFSTR("timbre2.syx")
#define TIMBRE3_DICTIONARY_KEY   CFSTR("timbre3.syx")
#define TIMBRE4_DICTIONARY_KEY   CFSTR("timbre4.syx")
#define TIMBRE5_DICTIONARY_KEY   CFSTR("timbre5.syx")

class MT32Synth : public MusicDeviceBase {
    public:
        // parallel synthesis engines and audio buffers
        MT32Emu::Synth *synths[NUM_PARALLEL_SYNTHS];
        MT32Emu::Bit16s* curAudioData[NUM_PARALLEL_SYNTHS];
  
        // TODO - master mix of all synth audio output
        MT32Emu::Bit16s* mixAudioData;
  
        // RenderAudioBus() needs to tell EncoderDataProc() which bus is rendering
        // synths[0] renders audio bus 0 (stereo pair)
        // synths[1] renders audio bus 1 etc...
        int curRenderAudioBus = -1;

    private:
        // workaround Ableton Live limitations
        // use CoreMidi port - not (Ableton Live) host handlers
        MIDIClientRef m_client = 0;
        MIDIEndpointRef m_endpointOut = 0;
        MIDIEndpointRef m_endpointIn = 0;
  
        // convert MT32Emu 16bit unsigned int 32khz audio to whatever required
        // parallel AudioConverters are required for the eight stereo pair audio outputs
        AudioConverterRef audioConverters[NUM_PARALLEL_SYNTHS];
        CAStreamBasicDescription destFormat;
  
        // one pair of ROM images is allocated, loaded, and freed at startup
        const MT32Emu::ROMImage *romImage;
        const MT32Emu::ROMImage *pcmRomImage;

    public:
        MT32Synth(ComponentInstance inComponentInstance);
        virtual ~MT32Synth();
  
        virtual OSStatus Initialize();
        virtual void Cleanup();
        virtual OSStatus Version();

        // CoreMidi virtual port configuration, receive, and transmit
        void CoreMidiPortSetup();
        static void CoreMidiRX (
            const MIDIPacketList *packetList,
            void* readProcRefCon, void* srcConnRefCon);
        void CoreMidiTimbreTX(Byte addrH, Byte addrL);
  
        // getSysexTimbre() writes timbre sysex dump with 0xF0 and 0xF7 to buffer
        // - we trust buffer allocated correctly
        // - returns number of bytes
        uint16_t getSysexTimbre(Byte addrH, Byte addrL, Byte* buffer);
  
        // save and restore state of the MT32Emu "temporary" timbres
        virtual OSStatus SaveState(CFPropertyListRef* outData);
        virtual OSStatus RestoreState(CFPropertyListRef inData);
  
        void RestoreAllStateAsSysex( CFDataRef data ); // all parallel instances
        void RestoreOneStateAsSysex( MT32Emu::Synth *synth, CFDataRef data ); // specific instance
  
        virtual bool StreamFormatWritable(  AudioUnitScope scope, AudioUnitElement element);
  
        // workaround Ableton Live limitations
        // use CoreMidi port - not (Ableton Live) host MIDI
        virtual OSStatus HandleNoteOn(UInt8 inChannel, UInt8 inNoteNumber, UInt8 inVelocity, UInt32 inStartFrame)
        { return noErr; }
  
        virtual OSStatus HandleNoteOff(  UInt8 inChannel, UInt8 inNoteNumber, UInt8 inVelocity, UInt32 inStartFrame)
        { return noErr; }
  
        virtual OSStatus HandlePitchWheel(UInt8 inChannel, UInt8 inPitch1, UInt8 inPitch2, UInt32 inStartFrame)
        { return noErr; }
  
        virtual OSStatus HandleProgramChange(  UInt8  inChannel, UInt8   inValue)
        { return noErr; }
  
        virtual OSStatus HandleSysEx(const UInt8 *  inData, UInt32 inLength )
        { return noErr; }
  
        virtual OSStatus GetParameterValueStrings(AudioUnitScope inScope, AudioUnitParameterID inParameterID, CFArrayRef *outStrings);
  
        virtual OSStatus SetParameter(AudioUnitParameterID inID, AudioUnitScope           inScope, AudioUnitElement inElement, AudioUnitParameterValue      inValue, UInt32  inBufferOffsetInFrames);
  
        virtual UInt32 SupportedNumChannels (const AUChannelInfo** outInfo);
  
        virtual OSStatus GetParameterInfo(AudioUnitScope inScope,
                              AudioUnitParameterID inParameterID,
                              AudioUnitParameterInfo & outParameterInfo);
  
        virtual OSStatus RenderBus(AudioUnitRenderActionFlags &ioActionFlags, const AudioTimeStamp &inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames);
};
