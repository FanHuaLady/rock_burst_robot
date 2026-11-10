set confirm off
set pagination off
target extended-remote localhost:3333
monitor reset halt
load
tbreak Network_Poll
continue
printf "network snapshot\n"
print network_initialized
print network_link_state
print network_ms
print control_server_client_pcb
print control_server_mode
print control_server_handshake_complete
print control_server_last_valid_message_ms
print g_lwip_assert_count
print g_lwip_assert_text
monitor resume
detach
quit
