#pragma once
// Start the Phase 1 server. If strict_ts is true, enforce strictly increasing timestamps.
void start_server(int port, bool strict_ts = true);
