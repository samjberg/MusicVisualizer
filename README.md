# MusicVisualizer



This is just a hobby project I made/am making (currently am making unless I just forget to update this README).  It's just a standard music (audio) visualizer, with everything that entails, including a .wav file reader.  It is being developed on Windows using MinGW, CMake for the build system with Ninja as the generator (necessary to generate compile\_commands.json, which is necessary for my development setup)

I was going to write that there is a problem where it only works on Windows and does not work on MacOS (and so likely other unix-like systems), however I just fixed those issues, and it is running fine on MacOS.



## How to build and run it

* ##### Clone the repo and then build by running the following two commands:

    * cmake -S . -B build -G Ninja -DCMAKE\_EXPORT\_COMPILE\_COMMANDS=ON
    * cmake --build build


(The last flag (-DCMAKE\_EXPORT\_COMPILE\_COMMANDS=ON) is only necessary for generating compile\_commands.json, it likely isn't necessary to simply run the code.)  

* #### Then to run it:

    * ./build/music\_visualizer.exe (on windows)
    * ./build/music\_visualizer (on basically any other OS)


## Currently supported command line flags

* -c or --frames-per-chunk or --fpc
    * Sets the number of audio frames read per chunk
    * This determines how many audio frames are used to calculate each visual "frame" of the fft bars display

* -b or --num-bars
    * Sets the number of bars in the visual fft display, the number of fft bins
    * A higher number gives more resolution in frequency space

* -g
    * Sets type of color gradient used, the options are:
        * Hor: horizontal.  This is a simple gradient from left to right across the bars.  Each bar has a constant color.
        * Ver: vertical.  This is a gradient based on the bar height, so each bar changes color depending on its current height.

* -minfreq
    * Sets the lowest frequency (in hz) displayed visually

* -maxfreq
    * Sets the highest frequency (in hz) displayed visually

* -l
    * Sets the time/interpolation parameter for the lerp (linear interpolation) used for bar heights moving between audio updates

* -mqc
    * Sets maximum number of queued chunks (in the SDL_AudioStream) before streaming more audio data.  Set this to a higher value if you are seeing/hearing stuttering.

* -rs
    * Sets the "rising speed" of bars.  This is the interpolation speed for bars while they are rising

* -fs
    * Sets the "falling speed" of bars.  This is the interpolation speed for bars while they are falling

* -s
    * Sets the "rising speed" of bars to the specified value, and sets "falling speed" to 1/2 of the specified value.
