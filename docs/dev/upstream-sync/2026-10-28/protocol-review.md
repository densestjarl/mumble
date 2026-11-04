Protocol review against official master 162f42805 and stable v1.5.939:

- Descriptor checks pass: existing field numbers, types, labels, defaults and enum values are preserved; extensions add no required fields.
- TCP additions are FileTransferControl=27 and FileData=28; UDP Video=2. No collision with either official reference.
- PROTOCOL.md is fork-only; upstream has no file to overwrite. Its documented extension fields and transport IDs agree with Mumble.proto, MumbleUDP.proto and MumbleProtocol.h.
- Version.cpp and Version.h are identical to upstream master. The standard Version message uses MumbleProto::setVersion; extension support uses optional ServerConfig fields rather than interpreting the advertised official version as a fork capability.
- MainWindow starts/resets video support to false and enables sending on ServerConfig.video_relay_version. File-transfer setup is gated by the negotiated relay capability. The GUI harness verifies disabled sending and absence of identity prompts against official servers, and a fork-to-fork positive fixture checks availability.
- The video relay excludes pre-1.5 clients whose legacy UDP parser could interpret a protobuf type byte as audio. Modern official parsers reject unknown UDP types without changing the connection. File records require explicit file capability and receipt capability on sender and recipients.

Scope: static review plus executable descriptor checks; actual interop results are recorded separately.
