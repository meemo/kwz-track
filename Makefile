# kwz-track Makefile

## flags
common_flags = -std=c++11 -march=native -Wall -Wextra -Wpedantic
optimization_flags = -Ofast
lib = -lz
infile = kwz-track.cpp
outfile = -o kwz-track

kwz-track: kwz-track.cpp
	g++ $(common_flags) $(optimization_flags) $(infile) $(lib) $(outfile)
