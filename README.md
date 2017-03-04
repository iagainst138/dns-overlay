# dns-overlay

Uses a mount namespace to create a unique /etc/resolv.conf for a process tree.


## Usage

```sh
dns-overlay -f /path/to/custom/resolv.conf -c bash
```

This will mount /path/to/custom/resolv.conf over /etc/resolv.conf and start a new bash shell. This will result in the new shell and its children using different DNS servers to the rest of the processes. The CAP_SYS_ADMIN capability is set on the dns-overlay binary and is dropped before starting the child process.

> NOTE:
>
> Child processes are made with the system call.


## Building

```sh
make setcap
```

This will create the binary dns-overlay and set the CAP_SYS_ADMIN capability on it (assumes the user building can sudo).


## Why?

I needed to test some stuff I was doing in VMs with DNS and didn't want changes to affect other processes.
