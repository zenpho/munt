mt32emu
=======
mt32emu is a C++ static link library required to build the AudioUnit instrument plugin

mt32emuAU
=========
mt32emuAU is an AudioUnit instrument plugin adapted from Ivan Safrin's 2014 plugin code. 

The AudioUnit version presented here has been adapted to workaround missing features in Ableton Live for multichannel and system exclusive MIDI. You may use the CoreMidi virtual ports "MT32_input" and "MT32_output" for multichannel MIDI and to export and import timbre parameter data using a third party editor. I have had good success with using Sean Luke's Edisyn [https://github.com/eclab/edisyn] for realtime patch editing via system exclusive.
