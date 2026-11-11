# Reporting startup failures

Duck Hunt records startup logs automatically, including failures before Play.
After a failure, look for the `diagnostics` folder beside `DuckHuntRecomp.exe`.

If the game produces no log or exits immediately, run `Collect-Diagnostics.cmd`
from the extracted download. Select your ROM and reproduce the problem. Close
the game if it remains open. The helper reports the folder to send and captures
the process exit code, output, Windows version and graphics driver information.

If the installation folder is read-only, the report is written under
`%LOCALAPPDATA%\NESRecomp\diagnostics` instead.

Zip the report folder and attach it to your issue along with the release name
and the last action you took. Logs and crash dumps can contain local paths or
process memory; review them before sharing. Nothing uploads automatically.
Keep your ROM and existing saves/settings outside the report archive.
