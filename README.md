# DaffyChat: Ephemeral, Extensible, Voice-Enabled Chatrooms

DaffyChat is an application that provides ephemeral, voice-enabled chatrooms. DaffyChat rooms are *extensible*, meaning, there are several ways you can extend your experience with a room. Each DaffyChat room runs on a separate container, isolated from the system. Whenever you prefix a message with `/`, that becomes a special message, which the many extensibiity methods of DaffyChat can intercept.

There are several extension methods offered by DaffyChat, server-side and client-side. 

**Client-Side Extensibility**
- The frontend event bus. You can write a backend extension that publishes an event, intercepted by the frontend Event Bridge. You can respond to these events in any way you want;
- WebAssembly modules. You can write a program, compile it to WASM, and inject it to your chatroom. DaffyChat offers a language called "Daffyscript" which compiles directly to DaffyChat-conformat WASM modules;
- You can start a room with a "Room Recipe". These recipes are writen in DaffyScript, again. A Room Recipe specifies how a room should function, esepcially vis-a-vis the extensions;
- The APIs. DaffyChat rooms have access to many REST APIs, e.g. the Bot API. These services are defined using DSSL, or the Daffy Service Specification Language. We'll discusss them momentarily.

**Server-Side Extensiblity**
- The services. We write a DaffyChat service in the aformentioned DSSL. We then daemonize the service, and launch it. There's a central daemon manager, because otherwise, it would be impossible. Daemonizing every service stops us from having to create a systemd spec for each service;
- Lua. The classic extension language, which might as well be embedded into my eyeglasses. With Lua, you can do many things. There are several libraries provided that help the task realized;
- Plugins, via the DaffyChat plugin API. We use the interface declared in the plugins header file to write a program, compile it to a shared library, and inject it into DaffyChat's runtime;

**Can I use DaffyChat yet?** Not right now. Some kinks need to be fixed, and some chinks need to be untied off the chain. I wrote DaffyChat to chat with someone very important to me. That is why I want it finished soon.
