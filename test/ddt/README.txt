# Data Driven Tests

Put here a directory named [whatever] containing (at least) the following files:
 - config.yaml: containing the description of the analysis to perform
 - expect.log: the expected output of the test

Specifing the URLs to the artifacts using the [file://] protocol the test will look in the [whatever] directory for them

example config.yaml:

---
filters: 
- s: 'DEBUG'
- r: 'INFO|WARNING'

normalizers: 
- s: 'luca'
- r: '\d{2}:\d{2}:\d{2}'

artifacts:
- alias: test-env1
  target: file://target.log
  reference:
  - file://ref1.log
---

In the same directory there must be a target.log, a ref1.log (because listed in the config.yaml) and an expect.log file (because expected by the test suite).
The output of the algorithm will be compared to the content of the expect.log file.
