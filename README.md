# kwz-audio
Extracts audio from .kwz files and writes it to a .wav file. Also works with .gz compressed .kwz files.


# Requirements
Assumes `build-essential` or equivalent is installed (compilation tools).

Requires zlib development headers which can be installed with the following commands.

## Debian
```shell
sudo apt install zlib1g-dev
```

## CentOS
```shell
yum install zlib-devel
```


# Compilation
```shell
make
```


# Usage
```shell
./kwz-audio {track index} {input file} {output file} {initial step index}
```

Where:

`{track index}` is the index of the track to decode (numbered in the table below)

`{input file}` is the path to your input .kwz file

`{output file}` is the path to the .wav file you wish for the program to output.

`{initial step index}` is the initial step index you wish to decode the track with. Defaults to 0 if not specified. Valid range is 0-40.

Note: do not include `{}` curly brackets.

| Track Index | Description           |
|-------------|-----------------------|
| 0           | BGM, background music |
| 1           | Sound Effect 1 (A)    |
| 2           | Sound Effect 2 (X)    |
| 3           | Sound Effect 3 (Y)    |
| 4           | Sound Effect 4 (Up)   |
