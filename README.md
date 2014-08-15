# Chronos

Chronos is a zero dependency event log that was designed to be scriptable. It supports the operations of appending, retrieving a single key, and iterating over all of the entries efficiently.

Note that Chronos builds with `gcc -D_GNU_SOURCE` which may not be supported on all platforms. This flag is used to avoid writting a fair amount of code to do string scanning, and time structure maniplutaion. If you find an unsupported platform create an issue, if you also write a patch, please send a pull request.

### Setup

Run: `tup init ; tup` to build Chronos.

### Commands

Chronos is intended to be used in a scripted environment, and supports a few commands.

* `chronos init <directory>` creates a new chronos log in the specified directory.
* `chronos list <directory>` lists all of the times at which data was inserted into Chronos.
* `chronos get <directory> <key>` retrieves a single key and writes it to stdout.
* `chronos append <directory>` inserts new data into the Chronos log, reading the data from stdin.

Chronos is multiprocess safe, and accomplishes this by using file locks on the directory that houses it's index and data store.

### Performance

Chronos has been informally benchmarked and supports ~150 insertions per second under the following scenario:

```
while true ; do cat 1kfile | chronos append ./dir ; done
```

### Running Time

Since one of the goals of Chronos is efficiency, here are the running times for the various commands:

Command | Running Time | Comments
:-------|:-------------|:--------
init    | O( 1 )       |
list    | O( n )       |
get     | O( log(n) + m ) | m = time to write the data to stdout
append  | O( 1 + m )   | m = time to read the data in and write it to file

