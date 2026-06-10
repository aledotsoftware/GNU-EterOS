## JAA Global System State - Agent Update

- Improved DNS Resolution: `cmd_ota.c` now dynamically resolves update server URLs via `net_gethostbyname` instead of hardcoded IPs.
- Addressed memory leaks in `cmd_ota.c` update subcommand to properly `kfree` partition nodes on error flows.
