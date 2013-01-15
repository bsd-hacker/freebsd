#ifndef BSDBUS_H
#define BSDBUS_H

#include <stdint.h>
#include <stdio.h>

enum DBus_connection_state {
	DBUS_CONNECTED = 0,
	DBUS_DISCONNECTED = 1
};

enum DBus_connection_type {
	DBUS_SYSTEM_BUS = 0,
	DBUS_SESSION_BUS = 1,
	DBUS_ALL_KNOWN_BUS = 2
};

const char DBus_connection_str[DBUS_ALL_KNOWN_BUS][32] = {
	"DBUS_SYSTEM_BUS_ADDRESS",
	"DBUS_SESSION_BUS_ADDRESS"
};

const char DBus_default_path[DBUS_ALL_KNOWN_BUS][32] = {
	"/var/run/dbus/system-bus-socket",
	"/var/run/dbus/session-bus-socket"
};

enum DBus_communication_state {
	DBUS_COMMUNICATION_FAILED = 0,
	DBUS_COMMUNICATION_OK = 1
};

struct DBus_connection
{
	enum DBus_connection_type	type;
	enum DBus_connection_state	state;
	FILE						*fd;
	int							socket;
};

enum DBus_param_type
{
	DBUS_PARAM_INVALID = 0,
	DBUS_PARAM_BYTE = 1,
	DBUS_PARAM_BOOLEAN = 2,
	DBUS_PARAM_INT16 = 3,
	DBUS_PARAM_UINT16 = 4,
	DBUS_PARAM_INT32 = 5,
	DBUS_PARAM_UINT32 = 6,
	DBUS_PARAM_INT64 = 7,
	DBUS_PARAM_UINT64 = 8,
	DBUS_PARAM_DOUBLE = 9,
	DBUS_PARAM_STRING = 10,
	DBUS_PARAM_OBJECT_PATH = 11,
	DBUS_PARAM_SIGNATURE = 12,
	DBUS_PARAM_ARRAY = 13,
	DBUS_PARAM_STRUCT = 14,
	DBUS_PARAM_VARIANT = 15,
	DBUS_PARAM_DICT_ENTRY = 16,
	DBUS_ALL_TYPES = 17
};
struct DBus_body_elem
{
	struct DBus_body_elem	*next;
	enum DBus_param_type		type;
	void					*data;
	size_t					size;
};

struct DBus_message {
	// outcoming message data
	struct DBus_body_elem	*start;
	struct DBus_body_elem	*tail;
	
	// raw message data
	size_t					size;
	void					*data;
	
	// raw incoming data
	const uint8_t		    *body;
	const uint8_t		    *header;
	
	size_t					elem_count;
};

struct DBus_connection		*dbus_connect(enum DBus_connection_type conType);
struct DBus_connection		*dbus_open_connect(struct DBus_connection * connection, const char *path);
void						dbus_disconnect(struct DBus_connection *connection);

enum DBus_communication_state	dbus_send(struct DBus_connection *connection,
							struct DBus_message *message,
						    const char *path, const char *interface, const char *name);

void						dbus_free_message(struct DBus_message *message);
void						*dbus_format_message(struct DBus_message *message, int pad);
struct DBus_message			*dbus_alloc_message();

void						dbus_push_elem(struct DBus_message *message, enum DBus_param_type type, void *data, size_t size);
void						dbus_push_byte(struct DBus_message *message, uint8_t val);
void						dbus_push_boolean(struct DBus_message *message, uint32_t val);
void						dbus_push_int16(struct DBus_message *message, int16_t val);
void						dbus_push_uint16(struct DBus_message *message, uint16_t val);
void						dbus_push_int32(struct DBus_message *message, int32_t val);
void						dbus_push_uint32(struct DBus_message *message, uint32_t val);
void						dbus_push_int64(struct DBus_message *message, int64_t val);
void						dbus_push_uint64(struct DBus_message *message, uint64_t val);
void						dbus_push_double(struct DBus_message *message, double val);
void						dbus_push_string(struct DBus_message *message, const char *val);
void						dbus_push_object_path(struct DBus_message *message, const char *val);
void						dbus_push_signature(struct DBus_message *message, const char *val);
void						dbus_push_struct_align(struct DBus_message *message);

#define RETURN_IF(x)		(if (x) return);

enum DBus_endian {
	DBUS_BIG_ENDIAN 		= (uint8_t)'B',
	DBUS_LITTLE_ENDIAN 	= (uint8_t)'l'
};

enum DBus_message_type {
	DBUS_MESSAGE_INVALID = 0,
	DBUS_METHOD_CALL = 1,
	DBUS_METHOD_RETURN = 2,
	DBUS_ERROR = 3,
	DBUS_SIGNAL = 4
};

enum DBus_message_flags {
	DBUS_NO_REPLY_EXPECTED = 0x1,
	DBUS_NO_AUTO_START = 0x2
};

const uint8_t DBUS_PROTOCOL_VERSION = 1;

struct DBus_message_header
{
	uint8_t	endyFlag;
	uint8_t	type;
	uint8_t	flags;
	uint8_t	proto;
	uint32_t length;
	uint32_t serial;
	uint32_t arrSize;
	/* ended by array of struct (byte, variant) */
};

struct DBus_tube
{
	struct DBus_connection      *connection;
	struct DBus_message_header  header;
	struct DBus_message		    *header_fields;
};

struct DBus_tube *dbus_signal_tube(struct DBus_connection *connection,
					const char *path, const char *interface, char *signature);
					
void			 dbus_tube_send(struct DBus_tube *tube,
					const char *name,
				    struct DBus_message *message);
				    
void			 dbus_tube_close(struct DBus_tube_open);

enum DBus_field_code {
	DBUS_INVALID = 0,
	DBUS_PATH = 1, 
	DBUS_INTERFACE = 2,
	DBUS_MEMBER	= 3,
	DBUS_ERROR_NAME	= 4,
	DBUS_REPLY_SERIAL = 5,
	DBUS_DESTINATION = 6,
	DBUS_SENDER	= 7,
	DBUS_SIGNATURE = 8,
	DBUS_ALL_FIELDS = 9
};

const size_t    DBus_field_align[DBUS_ALL_TYPES] = { 0, 1, 4, 2, 2, 4, 4, 8, 8, 8, 4, 4, 1, 4, 8, 1, 8 };
const uint8_t   DBus_param_code[DBUS_ALL_TYPES] = { 0, 121, 98, 110, 113, 105, 117, 120, 116, 100, 115, 111, 103, 97, 114, 118, 101 };

const char DBUS_INTERFACE_DBUS[] = "org.freedesktop.DBus";
const char DBUS_SERVICE_DBUS[] = "org.freedesktop.DBus";
const char DBUS_PATH_DBUS[] = "/org/freedesktop/DBus";

const int DBUS_NO_PADDING = 0;

#endif /* BSDBUS_H */

