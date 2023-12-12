# Compiling

To build all the passes for the first time run
```
export NOELLE_HOME=path/to/noelle
make
```
or
```
NOELLE_HOME=path/to/noelle make
```
Please run `make clean` to retry if `make` fails.

## Running 
The script `dt-opt` is a shortcut for running passes. For example
```
./dt-opt -printer program.bc
```
