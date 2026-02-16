# FIXpipelineSender
A low-latency FIX order sender prototype designed for lock-free messaging, thread isolation, and raw TCP throughput for trading systems.

Test cases cover: Normal valid messages, Long fields, Garbage before message, Partial messages, Malformed BodyLength, Wrong checksum, Multiple concatenated messages, Reordered fields, A large message.
Works with FIXpipelineReceiver.
