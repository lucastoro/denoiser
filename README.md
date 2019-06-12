# Artifact Denoiser
A basic Log simplifier based on the "Artificial ignorance" concept.

## Rationale:
Given 2 log files, one containing the output of a successful run and the other a failed one, it is possible to strip
from the second one multiple non meaningful information using the first as a reference.

The assumption is that if one consider:  
**[Good log] = [non meaningful information]**  
**[Bad log]  = [non meaningful information] + [meaningful information]**  
then:  
**[meaningful information] = [Bad log] - [Good Log]**

The algorithm works as follow:
1) transforms every line of the "good" log(s) file in a set of hashes.
2) for each line of the "bad" log file, computes the hash of that line, and if it's not occurring in the reference hash
   set then treats it as meaningful and reports it to the user.

Lines from both log files need to be "generalized" to be meaningfully comparable, for instance the following lines:

`10-10-2017 | DEBUG | Nothing important happened`

and

`11-11-2018 | DEBUG | Nothing important happened`

should be considered identical even if their content are actually different (the dates are different).
To achieve this behavior log lines are "preprocessed" or "normalized" before being actually analyzed.
This is achieved evaluating a bunch of regular expressions (more generically "patterns") on the input files and
discarding all matching information.
In the example above the pattern would be a regular expression in the form `\d{2}-\d{2}-\d{4}` that will cause the
algorithm to threat the two lines as identical.

## Motivation
Working with a CI infrastructure running thousands of daily jobs isn't an easy task, in particular when that nasty bug
you're hunting is hidden in a eighty thousand lines long log file. This work is my attempt to ease a bit my work and
that of my coworkers. (Who am I fooling I just wanted an excuse to spend some night with C++17)

## Terminology
As this small work was born by the need to post-process [Jenkins](https://jenkins.io) jobs logs, some terminology
was adopted from that environment, in particular the term "artifact" here is used as something that is a bit more
generic than a "log" but not as coarse as a "file", but for the sake of understanding each other you can easily find &
replace every occurrence of "artifact" with "log file".

## Usage
The process is invoked with one primary argument: the configuration file.
This can be read directly from `stdin` (the default behavior) or from disk, through the `--config` option.
The configuration file contains all the informations needed to perform the analysis.
Other flags are used to tune output and behavior:
```
--config    -c: read the configuration from the given filename instead from stdin
--directory -d: change the working directory to the given path
--no-lines  -n: do not output line numbers in the output
--verbose   -v: print information regarding the process to stderr
--profile   -p: print profiling information to stderr
--debug     -d: print even more information to stderr
```
When compiled with support for thread pools the following will be available:
```
--jobs      -j: use the given number of threads, defaults to the number of hw threads
```
When compiled with the tests enabled the following options will be available as well:
```
--test      -t: executes the unit tests
--gtest_*     : in combination of --test will be forwarded to the gtest test suite
```
The process output is always written on the standard output stream, while errors, logs and profiling data will be
written on the standard error stream.

### Configuration file
The configuration file is a YAML file composed of 5 section:
 - the `alias` section will hold a simple mnemonic name for the processing.
 - the `target` section will tell the algorithm from where to load the target ("bad") log file.
 - the `reference` section holds a list of other artifacts from which to generate the reference table. Note that it
   is possible (though not necessary) to specify multiple "reference" ("good") files from which to build the hash set.
   All files will participate in filling the hash bucket. 
 - the `filters` section contains a list of patterns (see the Patterns section below) that will cause corresponding
   whole lines to be discarded by the denoiser algorithm.
   Filters are useful when you know that some line in the artifact won't be of any interest.
   So for instance if you want to ignore all 'debug' lines you will add an entry in the filters section containing a
   pattern selecting those lines (e.g. '[DEBUG]' or '[D]'...).
 - the `normalizers` section contains a list of patterns that will cause corresponding entries to be ignored by the
   denoiser algorithm.
   Normalizers are used when there are instance-specific informations in the log that you want to flatten out, like
   dates, PIDs, IP addresses, UUIDs and so on.
   So let's say you want to normalize out all `hh:mm::ss` times from the log you would add something like
   `\\d{2}:\\d{2}:\\d{2}` in the normalizers section.

### Patterns
Each entry in the patterns section may be a string or a regular expression.
In the YAML file each entry is represented by a `key:value` pair, where the key must be either "s", to indicate that the
pattern is a string, or a "r" indicating that the pattern is a regular expression.
Both strings and regular expressions are evaluated case-sensitive, strings will be searched inside

Artifacts can be loaded from the local hard drive or downloaded from the web through the HTTP(S) protocol.
To specify a local artifact use the `file://` protocol specifier, while when downloading from the web, use either
`http://` or `https://` accordingly. Local artifacts are always searched from the current working directory.

#### A word about the file:// protocol
The `file://` protocol used in this work may look similar to the [File URI Scheme](https://tools.ietf.org/html/rfc8089)
but is in fact pretty different, first of all the hostname part of the URI is not supported at all, while in the RFC is
considered optional, and secondly the **two** slashes "//" are part of the protocol identifier.  
- In the form `file://path/to/file` the path will be considered relative to the current directory
- while when specified as `file:///path/to/file` (with 3 slashes) it will be considered an absolute path starting from
  the root of the file system.

### Example configuration
This is a simple YAML configuration:

```
filters: # the filter section will discard whole lines
 - s: 'DEBUG' # this string will cause all lines containing 'DEBUG' to be ignored
 - r: 'INFO|WARNING' # this reg. expression will cause all matching lines to be ignored

normalizers: # the normalizers section will discard specific portions
 - s: 'luca' # this string will cause all occurences of 'luca' to be ignored
 - r: '\\d{2}:\\d{2}:\\d{2}' # this reg. expression will cause all 'dd:mm:ss' dates to be ignored

alias: project-output # a simple mnemonic
target: file://output.log # this will load the output.log file from disk
reference:
 - http://logs.localhost:/log-0001.log # here we use 3 different "reference" files, one fetched through HTTP
 - https://logs.localhost:/log-0002.log # the other through HTTPS
```

### Unicode support
The project has partial support for unicode, deconding from `UTF-8`. "Partial" means that while full UTF-8 deconding is
enabled, the application does not try to [normalize the codepoints](https://unicode.org/reports/tr15) in any way.
If you're an unicode expert, you know that this means that I'm a lazy person, but if you're not, this just means that it
should be good enough for you.  
`UTF-8`, `Latin 1` and plain `ASCII` decoders are supported, and will be used both when reading from disk and when
downloading through http. When downloading the right decoder will be inferred by the `Content-Type` header field, while
when loading from disk `UTF-8` will be used by default.

## Small technicalities
This implementation relies heavily on multi-threading (in particular when built with the `DENOISER_THREAD_POOL` option
enabled). By default the denoiser will use all CPU cores available, you can change this behavior via the `--job`
option ("a la make").

## Building
This is a pretty standard [CMake](https://cmake.org) project, as usual the pattern is
```
git clone --recursive https://github.com/lucastoro/denoiser.git
mkdir build
cd build
cmake ../denoiser
make
```

### Dependencies & Requirements
All direct dependencies ([yaml-cpp](https://github.com/jbeder/yaml-cpp),
[google test](https://github.com/google/googletest) and [curlpp](http://www.curlpp.org/)) are fetched as submodules of
the project, but curlpp may still need `libcurl` to compile properly; `sudo apt install libcurl4-openssl-dev` or
`yum install libcurl-devel` or `apk add curl-dev` should do the trick, but check `curlpp` project for more informations.

This is a **C++17** project so a suitable version of the compiler will be needed (`gcc 7` or `clang 4` should do the
trick), also `CMake 3.8` is required at least.

## Testing
When built with the `WITH_TESTS` option enabled (the default) the project will contain a number of unit tests that can
be executed with the `--test` flag. The test suite is built with the [Google Test](https://github.com/google/googletest)
library and all relevant flags (`--gtest_filter`, `--gtest_repeat`...) will be forwarded to it.

While being developed mostly for some level of CI testing, it is also possible to execute the test suite using
[Docker](https://www.docker.com/), in such case the process will generate an image, compile the source code and run
the test cases inside the container, it may be triggered invoking the `test/run.sh` script.

## References and Thanks
http://www.ranum.com/security/computer_security/papers/ai for the idea.  
Thanks to **Marco Pensallorto** for the inspiration.

[![MIT license](https://img.shields.io/badge/License-MIT-blue.svg)](https://lbesson.mit-license.org/)
[![Build Status](https://travis-ci.org/lucastoro/denoiser.svg?branch=master)](https://travis-ci.org/lucastoro/denoiser)
