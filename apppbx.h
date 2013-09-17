/*****************************************************************************\
**                                                                           **
** Linux Call Router                                                         **
**                                                                           **
**---------------------------------------------------------------------------**
** Copyright: Andreas Eversberg                                              **
**                                                                           **
** EndpointAppPBX header file                                                **
**                                                                           **
\*****************************************************************************/ 


enum { /* release actions: see epoint.release */
	RELEASE_NONE,
	RELEASE_JOIN,		/* join, hold */
	RELEASE_PORT_JOINONLY,	/* join, port */
	RELEASE_ALL,		/* join, hold, port */
};

enum { /* states as viewed from io port (state of calls are always connected) */
	EPOINT_STATE_IDLE,		/* no call */
	EPOINT_STATE_IN_SETUP,		/* setup sent */
	EPOINT_STATE_OUT_SETUP,		/* setup sent */
	EPOINT_STATE_IN_OVERLAP,	/* more information */
	EPOINT_STATE_OUT_OVERLAP,	/* more information */
	EPOINT_STATE_IN_PROCEEDING,	/* proceeding */
	EPOINT_STATE_OUT_PROCEEDING,	/* proceeding */
	EPOINT_STATE_IN_ALERTING,	/* ringing */
	EPOINT_STATE_OUT_ALERTING,	/* ringing */
	EPOINT_STATE_CONNECT,		/* connected */
	EPOINT_STATE_IN_DISCONNECT,	/* disconnected receiving tones */
	EPOINT_STATE_OUT_DISCONNECT,	/* disconnected sending tones */
};

#define EPOINT_STATE_NAMES \
static const char *state_name[] = { \
	"EPOINT_STATE_IDLE", \
	"EPOINT_STATE_IN_SETUP", \
	"EPOINT_STATE_OUT_SETUP", \
	"EPOINT_STATE_IN_OVERLAP", \
	"EPOINT_STATE_OUT_OVERLAP", \
	"EPOINT_STATE_IN_PROCEEDING", \
	"EPOINT_STATE_OUT_PROCEEDING", \
	"EPOINT_STATE_IN_ALERTING", \
	"EPOINT_STATE_OUT_ALERTING", \
	"EPOINT_STATE_CONNECT", \
	"EPOINT_STATE_IN_DISCONNECT", \
	"EPOINT_STATE_OUT_DISCONNECT", \
}; \
int state_name_num = sizeof(state_name) / sizeof(char *);

int vbox_refresh(struct lcr_timer *timer, void *instance, int index);

extern class EndpointAppPBX *apppbx_first;

/* structure of an EndpointAppPBX */
class EndpointAppPBX : public EndpointApp
{
	public:
	EndpointAppPBX(class Endpoint *epoint, int origin);
	~EndpointAppPBX();

	class EndpointAppPBX	*next;

	int			e_hold;			/* is this endpoint on hold ? */
	char			e_tone[256];		/* save tone for resuming ports */

	unsigned int		e_adminid;

	/* states */
	int			e_state;		/* state of endpoint */
	char			e_extension_interface[32];/* current internal isdn interface (usefull for callback to internal phone) */
	struct caller_info	e_callerinfo;		/* information about the caller */
	struct dialing_info	e_dialinginfo;		/* information about dialing */
	struct connect_info	e_connectinfo;		/* information about connected line */
	struct redir_info	e_redirinfo;		/* info about redirection (to the calling user) */
	struct capa_info	e_capainfo;		/* info about l3,l2 capacity */
	struct rtp_info		e_rtpinfo;		/* info about rtp port forwarding and payload type */
	time_t			e_start, e_stop;	/* time */
	int			e_origin;		/* origin of call 0=incoming 1=outgoing */
	struct route_ruleset	*e_ruleset;		/* current ruleset pointer (NULL=no ruleset) */
	struct route_rule	*e_rule;		/* current rule pointer (NULL=no rule) */
	struct route_action	*e_action;		/* current action pointer (NULL=no action) */
	int			e_rule_nesting;		/* 'goto'/'menu' recrusion counter to prevent infinie loops */
	struct route_action	*e_match_to_action;	/* what todo when timeout */
	char			*e_match_to_extdialing;	/* dialing after matching timeout rule */
	int			e_select;		/* current selection for various selector options */
	char			*e_extdialing;		/* dialing after matching rule */
	int		e_overlap;		/* is set if additional information is/are received after setup */
	struct extension e_ext;			/* extension information */

//	int e_knocking;				/* true, if knocking */
//	double e_knocktime;			/* next time to knock */

//	char e_join_tone[64], e_hold_tone[64];	/* current tone */
	int e_join_pattern/*, e_hold_pattern*/;	/* pattern available */

	/* action */
	char e_dialing_queue[32];		/* holds dialing during setup state */
	double e_powerdelay;			/* delay when to redial */
	int e_powercount;			/* power count */
	int e_powerlimit;			/* power limit */
	struct lcr_timer	e_action_timeout;
	struct lcr_timer	e_match_timeout;

	/* port relation */
	int e_multipoint_cause;			/* cause value of disconnected multiport calls (highest priority) */
	int e_multipoint_location;		/* location of cause with highest priority */

	/* join relation */
	int e_join_cause;
	int e_join_location;	

	/* callback */
	char e_cbdialing[256];			/* dialing information after callback */
	char e_cbcaller[256];			/* extension for the epoint which calls back */
	char e_cbto[32];			/* override callerid to call back to */
	struct caller_info e_callbackinfo;	/* information about the callback caller */
	struct lcr_timer	e_redial_timeout;
	int e_powerdial_on;
	struct lcr_timer	e_powerdial_timeout;
	struct lcr_timer	e_cfnr_timeout;
	struct lcr_timer	e_cfnr_call_timeout;
	struct lcr_timer	e_callback_timeout;
	struct lcr_timer	e_password_timeout;

	/* dtmf stuff */
	int e_connectedmode;			/* if the port should stay connected if the enpoint disconnects or releases (usefull for callback) */
	int e_dtmf;				/* allow dtmf */
	/* read doc/keypad.txt for more information */
	int e_dtmf_time;			/* time when the last key was received. */
	int e_dtmf_last;			/* last dtmf key */
	int e_enablekeypad;			/* remote app requests dtmf */

	/* transmit and receive state */
	int e_tx_state;				/* current endpoint's state */
	int e_rx_state;				/* current endpoint's state */

	/* vbox playback variables */
	char e_vbox[32];			/* current vbox extension (during playback) */
	int e_vbox_state;			/* state of vbox during playback */
	int e_vbox_menu;			/* currently selected menu using '*' and '#' */
	char e_vbox_display[128];		/* current display message */
	struct lcr_timer e_vbox_refresh;	/* display must be refreshed du to change */
	int e_vbox_counter;			/* current playback counter in seconds */
	int e_vbox_counter_max;			/* size of file in seconds */
	int e_vbox_counter_last;		/* temp variable to recognise a change in seconds */
	int e_vbox_play;			/* current file that is played */
	int e_vbox_speed;			/* current speed to play */
	int e_vbox_index_num;			/* number of files */
	char e_vbox_index_file[128];		/* current file name */
	int e_vbox_index_hour;			/* current time the file recorded... */
	int e_vbox_index_min;
	int e_vbox_index_mon;
	int e_vbox_index_mday;
	int e_vbox_index_year;
	char e_vbox_index_callerid[128];	/* current caller id */
	int e_vbox_index_callerid_index;	/* next digit to speak */

	/* efi */
	int e_efi_state;			/* current spoken sample */
	int e_efi_digit;			/* current spoken digit */

#ifdef WITH_CRYPT
	/* crypt states and vars */
	int e_crypt;				/* current user level crypt state */
	int e_crypt_state;			/* current crypt manager state */
	char e_crypt_info[33];			/* last information text */
	int e_crypt_timeout_sec;		/* timer */
	int e_crypt_timeout_usec;		/* timer */
	unsigned int e_crypt_random;		/* current random number for ident */
	unsigned int e_crypt_bogomips;		/* bogomips for ident */
	unsigned char e_crypt_key[256];		/* the session key */
	int e_crypt_key_len;
	unsigned char e_crypt_ckey[256];	/* the encrypted session key */
	int e_crypt_ckey_len;
	unsigned char e_crypt_rsa_n[512];	/* rsa key */
	unsigned char e_crypt_rsa_e[16];
	unsigned char e_crypt_rsa_d[512];
	unsigned char e_crypt_rsa_p[512];
	unsigned char e_crypt_rsa_q[512];
	unsigned char e_crypt_rsa_dmp1[512];
	unsigned char e_crypt_rsa_dmq1[512];
	unsigned char e_crypt_rsa_iqmp[512];
	int e_crypt_rsa_n_len;
	int e_crypt_rsa_e_len;
	int e_crypt_rsa_d_len;
	int e_crypt_rsa_p_len;
	int e_crypt_rsa_q_len;
	int e_crypt_rsa_dmp1_len;
	int e_crypt_rsa_dmq1_len;
	int e_crypt_rsa_iqmp_len;
	int e_crypt_keyengine_busy;		/* current job and busy state */
	int e_crypt_keyengine_return;		/* return */
	struct lcr_timer e_crypt_handler; /* poll timer for crypt events */
#endif

	/* messages */
	void hookflash(void);
	void ea_message_port(unsigned int port_id, int message, union parameter *param);
	void port_setup(struct port_list *portlist, int message_type, union parameter *param);
	void port_information(struct port_list *portlist, int message_type, union parameter *param);
	void port_dtmf(struct port_list *portlist, int message_type, union parameter *param);
	void port_crypt(struct port_list *portlist, int message_type, union parameter *param);
	void port_overlap(struct port_list *portlist, int message_type, union parameter *param);
	void port_proceeding(struct port_list *portlist, int message_type, union parameter *param);
	void port_alerting(struct port_list *portlist, int message_type, union parameter *param);
	void port_connect(struct port_list *portlist, int message_type, union parameter *param);
	void port_disconnect_release(struct port_list *portlist, int message_type, union parameter *param);
	void port_timeout(struct port_list *portlist, int message_type, union parameter *param);
	void port_notify(struct port_list *portlist, int message_type, union parameter *param);
	void port_progress(struct port_list *portlist, int message_type, union parameter *param);
	void port_facility(struct port_list *portlist, int message_type, union parameter *param);
	void port_3pty(struct port_list *portlist, int message_type, union parameter *param);
	void port_transfer(struct port_list *portlist, int message_type, union parameter *param);
	void port_suspend(struct port_list *portlist, int message_type, union parameter *param);
	void port_resume(struct port_list *portlist, int message_type, union parameter *param);
	void port_enablekeypad(struct port_list *portlist, int message_type, union parameter *param);
	void port_disable_dejitter(struct port_list *portlist, int message_type, union parameter *param);
	void ea_message_join(unsigned int join_id, int message, union parameter *param);
	void join_crypt(struct port_list *portlist, int message_type, union parameter *param);
	void join_mISDNsignal(struct port_list *portlist, int message_type, union parameter *param);
	void join_bridge(struct port_list *portlist, int message_type, union parameter *param);
	void join_setup(struct port_list *portlist, int message_type, union parameter *param);
	void join_information(struct port_list *portlist, int message_type, union parameter *param);
	void join_overlap(struct port_list *portlist, int message_type, union parameter *param);
	void join_proceeding(struct port_list *portlist, int message_type, union parameter *param);
	void join_alerting(struct port_list *portlist, int message_type, union parameter *param);
	void join_connect(struct port_list *portlist, int message_type, union parameter *param);
	void join_disconnect_release(int message_type, union parameter *param);
	void join_notify(struct port_list *portlist, int message_type, union parameter *param);
	void join_facility(struct port_list *portlist, int message_type, union parameter *param);
	void join_dtmf(struct port_list *portlist, int message_type, union parameter *param);
	void join_disable_dejitter(struct port_list *portlist, int message_type, union parameter *param);

	/* epoint */
	void new_state(int state);
	void release(int release, int joinlocation, int joincause, int portlocation, int portcause, int force);
	void notify_active(void);
	void keypad_function(char digit);
	void set_tone(struct port_list *portlist, const char *tone);
	void out_setup(int cfnr);
	char *apply_callerid_display(const char *id, int itype, int ntype, int present, int screen, const char *extension, const char *name);
	void auth(int job, int bit_num);

	/* vbox playback stuff */
	void vbox_init(void);
	void vbox_index_read(int num);
	void vbox_index_remove(int num);
	void efi_message_eof(void);
	void vbox_message_eof(void);
	void set_tone_vbox(const char *tone);
	void set_play_vbox(const char *file, int offset);
	void set_play_speed(int speed);

	/* efi */
	void set_tone_efi(const char *tone);
	
	/* routing */
	struct route_ruleset *rulesetbyname(char *name);
	struct route_action *route(struct route_ruleset *ruleset);
	struct route_param *routeparam(struct route_action *action, unsigned long long id);

	/* init / dialing / hangup */
	void action_init_call(void);
	void action_dialing_internal(void);
	void action_dialing_external(void);
	void action_dialing_h323(void);
	void action_dialing_vbox_record(void);
	void action_init_partyline(void);
	void action_hangup_call(void);
	void action_dialing_login(void);
	void action_init_change_callerid(void);
	void _action_callerid_calleridnext(int next);
	void action_dialing_callerid(void);
	void action_dialing_calleridnext(void);
	void action_init_change_forward(void);
	void action_dialing_forward(void);
	void action_init_redial_reply(void);
	void _action_redial_reply(int in);
	void action_dialing_redial(void);
	void action_dialing_reply(void);
	void action_dialing_powerdial(void);
	void action_dialing_callback(void);
	void action_hangup_callback(void);
	void action_dialing_abbrev(void);
	void action_dialing_test(void);
	void action_init_play(void);
	void action_init_vbox_play(void);
	void action_init_efi(void);
	void action_init_pots_retrieve(void);
	void action_init_pots_release(void);
	void action_init_pots_reject(void);
	void action_init_pots_answer(void);
	void action_init_pots_3pty(void);
	void action_init_pots_transfer(void);
	void action_dialing_vbox_play(void);
	void action_dialing_calculator(void);
	void action_dialing_timer(void);
	void _action_goto_menu(int mode);
	void action_dialing_goto(void);
	void action_dialing_menu(void);
	void action_dialing_disconnect(void);
	void action_dialing_release(void);
	void action_dialing_help(void);
	void action_dialing_deflect(void);
	void action_dialing_setforward(void);
	void action_init_execute(void);
	void action_hangup_execute(void);
	void action_execute(void);
	void action_hangup_file(void);
	void action_init_pick(void);
	void action_dialing_password(void);
	void action_dialing_password_wr(void);
	void process_dialing(int timeout);
	void process_hangup(int cause, int location);

	/* facility function */
	void pick_join(char *extension);
	int join_join_dss1(int invoke_id);
	int join_join_fxs(void);
	int join_3pty_dss1(void);
	int join_3pty_fxs(void);
	int split_3pty(void);
	void encrypt_shared(void);
	void encrypt_keyex(void);
	void encrypt_off(void);
	void encrypt_result(int message, char *text);
	int check_external(const char **errstr, class Port **port);

	/* crypt */
	void cryptman_keyengine(int job);
	void cr_ident(int message, unsigned char *param, int len);
	void cr_activate(int message, unsigned char *param, int len);
	void cr_deactivate(int message, unsigned char *param, int len);
	void cr_master(int message, unsigned char *param, int len);
	void cr_slave(int message, unsigned char *param, int len);
	void cr_looped(int message, unsigned char *param, int len);
	void cr_abort(int message, unsigned char *param, int len);
	void cr_abort_engine(int message, unsigned char *param, int len);
	void cr_abort_wait(int message, unsigned char *param, int len);
	void cr_genrsa(int message, unsigned char *param, int len);
	void cr_keyerror(int message, unsigned char *param, int len);
	void cr_pubkey(int message, unsigned char *param, int len);
	void cr_cptrsa(int message, unsigned char *param, int len);
	void cr_cskey(int message, unsigned char *param, int len);
	void cr_decrsa(int message, unsigned char *param, int len);
	void cr_waitdelay(int message, unsigned char *param, int len);
	void cr_bfactive(int message, unsigned char *param, int len);
	void cr_crypterror(int message, unsigned char *param, int len);
	void cr_release(int message, unsigned char *param, int len);
	void cr_sactivate(int message, unsigned char *param, int len);
	void cr_sdeactivate(int message, unsigned char *param, int len);
	void cr_sbfactive(int message, unsigned char *param, int len);
	void cr_scrypterror(int message, unsigned char *param, int len);
	void cr_sabort(int message, unsigned char *param, int len);
	void cr_info(int message, unsigned char *param, int len);
	void cryptman_message(int message, unsigned char *param, int len);
	void cryptman_msg2man(unsigned char *param, int len);
	void cryptman_addinf(unsigned char *buf, int buf_size, int element, int len, void *data);
	int cryptman_sizeofinf(unsigned char *buf, int element);
	unsigned char *cryptman_getinf(unsigned char *buf, int element, unsigned char *to);
	void cryptman_msg2peer(unsigned char *buf);
	void cryptman_msg2user(int msg, const char *text);
	void cryptman_msg2crengine(int msg, unsigned char *buf, int len);
	void cryptman_state(int state);
	void cryptman_timeout(int secs);
	void message_disconnect_port(struct port_list *portlist, int cause, int location, const char *display, const struct param_transfer *transfer);
	void logmessage(int message_type, union parameter *param, unsigned int port_id, int dir);
	void trace_header(const char *name, int direction);
};


void apply_callerid_restriction(struct extension *ext, char *id, int *ntype, int *present, int *screen, char *extension, char *name);
void send_mail(char *filename, char *callerid, char *callerintern, char *callername, char *vbox_email, int vbox_year, int vbox_mon, int vbox_mday, int vbox_hour, int vbox_min, char *terminal);


