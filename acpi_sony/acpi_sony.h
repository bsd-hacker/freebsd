/* 
 * ACPI Sony Programmable I/O Control Device and 
 * Sony Notebook Controller driver for VAIO
  *
 * based on:
 *  - sony-laptop	-	Linux driver for Sony notebooks
  *						http://www.linux.it/~malattia/wiki/index.php/Sony-laptop
  *	- acpi_sony		- 	older SNC driver for FreeBSD
  *	- spic			- 	older SPIC driver for FreeBSD
  */

#ifndef _ACPI_SONY_H_
#define _ACPI_SONY_H_

#include <sys/param.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#define ACPI_SONY_VERBOSE

enum SPIC_TYPE {
	SPIC_TYPE1 = 0,
	SPIC_TYPE2 = 1,
	SPIC_TYPE3 = 2,
	SPIC_TYPE4 = 3,
	SPIC_ALL_TYPES = 4
};

/* event codes, almost the same as in sony-laptop */
#define SPIC_EVENT_NONE				 		255
#define SPIC_EVENT_JOGDIAL_DOWN		 		1
#define SPIC_EVENT_JOGDIAL_UP				2
#define SPIC_EVENT_JOGDIAL_DOWN_PRESSED	 	3
#define SPIC_EVENT_JOGDIAL_UP_PRESSED		4
#define SPIC_EVENT_JOGDIAL_PRESSED			5
#define SPIC_EVENT_JOGDIAL_RELEASED			6
#define SPIC_EVENT_CAPTURE_PRESSED			7
#define SPIC_EVENT_CAPTURE_RELEASED			8
#define SPIC_EVENT_CAPTURE_PARTIALPRESSED	9
#define SPIC_EVENT_CAPTURE_PARTIALRELEASED	10
#define SPIC_EVENT_FNKEY_ESC					11
#define SPIC_EVENT_FNKEY_F1					12
#define SPIC_EVENT_FNKEY_F2					13
#define SPIC_EVENT_FNKEY_F3					14
#define SPIC_EVENT_FNKEY_F4					15
#define SPIC_EVENT_FNKEY_F5					16
#define SPIC_EVENT_FNKEY_F6					17
#define SPIC_EVENT_FNKEY_F7					18
#define SPIC_EVENT_FNKEY_F8					19
#define SPIC_EVENT_FNKEY_F9					20
#define SPIC_EVENT_FNKEY_F10					21
#define SPIC_EVENT_FNKEY_F11					22
#define SPIC_EVENT_FNKEY_F12					23
#define SPIC_EVENT_FNKEY_1					24
#define SPIC_EVENT_FNKEY_2					25
#define SPIC_EVENT_FNKEY_D					26
#define SPIC_EVENT_FNKEY_E					27
#define SPIC_EVENT_FNKEY_F					28
#define SPIC_EVENT_FNKEY_S					29
#define SPIC_EVENT_FNKEY_B					30
#define SPIC_EVENT_BLUETOOTH_PRESSED			31
#define SPIC_EVENT_PKEY_P1					32
#define SPIC_EVENT_PKEY_P2					33
#define SPIC_EVENT_PKEY_P3					34
#define SPIC_EVENT_BACK_PRESSED				35
#define SPIC_EVENT_LID_CLOSED				36
#define SPIC_EVENT_LID_OPENED				37
#define SPIC_EVENT_BLUETOOTH_ON				38
#define SPIC_EVENT_BLUETOOTH_OFF				39
#define SPIC_EVENT_HELP_PRESSED				40
#define SPIC_EVENT_FNKEY_ONLY				41
#define SPIC_EVENT_JOGDIAL_FAST_DOWN			42
#define SPIC_EVENT_JOGDIAL_FAST_UP			43
#define SPIC_EVENT_JOGDIAL_FAST_DOWN_PRESSED	44
#define SPIC_EVENT_JOGDIAL_FAST_UP_PRESSED	45
#define SPIC_EVENT_JOGDIAL_VFAST_DOWN		46
#define SPIC_EVENT_JOGDIAL_VFAST_UP			47
#define SPIC_EVENT_JOGDIAL_VFAST_DOWN_PRESSED	48
#define SPIC_EVENT_JOGDIAL_VFAST_UP_PRESSED	49
#define SPIC_EVENT_ZOOM_PRESSED				50
#define SPIC_EVENT_ZOOM_IN_PRESSED			51
#define SPIC_EVENT_ZOOM_OUT_PRESSED			52
#define SPIC_EVENT_THUMBPHRASE_PRESSED		53
#define SPIC_EVENT_MEYE_FACE					54
#define SPIC_EVENT_MEYE_OPPOSITE				55
#define SPIC_EVENT_MEMORYSTICK_INSERT		56
#define SPIC_EVENT_MEMORYSTICK_EJECT			57
#define SPIC_EVENT_ANYBUTTON_RELEASED		58
#define SPIC_EVENT_BATTERY_INSERT			59
#define SPIC_EVENT_BATTERY_REMOVE			60
#define SPIC_EVENT_FNKEY_RELEASED			61
#define SPIC_EVENT_WIRELESS_ON				62
#define SPIC_EVENT_WIRELESS_OFF				63
#define SPIC_EVENT_TOGGLE_SPEED				64
#define SPIC_EVENT_TOGGLE_STAMINA			65

/* definitions of some additional methods */
#define ACPI_SONY_METHOD_FAN				0x0000
#define ACPI_SONY_METHOD_CAMERA_POWER	0x0001
#define ACPI_SONY_METHOD_BLUETOOTH_POWER 0x0002
#define ACPI_SONY_METHOD_WWAN_POWER 		0x0003

/* EC data offset to fan level */
#define SONY_EC_FANSPEED			0x93

/* PCI vendor definitions, used to determiny SPIC type model */
#define PCI_VENDOR_ID_INTEL				0x8086
#define PCI_DEVICE_ID_INTEL_82371AB_3	0x7113
#define PCI_DEVICE_ID_INTEL_ICH6_1		0x2641
#define PCI_DEVICE_ID_INTEL_ICH7_1		0x27b9
#define PCI_DEVICE_ID_INTEL_ICH8_4		0x2815

/* used to define entries for SPIC type detection */
#define SPIC_PCIID(_vid, _did, _type)		\
	{ 								\
		.vid	= PCI_VENDOR_ID_ ## _vid,	\
		.did	= PCI_DEVICE_ID_ ## _did,	\
		.type = _type				\
	}	

#define SPIC_PCIID_END()		\
	{						\
		.vid	= 0,				\
		.did	= 0,				\
		.type = 0				\
	}

struct spic_pciid  {
	uint16_t		vid;
	uint16_t		did;
	enum SPIC_TYPE	type;
};

const struct spic_pciid spic_pciids[] =
{
 	SPIC_PCIID(INTEL, INTEL_82371AB_3, SPIC_TYPE1),
	SPIC_PCIID(INTEL, INTEL_ICH6_1, SPIC_TYPE3),
	SPIC_PCIID(INTEL, INTEL_ICH7_1, SPIC_TYPE4),
	SPIC_PCIID(INTEL, INTEL_ICH8_4, SPIC_TYPE4),

	SPIC_PCIID_END()
};

/* mapping from SPIC code to event code */
struct spic_event {
	uint8_t		code;		/* code, returned by SPIC */
	uint8_t		event;		/* one of SPIC_EVENT_... */
};

struct acpi_sony_softc;
struct spic_event_group;

/* event handler for events */
typedef int (*acpi_sony_event_handler_t)(struct acpi_sony_softc *sc,  struct spic_event_group *group,
				uint8_t mask, uint8_t event);

/* default handler for events */
static int acpi_spic_default_handler(struct acpi_sony_softc *sc,  struct spic_event_group *group,
				uint8_t mask, uint8_t event);

/* defines group of events by event mask */
struct spic_event_group {
	uint8_t		mask;				/* event mask */
	struct spic_event	*events;		/* pointer to definition of events */
	acpi_sony_event_handler_t	handler;	/* optional handler of event */
};

/* used to define event group */
#define SPIC_EVENT_GROUP(_mask, _events, _handler)	\
	{ 								\
		.handler	= _handler,			\
		.mask	= _mask,				\
		.events	= _events			\
	}								

/* marker of groups definition end */ 
#define SPIC_END_GROUP()				\
	{ 								\
		.handler	= NULL,				\
		.mask	= 0,					\
		.events	= 0					\
	}								

/* used to define code <-> event mappings */
#define SPIC_EVENT(_code, _event)		\
	{ 								\
		.code	= _code,				\
		.event	= SPIC_EVENT_##_event \
	}								

/* marker of events description end */
#define SPIC_END_EVENTS()				\
	{ 								\
		.code	= 0,					\
		.event	= 0					\
	}								


/* NOTE:
 * event's codes and masks MUST BE in ascending order
 */

/* button release events */
const struct spic_event release_ev[] = {
	SPIC_EVENT(0x00, ANYBUTTON_RELEASED),
    
	SPIC_END_EVENTS()
};

/* jogddial events  */
const struct spic_event jogdial_ev[] = {
	SPIC_EVENT(0x01, JOGDIAL_DOWN),
	SPIC_EVENT(0x02, JOGDIAL_FAST_DOWN),
	SPIC_EVENT(0x03, JOGDIAL_VFAST_DOWN),
	
	SPIC_EVENT(0x1d, JOGDIAL_VFAST_UP),
	SPIC_EVENT(0x1e, JOGDIAL_FAST_UP),
	SPIC_EVENT(0x1f, JOGDIAL_UP),
	
	SPIC_EVENT(0x40, JOGDIAL_PRESSED),
	
	SPIC_EVENT(0x41, JOGDIAL_DOWN_PRESSED),
	SPIC_EVENT(0x42, JOGDIAL_FAST_DOWN_PRESSED),
	SPIC_EVENT(0x43, JOGDIAL_VFAST_DOWN_PRESSED),
	
	SPIC_EVENT(0x5d, JOGDIAL_VFAST_UP_PRESSED),
	SPIC_EVENT(0x5e, JOGDIAL_FAST_UP_PRESSED),
	SPIC_EVENT(0x5f, JOGDIAL_UP_PRESSED),
	
	SPIC_END_EVENTS()
};

/* capture button events */
const struct spic_event capture_ev[] = {
	SPIC_EVENT(0x01, CAPTURE_PARTIALRELEASED),
	SPIC_EVENT(0x05, CAPTURE_PARTIALPRESSED),
	SPIC_EVENT(0x07, CAPTURE_PRESSED),
	SPIC_EVENT(0x40, CAPTURE_PRESSED),
	
	SPIC_END_EVENTS()
};

/* Fn keys events */
const struct spic_event fnkey_ev[] = {
	SPIC_EVENT(0x10, FNKEY_ESC),
	SPIC_EVENT(0x11, FNKEY_F1),
	SPIC_EVENT(0x12, FNKEY_F2),
	SPIC_EVENT(0x13, FNKEY_F3),
	SPIC_EVENT(0x14, FNKEY_F4),
	SPIC_EVENT(0x15, FNKEY_F5),
	SPIC_EVENT(0x16, FNKEY_F6),
	SPIC_EVENT(0x17, FNKEY_F7),
	SPIC_EVENT(0x18, FNKEY_F8),
	SPIC_EVENT(0x19, FNKEY_F9),
	SPIC_EVENT(0x1a, FNKEY_F10),
	SPIC_EVENT(0x1b, FNKEY_F11),
	SPIC_EVENT(0x1c, FNKEY_F12),
	SPIC_EVENT(0x1f, FNKEY_RELEASED),
	SPIC_EVENT(0x21, FNKEY_1),
	SPIC_EVENT(0x22, FNKEY_2),
	SPIC_EVENT(0x31, FNKEY_D),
	SPIC_EVENT(0x32, FNKEY_E),
	SPIC_EVENT(0x33, FNKEY_F),
	SPIC_EVENT(0x34, FNKEY_S),
	SPIC_EVENT(0x35, FNKEY_B),
	SPIC_EVENT(0x36, FNKEY_ONLY),
	
	SPIC_END_EVENTS()
};

/* program key events */
const struct spic_event pkey_ev[] = {
	SPIC_EVENT(0x01, PKEY_P1),
	SPIC_EVENT(0x02, PKEY_P2),
	SPIC_EVENT(0x03, TOGGLE_STAMINA),
	SPIC_EVENT(0x04, PKEY_P3),
	
	SPIC_END_EVENTS()
};

/* bluetooth events */
const struct spic_event bluetooth_ev[] = {
	SPIC_EVENT(0x55, BLUETOOTH_PRESSED),
	SPIC_EVENT(0x59, BLUETOOTH_ON),
	SPIC_EVENT(0x5a, BLUETOOTH_OFF),
    
	SPIC_END_EVENTS()
};

/* wireless events */
const struct spic_event wireless_ev[] = {
	SPIC_EVENT(0x59, WIRELESS_ON),
	SPIC_EVENT(0x5a, WIRELESS_OFF),
    
	SPIC_END_EVENTS()
};

/* back button events */
const struct spic_event back_ev[] = {
	SPIC_EVENT(0x20, BACK_PRESSED),
    
	SPIC_END_EVENTS()
};

/* help button events */
const struct spic_event help_ev[] = {
	SPIC_EVENT(0x3b, HELP_PRESSED),
    
	SPIC_END_EVENTS()
};

/* lid events */
const struct spic_event lid_ev[] = {
	SPIC_EVENT(0x50, LID_OPENED),
	SPIC_EVENT(0x51, LID_CLOSED),
	
	SPIC_END_EVENTS()
};

/* zoom events */
const struct spic_event zoom_ev[] = {
	SPIC_EVENT(0x10, ZOOM_IN_PRESSED),
	SPIC_EVENT(0x20, ZOOM_OUT_PRESSED),
	SPIC_EVENT(0x39, ZOOM_PRESSED),
    
	SPIC_END_EVENTS()
};

/* thumbphrase events */
const struct spic_event thumbphrase_ev[] = {
	SPIC_EVENT(0x3a, THUMBPHRASE_PRESSED),
	
	SPIC_END_EVENTS()
};

/* motioneye camera events */
const struct spic_event meye_ev[] = {
	SPIC_EVENT(0x00, MEYE_FACE),
	SPIC_EVENT(0x01, MEYE_OPPOSITE),
    
	SPIC_END_EVENTS()
};

/* memorystick events */
const struct spic_event memorystick_ev[] = {
	SPIC_EVENT(0x53, MEMORYSTICK_INSERT),
	SPIC_EVENT(0x54, MEMORYSTICK_EJECT),
	
	SPIC_END_EVENTS()
};

/* battery events */
const struct spic_event battery_ev[] = {
	SPIC_EVENT(0x20, BATTERY_INSERT),
	SPIC_EVENT(0x30, BATTERY_REMOVE),

	SPIC_END_EVENTS()
};

/* type4 extra events */
const struct spic_event type4_extra_ev[] = {
	SPIC_EVENT(0x5c, NONE),
	SPIC_EVENT(0x5f, NONE),
	SPIC_EVENT(0x61, NONE),
	SPIC_END_EVENTS()
};

/* type4 extra event handler */
static int acpi_spic4_extra_handler(struct acpi_sony_softc *sc,
			struct spic_event_group *group, uint8_t mask, uint8_t event);

/* type4 extra event handler */
static int acpi_spic4_pkey_handler(struct acpi_sony_softc *sc,
			struct spic_event_group *group, uint8_t mask, uint8_t event);

/* type1 event groups */
const struct spic_event_group spic_type1_events[] = {
	SPIC_EVENT_GROUP(0x00, release_ev, NULL),
	SPIC_EVENT_GROUP(0x10, jogdial_ev, NULL),
	SPIC_EVENT_GROUP(0x20, fnkey_ev, NULL),
	SPIC_EVENT_GROUP(0x30, bluetooth_ev, NULL),
	SPIC_EVENT_GROUP(0x30, lid_ev, NULL),
	SPIC_EVENT_GROUP(0x30, memorystick_ev, NULL),
	SPIC_EVENT_GROUP(0x40, pkey_ev, NULL),
	SPIC_EVENT_GROUP(0x40, battery_ev, NULL),
	SPIC_EVENT_GROUP(0x60, capture_ev, NULL),
	SPIC_EVENT_GROUP(0x70, meye_ev, NULL),
	
    	SPIC_END_GROUP()
};

/* type2 event groups */
const struct spic_event_group spic_type2_events[] = {
	SPIC_EVENT_GROUP(0x00, release_ev, NULL),
	SPIC_EVENT_GROUP(0x08, pkey_ev, NULL),
	SPIC_EVENT_GROUP(0x11, jogdial_ev, NULL),
	SPIC_EVENT_GROUP(0x11, back_ev, NULL),
	SPIC_EVENT_GROUP(0x20, thumbphrase_ev, NULL),
	SPIC_EVENT_GROUP(0x21, fnkey_ev, NULL),
	SPIC_EVENT_GROUP(0x21, help_ev, NULL),
	SPIC_EVENT_GROUP(0x21, zoom_ev, NULL),
	SPIC_EVENT_GROUP(0x31, bluetooth_ev, NULL),
	SPIC_EVENT_GROUP(0x31, pkey_ev, NULL),
	SPIC_EVENT_GROUP(0x31, memorystick_ev, NULL),
	SPIC_EVENT_GROUP(0x38, lid_ev, NULL),
	SPIC_EVENT_GROUP(0x41, battery_ev, NULL),
	SPIC_EVENT_GROUP(0x61, capture_ev, NULL),

	SPIC_END_GROUP()
};

/* type3 event groups */
const struct spic_event_group spic_type3_events[] = {
	SPIC_EVENT_GROUP(0x00, release_ev, NULL),
	SPIC_EVENT_GROUP(0x21, fnkey_ev, NULL),
	SPIC_EVENT_GROUP(0x31, pkey_ev, NULL),
	SPIC_EVENT_GROUP(0x31, wireless_ev, NULL),
	SPIC_EVENT_GROUP(0x31, memorystick_ev, NULL),
	SPIC_EVENT_GROUP(0x41, battery_ev, NULL),
	
	SPIC_END_GROUP()
};

/* type4 event groups */
const struct spic_event_group spic_type4_events[] = {
	SPIC_EVENT_GROUP(0x00, release_ev, NULL),
    	SPIC_EVENT_GROUP(0x05, pkey_ev, acpi_spic4_pkey_handler),
	SPIC_EVENT_GROUP(0x05, zoom_ev, NULL),
	SPIC_EVENT_GROUP(0x05, capture_ev, NULL),
	SPIC_EVENT_GROUP(0x21, fnkey_ev, NULL),
	SPIC_EVENT_GROUP(0x31, wireless_ev, NULL),
	SPIC_EVENT_GROUP(0x31, memorystick_ev, NULL),
	SPIC_EVENT_GROUP(0x31, type4_extra_ev, acpi_spic4_extra_handler),
	SPIC_EVENT_GROUP(0x41, battery_ev, NULL),
	    
	SPIC_END_GROUP()
};

const struct   {
	uint16_t				evport_offset;
	struct spic_event_group	*events;
} spic_types[SPIC_ALL_TYPES] = {
	{ 0x04, spic_type1_events },
	{ 0x12, spic_type2_events },
	{ 0x12, spic_type3_events },
	{ 0x12, spic_type4_events }
};

/* used to define SNC oids */
#define SNC_OID(_get, _set, _name, _descript)	\
	{ 										\
		.nodename	= _name,					\
		.getmethod	= _get,					\
		.setmethod	= _set,					\
		.comment = _descript					\
	}

/* ACPI Methods, for which oids may be created */
const struct {
	char *nodename;
	char *getmethod;
	char *setmethod;
	char *comment;
} acpi_snc_oids[] = {
	SNC_OID("GBRT", "SBRT", "brightness", "Display brightness"),
	SNC_OID("GPBR", "SPBR", "def_brightness", "Display default brightness"),
	SNC_OID("GCTR", "SCTR", "contrast", "Display contrast"),
	SNC_OID("GPCR", "SPCR", "def_contrast", "Display default contrast"),
	SNC_OID("GAZP", "AZPW", "power_audio", "Audio device power"),
	SNC_OID("GLNP", "LNPW", "power_lan", "Ethernet device power"),
	SNC_OID("GWDP", NULL, "power_wlan", "Wireless device power"),
	SNC_OID("GCDP", "CDPW", "power_cd", "CD power"),
	SNC_OID("GCDP", "SCDP", "power_cd", "CD power"), 	/* alternative combination */
	SNC_OID("GLID", NULL, "lid_status", "LID status"),
	SNC_OID("GHKE", NULL,	"hot_key", "Hot key status"),
	SNC_OID("GILS", "SILS", "indicator", "Lamp indicator status"),
	SNC_OID("GMCB", "CMGB", "gain_bass", "Gain bass"),
	SNC_OID("GPID", NULL,	"processor_id", "Processor id (if applicable)"),
	/* experimental */
	
	/* end of table */
	{ NULL, NULL, NULL, NULL }
};

/* used to define SPIC oids */
#define SPIC_OID(_name, _method, _flags, _descript)	\
	{ 											\
		.name	= _name,							\
		.method	= ACPI_SONY_METHOD_##_method,	\
		.access	= _flags,						\
		.comment = _descript						\
	}

/* additional oids */
const struct {
	char	*name;
	int	method;
	int	access;
        char	*comment;
} acpi_spic_oids[] = {
	SPIC_OID("fan_speed", FAN, CTLTYPE_INT | CTLFLAG_RW, "Fan speed"),
	SPIC_OID("power_bluetooth", BLUETOOTH_POWER, CTLTYPE_INT | CTLFLAG_RW, "Bluetooth power state"),
	   
	{ NULL, 0, 0, NULL }
};

/* driver structure for device */
struct acpi_sony_softc {

	/* device and it's ACPI handle */
	device_t	dev;
	ACPI_HANDLE	handle;

	/* embedded controller and it's ACPI handle*/
	device_t	ec_dev;
	ACPI_HANDLE	ec_handle;
    
	/* 1 if device is SPIC*/
	int			is_spic;

	// SPIC related only
	enum SPIC_TYPE			model;
	uint16_t        			evport_offset;
	struct spic_event_group	*events;

	struct sysctl_ctx_list	sysctl_ctx;
	struct sysctl_oid       	*sysctl_tree;

	uint8_t         				power_bluetooth;
	
	uint8_t					prev_event;
	/* start of IO ports range */
	uint16_t				port_addr;
	/* IRQ */
	uint8_t					intr;
	uint8_t					open_count;
    
	/* resource allocation related */
	struct resource			*port_res;	/* IO ports resource */
	struct resource			*intr_res;	/* IRQ resource */
	int     					port_rid;	/* IO ports resource id */
	int						intr_rid;	/* IRQ resource id */
	
	void					*icookie;	/* coockie for interrupt */
    
	/* Received via ACPI walking resources */
	ACPI_RESOURCE_IO		io;
	ACPI_RESOURCE_IRQ		interrupt;
 };

#define SPIC_DESCRIPT(event)	\
	{						\
		.code = SPIC_EVENT_ ## event,		\
		.name = #event,		\
	}

#ifdef ACPI_SONY_VERBOSE
const struct {
	uint8_t	code;
	char	*name;
} event_descriptors[] =
{
	SPIC_DESCRIPT(JOGDIAL_DOWN),
	SPIC_DESCRIPT(JOGDIAL_UP),
	SPIC_DESCRIPT(JOGDIAL_DOWN_PRESSED),
	SPIC_DESCRIPT(JOGDIAL_UP_PRESSED),
	SPIC_DESCRIPT(JOGDIAL_PRESSED),
	SPIC_DESCRIPT(JOGDIAL_RELEASED),
	SPIC_DESCRIPT(CAPTURE_PRESSED),
	SPIC_DESCRIPT(CAPTURE_RELEASED),
	SPIC_DESCRIPT(CAPTURE_PARTIALPRESSED),
	SPIC_DESCRIPT(CAPTURE_PARTIALRELEASED),
	SPIC_DESCRIPT(FNKEY_ESC),
	SPIC_DESCRIPT(FNKEY_F1),
	SPIC_DESCRIPT(FNKEY_F2),
	SPIC_DESCRIPT(FNKEY_F3),
	SPIC_DESCRIPT(FNKEY_F4),
	SPIC_DESCRIPT(FNKEY_F5),
	SPIC_DESCRIPT(FNKEY_F6),
	SPIC_DESCRIPT(FNKEY_F7),
	SPIC_DESCRIPT(FNKEY_F8),
	SPIC_DESCRIPT(FNKEY_F9),
	SPIC_DESCRIPT(FNKEY_F10),
	SPIC_DESCRIPT(FNKEY_F11),
	SPIC_DESCRIPT(FNKEY_F12),
	SPIC_DESCRIPT(FNKEY_1),
	SPIC_DESCRIPT(FNKEY_2),
	SPIC_DESCRIPT(FNKEY_D),
	SPIC_DESCRIPT(FNKEY_E),
	SPIC_DESCRIPT(FNKEY_F),
	SPIC_DESCRIPT(FNKEY_S),
	SPIC_DESCRIPT(FNKEY_B),
	SPIC_DESCRIPT(BLUETOOTH_PRESSED),
	SPIC_DESCRIPT(PKEY_P1),
	SPIC_DESCRIPT(PKEY_P2),
	SPIC_DESCRIPT(PKEY_P3),
	SPIC_DESCRIPT(BACK_PRESSED),
	SPIC_DESCRIPT(LID_CLOSED),
	SPIC_DESCRIPT(LID_OPENED),
	SPIC_DESCRIPT(BLUETOOTH_ON),
	SPIC_DESCRIPT(BLUETOOTH_OFF),
	SPIC_DESCRIPT(HELP_PRESSED),
	SPIC_DESCRIPT(FNKEY_ONLY),
	SPIC_DESCRIPT(JOGDIAL_FAST_DOWN),
	SPIC_DESCRIPT(JOGDIAL_FAST_UP),
	SPIC_DESCRIPT(JOGDIAL_FAST_DOWN_PRESSED),
	SPIC_DESCRIPT(JOGDIAL_FAST_UP_PRESSED),
	SPIC_DESCRIPT(JOGDIAL_VFAST_DOWN),
	SPIC_DESCRIPT(JOGDIAL_VFAST_UP),
	SPIC_DESCRIPT(JOGDIAL_VFAST_DOWN_PRESSED),
	SPIC_DESCRIPT(JOGDIAL_VFAST_UP_PRESSED),
	SPIC_DESCRIPT(ZOOM_PRESSED),
	SPIC_DESCRIPT(ZOOM_IN_PRESSED),
	SPIC_DESCRIPT(ZOOM_OUT_PRESSED),
	SPIC_DESCRIPT(THUMBPHRASE_PRESSED),
	SPIC_DESCRIPT(MEYE_FACE),
	SPIC_DESCRIPT(MEYE_OPPOSITE),
	SPIC_DESCRIPT(MEMORYSTICK_INSERT),
	SPIC_DESCRIPT(MEMORYSTICK_EJECT),
	SPIC_DESCRIPT(ANYBUTTON_RELEASED),
	SPIC_DESCRIPT(BATTERY_INSERT),
	SPIC_DESCRIPT(BATTERY_REMOVE),
	SPIC_DESCRIPT(FNKEY_RELEASED),
	SPIC_DESCRIPT(WIRELESS_ON),
	SPIC_DESCRIPT(WIRELESS_OFF),
	SPIC_DESCRIPT(TOGGLE_SPEED),
	SPIC_DESCRIPT(TOGGLE_STAMINA)
};

#endif

#endif	/* _ACPI_SONY_H_ */
