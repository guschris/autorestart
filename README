# health_montor

A simple Linux command line tool that:
* starts a child process, redirecting `stdout` and `stderr`
* checks it's responding by calling a HTTP health endpoint every 5 seconds
* retarts it (up to 3 times) if it stops or the heath check fails

## Building

1. Install `libcurl-dev`, e.g. `sudo apt install libcurl3-gnutls-dev` on Debian
2. run `./build`

## Usage

```
Usage: ./health_monitor [--health <url>] [--retry <count>] -- <child_process> [args...]
```

Where:
* `--health` is the HTTP endpoint to check
* `--retry` is the maximum numer of times to retry the fail process
* all arguments after `--` is the command to run

If the process fails more than the maximum number of allowed retries than and exit code of `1` is returned.