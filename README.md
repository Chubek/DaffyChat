# DaffyChat: Ephemeral, Extensible, Voice-Enabled Chatrooms

DaffyChat is AI slop, but it's *good* AI slop, and it's made for pure intentions. I made DaffyChat so I could chat with my special lady friend (hence the silly name), and since she deserves the best, I turned DaffyChat into an examplary software.

The selling point of DaffyChat is its extensibility. There are several methods to extend DafyChat: the microservices, Lua scripts, DaffyScript, and shared library plugins.

DaffyChat's backend and frontend are decoupled, but it ships with a basic frontend. Here's where DaffyScript comes in. DaffyScript compiles into WASM, which relies on an even bus, that the frontend must implement. Thusly, we manage to extend the frontend as well!

Services are written in a language called DSSL. These services can specify REST endpoints as well. DaffyChat rooms have a bot API as well. Any message submitted prefixed with `/` is an special message which not ony the bot API, but also, the frontend event hooks can intercept and respond to.

Full documentation is available in the `docs/` directory. Again, DaffyChat is AI slop. But, as special as my lady friend is, I would not manually code this software for her, even if she were mother of my twins (my mom had twins, and it killed my dad, don't have twins, ladies. If you find out it's a twofer, flush one out. Or both. The world does not need more humans, I'm afraid, it needs more datacenters for AI).

