//
//  MT32EmuAU.cpp
//  muntAU
//
//  Updated 2023 jan 05 by Zenpho
//  added CoreMidi virtual ports for timbre dump data
//  to work around Ableton Live limitations when hosting
//  multitimbral plugins and using MIDI Sysex messages
//
//  Orginal code by Ivan Safrin on 1/17/14.
//  Copyright (c) 2014 Ivan Safrin. All rights reserved.
//

#include "MT32EmuAU.h"
#include <AudioToolbox/AudioToolbox.h>

AUDIOCOMPONENT_ENTRY(AUMusicDeviceFactory, MT32Synth)

#define TIMBRE1_DICTIONARY_KEY   CFSTR("timbre1.syx")
#define TIMBRE2_DICTIONARY_KEY   CFSTR("timbre2.syx")
#define TIMBRE3_DICTIONARY_KEY   CFSTR("timbre3.syx")
#define TIMBRE4_DICTIONARY_KEY   CFSTR("timbre4.syx")
#define TIMBRE5_DICTIONARY_KEY   CFSTR("timbre5.syx")

MT32Synth::MT32Synth(ComponentInstance inComponentInstance)
: MusicDeviceBase(inComponentInstance, 0, 2)
{
    CreateElements(); // AUBase::CreateElements()
  
    synth = NULL;
    lastBufferData = NULL;
  
    lastBufferPartnum = 0;
}

MT32Synth::~MT32Synth()
{
  // tear down CoreMidi ports
  MIDIEndpointDispose(m_endpointOut);
  MIDIEndpointDispose(m_endpointIn);
  MIDIClientDispose(m_client);
}

OSStatus MT32Synth::Version()
{
    return 0xFFFFFFFF;
}

OSStatus MT32Synth::Initialize()
{
    destFormat = GetStreamFormat(kAudioUnitScope_Output, 0);
  
    AudioStreamBasicDescription sourceDescription;
    sourceDescription.mSampleRate = 32000;
    sourceDescription.mBytesPerFrame = 4;
    sourceDescription.mBitsPerChannel = 16;
    sourceDescription.mFormatID = kAudioFormatLinearPCM;
    sourceDescription.mBytesPerPacket = 4;
    sourceDescription.mChannelsPerFrame = 2;
    sourceDescription.mFormatFlags = kAudioFormatFlagIsSignedInteger;
    sourceDescription.mFramesPerPacket = 1;
    sourceDescription.mReserved = 0;
  
    AudioConverterNew(&sourceDescription, &destFormat, &audioConverterRef);
  
    MT32Emu::FileStream controlROMFile;
    MT32Emu::FileStream pcmROMFile;
  
    if(!controlROMFile.open("/Library/MT32/MT32_CONTROL.ROM")) {
        printf("Error opening MT32_CONTROL.ROM\n");
    }
  
    if(!pcmROMFile.open("/Library/MT32/MT32_PCM.ROM")) {
        printf("Error opening MT32_PCM.ROM\n");
    }
  
    romImage = MT32Emu::ROMImage::makeROMImage(&controlROMFile);
    pcmRomImage = MT32Emu::ROMImage::makeROMImage(&pcmROMFile);
  
    synth = new MT32Emu::Synth();
    synth->open(*romImage, *pcmRomImage);
  
    MT32Emu::ROMImage::freeROMImage(romImage);
    MT32Emu::ROMImage::freeROMImage(pcmRomImage);
  
    MusicDeviceBase::Initialize();
  
    synth->setOutputGain(2.0);
    synth->setReverbOutputGain(0.0);
    synth->setReverbEnabled(false);
  
    fprintf(stdout, "MUNT:MT32 configured for %d partials\n", synth->getPartialCount());
  
    CoreMidiPortSetup();
  
    return noErr;
}

void MT32Synth::CoreMidiPortSetup()
{
    //fprintf(stdout, "MIDI:coremidi setup start\n");

    OSStatus status = 0;
  
    if( m_client == 0 ) // missing client?
    if ((status = MIDIClientCreate(CFSTR("MUNTAU"), NULL, NULL, &m_client))) {
        fprintf(stdout, "MIDI:coremidi Error creating client: %d\n", (int)status);
        return;
    }

    // TODO - iterate over existing virtual port names first
    //      - give any port name we create a unique id
    //      - show the unique id as an AudioUnitParameter value for our user
    //      - error handling - autoincrement when the user sets a value that would clash
  
    if( m_endpointIn == 0) // missing input endpoint?
    if ((status = MIDIDestinationCreate(m_client, CFSTR("MT32_input"), CoreMidiRX, (void*)this, &m_endpointIn)))
    {
        fprintf(stdout, "MIDI:coremidi Error creating endpoint: %d\n", (int)status);
        return;
    }
  
    if( m_endpointOut == 0) // missing output endpoint?
    if ((status = MIDISourceCreate(m_client, CFSTR("MT32_output"), &m_endpointOut))) {
        fprintf(stdout, "MIDI:coremidi Error creating endpoint: %d\n", (int)status);
        return;
    }
  
    fprintf(stdout, "MIDI:coremidi setup finished with status %d\n", (int)status);
}

void MT32Synth::CoreMidiRX(const MIDIPacketList *packetList, void* readProcRefCon, void* srcConnRefCon)
{
    enum e_CMSysexState {
        WAIT_FOR_0XF0 = 0,
        WAIT_FOR_0XF7,
        SEND_TO_MT32,
    };
    static e_CMSysexState cmSysexState = WAIT_FOR_0XF0;
    static Byte           cmSysexData[2048];
    static UInt32         cmSysexLen = 0;

    int numPackets = packetList->numPackets;
    MIDIPacket *packet = (MIDIPacket*)packetList->packet;
  
    /* debugging
    fprintf(stdout, "MIDI:rx (numpkts %d)\n", (unsigned int)packetList->numPackets);
    for (int pkt=0; pkt<numPackets; pkt++)
    {
      fprintf(stdout, "MIDI:rx %d (len %d)\n", pkt, packet->length);
      for (int i=0; i<packet->length; i++) {
        fprintf(stdout, "  0x%x ", packet->data[i]);
      }
      fprintf(stdout, "\n");

      packet = MIDIPacketNext(packet);
    }
    */
  
    for (int pkt=0; pkt<numPackets; pkt++)
    {
      if( cmSysexState == WAIT_FOR_0XF0 )
      {
          if( packet->length >= 1 && packet->length <= 4 ) // not sysex and between 1-4 bytes inc status
          {
              unsigned int msg = 0; // build 32bit msg
              if( packet->length >= 1 ) msg |= ( packet->data[0] );
              if( packet->length >= 2 ) msg |= ( packet->data[1] << 8 );
              if( packet->length >= 3 ) msg |= ( packet->data[2] << 16 );
              if( packet->length == 4 ) msg |= ( packet->data[3] << 24 );

              //fprintf(stdout, "msg: 0x%x \n", msg);
              static_cast<MT32Synth*>(readProcRefCon)->synth->playMsg( msg );
              packet = MIDIPacketNext(packet);
              continue;
          }
      
          if( packet->length >= 1 && packet->data[0] == 0xF0 )
          {
              if( (cmSysexLen + packet->length) > 2048 ) return; // abort
            
              //fprintf(stdout, "got %d bytes ", (unsigned int)packet->length);
            
              memcpy( cmSysexData, packet->data, packet->length );
              cmSysexLen = packet->length;
            
              if ( cmSysexData[cmSysexLen-1] == 0xF7 )
                cmSysexState = SEND_TO_MT32;
              else
              {
                cmSysexState = WAIT_FOR_0XF7;
                packet = MIDIPacketNext(packet);
                continue;
              }
          }
      }
      if( cmSysexState == WAIT_FOR_0XF7 )
      {
          if( packet->length >= 1 )
          {
              if( (cmSysexLen + packet->length) > 2048 ) return; // abort
            
              //fprintf(stdout, "more %d bytes ", (unsigned int)packet->length);
          
              memcpy( cmSysexData+cmSysexLen, packet->data, packet->length );
              cmSysexLen += packet->length;
            
              if ( cmSysexData[cmSysexLen-1] == 0xF7 )
                cmSysexState = SEND_TO_MT32;
              else
              {
                cmSysexState = WAIT_FOR_0XF7;
                packet = MIDIPacketNext(packet);
                continue;
              }
          }
      }
      if( cmSysexState == SEND_TO_MT32 )
      {
          /* debugging
          fprintf(stdout, "COMPLETE SYSTEM EXCLUSIVE: ");
          for (int i=0; i<cmSysexLen; i++) {
            fprintf(stdout, "  0x%x ", cmSysexData[i]);
          }
          fprintf(stdout, "\n");
          */
        
          static_cast<MT32Synth*>(readProcRefCon)->synth->playSysex( cmSysexData, cmSysexLen );
          cmSysexState = WAIT_FOR_0XF0;
        
          // F0 41 10 16 11 "lookup memory"
          // see libmt32emu memory offset comment in CoreMidiTimbreResponse()
          if( cmSysexData[0] == 0xF0 &&
            ( cmSysexData[1] == 0x41 ) &&
            ( cmSysexData[2] == 0x10 ) &&
            ( cmSysexData[3] == 0x16 ) &&
            ( cmSysexData[4] == 0x11 ) &&
            ( cmSysexData[5] == 0x04 ) && // timbre base address is 0x040000
            ( cmSysexLen >= 7 ) )
          {
            Byte addrH = cmSysexData[6];
            Byte addrL = cmSysexData[7];
            static_cast<MT32Synth*>(readProcRefCon)->CoreMidiTimbreTX( addrH, addrL );
          }
      }
      
      packet = MIDIPacketNext(packet);
  }// for();
  
}// func()

// getSysexTimbre() writes timbre sysex dump with 0xF0 and 0xF7 to buffer
// - we trust buffer allocated correctly
// - returns number of bytes
uint16_t MT32Synth::getSysexTimbre(Byte addrH, Byte addrL, Byte* buffer)
{
  // libmt32emu memory offsets for timbre parameters
  // T1 (0x04 << 16) | (0x00 << 8) | 0x00;
  // T2 (0x04 << 16) | (0x01 << 8) | 0x76;
  // T3 (0x04 << 16) | (0x03 << 8) | 0x6C;
  // T4 (0x04 << 16) | (0x05 << 8) | 0x62;
  // T5 (0x04 << 16) | (0x07 << 8) | 0x58;

  // ROL MT32 system exclusive header for timbre response
  uint8_t header[5] = { 0xF0, 0x41, 0x10, 0x16, 0x12 };
  memcpy(buffer, header, 5);
  
  // append timbre parameters from libmt32emu memory
  buffer[5] = 0x04; // timbre base address is 0x040000
  buffer[6] = addrH;
  buffer[7] = addrL;
  
  uint32_t addr = MT32EMU_MEMADDR( (0x04 << 16) | (addrH << 8) | addrL );
  uint16_t timbreSize = sizeof(MT32Emu::TimbreParam);
  synth->readMemory(addr, timbreSize, buffer + 8);
  
  // append checksum and sysex end marker 0xF7
  uint8_t checksum = synth->calcSysexChecksum(buffer + 5, timbreSize + 8, 0);
  buffer[timbreSize + 8] = checksum;
  buffer[timbreSize + 9] = 0xF7;
  uint16_t bufferLen = timbreSize + 10;
  
  /* //debugging
  fprintf(stdout, "DATA FOR ADDR: %04x \n", addr);
  for (int i=0; i<bufferLen; i++) {
    fprintf(stdout, " %02x ", buffer[i]);
  }
  fprintf(stdout, "\n");
  */
  
  return bufferLen;
}

void MT32Synth::CoreMidiTimbreTX(Byte addrH, Byte addrL)
{
  // build a MidiPacket for sending to a CoreMidi port
  struct {
    MIDITimeStamp timeStamp = 0;
    UInt16        length = 0;
    Byte          data[512];     // room for timbre bytes plus header
  } tx;
  memset(&tx, 0, sizeof(tx));
  
  tx.length = getSysexTimbre(addrH, addrL, tx.data);
  
  // transmit the timbre as a CoreMidi PacketList
  MIDIPacketList txMPL;
  MIDIPacketListInit(&txMPL);
  MIDIPacketListAdd(&txMPL, 512, txMPL.packet, 0, tx.length, tx.data);
  MIDIReceived(m_endpointOut, &txMPL);
}

UInt32 MT32Synth::SupportedNumChannels (const AUChannelInfo** outInfo) // audio channels
{
    static const AUChannelInfo sChannels[1] = { {0, 2} };
    if (outInfo) *outInfo = sChannels;
    return sizeof (sChannels) / sizeof (AUChannelInfo);
}

bool MT32Synth::StreamFormatWritable(  AudioUnitScope scope, AudioUnitElement element)
{
    return true;
}

static OSStatus EncoderDataProcAll(AudioConverterRef inAudioConverter, UInt32 *ioNumberDataPackets, AudioBufferList *ioData, AudioStreamPacketDescription **outDataPacketDescription, void *inUserData)
{
    MT32Synth *_this = (MT32Synth*) inUserData;

    if(_this->lastBufferData) {
        free(_this->lastBufferData);
    }
  
    unsigned int amountToWrite = *ioNumberDataPackets;

    unsigned int dataSize = amountToWrite * sizeof(MT32Emu::Bit16s) * 2; // stereo
    MT32Emu::Bit16s *data = (MT32Emu::Bit16s*) malloc(dataSize);
    _this->synth->render(data, amountToWrite, -1);
  
    ioData->mNumberBuffers = 1;
    ioData->mBuffers[0].mData = data;
    ioData->mBuffers[0].mDataByteSize = dataSize;
    _this->lastBufferData = data;
  
    return noErr;
}

static OSStatus EncoderDataProc(AudioConverterRef inAudioConverter, UInt32 *ioNumberDataPackets, AudioBufferList *ioData, AudioStreamPacketDescription **outDataPacketDescription, void *inUserData)
{
    MT32Synth *_this = (MT32Synth*) inUserData;

    if(_this->lastBufferData) {
        free(_this->lastBufferData);
    }
    
    unsigned int amountToWrite = *ioNumberDataPackets;
    unsigned int dataSize = amountToWrite * sizeof(MT32Emu::Bit16s) * 2; // stereo
  
    MT32Emu::Bit16s *data = (MT32Emu::Bit16s*) malloc(dataSize);
    _this->synth->render(data, amountToWrite, _this->lastBufferPartnum);
  
    ioData->mNumberBuffers = 1;
    ioData->mBuffers[0].mData = data;
    ioData->mBuffers[0].mDataByteSize = dataSize;
    _this->lastBufferData = data;
  
    return noErr;
}

OSStatus MT32Synth::Render(AudioUnitRenderActionFlags &ioActionFlags, const AudioTimeStamp &inTimeStamp, UInt32 inNumberFrames)
{
    if(!synth) {
        return noErr;
    }
  
    for(int bus=0; bus<2; bus++)
    {
      lastBufferPartnum = bus;
    
      AUOutputElement* outputBus = GetOutput(lastBufferPartnum);
      outputBus->PrepareBuffer(inNumberFrames);
    
      AudioBufferList& outputBufList = outputBus->GetBufferList();
      AUBufferList::ZeroBuffer(outputBufList);

      UInt32 ioOutputDataPackets = inNumberFrames * destFormat.mFramesPerPacket;

      AudioConverterFillComplexBuffer(audioConverterRef, EncoderDataProc, (void*) this, &ioOutputDataPackets, &outputBufList, NULL);
    }

    return noErr;
}

void MT32Synth::Cleanup()
{
    synth->close();
    delete synth;
    MusicDeviceBase::Cleanup();
}

OSStatus MT32Synth::SetParameter(AudioUnitParameterID inID, AudioUnitScope   inScope, AudioUnitElement inElement, AudioUnitParameterValue  inValue, UInt32  inBufferOffsetInFrames)
{
    return MusicDeviceBase::SetParameter(inID, inScope, inElement, inValue, inBufferOffsetInFrames);
}

OSStatus MT32Synth::GetParameterValueStrings(AudioUnitScope inScope, AudioUnitParameterID inParameterID, CFArrayRef *outStrings)
{
    if (outStrings == NULL) return noErr;
    return noErr;
}

OSStatus MT32Synth::SaveState(CFPropertyListRef* outData)
{
    OSStatus err = MusicDeviceBase::SaveState(outData);
  
    if (err != noErr)
            return err;
  
    assert (CFGetTypeID (*outData) == CFDictionaryGetTypeID());
  
    CFMutableDictionaryRef dict = (CFMutableDictionaryRef) *outData;
    CFDataRef dictValue = 0;

    Byte timbreSyx[512];
    memset(timbreSyx, 0, 512);
  
    //
    const uint8_t LOOKUP_SIZE = 5;
    struct lookup_t {
      CFStringRef dictKey;
  
      // @see getSysexTimbre() for libmt32emu timbre addr map comment
      uint8_t addrH;
      uint8_t addrL;
    };
  
    struct lookup_t lookup[LOOKUP_SIZE] = {
      {TIMBRE1_DICTIONARY_KEY, 0x00, 0x00}, //addrH first
      {TIMBRE2_DICTIONARY_KEY, 0x01, 0x76},
      {TIMBRE3_DICTIONARY_KEY, 0x03, 0x6C},
      {TIMBRE4_DICTIONARY_KEY, 0x05, 0x62},
      {TIMBRE5_DICTIONARY_KEY, 0x07, 0x58}
    };
  
    for(uint8_t i=0; i<LOOKUP_SIZE; i++)
    {
      memset(timbreSyx, 0, 512);
      uint16_t syxLen = getSysexTimbre(lookup[i].addrH, lookup[i].addrL, timbreSyx);
      dictValue = CFDataCreate
        (kCFAllocatorDefault, (const Byte*) timbreSyx, (CFIndex) syxLen );
      CFDictionarySetValue (dict, lookup[i].dictKey, dictValue);
      CFRelease(dictValue);
    }

    return noErr;
}

void MT32Synth::RestoreStateAsSysex( CFDataRef data )
{
    if (data == 0) return; // skip if not found
  
    const int numBytes = (int) CFDataGetLength (data);
    if (numBytes != 256) return; // must be 256 bytes incl 0xF0 and 0xF7
  
    //fprintf(stdout, "restoring %d bytes\n", numBytes);

    // send to libmt32emu
    synth->playSysex( CFDataGetBytePtr (data), numBytes );
}

OSStatus MT32Synth::RestoreState(CFPropertyListRef inData)
{
    MusicDeviceBase::RestoreState(inData);
  
    CFDictionaryRef dict = (CFDictionaryRef) inData;
    CFDataRef data = 0;
  
    // TODO - cleanup copy-paste-repeat
  
    if (CFDictionaryGetValueIfPresent (dict, TIMBRE1_DICTIONARY_KEY, (const void**) &data))
      RestoreStateAsSysex( data );
  
    if (CFDictionaryGetValueIfPresent (dict, TIMBRE2_DICTIONARY_KEY, (const void**) &data))
       RestoreStateAsSysex(data);
  
    if (CFDictionaryGetValueIfPresent (dict, TIMBRE3_DICTIONARY_KEY, (const void**) &data))
      RestoreStateAsSysex( data );
  
    if (CFDictionaryGetValueIfPresent (dict, TIMBRE4_DICTIONARY_KEY, (const void**) &data))
       RestoreStateAsSysex(data);
  
    if (CFDictionaryGetValueIfPresent (dict, TIMBRE5_DICTIONARY_KEY, (const void**) &data))
      RestoreStateAsSysex( data );

    return noErr;
}

OSStatus MT32Synth::GetParameterInfo(AudioUnitScope inScope,
                                     AudioUnitParameterID inParameterID,
                                     AudioUnitParameterInfo & outParameterInfo)
{
  return noErr;
}
