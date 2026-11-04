# Protocol and capability review

The descriptor/transport-ID audits pass against official master 08ac9b7ee87ad996eb7b4238bc57f160d0b01fce and stable v1.5.939 (dfbac8d3d9c967003e74b4ffc5610f7d45e8f666). Existing field names, tags, types, labels, defaults and enum values are unchanged. Added fields in existing and new messages are optional or repeated; no required fields were added. TCP FileTransferControl=27 and FileData=28 and protobuf UDP Video=2 do not collide with either reference.

Official master and stable have no root PROTOCOL.md; the fork document describes additional file-transfer messages and optional capabilities. The merge changes neither that document nor either .proto file. Handler requirements for payload/ID/index/data are semantic validation, not required protobuf fields.

ProtoUtils.cpp still sends standard version_v1 and version_v2 encodings. The merge changes no Version/ProtoUtils/MumbleProtocol code. UDP framing still switches at the official 1.5 protobuf boundary. Fork-only capability is advertised through optional ServerConfig fields, not a synthetic standard protocol version.

MainWindow starts video relay support disabled, clears it on connect/disconnect, and enables it only after ServerConfig.video_relay_version >= 1. FileTransferManager similarly starts/resets with relay support disabled and enables it only after file_transfer_relay_version >= 1. Identity setup also requires relay support and an enabled file-sharing setting. Partial ServerConfig messages retain negotiated state. Official servers omit both capability fields; mixed GUI fixtures check that file/video controls stay disabled without identity prompts. The fork-positive fixture checks the enabled path and receipt-capable wire announcement.

The database implementation and schema migrations are byte-identical to origin/master. The incoming server changes only rename macOS preprocessor guards. Database tests distinguish upgrade acceptance from unsupported schema downgrade.
