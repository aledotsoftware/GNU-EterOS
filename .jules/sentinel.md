## 2024-05-22 - [DHCP Buffer Overflow]
**Vulnerability:** Inline DHCP options parsing without proper bounds checking against the actual received packet length.
**Learning:** Network protocols often define minimum structure sizes but allow variable length fields (options). Assuming a struct fits the buffer is dangerous. The code relied on `sizeof(struct dhcp_packet)` which includes a fixed-size options array (308 bytes), but real packets can be shorter or longer.
**Prevention:** Always parse variable-length fields using a dedicated function that takes the *actual* received buffer length as a hard limit, not the expected struct size. Use pointer arithmetic carefully to validate every read.
