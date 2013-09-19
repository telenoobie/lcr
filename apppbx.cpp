/*****************************************************************************\
**                                                                           **
** Linux Call Router                                                         **
**                                                                           **
**---------------------------------------------------------------------------**
** Copyright: Andreas Eversberg                                              **
**                                                                           **
** The EndpointAppPBX implements PBX4Linux                                   **
**                                                                           **
\*****************************************************************************/ 


#include "main.h"

class EndpointAppPBX *apppbx_first = NULL;

int action_timeout(struct lcr_timer *timer, void *instance, int index);
int match_timeout(struct lcr_timer *timer, void *instance, int index);
int redial_timeout(struct lcr_timer *timer, void *instance, int index);
int powerdial_timeout(struct lcr_timer *timer, void *instance, int index);
int cfnr_timeout(struct lcr_timer *timer, void *instance, int index);
int cfnr_call_timeout(struct lcr_timer *timer, void *instance, int index);
int password_timeout(struct lcr_timer *timer, void *instance, int index);
int callback_timeout(struct lcr_timer *timer, void *instance, int index);

/*
 * EndpointAppPBX constructor
 */
EndpointAppPBX::EndpointAppPBX(class Endpoint *epoint, int origin) : EndpointApp(epoint, origin, EAPP_TYPE_PBX)
{
	class EndpointAppPBX **apppointer;

#ifdef WITH_CRYPT
	memset(&e_crypt_handler, 0, sizeof(e_crypt_handler));
	add_timer(&e_crypt_handler, crypt_handler, this, 0);
#endif
	memset(&e_vbox_refresh, 0, sizeof(e_vbox_refresh));
	add_timer(&e_vbox_refresh, vbox_refresh, this, 0);
	memset(&e_action_timeout, 0, sizeof(e_action_timeout));
	add_timer(&e_action_timeout, action_timeout, this, 0);
	memset(&e_match_timeout, 0, sizeof(e_match_timeout));
	add_timer(&e_match_timeout, match_timeout, this, 0);
	memset(&e_redial_timeout, 0, sizeof(e_redial_timeout));
	add_timer(&e_redial_timeout, redial_timeout, this, 0);
	memset(&e_powerdial_timeout, 0, sizeof(e_powerdial_timeout));
	add_timer(&e_powerdial_timeout, powerdial_timeout, this, 0);
	memset(&e_cfnr_timeout, 0, sizeof(e_cfnr_timeout));
	add_timer(&e_cfnr_timeout, cfnr_timeout, this, 0);
	memset(&e_cfnr_call_timeout, 0, sizeof(e_cfnr_call_timeout));
	add_timer(&e_cfnr_call_timeout, cfnr_call_timeout, this, 0);
	memset(&e_callback_timeout, 0, sizeof(e_callback_timeout));
	add_timer(&e_callback_timeout, callback_timeout, this, 0);
	memset(&e_password_timeout, 0, sizeof(e_password_timeout));
	add_timer(&e_password_timeout, password_timeout, this, 0);
	e_powerdial_on = 0;

	/* add application to chain */
	next = NULL;
	apppointer = &apppbx_first;
	while(*apppointer)
		apppointer = &((*apppointer)->next);
	*apppointer = this;

	/* initialize */
        memset(&e_ext, 0, sizeof(struct extension));
	// *************** NOTE: also change value in read_extension() **************
	e_ext.rights = 4; /* international */
	e_ext.rx_gain = e_ext.tx_gain = 0;
        e_state = EPOINT_STATE_IDLE;
        e_ext.number[0] = '\0';
	e_extension_interface[0] = '\0';
        memset(&e_callerinfo, 0, sizeof(struct caller_info));
        memset(&e_dialinginfo, 0, sizeof(struct dialing_info));
        memset(&e_connectinfo, 0, sizeof(struct connect_info));
        memset(&e_redirinfo, 0, sizeof(struct redir_info));
        memset(&e_capainfo, 0, sizeof(struct capa_info));
        memset(&e_rtpinfo, 0, sizeof(struct rtp_info));
        e_start = e_stop = 0;
	e_origin = origin;
	e_ruleset = ruleset_main;
	if (e_ruleset)
        	e_rule = e_ruleset->rule_first;
	e_rule_nesting = 0;
        e_action = NULL;
	e_match_to_action = NULL;
        e_select = 0;
        e_extdialing = e_dialinginfo.id;
//        e_knocking = 0;
//        e_knocktime = 0;
	e_hold = 0;
//        e_join_tone[0] = e_hold_tone[0] = '\0';
        e_join_pattern /*= e_hold_pattern*/ = 0;
	e_tone[0] = '\0';
	e_adminid = 0; // will be set, if call was initiated via admin socket
        e_powerdelay = 0;
        e_powerlimit = 0;
        e_cbdialing[0] = '\0';
        e_cbcaller[0] = '\0';
	e_cbto[0] = '\0';
        memset(&e_callbackinfo, 0, sizeof(struct caller_info));
        e_connectedmode = 0;
        e_dtmf = 0;
        e_dtmf_time = 0;
        e_dtmf_last = 0;
	e_enablekeypad = 0;
	e_multipoint_cause = 0;
	e_multipoint_location = 0;
	e_dialing_queue[0] = '\0';
#ifdef WITH_CRYPT
	e_crypt = CRYPT_OFF;
	e_crypt_state = CM_ST_NULL;
	e_crypt_keyengine_busy = 0;
	e_crypt_info[0] = '\0';
#endif
	e_overlap = 0;
	e_vbox[0] = '\0';
	e_tx_state = NOTIFY_STATE_ACTIVE;
	e_rx_state = NOTIFY_STATE_ACTIVE;
	e_join_cause = e_join_location = 0;
/*********************************
 *********************************
 ********* ATTENTION *************
 *********************************
 *********************************/
/* if you add new values, that must be initialized, also check if they must
 * be initialized when doing callback
 */

}

/*
 * EpointAppPBX destructor
 */
EndpointAppPBX::~EndpointAppPBX(void)
{
	class EndpointAppPBX *temp, **tempp;

#ifdef WITH_CRYPT
	del_timer(&e_crypt_handler);
#endif
	del_timer(&e_vbox_refresh);
	del_timer(&e_action_timeout);
	del_timer(&e_match_timeout);
	del_timer(&e_redial_timeout);
	del_timer(&e_powerdial_timeout);
	del_timer(&e_cfnr_timeout);
	del_timer(&e_cfnr_call_timeout);
	del_timer(&e_callback_timeout);
	del_timer(&e_password_timeout);

	/* detach */
	temp =apppbx_first;
	tempp = &apppbx_first;
	while(temp) {
		if (temp == this)
			break;

		tempp = &temp->next;
		temp = temp->next;
	}
	if (temp == 0)
		FATAL("Endpoint not in endpoint's list.\n");
	*tempp = next;

}


/*
 * trace header for application
 */
void EndpointAppPBX::trace_header(const char *name, int direction)
{
	struct trace _trace;

	char msgtext[sizeof(_trace.name)];

	SCPY(msgtext, name);

	/* init trace with given values */
	start_trace(-1,
		    NULL,
		    numberrize_callerinfo(e_callerinfo.id, e_callerinfo.ntype, options.national, options.international),
		    e_dialinginfo.id,
		    direction,
		    CATEGORY_EP,
		    ea_endpoint->ep_serial,
		    msgtext);
}


EPOINT_STATE_NAMES

/* set new endpoint state
 */
void EndpointAppPBX::new_state(int state)
{
#if 0
	if (e_state != state) {
		trace_header("NEW STATE", DIRECTION_NONE);
		add_trace("state", "old", "%s", state_name[e_state]);
		add_trace("state", "new", "%s", state_name[state]);
		end_trace();
	}
#endif
	e_state = state;
}


/* release join and port (as specified)
 */
void EndpointAppPBX::release(int release, int joinlocation, int joincause, int portlocation, int portcause, int force)
{
	struct port_list *portlist;
	struct lcr_msg *message;
	char cause[16];

	/* message to test call */
	admin_call_response(e_adminid, ADMIN_CALL_RELEASE, "", joincause, joinlocation, 0);

	/* if a release is pending */
	if (release==RELEASE_JOIN || release==RELEASE_ALL || release==RELEASE_PORT_JOINONLY) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d): do pending release (joincause %d location %d)\n", ea_endpoint->ep_serial, joincause, joinlocation);
		if (ea_endpoint->ep_join_id) {
			message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_RELEASE);
			message->param.disconnectinfo.cause = joincause;
			message->param.disconnectinfo.location = joinlocation;
			message_put(message);
			ea_endpoint->ep_join_id = 0;
		}
		e_join_pattern = 0;
#if 0
		if (release != RELEASE_PORT_JOINONLY) {
			if (e_hold_id)
				join_release(e_hold_id, ea_endpoint->ep_serial, 1, joinlocation, joincause);
			e_hold_id = 0;
		}
#endif
	}
	if (release==RELEASE_ALL || release==RELEASE_PORT_JOINONLY) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) do pending release (portcause %d portlocation)\n", ea_endpoint->ep_serial, portcause, portlocation);
		while((portlist = ea_endpoint->ep_portlist)) {
			if (portlist->port_id) {
				SPRINT(cause, "cause_%02x", portcause);
				set_tone(portlist, cause);
				message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_RELEASE);
				message->param.disconnectinfo.cause = portcause;
				message->param.disconnectinfo.location = portlocation;
				message->param.disconnectinfo.force = force; // set, if port should release imediately
				message_put(message);
				logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
			}
			ea_endpoint->free_portlist(portlist);
		}

		/* if callback is enabled, call back with the given caller id */
		if (e_callback_timeout.active) {
			/* reset some stuff */
		        new_state(EPOINT_STATE_IDLE);
			memset(&e_connectinfo, 0, sizeof(struct connect_info));
			memset(&e_redirinfo, 0, sizeof(struct redir_info));
			e_start = e_stop = 0;
			e_ruleset = ruleset_main;
			if (e_ruleset)
        			e_rule = e_ruleset->rule_first;
			e_action = NULL;
			unsched_timer(&e_action_timeout);
			unsched_timer(&e_match_timeout);
			unsched_timer(&e_cfnr_timeout);
			unsched_timer(&e_cfnr_call_timeout);
			e_match_to_action = NULL;
			//e_select = 0;
        		e_extdialing = e_dialinginfo.id;
			e_connectedmode = 0;
			e_dtmf = 0;
			e_dtmf_time = 0;
			e_dtmf_last = 0;
			e_enablekeypad = 0;
			e_multipoint_cause = 0;
			e_multipoint_location = 0;
			e_dialing_queue[0] = '\0';
#ifdef WITH_CRYPT
			e_crypt = 0;
			e_crypt_state = CM_ST_NULL;
			e_crypt_keyengine_busy = 0;
			e_crypt_info[0] = '\0'; 
#endif
			e_tone[0] = '\0';
			e_overlap = 0;
			e_vbox[0] = '\0';
			e_tx_state = NOTIFY_STATE_ACTIVE;
			e_rx_state = NOTIFY_STATE_ACTIVE;
			e_join_cause = e_join_location = 0;
			e_rule_nesting = 0;
			/* the caller info of the callback user */
			memcpy(&e_callbackinfo, &e_callerinfo, sizeof(e_callbackinfo));
			memset(&e_dialinginfo, 0, sizeof(e_dialinginfo));
			/* create dialing by callerinfo */
			if (e_ext.number[0] && e_extension_interface[0]) {
				PDEBUG(DEBUG_EPOINT, "EPOINT(%d) preparing callback to internal: %s interface %s\n", ea_endpoint->ep_serial, e_ext.number, e_extension_interface);
				/* create callback to the current terminal */
				SCPY(e_dialinginfo.id, e_ext.number);
				SCPY(e_dialinginfo.interfaces, e_extension_interface);
				e_dialinginfo.itype = INFO_ITYPE_ISDN_EXTENSION;
				e_dialinginfo.ntype = INFO_NTYPE_UNKNOWN;
			} else {
				if (e_cbto[0]) {
					SCPY(e_dialinginfo.id, e_cbto);
				} else {
					/* numberrize caller id and use it to dial to the callback */
					SCPY(e_dialinginfo.id, numberrize_callerinfo(e_callerinfo.id,e_callerinfo.ntype, options.national, options.international));
				}
				e_dialinginfo.itype = INFO_ITYPE_ISDN;
				e_dialinginfo.ntype = INFO_NTYPE_UNKNOWN;
				PDEBUG(DEBUG_EPOINT, "EPOINT(%d) preparing callback to external: %s\n", ea_endpoint->ep_serial, e_dialinginfo.id);
			}
			return;
		}

		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) do pending release of epoint itself.\n", ea_endpoint->ep_serial);
		if (--ea_endpoint->ep_use <= 0) /* when e_lock is 0, the endpoint will be deleted */
			trigger_work(&ea_endpoint->ep_delete);
		return;
	}
}


/* cancel callerid if restricted, unless anon-ignore is enabled at extension or port is of type external (so called police gets caller id :)*/
void apply_callerid_restriction(struct extension *ext, char *id, int *ntype, int *present, int *screen, char *extension, char *name)
{
	PDEBUG(DEBUG_EPOINT, "id='%s' ntype=%d present=%d screen=%d extension='%s' name='%s'\n", (id)?id:"NULL", (ntype)?*ntype:-1, (present)?*present:-1, (screen)?*screen:-1, (extension)?extension:"NULL", (name)?name:"NULL");

	/* caller id is not restricted, so we do nothing */
	if (*present != INFO_PRESENT_RESTRICTED)
		return;

	/* only extensions are restricted */
	if (!ext->number[0])
		return;

	/* if we enabled anonymouse ignore */
	if (ext->anon_ignore)
		return;

	/* else we remove the caller id */
	if (id)
		id[0] = '\0';
	if (ntype)
		*ntype = INFO_NTYPE_UNKNOWN;
//	if (screen)
//		*screen = INFO_SCREEN_USER;
// maybe we should not make voip address anonymous
//	if (voip)
//		voip[0] = '\0';
// maybe it's no fraud to present extension id
//	if (extension)
//		extension[0] = '\0';
	if (name)
		name[0] = '\0';
}

/* used display message to display callerid as available */
char *EndpointAppPBX::apply_callerid_display(const char *id, int itype, int ntype, int present, int screen, const char *extension, const char *name)
{
	static char display[81];

	display[0] = '\0';
	const char *cid = numberrize_callerinfo(id, ntype, options.national, options.international);

	PDEBUG(DEBUG_EPOINT, "EPOINT(%d) id='%s' itype=%d ntype=%d present=%d screen=%d extension='%s' name='%s'\n", ea_endpoint->ep_serial, (id)?id:"NULL", itype, ntype, present, screen, (extension)?extension:"NULL", (name)?name:"NULL");

	if (!id)
		id = "";
	if (!extension)
		extension = "";
	if (!name)
		name = "";

	/* NOTE: is caller is is not available for this extesion, it has been removed by apply_callerid_restriction already */

	/* internal extension's caller id */
	if (extension[0] && e_ext.display_int) {
		if (!display[0])
			SCAT(display, extension);
		if (display[0])
			SCAT(display, " ");
		if (itype == INFO_ITYPE_VBOX)
			SCAT(display, "(vbox)");
		else
			SCAT(display, "(int)");
	}

	/* external caller id */
	if (!extension[0] && e_ext.display_ext) {
		if (!display[0]) {
			if (!cid[0]) {
				if (present == INFO_PRESENT_RESTRICTED)
					SCAT(display, "anonymous");
				else
					SCAT(display, "unknown");
			}
			else
				SCAT(display, cid);
		}
	}

	/* display if callerid is anonymouse but available due anon-ignore */
	if (e_ext.display_anon && present==INFO_PRESENT_RESTRICTED) {
		if (!cid[0])
			SCAT(display, "unknown");
		else 
			SCAT(display, cid);
		SCAT(display, " anon");
	}

	/* display if callerid is anonymouse but available due anon-ignore */
	if (e_ext.display_fake && screen==INFO_SCREEN_USER && ntype!=INFO_NTYPE_NOTPRESENT) {
		if (!display[0]) {
			if (!id[0]) {
				if (present == INFO_PRESENT_RESTRICTED)
					SCAT(display, "anonymous");
				else
					SCAT(display, "unknown");
			}
			else
				SCAT(display, cid);
		}
		SCAT(display, " fake");
	}

	/* caller name */
	if (name[0] && e_ext.display_name) {
		if (!display[0] && cid[0])
				SCAT(display, cid);
		if (display[0])
				SCAT(display, " ");
		SCAT(display, name);
	}

	return(display);
}

/*
 * uses the current state to notify activity
 */
void EndpointAppPBX::notify_active(void)
{
	struct port_list *portlist = ea_endpoint->ep_portlist;
	struct lcr_msg *message;
	int notify = 0;

	switch(e_tx_state) {
		case NOTIFY_STATE_ACTIVE:
		/* we are already active, so we don't do anything */
		break;

		case NOTIFY_STATE_SUSPEND:
		notify = INFO_NOTIFY_USER_RESUMED;
		while(portlist) {
			set_tone(portlist, NULL);
			portlist = portlist->next;
		}
		portlist = ea_endpoint->ep_portlist;
		break;

		case NOTIFY_STATE_HOLD:
		notify = INFO_NOTIFY_REMOTE_RETRIEVAL;
		while(portlist) {
			set_tone(portlist, NULL);
			portlist = portlist->next;
		}
		portlist = ea_endpoint->ep_portlist;
		break;

		case NOTIFY_STATE_CONFERENCE:
		notify = INFO_NOTIFY_CONFERENCE_DISCONNECTED;
		while(portlist) {
			set_tone(portlist, NULL);
			portlist = portlist->next;
		}
		portlist = ea_endpoint->ep_portlist;
		break;

		default:
		PERROR("unknown e_tx_state = %d\n", e_tx_state);
	}

	if (notify)
	while(portlist) {
		message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_NOTIFY);
		message->param.notifyinfo.notify = notify;
		message_put(message);
		logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
		portlist = portlist->next;
	}
}


/*
 * keypad functions during call. one example to use this is to put a call on hold or start a conference
 */
void EndpointAppPBX::keypad_function(char digit)
{
	class Port *port;

	/* we must be in a call, in order to send messages to the call */
	if (e_ext.number[0] == '\0') {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) IGNORING keypad received not from extension.\n", ea_endpoint->ep_serial);
		return;
	}

	switch(digit) {
		/* join conference */
		case '3':
		if (ea_endpoint->ep_join_id == 0) {
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d) keypad received during connect but not during a call.\n", ea_endpoint->ep_serial);
			break;
		}
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) join call with call on hold\n", ea_endpoint->ep_serial);
		port = find_port_id(ea_endpoint->ep_portlist->port_id);
		if (!port)
			break;
		if ((port->p_type & PORT_CLASS_POTS_MASK) == PORT_CLASS_POTS_FXS)
			join_join_fxs();
		else if ((port->p_type & PORT_CLASS_mISDN_MASK) == PORT_CLASS_DSS1)
			join_join_dss1(-1);
		break;

#ifdef WITH_CRYPT
		/* crypt shared */
		case '7':
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) shared key encryption selected.\n", ea_endpoint->ep_serial);
		encrypt_shared();
		break;

		/* crypt key-exchange */
		case '8':
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) key exchange encryption selected.\n", ea_endpoint->ep_serial);
		encrypt_keyex();
		break;

		/* crypt off */
		case '9':
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) encryption off selected.\n", ea_endpoint->ep_serial);
		encrypt_off();
		break;
#endif

		default:	
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) unsupported keypad digit '%c'.\n", ea_endpoint->ep_serial, digit);
	}
}


/* set tone pattern for port */
void EndpointAppPBX::set_tone(struct port_list *portlist, const char *tone)
{
	struct lcr_msg *message;

	if (!tone)
		tone = "";

	/* store for suspended processes */
	SCPY(e_tone, tone);


	if (e_join_pattern /* pattern are provided */
	 && !(e_ext.own_setup && e_state == EPOINT_STATE_IN_SETUP)
	 && !(e_ext.own_setup && e_state == EPOINT_STATE_IN_OVERLAP)
	 && !(e_ext.own_proceeding && e_state == EPOINT_STATE_IN_PROCEEDING)
	 && !(e_ext.own_alerting && e_state == EPOINT_STATE_IN_ALERTING)
	 && !(e_ext.own_cause && e_state == EPOINT_STATE_IN_DISCONNECT)
	 && !(e_ext.own_setup && e_state == EPOINT_STATE_OUT_SETUP)
	 && !(e_ext.own_setup && e_state == EPOINT_STATE_OUT_OVERLAP)
	 && !(e_ext.own_proceeding && e_state == EPOINT_STATE_OUT_PROCEEDING)
	 && !(e_ext.own_alerting && e_state == EPOINT_STATE_OUT_ALERTING)
	 && !(e_ext.own_cause && e_state == EPOINT_STATE_OUT_DISCONNECT)
	 && tone[0] && !!strncmp(tone,"crypt_*",6)) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) tone not provided since patterns are available\n", ea_endpoint->ep_serial);
		tone = "";
	}

	if (portlist) {
		message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_TONE);
		SCPY(message->param.tone.dir, e_ext.tones_dir);
		SCPY(message->param.tone.name, tone);
		message_put(message);
		logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
	} else {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) no port to notify tone.\n", ea_endpoint->ep_serial);
		return;
	}
}


/* outgoing setup to port(s)
 * ports will be created and a setup is sent if everything is ok. otherwhise
 * the endpoint is destroyed.
 */
void EndpointAppPBX::out_setup(int cfnr)
{
	struct dialing_info	dialinginfo;
	class Port		*port;
	struct port_list	*portlist;
	struct lcr_msg		*message;
	int			anycall = 0;
	int			cause = CAUSE_RESSOURCEUNAVAIL;
	const char		*p;
	char			cfp[64];
	struct interface	*interface;
#ifdef WITH_MISDN
	struct mISDNport	*mISDNport;
#endif
	char			portname[32];
	char			*dirname;
	class EndpointAppPBX	*atemp;
//	char			allowed_ports[256];
//	char			exten[256];
	char			ifname[sizeof(e_ext.interfaces)],
				*ifname_p,
				number[256],
				*number_p;
	struct port_settings	port_settings;
#ifdef WITH_MISDN
	int			channel = 0;
#endif
	struct admin_list	*admin;
	int			earlyb;
	int			mode = B_MODE_TRANSPARENT;

	/* set bchannel mode */
	mode = e_capainfo.source_mode;

	/* create settings for creating port */
	memset(&port_settings, 0, sizeof(port_settings));
	if (e_ext.tones_dir)
		SCPY(port_settings.tones_dir, e_ext.tones_dir);
	else
		SCPY(port_settings.tones_dir, options.tones_dir);
	port_settings.no_seconds = e_ext.no_seconds;
	
	/* NOTE: currently the try_card feature is not supported. it should be used later to try another card, if the outgoing call fails on one port */

	/* check what dialinginfo.itype we got */
	switch(e_dialinginfo.itype) {
		/* *********************** call to extension or vbox */
		case INFO_ITYPE_ISDN_EXTENSION:
		/* check if we deny incoming calls when we use an extension */
		if (e_ext.noknocking) {
			atemp = apppbx_first;
			while(atemp) {
				if (atemp != this)
				if (!strcmp(atemp->e_ext.number, e_ext.number))
					break;
				atemp = atemp->next;
			}
			if (atemp) {
				PERROR("EPOINT(%d) noknocking and currently a call\n", ea_endpoint->ep_serial);
				release(RELEASE_ALL, LOCATION_PRIVATE_LOCAL, CAUSE_BUSY, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, 0); /* RELEASE_TYSPE_ join, port */
				return; /* must exit here */
			}
		}
		/* FALL THROUGH !!!! */
		case INFO_ITYPE_VBOX:
		/* get dialed extension's info */
//		SCPY(exten, e_dialinginfo.id);
//		if (strchr(exten, ','))
//			*strchr(exten, ',') = '\0';
//		if (!read_extension(&e_ext, exten))
		if (!read_extension(&e_ext, e_dialinginfo.id)) {
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d) extension %s not configured\n", ea_endpoint->ep_serial, e_dialinginfo.id);
			release(RELEASE_ALL, LOCATION_PRIVATE_LOCAL, CAUSE_OUTOFORDER, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, 0); /* RELEASE_TYPE, join, port */
			return; /* must exit here */
		}
		e_dialinginfo.sending_complete = 1;

		if (e_dialinginfo.itype == INFO_ITYPE_VBOX) {
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d) dialing directly to VBOX\n", ea_endpoint->ep_serial);
			p = "vbox";
			goto vbox_only;
		}

		/* string from unconditional call forward (cfu) */
		p = e_ext.cfu;
		if (*p) {
			/* present to forwarded party */
			if (e_ext.anon_ignore && e_callerinfo.id[0]) {
				e_callerinfo.present = INFO_PRESENT_ALLOWED;
			}
			if (!!strcmp(p, "vbox") || (e_capainfo.bearer_capa==INFO_BC_AUDIO) || (e_capainfo.bearer_capa==INFO_BC_SPEECH))
				goto cfu_only;
		}

		/* string from busy call forward (cfb) */
		p = e_ext.cfb;
		if (*p) {
			class EndpointAppPBX *checkapp = apppbx_first;
			while(checkapp) {
				if (checkapp != this) { /* any other endpoint except our own */
					if (!strcmp(checkapp->e_ext.number, e_ext.number)) {
						/* present to forwarded party */
						if (e_ext.anon_ignore && e_callerinfo.id[0]) {
							e_callerinfo.present = INFO_PRESENT_ALLOWED;
						}
						if (!!strcmp(p, "vbox") || (e_capainfo.bearer_capa==INFO_BC_AUDIO) || (e_capainfo.bearer_capa==INFO_BC_SPEECH))
							goto cfb_only;
					}
				}
				checkapp = checkapp->next;
			}
		}

		/* string from no-response call forward (cfnr) */
		p = e_ext.cfnr;
		if (*p) {
			/* when cfnr is done, out_setup() will setup the call */
			if (cfnr) {
				/* present to forwarded party */
				if (e_ext.anon_ignore && e_callerinfo.id[0]) {
					e_callerinfo.present = INFO_PRESENT_ALLOWED;
				}
				goto cfnr_only;
			}
			if (!!strcmp(p, "vbox") || (e_capainfo.bearer_capa==INFO_BC_AUDIO) || (e_capainfo.bearer_capa==INFO_BC_SPEECH)) {
				schedule_timer(&e_cfnr_timeout, e_ext.cfnr_delay, 0);
				schedule_timer(&e_cfnr_call_timeout, e_ext.cfnr_delay + 1, 0); /* call one second after release */
				PDEBUG(DEBUG_EPOINT, "EPOINT(%d) setting time for call-forward-busy to %s with delay %ld.\n", ea_endpoint->ep_serial, e_ext.cfnr, e_ext.cfnr_delay);
			}
		}

		/* call to all internal interfaces */
		p = e_ext.interfaces;
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) generating multiple joins for extension %s to interfaces %s\n", ea_endpoint->ep_serial, e_dialinginfo.id, p);
		while(*p) {
			earlyb = 0;
			ifname[0] = '\0';
			while(*p!=',' && *p!='\0')
				if (*p > ' ')
					SCCAT(ifname, *p++);
			if (*p == ',')
				p++;
			/* search interface */
			interface = hunt_interface(ifname);
			if (!interface) {
				trace_header("INTERFACE (not found)", DIRECTION_NONE);
				add_trace("interface", NULL, "%s", ifname);
				end_trace();
				continue;
			}
			/* found interface */
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d) calling to interface %s\n", ea_endpoint->ep_serial, ifname);
			if (interface->remote) {
				admin = admin_first;
				while(admin) {
					if (admin->remote_name[0] && !strcmp(admin->remote_name, interface->remote_app))
						break;
					admin = admin->next;
				}
				if (!admin) {
					trace_header("INTERFACE (remote not connected)", DIRECTION_NONE);
					add_trace("application", NULL, "%s", interface->remote_app);
					end_trace();
					continue;
				}
				SPRINT(portname, "%s-%d-out", interface->name, 0);
				port = new Premote(PORT_TYPE_REMOTE_OUT, portname, &port_settings, interface, admin->sock);
				earlyb = (interface->is_earlyb == IS_YES);
			} else
#ifdef WITH_GSM_BS
			if (interface->gsm_bs) {
				SPRINT(portname, "%s-%d-out", interface->name, 0);
				port = new Pgsm_bs(PORT_TYPE_GSM_BS_OUT, portname, &port_settings, interface);
				earlyb = (interface->is_earlyb == IS_YES);
			} else
#endif
#ifdef WITH_GSM_MS
			if (interface->gsm_ms) {
				SPRINT(portname, "%s-%d-out", interface->name, 0);
				port = new Pgsm_ms(PORT_TYPE_GSM_MS_OUT, portname, &port_settings, interface);
				earlyb = (interface->is_earlyb == IS_YES);
			} else
#endif
#ifdef WITH_SIP
			if (interface->sip) {
				SPRINT(portname, "%s-%d-out", interface->name, 0);
				port = new Psip(PORT_TYPE_SIP_OUT, portname, &port_settings, interface);
				earlyb = (interface->is_earlyb == IS_YES);
			} else
#endif
			{
#ifdef WITH_MISDN
				/* hunt for mISDNport and create Port */
				mISDNport = hunt_port(ifname, &channel);
				if (!mISDNport) {
					trace_header("INTERFACE (busy)", DIRECTION_NONE);
					add_trace("interface", NULL, "%s", ifname);
					end_trace();
					continue;
				}

				SPRINT(portname, "%s-%d-out", mISDNport->ifport->interface->name, mISDNport->portnum);
#ifdef WITH_SS5
				if (mISDNport->ss5)
					port = ss5_hunt_line(mISDNport);
				else
#endif
#ifdef ISDN_P_FXS_POTS
				if (mISDNport->pots)
					port = new Pfxs(PORT_TYPE_POTS_FXS_OUT, mISDNport, portname, &port_settings, mISDNport->ifport->interface, mode);
				else
#endif
					port = new Pdss1((mISDNport->ntmode)?PORT_TYPE_DSS1_NT_OUT:PORT_TYPE_DSS1_TE_OUT, mISDNport, portname, &port_settings, mISDNport->ifport->interface, channel, mISDNport->ifport->channel_force, mode);
				earlyb = mISDNport->earlyb;
#else
			trace_header("INTERFACE (has no function)", DIRECTION_NONE);
			add_trace("interface", NULL, "%s", ifname);
			end_trace();
			continue;
#endif
			}
			if (!port)
				FATAL("Failed to create Port instance\n");
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d) got port %s\n", ea_endpoint->ep_serial, port->p_name);
			memset(&dialinginfo, 0, sizeof(dialinginfo));
			SCPY(dialinginfo.id, e_dialinginfo.id);
			dialinginfo.itype = INFO_ITYPE_ISDN_EXTENSION;
			dialinginfo.ntype = e_dialinginfo.ntype;
			/* create port_list relation */
			portlist = ea_endpoint->portlist_new(port->p_serial, port->p_type, earlyb);
			if (!portlist) {
				PERROR("EPOINT(%d) cannot allocate port_list relation\n", ea_endpoint->ep_serial);
				delete port;
				goto check_anycall_intern;
			}
			/* directory.list */
			if (e_callerinfo.id[0] && e_ext.display_name) {
				dirname = parse_directory(e_callerinfo.id, e_callerinfo.ntype);
				if (dirname)
					SCPY(e_callerinfo.name, dirname);
			}
//			dss1 = (class Pdss1 *)port;
			/* message */
//printf("INTERNAL caller=%s,id=%s,dial=%s\n", param.setup.networkid, param.setup.callerinfo.id, param.setup.dialinginfo.id);
			message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_SETUP);
			memcpy(&message->param.setup.dialinginfo, &dialinginfo, sizeof(struct dialing_info));
			memcpy(&message->param.setup.redirinfo, &e_redirinfo, sizeof(struct redir_info));
			memcpy(&message->param.setup.callerinfo, &e_callerinfo, sizeof(struct caller_info));
			memcpy(&message->param.setup.capainfo, &e_capainfo, sizeof(struct capa_info));
			memcpy(&message->param.setup.rtpinfo, &e_rtpinfo, sizeof(struct rtp_info));
//terminal			SCPY(message->param.setup.from_terminal, e_ext.number);
//terminal			if (e_dialinginfo.id)
//terminal				SCPY(message->param.setup.to_terminal, e_dialinginfo.id);
			/* handle restricted caller ids */
			apply_callerid_restriction(&e_ext, message->param.setup.callerinfo.id, &message->param.setup.callerinfo.ntype, &message->param.setup.callerinfo.present, &message->param.setup.callerinfo.screen, message->param.setup.callerinfo.extension, message->param.setup.callerinfo.name);
			apply_callerid_restriction(&e_ext, message->param.setup.callerinfo.id2, &message->param.setup.callerinfo.ntype2, &message->param.setup.callerinfo.present2, &message->param.setup.callerinfo.screen2, message->param.setup.callerinfo.extension, message->param.setup.callerinfo.name);
			apply_callerid_restriction(&e_ext, message->param.setup.redirinfo.id, &message->param.setup.redirinfo.ntype, &message->param.setup.redirinfo.present, 0, message->param.setup.redirinfo.extension, NULL);
			/* display callerid if desired for extension */
			SCPY(message->param.setup.callerinfo.display, apply_callerid_display(message->param.setup.callerinfo.id, message->param.setup.callerinfo.itype, message->param.setup.callerinfo.ntype, message->param.setup.callerinfo.present, message->param.setup.callerinfo.screen, message->param.setup.callerinfo.extension, message->param.setup.callerinfo.name));
//printf("\n\ndisplay = %s\n\n\n",message->param.setup.callerinfo.display);
			/* use cnip, if enabld */
	//		if (!e_ext.centrex)
	//			message->param.setup.callerinfo.name[0] = '\0';
			/* screen clip if prefix is required */
			if (message->param.setup.callerinfo.id[0] && e_ext.clip_prefix[0]) {
				SCPY(message->param.setup.callerinfo.id, e_ext.clip_prefix);
				SCAT(message->param.setup.callerinfo.id, numberrize_callerinfo(e_callerinfo.id,e_callerinfo.ntype, options.national, options.international));
				message->param.setup.callerinfo.ntype = INFO_NTYPE_UNKNOWN;
			}
			if (message->param.setup.callerinfo.id2[0] && e_ext.clip_prefix[0]) {
				SCPY(message->param.setup.callerinfo.id2, e_ext.clip_prefix);
				SCAT(message->param.setup.callerinfo.id2, numberrize_callerinfo(e_callerinfo.id2,e_callerinfo.ntype2, options.national, options.international));
				message->param.setup.callerinfo.ntype2 = INFO_NTYPE_UNKNOWN;
			}
			/* use internal caller id */
			if (e_callerinfo.extension[0] && (message->param.setup.callerinfo.present!=INFO_PRESENT_RESTRICTED || e_ext.anon_ignore)) {
				SCPY(message->param.setup.callerinfo.id, e_callerinfo.extension);
				message->param.setup.callerinfo.ntype = INFO_NTYPE_UNKNOWN;
				message->param.setup.callerinfo.ntype2 = INFO_NTYPE_NOTPRESENT;
			}
			message_put(message);
			logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
			anycall = 1;
		}

		/* string from parallel call forward (cfp) */
		p = e_ext.cfp;
		if (*p) {
			if (e_ext.anon_ignore && e_callerinfo.id[0]) {
				e_callerinfo.present = INFO_PRESENT_ALLOWED;
				e_callerinfo.present2 = INFO_PRESENT_ALLOWED;
			}
		}

		vbox_only: /* entry point for answering machine only */
		cfu_only: /* entry point for cfu */
		cfb_only: /* entry point for cfb */
		cfnr_only: /* entry point for cfnr */
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) call extension %s for external destiantion(s) '%s'\n", ea_endpoint->ep_serial, e_dialinginfo.id, p);
//		i=0;
		while(*p) {
			earlyb = 0;
			/* only if vbox should be dialed, and terminal is given */
			if (!strcmp(p, "vbox") && e_ext.number[0]) {
				/* go to the end of p */
				p += strlen(p);

				/* answering vbox call */
				PDEBUG(DEBUG_EPOINT, "EPOINT(%d) answering machine\n", ea_endpoint->ep_serial);
				/* alloc port */
				if (!(port = new VBoxPort(PORT_TYPE_VBOX_OUT, &port_settings)))
					FATAL("No memory for VBOX Port instance\n");
				PDEBUG(DEBUG_EPOINT, "EPOINT(%d) allocated port %s\n", ea_endpoint->ep_serial, port->p_name);
				UCPY(cfp, e_ext.number); /* cfp or any other direct forward/vbox */
			} else {
				cfp[0] = '\0';
				while(*p!=',' && *p!='\0')
					SCCAT(cfp, *p++);
				if (*p == ',')
					p++;
				/* external call */
				PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cfp external %s\n", ea_endpoint->ep_serial, cfp);
#ifdef WITH_MISDN
				/* hunt for mISDNport and create Port */
				mISDNport = hunt_port(e_dialinginfo.interfaces[0]?e_dialinginfo.interfaces:NULL, &channel);
				if (mISDNport) {
					/* creating EXTERNAL port*/
					SPRINT(portname, "%s-%d-out", mISDNport->ifport->interface->name, mISDNport->portnum);
#ifdef WITH_SS5
					if (mISDNport->ss5)
						port = ss5_hunt_line(mISDNport);
					else
#endif
#ifdef ISDN_P_FXS_POTS
					if (mISDNport->pots)
						port = new Pfxs(PORT_TYPE_POTS_FXS_OUT, mISDNport, portname, &port_settings, mISDNport->ifport->interface, mode);
					else
#endif
						port = new Pdss1((mISDNport->ntmode)?PORT_TYPE_DSS1_NT_OUT:PORT_TYPE_DSS1_TE_OUT, mISDNport, portname, &port_settings, mISDNport->ifport->interface, channel, mISDNport->ifport->channel_force, mode);
					if (!port)
						FATAL("No memory for Port instance\n");
					earlyb = mISDNport->earlyb;
				} else
#endif
				{
					port = NULL;
					trace_header("INTERFACE (too busy)", DIRECTION_NONE);
					add_trace("interface", NULL, "%s", e_dialinginfo.interfaces[0]?e_dialinginfo.interfaces:"any interface");
					end_trace();
				}
			}
			if (!port) {
				PDEBUG(DEBUG_EPOINT, "EPOINT(%d) no port found or created, which is idle.\n", ea_endpoint->ep_serial);
				goto check_anycall_intern;
			}
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d) found or created port %s\n", ea_endpoint->ep_serial, port->p_name);
			memset(&dialinginfo, 0, sizeof(dialinginfo));
			SCPY(dialinginfo.id, cfp);
			dialinginfo.itype = INFO_ITYPE_ISDN;
			dialinginfo.ntype = e_dialinginfo.ntype;
			portlist = ea_endpoint->portlist_new(port->p_serial, port->p_type, earlyb);
			if (!portlist) {
				PERROR("EPOINT(%d) cannot allocate port_list relation\n", ea_endpoint->ep_serial);
				delete port;
				goto check_anycall_intern;
			}
//printf("EXTERNAL caller=%s,id=%s,dial=%s\n", param.setup.networkid, param.setup.callerinfo.id, param.setup.dialinginfo.id);
			message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_SETUP);
			memcpy(&message->param.setup.dialinginfo, &dialinginfo, sizeof(struct dialing_info));
			memcpy(&message->param.setup.redirinfo, &e_redirinfo, sizeof(struct redir_info));
			memcpy(&message->param.setup.callerinfo, &e_callerinfo, sizeof(struct caller_info));
			/* if clip is hidden */
			if (e_ext.clip==CLIP_HIDE && port->p_type!=PORT_TYPE_VBOX_OUT) {
				SCPY(message->param.setup.callerinfo.id, e_ext.callerid);
				SCPY(message->param.setup.callerinfo.extension, e_ext.number);
				message->param.setup.callerinfo.ntype = e_ext.callerid_type;
				message->param.setup.callerinfo.present = e_ext.callerid_present;
				message->param.setup.callerinfo.ntype = INFO_NTYPE_NOTPRESENT;
			}
			memcpy(&message->param.setup.capainfo, &e_capainfo, sizeof(struct capa_info));
//terminal			SCPY(message->param.setup.from_terminal, e_ext.number);
//terminal			if (e_dialinginfo.id)
//terminal				SCPY(message->param.setup.to_terminal, e_dialinginfo.id);
				/* handle restricted caller ids */
			apply_callerid_restriction(&e_ext, message->param.setup.callerinfo.id, &message->param.setup.callerinfo.ntype, &message->param.setup.callerinfo.present, &message->param.setup.callerinfo.screen, message->param.setup.callerinfo.extension, message->param.setup.callerinfo.name);
			apply_callerid_restriction(&e_ext, message->param.setup.callerinfo.id2, &message->param.setup.callerinfo.ntype2, &message->param.setup.callerinfo.present2, &message->param.setup.callerinfo.screen2, message->param.setup.callerinfo.extension, message->param.setup.callerinfo.name);
			apply_callerid_restriction(&e_ext, message->param.setup.redirinfo.id, &message->param.setup.redirinfo.ntype, &message->param.setup.redirinfo.present, 0, message->param.setup.redirinfo.extension, NULL);
			/* display callerid if desired for extension */
			SCPY(message->param.setup.callerinfo.display, apply_callerid_display(message->param.setup.callerinfo.id, message->param.setup.callerinfo.itype, message->param.setup.callerinfo.ntype, message->param.setup.callerinfo.present, message->param.setup.callerinfo.screen, message->param.setup.callerinfo.extension, message->param.setup.callerinfo.name));
			message_put(message);
			logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
			anycall = 1;
		}

		check_anycall_intern:
		/* now we have all ports created */
		if (!anycall) {
			trace_header("INTERFACE (no extension's interface)", DIRECTION_NONE);
			end_trace();
			if (!ea_endpoint->ep_join_id)
				break;
			release(RELEASE_ALL, LOCATION_PRIVATE_LOCAL, cause, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, 0); /* RELEASE_TYPE, join, port */
			return; /* must exit here */
		}
		break;

		/* *********************** external call */
		default:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) dialing external: called='%s' keypad='%s'\n", ea_endpoint->ep_serial, e_dialinginfo.id, e_dialinginfo.keypad);
		/* call to extenal interfaces */
		if (e_dialinginfo.keypad[0])
			number_p = e_dialinginfo.keypad;
		else
			number_p = e_dialinginfo.id;
		do {
			number[0] = '\0';
			while(*number_p!=',' && *number_p!='\0')
				SCCAT(number, *number_p++);
			if (*number_p == ',')
				number_p++;
			/* found number */

			ifname_p = e_dialinginfo.interfaces;
			if (*ifname_p == '+')
				ifname_p++;
			do {
				earlyb = 0;
				ifname[0] = '\0';
				while(*ifname_p!=',' && *ifname_p!='\0')
					SCCAT(ifname, *ifname_p++);
				if (*ifname_p == ',')
					ifname_p++;
				/* found interface name */

				/* search interface */
				PDEBUG(DEBUG_EPOINT, "EPOINT(%d) calling to number '%s' interface '%s'\n", ea_endpoint->ep_serial, number, ifname[0]?ifname:"any interface");
				interface = hunt_interface(ifname[0]?ifname:NULL);
				if (!interface) {
					trace_header("INTERFACE (not found)", DIRECTION_NONE);
					add_trace("interface", NULL, "%s", ifname);
					end_trace();
					continue;
				}
				/* found interface */
				if (interface->remote) {
					admin = admin_first;
					while(admin) {
						if (admin->remote_name[0] && !strcmp(admin->remote_name, interface->remote_app))
							break;
						admin = admin->next;
					}
					if (!admin) {
						trace_header("INTERFACE (remote not connected)", DIRECTION_NONE);
						add_trace("application", NULL, "%s", interface->remote_app);
						end_trace();
						continue;
					}
					SPRINT(portname, "%s-%d-out", interface->name, 0);
					port = new Premote(PORT_TYPE_REMOTE_OUT, portname, &port_settings, interface, admin->sock);
					earlyb = (interface->is_earlyb == IS_YES);
				} else
#ifdef WITH_GSM_BS
				if (interface->gsm_bs) {
					SPRINT(portname, "%s-%d-out", interface->name, 0);
					port = new Pgsm_bs(PORT_TYPE_GSM_BS_OUT, portname, &port_settings, interface);
					earlyb = (interface->is_earlyb == IS_YES);
				} else
#endif
#ifdef WITH_GSM_MS
				if (interface->gsm_ms) {
					SPRINT(portname, "%s-%d-out", interface->name, 0);
					port = new Pgsm_ms(PORT_TYPE_GSM_MS_OUT, portname, &port_settings, interface);
					earlyb = (interface->is_earlyb == IS_YES);
				} else
#endif
#ifdef WITH_SIP
				if (interface->sip) {
					SPRINT(portname, "%s-%d-out", interface->name, 0);
					port = new Psip(PORT_TYPE_SIP_OUT, portname, &port_settings, interface);
					earlyb = (interface->is_earlyb == IS_YES);
				} else
#endif
				{
#ifdef WITH_MISDN
					/* hunt for mISDNport and create Port */
					mISDNport = hunt_port(ifname[0]?ifname:NULL, &channel);
					if (!mISDNport) {
						trace_header("INTERFACE (too busy)", DIRECTION_NONE);
						add_trace("interface", NULL, "%s", ifname[0]?ifname:"any interface");
						end_trace();
						continue;
					}
					/* creating EXTERNAL port*/
					SPRINT(portname, "%s-%d-out", mISDNport->ifport->interface->name, mISDNport->portnum);

#ifdef WITH_SS5
					if (mISDNport->ss5)
						port = ss5_hunt_line(mISDNport);
					else
#endif
#ifdef ISDN_P_FXS_POTS
					if (mISDNport->pots)
						port = new Pfxs(PORT_TYPE_POTS_FXS_OUT, mISDNport, portname, &port_settings, mISDNport->ifport->interface, mode);
					else
#endif
						port = new Pdss1((mISDNport->ntmode)?PORT_TYPE_DSS1_NT_OUT:PORT_TYPE_DSS1_TE_OUT, mISDNport, portname, &port_settings, mISDNport->ifport->interface, channel, mISDNport->ifport->channel_force, mode);
					earlyb = mISDNport->earlyb;
#else
					trace_header("INTERFACE (has no function)", DIRECTION_NONE);
					add_trace("interface", NULL, "%s", ifname);
					end_trace();
					continue;
#endif
				}
				if (!port)
					FATAL("No memory for Port instance\n");
				PDEBUG(DEBUG_EPOINT, "EPOINT(%d) created port %s\n", ea_endpoint->ep_serial, port->p_name);
				memset(&dialinginfo, 0, sizeof(dialinginfo));
				if (e_dialinginfo.keypad[0])
					SCPY(dialinginfo.keypad, number);
				else
					SCPY(dialinginfo.id, number);
				dialinginfo.itype = INFO_ITYPE_ISDN;
				dialinginfo.ntype = e_dialinginfo.ntype;
				dialinginfo.sending_complete = e_dialinginfo.sending_complete;
				portlist = ea_endpoint->portlist_new(port->p_serial, port->p_type, earlyb);
				if (!portlist) {
					PERROR("EPOINT(%d) cannot allocate port_list relation\n", ea_endpoint->ep_serial);
					delete port;
					continue;
				}
	//printf("EXTERNAL caller=%s,id=%s,dial=%s\n", param.setup.networkid, param.setup.callerinfo.id, param.setup.dialinginfo.id);
				message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_SETUP);
				memcpy(&message->param.setup.dialinginfo, &dialinginfo, sizeof(struct dialing_info));
				memcpy(&message->param.setup.redirinfo, &e_redirinfo, sizeof(struct redir_info));
				memcpy(&message->param.setup.callerinfo, &e_callerinfo, sizeof(struct caller_info));
				memcpy(&message->param.setup.capainfo, &e_capainfo, sizeof(struct capa_info));
				memcpy(&message->param.setup.rtpinfo, &e_rtpinfo, sizeof(struct rtp_info));
	//terminal			SCPY(message->param.setup.from_terminal, e_ext.number);
	//terminal			if (e_dialinginfo.id)
	//terminal				SCPY(message->param.setup.to_terminal, e_dialinginfo.id);
					/* handle restricted caller ids */
				apply_callerid_restriction(&e_ext, message->param.setup.callerinfo.id, &message->param.setup.callerinfo.ntype, &message->param.setup.callerinfo.present, &message->param.setup.callerinfo.screen, message->param.setup.callerinfo.extension, message->param.setup.callerinfo.name);
				apply_callerid_restriction(&e_ext, message->param.setup.callerinfo.id2, &message->param.setup.callerinfo.ntype2, &message->param.setup.callerinfo.present2, &message->param.setup.callerinfo.screen2, message->param.setup.callerinfo.extension, message->param.setup.callerinfo.name);
				apply_callerid_restriction(&e_ext, message->param.setup.redirinfo.id, &message->param.setup.redirinfo.ntype, &message->param.setup.redirinfo.present, 0, message->param.setup.redirinfo.extension, NULL);
				/* display callerid if desired for extension */
				SCPY(message->param.setup.callerinfo.display, apply_callerid_display(message->param.setup.callerinfo.id, message->param.setup.callerinfo.itype, message->param.setup.callerinfo.ntype, message->param.setup.callerinfo.present, message->param.setup.callerinfo.screen, message->param.setup.callerinfo.extension, message->param.setup.callerinfo.name));
				message_put(message);
				logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
				anycall = 1;

				/* found an interface
				 * continue only if + is given, so every interface is calles parallel */
				if (e_dialinginfo.interfaces[0] != '+')
					break;
			} while (*ifname_p);
		} while(*number_p);

		/* now we have all ports created */
		if (!anycall) {
			trace_header("INTERFACE (no free ports found)", DIRECTION_NONE);
			end_trace();
			if (!ea_endpoint->ep_join_id)
				break;
			release(RELEASE_ALL, LOCATION_PRIVATE_LOCAL, CAUSE_NOCHANNEL, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, 0); /* RELEASE_TYPE, join, port */
			return; /* must exit here */
		}
		break;
	}

}

int action_timeout(struct lcr_timer *timer, void *instance, int index)
{
	class EndpointAppPBX *ea = (class EndpointAppPBX *)instance;

	if (!ea->e_action || ea->e_state == EPOINT_STATE_CONNECT)
		return 0;

	unsched_timer(&ea->e_redial_timeout);
	PDEBUG(DEBUG_EPOINT, "EPOINT(%d) current action timed out.\n", ea->ea_endpoint->ep_serial);
	ea->e_multipoint_cause = 0;
	ea->e_multipoint_location = 0;
	ea->new_state(EPOINT_STATE_IN_OVERLAP);
	ea->e_join_pattern = 0;
	ea->process_dialing(1);
	/* we must exit, because our endpoint might be gone */

	return 0;
}

int match_timeout(struct lcr_timer *timer, void *instance, int index)
{
	class EndpointAppPBX *ea = (class EndpointAppPBX *)instance;

	if (!ea->e_action) {
		unsched_timer(&ea->e_redial_timeout);
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) we got a match timeout.\n", ea->ea_endpoint->ep_serial);
		ea->process_dialing(0);
		/* we must exit, because our endpoint might be gone */
	}

	return 0;
}

int redial_timeout(struct lcr_timer *timer, void *instance, int index)
{
	class EndpointAppPBX *ea = (class EndpointAppPBX *)instance;

	PDEBUG(DEBUG_EPOINT, "EPOINT(%d) starting redial.\n", ea->ea_endpoint->ep_serial);

	ea->new_state(EPOINT_STATE_OUT_SETUP);
	/* call special setup routine */
	ea->out_setup(0);

	return 0;
}

int powerdial_timeout(struct lcr_timer *timer, void *instance, int index)
{
	class EndpointAppPBX *ea = (class EndpointAppPBX *)instance;

	/* leave power dialing on */
	ea->e_powerdial_on = 1;
	PDEBUG(DEBUG_EPOINT, "EPOINT(%d) starting redial of powerdial.\n", ea->ea_endpoint->ep_serial);

	/* redial */
	ea->e_ruleset = ruleset_main;
	if (ea->e_ruleset)
       		ea->e_rule = ea->e_ruleset->rule_first;
	ea->e_action = NULL;
	ea->new_state(EPOINT_STATE_IN_OVERLAP);
	ea->process_dialing(0);

	return 0;
}

int cfnr_timeout(struct lcr_timer *timer, void *instance, int index)
{
	class EndpointAppPBX *ea = (class EndpointAppPBX *)instance;
	struct port_list *portlist;
	struct lcr_msg *message;

	PDEBUG(DEBUG_EPOINT, "EPOINT(%d) call-forward-no-response time has expired, hanging up.\n", ea->ea_endpoint->ep_serial);

	/* release all ports */
	while((portlist = ea->ea_endpoint->ep_portlist)) {
		message = message_create(ea->ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_RELEASE);
		message->param.disconnectinfo.cause = CAUSE_NORMAL; /* normal clearing */
		message->param.disconnectinfo.location = LOCATION_PRIVATE_LOCAL;
		message_put(message);
		ea->logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
		ea->ea_endpoint->free_portlist(portlist);
	}
	/* put on hold */
	message = message_create(ea->ea_endpoint->ep_serial, ea->ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_AUDIOPATH);
	message->param.audiopath = 0;
	message_put(message);
	/* indicate no patterns */
	message = message_create(ea->ea_endpoint->ep_serial, ea->ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_NOPATTERN);
	message_put(message);
	/* set setup state, since we have no response from the new join */
	ea->new_state(EPOINT_STATE_OUT_SETUP);

	return 0;
}

int cfnr_call_timeout(struct lcr_timer *timer, void *instance, int index)
{
	class EndpointAppPBX *ea = (class EndpointAppPBX *)instance;

	PDEBUG(DEBUG_EPOINT, "EPOINT(%d) call-forward-busy time has expired, calling the forwarded number: %s.\n", ea->ea_endpoint->ep_serial, ea->e_ext.cfnr);
	ea->out_setup(1);

	return 0;
}

int callback_timeout(struct lcr_timer *timer, void *instance, int index)
{
	class EndpointAppPBX *ea = (class EndpointAppPBX *)instance;

	if (ea->e_state == EPOINT_STATE_IDLE) {
		/* epoint is idle, check callback */
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) starting callback.\n", ea->ea_endpoint->ep_serial);
		ea->new_state(EPOINT_STATE_OUT_SETUP);
		ea->out_setup(0);
	}

	return 0;
}

int password_timeout(struct lcr_timer *timer, void *instance, int index)
{
	class EndpointAppPBX *ea = (class EndpointAppPBX *)instance;

	if (ea->e_action->index==ACTION_PASSWORD || ea->e_action->index==ACTION_PASSWORD_WRITE) {
		struct port_list *portlist;

		ea->e_ruleset = ruleset_main;
		if (ea->e_ruleset)
	       		ea->e_rule = ea->e_ruleset->rule_first;
		ea->e_action = NULL;
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) password timeout %s\n", ea->ea_endpoint->ep_serial, ea->e_extdialing);
		ea->trace_header("PASSWORD timeout", DIRECTION_NONE);
		end_trace();
		ea->e_connectedmode = 0;
		ea->e_dtmf = 0;
		ea->new_state(EPOINT_STATE_OUT_DISCONNECT);
		portlist = ea->ea_endpoint->ep_portlist;
		if (portlist) {
			ea->message_disconnect_port(portlist, CAUSE_NORMAL, LOCATION_PRIVATE_LOCAL, "", NULL);
			ea->set_tone(portlist, "cause_10");
		}
	}

	return 0;
}

/* doing a hookflash */
void EndpointAppPBX::hookflash(void)
{
	class Port *port;
	time_t now;

	/* be sure that we are active */
	notify_active();
	e_tx_state = NOTIFY_STATE_ACTIVE;

	trace_header("HOOKFLASH DTMF", DIRECTION_NONE);
	end_trace();
	if (ea_endpoint->ep_use > 1) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot hooflash while child process is running.\n", ea_endpoint->ep_serial);
		return;
	}
	/* dialtone after pressing the hash key */
	process_hangup(e_join_cause, e_join_location);
	e_multipoint_cause = 0;
	e_multipoint_location = 0;
	port = find_port_id(ea_endpoint->ep_portlist->port_id);
	if (port) {
		port->set_echotest(0);
	}
	if (ea_endpoint->ep_join_id) {
		release(RELEASE_JOIN, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, 0); /* RELEASE_TYPE, join, port */
	}
	e_ruleset = ruleset_main;
	if (e_ruleset)
	       	e_rule = e_ruleset->rule_first;
	e_action = NULL;
	new_state(EPOINT_STATE_IN_OVERLAP);
	e_connectedmode = 1;
	SCPY(e_dialinginfo.id, e_ext.prefix);
        e_extdialing = e_dialinginfo.id;
	e_join_pattern = 0;
	if (e_dialinginfo.id[0]) {
		set_tone(ea_endpoint->ep_portlist, "dialing");
		process_dialing(0);
	} else {
		set_tone(ea_endpoint->ep_portlist, "dialpbx");
	}
	time(&now);
	e_dtmf_time = now;
	e_dtmf_last = '\0';
}


/* messages from port
 */
/* port MESSAGE_SETUP */
void EndpointAppPBX::port_setup(struct port_list *portlist, int message_type, union parameter *param)
{
	struct lcr_msg		*message;
	char			buffer[256];
	int			writeext;		/* flags need to write extension after modification */
	class Port		*port;

	logmessage(message_type, param, portlist->port_id, DIRECTION_IN);
	
	portlist->port_type = param->setup.port_type;
	memcpy(&e_callerinfo, &param->setup.callerinfo, sizeof(e_callerinfo));
	memcpy(&e_dialinginfo, &param->setup.dialinginfo, sizeof(e_dialinginfo));
	memcpy(&e_redirinfo, &param->setup.redirinfo, sizeof(e_redirinfo));
	memcpy(&e_capainfo, &param->setup.capainfo, sizeof(e_capainfo));
	memcpy(&e_rtpinfo, &param->setup.rtpinfo, sizeof(e_rtpinfo));

	/* convert (inter-)national number type */
	SCPY(e_dialinginfo.id, numberrize_callerinfo(e_dialinginfo.id, e_dialinginfo.ntype, options.national, options.international));
	e_dialinginfo.ntype = INFO_NTYPE_UNKNOWN;

//	e_dtmf = param->setup.dtmf;
	/* screen incoming caller id */
	if (e_callerinfo.interface[0]) {
		do_screen(0, e_callerinfo.id, sizeof(e_callerinfo.id), &e_callerinfo.ntype, &e_callerinfo.present, e_callerinfo.interface);
		if (e_callerinfo.id2[0]) do_screen(0, e_callerinfo.id2, sizeof(e_callerinfo.id2), &e_callerinfo.ntype2, &e_callerinfo.present2, e_callerinfo.interface);
		if (e_redirinfo.id[0]) do_screen(0, e_redirinfo.id, sizeof(e_redirinfo.id), &e_redirinfo.ntype, &e_redirinfo.present, e_callerinfo.interface);
	}

	/* process extension */
	if (e_callerinfo.itype == INFO_ITYPE_ISDN_EXTENSION) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) incoming call is extension\n", ea_endpoint->ep_serial);
		/* port makes call from extension */
		SCPY(e_callerinfo.extension, e_callerinfo.id);
		SCPY(e_ext.number, e_callerinfo.extension);
		SCPY(e_extension_interface, e_callerinfo.interface);
	} else {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) incoming call is external or voip\n", ea_endpoint->ep_serial);
	}

	if (e_callerinfo.itype == INFO_ITYPE_ISDN_EXTENSION) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) call from extension '%s'\n", ea_endpoint->ep_serial, e_ext.number);

		/* get extension's info about caller */
		if (!read_extension(&e_ext, e_ext.number)) {
			/* extension doesn't exist */
			trace_header("EXTENSION (not created)", DIRECTION_IN);
			add_trace("extension", NULL, "%s", e_ext.number);
			end_trace();
			message_disconnect_port(portlist, CAUSE_REJECTED, LOCATION_PRIVATE_LOCAL, "", NULL);
			new_state(EPOINT_STATE_OUT_DISCONNECT);
			set_tone(portlist, "cause_80"); /* pbx cause: extension not authorized */
			e_ext.number[0] = '\0'; /* no terminal */
			return;
		}
		writeext = 0;

		/* put prefix (next) in front of e_dialinginfo.id */
		if (e_ext.next[0]) {
			SPRINT(buffer, "%s%s", e_ext.next, e_dialinginfo.id);
			SCPY(e_dialinginfo.id, buffer);
			e_ext.next[0] = '\0';
			writeext = 1;
		} else if (e_ext.prefix[0]) {
			SPRINT(buffer, "%s%s", e_ext.prefix, e_dialinginfo.id);
			SCPY(e_dialinginfo.id, buffer);
		}

		/* screen caller id by extension's config */
		e_callerinfo.screen = INFO_SCREEN_NETWORK;
		if (e_ext.name[0])
			SCPY(e_callerinfo.name, e_ext.name);
		/* use caller id (or if exist: id_next_call) for this call */
		if (e_ext.id_next_call_present >= 0) {
			SCPY(e_callerinfo.id, e_ext.id_next_call);
			/* if we restrict the pesentation */
			if (e_ext.id_next_call_present==INFO_PRESENT_ALLOWED && e_callerinfo.present==INFO_PRESENT_RESTRICTED)
				e_callerinfo.present = INFO_PRESENT_RESTRICTED;
			else	e_callerinfo.present = e_ext.id_next_call_present;
			e_callerinfo.ntype = e_ext.id_next_call_type;
			e_ext.id_next_call_present = -1;
			writeext = 1;
		} else {
			SCPY(e_callerinfo.id, e_ext.callerid);
			/* if we restrict the pesentation */
			if (e_ext.callerid_present==INFO_PRESENT_ALLOWED && e_callerinfo.present==INFO_PRESENT_RESTRICTED)
				e_callerinfo.present = INFO_PRESENT_RESTRICTED;
			else	e_callerinfo.present = e_ext.callerid_present;
			e_callerinfo.ntype = e_ext.callerid_type;
		}
		e_callerinfo.ntype2 = INFO_NTYPE_NOTPRESENT;

		/* extension is written */
		if (writeext)
			write_extension(&e_ext, e_ext.number);

		/* set volume of rx and tx */
		if (param->setup.callerinfo.itype == INFO_ITYPE_ISDN_EXTENSION)
		if (e_ext.tx_gain!=0 || e_ext.rx_gain!=0) {
			message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_mISDNSIGNAL);
			message->param.mISDNsignal.message = mISDNSIGNAL_VOLUME;
			message->param.mISDNsignal.rx_gain = e_ext.tx_gain;
			message->param.mISDNsignal.tx_gain = e_ext.rx_gain;
			message_put(message);
		}

		/* start recording if enabled */
		if (e_ext.record!=CODEC_OFF && (e_capainfo.bearer_capa==INFO_BC_SPEECH || e_capainfo.bearer_capa==INFO_BC_AUDIO)) {
			/* check if we are a terminal */
			if (e_ext.number[0] == '\0')
				PERROR("Port(%d) cannot record because we are not a terminal\n", ea_endpoint->ep_serial);
			else {
				port = find_port_id(portlist->port_id);
				if (port)
					port->open_record(e_ext.record, 0, 0, e_ext.number, e_ext.anon_ignore, "", 0);
			}
		}
	} else {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) call from external port\n", ea_endpoint->ep_serial);
		/* no terminal identification */
		e_ext.number[0] = '\0';
		e_extension_interface[0] = '\0';
		memset(&e_ext, 0, sizeof(e_ext));
		e_ext.rights = 4; /* right to dial internat */
	}

	/* incoming call */
	e_ruleset = ruleset_main;
	if (e_ruleset)
       		e_rule = e_ruleset->rule_first;
	e_action = NULL;
        e_extdialing = e_dialinginfo.id;
	new_state(EPOINT_STATE_IN_SETUP);
	if (e_dialinginfo.id[0]) {
		set_tone(portlist, "dialing");
	} else {
		if (e_ext.number[0])
			set_tone(portlist, "dialpbx");
		else
			set_tone(portlist, "dialtone");
	}
	process_dialing(0);
	if (e_state == EPOINT_STATE_IN_SETUP) {
		/* request MORE info, if not already at higher state */
		new_state(EPOINT_STATE_IN_OVERLAP);
		message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_OVERLAP);
		message_put(message);
		logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
	}
}

/* port MESSAGE_INFORMATION */
void EndpointAppPBX::port_information(struct port_list *portlist, int message_type, union parameter *param)
{
	struct lcr_msg		*message;

	logmessage(message_type, param, portlist->port_id, DIRECTION_IN);

	/* ignore information message without digit information */
	if (!param->information.id[0])
		return;

	e_overlap = 1;

	/* turn off dtmf detection, in case dtmf is sent with keypad information */
	if (e_dtmf) {
		trace_header("DTMF (disabling due to keypad)", DIRECTION_IN);
		end_trace();
		e_dtmf = 0;
	}

	/* if vbox_play is done, the information are just used as they come */
	if (e_action)
	if (e_action->index == ACTION_VBOX_PLAY) {
		/* concat dialing string */
		SCAT(e_dialinginfo.id, param->information.id);
		process_dialing(0);
		return;
	}

	/* keypad when disconnect but in connected mode */
	if (e_state==EPOINT_STATE_OUT_DISCONNECT && e_connectedmode) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) keypad information received after disconnect: %s.\n", ea_endpoint->ep_serial, param->information.id);
		/* processing keypad function */
		if (param->information.id[0] == '0') {
			hookflash();
		}
		return;
	}

	/* keypad when connected */
	if (e_state == EPOINT_STATE_CONNECT || e_state == EPOINT_STATE_IN_ALERTING) {
		if (e_enablekeypad) {
			message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, message_type);
			memcpy(&message->param, param, sizeof(union parameter));
			message_put(message);
			return;
		}
		if (e_ext.keypad) {
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d) keypad information received during connect: %s.\n", ea_endpoint->ep_serial, param->information.id);
			/* processing keypad function */
			if (param->information.id[0] == '0') {
				hookflash();
			}
			if (param->information.id[0])
				keypad_function(param->information.id[0]);
		} else {
			if (e_ext.number[0])
				trace_header("KEYPAD (not enabled by extension's settings)", DIRECTION_IN);
			else
				trace_header("KEYPAD (not enabled for external interfaces)", DIRECTION_IN);
			end_trace();
		}
		return;
	}
	if (e_state != EPOINT_STATE_IN_OVERLAP) {
		if (e_ext.number[0])
			trace_header("KEYPAD (ignored, not connected and not dialing)", DIRECTION_IN);
		else
			trace_header("KEYPAD (not enabled for external interfaces)", DIRECTION_IN);
		end_trace();
		return;
	}
	if (!param->information.id[0])
		return;
	if (e_dialinginfo.id[0]=='\0' && !e_action) {
		set_tone(portlist, "dialing");
	}
	if (e_action)
	if (e_action->index==ACTION_OUTDIAL
	 || e_action->index==ACTION_EXTERNAL) {
		if (!e_extdialing)
			set_tone(portlist, "dialing");
		else if (!e_extdialing[0])
			set_tone(portlist, "dialing");
	}
	/* concat dialing string */
	SCAT(e_dialinginfo.id, param->information.id);
	process_dialing(0);
}

/* port MESSAGE_DTMF */
void EndpointAppPBX::port_dtmf(struct port_list *portlist, int message_type, union parameter *param)
{
	time_t now;
	struct lcr_msg		*message;

	time(&now);

	/* only if dtmf detection is enabled */
	if (!e_dtmf) {
		trace_header("DTMF (disabled)", DIRECTION_IN);
		end_trace();
		return;
	}
	trace_header("DTMF", DIRECTION_IN);
	add_trace("digit", NULL, "%c", param->dtmf);
	end_trace();

#if 0
NOTE: vbox is now handled due to overlap state
	/* if vbox_play is done, the dtmf digits are just used as they come */
	if (e_action)
	if (e_action->index == ACTION_VBOX_PLAY) {
		/* concat dialing string */
		if (strlen(e_dialinginfo.id)+1 < sizeof(e_dialinginfo.id)) {
			e_dialinginfo.id[strlen(e_dialinginfo.id)+1] = '\0';
			e_dialinginfo.id[strlen(e_dialinginfo.id)] = param->dtmf;
			process_dialing(0);
		}
		/* continue to process *X# sequences */
	}
#endif

	/* check for *X# sequence */
	if (e_state == EPOINT_STATE_CONNECT || e_state == EPOINT_STATE_IN_ALERTING) {
		if (e_enablekeypad) {
			message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, message_type);
			memcpy(&message->param, param, sizeof(union parameter));
			message_put(message);
			return;
		}
		if (e_dtmf_time+3 < now) {
			/* the last digit was too far in the past to be a sequence */
			if (param->dtmf == '*')
				/* only start is allowed in the sequence */
				e_dtmf_last = '*';
			else
				e_dtmf_last = '\0';
		} else {
			/* we have a sequence of digits, see what we got */
			if (param->dtmf == '*')
				e_dtmf_last = '*';
			else if (param->dtmf>='0' && param->dtmf<='9') {
				/* we need to have a star before we receive the digit of the sequence */
				if (e_dtmf_last == '*')
					e_dtmf_last = param->dtmf;
			} else if (param->dtmf == '#') {
				/* the hash key */
				if (e_dtmf_last>='0' && e_dtmf_last<='9') {
					PDEBUG(DEBUG_EPOINT, "EPOINT(%d) dtmf sequence *%c# detected.\n", ea_endpoint->ep_serial, e_dtmf_last);
					if (e_dtmf_last == '0') {
						hookflash();
						return;
					}
					/* processing keypad function */
					if (param->dtmf)
						keypad_function(e_dtmf_last);
					e_dtmf_last = '\0';
				}
			}
		}

		/* set last time of dtmf */
		e_dtmf_time = now;
		return;
	}

	/* check for ## hookflash during dialing */
	if (e_action)
	if (e_action->index==ACTION_PASSWORD
	 || e_action->index==ACTION_PASSWORD_WRITE)
		goto password;
	if (param->dtmf=='#') { /* current digit is '#' */
		if (e_state==EPOINT_STATE_IN_DISCONNECT
		 || (e_state!=EPOINT_STATE_CONNECT && e_dtmf_time+3>=now && e_dtmf_last=='#')) { /* when disconnected, just #. when dialing, ##. */
			hookflash();
			return;
		} else {
			e_dtmf_time = now;
			e_dtmf_last = '#';
		}
	} else {
		password:
		e_dtmf_time = now;
		e_dtmf_last = '\0';
	}
	

	/* dialing using dtmf digit */
	if (e_state==EPOINT_STATE_IN_OVERLAP){ // && e_state==e_connectedmode)
		if (e_dialinginfo.id[0]=='\0' && !e_action) {
			set_tone(portlist, "dialing");
		}
		/* concat dialing string */
		if (strlen(e_dialinginfo.id)+1 < sizeof(e_dialinginfo.id)) {
			e_dialinginfo.id[strlen(e_dialinginfo.id)+1] = '\0';
			e_dialinginfo.id[strlen(e_dialinginfo.id)] = param->dtmf;
			process_dialing(0);
		}
	}
}

/* port MESSAGE_CRYPT */
void EndpointAppPBX::port_crypt(struct port_list *portlist, int message_type, union parameter *param)
{
#ifdef WITH_CRYPT
	/* send crypt response to cryptman */
	if (param->crypt.type == CR_MESSAGE_IND)
		cryptman_msg2man(param->crypt.data, param->crypt.len);
	else
		cryptman_message(param->crypt.type, param->crypt.data, param->crypt.len);
#endif
}

/* port MESSAGE_OVERLAP */
void EndpointAppPBX::port_overlap(struct port_list *portlist, int message_type, union parameter *param)
{
	struct lcr_msg *message;

	logmessage(message_type, param, portlist->port_id, DIRECTION_IN);

	/* signal to call tool */
	admin_call_response(e_adminid, ADMIN_CALL_SETUP_ACK, "", 0, 0, 0);

	if (e_dialing_queue[0] && portlist) {
		/* send what we have not dialed yet, because we had no setup complete */
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) dialing pending digits: '%s'\n", ea_endpoint->ep_serial, e_dialing_queue);
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_portlist->port_id, EPOINT_TO_PORT, MESSAGE_INFORMATION);
		SCPY(message->param.information.id, e_dialing_queue);
		message->param.information.ntype = INFO_NTYPE_UNKNOWN;
		message_put(message);
		logmessage(message->type, &message->param, ea_endpoint->ep_portlist->port_id, DIRECTION_OUT);
		e_dialing_queue[0] = '\0';
	}
	/* check if pattern is available */
	if (!ea_endpoint->ep_portlist->next && portlist->early_b) { /* one port_list relation and tones available */
		/* indicate patterns */
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_PATTERN);
		message_put(message);

		/* connect audio, if not already */
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_AUDIOPATH);
		message->param.audiopath = 1;
		message_put(message);
	} else {
		/* indicate no patterns */
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_NOPATTERN);
		message_put(message);

		/* disconnect audio, if not already */
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_AUDIOPATH);
		message->param.audiopath = 0;
		message_put(message);
	}
	new_state(EPOINT_STATE_OUT_OVERLAP);
	/* if we are in a join */
	if (ea_endpoint->ep_join_id) { 
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, message_type);
		memcpy(&message->param, param, sizeof(union parameter));
		message_put(message);
	}
}

/* port MESSAGE_PROCEEDING */
void EndpointAppPBX::port_proceeding(struct port_list *portlist, int message_type, union parameter *param)
{
	struct lcr_msg *message;

	logmessage(message_type, param, portlist->port_id, DIRECTION_IN);

	/* signal to call tool */
	admin_call_response(e_adminid, ADMIN_CALL_PROCEEDING, "", 0, 0, 0);

	e_state = EPOINT_STATE_OUT_PROCEEDING;
	/* check if pattern is availatle */
	if (!ea_endpoint->ep_portlist->next && (portlist->early_b || portlist->port_type==PORT_TYPE_VBOX_OUT)) { /* one port_list relation and tones available */
		/* indicate patterns */
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_PATTERN);
		message_put(message);

		/* connect audio, if not already */
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_AUDIOPATH);
		message->param.audiopath = 1;
		message_put(message);
	} else {
		/* indicate no patterns */
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_NOPATTERN);
		message_put(message);

		/* disconnect audio, if not already */
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_AUDIOPATH);
		message->param.audiopath = 0;
		message_put(message);
	}
	/* if we are in a call */
	if (ea_endpoint->ep_join_id) { 
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, message_type);
		memcpy(&message->param, param, sizeof(union parameter));
		message_put(message);
	}
}

/* port MESSAGE_ALERTING */
void EndpointAppPBX::port_alerting(struct port_list *portlist, int message_type, union parameter *param)
{
	struct lcr_msg *message;

	logmessage(message_type, param, portlist->port_id, DIRECTION_IN);

	/* signal to call tool */
	admin_call_response(e_adminid, ADMIN_CALL_ALERTING, "", 0, 0, 0);
//#warning hack!!
//	if (e_adminid)
//		set_tone(portlist, "hold");

	new_state(EPOINT_STATE_OUT_ALERTING);
	/* check if pattern is available */
	if (!ea_endpoint->ep_portlist->next && (portlist->early_b || portlist->port_type==PORT_TYPE_VBOX_OUT)) { /* one port_list relation and tones available */
		/* indicate patterns */
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_PATTERN);
		message_put(message);

		/* connect audio, if not already */
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_AUDIOPATH);
		message->param.audiopath = 1;
		message_put(message);
	} else {
		/* indicate no patterns */
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_NOPATTERN);
		message_put(message);

		/* disconnect audio, if not already */
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_AUDIOPATH);
		message->param.audiopath = 0;
		message_put(message);
	}
	/* if we are in a call */
	if (ea_endpoint->ep_join_id) { 
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, message_type);
		memcpy(&message->param, param, sizeof(union parameter));
		message_put(message);
	}
}

/* port MESSAGE_CONNECT */
void EndpointAppPBX::port_connect(struct port_list *portlist, int message_type, union parameter *param)
{
	struct lcr_msg *message;
	char buffer[256];
	unsigned int port_id = portlist->port_id;
	struct port_list *tportlist;
	class Port *port;
	time_t now;

	logmessage(message_type, param, portlist->port_id, DIRECTION_IN);

	/* signal to call tool */
	admin_call_response(e_adminid, ADMIN_CALL_CONNECT, numberrize_callerinfo(param->connectinfo.id,param->connectinfo.ntype, options.national, options.international), 0, 0, 0);

	memcpy(&e_connectinfo, &param->connectinfo, sizeof(e_connectinfo));
	PDEBUG(DEBUG_EPOINT, "EPOINT(%d) removing all other ports (start)\n", ea_endpoint->ep_serial);
	while(ea_endpoint->ep_portlist->next) { /* as long as we have at least two ports */
		tportlist = ea_endpoint->ep_portlist;
		if (tportlist->port_id == port_id) /* if the first portlist is the calling one, the second must be a different one */
			tportlist = tportlist->next;
		if (tportlist->port_id == port_id)
			FATAL("EPOINT(%d) this should not happen since the portlist list must not have two links to the same port - exitting.\n");
		message = message_create(ea_endpoint->ep_serial, tportlist->port_id, EPOINT_TO_PORT, MESSAGE_RELEASE);
		message->param.disconnectinfo.cause = CAUSE_NONSELECTED; /* non selected user clearing */
		message->param.disconnectinfo.location = LOCATION_PRIVATE_LOCAL;
		message_put(message);
		logmessage(message->type, &message->param, tportlist->port_id, DIRECTION_OUT);
		ea_endpoint->free_portlist(tportlist);
	}
	PDEBUG(DEBUG_EPOINT, "EPOINT(%d) removing all other ports (end)\n", ea_endpoint->ep_serial);

	time(&now);
	e_start = now;

	if (e_callerinfo.interface[0])
		do_screen(0, e_connectinfo.id, sizeof(e_connectinfo.id), &e_connectinfo.ntype, &e_connectinfo.present, e_connectinfo.interface);

	/* screen connected name */
	if (e_ext.name[0])
		SCPY(e_connectinfo.name, e_ext.name);

	/* add internal id to colp */
	SCPY(e_connectinfo.extension, e_ext.number);

	/* we store the connected port number */
	SCPY(e_extension_interface, e_connectinfo.interface);

	/* for internal and am calls, we get the extension's id */
	if (portlist->port_type==PORT_TYPE_VBOX_OUT || e_ext.colp==COLP_HIDE) {
		SCPY(e_connectinfo.id, e_ext.callerid);
		SCPY(e_connectinfo.extension, e_ext.number);
		e_connectinfo.itype = INFO_ITYPE_ISDN_EXTENSION;
		e_connectinfo.ntype = e_ext.callerid_type;
		e_connectinfo.present = e_ext.callerid_present;
	}
	if (portlist->port_type==PORT_TYPE_VBOX_OUT) {
		e_connectinfo.itype = INFO_ITYPE_VBOX;
		e_connectinfo.ntype = INFO_NTYPE_UNKNOWN;
	}

	new_state(EPOINT_STATE_CONNECT);

	/* set volume of rx and tx */
	if (e_ext.tx_gain!=0 || e_ext.rx_gain!=0) {
		message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_mISDNSIGNAL);
		message->param.mISDNsignal.message = mISDNSIGNAL_VOLUME;
		message->param.mISDNsignal.rx_gain = e_ext.tx_gain;
		message->param.mISDNsignal.tx_gain = e_ext.rx_gain;
		message_put(message);
	}

	unsched_timer(&e_cfnr_timeout);
	unsched_timer(&e_cfnr_call_timeout);
	if (e_ext.number[0])
		e_dtmf = 1; /* allow dtmf */

	/* modify colp */
	/* other calls with no caller id (or not available for the extension) and force colp */
	if ((e_connectinfo.id[0]=='\0' || (e_connectinfo.present==INFO_PRESENT_RESTRICTED && !e_ext.anon_ignore))&& e_ext.colp==COLP_FORCE) {
		e_connectinfo.ntype = INFO_NTYPE_NOTPRESENT;
		if ((portlist->port_type & PORT_CLASS_DIR_MASK) == PORT_CLASS_DIR_OUT) {
			/* external extension answered */
			port = find_port_id(portlist->port_id);
			if (port) {
				SCPY(e_connectinfo.id, nationalize_callerinfo(port->p_dialinginfo.id, &e_connectinfo.ntype, options.national, options.international));
				e_connectinfo.present = INFO_PRESENT_ALLOWED;
			}
		}
	}

	/* send connect to join */
	if (ea_endpoint->ep_join_id) {
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, message_type);
		memcpy(&message->param.connectinfo, &e_connectinfo, sizeof(struct connect_info));
		message_put(message);

		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_AUDIOPATH);
		message->param.audiopath = 1;
		message_put(message);
	} else if (!e_adminid) {
		/* callback */
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) we have a callback, so we create a call with cbcaller: \"%s\".\n", ea_endpoint->ep_serial, e_cbcaller);
		SCPY(e_ext.number, e_cbcaller);
		new_state(EPOINT_STATE_IN_OVERLAP);
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) callback from extension '%s'\n", ea_endpoint->ep_serial, e_ext.number);

		/* get extension's info about terminal */
		if (!read_extension(&e_ext, e_ext.number)) {
			/* extension doesn't exist */
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d) rejecting callback from not existing extension: '%s'\n", ea_endpoint->ep_serial, e_ext.number);
			message_disconnect_port(portlist, CAUSE_REJECTED, LOCATION_PRIVATE_LOCAL, "", NULL);
			new_state(EPOINT_STATE_OUT_DISCONNECT);
			set_tone(portlist, "cause_80"); /* pbx cause: extension not authorized */
			return;
		}

		/* put prefix in front of e_cbdialing */
		SPRINT(buffer, "%s%s", e_ext.prefix, e_cbdialing);
		SCPY(e_dialinginfo.id, buffer);
		e_dialinginfo.itype = INFO_ITYPE_ISDN;
		e_dialinginfo.ntype = INFO_NTYPE_UNKNOWN;

		/* use caller id (or if exist: id_next_call) for this call */
		e_callerinfo.screen = INFO_SCREEN_NETWORK;
		SCPY(e_callerinfo.extension, e_ext.number);
		if (e_ext.id_next_call_present >= 0) {
			SCPY(e_callerinfo.id, e_ext.id_next_call);
			e_callerinfo.present = e_ext.id_next_call_present;
			e_callerinfo.ntype = e_ext.id_next_call_type;
			e_ext.id_next_call_present = -1;
			/* extension is written */
			write_extension(&e_ext, e_ext.number);
		} else {
			SCPY(e_callerinfo.id, e_ext.callerid);
			e_callerinfo.present = e_ext.callerid_present;
			e_callerinfo.ntype = e_ext.callerid_type;
		}
		e_callerinfo.ntype2 = INFO_NTYPE_NOTPRESENT;

		e_connectedmode = 1; /* dtmf-hangup & disconnect prevention */
		e_dtmf = 1;

		/* check if caller id is NOT authenticated */
		if (!parse_callbackauth(e_ext.number, &e_callbackinfo)) {
			/* make call state to enter password */
			new_state(EPOINT_STATE_IN_OVERLAP);
			e_action = &action_password_write;
			unsched_timer(&e_match_timeout);
			e_match_to_action = NULL;
			e_dialinginfo.id[0] = '\0';
			e_extdialing = strchr(e_dialinginfo.id, '\0');
			schedule_timer(&e_password_timeout, 20, 0);
			process_dialing(0);
		} else {
			/* incoming call (callback) */
			e_ruleset = ruleset_main;
			if (e_ruleset)
				e_rule = e_ruleset->rule_first;
			e_action = NULL;
			e_extdialing = e_dialinginfo.id;
			if (e_dialinginfo.id[0]) {
				set_tone(portlist, "dialing");
				process_dialing(0);
			} else {
				set_tone(portlist, "dialpbx");
			}
		}
	} else { /* testcall */
		set_tone(portlist, "hold");
	}

	/* start recording if enabled, not when answering machine answers */
	if (param->connectinfo.itype!=INFO_ITYPE_VBOX && e_ext.number[0] && e_ext.record!=CODEC_OFF && (e_capainfo.bearer_capa==INFO_BC_SPEECH || e_capainfo.bearer_capa==INFO_BC_AUDIO)) {
		/* check if we are a terminal */
		if (e_ext.number[0] == '\0')
			PERROR("Port(%d) cannot record because we are not a terminal\n", ea_endpoint->ep_serial);
		else {
			port = find_port_id(portlist->port_id);
			if (port)
				port->open_record(e_ext.record, 0, 0, e_ext.number, e_ext.anon_ignore, "", 0);
		}
	}
}

/* port MESSAGE_DISCONNECT MESSAGE_RELEASE */
void EndpointAppPBX::port_disconnect_release(struct port_list *portlist, int message_type, union parameter *param)
{
	struct lcr_msg	*message;
	char		buffer[256];
	unsigned int	port_id = portlist->port_id;
	int		cause,
			location;

	logmessage(message_type, param, portlist->port_id, DIRECTION_IN);

	/* signal to call tool */
	admin_call_response(e_adminid, (message_type==MESSAGE_DISCONNECT)?ADMIN_CALL_DISCONNECT:ADMIN_CALL_RELEASE, "", param->disconnectinfo.cause, param->disconnectinfo.location, 0);

//#warning does this work? only disconnect when incoming port hat not already disconnected yet?
	if (e_state==EPOINT_STATE_IN_DISCONNECT && message_type!=MESSAGE_RELEASE){ // || e_state==EPOINT_STATE_OUT_DISCONNECT || e_state==EPOINT_STATE_IDLE)
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) ignored because we are already disconnected.\n", ea_endpoint->ep_serial);
		return;
	}

	/* collect cause */
	PDEBUG(DEBUG_EPOINT, "EPOINT(%d) current multipoint cause %d location %d, received cause %d location %d.\n", ea_endpoint->ep_serial, e_multipoint_cause, e_multipoint_location, param->disconnectinfo.cause, param->disconnectinfo.location);
	collect_cause(&e_multipoint_cause, &e_multipoint_location, param->disconnectinfo.cause, param->disconnectinfo.location);
	PDEBUG(DEBUG_EPOINT, "EPOINT(%d) new multipoint cause %d location %d.\n", ea_endpoint->ep_serial, e_multipoint_cause, e_multipoint_location);

	/* check if we have more than one portlist relation and we just ignore the disconnect */
	if (ea_endpoint->ep_portlist) if (ea_endpoint->ep_portlist->next) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) the disconnect was from a multipoint call. we just release that relation.\n", ea_endpoint->ep_serial);
		portlist = ea_endpoint->ep_portlist;
		while(portlist) {
			if (portlist->port_id == port_id)
				break;
			portlist = portlist->next;
		}
		if (!portlist)
			FATAL("EPOINT(%d) no portlist related to the calling port.\n", ea_endpoint->ep_serial);
		if (message_type != MESSAGE_RELEASE) {
			message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_RELEASE);
			message->param.disconnectinfo.cause = CAUSE_NORMAL; /* normal clearing */
			message->param.disconnectinfo.location = LOCATION_PRIVATE_LOCAL;
			message_put(message);
			logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
		}
		ea_endpoint->free_portlist(portlist);
		return; /* one relation removed */ 
	}
	if (e_state == EPOINT_STATE_CONNECT) {
		/* use cause from port after connect */
		cause = param->disconnectinfo.cause;
		location = param->disconnectinfo.location;
	} else {
		/* use multipoint cause if no connect yet */
		if (e_multipoint_cause) {
			cause = e_multipoint_cause;
			location = e_multipoint_location;
		} else {
			cause = CAUSE_NOUSER;
			location = LOCATION_PRIVATE_LOCAL;
		}
	}

	unsched_timer(&e_cfnr_timeout);
	unsched_timer(&e_cfnr_call_timeout);

	/* process hangup */
	process_hangup(e_join_cause, e_join_location);
	e_multipoint_cause = 0;
	e_multipoint_location = 0;

	if (message_type == MESSAGE_DISCONNECT) {
		/* tone to disconnected end */
		SPRINT(buffer, "cause_%02x", cause);
		if (ea_endpoint->ep_portlist)
			set_tone(ea_endpoint->ep_portlist, buffer);

		new_state(EPOINT_STATE_IN_DISCONNECT);
	}

	if (ea_endpoint->ep_join_id) {
		int haspatterns = 0;
		/* check if pattern is available */
		if (ea_endpoint->ep_portlist)
		if (!ea_endpoint->ep_portlist->next && ea_endpoint->ep_portlist->early_b)
		if (joinpbx_countrelations(ea_endpoint->ep_join_id)==2 // we must count relations, in order not to disturb the conference ; NOTE: asterisk always counts two, since it is a point to point call 
		 && message_type != MESSAGE_RELEASE) // if we release, we are done
			haspatterns = 1;
		if (haspatterns) {
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d) the port has patterns.\n", ea_endpoint->ep_serial);
			/* indicate patterns */
			message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_PATTERN);
			message_put(message);
			/* connect audio, if not already */
			message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_AUDIOPATH);
			message->param.audiopath = 1;
			message_put(message);
			/* send disconnect */
			message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, message_type);
			memcpy(&message->param, param, sizeof(union parameter));
			message_put(message);
			/* disable encryption if disconnected */
//PERROR("REMOVE ME: state =%d, %d\n", e_crypt_state, e_crypt);
#ifdef WITH_CRYPT
			if (e_crypt_state)
				cryptman_message(CI_DISCONNECT_IND, NULL, 0);
#endif
			return;
		} else {
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d) the port has no patterns.\n", ea_endpoint->ep_serial);
		}
	}
	if (message_type == MESSAGE_RELEASE)
		ea_endpoint->free_portlist(portlist);
	release(RELEASE_ALL, location, cause, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, 0); /* RELEASE_TYPE, callcause, portcause */
	return; /* must exit here */
}

/* port MESSAGE_TIMEOUT */
void EndpointAppPBX::port_timeout(struct port_list *portlist, int message_type, union parameter *param)
{
	char cause[16];

	trace_header("TIMEOUT", DIRECTION_IN);
	message_type = MESSAGE_DISCONNECT;
	switch (param->state) {
		case PORT_STATE_OUT_SETUP:
		case PORT_STATE_OUT_OVERLAP:
		add_trace("state", NULL, "outgoing setup/dialing");
		end_trace();
		/* no user responding */
		release(RELEASE_ALL, LOCATION_PRIVATE_LOCAL, CAUSE_NOUSER, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, 0);
		return; /* must exit here */

		case PORT_STATE_IN_SETUP:
		case PORT_STATE_IN_OVERLAP:
		add_trace("state", NULL, "incoming setup/dialing");
		param->disconnectinfo.cause = CAUSE_INVALID; /* number incomplete */
		param->disconnectinfo.location = LOCATION_PRIVATE_LOCAL;
		break;

		case PORT_STATE_OUT_PROCEEDING:
		add_trace("state", NULL, "outgoing proceeding");
		end_trace();
		param->disconnectinfo.cause = CAUSE_NOUSER; /* no user responding */
		param->disconnectinfo.location = LOCATION_PRIVATE_LOCAL;
		release(RELEASE_ALL, LOCATION_PRIVATE_LOCAL, CAUSE_NOUSER, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, 0);
		return; /* must exit here */

		case PORT_STATE_IN_PROCEEDING:
		add_trace("state", NULL, "incoming proceeding");
		param->disconnectinfo.cause = CAUSE_NOUSER;
		param->disconnectinfo.location = LOCATION_PRIVATE_LOCAL; /* no user responding */
		break;

		case PORT_STATE_OUT_ALERTING:
		add_trace("state", NULL, "outgoing alerting");
		end_trace();
		param->disconnectinfo.cause = CAUSE_NOANSWER; /* no answer */
		param->disconnectinfo.location = LOCATION_PRIVATE_LOCAL;
		release(RELEASE_ALL, LOCATION_PRIVATE_LOCAL, CAUSE_NOANSWER, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, 0);
		return; /* must exit here */

		case PORT_STATE_CONNECT:
		add_trace("state", NULL, "connect");
		end_trace();
		param->disconnectinfo.cause = CAUSE_NORMAL; /* normal */
		param->disconnectinfo.location = LOCATION_PRIVATE_LOCAL;
		release(RELEASE_ALL, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, 0);
		return; /* must exit here */

		case PORT_STATE_IN_ALERTING:
		add_trace("state", NULL, "incoming alerting");
		param->disconnectinfo.cause = CAUSE_NOANSWER;
		param->disconnectinfo.location = LOCATION_PRIVATE_LOCAL;
		break;

		case PORT_STATE_IN_DISCONNECT:
		case PORT_STATE_OUT_DISCONNECT:
		add_trace("state", NULL, "disconnect");
		end_trace();
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) in this special case, we release due to disconnect timeout.\n", ea_endpoint->ep_serial);
		release(RELEASE_ALL, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, 0);
		return; /* must exit here */

		default:
		param->disconnectinfo.cause = 31; /* normal unspecified */
		param->disconnectinfo.location = LOCATION_PRIVATE_LOCAL;
	}
	end_trace();
	/* release call, disconnect isdn */
	e_join_pattern = 0;
	new_state(EPOINT_STATE_OUT_DISCONNECT);
	SPRINT(cause, "cause_%02x", param->disconnectinfo.cause);
	SCPY(e_tone, cause);
	while(portlist) {
		set_tone(portlist, cause);
		message_disconnect_port(portlist, param->disconnectinfo.cause, param->disconnectinfo.location, "", NULL);
		portlist = portlist->next;
	}
	release(RELEASE_JOIN, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, 0); /* RELEASE_TYPE, join, port */
}

/* port MESSAGE_NOTIFY */
void EndpointAppPBX::port_notify(struct port_list *portlist, int message_type, union parameter *param)
{
	logmessage(message_type, param, portlist->port_id, DIRECTION_IN);

	struct lcr_msg *message;

	/* signal to call tool */
	admin_call_response(e_adminid, ADMIN_CALL_NOTIFY, numberrize_callerinfo(param->notifyinfo.id,param->notifyinfo.ntype, options.national, options.international), 0, 0, param->notifyinfo.notify);
	if (param->notifyinfo.notify) {
		e_rx_state = track_notify(e_rx_state, param->notifyinfo.notify);
	}

	/* if we get notification from stack, local shows if we disabled/enabled audio stream */
	if (param->notifyinfo.local) switch(param->notifyinfo.notify) {
		case INFO_NOTIFY_REMOTE_HOLD:
		case INFO_NOTIFY_USER_SUSPENDED:
		/* tell call about it */
		if (ea_endpoint->ep_join_id) {
			message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_AUDIOPATH);
			message->param.audiopath = 0;
			message_put(message);
		}
		break;

		case INFO_NOTIFY_REMOTE_RETRIEVAL:
		case INFO_NOTIFY_USER_RESUMED:
		/* set volume of rx and tx */
		if (param->setup.callerinfo.itype == INFO_ITYPE_ISDN_EXTENSION)
		if (e_ext.tx_gain!=0 || e_ext.rx_gain!=0)
		if (portlist) {
			message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_mISDNSIGNAL);
			message->param.mISDNsignal.message = mISDNSIGNAL_VOLUME;
			message->param.mISDNsignal.rx_gain = e_ext.tx_gain;
			message->param.mISDNsignal.tx_gain = e_ext.rx_gain;
			message_put(message);
		}
		/* set current tone */
		if (portlist)
			set_tone(portlist, e_tone);
		/* tell call about it */
		if (ea_endpoint->ep_join_id) {
			message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_AUDIOPATH);
			message->param.audiopath = 1;
			message_put(message);
		}
		break;
	}

	/* notify call if available */
	if (ea_endpoint->ep_join_id) {
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_NOTIFY);
		memcpy(&message->param.notifyinfo, &param->notifyinfo, sizeof(struct notify_info));
		message_put(message);
	}

}

/* port MESSAGE_PROGRESS */
void EndpointAppPBX::port_progress(struct port_list *portlist, int message_type, union parameter *param)
{
	logmessage(message_type, param, portlist->port_id, DIRECTION_IN);

	struct lcr_msg *message;

	/* signal to call tool */
	admin_call_response(e_adminid, ADMIN_CALL_PROGRESS, "", 0, param->progressinfo.location, param->progressinfo.progress);

	/* send progress to call if available */
	if (ea_endpoint->ep_join_id) {
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_PROGRESS);
		memcpy(&message->param.progressinfo, &param->progressinfo, sizeof(struct progress_info));
		message_put(message);
	}

}

/* port MESSAGE_FACILITY */
void EndpointAppPBX::port_facility(struct port_list *portlist, int message_type, union parameter *param)
{
	logmessage(message_type, param, portlist->port_id, DIRECTION_IN);

	struct lcr_msg *message;

	message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_FACILITY);
	memcpy(&message->param.facilityinfo, &param->facilityinfo, sizeof(struct facility_info));
	message_put(message);
}

/* port MESSAGE_3PTY */
void EndpointAppPBX::port_3pty(struct port_list *portlist, int message_type, union parameter *param)
{
	logmessage(message_type, param, portlist->port_id, DIRECTION_IN);

	struct lcr_msg *message;
	int rc;

	/* 3PTY bridge */
	if (param->threepty.begin)
		rc = join_3pty_dss1();
	else if (param->threepty.end)
		rc = split_3pty();
	else
		return;

	message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_3PTY);
	message->param.threepty.begin = param->threepty.begin;
	message->param.threepty.end = param->threepty.end;
	if (rc < 0)
		message->param.threepty.error = 1;
	else
		message->param.threepty.result = 1;
	message->param.threepty.invoke_id = param->threepty.invoke_id;
	logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
	message_put(message);
}

/* port MESSAGE_TRANSFER */
void EndpointAppPBX::port_transfer(struct port_list *portlist, int message_type, union parameter *param)
{
	logmessage(message_type, param, portlist->port_id, DIRECTION_IN);

	class Port *port;
	struct lcr_msg *message;
	int rc;

	/* bridge for real */
	if (!(port = find_port_id(portlist->port_id)))
		return;
	if ((port->p_type & PORT_CLASS_POTS_MASK) == PORT_CLASS_POTS_FXS)
		join_join_fxs();
	else if ((port->p_type & PORT_CLASS_mISDN_MASK) == PORT_CLASS_DSS1
	      || (port->p_type & PORT_CLASS_GSM_MASK) == PORT_CLASS_GSM_BS) {
		rc = join_join_dss1(param->transfer.invoke_id);

		if (rc < 0) {
			message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_TRANSFER);
			message->param.transfer.error = 1;
			message->param.transfer.invoke_id = param->transfer.invoke_id;
			logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
			message_put(message);
		}
	}
}

/* port MESSAGE_SUSPEND */
/* NOTE: before supending, the inactive-notification must be done in order to set call mixer */
void EndpointAppPBX::port_suspend(struct port_list *portlist, int message_type, union parameter *param)
{
	logmessage(message_type, param, portlist->port_id, DIRECTION_IN);

	/* epoint is now parked */
	ea_endpoint->ep_park = 1;
	memcpy(ea_endpoint->ep_park_callid, param->parkinfo.callid, sizeof(ea_endpoint->ep_park_callid));
	ea_endpoint->ep_park_len = (param->parkinfo.len>8)?8:param->parkinfo.len;

	/* remove port relation */
	ea_endpoint->free_portlist(portlist);
}

/* port MESSAGE_RESUME */
/* NOTE: before resume, the active-notification must be done in order to set call mixer */
void EndpointAppPBX::port_resume(struct port_list *portlist, int message_type, union parameter *param)
{
	logmessage(message_type, param, portlist->port_id, DIRECTION_IN);

	/* epoint is now resumed */
	ea_endpoint->ep_park = 0;

}

/* port MESSAGE_ENABLEKEYPAD */
void EndpointAppPBX::port_enablekeypad(struct port_list *portlist, int message_type, union parameter *param)
{
	struct lcr_msg *message;

	logmessage(message_type, param, portlist->port_id, DIRECTION_IN);

	message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_ENABLEKEYPAD);
	memcpy(&message->param, param, sizeof(union parameter));
	message_put(message);
}


/* port MESSAGE_DISABLE_DEJITTER */
void EndpointAppPBX::port_disable_dejitter(struct port_list *portlist, int message_type, union parameter *param)
{
	struct lcr_msg *message;

	logmessage(message_type, param, portlist->port_id, DIRECTION_IN);

	message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_DISABLE_DEJITTER);
	memcpy(&message->param, param, sizeof(union parameter));
	message_put(message);
}


/* port sends message to the endpoint
 */
void EndpointAppPBX::ea_message_port(unsigned int port_id, int message_type, union parameter *param)
{
	struct port_list *portlist;

	portlist = ea_endpoint->ep_portlist;
	while(portlist) {
		if (port_id == portlist->port_id)
			break;
		portlist = portlist->next;
	}
	if (!portlist) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) warning: port is not related to this endpoint. This may happen, if port has been released after the message was created.\n", ea_endpoint->ep_serial);
		return;
	}

//	PDEBUG(DEBUG_EPOINT, "received message %d (terminal %s, caller id %s)\n", message, e_ext.number, e_callerinfo.id);
	switch(message_type) {
		case MESSAGE_TONE_EOF: /* tone is end of file */
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) current tone is now end of file.\n", ea_endpoint->ep_serial);
		if (e_action) {
			if (e_action->index == ACTION_VBOX_PLAY) {
				vbox_message_eof();
			}
			if (e_action->index == ACTION_EFI) {
				efi_message_eof();
			}
		}
		break;

		case MESSAGE_TONE_COUNTER: /* counter info received */
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) received counter information: %d / %d seconds after start of tone.\n", ea_endpoint->ep_serial, param->counter.current, param->counter.max);
		if (e_action)
		if (e_action->index == ACTION_VBOX_PLAY) {
			e_vbox_counter = param->counter.current;
			if (param->counter.max >= 0)
				e_vbox_counter_max = param->counter.max;
		}
		break;

		/* PORT sends SETUP message */
		case MESSAGE_SETUP:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) incoming call from callerid=%s, dialing=%s\n", ea_endpoint->ep_serial, param->setup.callerinfo.id, param->setup.dialinginfo.id);
		if (e_state!=EPOINT_STATE_IDLE) {
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d) ignored because we are not in idle state.\n", ea_endpoint->ep_serial);
			break;
		}
		port_setup(portlist, message_type, param);
		break;

		/* PORT sends INFORMATION message */
		case MESSAGE_INFORMATION: /* additional digits received */
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) incoming call dialing more=%s (terminal '%s', caller id '%s')\n", ea_endpoint->ep_serial, param->information.id, e_ext.number, e_callerinfo.id);
		port_information(portlist, message_type, param);
		break;

		/* PORT sends FACILITY message */
		case MESSAGE_FACILITY:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) incoming facility (terminal '%s', caller id '%s')\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		port_facility(portlist, message_type, param);
		break;

		case MESSAGE_3PTY:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) incoming 3PTY facility (terminal '%s', caller id '%s')\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		port_3pty(portlist, message_type, param);
		break;

		case MESSAGE_TRANSFER:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) incoming TRANSFER request (terminal '%s', caller id '%s')\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		port_transfer(portlist, message_type, param);
		break;

		/* PORT sends DTMF message */
		case MESSAGE_DTMF: /* dtmf digits received */
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) dtmf digit=%c (terminal '%s', caller id '%s')\n", ea_endpoint->ep_serial, param->dtmf, e_ext.number, e_callerinfo.id);
		port_dtmf(portlist, message_type, param);
		break;

		/* PORT sends CRYPT message */
		case MESSAGE_CRYPT: /* crypt response received */
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) crypt response=%d\n", ea_endpoint->ep_serial, param->crypt.type);
		port_crypt(portlist, message_type, param);
		break;

		/* PORT sends MORE message */
		case MESSAGE_OVERLAP:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) outgoing call is accepted [overlap dialing] (terminal '%s', caller id '%s')\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		if (e_state != EPOINT_STATE_OUT_SETUP) {
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d) ignored because we are not in setup state (for port_list: another portlist might have changed the state already).\n", ea_endpoint->ep_serial);
			break;
		}
		port_overlap(portlist, message_type, param);
		break;

		/* PORT sends PROCEEDING message */
		case MESSAGE_PROCEEDING: /* port is proceeding */
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) outgoing call is proceeding (terminal '%s', caller id '%s')\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		if (e_state!=EPOINT_STATE_OUT_SETUP
		 && e_state!=EPOINT_STATE_OUT_OVERLAP) {
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d) ignored because we are not in overlap state (for port_list: another portlist might have changed the state already).\n", ea_endpoint->ep_serial);
			break;
		}
		port_proceeding(portlist, message_type, param);
		break;

		/* PORT sends ALERTING message */
		case MESSAGE_ALERTING:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) outgoing call is ringing (terminal '%s', caller id '%s')\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		if (e_state!=EPOINT_STATE_OUT_SETUP
		 && e_state!=EPOINT_STATE_OUT_OVERLAP
		 && e_state!=EPOINT_STATE_OUT_PROCEEDING) {
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d) ignored because we are not in setup or proceeding state (for port_list: another portlist might have changed the state already).\n", ea_endpoint->ep_serial);
			break;
		}
		port_alerting(portlist, message_type, param);
		break;

		/* PORT sends CONNECT message */
		case MESSAGE_CONNECT:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) outgoing call connected to %s (terminal '%s', caller id '%s')\n", ea_endpoint->ep_serial, e_connectinfo.id, e_ext.number, e_callerinfo.id);
		if (e_state!=EPOINT_STATE_OUT_SETUP
		 && e_state!=EPOINT_STATE_OUT_OVERLAP
		 && e_state!=EPOINT_STATE_OUT_PROCEEDING
		 && e_state!=EPOINT_STATE_OUT_ALERTING) {
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d) ignored because we are not in setup, proceeding or alerting state.\n", ea_endpoint->ep_serial);
			break;
		}
		port_connect(portlist, message_type, param);
		break;

		/* PORT sends DISCONNECT message */
		case MESSAGE_DISCONNECT: /* port is disconnected */
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) call disconnect with cause=%d location=%d (terminal '%s', caller id '%s')\n", ea_endpoint->ep_serial, param->disconnectinfo.cause, param->disconnectinfo.location, e_ext.number, e_callerinfo.id);
		port_disconnect_release(portlist, message_type, param);
		break;

		/* PORT sends a RELEASE message */
		case MESSAGE_RELEASE: /* port releases */
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) release with cause=%d location=%d (terminal '%s', caller id '%s')\n", ea_endpoint->ep_serial, param->disconnectinfo.cause, param->disconnectinfo.location, e_ext.number, e_callerinfo.id);
		/* portlist is release at port_disconnect_release, thanx Paul */
		port_disconnect_release(portlist, message_type, param);
		break;

		/* PORT sends a TIMEOUT message */
		case MESSAGE_TIMEOUT:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received timeout (state=%d).\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id, param->state);
		port_timeout(portlist, message_type, param);
		break; /* release */

		/* PORT sends a NOTIFY message */
		case MESSAGE_NOTIFY:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received notify.\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		port_notify(portlist, message_type, param);
		break;

		/* PORT sends a PROGRESS message */
		case MESSAGE_PROGRESS:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received progress.\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		port_progress(portlist, message_type, param);
		break;

		/* PORT sends a SUSPEND message */
		case MESSAGE_SUSPEND:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received suspend.\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		port_suspend(portlist, message_type, param);
		break; /* suspend */

		/* PORT sends a RESUME message */
		case MESSAGE_RESUME:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received resume.\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		port_resume(portlist, message_type, param);
		break;

#if 0
		kann nach dem test gel�scht werden, da eine direkte funktion im join und im mISDN zum austausch der message existiert
		/* port assigns bchannel */
		case MESSAGE_BCHANNEL: /* bchannel assignment messafe */
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received bchannel message %d from port.\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id, param->bchannel.type);
		/* only one port is expected to be connected to bchannel */
		message = message_forward(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, param);
		logmessage(message->type, &message->param, portlist->port_id, DIRECTION_IN);
		break;
#endif

		/* PORT requests DTMF */
		case MESSAGE_ENABLEKEYPAD:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') requests DTMF/KEYPAD.\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		port_enablekeypad(portlist, message_type, param);
		break;

		case MESSAGE_DISABLE_DEJITTER:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) incoming disable dejitter message (terminal '%s', caller id '%s')\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		port_disable_dejitter(portlist, message_type, param);
		break;


		default:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received a wrong message: %d\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id, message_type);
	}

	/* Note: this endpoint may be destroyed, so we MUST return */
}


/* messages from join
 */
/* join MESSAGE_CRYPT */
void EndpointAppPBX::join_crypt(struct port_list *portlist, int message_type, union parameter *param)
{
#ifdef WITH_CRYPT
	switch(param->crypt.type) {
		/* message from remote port to "crypt manager" */
		case CU_ACTK_REQ:           /* activate key-exchange */
		case CU_ACTS_REQ:            /* activate shared key */
		case CU_DACT_REQ:          /* deactivate */
		case CU_INFO_REQ:         /* request last info message */
		cryptman_message(param->crypt.type, param->crypt.data, param->crypt.len);
		break;

		/* message from "crypt manager" to user */
		case CU_ACTK_CONF:          /* key-echange done */
		case CU_ACTS_CONF:          /* shared key done */
		case CU_DACT_CONF:           /* deactivated */
		case CU_DACT_IND:           /* deactivated */
		case CU_ERROR_IND:         /* receive error message */
		case CU_INFO_IND:         /* receive info message */
		case CU_INFO_CONF:         /* receive info message */
		encrypt_result(param->crypt.type, (char *)param->crypt.data);
		break;

		default:
		PERROR("EPOINT(%d) epoint with terminal '%s' (caller id '%s') unknown crypt message: '%d'\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id, param->crypt.type);
	}
#endif
}

/* join MESSAGE_INFORMATION */
void EndpointAppPBX::join_information(struct port_list *portlist, int message_type, union parameter *param)
{
	struct lcr_msg *message;

	e_overlap = 1;

	while(portlist) {
		message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_INFORMATION);
		memcpy(&message->param.information, &param->information, sizeof(struct dialing_info));
		message_put(message);
		logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
		portlist = portlist->next;
	}
}

/* join MESSAGE_FACILITY */
void EndpointAppPBX::join_facility(struct port_list *portlist, int message_type, union parameter *param)
{
	struct lcr_msg *message;

	if (!e_ext.facility && e_ext.number[0]) {
		return;
	}

	while(portlist) {
		message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_FACILITY);
		memcpy(&message->param.facilityinfo, &param->facilityinfo, sizeof(struct facility_info));
		message_put(message);
		logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
		portlist = portlist->next;
	}
}

/* join MESSAGE_MORE */
void EndpointAppPBX::join_overlap(struct port_list *portlist, int message_type, union parameter *param)
{
	struct lcr_msg *message;

	new_state(EPOINT_STATE_IN_OVERLAP);
	
	/* own dialtone */
	if (e_join_pattern && e_ext.own_setup) {
		/* disconnect audio */
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_AUDIOPATH);
		message->param.audiopath = 0;
		message_put(message);
	}
	if (e_action) if (e_action->index == ACTION_OUTDIAL || e_action->index == ACTION_EXTERNAL) {
			if (e_dialinginfo.id[0])
				set_tone(portlist, "dialing");
			else
				set_tone(portlist, "dialtone");
			return;
	}
	if (e_dialinginfo.id[0]) {
		set_tone(portlist, "dialing");
	} else {
		if (e_ext.number[0])
			set_tone(portlist, "dialpbx");
		else
			set_tone(portlist, "dialtone");
	}
}

/* join MESSAGE_PROCEEDING */
void EndpointAppPBX::join_proceeding(struct port_list *portlist, int message_type, union parameter *param)
{
	struct lcr_msg *message;

	new_state(EPOINT_STATE_IN_PROCEEDING);

	/* own proceeding tone */
	if (e_join_pattern) {
		/* connect / disconnect audio */
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_AUDIOPATH);
		if (e_ext.own_proceeding)
			message->param.audiopath = 0;
		else
			message->param.audiopath = 1;
		message_put(message);
	}
//			UCPY(e_join_tone, "proceeding");
	if (portlist) {
		message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_PROCEEDING);
		message_put(message);
		logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
	}
	set_tone(portlist, "proceeding");
}

/* join MESSAGE_ALERTING */
void EndpointAppPBX::join_alerting(struct port_list *portlist, int message_type, union parameter *param)
{
	struct lcr_msg *message;

	new_state(EPOINT_STATE_IN_ALERTING);

	/* own alerting tone */
	if (e_join_pattern) {
		/* connect / disconnect audio */
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_AUDIOPATH);
		if (e_ext.own_alerting)
			message->param.audiopath = 0;
		else
			message->param.audiopath = 1;
		message_put(message);
	}
	if (portlist) {
		message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_ALERTING);
		message_put(message);
		logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
	}
	if (e_action) if (e_action->index == ACTION_OUTDIAL || e_action->index == ACTION_EXTERNAL) {
		set_tone(portlist, "ringing");
		return;
	}
	if (e_ext.number[0])
		set_tone(portlist, "ringpbx");
	else
		set_tone(portlist, "ringing");

	if (e_ext.number[0])
		e_dtmf = 1; /* allow dtmf */
}

/* join MESSAGE_CONNECT */
void EndpointAppPBX::join_connect(struct port_list *portlist, int message_type, union parameter *param)
{
	struct lcr_msg *message;
	time_t now;

	new_state(EPOINT_STATE_CONNECT);
//			UCPY(e_join_tone, "");
//			
	if (e_ext.number[0])
		e_dtmf = 1; /* allow dtmf */

	e_powerdial_on = 0;
	unsched_timer(&e_powerdial_timeout);
	memcpy(&e_connectinfo, &param->connectinfo, sizeof(e_callerinfo));
	if(portlist) {
		message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_CONNECT);
		memcpy(&message->param, param, sizeof(union parameter));

		/* screen clip if prefix is required */
		if (e_ext.number[0] && message->param.connectinfo.id[0] && e_ext.clip_prefix[0]) {
			SCPY(message->param.connectinfo.id, e_ext.clip_prefix);
			SCAT(message->param.connectinfo.id, numberrize_callerinfo(e_connectinfo.id,e_connectinfo.ntype, options.national, options.international));
			message->param.connectinfo.ntype = INFO_NTYPE_UNKNOWN;
		}

		/* use internal caller id */
		if (e_ext.number[0] && e_connectinfo.extension[0] && (message->param.connectinfo.present!=INFO_PRESENT_RESTRICTED || e_ext.anon_ignore)) {
			SCPY(message->param.connectinfo.id, e_connectinfo.extension);
			message->param.connectinfo.ntype = INFO_NTYPE_UNKNOWN;
		}

		/* handle restricted caller ids */
		apply_callerid_restriction(&e_ext, message->param.connectinfo.id, &message->param.connectinfo.ntype, &message->param.connectinfo.present, &message->param.connectinfo.screen, message->param.connectinfo.extension, message->param.connectinfo.name);
		/* display callerid if desired for extension */
		SCPY(message->param.connectinfo.display, apply_callerid_display(message->param.connectinfo.id, message->param.connectinfo.itype, message->param.connectinfo.ntype, message->param.connectinfo.present, message->param.connectinfo.screen, message->param.connectinfo.extension, message->param.connectinfo.name));

		/* use conp, if enabld */
//		if (!e_ext.centrex)
//			message->param.connectinfo.name[0] = '\0';

		/* send connect */
		message_put(message);
		logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
	}
	set_tone(portlist, NULL);
	e_join_pattern = 0;
	message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_AUDIOPATH);
	message->param.audiopath = 1;
	message_put(message);
	time(&now);
	e_start = now;
}

/* join MESSAGE_DISCONNECT MESSAGE_RELEASE */
void EndpointAppPBX::join_disconnect_release(int message_type, union parameter *param)
{
	char cause[16];
	struct lcr_msg *message;
	struct port_list *portlist = NULL;
	time_t now;


	/* be sure that we are active */
	notify_active();
	e_tx_state = NOTIFY_STATE_ACTIVE;

	/* we are powerdialing, if e_powerdial_on is set and limit is not exceeded if given */
	if (e_powerdial_on && ((e_powercount+1)<e_powerlimit || e_powerlimit<1)) {
		release(RELEASE_JOIN, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL,0 ); /* RELEASE_TYPE, join, port */

		/* set time for power dialing */
		schedule_timer(&e_powerdial_timeout, (int)e_powerdelay, 0); /* set redial in the future */
		e_powercount++;

		/* set redial tone */
		if (ea_endpoint->ep_portlist) {
			e_join_pattern = 0;
		}
		set_tone(ea_endpoint->ep_portlist, "redial");
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') redialing in %d seconds\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id, (int)e_powerdelay);
		/* send proceeding when powerdialing and still setup (avoid dialing timeout) */
		if (e_state==EPOINT_STATE_IN_OVERLAP) {
			new_state(EPOINT_STATE_IN_PROCEEDING);
			if (ea_endpoint->ep_portlist) {
				message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_portlist->port_id, EPOINT_TO_PORT, MESSAGE_PROCEEDING);
				message_put(message);
				logmessage(message->type, &message->param, ea_endpoint->ep_portlist->port_id, DIRECTION_OUT);
			}
/* caused the error, that the first knock sound was not there */
/*					set_tone(portlist, "proceeding"); */
		}
		/* send display of powerdialing */
		if (e_ext.display_dialing) {
			portlist = ea_endpoint->ep_portlist;
			while (portlist) {
				message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_NOTIFY);
				if (e_powerlimit)
					SPRINT(message->param.notifyinfo.display, "Retry %d of %d", e_powercount, e_powerlimit);
				else
					SPRINT(message->param.notifyinfo.display, "Retry %d", e_powercount);
				message_put(message);
				logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
				portlist = portlist->next;
			}
		}
		return;
	}

	/* set stop time */
	time(&now);
	e_stop = now;

	if ((e_state!=EPOINT_STATE_CONNECT
	  && e_state!=EPOINT_STATE_OUT_DISCONNECT
	  && e_state!=EPOINT_STATE_IN_OVERLAP
	  && e_state!=EPOINT_STATE_IN_PROCEEDING
	  && e_state!=EPOINT_STATE_IN_ALERTING)
	 || !ea_endpoint->ep_portlist) { /* or no port */
		process_hangup(param->disconnectinfo.cause, param->disconnectinfo.location);
		release(RELEASE_ALL, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, LOCATION_PRIVATE_LOCAL, param->disconnectinfo.cause, 0); /* RELEASE_TYPE, join, port */
		return; /* must exit here */
	}
	/* save cause */
	if (!e_join_cause) {
		e_join_cause = param->disconnectinfo.cause;
		e_join_location = param->disconnectinfo.location;
	}

	/* on release we need the audio again! */
	if (message_type == MESSAGE_RELEASE) {
		e_join_pattern = 0;
		ea_endpoint->ep_join_id = 0;
	}
	/* disconnect and select tone */
	new_state(EPOINT_STATE_OUT_DISCONNECT);
	SPRINT(cause, "cause_%02x", param->disconnectinfo.cause);
	/* if own_cause, we must release the join */
	if (e_ext.own_cause /* own cause */
	 || !e_join_pattern) { /* no patterns */
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) we have own cause or we have no patterns. (own_cause=%d pattern=%d)\n", ea_endpoint->ep_serial, e_ext.own_cause, e_join_pattern);
		if (message_type != MESSAGE_RELEASE)
			release(RELEASE_JOIN, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, 0); /* RELEASE_TYPE, join, port */
		e_join_pattern = 0;
	} else { /* else we enable audio */
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_AUDIOPATH);
		message->param.audiopath = 1;
		message_put(message);
	}
	/* send disconnect message */
	SCPY(e_tone, cause);
	portlist = ea_endpoint->ep_portlist;
	while(portlist) {
		set_tone(portlist, cause);
		message_disconnect_port(portlist, param->disconnectinfo.cause, param->disconnectinfo.location, "", &param->disconnectinfo.transfer);
		portlist = portlist->next;
	}
}

/* join MESSAGE_SETUP */
void EndpointAppPBX::join_setup(struct port_list *portlist, int message_type, union parameter *param)
{
	struct lcr_msg *message;
//	struct interface	*interface;

	/* if we already in setup state, we just update the dialing with new digits */
	if (e_state == EPOINT_STATE_OUT_SETUP
	 || e_state == EPOINT_STATE_OUT_OVERLAP) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) we are in setup state, so we do overlap dialing.\n", ea_endpoint->ep_serial);
		/* if digits changed, what we have already dialed */
		if (!!strncmp(e_dialinginfo.id,param->setup.dialinginfo.id,strlen(e_dialinginfo.id))) {
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d) we have dialed digits which have been changed or we have a new multidial, so we must redial.\n", ea_endpoint->ep_serial);
			/* release all ports */
			while((portlist = ea_endpoint->ep_portlist)) {
				message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_RELEASE);
				message->param.disconnectinfo.cause = CAUSE_NORMAL; /* normal clearing */
				message->param.disconnectinfo.location = LOCATION_PRIVATE_LOCAL;
				message_put(message);
				logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
				ea_endpoint->free_portlist(portlist);
			}

			/* disconnect audio */
			message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_AUDIOPATH);
			message->param.audiopath = 0;
			message_put(message);

			/* get dialing info */
			memcpy(&e_callerinfo, &param->setup.callerinfo, sizeof(e_callerinfo));
			memcpy(&e_dialinginfo, &param->setup.dialinginfo, sizeof(e_dialinginfo));
			memcpy(&e_redirinfo, &param->setup.redirinfo, sizeof(e_redirinfo));
			memcpy(&e_capainfo, &param->setup.capainfo, sizeof(e_capainfo));
			new_state(EPOINT_STATE_OUT_OVERLAP);

			/* get time */
			schedule_timer(&e_redial_timeout, 1, 0);
			return;
		}
		/* if we have a pending redial, so we just adjust the dialing number */
		if (e_redial_timeout.active) {
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d) redial in progress, so we update the dialing number to %s.\n", ea_endpoint->ep_serial, param->setup.dialinginfo.id);
			memcpy(&e_dialinginfo, &param->setup.dialinginfo, sizeof(e_dialinginfo));
			return;
		}
		if (!ea_endpoint->ep_portlist) {
			PERROR("ERROR: overlap dialing to a NULL port relation\n");
		}
		if (ea_endpoint->ep_portlist->next) {
			PERROR("ERROR: overlap dialing to a port_list port relation\n");
		}
		if (e_state == EPOINT_STATE_OUT_SETUP) {
			/* queue digits */
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d) digits '%s' are queued because we didn't receive a setup acknowledge.\n", ea_endpoint->ep_serial, param->setup.dialinginfo.id);
			SCAT(e_dialing_queue, param->setup.dialinginfo.id + strlen(e_dialinginfo.id));
			
		} else {
			/* get what we have not dialed yet */
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d) we have already dialed '%s', we received '%s', what's left '%s'.\n", ea_endpoint->ep_serial, e_dialinginfo.id, param->setup.dialinginfo.id, param->setup.dialinginfo.id+strlen(e_dialinginfo.id));
			message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_portlist->port_id, EPOINT_TO_PORT, MESSAGE_INFORMATION);
			SCPY(message->param.information.id, param->setup.dialinginfo.id + strlen(e_dialinginfo.id));
			message->param.information.ntype = INFO_NTYPE_UNKNOWN;
			message_put(message);
			logmessage(message->type, &message->param, ea_endpoint->ep_portlist->port_id, DIRECTION_OUT);
		}
		/* always store what we have dialed or queued */
		memcpy(&e_dialinginfo, &param->setup.dialinginfo, sizeof(e_dialinginfo));
		
		return;
	}
	if (e_state != EPOINT_STATE_IDLE) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) ignored because we are not in idle state.\n", ea_endpoint->ep_serial);
		return;
	}
	/* if an internal extension is dialed, copy that number */
	if (param->setup.dialinginfo.itype==INFO_ITYPE_ISDN_EXTENSION || param->setup.dialinginfo.itype==INFO_ITYPE_VBOX)
		SCPY(e_ext.number, param->setup.dialinginfo.id);
	/* if an internal extension is dialed, get extension's info about caller */
	if (e_ext.number[0]) {
		if (!read_extension(&e_ext, e_ext.number)) {
			e_ext.number[0] = '\0';
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d) the called terminal='%s' is not found in directory tree!\n", ea_endpoint->ep_serial, e_ext.number);
		}
	}

	memcpy(&e_callerinfo, &param->setup.callerinfo, sizeof(e_callerinfo));
	memcpy(&e_dialinginfo, &param->setup.dialinginfo, sizeof(e_dialinginfo));
	memcpy(&e_redirinfo, &param->setup.redirinfo, sizeof(e_redirinfo));
	memcpy(&e_capainfo, &param->setup.capainfo, sizeof(e_capainfo));
	memcpy(&e_rtpinfo, &param->setup.rtpinfo, sizeof(e_rtpinfo));

	/* process (voice over) data calls */
	if (e_ext.datacall && e_capainfo.bearer_capa!=INFO_BC_SPEECH && e_capainfo.bearer_capa!=INFO_BC_AUDIO) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) handling data call as audio call: '%s'\n", ea_endpoint->ep_serial, e_ext.number);
		memset(&e_capainfo, 0, sizeof(e_capainfo));
		e_capainfo.bearer_capa = INFO_BC_AUDIO;
		e_capainfo.bearer_mode = INFO_BMODE_CIRCUIT;
		e_capainfo.bearer_info1 = (options.law=='u')?INFO_INFO1_ULAW:INFO_INFO1_ALAW;
	}

	new_state(EPOINT_STATE_OUT_SETUP);
	/* call special setup routine */
	out_setup(0);
}

/* join MESSAGE_mISDNSIGNAL */
void EndpointAppPBX::join_mISDNsignal(struct port_list *portlist, int message_type, union parameter *param)
{
	struct lcr_msg *message;

	while(portlist) {
		message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_mISDNSIGNAL);
		memcpy(&message->param, param, sizeof(union parameter));
		message_put(message);
		portlist = portlist->next;
	}
}

/* join MESSAGE_BRIDE */
void EndpointAppPBX::join_bridge(struct port_list *portlist, int message_type, union parameter *param)
{
	struct lcr_msg *message;

	while(portlist) {
		message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_BRIDGE);
		memcpy(&message->param, param, sizeof(union parameter));
		message_put(message);
		portlist = portlist->next;
	}
}

/* join MESSAGE_NOTIFY */
void EndpointAppPBX::join_notify(struct port_list *portlist, int message_type, union parameter *param)
{
	struct lcr_msg *message;
	int new_state;

	if (param->notifyinfo.notify) {
		new_state = track_notify(e_tx_state, param->notifyinfo.notify);
//		/* if notification was generated locally, we turn hold music on/off */ 
//		if (param->notifyinfo.local)
// NOTE: we always assume that we send hold music on suspension of call, because we don't track if audio is available or not (we assume that we always have no audio, to make it easier)
		{
			if (e_hold) {
				/* unhold if */
				if (new_state!=NOTIFY_STATE_HOLD && new_state!=NOTIFY_STATE_SUSPEND) {
					if (!strcmp(e_tone, "hold")) { // don't interrupt other tones
						while(portlist) {
							set_tone(portlist, "");
							portlist = portlist->next;
						}
					}
					portlist = ea_endpoint->ep_portlist;
					e_hold = 0;
				}
			} else {
				/* hold if */
				if (new_state==NOTIFY_STATE_HOLD || new_state==NOTIFY_STATE_SUSPEND) {
					while(portlist) {
						set_tone(portlist, "hold");
						portlist = portlist->next;
					}
					portlist = ea_endpoint->ep_portlist;
					e_hold = 1;
				}
			}
		}
		/* save new state */
		e_tx_state = new_state;
	}

	/* notify port(s) about it */
	while(portlist) {
		message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_NOTIFY);
		memcpy(&message->param.notifyinfo, &param->notifyinfo, sizeof(struct notify_info));
		/* handle restricted caller ids */
		apply_callerid_restriction(&e_ext, message->param.notifyinfo.id, &message->param.notifyinfo.ntype, &message->param.notifyinfo.present, 0, message->param.notifyinfo.extension, NULL);
		/* display callerid if desired for extension */
		SCPY(message->param.notifyinfo.display, apply_callerid_display(message->param.notifyinfo.id, message->param.notifyinfo.itype, message->param.notifyinfo.ntype, message->param.notifyinfo.present, 0, message->param.notifyinfo.extension, NULL));
		message_put(message);
		logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
		portlist = portlist->next;
	}
}

/* join MESSAGE_DTMF */
void EndpointAppPBX::join_dtmf(struct port_list *portlist, int message_type, union parameter *param)
{
	struct lcr_msg *message;

	while(portlist) {
		message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_DTMF);
		memcpy(&message->param, param, sizeof(union parameter));
		message_put(message);
		portlist = portlist->next;
	}
}

/* join MESSAGE_DISABLE_DEJITTER */
void EndpointAppPBX::join_disable_dejitter(struct port_list *portlist, int message_type, union parameter *param)
{
	struct lcr_msg *message;

	while(portlist) {
		message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_DISABLE_DEJITTER);
		memcpy(&message->param, param, sizeof(union parameter));
		message_put(message);
		portlist = portlist->next;
	}
}

/* JOIN sends messages to the endpoint
 */
void EndpointAppPBX::ea_message_join(unsigned int join_id, int message_type, union parameter *param)
{
	struct port_list *portlist;
	struct lcr_msg *message;

	if (!join_id) {
		PERROR("EPOINT(%d) error: JOIN == NULL.\n", ea_endpoint->ep_serial);
		return;
	}

	portlist = ea_endpoint->ep_portlist;

//	PDEBUG(DEBUG_EPOINT, "EPOINT(%d) received message %d for active JOIN (terminal %s, caller id %s state=%d)\n", ea_endpoint->ep_serial, message, e_ext.number, e_callerinfo.id, e_state);
	switch(message_type) {
		/* JOIN SENDS TONE message */
		case MESSAGE_TONE:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received tone message: '%d'\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id, param->tone.name);
		set_tone(portlist, param->tone.name);
		break;

		/* JOIN SENDS CRYPT message */
		case MESSAGE_CRYPT:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received crypt message: '%d'\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id, param->crypt.type);
		join_crypt(portlist, message_type, param);
		break;

		/* JOIN sends INFORMATION message */
		case MESSAGE_INFORMATION:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received more digits: '%s'\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id, param->information.id);
		join_information(portlist, message_type, param);
		break;

		/* JOIN sends FACILITY message */
		case MESSAGE_FACILITY:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received facility\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		join_facility(portlist, message_type, param);
		break;

		/* JOIN sends OVERLAP message */
		case MESSAGE_OVERLAP:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received 'more info required'\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		if (e_state!=EPOINT_STATE_IN_SETUP
		 && e_state!=EPOINT_STATE_IN_OVERLAP) {
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d) ignored because we are not in setup state.\n", ea_endpoint->ep_serial);
			break;
		}
		join_overlap(portlist, message_type, param);
		break;

		/* JOIN sends PROCEEDING message */
		case MESSAGE_PROCEEDING:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s (caller id '%s') received proceeding\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		if(e_state!=EPOINT_STATE_IN_OVERLAP) {
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d) ignored because we are not in setup state.\n", ea_endpoint->ep_serial);
			break;
		}
		join_proceeding(portlist, message_type, param);
		break;

		/* JOIN sends ALERTING message */
		case MESSAGE_ALERTING:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received alerting\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		if (e_state!=EPOINT_STATE_IN_OVERLAP
		 && e_state!=EPOINT_STATE_IN_PROCEEDING
		 && e_state!=EPOINT_STATE_IN_ALERTING /* second alerting */
		 && e_state!=EPOINT_STATE_CONNECT) { /* alerting after transfer */
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d) ignored because we are not in setup or proceeding state.\n", ea_endpoint->ep_serial);
			break;
		}
		join_alerting(portlist, message_type, param);
		break;

		/* JOIN sends CONNECT message */
		case MESSAGE_CONNECT:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received connect\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		if (e_state!=EPOINT_STATE_IN_OVERLAP
		 && e_state!=EPOINT_STATE_IN_PROCEEDING
		 && e_state!=EPOINT_STATE_IN_ALERTING) {
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d) ignored because we are not in setup, proceeding or alerting state.\n", ea_endpoint->ep_serial);
			break;
		}
		join_connect(portlist, message_type, param);
		break;

		/* JOIN sends DISCONNECT/RELEASE message */
		case MESSAGE_DISCONNECT: /* JOIN disconnect */
		case MESSAGE_RELEASE: /* JOIN releases */
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received %s with cause %d location %d\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id, (message_type==MESSAGE_DISCONNECT)?"disconnect":"release", param->disconnectinfo.cause, param->disconnectinfo.location);
		join_disconnect_release(message_type, param);
		break;

		/* JOIN sends SETUP message */
		case MESSAGE_SETUP:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint received setup from terminal='%s',id='%s' to id='%s' (dialing itype=%d)\n", ea_endpoint->ep_serial, param->setup.callerinfo.extension, param->setup.callerinfo.id, param->setup.dialinginfo.id, param->setup.dialinginfo.itype);
		join_setup(portlist, message_type, param);
		break;

		/* JOIN sends special mISDNSIGNAL message */
		case MESSAGE_mISDNSIGNAL: /* isdn message to port */
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received mISDNsignal message.\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		join_mISDNsignal(portlist, message_type, param);
		break;

		/* JOIN sends bridge message */
		case MESSAGE_BRIDGE: /* bride message to port */
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received bridge message.\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		join_bridge(portlist, message_type, param);
		break;

		/* JOIN has pattern available */
		case MESSAGE_PATTERN: /* indicating pattern available */
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received pattern availability.\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		if (!e_join_pattern) {
			PDEBUG(DEBUG_EPOINT, "-> pattern becomes available\n");
			e_join_pattern = 1;
			SCPY(e_tone, "");
			while(portlist) {
				set_tone(portlist, NULL);
				portlist = portlist->next;
			}
			/* connect our audio tx and rx (blueboxing should be possibe before connect :)*/
			message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_AUDIOPATH);
			message->param.audiopath = 1;
			message_put(message);
		}
		break;

		/* JOIN has no pattern available */
		case MESSAGE_NOPATTERN: /* indicating no pattern available */
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received pattern NOT available.\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		if (e_join_pattern) {
			PDEBUG(DEBUG_EPOINT, "-> pattern becomes unavailable\n");
			e_join_pattern = 0;
			/* disconnect our audio tx and rx */
			message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_AUDIOPATH);
			message->param.audiopath = 0;
			message_put(message);
		}
		break;

#if 0
		/* JOIN (dunno at the moment) */
		case MESSAGE_REMOTE_AUDIO:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received audio remote request.\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_AUDIOPATH);
		message->param.audiopath = param->channel;
		message_put(message);
		break;
#endif

		/* JOIN sends a notify message */
		case MESSAGE_NOTIFY:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received notify.\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		join_notify(portlist, message_type, param);
		break;

		/* JOIN wants keypad / dtmf */
		case MESSAGE_ENABLEKEYPAD:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received keypad enable request.\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		e_enablekeypad = 1;
		e_dtmf = 1;
		trace_header("ENABLE KEYPAD", DIRECTION_NONE);
		end_trace();
		break;

		/* JOIN sends a DTMF message */
		case MESSAGE_DTMF:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received dtmf.\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		join_dtmf(portlist, message_type, param);
		break;

		/* JOIN sends a DISABLE_DEJITTER message */
		case MESSAGE_DISABLE_DEJITTER:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received disable dejitter.\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id);
		join_disable_dejitter(portlist, message_type, param);
		break;

		default:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) epoint with terminal '%s' (caller id '%s') received a wrong message: #%d\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id, message_type);
	}
}


/* pick_join will connect the first incoming call found. the endpoint
 * will receivce a MESSAGE_CONNECT.
 */
int match_list(char *list, char *item)
{
	char *end, *next = NULL;

	/* no list make matching */
	if (!list)
		return(1);

	while(42) {
		/* eliminate white spaces */
		while (*list > '\0' && *list <= ' ')
			list++;
		if (*list == ',') {
			list++;
			continue;
		}
		/* if end of list is reached, we return */
		if (list[0] == '\0')
			return(0);
		/* if we have more than one entry (left) */
		if ((end = strchr(list, ',')))
			next = end + 1;
		else
			next = end = strchr(list, '\0');
		while (*(end-1) <= ' ')
			end--;
		/* if string part matches item */
		if (!strncmp(list, item, end-list))
			return(1);
		list = next;
	}
}

void EndpointAppPBX::pick_join(char *extensions)
{
	struct lcr_msg *message;
	struct port_list *portlist;
	class Port *port;
	class EndpointAppPBX *eapp, *found;
	class Join *join;
	class JoinPBX *joinpbx;
	struct join_relation *relation;
	int vbox;

	/* find an endpoint that is ringing internally or vbox with higher priority */
	vbox = 0;
	found = NULL;
	eapp = apppbx_first;
	while(eapp) {
		if (eapp!=this && ea_endpoint->ep_portlist) {
			portlist = eapp->ea_endpoint->ep_portlist;
			while(portlist) {
				if ((port = find_port_id(portlist->port_id))) {
					if (port->p_type == PORT_TYPE_VBOX_OUT) {
						if (match_list(extensions, eapp->e_ext.number)) {
							found = eapp;
							vbox = 1;
							break;
						}
					}
					if ((portlist->port_type & PORT_CLASS_DIR_MASK) == PORT_CLASS_DIR_OUT
					 && port->p_state==PORT_STATE_OUT_ALERTING)
						if (match_list(extensions, eapp->e_ext.number)) {
							found = eapp;
						}
				}
				portlist = portlist->next;
			}
			if (portlist)
				break;
		}
		eapp = eapp->next;
	}

	/* if no endpoint found */
	if (!found) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) nobody is ringing internally (or we don't have her in the access list), so we disconnect.\n", ea_endpoint->ep_serial);
reject:
		set_tone(ea_endpoint->ep_portlist, "cause_10");
		message_disconnect_port(ea_endpoint->ep_portlist, CAUSE_NORMAL, LOCATION_PRIVATE_LOCAL, "", NULL);
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		return;
	}
	eapp = found;

	if (ea_endpoint->ep_join_id) {
		PERROR("EPOINT(%d) we already have a join. SOFTWARE ERROR.\n", ea_endpoint->ep_serial);
		goto reject;
	}
	if (!eapp->ea_endpoint->ep_join_id) {
		PERROR("EPOINT(%d) ringing endpoint has no join.\n", ea_endpoint->ep_serial);
		goto reject;
	}
	join = find_join_id(eapp->ea_endpoint->ep_join_id);
	if (!join) {
		PERROR("EPOINT(%d) ringing endpoint's join not found.\n", ea_endpoint->ep_serial);
		goto reject;
	}
	if (join->j_type != JOIN_TYPE_PBX) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) ringing endpoint's join is not a PBX join, so we must reject.\n", ea_endpoint->ep_serial);
		goto reject;
	}
	joinpbx = (class JoinPBX *)join;
	relation = joinpbx->j_relation;
	if (!relation) {
		PERROR("EPOINT(%d) ringing endpoint's join has no relation. SOFTWARE ERROR.\n", ea_endpoint->ep_serial);
		goto reject;
	}
	while (relation->epoint_id != eapp->ea_endpoint->ep_serial) {
		relation = relation->next;
		if (!relation) {
			PERROR("EPOINT(%d) ringing endpoint's join has no relation to that join. SOFTWARE ERROR.\n", ea_endpoint->ep_serial);
			goto reject;
		}
	}

	PDEBUG(DEBUG_EPOINT, "EPOINT(%d) found ringing endpoint: %d.\n", ea_endpoint->ep_serial, eapp->ea_endpoint->ep_serial);

	if (options.deb & DEBUG_EPOINT) {
		class Join *debug_c = join_first;
		class Endpoint *debug_e = epoint_first;
		class Port *debug_p = port_first;

		joinpbx_debug(joinpbx, "EndpointAppPBX::pick_join(before)");

		PDEBUG(DEBUG_EPOINT, "showing all joins:\n");
		while(debug_c) {
			PDEBUG(DEBUG_EPOINT, "join=%ld\n", debug_c->j_serial);
			debug_c = debug_c->next;
		}
		PDEBUG(DEBUG_EPOINT, "showing all endpoints:\n");
		while(debug_e) {
			PDEBUG(DEBUG_EPOINT, "ep=%ld, join=%ld\n", debug_e->ep_serial, debug_e->ep_join_id);
			debug_e = debug_e->next;
		}
		PDEBUG(DEBUG_EPOINT, "showing all ports:\n");
		while(debug_p) {
			PDEBUG(DEBUG_EPOINT, "port=%ld, ep=%ld (active)\n", debug_p->p_serial, ACTIVE_EPOINT(debug_p->p_epointlist));
			debug_p = debug_p->next;
		}
	}

	/* relink join */
	ea_endpoint->ep_join_id = eapp->ea_endpoint->ep_join_id; /* we get the join */
	relation->epoint_id = ea_endpoint->ep_serial; /* the join gets us */
	eapp->ea_endpoint->ep_join_id = 0; /* the ringing endpoint will get disconnected */

	/* connnecting our endpoint */
	new_state(EPOINT_STATE_CONNECT);
	if (e_ext.number[0])
		e_dtmf = 1;
	set_tone(ea_endpoint->ep_portlist, NULL);

	/* now we send a release to the ringing endpoint */
	message = message_create(ea_endpoint->ep_join_id, eapp->ea_endpoint->ep_serial, JOIN_TO_EPOINT, MESSAGE_RELEASE);
	message->param.disconnectinfo.cause = CAUSE_NONSELECTED; /* non selected user clearing */
	message->param.disconnectinfo.location = LOCATION_PRIVATE_LOCAL;
	message_put(message);

	/* we send a connect to the join with our caller id */
	message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_CONNECT);
	SCPY(message->param.connectinfo.id, e_callerinfo.id);
	message->param.connectinfo.present = e_callerinfo.present;
	message->param.connectinfo.screen = e_callerinfo.screen;
	message->param.connectinfo.itype = e_callerinfo.itype;
	message->param.connectinfo.ntype = e_callerinfo.ntype;
	message_put(message);

	/* we send a connect to our port with the remote callerid */
	message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_portlist->port_id, EPOINT_TO_PORT, MESSAGE_CONNECT);
	SCPY(message->param.connectinfo.id, eapp->e_callerinfo.id);
	message->param.connectinfo.present = eapp->e_callerinfo.present;
	message->param.connectinfo.screen = eapp->e_callerinfo.screen;
	message->param.connectinfo.itype = eapp->e_callerinfo.itype;
	message->param.connectinfo.ntype = eapp->e_callerinfo.ntype;
	/* handle restricted caller ids */
	apply_callerid_restriction(&e_ext, message->param.connectinfo.id, &message->param.connectinfo.ntype, &message->param.connectinfo.present, &message->param.connectinfo.screen, message->param.connectinfo.extension, message->param.connectinfo.name);
	/* display callerid if desired for extension */
	SCPY(message->param.connectinfo.display, apply_callerid_display(message->param.connectinfo.id, message->param.connectinfo.itype,  message->param.connectinfo.ntype, message->param.connectinfo.present, message->param.connectinfo.screen, message->param.connectinfo.extension, message->param.connectinfo.name));
	message_put(message);

	/* we send a connect to the audio path (not for vbox) */
	message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_AUDIOPATH);
	message->param.audiopath = 1;
	message_put(message);

	/* beeing paranoid, we make call update */
	trigger_work(&joinpbx->j_updatebridge);

	if (options.deb & DEBUG_EPOINT) {
		class Join *debug_c = join_first;
		class Endpoint *debug_e = epoint_first;
		class Port *debug_p = port_first;

		joinpbx_debug(joinpbx, "EndpointAppPBX::pick_join(after)");

		PDEBUG(DEBUG_EPOINT, "showing all joins:\n");
		while(debug_c) {
			PDEBUG(DEBUG_EPOINT, "join=%ld\n", debug_c->j_serial);
			debug_c = debug_c->next;
		}
		PDEBUG(DEBUG_EPOINT, "showing all endpoints:\n");
		while(debug_e) {
			PDEBUG(DEBUG_EPOINT, "ep=%ld, join=%ld\n", debug_e->ep_serial, debug_e->ep_join_id);
			debug_e = debug_e->next;
		}
		PDEBUG(DEBUG_EPOINT, "showing all ports:\n");
		while(debug_p) {
			PDEBUG(DEBUG_EPOINT, "port=%ld, ep=%ld\n", debug_p->p_serial, ACTIVE_EPOINT(debug_p->p_epointlist));
			debug_p = debug_p->next;
		}
	}
}


/* join calls (look for a join that is on hold (same isdn/gsm interface/terminal))
 */
int EndpointAppPBX::join_join_dss1(int invoke_id)
{
	struct lcr_msg *message;
	struct join_relation *add_relation, *remove_relation;
	struct join_relation **add_relation_pointer, **remove_relation_pointer;
	class Join *our_join, *other_join, *add_join, *remove_join;
	class JoinPBX *our_joinpbx, *other_joinpbx, *add_joinpbx, *remove_joinpbx;
	class EndpointAppPBX *other_eapp, *remove_eapp_hold, *remove_eapp_active;
	class Port *our_port, *other_port;
#ifdef WITH_MISDN
	class Pdss1 *our_pdss1 = NULL, *other_pdss1;
#endif
#ifdef WITH_GSM_BS
	class Pgsm_bs *our_gsm_bs = NULL, *other_gsm_bs;
#endif
	class Endpoint *temp_epoint;

	/* are we a candidate to join a join? */
	our_join = find_join_id(ea_endpoint->ep_join_id);
	if (!our_join) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: our join doesn't exist anymore.\n", ea_endpoint->ep_serial);
		return -1;
	}
	if (our_join->j_type != JOIN_TYPE_PBX) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: join is not a pbx join.\n", ea_endpoint->ep_serial);
		return -1;
	}
	our_joinpbx = (class JoinPBX *)our_join;
	if (!ea_endpoint->ep_portlist) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: we have no port.\n", ea_endpoint->ep_serial);
		return -1;
	}
	our_port = find_port_id(ea_endpoint->ep_portlist->port_id);
	if (!our_port) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: our port doesn't exist anymore.\n", ea_endpoint->ep_serial);
		return -1;
	}
#ifdef WITH_MISDN
	if ((our_port->p_type & PORT_CLASS_mISDN_MASK) == PORT_CLASS_DSS1)
		our_pdss1 = (class Pdss1 *)our_port;
#endif
#ifdef WITH_GSM_BS
	if ((our_port->p_type & PORT_CLASS_GSM_MASK) == PORT_CLASS_GSM_BS)
		our_gsm_bs = (class Pgsm_bs *)our_port;
#endif

	/* find an endpoint that has the same mISDNport/ces that we are on */
	other_eapp = apppbx_first;
	while(other_eapp) {
		if (other_eapp == this) {
			other_eapp = other_eapp->next;
			continue;
		}
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) comparing other endpoint candiate: (ep%d) terminal='%s' port=%s join=%d.\n", ea_endpoint->ep_serial, other_eapp->ea_endpoint->ep_serial, other_eapp->e_ext.number, (other_eapp->ea_endpoint->ep_portlist)?"YES":"NO", other_eapp->ea_endpoint->ep_join_id);
		if (other_eapp->ea_endpoint->ep_portlist /* has port */
		 && other_eapp->ea_endpoint->ep_join_id) { /* has join */
			other_port = find_port_id(other_eapp->ea_endpoint->ep_portlist->port_id);
			if (other_port) { /* port still exists */
#ifdef WITH_MISDN
				if (our_pdss1
				 && (other_port->p_type==PORT_TYPE_DSS1_NT_OUT
				  || other_port->p_type==PORT_TYPE_DSS1_NT_IN)) { /* port is isdn nt-mode */
					other_pdss1 = (class Pdss1 *)other_port;
					PDEBUG(DEBUG_EPOINT, "EPOINT(%d) comparing other endpoint's port is of type isdn! comparing our portnum=%d with other's portnum=%d hold=%s ces=%d\n", ea_endpoint->ep_serial, our_pdss1->p_m_mISDNport->portnum, other_pdss1->p_m_mISDNport->portnum, (other_pdss1->p_hold)?"YES":"NO", other_pdss1->p_m_d_ces);
					if (1 //other_pdss1->p_hold /* port is on hold */
					 && other_pdss1->p_m_mISDNport == our_pdss1->p_m_mISDNport /* same isdn interface */
					 && other_pdss1->p_m_d_ces == our_pdss1->p_m_d_ces) /* same tei+sapi */
						break;
				} else
#endif
#ifdef WITH_GSM_BS
				if (our_gsm_bs
				 && (other_port->p_type==PORT_TYPE_GSM_BS_OUT
				  || other_port->p_type==PORT_TYPE_GSM_BS_IN)) { /* port is GSM bs-mode */
					other_gsm_bs = (class Pgsm_bs *)other_port;
					PDEBUG(DEBUG_EPOINT, "EPOINT(%d) comparing other endpoint's port is of type GSM! comparing our imsi with other imsi=%s\n", ea_endpoint->ep_serial, our_gsm_bs->p_g_imsi, other_gsm_bs->p_g_imsi);
					if (!strcmp(other_gsm_bs->p_g_imsi, our_gsm_bs->p_g_imsi)) /* same tei+sapi */
						break;
				} else
#endif
				{
					PDEBUG(DEBUG_EPOINT, "EPOINT(%d) comparing other endpoint's port is of other type!\n", ea_endpoint->ep_serial);
				}
			} else {
				PDEBUG(DEBUG_EPOINT, "EPOINT(%d) comparing other endpoint's port doesn't exist enymore.\n", ea_endpoint->ep_serial);
			}
		}
		other_eapp = other_eapp->next;
	}
	if (!other_eapp) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: no other endpoint on same isdn terminal.\n", ea_endpoint->ep_serial);
		return -1;
	}
	PDEBUG(DEBUG_EPOINT, "EPOINT(%d) port with same terminal found.\n", ea_endpoint->ep_serial);

	/* if we have the same join */
	if (other_eapp->ea_endpoint->ep_join_id == ea_endpoint->ep_join_id) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: we and the other have the same join.\n", ea_endpoint->ep_serial);
		return -1;
	}
	other_join = find_join_id(other_eapp->ea_endpoint->ep_join_id);
	if (!other_join) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: other join doesn't exist anymore.\n", ea_endpoint->ep_serial);
		return -1;
	}
	if (other_join->j_type != JOIN_TYPE_PBX) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: other join is not a pbx join.\n", ea_endpoint->ep_serial);
		return -1;
	}
	other_joinpbx = (class JoinPBX *)other_join;
	if (our_joinpbx->j_partyline && other_joinpbx->j_partyline) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: both joins are partylines.\n", ea_endpoint->ep_serial);
		return -1;
	}

	/* now find out which is ACTIVE-IDLE and which is ACTIVE-HELD */
	if (our_port->p_hold && !other_port->p_hold) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) our relation is on hold and other is active, so we move our relations to other relations\n", ea_endpoint->ep_serial);
		remove_eapp_hold = this;
		remove_eapp_active = other_eapp;
		remove_join = our_join;
		remove_joinpbx = our_joinpbx;
		add_join = other_join;
		add_joinpbx = other_joinpbx;
	} else {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) our relation is active or other is on hold, so we move ohter relations to our relations\n", ea_endpoint->ep_serial);
		remove_eapp_hold = other_eapp;
		remove_eapp_active = this;
		remove_join = other_join;
		remove_joinpbx = other_joinpbx;
		add_join = our_join;
		add_joinpbx = our_joinpbx;
	}

	/* remove relation to endpoint for join on hold */
	remove_relation = remove_joinpbx->j_relation;
	remove_relation_pointer = &remove_joinpbx->j_relation;
	while(remove_relation) {
		if (remove_relation->epoint_id == remove_eapp_hold->ea_endpoint->ep_serial) {
			/* detach endpoint on hold */
			*remove_relation_pointer = remove_relation->next;
			FREE(remove_relation, sizeof(struct join_relation));
			cmemuse--;
			remove_relation = *remove_relation_pointer;
			remove_eapp_hold->ea_endpoint->ep_join_id = 0;
			continue;
		}

		/* change join/hold pointer of endpoint to the new join */
		temp_epoint = find_epoint_id(remove_relation->epoint_id);
		if (temp_epoint) {
			if (temp_epoint->ep_join_id == remove_join->j_serial)
				temp_epoint->ep_join_id = add_join->j_serial;
		}

		remove_relation_pointer = &remove_relation->next;
		remove_relation = remove_relation->next;
	}
	PDEBUG(DEBUG_EPOINT, "EPOINT(%d) endpoint (hold) removed, other enpoints on join relinked.\n", ea_endpoint->ep_serial);

	if (invoke_id >= 0) {
		/* remove relation to endpoint for active join */
		remove_relation = add_joinpbx->j_relation;
		remove_relation_pointer = &add_joinpbx->j_relation;
		while(remove_relation) {
			if (remove_relation->epoint_id == remove_eapp_active->ea_endpoint->ep_serial) {
				/* detach active endpoint */
				*remove_relation_pointer = remove_relation->next;
				FREE(remove_relation, sizeof(struct join_relation));
				cmemuse--;
				remove_relation = *remove_relation_pointer;
				remove_eapp_active->ea_endpoint->ep_join_id = 0;
				continue;
			}

			remove_relation_pointer = &remove_relation->next;
			remove_relation = remove_relation->next;
		}
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) endpoint (active) removed, other enpoints on join relinked.\n", ea_endpoint->ep_serial);

		/* if active endpoint is in alerting state, send alerting message to join */
		if (remove_eapp_active->e_state == EPOINT_STATE_IN_ALERTING) {
			if (add_joinpbx->j_relation && !add_joinpbx->j_relation->next) {
				/* if channel state indicateds "audio" (1), we tell the other endpoint that patterns are available */
				if (add_joinpbx->j_relation->channel_state) {
					message = message_create(add_joinpbx->j_relation->epoint_id, add_join->j_serial, EPOINT_TO_JOIN, MESSAGE_PATTERN);
					message_put(message);
				}
				message = message_create(add_joinpbx->j_relation->epoint_id, add_join->j_serial, EPOINT_TO_JOIN, MESSAGE_ALERTING);
				message_put(message);
			}
		}
	}

	/* join call relations: we add the members of the join on hold to the active join */
	add_relation = add_joinpbx->j_relation;
	add_relation_pointer = &add_joinpbx->j_relation;
	while(add_relation) {
		add_relation_pointer = &add_relation->next;
		add_relation = add_relation->next;
	}
	*add_relation_pointer = remove_joinpbx->j_relation;
	remove_joinpbx->j_relation = NULL;
	PDEBUG(DEBUG_EPOINT, "EPOINT(%d) relations joined.\n", ea_endpoint->ep_serial);

	/* release endpoint on hold */
	message = message_create(remove_joinpbx->j_serial, remove_eapp_hold->ea_endpoint->ep_serial, JOIN_TO_EPOINT, MESSAGE_RELEASE);
	message->param.disconnectinfo.cause = CAUSE_NORMAL; /* normal */
	message->param.disconnectinfo.location = LOCATION_PRIVATE_LOCAL;
	if (invoke_id >= 0) {
		/* send the result with disconnect message to the invoking endpoint */
		if (remove_eapp_hold->ea_endpoint->ep_serial == ea_endpoint->ep_serial) {
			message->param.disconnectinfo.transfer.result = 1;
			message->param.disconnectinfo.transfer.invoke_id = invoke_id;
		}
	}
	message_put(message);
	
	if (invoke_id >= 0) {
		/* release active endpoint */
		message = message_create(add_joinpbx->j_serial, remove_eapp_active->ea_endpoint->ep_serial, JOIN_TO_EPOINT, MESSAGE_RELEASE);
		message->param.disconnectinfo.cause = CAUSE_NORMAL; /* normal */
		message->param.disconnectinfo.location = LOCATION_PRIVATE_LOCAL;
		/* send the result with disconnect message to the invoking endpoint */
		if (remove_eapp_active->ea_endpoint->ep_serial == ea_endpoint->ep_serial) {
			message->param.disconnectinfo.transfer.result = 1;
			message->param.disconnectinfo.transfer.invoke_id = invoke_id;
		}
		message_put(message);
	}

	/* if we are not a partyline, we get partyline state from other join */
	add_joinpbx->j_partyline += remove_joinpbx->j_partyline; 

	/* remove empty join */
	delete remove_join;
	PDEBUG(DEBUG_EPOINT, "EPOINT(%d)join completely removed!\n", ea_endpoint->ep_serial);

	/* mixer must update */
	trigger_work(&add_joinpbx->j_updatebridge);

	/* we send a retrieve to that endpoint */
	// mixer will update the hold-state of the join and send it to the endpoints is changes

	return 0;
}

/* join calls (look for a join that is on hold (same fxs interface/terminal))
 */
int EndpointAppPBX::join_join_fxs(void)
{
#ifdef WITH_MISDN
	struct lcr_msg *message;
	struct join_relation *add_relation, *remove_relation;
	struct join_relation **add_relation_pointer, **remove_relation_pointer;
	class Join *our_join, *other_join, *add_join, *remove_join;
	class JoinPBX *our_joinpbx, *other_joinpbx, *add_joinpbx, *remove_joinpbx;
	class EndpointAppPBX *other_eapp, *remove_eapp;
	class Port *our_port, *other_port;
	class Pfxs *our_fxs, *other_fxs;
	class Endpoint *temp_epoint;

	/* are we a candidate to join a join? */
	our_join = find_join_id(ea_endpoint->ep_join_id);
	if (!our_join) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: our join doesn't exist anymore.\n", ea_endpoint->ep_serial);
		return -1;
	}
	if (our_join->j_type != JOIN_TYPE_PBX) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: join is not a pbx join.\n", ea_endpoint->ep_serial);
		return -1;
	}
	our_joinpbx = (class JoinPBX *)our_join;
	if (!ea_endpoint->ep_portlist) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: we have no port.\n", ea_endpoint->ep_serial);
		return -1;
	}
	if (!e_ext.number[0]) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: we are not internal extension.\n", ea_endpoint->ep_serial);
		return -1;
	}
	our_port = find_port_id(ea_endpoint->ep_portlist->port_id);
	if (!our_port) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: our port doesn't exist anymore.\n", ea_endpoint->ep_serial);
		return -1;
	}
	if ((our_port->p_type & PORT_CLASS_POTS_MASK) != PORT_CLASS_POTS_FXS) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: our port is not fxs.\n", ea_endpoint->ep_serial);
		return -1;
	}
	our_fxs = (class Pfxs *)our_port;

	/* find an endpoint that has the same mISDNport that we are on */
	other_eapp = apppbx_first;
	while(other_eapp) {
		if (other_eapp == this) {
			other_eapp = other_eapp->next;
			continue;
		}
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) comparing other endpoint candiate: (ep%d) terminal='%s' port=%s join=%d.\n", ea_endpoint->ep_serial, other_eapp->ea_endpoint->ep_serial, other_eapp->e_ext.number, (other_eapp->ea_endpoint->ep_portlist)?"YES":"NO", other_eapp->ea_endpoint->ep_join_id);
		if (other_eapp->e_ext.number[0] /* has terminal */
		 && other_eapp->ea_endpoint->ep_portlist /* has port */
		 && other_eapp->ea_endpoint->ep_join_id) { /* has join */
			other_port = find_port_id(other_eapp->ea_endpoint->ep_portlist->port_id);
			if (other_port) { /* port still exists */
				if (other_port->p_type==PORT_TYPE_POTS_FXS_OUT
				 || other_port->p_type==PORT_TYPE_POTS_FXS_IN) { /* port is FXS */
					other_fxs = (class Pfxs *)other_port;
					PDEBUG(DEBUG_EPOINT, "EPOINT(%d) comparing other endpoint's port is of type isdn! comparing our portnum=%d with other's portnum=%d hold=%s state=%d\n", ea_endpoint->ep_serial, our_fxs->p_m_mISDNport->portnum, other_fxs->p_m_mISDNport->portnum, (other_fxs->p_hold)?"YES":"NO", other_fxs->p_state);
					if (1 //other_fxs->p_hold /* port is on hold */
					 && other_fxs->p_m_mISDNport == our_fxs->p_m_mISDNport) /* same isdn interface */
						break;
				} else {
					PDEBUG(DEBUG_EPOINT, "EPOINT(%d) comparing other endpoint's port is of other type!\n", ea_endpoint->ep_serial);
				}
			} else {
				PDEBUG(DEBUG_EPOINT, "EPOINT(%d) comparing other endpoint's port doesn't exist enymore.\n", ea_endpoint->ep_serial);
			}
		}
		other_eapp = other_eapp->next;
	}
	if (!other_eapp) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: no other endpoint on same FXS terminal.\n", ea_endpoint->ep_serial);
		return -1;
	}
	PDEBUG(DEBUG_EPOINT, "EPOINT(%d) port with same terminal found.\n", ea_endpoint->ep_serial);

	/* if we have the same join */
	if (other_eapp->ea_endpoint->ep_join_id == ea_endpoint->ep_join_id) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: we and the other have the same join.\n", ea_endpoint->ep_serial);
		return -1;
	}
	other_join = find_join_id(other_eapp->ea_endpoint->ep_join_id);
	if (!other_join) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: other join doesn't exist anymore.\n", ea_endpoint->ep_serial);
		return -1;
	}
	if (other_join->j_type != JOIN_TYPE_PBX) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: other join is not a pbx join.\n", ea_endpoint->ep_serial);
		return -1;
	}
	other_joinpbx = (class JoinPBX *)other_join;
	if (our_joinpbx->j_partyline && other_joinpbx->j_partyline) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: both joins are partylines.\n", ea_endpoint->ep_serial);
		return -1;
	}

	/* now find out which is ACTIVE-IDLE and which is ACTIVE-HELD */
	if (our_port->p_hold && !other_port->p_hold) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) our relation is on hold and other is active, so we move our relations to other relations\n", ea_endpoint->ep_serial);
		remove_eapp = this;
		remove_join = our_join;
		remove_joinpbx = our_joinpbx;
		add_join = other_join;
		add_joinpbx = other_joinpbx;
	} else {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) our relation is active or other is on hold, so we move ohter relations to our relations\n", ea_endpoint->ep_serial);
		remove_eapp = other_eapp;
		remove_join = other_join;
		remove_joinpbx = other_joinpbx;
		add_join = our_join;
		add_joinpbx = our_joinpbx;
	}

	/* remove relation to endpoint for join on hold */
	remove_relation = remove_joinpbx->j_relation;
	remove_relation_pointer = &remove_joinpbx->j_relation;
	while(remove_relation) {
		if (remove_relation->epoint_id == remove_eapp->ea_endpoint->ep_serial) {
			/* detach other endpoint */
			*remove_relation_pointer = remove_relation->next;
			FREE(remove_relation, sizeof(struct join_relation));
			cmemuse--;
			remove_relation = *remove_relation_pointer;
			remove_eapp->ea_endpoint->ep_join_id = 0;
			continue;
		}

		/* change join/hold pointer of endpoint to the new join */
		temp_epoint = find_epoint_id(remove_relation->epoint_id);
		if (temp_epoint) {
			if (temp_epoint->ep_join_id == remove_join->j_serial)
				temp_epoint->ep_join_id = add_join->j_serial;
		}

		remove_relation_pointer = &remove_relation->next;
		remove_relation = remove_relation->next;
	}
	PDEBUG(DEBUG_EPOINT, "EPOINT(%d) endpoint removed, other enpoints on join relinked.\n", ea_endpoint->ep_serial);

	/* join call relations */
	add_relation = add_joinpbx->j_relation;
	add_relation_pointer = &add_joinpbx->j_relation;
	while(add_relation) {
		add_relation_pointer = &add_relation->next;
		add_relation = add_relation->next;
	}
	*add_relation_pointer = remove_joinpbx->j_relation;
	remove_joinpbx->j_relation = NULL;
	PDEBUG(DEBUG_EPOINT, "EPOINT(%d) relations joined.\n", ea_endpoint->ep_serial);

	/* release endpoint */
	message = message_create(remove_joinpbx->j_serial, remove_eapp->ea_endpoint->ep_serial, JOIN_TO_EPOINT, MESSAGE_RELEASE);
	message->param.disconnectinfo.cause = CAUSE_NORMAL; /* normal */
	message->param.disconnectinfo.location = LOCATION_PRIVATE_LOCAL;
	message_put(message);
	
	/* if we are not a partyline, we get partyline state from other join */
	add_joinpbx->j_partyline += remove_joinpbx->j_partyline; 

	/* remove empty join */
	delete remove_join;
	PDEBUG(DEBUG_EPOINT, "EPOINT(%d)join completely removed!\n", ea_endpoint->ep_serial);

	/* mixer must update */
	trigger_work(&add_joinpbx->j_updatebridge);

	/* we send a retrieve to that endpoint */
	// mixer will update the hold-state of the join and send it to the endpoints is changes

	return 0;
#else
	PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: no mISDN support anyway.\n", ea_endpoint->ep_serial);

	return -1;
#endif
}

/* do audio bridge of endpoints on same isdn/gsm terminal */
int EndpointAppPBX::join_3pty_dss1(void)
{
	class Join *our_join, *other_join;
	class JoinPBX *our_joinpbx, *other_joinpbx;
	class EndpointAppPBX *other_eapp;
	class Port *our_port, *other_port;
#ifdef WITH_MISDN
	class Pdss1 *our_pdss1 = NULL, *other_pdss1;
#endif
#ifdef WITH_GSM_BS
	class Pgsm_bs *our_gsm_bs = NULL, *other_gsm_bs;
#endif

	/* are we a candidate to join a join? */
	our_join = find_join_id(ea_endpoint->ep_join_id);
	if (!our_join) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: our join doesn't exist anymore.\n", ea_endpoint->ep_serial);
		return -1;
	}
	if (our_join->j_type != JOIN_TYPE_PBX) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: join is not a pbx join.\n", ea_endpoint->ep_serial);
		return -1;
	}
	our_joinpbx = (class JoinPBX *)our_join;
	if (!ea_endpoint->ep_portlist) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: we have no port.\n", ea_endpoint->ep_serial);
		return -1;
	}
	our_port = find_port_id(ea_endpoint->ep_portlist->port_id);
	if (!our_port) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: our port doesn't exist anymore.\n", ea_endpoint->ep_serial);
		return -1;
	}
#ifdef WITH_MISDN
	if ((our_port->p_type & PORT_CLASS_mISDN_MASK) == PORT_CLASS_DSS1)
		our_pdss1 = (class Pdss1 *)our_port;
#endif
#ifdef WITH_GSM_BS
	if ((our_port->p_type & PORT_CLASS_GSM_MASK) == PORT_CLASS_GSM_BS)
		our_gsm_bs = (class Pgsm_bs *)our_port;
#endif

	/* find an endpoint that has the same mISDNport/ces that we are on */
	other_eapp = apppbx_first;
	while(other_eapp) {
		if (other_eapp == this) {
			other_eapp = other_eapp->next;
			continue;
		}
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) comparing other endpoint candiate: (ep%d) terminal='%s' port=%s join=%d.\n", ea_endpoint->ep_serial, other_eapp->ea_endpoint->ep_serial, other_eapp->e_ext.number, (other_eapp->ea_endpoint->ep_portlist)?"YES":"NO", other_eapp->ea_endpoint->ep_join_id);
		if (other_eapp->ea_endpoint->ep_portlist /* has port */
		 && other_eapp->ea_endpoint->ep_join_id) { /* has join */
			other_port = find_port_id(other_eapp->ea_endpoint->ep_portlist->port_id);
			if (other_port) { /* port still exists */
#ifdef WITH_MISDN
				if (our_pdss1
				 && (other_port->p_type==PORT_TYPE_DSS1_NT_OUT
				  || other_port->p_type==PORT_TYPE_DSS1_NT_IN)) { /* port is isdn nt-mode */
					other_pdss1 = (class Pdss1 *)other_port;
					PDEBUG(DEBUG_EPOINT, "EPOINT(%d) comparing other endpoint's port is of type isdn! comparing our portnum=%d with other's portnum=%d hold=%s ces=%d\n", ea_endpoint->ep_serial, our_pdss1->p_m_mISDNport->portnum, other_pdss1->p_m_mISDNport->portnum, (other_pdss1->p_hold)?"YES":"NO", other_pdss1->p_m_d_ces);
					if (1 //other_pdss1->p_hold /* port is on hold */
					 && other_pdss1->p_m_mISDNport == our_pdss1->p_m_mISDNport /* same isdn interface */
					 && other_pdss1->p_m_d_ces == our_pdss1->p_m_d_ces) /* same tei+sapi */
						break;
				} else
#endif
#ifdef WITH_GSM_BS
				if (our_gsm_bs
				 && (other_port->p_type==PORT_TYPE_GSM_BS_OUT
				  || other_port->p_type==PORT_TYPE_GSM_BS_IN)) { /* port is GSM bs-mode */
					other_gsm_bs = (class Pgsm_bs *)other_port;
					PDEBUG(DEBUG_EPOINT, "EPOINT(%d) comparing other endpoint's port is of type GSM! comparing our imsi with other imsi=%s\n", ea_endpoint->ep_serial, our_gsm_bs->p_g_imsi, other_gsm_bs->p_g_imsi);
					if (!strcmp(other_gsm_bs->p_g_imsi, our_gsm_bs->p_g_imsi)) /* same tei+sapi */
						break;
				} else
#endif
				{
					PDEBUG(DEBUG_EPOINT, "EPOINT(%d) comparing other endpoint's port is of other type!\n", ea_endpoint->ep_serial);
				}
			} else {
				PDEBUG(DEBUG_EPOINT, "EPOINT(%d) comparing other endpoint's port doesn't exist enymore.\n", ea_endpoint->ep_serial);
			}
		}
		other_eapp = other_eapp->next;
	}
	if (!other_eapp) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: no other endpoint on same terminal.\n", ea_endpoint->ep_serial);
		return -1;
	}
	PDEBUG(DEBUG_EPOINT, "EPOINT(%d) port with same terminal found.\n", ea_endpoint->ep_serial);

	/* if we have the same join */
	if (other_eapp->ea_endpoint->ep_join_id == ea_endpoint->ep_join_id) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: we and the other have the same join.\n", ea_endpoint->ep_serial);
		return -1;
	}
	other_join = find_join_id(other_eapp->ea_endpoint->ep_join_id);
	if (!other_join) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: other join doesn't exist anymore.\n", ea_endpoint->ep_serial);
		return -1;
	}
	if (other_join->j_type != JOIN_TYPE_PBX) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: other join is not a pbx join.\n", ea_endpoint->ep_serial);
		return -1;
	}
	other_joinpbx = (class JoinPBX *)other_join;
	if (our_joinpbx->j_partyline && other_joinpbx->j_partyline) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: both joins are partylines.\n", ea_endpoint->ep_serial);
		return -1;
	}

	if (our_joinpbx->j_3pty) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: our join already doing 3PTY.\n", ea_endpoint->ep_serial);
		return -1;
	}
	if (other_joinpbx->j_3pty) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: other join already doing 3PTY.\n", ea_endpoint->ep_serial);
		return -1;
	}

	/* set 3PTY bridge */
	other_joinpbx->j_3pty = our_joinpbx->j_serial;
	our_joinpbx->j_3pty = other_joinpbx->j_serial;

	/* mixer must update */
	trigger_work(&our_joinpbx->j_updatebridge);
	trigger_work(&other_joinpbx->j_updatebridge);

	/* we send a retrieve to that endpoint */
	// mixer will update the hold-state of the join and send it to the endpoints is changes

	return 0;
}

/* do audio bridge of endpoints on same fxs terminal */
int EndpointAppPBX::join_3pty_fxs(void)
{
#ifdef WITH_MISDN
	class Join *our_join, *other_join;
	class JoinPBX *our_joinpbx, *other_joinpbx;
	class EndpointAppPBX *other_eapp;
	class Port *our_port, *other_port;
	class Pfxs *our_fxs, *other_fxs;

	/* are we a candidate to join a join? */
	our_join = find_join_id(ea_endpoint->ep_join_id);
	if (!our_join) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: our join doesn't exist anymore.\n", ea_endpoint->ep_serial);
		return -1;
	}
	if (our_join->j_type != JOIN_TYPE_PBX) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: join is not a pbx join.\n", ea_endpoint->ep_serial);
		return -1;
	}
	our_joinpbx = (class JoinPBX *)our_join;
	if (!ea_endpoint->ep_portlist) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: we have no port.\n", ea_endpoint->ep_serial);
		return -1;
	}
	if (!e_ext.number[0]) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: we are not internal extension.\n", ea_endpoint->ep_serial);
		return -1;
	}
	our_port = find_port_id(ea_endpoint->ep_portlist->port_id);
	if (!our_port) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: our port doesn't exist anymore.\n", ea_endpoint->ep_serial);
		return -1;
	}
	if ((our_port->p_type & PORT_CLASS_POTS_MASK) != PORT_CLASS_POTS_FXS) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: our port is not FXS pots.\n", ea_endpoint->ep_serial);
		return -1;
	}
	our_fxs = (class Pfxs *)our_port;

	/* find an endpoint that has the same mISDNport that we are on */
	other_eapp = apppbx_first;
	while(other_eapp) {
		if (other_eapp == this) {
			other_eapp = other_eapp->next;
			continue;
		}
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) comparing other endpoint candiate: (ep%d) terminal='%s' port=%s join=%d.\n", ea_endpoint->ep_serial, other_eapp->ea_endpoint->ep_serial, other_eapp->e_ext.number, (other_eapp->ea_endpoint->ep_portlist)?"YES":"NO", other_eapp->ea_endpoint->ep_join_id);
		if (other_eapp->e_ext.number[0] /* has terminal */
		 && other_eapp->ea_endpoint->ep_portlist /* has port */
		 && other_eapp->ea_endpoint->ep_join_id) { /* has join */
			other_port = find_port_id(other_eapp->ea_endpoint->ep_portlist->port_id);
			if (other_port) { /* port still exists */
				if (other_port->p_type==PORT_TYPE_POTS_FXS_OUT
				 || other_port->p_type==PORT_TYPE_POTS_FXS_IN) { /* port is isdn nt-mode */
					other_fxs = (class Pfxs *)other_port;
					PDEBUG(DEBUG_EPOINT, "EPOINT(%d) comparing other endpoint's port is of type FXS! comparing our portnum=%d with other's portnum=%d hold=%s state=%d\n", ea_endpoint->ep_serial, our_fxs->p_m_mISDNport->portnum, other_fxs->p_m_mISDNport->portnum, (other_fxs->p_hold)?"YES":"NO", other_fxs->p_state);
					if (1 //other_fxs->p_hold /* port is on hold */
					 && other_fxs->p_m_mISDNport == our_fxs->p_m_mISDNport) /* same pots interface */
						break;
				} else {
					PDEBUG(DEBUG_EPOINT, "EPOINT(%d) comparing other endpoint's port is of other type!\n", ea_endpoint->ep_serial);
				}
			} else {
				PDEBUG(DEBUG_EPOINT, "EPOINT(%d) comparing other endpoint's port doesn't exist enymore.\n", ea_endpoint->ep_serial);
			}
		}
		other_eapp = other_eapp->next;
	}
	if (!other_eapp) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: no other endpoint on same FXS terminal.\n", ea_endpoint->ep_serial);
		return -1;
	}
	PDEBUG(DEBUG_EPOINT, "EPOINT(%d) port with same terminal found.\n", ea_endpoint->ep_serial);

	/* if we have the same join */
	if (other_eapp->ea_endpoint->ep_join_id == ea_endpoint->ep_join_id) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: we and the other have the same join.\n", ea_endpoint->ep_serial);
		return -1;
	}
	other_join = find_join_id(other_eapp->ea_endpoint->ep_join_id);
	if (!other_join) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: other join doesn't exist anymore.\n", ea_endpoint->ep_serial);
		return -1;
	}
	if (other_join->j_type != JOIN_TYPE_PBX) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: other join is not a pbx join.\n", ea_endpoint->ep_serial);
		return -1;
	}
	other_joinpbx = (class JoinPBX *)other_join;
	if (our_joinpbx->j_partyline && other_joinpbx->j_partyline) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: both joins are partylines.\n", ea_endpoint->ep_serial);
		return -1;
	}

	if (our_joinpbx->j_3pty) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: our join already doing 3PTY.\n", ea_endpoint->ep_serial);
		return -1;
	}
	if (other_joinpbx->j_3pty) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: other join already doing 3PTY.\n", ea_endpoint->ep_serial);
		return -1;
	}

	/* set 3PTY bridge */
	other_joinpbx->j_3pty = our_joinpbx->j_serial;
	our_joinpbx->j_3pty = other_joinpbx->j_serial;

	/* mixer must update */
	trigger_work(&our_joinpbx->j_updatebridge);
	trigger_work(&other_joinpbx->j_updatebridge);

	/* we send a retrieve to that endpoint */
	// mixer will update the hold-state of the join and send it to the endpoints is changes

	return 0;
#else
	PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot join: no mISDN support anyway.\n", ea_endpoint->ep_serial);

	return -1;
#endif
}

/* split audio bridge */
int EndpointAppPBX::split_3pty(void)
{
	class Join *our_join, *other_join;
	class JoinPBX *our_joinpbx, *other_joinpbx;

	/* are we a candidate to join a join? */
	our_join = find_join_id(ea_endpoint->ep_join_id);
	if (!our_join) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot split: our join doesn't exist anymore.\n", ea_endpoint->ep_serial);
		return -1;
	}
	if (our_join->j_type != JOIN_TYPE_PBX) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot split: join is not a pbx join.\n", ea_endpoint->ep_serial);
		return -1;
	}
	our_joinpbx = (class JoinPBX *)our_join;

	if (!our_joinpbx->j_3pty) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot split: we don't have a 3PTY.\n", ea_endpoint->ep_serial);
		return -1;
	}

	other_join = find_join_id(our_joinpbx->j_3pty);
	if (!other_join) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot split: other join doesn't exist anymore.\n", ea_endpoint->ep_serial);
		return -1;
	}
	if (other_join->j_type != JOIN_TYPE_PBX) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) cannot split: join is not a pbx join.\n", ea_endpoint->ep_serial);
		return -1;
	}
	other_joinpbx = (class JoinPBX *)other_join;

	our_joinpbx->j_3pty = 0;
	other_joinpbx->j_3pty = 0;

	/* mixer must update */
	trigger_work(&our_joinpbx->j_updatebridge);
	trigger_work(&other_joinpbx->j_updatebridge);

	/* we send a retrieve to that endpoint */
	// mixer will update the hold-state of the join and send it to the endpoints is changes

	return 0;
}

/* check if we have an external call
 * this is used to check for encryption ability
 */
int EndpointAppPBX::check_external(const char **errstr, class Port **port)
{
	struct join_relation *relation;
	class Join *join;
	class JoinPBX *joinpbx;
	class Endpoint *epoint;

	/* some paranoia check */
	if (!ea_endpoint->ep_portlist) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) error: we have no port.\n", ea_endpoint->ep_serial);
		*errstr = "No Call";
		return(1);
	}
	if (!e_ext.number[0]) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) error: we are not internal extension.\n", ea_endpoint->ep_serial);
		*errstr = "No Call";
		return(1);
	}

	/* check if we have a join with 2 parties */
	join = find_join_id(ea_endpoint->ep_join_id);
	if (!join) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) we have currently no join.\n", ea_endpoint->ep_serial);
		*errstr = "No Call";
		return(1);
	}
	if (join->j_type != JOIN_TYPE_PBX) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) join is not a pbx join.\n", ea_endpoint->ep_serial);
		*errstr = "No PBX Call";
		return(1);
	}
	joinpbx = (class JoinPBX *)join;
	relation = joinpbx->j_relation;
	if (!relation) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) join has no relation.\n", ea_endpoint->ep_serial);
		*errstr = "No Call";
		return(1);
	}
	if (!relation->next) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) join has no 2nd relation.\n", ea_endpoint->ep_serial);
		*errstr = "No Call";
		return(1);
	}
	if (relation->next->next) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) join has more than two relations.\n", ea_endpoint->ep_serial);
		*errstr = "Err: Conference";
		return(1);
	}
	if (relation->epoint_id == ea_endpoint->ep_serial) {
		relation = relation->next;
		if (relation->epoint_id == ea_endpoint->ep_serial) {
			PERROR("EPOINT(%d) SOFTWARE ERROR: both join relations are related to our endpoint.\n", ea_endpoint->ep_serial);
			*errstr = "Software Error";
			return(1);
		}
	}

	/* check remote port for external call */
	epoint = find_epoint_id(relation->epoint_id);
	if (!epoint) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) join has no 2nd endpoint.\n", ea_endpoint->ep_serial);
		*errstr = "No Call";
		return(1);
	}
	if (!epoint->ep_portlist) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) 2nd endpoint has not port.\n", ea_endpoint->ep_serial);
		*errstr = "No Call";
		return(1);
	}
	*port = find_port_id(epoint->ep_portlist->port_id);
	if (!(*port)) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) 2nd endpoint has an none existing port.\n", ea_endpoint->ep_serial);
		*errstr = "No Call";
		return(1);
	}
	if (((*port)->p_type & PORT_CLASS_mISDN_MASK) != PORT_CLASS_DSS1) { /* port is not external isdn */
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) 2nd endpoint has not an external port.\n", ea_endpoint->ep_serial);
		*errstr = "No Ext Call";
		return(1);
	}
	if ((*port)->p_state != PORT_STATE_CONNECT) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) 2nd endpoint's port is not in connected state.\n", ea_endpoint->ep_serial);
		*errstr = "No Ext Connect";
		return(1);
	}
	return(0);
}

void EndpointAppPBX::logmessage(int message_type, union parameter *param, unsigned int port_id, int dir)
{
	const char *logtext = "unknown";
	char buffer[64];

	switch(message_type) {
		case MESSAGE_SETUP:
		trace_header("SETUP", dir);
		if (dir == DIRECTION_OUT)
			add_trace("to", NULL, "CH(%lu)", port_id);
		if (dir == DIRECTION_IN)
			add_trace("from", NULL, "CH(%lu)", port_id);
		if (param->setup.callerinfo.extension[0])
			add_trace("extension", NULL, "%s", param->setup.callerinfo.extension);
		if (param->setup.callerinfo.interface[0])
			add_trace("interface", "from", "%s", param->setup.callerinfo.interface);
		if (param->setup.dialinginfo.interfaces[0])
			add_trace("interface", "to", "%s", param->setup.dialinginfo.interfaces);
		add_trace("caller id", "number", "%s", numberrize_callerinfo(param->setup.callerinfo.id, param->setup.callerinfo.ntype, options.national, options.international));
		switch(param->setup.callerinfo.present) {
		      	case INFO_PRESENT_RESTRICTED:
			add_trace("caller id", "present", "restricted");
			break;
		      	case INFO_PRESENT_ALLOWED:
			add_trace("caller id", "present", "allowed");
			break;
		      	default:
			add_trace("caller id", "present", "not available");
		}
		if (param->setup.callerinfo.ntype2) {
			add_trace("caller id2", "number", "%s", numberrize_callerinfo(param->setup.callerinfo.id2, param->setup.callerinfo.ntype2, options.national, options.international));
			switch(param->setup.callerinfo.present) {
			      	case INFO_PRESENT_RESTRICTED:
				add_trace("caller id2", "present", "restricted");
				break;
			      	case INFO_PRESENT_ALLOWED:
				add_trace("caller id2", "present", "allowed");
				break;
			      	default:
				add_trace("caller id2", "present", "not available");
			}
		}
		if (param->setup.redirinfo.id[0]) {
			add_trace("redir'ing", "number", "%s", numberrize_callerinfo(param->setup.redirinfo.id, param->setup.redirinfo.ntype, options.national, options.international));
			switch(param->setup.redirinfo.present) {
				case INFO_PRESENT_RESTRICTED:
				add_trace("redir'ing", "present", "restricted");
				break;
				case INFO_PRESENT_ALLOWED:
				add_trace("redir'ing", "present", "allowed");
				break;
				default:
				add_trace("redir'ing", "present", "not available");
			}
		}
		if (param->setup.dialinginfo.id[0])
			add_trace("dialing", NULL, "%s", param->setup.dialinginfo.id);
		if (param->setup.dialinginfo.keypad[0])
			add_trace("keypad", NULL, "%s", param->setup.dialinginfo.keypad);
		if (param->setup.dialinginfo.display[0])
			add_trace("display", NULL, "%s", param->setup.dialinginfo.display);
		if (param->setup.dialinginfo.sending_complete)
			add_trace("complete", NULL, "true", param->setup.dialinginfo.sending_complete);
		end_trace();
		break;

		case MESSAGE_OVERLAP:
		trace_header("SETUP ACKNOWLEDGE", dir);
		if (dir == DIRECTION_OUT)
			add_trace("to", NULL, "CH(%lu)", port_id);
		if (dir == DIRECTION_IN)
			add_trace("from", NULL, "CH(%lu)", port_id);
		end_trace();
		break;

		case MESSAGE_PROCEEDING:
		trace_header("PROCEEDING", dir);
		if (dir == DIRECTION_OUT)
			add_trace("to", NULL, "CH(%lu)", port_id);
		if (dir == DIRECTION_IN)
			add_trace("from", NULL, "CH(%lu)", port_id);
		end_trace();
		break;

		case MESSAGE_ALERTING:
		trace_header("ALERTING", dir);
		if (dir == DIRECTION_OUT)
			add_trace("to", NULL, "CH(%lu)", port_id);
		if (dir == DIRECTION_IN)
			add_trace("from", NULL, "CH(%lu)", port_id);
		end_trace();
		break;

		case MESSAGE_CONNECT:
		trace_header("CONNECT", dir);
		if (dir == DIRECTION_OUT)
			add_trace("to", NULL, "CH(%lu)", port_id);
		if (dir == DIRECTION_IN)
			add_trace("from", NULL, "CH(%lu)", port_id);
		if (param->connectinfo.extension[0])
			add_trace("extension", NULL, "%s", param->connectinfo.extension);
		add_trace("connect id", "number", "%s", numberrize_callerinfo(param->connectinfo.id, param->connectinfo.ntype, options.national, options.international));
		switch(param->connectinfo.present) {
		      	case INFO_PRESENT_RESTRICTED:
			add_trace("connect id", "present", "restricted");
			break;
		      	case INFO_PRESENT_ALLOWED:
			add_trace("connect id", "present", "allowed");
			break;
		      	default:
			add_trace("connect id", "present", "not available");
		}
		if (param->connectinfo.display[0])
			add_trace("display", NULL, "%s", param->connectinfo.display);
		end_trace();
		break;

		case MESSAGE_DISCONNECT:
		case MESSAGE_RELEASE:
		if (message_type == MESSAGE_DISCONNECT)
			trace_header("DISCONNECT", dir);
		else
			trace_header("RELEASE", dir);
		if (dir == DIRECTION_OUT)
			add_trace("to", NULL, "CH(%lu)", port_id);
		if (dir == DIRECTION_IN)
			add_trace("from", NULL, "CH(%lu)", port_id);
		add_trace("cause", "value", "%d", param->disconnectinfo.cause);
		switch(param->disconnectinfo.location) {
			case LOCATION_USER:
			add_trace("cause", "location", "0-User");
			break;
			case LOCATION_PRIVATE_LOCAL:
			add_trace("cause", "location", "1-Local-PBX");
			break;
			case LOCATION_PUBLIC_LOCAL:
			add_trace("cause", "location", "2-Local-Exchange");
			break;
			case LOCATION_TRANSIT:
			add_trace("cause", "location", "3-Transit");
			break;
			case LOCATION_PUBLIC_REMOTE:
			add_trace("cause", "location", "4-Remote-Exchange");
			break;
			case LOCATION_PRIVATE_REMOTE:
			add_trace("cause", "location", "5-Remote-PBX");
			break;
			case LOCATION_INTERNATIONAL:
			add_trace("cause", "location", "7-International-Exchange");
			break;
			case LOCATION_BEYOND:
			add_trace("cause", "location", "10-Beyond-Interworking");
			break;
			default:
			add_trace("cause", "location", "%d", param->disconnectinfo.location);
		}
		if (param->disconnectinfo.display[0])
			add_trace("display", NULL, "%s", param->disconnectinfo.display);
		end_trace();
		break;

		case MESSAGE_NOTIFY:
		switch(param->notifyinfo.notify) {
			case 0x00:
			logtext = "NULL";
			break;
			case 0x80:
			logtext = "USER_SUSPENDED";
			break;
			case 0x82:
			logtext = "BEARER_SERVICE_CHANGED";
			break;
			case 0x81:
			logtext = "USER_RESUMED";
			break;
			case 0xc2:
			logtext = "CONFERENCE_ESTABLISHED";
			break;
			case 0xc3:
			logtext = "CONFERENCE_DISCONNECTED";
			break;
			case 0xc4:
			logtext = "OTHER_PARTY_ADDED";
			break;
			case 0xc5:
			logtext = "ISOLATED";
			break;
			case 0xc6:
			logtext = "REATTACHED";
			break;
			case 0xc7:
			logtext = "OTHER_PARTY_ISOLATED";
			break;
			case 0xc8:
			logtext = "OTHER_PARTY_REATTACHED";
			break;
			case 0xc9:
			logtext = "OTHER_PARTY_SPLIT";
			break;
			case 0xca:
			logtext = "OTHER_PARTY_DISCONNECTED";
			break;
			case 0xcb:
			logtext = "CONFERENCE_FLOATING";
			break;
			case 0xcc:
			logtext = "CONFERENCE_DISCONNECTED_PREEMTED";
			break;
			case 0xcf:
			logtext = "CONFERENCE_FLOATING_SERVED_USER_PREEMTED";
			break;
			case 0xe0:
			logtext = "CALL_IS_A_WAITING_CALL";
			break;
			case 0xe8:
			logtext = "DIVERSION_ACTIVATED";
			break;
			case 0xe9:
			logtext = "RESERVED_CT_1";
			break;
			case 0xea:
			logtext = "RESERVED_CT_2";
			break;
			case 0xee:
			logtext = "REVERSE_CHARGING";
			break;
			case 0xf9:
			logtext = "REMOTE_HOLD";
			break;
			case 0xfa:
			logtext = "REMOTE_RETRIEVAL";
			break;
			case 0xfb:
			logtext = "CALL_IS_DIVERTING";
			break;
			default:
			SPRINT(buffer, "%d", param->notifyinfo.notify - 0x80);
			logtext = buffer;

		}
		trace_header("NOTIFY", dir);
		if (dir == DIRECTION_OUT)
			add_trace("to", NULL, "CH(%lu)", port_id);
		if (dir == DIRECTION_IN)
			add_trace("from", NULL, "CH(%lu)", port_id);
		if (param->notifyinfo.notify)
			add_trace("indicator", NULL, "%s", logtext);
		if (param->notifyinfo.id[0]) {
			add_trace("redir'on", "number", "%s", numberrize_callerinfo(param->notifyinfo.id, param->notifyinfo.ntype, options.national, options.international));
			switch(param->notifyinfo.present) {
				case INFO_PRESENT_RESTRICTED:
				add_trace("redir'on", "present", "restricted");
				break;
				case INFO_PRESENT_ALLOWED:
				add_trace("redir'on", "present", "allowed");
				break;
				default:
				add_trace("redir'on", "present", "not available");
			}
		}
		if (param->notifyinfo.display[0])
			add_trace("display", NULL, "%s", param->notifyinfo.display);
		end_trace();
		break;

		case MESSAGE_PROGRESS:
		switch(param->progressinfo.progress) {
			case 0x01:
			logtext = "Call is not end to end ISDN";
			break;
			case 0x02:
			logtext = "Destination address is non-ISDN";
			break;
			case 0x03:
			logtext = "Origination address is non-ISDN";
			break;
			case 0x04:
			logtext = "Call has returned to the ISDN";
			break;
			case 0x08:
			logtext = "In-band info or pattern available";
			break;
			default:
			SPRINT(buffer, "%d", param->progressinfo.progress);
			logtext = buffer;

		}
		trace_header("PROGRESS", dir);
		if (dir == DIRECTION_OUT)
			add_trace("to", NULL, "CH(%lu)", port_id);
		if (dir == DIRECTION_IN)
			add_trace("from", NULL, "CH(%lu)", port_id);
		add_trace("indicator", NULL, "%s", logtext);
		switch(param->progressinfo.location) {
			case LOCATION_USER:
			add_trace("cause", "location", "0-User");
			break;
			case LOCATION_PRIVATE_LOCAL:
			add_trace("cause", "location", "1-Local-PBX");
			break;
			case LOCATION_PUBLIC_LOCAL:
			add_trace("cause", "location", "2-Local-Exchange");
			break;
			case LOCATION_TRANSIT:
			add_trace("cause", "location", "3-Transit");
			break;
			case LOCATION_PUBLIC_REMOTE:
			add_trace("cause", "location", "4-Remote-Exchange");
			break;
			case LOCATION_PRIVATE_REMOTE:
			add_trace("cause", "location", "5-Remote-PBX");
			break;
			case LOCATION_INTERNATIONAL:
			add_trace("cause", "location", "7-International-Exchange");
			break;
			case LOCATION_BEYOND:
			add_trace("cause", "location", "10-Beyond-Interworking");
			break;
			default:
			add_trace("cause", "location", "%d", param->progressinfo.location);
		}
		end_trace();
		break;

		case MESSAGE_INFORMATION:
		trace_header("INFORMATION", dir);
		if (dir == DIRECTION_OUT)
			add_trace("to", NULL, "CH(%lu)", port_id);
		if (dir == DIRECTION_IN)
			add_trace("from", NULL, "CH(%lu)", port_id);
		if (param->information.id[0])
			add_trace("dialing", NULL, "%s", param->information.id);
		if (param->information.display[0])
			add_trace("display", NULL, "%s", param->information.display);
		if (param->information.sending_complete)
			add_trace("complete", NULL, "true", param->information.sending_complete);
		end_trace();
		break;

		case MESSAGE_FACILITY:
		trace_header("FACILITY", dir);
		if (dir == DIRECTION_OUT)
			add_trace("to", NULL, "CH(%lu)", port_id);
		if (dir == DIRECTION_IN)
			add_trace("from", NULL, "CH(%lu)", port_id);
		end_trace();
		break;

		case MESSAGE_TONE:
		trace_header("TONE", dir);
		if (dir == DIRECTION_OUT)
			add_trace("to", NULL, "CH(%lu)", port_id);
		if (dir == DIRECTION_IN)
			add_trace("from", NULL, "CH(%lu)", port_id);
		if (param->tone.name[0]) {
			add_trace("directory", NULL, "%s", param->tone.dir[0]?param->tone.dir:"default");
			add_trace("name", NULL, "%s", param->tone.name);
		} else
			add_trace("off", NULL, NULL);
		end_trace();
		break;

		case MESSAGE_SUSPEND:
		case MESSAGE_RESUME:
		if (message_type == MESSAGE_SUSPEND)
			trace_header("SUSPEND", dir);
		else
			trace_header("RESUME", dir);
		if (dir == DIRECTION_OUT)
			add_trace("to", NULL, "CH(%lu)", port_id);
		if (dir == DIRECTION_IN)
			add_trace("from", NULL, "CH(%lu)", port_id);
		if (param->parkinfo.len)
			add_trace("length", NULL, "%d", param->parkinfo.len);
		end_trace();
		break;

#if 0
		case MESSAGE_BCHANNEL:
		trace_header("BCHANNEL", dir);
		switch(param->bchannel.type) {
			case BCHANNEL_REQUEST:
			add_trace("type", NULL, "request");
			break;
			case BCHANNEL_ASSIGN:
			add_trace("type", NULL, "assign");
			break;
			case BCHANNEL_ASSIGN_ACK:
			add_trace("type", NULL, "assign_ack");
			break;
			case BCHANNEL_REMOVE:
			add_trace("type", NULL, "remove");
			break;
			case BCHANNEL_REMOVE_ACK:
			add_trace("type", NULL, "remove_ack");
			break;
		}
		if (param->bchannel.addr)
			add_trace("address", NULL, "%x", param->bchannel.addr);
		end_trace();
		break;
#endif

		case MESSAGE_3PTY:
		if (param->threepty.begin)
			trace_header("Begin3PTY", dir);
		if (param->threepty.end)
			trace_header("End3PTY", dir);
		if (param->threepty.invoke)
			add_trace("action", NULL, "invoke");
		if (param->threepty.result)
			add_trace("action", NULL, "result");
		if (param->threepty.error)
			add_trace("action", NULL, "error");
		add_trace("invoke-id", NULL, "%d", param->threepty.invoke_id);
		end_trace();
		break;

		case MESSAGE_TRANSFER:
		trace_header("TRANSFER", dir);
		if (param->transfer.invoke)
			add_trace("action", NULL, "invoke");
		if (param->transfer.result)
			add_trace("action", NULL, "result");
		if (param->transfer.error)
			add_trace("action", NULL, "error");
		add_trace("invoke-id", NULL, "%d", param->transfer.invoke_id);
		end_trace();
		break;

		case MESSAGE_DISABLE_DEJITTER:
		trace_header("DISBALE_DEJITTER", dir);
		if (param->queue)
			add_trace("queue", NULL, "%d", param->queue);
		end_trace();
		break;

		default:
		PERROR("EPOINT(%d) message not of correct type (%d)\n", ea_endpoint->ep_serial, message_type);
	}
}

void EndpointAppPBX::message_disconnect_port(struct port_list *portlist, int cause, int location, const char *display, const struct param_transfer *transfer)
{
	struct lcr_msg *message;

	if (!portlist)
		return;
	if (!portlist->port_id)
		return;

	if (!e_connectedmode) {
		message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_DISCONNECT);
		message->param.disconnectinfo.cause = cause;
		message->param.disconnectinfo.location = location;
		if (display[0])
			SCPY(message->param.disconnectinfo.display, display);
		else
			SCPY(message->param.disconnectinfo.display, get_isdn_cause(cause, location, e_ext.display_cause));
	} else {
		message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_NOTIFY);
		if (display[0])
			SCPY(message->param.notifyinfo.display, display);
		else
			SCPY(message->param.notifyinfo.display, get_isdn_cause(cause, location, e_ext.display_cause));
	}
	if (transfer) {
		memcpy(&message->param.disconnectinfo.transfer, transfer, sizeof(struct param_transfer));
	}
	message_put(message);
	logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
}

