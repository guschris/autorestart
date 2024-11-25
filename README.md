# monitor and monitor_ssh

`monitor` is a simple Linux command line tool that:
* starts a child process, redirecting `stdout` and `stderr`
* checks it's responding by calling a HTTP health endpoint every 5 seconds and writing the results to `stderr`
* restarts the child if it exits, or the health check fails

`monitor_ssh` is a tool to run `monitor cmd <arg>` remotely:
* starts `monitor ...` on a remote server via SSH, redirecting `stdout` and `stderr`
* checks the remote `monitor`is writing to `stderr` every 5 seconds
* restarts the SSH session if it exits, or `monitor` stops writing to `stderr`

## Building

1. Install `libcurl-dev`, e.g. `sudo apt install libcurl3-gnutls-dev` on Debian
1. Install `libssh-dev`, e.g. `sudo apt install libssh-dev` on Debian
2. run `./build`

The script will build two executables in the current directory:
* A 17K executable called `monitor`.
* A 18K executable called `monitor_ssh`.

## Usage

### monitor usage

```
Usage: ./monitor [--health <url>] [--retry <count>] -- <child_process> [args...]
```

Where:
* `--health` is the HTTP endpoint to check, defaults to `http://localhost/up`
* `--retry` is the maximum numer of times to retry the fail process, defaults to `3`
* all arguments after `--` is the command to run

If the process fails more than the maximum number of allowed retries than and exit code of `1` is returned.