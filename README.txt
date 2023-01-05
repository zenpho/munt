mt32emuAU
=========
mt32emuAU is an AudioUnit instrument plugin adapted from Ivan Safrin's 2014 plugin code. 

The version presented here has been adapted to workaround missing features in Ableton Live for multichannel and system exclusive MIDI. You may use the CoreMidi virtual ports "MT32_input" and "MT32_output". Usage in this way is similar to using a hardware port for multichannel MIDI. A third party "editor" may also be used to import and export timbre parameters. I have had good success with using Sean Luke's Edisyn [https://github.com/eclab/edisyn] for patch editing as well as storing/restoring the state of the virtual instrument when composing.

mt32emu
=======
mt32emu is a C++ static link library required to build the AudioUnit instrument plugin
