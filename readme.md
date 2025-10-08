# helperbot – user guide

Type `!commands` to see the list of commands. Use commands in a channel or in a DM with the bot unless noted.

## Quick list


## Mode / Invite / Topic docs

  `+i` invite-only, `+t` topic by ops only, `+k <key>`, `+l <limit>`  
  Examples:  
  - `/MODE #room +t` (only ops can set topic)  
  - `/MODE #room +k hunter2` (set a join key)  
  - `/MODE #room -k` (clear key)  
  - `/MODE #room +l 42` (limit to 42 users)



> The bot doesn’t perform admin actions like `kick` or setting modes; it only assists with info, utilities, and coordination.

## File Transfer

This server implements a simple file-transfer helper with two modes: automatic server-side streaming (recommended for tests / local files) and a legacy/manual push mode (where the sender streams base64 chunks to the server). Files saved by the server are placed under `File Transfers/` as `File Transfers/<tid>_<sanitized-filename>`.

High-level commands (raw IRC):

- `FILESEND <nick> <size> :<filename>` — sender offers a file to `<nick>`. `<size>` is optional (use `0` if unknown). The `<filename>` is a client-provided name; the server will sanitize it when saving.
- `FILEACCEPT <tid>` — receiver accepts an offer identified by transfer id `<tid>`. Streaming starts automatically for the auto-stream mode.
- `FILEDATA <tid> <base64chunk>` — (legacy/manual) sender pushes a base64-encoded chunk for transfer `<tid>`; server forwards the same base64 to the receiver and appends the decoded bytes to the saved file.
- `FILEDONE <tid>` — (legacy/manual) sender indicates the transfer is finished.
- `FILECANCEL <tid>` — either side cancels an active transfer.

Server numeric replies and messages you may see:

- `738` — broadcast to the receiver describing the incoming offer (sent by the server when `FILESEND` is issued).
- `739` — acknowledgement to the sender that an offer was created (includes the transfer id).
- `742 * <tid> :ACCEPTED` — sent to the accepting receiver when `FILEACCEPT` succeeds.
- `746 * STREAM :BEGIN <safe-filename>` — indicates streaming is about to begin (sent to both sender and receiver).
- `740 * <base64chunk>` — streaming data lines (base64) forwarded to the receiver.
- `741 * <filename> :FILE DONE` — file finished notification.
- `744 * <tid> :SAVED <path> (<seen>[/<declared>])` — server saved path and sizes.
- `745 * <tid> :HASH CRC32 <HEX>` — CRC32 of the streamed bytes (useful to verify integrity).
- `743 * <tid> :<reason>` — cancel acknowledgement.
- `400` — generic file-transfer error (e.g. cannot open source, write destination, invalid id).

Auto-stream (server reads a local file and streams it):

1. Sender runs: `FILESEND <nick> <size> :<filename>`
  - The server creates a transfer id `tid` and notifies the receiver.
2. Receiver runs: `FILEACCEPT <tid>`
  - The server will attempt to open a sanitized copy of `<filename>` relative to the server's working directory (it uses a sanitized basename). If the file exists on the server, the server will read it, base64-encode chunks and forward `740` lines to the receiver.
  - When finished, the server sends `741` (DONE), `744` (SAVED path and sizes) and `745` (CRC32 hash). The saved file path will look like `File Transfers/<tid>_<sanitized-filename>`.

Notes about auto-stream:
- The server's auto-stream reads from the server process CWD using a sanitized basename of the provided filename. For tests and local usage, place the file in the project root with a safe name (e.g. `demo.txt`) so the server can open it.
- The server always writes a copy into the `File Transfers/` folder (created on demand) as `File Transfers/<tid>_<safe>`; this is returned to both endpoints in the `744` numeric.

Legacy/manual mode (sender pushes base64):

1. Sender issues: `FILESEND <nick> <size> :<filename>`
2. Receiver accepts with: `FILEACCEPT <tid>`
3. Sender repeatedly sends: `FILEDATA <tid> <base64chunk>` for each encoded chunk. The server will forward each chunk to the receiver as `740` and append decoded bytes to the saved file.
4. When finished, sender issues: `FILEDONE <tid>` and both sides receive `741` and the server will report `744` and `745` as in auto-stream mode.

Examples (raw):

Sender (offer a 1024-byte file named demo.txt):
```
FILESEND Alice 1024 :demo.txt
```

Receiver (accept the transfer id 1):
```
FILEACCEPT 1
```

Legacy chunk push (sender):
```
FILEDATA 1 SGVsbG8gV29ybGQ=   # base64 chunk for "Hello World"
FILEDONE 1
```

If you see errors like `400` with messages such as "Cannot open source" or "Cannot write to destination", check that the server can access the file to stream (auto-stream) or that the sender is correctly pushing base64 (manual mode).
