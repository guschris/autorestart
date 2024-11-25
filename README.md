# health_monitor

A simple Linux command line tool that:
* starts a child process, redirecting `stdout` and `stderr`
* checks it's responding by calling a HTTP health endpoint every 5 seconds
* retarts it the child process stops or the heath check fails

## Building

All the source code is in `health_monitor.c`.

1. Install `libcurl-dev`, e.g. `sudo apt install libcurl3-gnutls-dev` on Debian
2. run `./build`

A 17K executable will be built in the current directory, called `health_monitor`.

## Usage

```
Usage: ./health_monitor [--health <url>] [--retry <count>] -- <child_process> [args...]
```

Where:
* `--health` is the HTTP endpoint to check, defaults to `http://localhost/up`
* `--retry` is the maximum numer of times to retry the fail process, defaults to `3`
* all arguments after `--` is the command to run

If the process fails more than the maximum number of allowed retries than and exit code of `1` is returned.