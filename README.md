# MatlibFLAC: A libFLAC++ interface for Matlab

This package contains Matlab classes for encoding and decoding [FLAC](https://xiph.org/flac/) files via libFLAC++. Its provides three major advantages over the built-in `audioread` and `audiowrite` functions:

 * **Streaming processing**: Data can be written and read incrementally from FLAC files, without keeping the entire contents in memory.
 * **More control**: These classes provide full control over all the file and (de)compression settings that the FLAC format allows.
 * **Consistent cross-platform behavior**: Under the hood, `audioread` and `audiowrite` depend on system libraries and their capabilities vary across platforms and Matlab releases. For example, R2014b could write 3000 Hz files on my desktop, but not Guillimin, even though they're both Unix-y.

The interface is meant to be object-oriented and very similar to libFLAC++. To encode data, you create a FileEncoder object, set whatever options are required, and then submit the data for compression via 'process'.
```
t = 0:(1/1000):10;
x = round(sin(2*pi*t) .* 2^14);
e = FileEncoder('test.flac');
e.sample_rate = 1000; %Change from default 44100 Hz
e.process([x;y]);
e.finish(); %Optional--also handled by delete()
```
Process can be called multiple times to incrementally build a file. The decoder works similarly:
```
d = FileDecoder(test.flac)
data = d.read_segment(1, 100);
plot(t(1:100), data(1,:));
```
and the same FileDecoder can be used to extract many segments from the same file. See the class documentation for more details. The properties follow libFLAC++'s naming scheme for the [FLAC::Encoder::File](https://xiph.org/flac/api/classFLAC_1_1Encoder_1_1File.html) and [FLAC::Decoder::File](https://xiph.org/flac/api/classFLAC_1_1Decoder_1_1File.html); see those docs for details.

## Installation
Precompiled binaries are available for Windows in `/precompiled`. Move those mex files into the same directory as FileEncoder and FileDecoder. For Mac and Linux, build as follows:

1. **Get a C++ compiler.** Since it needs to work with Matlab/MEX, you may need a much older version than whatever is installed installed on your system by default. R2016b, for example, uses gcc 4.9, instead of gcc7 See [here](https://www.mathworks.com/support/compilers.html) for a list of supported compilers.

 Set it up and run `mex -setup c++` when you are done.

  You *may* need to point MATLAB at this compiler, particularly if you have more than one installed. Save an empty file (`foo.cpp`) and try to compile it with `mex('-v', 'foo.cpp')`. One of the first lines in the output will say "Compiler location: ", followed by a path. *Make sure this is the right path.* If not, edit the file listed on the next line.  On R2016b, look for a section called `<locationFinder>` and adjust the command in `<cmdReturns name="which g++"/>`as needed (e.g, replace g++ with g++-4.9). On older versions, look for lines starting with CXX= or GCC=. There are a few and you must change them all.

2. **Get libFLAC++.** It is important that libFLAC++ be built with the same MEX-compatible compiler used above (which also often precludes using a systemwide copy of libFLAC). To do this on a POSIX-y system (Linux, Mac, or Cygwin/MingW on windows):

  a. Download the most recent version of the [libFLAC++ source](http://downloads.xiph.org/releases/flac/) and unzip/untar it.

  b. Configure it. You probably want to do something like `CC=gcc-4.9 ./configure --prefix=/path/for/FLAC/folder`, where gcc-4.9 is a MEX-compatible C compiler (don't put g++ there!) and the path is a folder where you stash the build products

  c. Make it. `make && make install`. Wait a bit while it runs; it will

3. **Build the MEX files.**  Edit `build.m` to point to your `/path/for/FLAC/folder` and run it to compile the MEX Files.

## To do
 * **SEEKTABLE**: The FLAC format supports SEEKTABLES, which accelerates random access to pre-determined parts of the file. These are not yet supported.
 * **Parallel supprt**  Obviously, you cannot submit data in parallel for encoding, though it *should* be possible to read from a file in parallel. Right now, this crashes `parallel_function.m`--it looks like it "migrates" the objects without calling the copy constructor.
 * **Copy** (for FileEncoder) and **load/save** constructors. This would mostly be useful for configuring a "template" encoder that could be reused.
 * **Metadata** The FLAC format allows for a ton of different metadata, ranging from simple text comments to album art. None of that is presently supported.

## Acknowledgements
* This uses [class_handle.hpp](https://www.mathworks.com/matlabcentral/fileexchange/38964-example-matlab-class-wrapper-for-a-c++-class), by Oliver Woodford.
* FLAC and libFLAC are mantained by the [Xiph.Org](xiph.org) Foundation
