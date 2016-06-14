typedef struct uds_server_s uds_server_t;

uds_server_t *start_uds_server(const char *socket_path);

void stop_uds_server(uds_server_t *server);
