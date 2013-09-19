
#include <sys/un.h>

extern int new_callref;

struct mncc_q_entry {
	struct mncc_q_entry *next;
	unsigned int len;
	char data[0];			/* struct gsm_mncc */
};

enum {
	LCR_GSM_TYPE_NETWORK,
	LCR_GSM_TYPE_MS,
};

struct lcr_gsm {
	char interface_name[64]; /* name of interface this instance is associated to */
	struct lcr_gsm	*gsm_ms_next;	/* list of MS instances, in case of MS */
	char		name[16];	/* name of MS instance, in case of MS */
	int		type;		/* LCR_GSM_TYPE_*/

	struct lcr_fd	mncc_lfd;	/* Unix domain socket to OpenBSC MNCC */
	struct mncc_q_entry *mncc_q_hd;
	struct mncc_q_entry *mncc_q_tail;
	struct lcr_timer socket_retry;	/* Timer to re-try connecting to BSC socket */
	struct sockaddr_un sun;		/* Socket address of MNCC socket */
};

/* GSM port class */
class Pgsm : public Port
{
	public:
	Pgsm(int type, char *portname, struct port_settings *settings, struct interface *interface);
	~Pgsm();

	char p_g_imsi[16]; /* imsi of current phone (used for ECT/MPTY with gsm_bs) */
	signed short p_g_samples[160]; /* last received audi packet */
	int p_g_tones; /* set, if tones are to be generated */
	int p_g_earlyb; /* set, if patterns are available */
	struct lcr_gsm *p_g_lcr_gsm; /* pointer to network/ms instance */
	unsigned int p_g_callref; /* ref by OpenBSC/Osmocom-BB */
	struct lcr_work p_g_delete; /* queue destruction of GSM port instance */
	unsigned int p_g_mode; /* data/transparent mode */
	int p_g_gsm_b_sock; /* gsm bchannel socket */
	struct lcr_fd p_g_gsm_b_fd; /* event node */
	int p_g_gsm_b_index; /* gsm bchannel socket index to use */
	int p_g_gsm_b_active; /* gsm bchannel socket is activated */
	struct lcr_msg *p_g_notify_pending;	/* queue for NOTIFY if not connected */
	struct lcr_msg *p_g_setup_pending;	/* queue SETUP until RTP is created */
	struct lcr_msg *p_g_connect_pending;	/* queue CONNECT until RTP is created and connected */
	void *p_g_fr_encoder, *p_g_fr_decoder;	/* gsm handle */
	void *p_g_hr_encoder, *p_g_hr_decoder;	/* gsm handle */
	void *p_g_amr_encoder, *p_g_amr_decoder;/* gsm handle */
	int p_g_amr_cmr, p_g_amr_cmr_valid;
	signed short p_g_rxdata[160]; /* receive audio buffer */
	int p_g_rxpos; /* position in audio buffer 0..159 */
	int p_g_tch_connected; /* indicates if audio is connected */
	int p_g_media_type; /* current payload type or 0 if not set */
	int p_g_payload_type; /* current payload type */

	int p_g_rtp_bridge; /* if we use a bridge */
	unsigned int p_g_rtp_ip_remote; /* stores ip */
	unsigned short p_g_rtp_port_remote; /* stores port */
	int p_g_rtp_payloads;
	int p_g_rtp_media_types[8];
	unsigned char p_g_rtp_payload_types[8];

	void frame_send(void *_frame, int len, int msg_type);
	void frame_receive(void *_frame);
	int audio_send(unsigned char *data, int len);
	int bridge_rx(unsigned char *data, int len);

	void send_mncc_rtp_connect(void);
	int hunt_bchannel(void);
	void modify_lchan(int media_type);
	void call_proc_ind(unsigned int msg_type, unsigned int callref, struct gsm_mncc *mncc);
	void alert_ind(unsigned int msg_type, unsigned int callref, struct gsm_mncc *mncc);
	void setup_cnf(unsigned int msg_type, unsigned int callref, struct gsm_mncc *mncc);
	void setup_compl_ind(unsigned int msg_type, unsigned int callref, struct gsm_mncc *mncc);
	void disc_ind(unsigned int msg_type, unsigned int callref, struct gsm_mncc *mncc);
	void rel_ind(unsigned int msg_type, unsigned int callref, struct gsm_mncc *mncc);
	void notify_ind(unsigned int msg_type, unsigned int callref, struct gsm_mncc *mncc);
	void rtp_create_ind(unsigned int msg_type, unsigned int callref, struct gsm_mncc *mncc);
	void rtp_connect_ind(unsigned int msg_type, unsigned int callref, struct gsm_mncc *mncc);
	void message_notify(unsigned int epoint_id, int message_id, union parameter *param);
	void message_progress(unsigned int epoint_id, int message_id, union parameter *param);
	void message_alerting(unsigned int epoint_id, int message_id, union parameter *param);
	void message_connect(unsigned int epoint_id, int message_id, union parameter *param);
	void message_disconnect(unsigned int epoint_id, int message_id, union parameter *param);
	void message_release(unsigned int epoint_id, int message_id, union parameter *param);
	int message_epoint(unsigned int epoint_id, int message_id, union parameter *param);
};

struct gsm_mncc *create_mncc(int msg_type, unsigned int callref);
int send_and_free_mncc(struct lcr_gsm *lcr_gsm, unsigned int msg_type, void *data);
void gsm_trace_header(const char *interface_name, class Pgsm *port, unsigned int msg_type, int direction);
int gsm_conf(struct gsm_conf *gsm_conf, char *conf_error);
int gsm_exit(int rc);
int gsm_init(void);
int mncc_socket_retry_cb(struct lcr_timer *timer, void *inst, int index);

