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
* `chronos iterate <directory> <print-format>` prints all the entries in the log to standard output, using specified print format.

* `chronosd daemon <directory>` starts a daemon that accepts appends via fifo, avoids much of the `chronos apppend` overhead.
* `chronosd append <directory>` works as `chronos append` would, except that it communicates to the chronos daemon.

Chronos is multi-process safe, and accomplishes this by using file locks on the directory that houses it's index and data store. Because of the way Chronos appends to it's physical datastore, it can allow reads to the datastore while writes are occuring (write data first, then index).

### Performance

Chronos has been informally bench-marked and supports ~60 insertions per second under the following scenario:

```
while true ; do cat 1kfile | chronos append ./dir ; done

# or like so, if you want to use a daemon
chronosd daemon ./dir &
while true ; do cat 1kfile | chronosd append ./dir ; done
```

The performance variant of Chronos can support a much higher rate of insertions, up to ~400 per second, by not requiring `fsync(2)`. `fsync(2)` asks the kernel to flush all data writes to the underlying Chronos datastorage before returning. This means that when `chronos(d) append` returns, the data may not have been written to disk (unlike the default variant).

### Concurrency

Chronos is safe to use concurrently. Using `chronos append` or `chronod daemon` invokes `flock(3)` (exclusive file system locking) on the directory passed to it. All other `chronos(d)` commands _do not_ use `flock(3)`, and instead assume that `chronos(d)` can correctly write to the files in such a way that it will not get an inconsistent file.

### Running Time

Since one of the goals of Chronos is efficiency, here are the running times for the various commands:

Command | Running Time
:-------|:------------
append  | O( 1 + io )
count   | O( 1 )
get     | O( log(n) + io )
init    | O( 1 )
iterate | O( n * io )
last    | O( 1 )
list    | O( n )

`n = # of keys`, `io = time to write data to stdout or datastore`

Note that chranosd has identical runtimes to chronos, except it attempts to avoid much of the process overhead that bottlenecks how quickly `chronos append` can execute.
