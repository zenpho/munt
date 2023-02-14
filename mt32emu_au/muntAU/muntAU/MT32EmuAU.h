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
#include "mt32emu/mt32emu.h"

#pragma once

class MT32Synth : public MusicDeviceBase {
    private:
        AudioConverterRef audioConverterRef;
        CAStreamBasicDescription destFormat;
  
        const MT32Emu::ROMImage *romImage;
        const MT32Emu::ROMImage *pcmRomImage;
  
        // workaround Ableton Live limitations
        // use CoreMidi port - not (Ableton Live) host handlers
        MIDIClientRef m_client = 0;
        MIDIEndpointRef m_endpointOut = 0;
        MIDIEndpointRef m_endpointIn = 0;

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
  
        // save and restore state of the mt32emu "temporary" timbres
        virtual OSStatus SaveState(CFPropertyListRef* outData);
        virtual OSStatus RestoreState(CFPropertyListRef inData);
        void RestoreStateAsSysex( CFDataRef data );
  
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
  
        virtual OSStatus Render(AudioUnitRenderActionFlags &ioActionFlags, const AudioTimeStamp &inTimeStamp, UInt32 inNumberFrames);
  
        MT32Emu::Synth *synth;
        void *lastBufferData;
};
