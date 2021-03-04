# procServUtils

`manage-procs` is a script to run procServ instances as systemd services.
It allows to completely manage the service with its command line interface.
See below for the available commands.

## Installation from source

Python prerequisites:

```bash
sudo pip install tabulate termcolor argcomplete
```

Then proceed to build and install procServ with the `--with-systemd-utils` configuration option. For example:

```bash
cd procserv
make
./configure --disable-doc --with-systemd-utils
make
sudo make install
```

**[OPTIONAL]** If you want to activate bash autocompletion run the following command:

```bash
sudo activate-global-python-argcomplete
```

This will activate global argcomplete completion. See [argcomplete documentation](https://pypi.org/project/argcomplete/#activating-global-completion) for more details.

## Using manage-procs

See `manage-procs --help` for all the commands.

### User or system services

All the `manage-procs` commands support one option to specify the destination of the service:

- `manage-procs --system` will manage system-wide services. This is the default options when running as root.

- `manage-procs --user` will manage user-level services. It is the equivalent
  of `systemctl --user`. This is the default when running as a non-privileged user.

**NOTE:** Not all linux distributions support user level systemd (eg: Centos 7). In those cases you should always use `--system`.

### Add a service

Let's add a service:

```bash
manage-procs add service_name some_command [command_args...]
```

This will install a new service called `service_name` which will run the specified command
with its args.

With the optional arguments one can specify the working directory, the user
running the process and also add some environment variables. For example:

```bash
manage-procs add -C /path/to/some_folder \
                 -U user_name -G user_group \
                 -e "FOO=foo" -e "BAR=bar" \
                 service_name some_command [command_args...]
```

Alternatively one can write an environment file like:

```bash
FOO=foo
BAR=bar
```

And run:

```bash
manage-procs add -C /path/to/some_folder \
                 -U user_name -G user_group \
                 -E /path/to/env_file \
                 service_name some_command [command_args...]
```

See `manage-procs add -h` for all the options.

### List services

```bash
manage-procs status
```

will list all the services installed with `manage-procs add` and their status.

```bash
manage-procs list
```

will show the underlying systemd services.

### Start, stop, restart service exection

```bash
manage-procs start service_name
manage-procs stop service_name
manage-procs restart service_name
```

### Remove or rename a service

To uninstall a service:

```bash
manage-procs remove service_name
```

To rename a service:

```bash
manage-procs rename service_name new_service_name
```

Note that this command will stop the service if it's running.

### Attach to a service

`procServ` enables the user to see the output of the inner command and, eventually, interact with it through a telent port or a domain socket.

```bash
manage-procs attach service_name
```

This will automatically open the right port for the desired service.

### Open service log files

All the output of the service is saved to the systemd log files. To open them run:

```bash
manage-procs logs [--follow] service_name
```
