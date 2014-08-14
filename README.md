# Chronos

Chronos is an event log that was designed to be scriptable. It supports the operations of appending, retrieving a single key, and iterating over all of the entries efficiently.

### Setup

Run: `tup init ; tup` to build Chronos.

### Commands

Chronos is intended to be used in a scripted environment, and supports a few commands.

* `chronos init <directory>` creates a new chronos log in the specified directory.
* `chronos list <directory>` lists all of the times at which data was inserted into Chronos.
* `chronos get <directory> <key>` retrieves a single key and writes it to stdout.
* `chronos submit <directory>` inserts new data into the Chronos log, reading the data from stdin.

