# ArtifactDenoiser
a basic Log simplifier based on the "Artificial ignorance" concept.

## Rationale:
Given 2 log files, one containing the output of a successful run and the other a failed one,
it is possible to strip from the second one multiple non meaningful information using the first
as a reference.

The assumption is that if one consider:  
**[Good log] = [non meaningful information]**  
**[Bad log]  = [non meaningful information] + [meaningful information]**  
then:  
**[meaningful information] = [Bad log] - [Good Log]**

The algorithm works as follow:
1) transforms every line of the "good" log file in a set of hashes.
2) for each line of the "bad" log file, computes the hash of that line, and if it's not
occurring in the reference hash set then treats it as meaningful and reports it to the user.
:
Lines from both log files need to be "generalized" to be meaningfully comparable, for instance
the following lines:

`10/10/2017 | DEBUG | Nothing important happened`

and

`11/11/2018 | DEBUG | Nothing important happened`

should be considered identical even if their content are actually different.
To achieve this behavior log lines are "preprocessed" or "normalized" before being actually
analyzed.

Also to speed up the things up a little there is a "filtering" step executed before  everything
else that may be used to skip lines that can be considered non useful prior to processing,
it may apply to headers or known non relevant content.

## References
http://www.ranum.com/security/computer_security/papers/ai