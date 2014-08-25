# Chronos

Chronos is a zero dependency event log that handles arbitrary binary data and is designed to be used in shell scripts. It supports the operations of appending, retrieving a single key, and iterating over all of the entries efficiently.

Note that Chronos builds with `gcc -D_GNU_SOURCE` which may not be supported on all platforms. This flag is used to avoid writing a fair amount of code to do string scanning, and time structure manipulation. If you find an unsupported platform create an issue, if you also write a patch, please send a pull request.

### Setup

Run: `tup init ; tup` to build Chronos.

### Commands

Chronos is intended to be used in a scripted environment, and supports a few commands.

* `chronos init <directory>` creates a new chronos log in the specified directory.
* `chronos count <directory>` counts the number of entries in the log.
* `chronos last <directory>` lists the last key inserted into Chronos.
* `chronos list <directory>` lists all of the times at which data was inserted into Chronos.
* `chronos get <directory> <key>` retrieves a single key and writes it to stdout.
* `chronos append <directory> [-t key]` inserts new data into the Chronos log, reading the data from stdin.
* `chronos iterate <directory> [d1 [, d2 [, d3]]]` prints all the entries in the log to standard output, using specified delimiters.

Chronos is multi-process safe, and accomplishes this by using file locks on the directory that houses it's index and data store.

### Performance

Chronos has been informally bench-marked and supports ~150 insertions per second under the following scenario:

```
while true ; do cat 1kfile | chronos append ./dir ; done
```

### Running Time

Since one of the goals of Chronos is efficiency, here are the running times for the various commands:

Command | Running Time | Comments
:-------|:-------------|:--------
append  | O( 1 + m )   | m = time to read the data in and write it to file
count   | O( 1 )       |
get     | O( log(n) + m ) | m = time to write the data to stdout
init    | O( 1 )       |
iterate | O( n )       |
last    | O( 1 )       |
list    | O( n )       |

