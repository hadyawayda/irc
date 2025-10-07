# helperbot – user guide

Type `!commands` to see the list of commands. Use commands in a channel or in a DM with the bot unless noted.

## Quick list

- `!help [topic]` – show docs. Topics include: help, about, ping, echo, who, modes, roll, 8ball, choose, seen, remind, poll, calc, uptime, mode, invite, topic  
- `!commands` – short list of commands
- `!about` – what the bot can do
- `!ping` – pong
- `!echo <text>` – repeat text
- `!who` – list users in the current channel (ops prefixed by `@`)
- `!modes` – ask the server to show modes (sends `MODE #chan`, you’ll get `324` reply)
- `!roll [XdY]` – roll dice (default 1d6)
- `!8ball <question>` – magic eightball
- `!choose a | b | c` – pick a random option
- `!seen <nick>` – when `<nick>` last spoke in this channel
- `!remind <in> <message>` – reminder; duration like `10s`, `5m`, `2h30m`, `1d2h`
- `!poll new Question | Option1 | Option2 [...]` – create poll (channel only)
- `!poll vote <id> <n>` – vote in poll `<id>` for option number `<n>`
- `!poll show <id>` – show poll status
- `!poll close <id>` – close poll and display results
- `!calc <expr>` – integer calc, `+ - * /` and parentheses
- `!uptime` – bot uptime

## Mode / Invite / Topic docs

- **Channel modes**:  
  `+i` invite-only, `+t` topic by ops only, `+k <key>`, `+l <limit>`  
  Examples:  
  - `/MODE #room +t` (only ops can set topic)  
  - `/MODE #room +k hunter2` (set a join key)  
  - `/MODE #room -k` (clear key)  
  - `/MODE #room +l 42` (limit to 42 users)

- **Invite**: ops can `/INVITE <nick> <#room>`. If the room is `+i`, the user can then `/JOIN`.

- **Topic**: `/TOPIC #room :New topic`. With `+t`, only ops can change it.

> The bot doesn’t perform admin actions like `kick` or setting modes; it only assists with info, utilities, and coordination.
