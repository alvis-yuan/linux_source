/*! \file print_api.h
 *  \brief Header file for the APIs to access the print server.
 */

#ifndef __PRINT_API_H__
#define __PRINT_API_H__

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <semaphore.h>
#include <time.h>

#define TPH_TEMPERATURE_NODE		"/sys/class/sunmi/base/tph_temperature"
#define TPH_VOLTAGE_NODE			"/sys/class/sunmi/base/tph_voltage_adc"
#define TPH_CASH_SW_NODE			"/sys/class/sunmi/base/gpio_cash_sw"
#define TPH_MOTOR_NTC_NODE			"/sys/class/sunmi/base/tph_ntc_temperature"
#define TPH_HALL_PAPER_SIZE_NODE	"/sys/class/sunmi/base/tph_paper_size"
#define TPH_HALL_HWID_ADC_NODE		"/sys/class/sunmi/base/hwid_adc"
#define TPH_PAPERTAKE_ADC_NODE		"/sys/class/sunmi/base/tph_papertake_adc"
#define PRINTER_DOT_PER_LINE		sys_global_var()->prt_dots_per_line //打印机一行的点数
#define SUNMI_PRINTER_DENSITY		(sys_global_var()->prt_dots_per_line==384?"Standard 384 dots":"Standard 576 dots")
#define SUNMI_PRINTER_SPEED			(sys_global_var()->prt_dots_per_line==384?"Max 160mm/s":"Max 250mm/s")
#define BAD_POINT_INF_FILE			"/data/PUB/bad_point_inf.txt"
#define BAD_POINT_CHECK_NUM			32 // 开机第一次做PRINTER_DOT_PER_LINE长度的检查，后续每次只做BAD_POINT_CHECK_NUM点数，为实现方便，目前只支持设为32的倍数

typedef struct {
	int offset;
	int chk_dots;
} badpoint_info_hdr_t;

/*! \enum PRINT_CHN_ID_xxx
 *  \brief Print channel IDs.
 */
enum {
	PRINT_CHN_ID_INT = 0,			/*!< Internal (PRINT_CHN_FLAG_NO_REPRINT, 200ms) */
	PRINT_CHN_ID_HTTPS,				/*!< Order data received with HTTPS (200ms) */
	PRINT_CHN_ID_MQTT,				/*!< Order data received with MQTT (200ms) */
	PRINT_CHN_ID_USB,				/*!< USB (200ms) */
	PRINT_CHN_ID_SPP,				/*!< Bluetooth SPP (600ms) */
	PRINT_CHN_ID_SPP2,				/*!< Bluetooth SPP (600ms) */
	PRINT_CHN_ID_SPP3,				/*!< Bluetooth SPP (600ms) */
	PRINT_CHN_ID_SPP4,				/*!< Bluetooth SPP (600ms) */
	PRINT_CHN_ID_BLE,				/*!< Bluetooth BLE (600ms) */
	PRINT_CHN_ID_TCP_BEGIN,			/*!< TCP Socket (WLAN or LAN) (600ms) */
#define NUMBER_OF_TCP_CHN 32
	PRINT_CHN_ID_TCP_END = PRINT_CHN_ID_TCP_BEGIN + NUMBER_OF_TCP_CHN - 1,
	PRINT_CHN_ID_SERIAL,			/*!< Serial (600ms) */
	PRINT_CHN_ID_AM,				/*!< Partner Applications (600ms) */
	PRINT_CHN_ID_CGI,				/*!< Web (600ms) */
	PRINT_CHN_ID_CMD,				/*!< Commands (for internal use) */
	PRINT_CHN_ID_MAX
};

/*! \enum PRN_BAD_POINT_DETECT_xxx
 *  \brief Thermal printer Bad Point Detecting Status.
 */
enum TPH_BAD_POINT_DETECT_STATE {
	PRN_BAD_POINT_DETECT_INIT = 0,
	PRN_BAD_POINT_DETECT_START,
	PRN_BAD_POINT_DETECT_FINISHED
};

/*! \enum PRN_START_PRINT_xxx
 *  \brief Task print strategies.
 */
enum {
	PRN_START_PRINT_DURING_RECEIVING = 0,
	PRN_START_PRINT_AFTER_COMPLETE
};

/*! \def PRINT_CHN_FLAG_xxx
 *  \brief Print channel flags.
 */
#define PRINT_CHN_FLAG_NO_REPRINT			(1 << 0) /*!< Do not reprint tasks of this channel after out-of-paper */

/*! \def TASK_FLAG_xxx
 *  \brief Task flags.
 */
#define TASK_FLAG_RECV_NOT_COMPLETE			(1 << 0)
#define TASK_FLAG_DONT_CARE_PAPER_STATUS	(1 << 1)
#define TASK_FLAG_DONT_REPRINT				(1 << 2)
#define TASK_FLAG_DONT_SEND_BUFFER_ID		(1 << 3)
#define TASK_FLAG_DISABLE_UTF8				(1 << 4)
#define TASK_FLAG_DISCARD_DATA_ON_OOP		(1 << 5) /* Discard data on out-of-paper */
#define TASK_FLAG_DONT_COLLECT_DATA			(1 << 6)
#define TASK_FLAG_REPRINTED_BY_FEED_KEY		(1 << 7) /* This is a reprint task by double-clicking FEED key */
#define TASK_FLAG_DONT_PRINT				(1 << 8) /* task delete */

#define TASK_FLAG_IS_SET(t,f)				((t)->flags &   (f))
#define TASK_FLAG_SET(t,f)					((t)->flags |=  (f))
#define TASK_FLAG_CLEAR(t,f)				((t)->flags &= ~(f))

#define PRINT_SERVER_SEM_BUFFER_POOL		"/print_server.buffer_pool"
#define PRINT_SERVER_SEM_TASK_POOL			"/print_server.task_pool"
#define PRINT_SERVER_SEM_CHANNELS			"/print_server.channels%02d"
#define PRINT_SERVER_SEM_TASK_QUEUE			"/print_server.task_queue"
#define PRINT_SERVER_SEM_STATUS				"/print_server.status"
#define PRINT_SERVER_SEM_REQUEST			"/print_server.request"
#define PRINT_SERVER_SHM					"/shm.print_server"
#define PRINT_SERVER_MQ						"/mq.print_server"

#define PRINT_BUFFER_SIZE					(250)
#define PRINT_BUFFER_POOL_SIZE				(8192)
#define PRINT_TASK_POOL_SIZE				(512)
#define PRINT_CHANNELS_MAX					(PRINT_CHN_ID_MAX)
#define PRINT_MQ_MSG_SIZE					(16)

/*! \typedef print_status_t
 *  \brief Structure of print status.
 */
typedef struct {
	uint32_t printing : 1;					/*!< 1: There is print task under printing */
	uint32_t no_paper : 1;					/*!< 1: Out-of-paper is detected */
	uint32_t paper_running_out : 1;			/*!< 1: Paper is running out */
	uint32_t paper_jam : 1;					/*!< 1: Paper jam is detected */
	uint32_t paper_not_taken : 1;			/*!< 1: Paper is not taken */
	uint32_t platen_opened : 1;				/*!< 1: The platen is open */
	uint32_t thermal_head_overheated : 1;	/*!< 1: Thermal head is overheated */
	uint32_t step_motor_overheated : 1;		/*!< 1: Step motor is overheated */
	uint32_t vpp_too_low : 1;				/*!< 1: VPP is too low */
	uint32_t vpp_too_high : 1;				/*!< 1: VPP is too high */
	uint32_t cutter : 3;					/*!< Cutter status */
	uint32_t paper_size : 1;				/*!< 1: 58mm paper holder installed */
	int task_id;							/*!< Task ID of the printing task */
	uint32_t printed_copies;				/*!< T.B.C */
	uint32_t total_copies;					/*!< T.B.C */
	uint32_t print_distance;				/*!< Print distance, in millimeter */
	uint32_t cashbox_open_count;			/*!< Cashbox open count */
	int motor_temperature;					/*!< Feed motor temperature */
} print_status_t;

typedef struct {
	uint8_t DataReady;
	uint8_t CJKMode;
	uint8_t CodePage;
	uint8_t ASC_WordSet;
	uint8_t CJK_WordSet;
	uint8_t Utf8_WordSet;
} print_settings_t;

typedef struct _print_buffer_t print_buffer_t;
typedef struct _print_task_t print_task_t;
typedef struct _print_channel_t print_channel_t;
typedef struct _print_log_t print_log_t;

struct _print_buffer_t {
	uint16_t next;
	uint16_t id;
	uint16_t len;
	uint8_t data[PRINT_BUFFER_SIZE];
} __attribute__((aligned(2)));

struct _print_task_t {
	uint16_t prev;
	uint16_t next;
	uint16_t head;
	uint16_t tail;
	int chn_id;
	int id;
	uint32_t copies;
	uint32_t bytes_received;
	uint32_t bytes_handled;
	uint32_t bytes_to_receive;
	uint16_t next_buffer_id;
	uint16_t buffer_id_resend_count;
	uint16_t buffer_sent;
	uint16_t buffer_total;
	uint16_t flags;
} __attribute__((aligned(2)));

struct _print_channel_t {
	uint32_t flags;
	int task_end_timeout;
	int no_data_received;
	int last_task_id;
	uint16_t task;
	struct {
		uint16_t head;
		uint16_t tail;
	} task_queue;
};

struct _print_log_t {
	char orderId[64];
	time_t reception_time; /*打印任务接收时间*/
	time_t finish_time; /*打印任务完成时间*/
};  

typedef struct {
	/*
	 * Print buffer pool.
	 */
	struct {
		print_buffer_t mem[PRINT_BUFFER_POOL_SIZE];
		uint16_t head;
		uint16_t tail;
		uint16_t free_count;
		uint16_t free_count_min;
	} buffer_pool;

	/*
	 * Print task pool.
	 */
	struct {
		print_task_t mem[PRINT_TASK_POOL_SIZE];
		uint16_t head;
		uint16_t tail;
		uint16_t free_count;
		uint16_t free_count_min;
		int next_task_id;
		int last_completed_task_id;
	} task_pool;

	/*
	 * Print channels.
	 */
	print_channel_t channels[PRINT_CHANNELS_MAX];

	/*
	 * Print queue.
	 */
	struct {
		uint16_t head;
		uint16_t tail;
		uint16_t buf_to_send;
		uint16_t buf_offset;
		int last_channel_id;
	} task_queue;

	/*
	 * Print status.
	 */
	print_status_t status;

	/*
	 * Print settings.
	 */
	print_settings_t settings;

	/*
	 * Request.
	 */
	struct {
		uint8_t status_change_subscribers;
		uint8_t task_logging;
		uint8_t task_print_strategy;
		uint8_t paper_not_taken_actions;
	} request;

	/*
	 * Bad points.
	 */
	struct {
		uint8_t result[72];
		uint32_t size;
		int state; /* 0: Idle; 1: Starting; 2: Running; 3: Completed */
	} bad_points;

	/*
	 * Paper location.
	 */
	struct {
		uint32_t black_mark_location;
		uint32_t located_black_marks;
		uint32_t current_location;
	} paper_location;

	/*
	 * Font library information.
	 */
	struct {
		char version[16];
		char language[16];
	} fontlib;

	/*
	 * Data collection information.
	 */
	struct {
		uint8_t enabled;
		char seqnum[16];
		time_t timestamp;
		int task_id;
		uint16_t task_index;
	} data_collection;

	struct {
		uint8_t enabled;
		uint64_t channel_allow;     //bits = 1 mean channel enable
		char seqnum[16];
		time_t timestamp;
		int task_id;
		uint16_t task_index;
	} third_party_data_collection;
	
	print_log_t print_log_mem[PRINT_TASK_POOL_SIZE];
} print_shm_t;

enum {
	PRINT_CMD_NONE = 0,
	PRINT_CMD_SET_FEEDING_STATE,
	PRINT_CMD_SET_DENSITY,
	PRINT_CMD_SET_MAXSPEED,
	PRINT_CMD_GENERATE_PULSE,
	PRINT_CMD_GET_PAPER_LOCATION,
	PRINT_CMD_SET_PAPER_NOT_TAKEN_ACTIONS,
	PRINT_CMD_GET_SETTINGS,
	PRINT_CMD_GET_BAD_POINTS,
	PRINT_CMD_HANDLE_REALTIME_COMMAND,
	PRINT_CMD_MAX
};

#pragma pack(1)

typedef union {
	uint8_t cmd;
	struct {
		uint8_t cmd;
		uint32_t from;
		uint32_t dots;
	} get_bad_points;
	struct {
		uint8_t cmd;
		uint8_t value;
		bool save;
	} set_value;
	struct {
		uint8_t cmd;
		uint8_t pin;
		uint16_t on_time;
		uint16_t off_time;
	} generate_pulse;
	struct {
		uint8_t cmd;
		uint8_t chn_id;
		uint8_t len;
		uint8_t data[1];
	} handle_realtime_command;
	uint8_t payload[PRINT_MQ_MSG_SIZE];
} print_msg_t;

#pragma pack()

#define PRINT_DATA_SHARE_POST_DATA_COLLECTION	"/print_data_share.post_data_collection"
typedef struct {
	sem_t *sem_notification;
	sem_t *sem_lock;
	sem_t *sem_completion;
} print_data_share_t;

typedef void (*print_status_change_handler)(const print_status_t *);

/*----------------------------------------------*
 | PUBLIC FUNCTIONS                             |
 *----------------------------------------------*/

/*! \fn int print_set_feeding_state(uint8_t state);
 *  \brief Start or stop paper feeding.
 *
 *  \param state Pass a non-zero value to start feeding, or zero to stop feeding.
 *  \return Returns 0 if succeeds; or -1 if fails.
 */
extern int print_set_feeding_state(uint8_t state);

extern int print_buffer_is_full(uint32_t bytes_to_reserve);

/*! \fn int print_channel_send_data(int chn_id, const void *data, uint32_t len, uint32_t copies);
 *  \brief Append data to a new or existing print task of a print channel.
 *
 *  Print data from different sources must be sent to print server via different
 *  channels to avoid mixture.
 *
 *  A print task is guaranteed to be printed in one piece of paper, i.e. the whole task
 *  would be printed again from beginning if out-of-paper happened during printing,
 *  unless the channel has PRINT_CHN_FLAG_NO_REPRINT flag set.
 *
 *  \param chn_id Specify the channel ID of the print channel to which the print data will be sent.
 *  \param data Data to print.
 *  \param len Length of 'data' in bytes.
 *  \param copies Number of copies to print. If 'copies' is not zero, a new print task is created,
 *         containing 'data' only, and it will be printed for 'copies' times. If 'copies' is zero,
 *         'data' will be appended to the existing task if any, or form a new task.
 *  \return Returns a unique positive task ID to identify the print task;
 *          or -1 if somthing goes wrong; or 0 if nothing to print (e.g. 'len' is zero).
 */
extern int print_channel_send_data_with_flag(int chn_id, const void *data, uint32_t len, uint32_t copies, uint32_t flag);
extern int print_channel_send_data(int chn_id, const void *data, uint32_t len, uint32_t copies);
extern int print_channel_send_data_cloud(int chn_id, const void *data, uint32_t len, uint32_t copies, char *orderNo);
extern int print_channel_send_data_with_flag_cloud(int chn_id, const void *data, uint32_t len, uint32_t copies, uint32_t flag, char *orderNo);

/*! \fn int print_supplemental_data(const char *seqnum, time_t timestamp, const void *data, uint32_t len, uint32_t copies);
 *  \brief Print supplemental data (Data Collection Feature).
 *
 *  Print supplemental data (Data Collection Feature).
 *
 *  \param seqnum Sequence number of the receipt to which the supplemental data to append.
 *  \param timestamp Timestamp when the receipt data is uploaded.
 *  \param data Data to print.
 *  \param len Length of 'data' in bytes.
 *  \param copies Number of copies to print.
 *  \return Returns a unique positive task ID to identify the print task;
 *          or -1 if somthing goes wrong; or 0 if nothing to print (e.g. 'len' is zero).
 */
extern int print_supplemental_data(const char *seqnum, time_t timestamp, const void *data, uint32_t len, uint32_t copies);

/*! \fn int print_task_is_complete(int task_id);
 *  \brief Query the printing status of a print task.
 *
 *  \param task_id The unique task ID returned by print_channel_send_data().
 *  \return Returns 1 if the print task has been printed completely, including all copies;
 *          or 0 if it is not completed yet.
 */
extern int print_task_is_complete(int task_id);

extern int print_task_is_complete_or_reset(int task_id);

extern int print_task_set_flag(int task_id, uint32_t flag);
extern int print_task_append_data(uint16_t task_index, uint16_t after, const void *data, uint32_t len);
extern int print_task_append_data_before_final_cutting(uint16_t task_index, void **data, uint32_t *data_len);

/*! \fn int print_get_status(print_status_t *status);
 *  \brief Retrieve print status.
 *
 *  \param status The retrieved print status is stored in it.
 *  \return Returns 0 if succeeds; or -1 if fails.
 */
extern int print_get_status(print_status_t *status);

extern int print_get_settings(print_settings_t *settings);

/*! \fn int print_status_change_subscribe(print_status_change_handler handler);
 *  \brief Subscribe to print status change post.
 *
 *  \param handler Callback function to handle print status change event.
 *  \return Returns 0 if succeeds; or -1 if fails.
 */
extern int print_status_change_subscribe(print_status_change_handler handler);

/*! \fn int print_status_change_unsubscribe(void);
 *  \brief Unsubscribe to print status change post.
 *         In normally case, this function should not be used.
 *
 *  \return Always returns 0.
 */
extern int print_status_change_unsubscribe(void);

/*! \fn int print_get_bad_points(uint32_t from, uint32_t dots, void *result, uint32_t *size);
 *  \brief Get bad point information.
 *
 *  \param from Examine for bad points starting from this point;
 *              especially if from is 0xFFFFFFFF, param dots is discarded and it would do a full scan
 *              (PRINTER_DOT_PER_LINE number of dots).
 *  \param dots Number of dots to examine.
 *  \param result Address to store the bad point information.
 *  \param size Pass the size of 'result' when calling this function, and when this
 *              function returns, it is modified to the actual size of 'result'.
 *
 *  \return This function returns -1 if error, or returns TPH_BAD_POINT_DETECT_STATE.
 */
extern int print_get_bad_points(uint32_t from, uint32_t dots, void *result, uint32_t *size);

/*! \fn int print_set_density(uint8_t density, bool save);
 *  \brief Set print density.
 *
 *  \param density Print density to be set, ranges from 10 to 200. Default value is 100.
 *  \param save Specify whether the setting should be saved.
 *
 *  \return Returns 0 if succeeds; or -1 if fails.
 */
extern int print_set_density(uint8_t density, bool save);

/*! \fn int print_set_maxspeed(uint8_t maxspeed, bool save);
 *  \brief Set maximum print speed.
 *
 *  \param maxspeed Maximum print speed to be set, ranges from 0 to 255. Default value is 255.
 *  \param save Specify whether the setting should be saved.
 *
 *  \return Returns 0 if succeeds; or -1 if fails.
 */
extern int print_set_maxspeed(uint8_t maxspeed, bool save);

/*! \fn int print_generate_pulse(uint8_t pin, uint16_t on_time, uint16_t off_time);
 *  \brief Generate pulse.
 *
 *  \param pin Not used for the moment.
 *  \param on_time The high level time of the pulse, in milliseconds.
 *  \param off_time The low level time of the pulse, in milliseconds.
 *
 *  \return Returns 0 if succeeds; or -1 if fails.
 */
extern int print_generate_pulse(uint8_t pin, uint16_t on_time, uint16_t off_time);

/*! \fn int print_get_fontlib_version(char *buf);
 *  \brief Get version information of the font library.
 *
 *  \param buf Buffer to store the version string. It should not smaller than 16 bytes.
 *
 *  \return Returns 0 if succeeds; or -1 if fails.
 */
extern int print_get_fontlib_version(char *buf);

/*! \fn int print_get_fontlib_language(char *buf);
 *  \brief Get language information of the font library.
 *
 *  \param buf Buffer to store the language string. It should not smaller than 16 bytes.
 *
 *  \return Returns 0 if succeeds; or -1 if fails.
 */
extern int print_get_fontlib_language(char *buf);

/*! \fn int print_set_task_logging_state(int enabled);
 *  \brief Enable or disable logging of print task data.
 *
 *  \param enabled Set to non-zero to enable logging, zero to disable logging.
 *
 *  \return Returns 0 if succeeds; or -1 if fails.
 */
extern int print_set_task_logging_state(int enabled);

extern int print_set_task_print_strategy(int strategy);

extern int print_get_last_channel_id(void);

extern int print_data_share_init(const char *name, print_data_share_t *pds, bool create);
extern int print_data_share_notify(print_data_share_t *pds);
extern int print_data_share_wait(print_data_share_t *pds);
extern int print_data_share_complete(print_data_share_t *pds);

extern const print_status_t *print_status(void);
extern const print_shm_t *print_shm(void);
extern const print_task_t *task_at(uint16_t t);

/*! \fn int print_api_init(void);
 *  \brief Initialize the print APIs.
 *
 *  Call this function once and only once before using any print APIs.
 *
 *  \return Returns 0 if succeeds; or -1 if fails.
 */
extern int print_api_init(void);

extern int badpoint_info_from_file(void *buf, int buf_len, badpoint_info_hdr_t *info);
extern int badpoint_info_save_file(const void *buf, int buf_len, const badpoint_info_hdr_t *info);
extern int PrintInfoPage(void);
extern char *get_bt_devname(char *buf, int len);
extern void set_data_collection_state(int enabled);
extern void set_third_party_data_collection_state(int enabled, uint64_t channel);

#define PAPER_NOT_TAKEN_ACTION_STOP_PRINTING (1 << 0)
#define PAPER_NOT_TAKEN_ACTION_PLAY_AUDIO    (1 << 1)
extern uint8_t get_paper_not_taken_actions(void);
extern void set_paper_not_taken_actions(uint8_t actions);
/*
 * func: 清除指定的打印任务
 * param: task_id = 0, 清除全部任务; > 0, 清除指定任务；<0 编号不存在
 * ret: 0：未知错误（例如编号不存在）；1：清除完成；
 */
int print_task_delete(int task_id);

#endif /* __PRINT_API_H__ */
