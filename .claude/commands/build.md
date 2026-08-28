Build the Qapla2 project using make.

IMPORTANT: Do NOT use the Bash tool for this. The Bash tool runs under bash, which expands
the glob `build/*` in the Makefile's `find` command, breaking source file discovery.
Instead, run the build via cmd.exe using the Bash tool with this exact command:

```
cmd //c "cd /d c:\\Development\\Qapla2 && make Release -j" 2>&1
```

Examine the output carefully:

- If the build succeeds, report success.
- If linking fails with an error indicating the output file is locked or in use (e.g. "Access is denied", "cannot open output file", "file in use", "Permission denied" on the `.exe` or `.dll`), **stop immediately** and tell the user that the process is already running and needs to be closed before linking can succeed. Do not retry.
- For any other error, report the relevant error lines to the user.
