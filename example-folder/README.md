# Savior simple example

The example simply checks a magic number value big enough to prevent AFL from generating testcases covering the true branch.

Run `make` to:
- build the SAVIOR instrumented binaries,
- create a seed into `seed_folder`,
- set a default configuration file (`example.conf`).

Verify the example behavior by running a testcase passing the magic number check:

```
./savior-example < test.in
read 41464c5f53544f50 compared against 41464c5f53544f50
```
