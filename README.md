# L4Re IO

IO is the management component that handles access to platform devices and
resources such as I/O memory, ports (on x86) and interrupts. It grants and
controls access to these resources for all other L4Re applications and VMs.

This package includes the following components:

* io - main server and resource manager
* libvbus - IO client interface and definitions
* libio-direct - convenience functions for requesting hardware resources
                 directly from the kernel/sigma0
* libio-io - convenience functions for requesting hardware resources from IO

# Documentation

This package is part of the L4Re operating system. For documentation and
build instructions please refer to [l4re.org](https://l4re.org).

# Contributions

We welcome contributions. Please see the
[contributors guide](https://l4re.org/contributing/).

# License

Detailed licensing and copyright information can be found in the
[LICENSE](LICENSE.spdx) file.
