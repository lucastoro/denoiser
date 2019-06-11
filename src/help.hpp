#pragma once

#include <iostream>
#include <cstring>

#define nl "\n"

static constexpr const char* HELP =
   "Denoise a document ('artifact') removing parts occurring in other artifacts, discarding"
nl "file-specific informations."
nl "TODO: Insert reference to the algorithm source"
nl
nl "OPTIONS:"
nl " --config    -c: read the configuration from the given filename"
nl " --stdin     - : read the configuration from the input stream"
nl " --directory -d: change the working directory to the given path"
nl " --verbose   -v: print information regarding the process to stderr"
nl " --profile   -p: print profiling information to stderr"
nl " --debug     -d: print even more information to stderr"
nl
nl "The --config and --stding options are mutually exclusive and exactly"
nl "one of them must be specified."
nl
nl "CONFIGURATION:"
nl "The configuration file is a YAML file composed of 3 section:"
nl " - the 'filters' section contains a list of patterns (see the PATTERNS section below) that will"
nl "   cause corrispective whole lines to be discarded by the denoiser algorithm."
nl "   Filters are useful when you know that some line in the artifact won't be of any interest."
nl "   So for instance if you want to ignore all 'debug' lines you will add an entry in the filters"
nl "   section containing a pattern selecting those lines (e.g. '[DEBUG]' or '[D]'...)."
nl " - the 'normalizers' section contains a list of patterns that will cause corrispective entries"
nl "   to be ingored by the denoiser algorithm."
nl "   Normalizers are used when there are instance-specific informations in the log that you want"
nl "   to flatten out, like dates, PIDs, IP addresses, UUIDs and so on."
nl "   So let's say you want to normalize out all hh:mm::ss times from the log you would add"
nl "   something like '\\d{2}:\\d{2}:\\d{2}' in the normalizers section."
nl " - the 'artifacts' section contains a list configuration entries, one for each artifact to"
nl "   process, see the section ARTIFACTS below for more informations on the format."
nl
nl "PATTERNS:"
nl "Each entry in the patterns section may be a string or a regular expression."
nl "In the YAML file each entry is represented by a key:value pair, where the key must be"
nl "either 's', to indicate that the pattern is a string, or a 'r' indicating that the pattern"
nl "is a regular expression."
nl
nl "ARTIFACTS:"
nl "Each artifact configuration will contain 3 sections:"
nl " - the alias subsection will hold a mnemonic name for the given artifact."
nl " - the target subsection will tell the algorithm from where to load the artifact file."
nl " - the reference subsection holds a list of other artifacts from which to generate the"
nl "   reference table."
nl "Artifacts can be loaded from the local hard drive or downloaded from the web through the"
nl "HTTP(S) protocol. To specify a local artifact use the 'file://' protocol speficier, while when"
nl "downloading from the web, use either 'http://' or 'https://' accordingly."
nl "Local artifacts are always searched from the current working directory."
nl
nl "HINTS and CAVEATS:"
nl " - filters are applied BEFORE normalizers."
nl " - the order of the regular expression MATTERS, this is an infortunate condition,"
nl "   but unavoidable one, as regular expressions may sub match other ones."
nl "   Take for instance the followings:"
nl "   * '[a-zA-Z]{3} \\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2} CET' (Mon 2017-03-20 15:09:58 CET)"
nl "   * '\\d{2}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}' (17-03-20 15:09:58)"
nl "   If These 2 regular expression were inverted, the second one would predate the first one"
nl "   that would result in something like 'Mon 20 CET' that in turn would not match the second one"
nl
nl "EXAMPLE:"
nl "This is a simple YAML configuration:"
nl "#  ---"
nl "#  filters:"
nl "#  - s: 'DEBUG' # this entry will cause all lines containing 'DEBUG' to be ignored"
nl "#  - r: 'INFO|WARNING' # this entry will cause all matching lines to be ignored"
nl "#"
nl "#  normalizers:"
nl "#  - s: 'luca' # this entry will cause all occurences of 'luca' to be ignored"
nl "#  - r: '\\d{2}:\\d{2}:\\d{2}' # this entry will cause all 'dd:mm:ss' dates to be ignored"
nl "#"
nl "#  artifacts:"
nl "#  - alias: project-output # a simple mnemonic"
nl "#    target: file://output.log # this will load the output.log file from disk"
nl "#    reference:"
nl "#    - https://logs.localhost://succesfull.log # this will download the file via HTTPS";

#undef nl

static const char* my_name(const char* self) {
  const auto ptr = strrchr(self, '/');
  return ptr ? ptr + 1 : self;
}

static inline void print_help(const char* self, std::ostream& os) {
  os << "Usage: " << my_name(self) << " [OPTIONS]" << std::endl;
  os << HELP << std::endl;
}
