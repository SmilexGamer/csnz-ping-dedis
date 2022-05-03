# csnz-ping-dedis
 A tool used to send server queries to CSO/CSNZ dedicated servers

# Thanks
 AnggaraNothing, for finding the header used in CSO server queries
 
 3p3r, for quick and dirty UDP send function - https://gist.github.com/3p3r/a7e534164c6d838a107f96d7b2f5f1dd#file-udp_send-cpp
 
# Usage
 csnz-ping-dedis <ip> <port_start> [query_type=A2A_PING]

 Query Types: 0 - A2S_INFO; 1 - A2S_PLAYER; 2 - A2S_RULES; 3 - A2A_PING; 4 - A2S_SERVERQUERY_GETCHALLENGE
