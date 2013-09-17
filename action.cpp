/*****************************************************************************\
**                                                                           **
** PBX4Linux                                                                 **
**                                                                           **
**---------------------------------------------------------------------------**
** Copyright: Andreas Eversberg                                              **
**                                                                           **
** all actions (and hangup) are processed here                               **
**                                                                           **
\*****************************************************************************/ 

#include "main.h"

extern char **environ;


/*
 * process init 'internal' / 'external' / 'vbox-record' / 'partyline'...
 */
void EndpointAppPBX::action_init_call(void)
{
	class Join		*join;

	/* a created call, this should never happen */
	if (ea_endpoint->ep_join_id) {
		if (options.deb & DEBUG_EPOINT)
			PERROR("EPOINT(%d): We already have a call instance, this should never happen!\n", ea_endpoint->ep_serial);
		return;
	}

	/* create join */
	PDEBUG(DEBUG_EPOINT, "EPOINT(%d): Creating new join instance.\n", ea_endpoint->ep_serial);
	join = new JoinPBX(ea_endpoint);
	if (!join)
		FATAL("No memoy for Join instance.\n");
	ea_endpoint->ep_join_id = join->j_serial;
	return;
}

/*
 * process dialing 'internal'
 */
void EndpointAppPBX::action_dialing_internal(void)
{
	struct capa_info	capainfo;
	struct caller_info	callerinfo;
	struct redir_info	redirinfo;
	struct rtp_info		rtpinfo;
	struct dialing_info	dialinginfo;
	struct port_list	*portlist = ea_endpoint->ep_portlist;
	struct lcr_msg		*message;
	struct extension	ext;
	struct route_param	*rparam;

	/* send proceeding, because number is complete */
	set_tone(portlist, "proceeding");
	message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_PROCEEDING);
	message_put(message);
	logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
	new_state(EPOINT_STATE_IN_PROCEEDING);

	/* create bearer/caller/dialinginfo */
	memcpy(&capainfo, &e_capainfo, sizeof(capainfo));
	memcpy(&callerinfo, &e_callerinfo, sizeof(callerinfo));
	memcpy(&redirinfo, &e_redirinfo, sizeof(redirinfo));
	memcpy(&rtpinfo, &e_rtpinfo, sizeof(rtpinfo));
	memset(&dialinginfo, 0, sizeof(dialinginfo));
	dialinginfo.itype = INFO_ITYPE_ISDN_EXTENSION;
	SCPY(dialinginfo.id, e_dialinginfo.id);

	/* process extension */
	if ((rparam = routeparam(e_action, PARAM_EXTENSION)))
		SCPY(dialinginfo.id, rparam->string_value);

	/* process number type */
	if ((rparam = routeparam(e_action, PARAM_TYPE)))
		dialinginfo.ntype = rparam->integer_value;

	/* process service */
	if ((rparam = routeparam(e_action, PARAM_CAPA))) {
		capainfo.bearer_capa = rparam->integer_value;
		if (capainfo.bearer_capa != INFO_BC_SPEECH
		 && capainfo.bearer_capa != INFO_BC_AUDIO) {
			capainfo.bearer_mode = INFO_BMODE_PACKET;
		}
		capainfo.bearer_info1 = INFO_INFO1_NONE;
		capainfo.hlc = INFO_HLC_NONE;
		capainfo.exthlc = INFO_HLC_NONE;
	}
	if ((rparam = routeparam(e_action, PARAM_BMODE))) {
		capainfo.bearer_mode = rparam->integer_value;
	}
	if ((rparam = routeparam(e_action, PARAM_INFO1))) {
		capainfo.bearer_info1 = rparam->integer_value;
	}
	if ((rparam = routeparam(e_action, PARAM_HLC))) {
		capainfo.hlc = rparam->integer_value;
	}
	if ((rparam = routeparam(e_action, PARAM_EXTHLC))) {
		capainfo.exthlc = rparam->integer_value;
	}

	/* process presentation */
	if ((rparam = routeparam(e_action, PARAM_PRESENT))) {
		callerinfo.present = (rparam->integer_value)?INFO_PRESENT_ALLOWED:INFO_PRESENT_RESTRICTED;
	}

	/* check if extension exists AND only if not multiple extensions */
	if (!strchr(dialinginfo.id,',') && !read_extension(&ext, dialinginfo.id)) {
		trace_header("ACTION extension (extension doesn't exist)", DIRECTION_NONE);
		add_trace("extension", NULL, dialinginfo.id);
		end_trace();
		release(RELEASE_JOIN, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, 0, 0, 0);
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		message_disconnect_port(portlist, CAUSE_UNALLOCATED, LOCATION_PRIVATE_LOCAL, "", NULL);
		set_tone(portlist, "cause_86");
		return;
	}
	/* check if internal calls are denied */
	if (e_ext.rights < 1) {
		trace_header("ACTION extension (dialing to extension denied)", DIRECTION_NONE);
		add_trace("extension", NULL, dialinginfo.id);
		end_trace();
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		release(RELEASE_JOIN, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, 0, 0, 0);
		message_disconnect_port(portlist, CAUSE_REJECTED, LOCATION_PRIVATE_LOCAL, "", NULL);
		set_tone(portlist, "cause_81");
		return;
	}

	/* add or update internal call */
	trace_header("ACTION extension (calling)", DIRECTION_NONE);
	add_trace("extension", NULL, dialinginfo.id);
	end_trace();
	message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_SETUP);
	memcpy(&message->param.setup.dialinginfo, &dialinginfo, sizeof(struct dialing_info));
	memcpy(&message->param.setup.redirinfo, &redirinfo, sizeof(struct redir_info));
	memcpy(&message->param.setup.callerinfo, &callerinfo, sizeof(struct caller_info));
	memcpy(&message->param.setup.capainfo, &capainfo, sizeof(struct capa_info));
	memcpy(&message->param.setup.rtpinfo, &rtpinfo, sizeof(struct rtp_info));
	message_put(message);
}

/* process dialing external
 */
void EndpointAppPBX::action_dialing_external(void)
{
	struct capa_info capainfo;
	struct caller_info callerinfo;
	struct redir_info redirinfo;
	struct rtp_info rtpinfo;
	struct dialing_info dialinginfo;
	char *p;
	struct port_list *portlist = ea_endpoint->ep_portlist;
	struct lcr_msg *message;
	struct route_param *rparam;

	/* special processing of delete characters '*' and '#' */
	if (e_ext.delete_ext) {
		/* dialing a # causes a clearing of complete number */
		if (strchr(e_extdialing, '#')) {
			e_extdialing[0] = '\0';
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d): '#' detected: terminal '%s' selected caller id '%s' and continues dialing: '%s'\n", ea_endpoint->ep_serial, e_ext.number, e_callerinfo.id, e_extdialing);
		}
		/* eliminate digits before '*', which is a delete digit
		 */
		if (strchr(e_extdialing, '*')) {
			/* remove digits */
			while((p=strchr(e_extdialing, '*'))) {
				if (p > e_extdialing) { /* only if there is a digit in front */
					UCPY(p-1, p);
					p--;
				}
				UCPY(p, p+1); /* remove '*' */
			}
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d): terminal %s deleted digits and got new string: %s\n", ea_endpoint->ep_serial, e_ext.number, e_extdialing);
		}
	}

	/* create bearer/caller/dialinginfo */
	memcpy(&capainfo, &e_capainfo, sizeof(capainfo));
	memcpy(&callerinfo, &e_callerinfo, sizeof(callerinfo));
	memcpy(&redirinfo, &e_redirinfo, sizeof(redirinfo));
	memcpy(&rtpinfo, &e_rtpinfo, sizeof(rtpinfo));
	memset(&dialinginfo, 0, sizeof(dialinginfo));
	dialinginfo.itype = INFO_ITYPE_ISDN;
//	dialinginfo.sending_complete = 0;
	SCPY(dialinginfo.id, e_extdialing);

	/* process prefix */
	if ((rparam = routeparam(e_action, PARAM_PREFIX)))
		SPRINT(dialinginfo.id, "%s%s", rparam->string_value, e_extdialing);

	if ((rparam = routeparam(e_action, PARAM_CONTEXT)))
		SCPY(dialinginfo.context, rparam->string_value);

	/* process keypad */
	if ((rparam = routeparam(e_action, PARAM_KEYPAD))) {
		SCPY(dialinginfo.keypad, dialinginfo.id);
		dialinginfo.id[0] = '\0';
	}

	/* process number complete */
	if ((rparam = routeparam(e_action, PARAM_COMPLETE)))
		dialinginfo.sending_complete = 1;

	/* process number type */
	if ((rparam = routeparam(e_action, PARAM_TYPE)))
		dialinginfo.ntype = rparam->integer_value;

	/* process service */
	if ((rparam = routeparam(e_action, PARAM_CAPA))) {
		capainfo.bearer_capa = rparam->integer_value;
		if (capainfo.bearer_capa != INFO_BC_SPEECH
		 && capainfo.bearer_capa != INFO_BC_AUDIO) {
			capainfo.bearer_mode = INFO_BMODE_PACKET;
		}
		capainfo.bearer_info1 = INFO_INFO1_NONE;
		capainfo.hlc = INFO_HLC_NONE;
		capainfo.exthlc = INFO_HLC_NONE;
	}
	if ((rparam = routeparam(e_action, PARAM_BMODE))) {
		capainfo.bearer_mode = rparam->integer_value;
	}
	if ((rparam = routeparam(e_action, PARAM_INFO1))) {
		capainfo.bearer_info1 = rparam->integer_value;
	}
	if ((rparam = routeparam(e_action, PARAM_HLC))) {
		capainfo.hlc = rparam->integer_value;
	}
	if ((rparam = routeparam(e_action, PARAM_EXTHLC))) {
		capainfo.exthlc = rparam->integer_value;
	}


	/* process callerid */
	if ((rparam = routeparam(e_action, PARAM_CALLERID))) {
		SCPY(callerinfo.id, rparam->string_value);
	}
	if ((rparam = routeparam(e_action, PARAM_CALLERIDTYPE))) {
		callerinfo.ntype = rparam->integer_value;
	}

	/* process presentation */
	if ((rparam = routeparam(e_action, PARAM_PRESENT))) {
		callerinfo.present = (rparam->integer_value)?INFO_PRESENT_ALLOWED:INFO_PRESENT_RESTRICTED;
	}

	/* process interfaces */
	if ((rparam = routeparam(e_action, PARAM_INTERFACES)))
		SCPY(dialinginfo.interfaces, rparam->string_value);

	/* check if local calls are denied */
	if (e_ext.rights < 2) {
		trace_header("ACTION extern (calling denied)", DIRECTION_NONE);
		end_trace();
		release(RELEASE_JOIN, LOCATION_PRIVATE_LOCAL, CAUSE_REJECTED, 0, 0, 0);
		set_tone(portlist, "cause_82");
		denied:
		message_disconnect_port(portlist, CAUSE_REJECTED, LOCATION_PRIVATE_LOCAL, "", NULL);
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		return;
	}

	if (!strncmp(dialinginfo.id, options.national, strlen(options.national))
	 || dialinginfo.ntype == INFO_NTYPE_NATIONAL
	 || dialinginfo.ntype == INFO_NTYPE_INTERNATIONAL) {
		/* check if national calls are denied */
		if (e_ext.rights < 3) {
			trace_header("ACTION extern (national calls denied)", DIRECTION_NONE);
			end_trace();
			release(RELEASE_JOIN, LOCATION_PRIVATE_LOCAL, CAUSE_REJECTED, 0, 0, 0);
			set_tone(portlist, "cause_83");
			goto denied;
		}
	}

	if (!strncmp(dialinginfo.id, options.international, strlen(options.international))
	 || dialinginfo.ntype == INFO_NTYPE_INTERNATIONAL) {
		/* check if international calls are denied */
		if (e_ext.rights < 4) {
			trace_header("ACTION extern (international calls denied)", DIRECTION_NONE);
			end_trace();
			release(RELEASE_JOIN, LOCATION_PRIVATE_LOCAL, CAUSE_REJECTED, 0, 0, 0);
			set_tone(portlist, "cause_84");
			goto denied;
		}
	}

	/* add or update outgoing call */
	trace_header("ACTION extern (calling)", DIRECTION_NONE);
	add_trace("number", NULL, dialinginfo.id);
	if (dialinginfo.sending_complete)
	add_trace("number", "complete", "yes");
	if (dialinginfo.interfaces[0])
		add_trace("interfaces", NULL, dialinginfo.interfaces);
	end_trace();
	message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_SETUP);
	memcpy(&message->param.setup.dialinginfo, &dialinginfo, sizeof(struct dialing_info));
	memcpy(&message->param.setup.redirinfo, &redirinfo, sizeof(struct redir_info));
	memcpy(&message->param.setup.callerinfo, &callerinfo, sizeof(struct caller_info));
	memcpy(&message->param.setup.capainfo, &capainfo, sizeof(struct capa_info));
	memcpy(&message->param.setup.rtpinfo, &rtpinfo, sizeof(struct rtp_info));
	message_put(message);
}


/*
 * process dialing the "am" and record
 */
void EndpointAppPBX::action_dialing_vbox_record(void)
{
	struct dialing_info dialinginfo;
	struct port_list *portlist = ea_endpoint->ep_portlist;
	struct lcr_msg *message;
	struct extension ext;
	struct route_param *rparam;

	portlist = ea_endpoint->ep_portlist;

	/* check for given extension */
	if (!(rparam = routeparam(e_action, PARAM_EXTENSION))) {
		trace_header("ACTION vbox-record (no extension given by parameter)", DIRECTION_NONE);
		end_trace();

		new_state(EPOINT_STATE_OUT_DISCONNECT);
		message_disconnect_port(portlist, CAUSE_SERVICEUNAVAIL, LOCATION_PRIVATE_LOCAL, "", NULL);
		set_tone(portlist, "cause_3f");
		return;
	}

	/* check if extension exists */
	if (!read_extension(&ext, rparam->string_value)) {
		trace_header("ACTION vbox-record (given extension does not exists)", DIRECTION_NONE);
		add_trace("extension", NULL, "%s", rparam->string_value);
		end_trace();
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		message_disconnect_port(portlist, CAUSE_UNALLOCATED, LOCATION_PRIVATE_LOCAL, "", NULL);
		set_tone(portlist, "cause_86");
		return;
	}

	/* check if internal calls are denied */
	if (e_ext.rights < 1) {
		trace_header("ACTION vbox-record (internal calls are denied)", DIRECTION_NONE);
		end_trace();
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		message_disconnect_port(portlist, CAUSE_REJECTED, LOCATION_PRIVATE_LOCAL, "", NULL);
		set_tone(portlist, "cause_81");
		return;
	}

	set_tone(portlist, "proceeding");
	message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_PROCEEDING);
	message_put(message);
	logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
	new_state(EPOINT_STATE_IN_PROCEEDING);

	memset(&dialinginfo, 0, sizeof(dialinginfo));
	dialinginfo.itype = INFO_ITYPE_VBOX;
	dialinginfo.sending_complete = 1;
	SCPY(dialinginfo.id, rparam->string_value);

	/* append special announcement (if given) */
	if ((rparam = routeparam(e_action, PARAM_ANNOUNCEMENT)))
	if (rparam->string_value[0]) {
		SCAT(dialinginfo.id, ",");
		SCAT(dialinginfo.id, rparam->string_value);
	}

	/* add or update internal call */
	trace_header("ACTION vbox-record (calling)", DIRECTION_NONE);
	add_trace("extension", NULL, "%s", dialinginfo.id);
	end_trace();
	message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_SETUP);
	memcpy(&message->param.setup.dialinginfo, &dialinginfo, sizeof(struct dialing_info));
	memcpy(&message->param.setup.redirinfo, &e_redirinfo, sizeof(struct redir_info));
	memcpy(&message->param.setup.callerinfo, &e_callerinfo, sizeof(struct caller_info));
	memcpy(&message->param.setup.capainfo, &e_capainfo, sizeof(struct capa_info));
	message_put(message);
}


/*
 * process partyline
 */
void EndpointAppPBX::action_init_partyline(void)
{
	class Join *join;
	class JoinPBX *joinpbx;
	struct port_list *portlist = ea_endpoint->ep_portlist;
	struct lcr_msg *message;
	struct route_param *rparam;
	int partyline, jingle = 0;
	struct join_relation *relation;

	portlist = ea_endpoint->ep_portlist;

	/* check for given extension */
	if (!(rparam = routeparam(e_action, PARAM_ROOM))) {
		trace_header("ACTION partyline (no room parameter)", DIRECTION_NONE);
		end_trace();
		noroom:
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		message_disconnect_port(portlist, CAUSE_SERVICEUNAVAIL, LOCATION_PRIVATE_LOCAL, "", NULL);
		set_tone(portlist, "cause_3f");
		return;
	}
	if (rparam->integer_value <= 0) {
		trace_header("ACTION partyline (illegal room parameter)", DIRECTION_NONE);
		add_trace("room", NULL, "%d", rparam->integer_value);
		end_trace();
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d): invalid value for 'room'.\n", ea_endpoint->ep_serial);
		goto noroom;
	}
	partyline = rparam->integer_value;
	if ((rparam = routeparam(e_action, PARAM_JINGLE)))
		jingle = 1;

	/* don't create join if partyline exists */
	join = join_first;
	while(join) {
		if (join->j_type == JOIN_TYPE_PBX) {
			joinpbx = (class JoinPBX *)join;
			if (joinpbx->j_partyline == partyline)
				break;
		}
		join = join->next;
	}
	if (!join) {
		/* create join */
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d): Creating new join instance.\n", ea_endpoint->ep_serial);
		if (!(join = new JoinPBX(ea_endpoint)))
			FATAL("No memory for join object\n");
	} else {
//NOTE: joinpbx must be set here
		/* add relation to existing join */
		if (!(relation=joinpbx->add_relation()))
			FATAL("No memory for join relation\n");
		relation->type = RELATION_TYPE_SETUP;
		relation->channel_state = 1;
		relation->rx_state = NOTIFY_STATE_ACTIVE;
		relation->tx_state = NOTIFY_STATE_ACTIVE;
		relation->epoint_id = ea_endpoint->ep_serial;

	}
	ea_endpoint->ep_join_id = join->j_serial;

	set_tone(portlist, "proceeding");
	message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_PROCEEDING);
	message_put(message);
	logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
	new_state(EPOINT_STATE_IN_PROCEEDING);

	/* send setup to join */
	trace_header("ACTION partyline (calling)", DIRECTION_NONE);
	add_trace("room", NULL, "%d", partyline);
	add_trace("jingle", NULL, (jingle)?"on":"off");
	end_trace();
	message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_join_id, EPOINT_TO_JOIN, MESSAGE_SETUP);
	message->param.setup.partyline = partyline;
	message->param.setup.partyline_jingle = jingle;
	memcpy(&message->param.setup.dialinginfo, &e_dialinginfo, sizeof(struct dialing_info));
	memcpy(&message->param.setup.redirinfo, &e_redirinfo, sizeof(struct redir_info));
	memcpy(&message->param.setup.callerinfo, &e_callerinfo, sizeof(struct caller_info));
	memcpy(&message->param.setup.capainfo, &e_capainfo, sizeof(struct capa_info));
	message_put(message);
}


/*
 * process hangup of all calls
 */
void EndpointAppPBX::action_hangup_call(void)
{
	trace_header("ACTION hangup", DIRECTION_NONE);
	end_trace();
}


/*
 * process dialing 'login'
 */
void EndpointAppPBX::action_dialing_login(void)
{
	struct port_list *portlist = ea_endpoint->ep_portlist;
	struct lcr_msg *message;
	char *extension;
	struct route_param *rparam;

	/* extension parameter */
	if ((rparam = routeparam(e_action, PARAM_EXTENSION))) {
		/* extension is given by parameter */
		extension = rparam->string_value;
		if (extension[0] == '\0')
			return;
		if (!read_extension(&e_ext, extension)) {
			trace_header("ACTION login (extension doesn't exist)", DIRECTION_NONE);
			add_trace("extension", NULL, "%s", extension);
			end_trace();
			/* extension doesn't exist */
			new_state(EPOINT_STATE_OUT_DISCONNECT);
			message_disconnect_port(portlist, CAUSE_UNALLOCATED, LOCATION_PRIVATE_LOCAL, "", NULL);
			set_tone(portlist, "cause_86");
			return;
		}
	} else {
		/* extension must be given by dialstring */
		extension = e_extdialing;
		if (extension[0] == '\0')
			return;
		if (!read_extension(&e_ext, extension)) {
			trace_header("ACTION login (extension incomplete or does not exist)", DIRECTION_NONE);
			add_trace("extension", NULL, "%s", extension);
			end_trace();
			return;
		}
	}

	/* we changed our extension */
	SCPY(e_ext.number, extension);
	new_state(EPOINT_STATE_CONNECT);
	e_dtmf = 1;
	e_connectedmode = 1;

	/* send connect with extension's caller id (COLP) */
	message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_CONNECT);
	SCPY(message->param.connectinfo.id, e_ext.callerid);
	message->param.connectinfo.ntype = e_ext.callerid_type;
	if (e_ext.callerid_present==INFO_PRESENT_ALLOWED && e_callerinfo.present==INFO_PRESENT_RESTRICTED)
		message->param.connectinfo.present = INFO_PRESENT_RESTRICTED;
	else	message->param.connectinfo.present = e_ext.callerid_present;
	/* handle restricted caller ids */
	apply_callerid_restriction(&e_ext, message->param.connectinfo.id, &message->param.connectinfo.ntype, &message->param.connectinfo.present, &message->param.connectinfo.screen, message->param.connectinfo.extension, message->param.connectinfo.name);
	/* display callerid if desired for extension */
	SCPY(message->param.connectinfo.display, apply_callerid_display(message->param.connectinfo.id, message->param.connectinfo.itype, message->param.connectinfo.ntype, message->param.connectinfo.present, message->param.connectinfo.screen, message->param.connectinfo.extension, message->param.connectinfo.name));
	message->param.connectinfo.ntype = e_ext.callerid_type;
	message_put(message);
	logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);

	/* set our caller id */
	SCPY(e_callerinfo.id, e_ext.callerid);
	e_callerinfo.ntype = e_ext.callerid_type;
	e_callerinfo.present = e_ext.callerid_present;

	/* enable connectedmode */
	e_connectedmode = 1;
	e_dtmf = 1;

	if (!(rparam = routeparam(e_action, PARAM_NOPASSWORD))) {
		/* make call state to enter password */
		trace_header("ACTION login (ask for password)", DIRECTION_NONE);
		add_trace("extension", NULL, "%s", e_ext.number);
		end_trace();
		new_state(EPOINT_STATE_IN_OVERLAP);
		e_ruleset = NULL;
		e_rule = NULL;
		e_action = &action_password;
		unsched_timer(&e_match_timeout);
		e_match_to_action = NULL;
		e_dialinginfo.id[0] = '\0';
		e_extdialing = strchr(e_dialinginfo.id, '\0');

		/* set timeout */
		schedule_timer(&e_password_timeout, 20, 0);

		/* do dialing */
		process_dialing(0);
	} else {
		/* make call state  */
		new_state(EPOINT_STATE_IN_OVERLAP);
		e_ruleset = ruleset_main;
		if (e_ruleset)
			e_rule = e_ruleset->rule_first;
		e_action = NULL;
		e_dialinginfo.id[0] = '\0';
		e_extdialing = e_dialinginfo.id;
		set_tone(portlist, "dialpbx");
	}
}


/*
 * process init 'change_callerid'
 */
void EndpointAppPBX::action_init_change_callerid(void)
{
	struct port_list *portlist = ea_endpoint->ep_portlist;

	if (!e_ext.change_callerid) {
		/* service not available */
		trace_header("ACTION change-callerid (denied for this caller)", DIRECTION_NONE);
		end_trace();
		message_disconnect_port(portlist, CAUSE_SERVICEUNAVAIL, LOCATION_PRIVATE_LOCAL, "", NULL);
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		set_tone(portlist,"cause_87");
		return;
	}
}

/* process dialing callerid
 */
void EndpointAppPBX::_action_callerid_calleridnext(int next)
{
	struct port_list *portlist = ea_endpoint->ep_portlist;
	struct route_param *rparam;
	char buffer[64], *callerid;
	char old_id[64] = "", new_id[64] = "";
	int old_type=0, new_type=0, old_present=0, new_present=0;

	if ((rparam = routeparam(e_action, PARAM_CALLERID))) {
		/* the caller ID is given by parameter */
		callerid = rparam->string_value;
	} else {
		/* caller ID is dialed */
		if (!strchr(e_extdialing, '#')) {
			/* no complete ID yet */
			return;
		}
		*strchr(e_extdialing, '#') = '\0';
		callerid = e_extdialing;
	}

	/* given callerid type */
	if ((rparam = routeparam(e_action, PARAM_CALLERIDTYPE)))
		switch(rparam->integer_value) {
			case INFO_NTYPE_SUBSCRIBER:
			SPRINT(buffer, "s%s", callerid);
			callerid = buffer;
			break;
			case INFO_NTYPE_NATIONAL:
			SPRINT(buffer, "n%s", callerid);
			callerid = buffer;
			break;
			case INFO_NTYPE_INTERNATIONAL:
			SPRINT(buffer, "i%s", callerid);
			callerid = buffer;
			break;
			default:
			SPRINT(buffer, "%s", callerid);
			callerid = buffer;
			break;
		}

	/* caller id complete, dialing with new caller id */
	/* write new parameters */
	if (read_extension(&e_ext, e_ext.number)) {
		old_present = (!next)?e_ext.callerid_present:e_ext.id_next_call_present;
		old_type = (!next)?e_ext.callerid_type:e_ext.id_next_call_type;
		SCPY(old_id, (!next)?e_ext.callerid:e_ext.id_next_call);
		if (callerid[0] == '\0') {
			/* no caller id */
			(!next)?e_ext.callerid_present:e_ext.id_next_call_present = INFO_PRESENT_RESTRICTED;
		} else {
			/* new caller id */
			(!next)?e_ext.callerid_present:e_ext.id_next_call_present = INFO_PRESENT_ALLOWED;
			if ((rparam = routeparam(e_action, PARAM_PRESENT))) if (rparam->integer_value == 0)
				(!next)?e_ext.callerid_present:e_ext.id_next_call_present = INFO_PRESENT_RESTRICTED;
			if (e_ext.callerid_type == INFO_NTYPE_UNKNOWN) /* if callerid is unknown, the given id is not nationalized */ {
				SCPY((!next)?e_ext.callerid:e_ext.id_next_call, callerid);
				(!next)?e_ext.callerid_type:e_ext.id_next_call_type = INFO_NTYPE_UNKNOWN;
			} else {
				SCPY((!next)?e_ext.callerid:e_ext.id_next_call, nationalize_callerinfo(callerid,&((!next)?e_ext.callerid_type:e_ext.id_next_call_type), options.national, options.international));
			}
			if (!next) e_ext.id_next_call_type = -1;
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d): nationalized callerid: '%s' type=%d\n", ea_endpoint->ep_serial, (!next)?e_ext.callerid:e_ext.id_next_call, (!next)?e_ext.callerid_type:e_ext.id_next_call_type);
		}
		new_present = (!next)?e_ext.callerid_present:e_ext.id_next_call_present;
		new_type = (!next)?e_ext.callerid_type:e_ext.id_next_call_type;
		SCPY(new_id, (!next)?e_ext.callerid:e_ext.id_next_call);
		write_extension(&e_ext, e_ext.number);
	}

	/* function activated */
	if (next)
		trace_header("ACTION change-callerid (only next call)", DIRECTION_NONE);
	else
		trace_header("ACTION change-callerid (all future calls)", DIRECTION_NONE);
	add_trace("old", "caller id", "%s", numberrize_callerinfo(old_id, old_type, options.national, options.international));
	add_trace("old", "present", "%s", (old_present==INFO_PRESENT_RESTRICTED)?"restricted":"allowed");
	add_trace("new", "caller id", "%s", numberrize_callerinfo(new_id, new_type, options.national, options.international));
	add_trace("new", "present", "%s", (new_present==INFO_PRESENT_RESTRICTED)?"restricted":"allowed");
	end_trace();
	message_disconnect_port(portlist, CAUSE_NORMAL, LOCATION_PRIVATE_LOCAL, "", NULL);
	new_state(EPOINT_STATE_OUT_DISCONNECT);
	set_tone(portlist,"activated");
}

/* process dialing callerid for all call
 */
void EndpointAppPBX::action_dialing_callerid(void)
{
	_action_callerid_calleridnext(0);
}

/* process dialing callerid for next call
 */
void EndpointAppPBX::action_dialing_calleridnext(void)
{
	_action_callerid_calleridnext(1);
}


/*
 * process init 'change_forward'
 */
void EndpointAppPBX::action_init_change_forward(void)
{
	struct port_list *portlist = ea_endpoint->ep_portlist;

	if (!e_ext.change_forward) {
		trace_header("ACTION change-forward (denied for this caller)", DIRECTION_NONE);
		end_trace();
		/* service not available */		
		message_disconnect_port(portlist, CAUSE_SERVICEUNAVAIL, LOCATION_PRIVATE_LOCAL, "", NULL);
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		set_tone(portlist,"cause_87");
		return;
	}
}

/* process dialing forwarding
 */
void EndpointAppPBX::action_dialing_forward(void)
{
	struct port_list *portlist = ea_endpoint->ep_portlist;
	int diversion = INFO_DIVERSION_CFU;
	char *dest = e_extdialing;
	struct route_param *rparam;

	/* if diversion type is given */
	if ((rparam = routeparam(e_action, PARAM_DIVERSION)))
		diversion = rparam->integer_value;

	if ((rparam = routeparam(e_action, PARAM_DEST))) {
		/* if destination is given */
		dest = rparam->string_value;
	} else {
		if (!strchr(e_extdialing, '#'))
			return;
		*strchr(e_extdialing, '#') = '\0';
		dest = e_extdialing;
	}

	PDEBUG(DEBUG_EPOINT, "EPOINT(%d): terminal %s: storing forwarding to '%s'.\n", ea_endpoint->ep_serial, e_ext.number, dest);
	if (read_extension(&e_ext, e_ext.number)) {
		switch(diversion) {
			case INFO_DIVERSION_CFU:
			trace_header("ACTION change-forward (new CFU=unconditional)", DIRECTION_NONE);
			add_trace("destin'", NULL, "%s", dest);
			end_trace();
			SCPY(e_ext.cfu, dest);
			break;
			case INFO_DIVERSION_CFB:
			trace_header("ACTION change-forward (new CFB=busy)", DIRECTION_NONE);
			add_trace("destin'", NULL, "%s", dest);
			end_trace();
			SCPY(e_ext.cfb, dest);
			break;
			case INFO_DIVERSION_CFNR:
			if ((rparam = routeparam(e_action, PARAM_DELAY)))
				e_ext.cfnr_delay = rparam->integer_value;
			trace_header("ACTION change-forward (new CFNR=no response)", DIRECTION_NONE);
			add_trace("destin'", NULL, "%s", dest);
			add_trace("delay", NULL, "%s", e_ext.cfnr_delay);
			end_trace();
			SCPY(e_ext.cfnr, dest);
			break;
			case INFO_DIVERSION_CFP:
			trace_header("ACTION change-forward (new CFP=parallel)", DIRECTION_NONE);
			add_trace("destin'", NULL, "%s", dest);
			end_trace();
			SCPY(e_ext.cfp, dest);
			break;
		}
		write_extension(&e_ext, e_ext.number);
	}
	/* function (de)activated */
	message_disconnect_port(portlist, CAUSE_NORMAL, LOCATION_PRIVATE_LOCAL, "", NULL);
	new_state(EPOINT_STATE_OUT_DISCONNECT);
	if (dest[0])
		set_tone(portlist,"activated");
	else
		set_tone(portlist,"deactivated");
}


/* process dialing redial
*/
void EndpointAppPBX::action_init_redial_reply(void)
{
	struct port_list *portlist = ea_endpoint->ep_portlist;

	e_select = 0;
	if (!e_ext.last_out[0]) {
		trace_header("ACTION redial/reply (no last number stored)", DIRECTION_NONE);
		end_trace();
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		message_disconnect_port(portlist, CAUSE_SERVICEUNAVAIL, LOCATION_PRIVATE_LOCAL, "", NULL);
		set_tone(portlist, "cause_3f");
		return;
	}
}

/* process dialing redial
*/
void EndpointAppPBX::_action_redial_reply(int in)
{
	struct lcr_msg *message;
	char *last;
	struct route_param *rparam;

	last = (in)?e_ext.last_in[0]:e_ext.last_out[0];

	/* if no display is available */
	if (!e_ext.display_menu)
		goto nodisplay;
	if (ea_endpoint->ep_portlist->port_type!=PORT_TYPE_DSS1_NT_IN && ea_endpoint->ep_portlist->port_type!=PORT_TYPE_DSS1_NT_OUT)
		goto nodisplay;

	/* if select is not given */
	if (!(rparam = routeparam(e_action, PARAM_SELECT)))
		goto nodisplay;

	/* scroll menu */
	if (e_extdialing[0]=='*' || e_extdialing[0]=='1') {
		/* find prev entry */
		e_select--;
		if (e_select < 0)
			e_select = 0;

	}
	if (e_extdialing[0]=='#' || e_extdialing[0]=='3') {
		/* find next entry */
		e_select++;
		if (e_select >= MAX_REMEMBER) {
			e_select--;
		} else if (in) {
			if (e_ext.last_in[e_select][0] == '\0')
				e_select--;
		} else
			if (e_ext.last_out[e_select][0] == '\0')
				e_select--;

	}

	last = (in)?e_ext.last_in[e_select]:e_ext.last_out[e_select];
	if (e_extdialing[0]=='0' || e_extdialing[0]=='2') {
		nodisplay:
		if (in)
			trace_header("ACTION reply (dialing)", DIRECTION_NONE);
		else
			trace_header("ACTION redial (dialing)", DIRECTION_NONE);
		add_trace("number", NULL, "%s", last);
		add_trace("last but", NULL, "%d", e_select);
		end_trace();
		SCPY(e_dialinginfo.id, last);
		e_extdialing = e_dialinginfo.id;
		e_action = NULL;
		process_dialing(0);
		return;
	}
	e_extdialing[0] = '\0';
	
	/* send display message to port */
	message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_portlist->port_id, EPOINT_TO_PORT, MESSAGE_NOTIFY);
	if (!strncmp(last, "extern:", 7))
		SPRINT(message->param.notifyinfo.display, "(%d) %s ext", e_select+1, last+7);
	else
	if (!strncmp(last, "intern:", 7))
		SPRINT(message->param.notifyinfo.display, "(%d) %s int", e_select+1, last+7);
	else
	if (!strncmp(last, "chan:", 4))
		SPRINT(message->param.notifyinfo.display, "(%d) %s chan", e_select+1, last+5);
	else
	if (!strncmp(last, "vbox:", 5))
		SPRINT(message->param.notifyinfo.display, "(%d) %s vbox", e_select+1, last+5);
	else
		SPRINT(message->param.notifyinfo.display, "(%d) %s", e_select+1, (last[0])?last:"- empty -");
	PDEBUG(DEBUG_EPOINT, "EPOINT(%d): terminal %s sending display:%s\n", ea_endpoint->ep_serial, e_ext.number, message->param.notifyinfo.display);
	message_put(message);
	logmessage(message->type, &message->param, ea_endpoint->ep_portlist->port_id, DIRECTION_OUT);
}

/* process dialing redial
*/
void EndpointAppPBX::action_dialing_redial(void)
{
	_action_redial_reply(0);
}

/* process dialing reply
*/
void EndpointAppPBX::action_dialing_reply(void)
{
	_action_redial_reply(1);
}


/* dialing powerdialing delay
 */
void EndpointAppPBX::action_dialing_powerdial(void)
{
	struct port_list *portlist = ea_endpoint->ep_portlist;
	struct lcr_msg *message;
	struct route_param *rparam;

	/* power dialing only possible if we have a last dialed number */
	if (!e_ext.last_out[0]) {
		trace_header("ACTION powerdial (no last number stored)", DIRECTION_NONE);
		end_trace();
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		message_disconnect_port(portlist, CAUSE_SERVICEUNAVAIL, LOCATION_PRIVATE_LOCAL, "", NULL);
		set_tone(portlist, "cause_3f");
		return;
	}

	/* limit */
	if ((rparam = routeparam(e_action, PARAM_LIMIT))) {
		e_powerlimit = rparam->integer_value;
	} else {
		e_powerlimit = 0;
	}

	/* delay */
	if ((rparam = routeparam(e_action, PARAM_DELAY))) {
		e_powerdelay = rparam->integer_value;
	} else {
		/* delay incomplete */
		if (!strchr(e_extdialing, '#'))
			return;
		*strchr(e_extdialing, '#') = '\0';
		e_powerdelay = e_extdialing[0]?atoi(e_extdialing): 0;
	}

	if (e_powerdelay < 1)
		e_powerdelay = 0.2;
	trace_header("ACTION powerdial (dialing)", DIRECTION_NONE);
	add_trace("number", NULL, "%s", e_ext.last_out[0]);
	add_trace("delay", NULL, "%d", e_powerdelay);
	end_trace();

	/* send connect to avoid overlap timeout */
//	new_state(EPOINT_STATE_CONNECT); connect may prevent further dialing
	if (e_ext.number[0])
		e_dtmf = 1;
	memset(&e_connectinfo, 0, sizeof(e_connectinfo));
	message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_portlist->port_id, EPOINT_TO_PORT, MESSAGE_CONNECT);
	message_put(message);
	logmessage(message->type, &message->param, ea_endpoint->ep_portlist->port_id, DIRECTION_OUT);

	/* do dialing */
	SCPY(e_dialinginfo.id, e_ext.last_out[0]);
	e_powerdial_on = 1; /* indicates the existence of powerdialing but no redial time given */
	e_powercount = 0;
	e_action = NULL;
	process_dialing(0);
}


/* dialing callback
 */
void EndpointAppPBX::action_dialing_callback(void)
{
	struct port_list *portlist = ea_endpoint->ep_portlist;
	struct route_param *rparam;
	struct extension cbext;

	portlist = ea_endpoint->ep_portlist;

	/* check given extension */
	if (!(rparam = routeparam(e_action, PARAM_EXTENSION))) {
		noextension:
		trace_header("ACTION callback (no extension defined)", DIRECTION_NONE);
		end_trace();
		disconnect:

		new_state(EPOINT_STATE_OUT_DISCONNECT);
		message_disconnect_port(portlist, CAUSE_SERVICEUNAVAIL, LOCATION_PRIVATE_LOCAL, "", NULL);
		set_tone(portlist, "cause_3f");
		e_action = NULL;
		e_cbcaller[0] = e_cbdialing[0] = '\0';
		return;
	}

	/* if extension is given */
	SCPY(e_cbcaller, rparam->string_value);
	if (e_cbcaller[0] == '\0')
		goto noextension;

	/* read callback extension */
	memset(&cbext, 0, sizeof(cbext));
	if (!read_extension(&cbext, e_cbcaller)) {
		trace_header("ACTION callback (extension doesn't exist)", DIRECTION_NONE);
		add_trace("extension", NULL, "%s", e_cbcaller);
		end_trace();
		goto disconnect;
	}

	/* if password is not given */
	if (cbext.password[0] == '\0') {
		trace_header("ACTION callback (no password set)", DIRECTION_NONE);
		add_trace("extension", NULL, "%s", e_cbcaller);
		end_trace();
		goto disconnect;
	}

	/* callback only possible if callerid exists OR it is given */
	if ((rparam = routeparam(e_action, PARAM_CALLTO)))
		SCPY(e_cbto, rparam->string_value);
	if (e_cbto[0]) {
		trace_header("ACTION callback (alternative caller id)", DIRECTION_NONE);
		add_trace("extension", NULL, "%s", e_cbcaller);
		add_trace("callerid", NULL, "%s", e_cbto);
		end_trace();
		SCPY(e_callerinfo.id, e_cbto);
		e_callerinfo.ntype = INFO_NTYPE_UNKNOWN;
		e_callerinfo.present = INFO_PRESENT_ALLOWED;
	}
	if (e_callerinfo.id[0]=='\0' || e_callerinfo.present==INFO_PRESENT_NOTAVAIL) {
		trace_header("ACTION callback (no caller ID available)", DIRECTION_NONE);
		add_trace("extension", NULL, "%s", e_cbcaller);
		end_trace();
		goto disconnect;
	}
	/* present id */
	e_callerinfo.present = INFO_PRESENT_ALLOWED;

}

/*
 * process hangup 'callback'
 */
void EndpointAppPBX::action_hangup_callback(void)
{
	struct route_param *rparam;
	int delay;

	/* set delay */
	delay = 2; /* default value */
	if ((rparam = routeparam(e_action, PARAM_DELAY)))
	if (rparam->integer_value>0)
		delay = rparam->integer_value;

	/* dialing after callback */
	if ((rparam = routeparam(e_action, PARAM_PREFIX)))
		SCPY(e_cbdialing, rparam->string_value);
	else
		SCPY(e_cbdialing, e_extdialing);

	trace_header("ACTION callback (dialing)", DIRECTION_NONE);
	add_trace("extension", NULL, "%s", e_cbcaller);
	add_trace("caller id", NULL, "%s", e_callerinfo.id);
	add_trace("delay", NULL, "%d", delay);
	add_trace("dialing", NULL, "%s", e_cbdialing);
	end_trace();

	/* set time to callback */
	schedule_timer(&e_callback_timeout, delay, 0);
}


/*
 * dialing action abbreviation
 */
void EndpointAppPBX::action_dialing_abbrev(void)
{
	struct port_list *portlist = ea_endpoint->ep_portlist;
	char *abbrev, *phone, *name;
	int result;

	portlist = ea_endpoint->ep_portlist;

	/* abbrev dialing is only possible if we have a caller defined */
	if (!e_ext.number[0]) {
		trace_header("ACTION abbreviation (only for extension)", DIRECTION_NONE);
		end_trace();
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		message_disconnect_port(portlist, CAUSE_SERVICEUNAVAIL, LOCATION_PRIVATE_LOCAL, "", NULL);
		set_tone(portlist, "cause_3f");
		return;
	}

	/* check abbreviation */
	abbrev = e_extdialing;
	phone = NULL;
	name = NULL;
	result = parse_phonebook(e_ext.number, &abbrev, &phone, &name);
	if (result == 0) {
		trace_header("ACTION abbreviation (not found)", DIRECTION_NONE);
		add_trace("abbrev", NULL, "%s", abbrev);
		end_trace();
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		message_disconnect_port(portlist, CAUSE_UNALLOCATED, LOCATION_PRIVATE_LOCAL, "", NULL);
		set_tone(portlist, "cause_01");
		return;
	}
	if (result == -1) { /* may match if more digits are dialed */
		return;
	}

	/* dial abbreviation */	
	trace_header("ACTION abbreviation (dialing)", DIRECTION_NONE);
	add_trace("abbrev", NULL, "%s", abbrev);
	add_trace("number", NULL, "%s", phone);
	if (name) if (name[0])
		add_trace("name", NULL, "%s", name);
	end_trace();
	SCPY(e_dialinginfo.id, phone);
	e_extdialing = e_dialinginfo.id;
	e_action = NULL;
	process_dialing(0);
}


/* process dialing 'test'
 */
void EndpointAppPBX::action_dialing_test(void)
{
	unsigned int cause;
	char causestr[16];
	struct port_list *portlist = ea_endpoint->ep_portlist;
	struct lcr_msg *message;
	class Port *port;
	char testcode[32] = "";
	struct route_param *rparam;

	/* given testcode */
	if ((rparam = routeparam(e_action, PARAM_PREFIX)))
		SCPY(testcode, rparam->string_value);
	SCAT(testcode, e_extdialing);

	switch(testcode[0]) {
		case '1':
		trace_header("ACTION test", DIRECTION_NONE);
		add_trace("test", NULL, "proceeding");
		end_trace();
		new_state(EPOINT_STATE_IN_PROCEEDING);
		set_tone(portlist, "proceeding");
		message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_PROCEEDING);
		message_put(message);
		logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
		break;
		
		case '2':
		trace_header("ACTION test", DIRECTION_NONE);
		add_trace("test", NULL, "alerting");
		end_trace();
		new_state(EPOINT_STATE_IN_ALERTING);
		set_tone(portlist, "ringpbx");
		message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_ALERTING);
		message_put(message);
		logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
		break;
		
		case '3':
		trace_header("ACTION test", DIRECTION_NONE);
		add_trace("test", NULL, "echo");
		end_trace();
		new_state(EPOINT_STATE_CONNECT);
		if (e_ext.number[0])
			e_dtmf = 1;
		set_tone(portlist, NULL);
		memset(&e_connectinfo, 0, sizeof(e_connectinfo));
		SCPY(e_connectinfo.id, e_callerinfo.id);
		SCPY(e_connectinfo.extension, e_callerinfo.extension);
		e_connectinfo.itype = e_callerinfo.itype;
		e_connectinfo.ntype = e_callerinfo.ntype;
		e_connectinfo.present = e_callerinfo.present;
		e_connectinfo.screen = e_callerinfo.screen;
		message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_CONNECT);
		memcpy(&message->param.connectinfo, &e_connectinfo, sizeof(struct connect_info));
		/* handle restricted caller ids */
		apply_callerid_restriction(&e_ext, message->param.connectinfo.id, &message->param.connectinfo.ntype, &message->param.connectinfo.present, &message->param.connectinfo.screen, message->param.connectinfo.extension, message->param.connectinfo.name);
		/* display callerid if desired for extension */
		SCPY(message->param.connectinfo.display, apply_callerid_display(message->param.connectinfo.id, message->param.connectinfo.itype, message->param.connectinfo.ntype, message->param.connectinfo.present, message->param.connectinfo.screen, message->param.connectinfo.extension, message->param.connectinfo.name));
		message_put(message);
		logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);

		port = find_port_id(portlist->port_id);
		if (port) {
			port->set_echotest(1);
		}
		break;
		
		case '4':
		trace_header("ACTION test", DIRECTION_NONE);
		add_trace("test", NULL, "tone");
		end_trace();
		new_state(EPOINT_STATE_CONNECT);
		if (e_ext.number[0])
			e_dtmf = 1;
		memset(&e_connectinfo, 0, sizeof(e_connectinfo));
		message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_CONNECT);
		message_put(message);
		logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
		set_tone(portlist, "test");
		break;
		
		case '5':
		trace_header("ACTION test", DIRECTION_NONE);
		add_trace("test", NULL, "hold music");
		end_trace();
		new_state(EPOINT_STATE_CONNECT);
		if (e_ext.number[0])
			e_dtmf = 1;
		memset(&e_connectinfo, 0, sizeof(e_connectinfo));
		message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_CONNECT);
		message_put(message);
		logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
		set_tone(portlist, "hold");
		break;
		
		case '6':
		if (strlen(testcode) < 4)
			break;
		testcode[4] = '\0';
		cause = atoi(testcode+1);
		if (cause > 255)
			cause = 0;
		trace_header("ACTION test", DIRECTION_NONE);
		add_trace("test", NULL, "announcement");
		add_trace("cause", NULL, "%d", cause);
		end_trace();
		new_state(EPOINT_STATE_CONNECT);
		if (e_ext.number[0])
			e_dtmf = 1;
		SPRINT(causestr,"cause_%02x",cause);
		memset(&e_connectinfo, 0, sizeof(e_connectinfo));
		message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_CONNECT);
		message_put(message);
		logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
		set_tone(portlist, causestr);
		break;
		
		case '7':
		if (strlen(testcode) < 4)
			break;
		testcode[4] = '\0';
		cause = atoi(testcode+1);
		if (cause > 127)
			cause = 0;
		trace_header("ACTION test", DIRECTION_NONE);
		add_trace("test", NULL, "disconnect");
		add_trace("cause", NULL, "%d", cause);
		end_trace();
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		SPRINT(causestr,"cause_%02x",cause);
		message_disconnect_port(portlist, cause, LOCATION_PRIVATE_LOCAL, "", NULL);
		set_tone(portlist, causestr);
		break;

		case '8': /* release */
		trace_header("ACTION test", DIRECTION_NONE);
		add_trace("test", NULL, "release");
		add_trace("cause", NULL, "16");
		end_trace();
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		message_disconnect_port(portlist, CAUSE_NORMAL, LOCATION_PRIVATE_LOCAL, "", NULL);
		set_tone(portlist, "release");
		break;

		case '9': /* text callerid test */
		trace_header("ACTION test", DIRECTION_NONE);
		add_trace("test", NULL, "callerid");
		end_trace();
		new_state(EPOINT_STATE_CONNECT);
		if (e_ext.number[0])
			e_dtmf = 1;
		memset(&e_connectinfo, 0, sizeof(e_connectinfo));
		SCPY(e_connectinfo.id, "12345678");
		SCPY(e_connectinfo.name, "Welcome to LCR");
		SCPY(e_connectinfo.display, "Welcome to LCR");
		e_connectinfo.ntype = INFO_NTYPE_UNKNOWN;
		e_connectinfo.present = INFO_PRESENT_ALLOWED;
		message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_CONNECT);
		memcpy(&message->param.connectinfo, &e_connectinfo, sizeof(message->param.connectinfo));
		message_put(message);
		logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
		set_tone(portlist, "hold");
		break;
	}
}


/* process init play
 */
void EndpointAppPBX::action_init_play(void)
{
	struct route_param *rparam;
	struct port_list *portlist = ea_endpoint->ep_portlist;

	/* check given sample */
	if (!(rparam = routeparam(e_action, PARAM_SAMPLE))) {
		trace_header("ACTION play (no sample given)", DIRECTION_NONE);
		end_trace();

		disconnect:
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		message_disconnect_port(portlist, CAUSE_UNSPECIFIED, LOCATION_PRIVATE_LOCAL, "", NULL);
		set_tone(portlist, "cause_3f");
		e_action = NULL;
		return;
	}

	/* if sample is given */
	if (rparam->string_value[0] == '\0') {
		trace_header("ACTION play (no sample given)", DIRECTION_NONE);
		end_trace();
		goto disconnect;
	}

	if (e_ext.number[0])
		e_dtmf = 1;

	set_tone(ea_endpoint->ep_portlist, rparam->string_value);
}


/*
 * action_*_vbox_play is implemented in "action_vbox.cpp"
 */


/*
 * process dialing of calculator
 */
void EndpointAppPBX::action_dialing_calculator(void)
{
	struct port_list *portlist = ea_endpoint->ep_portlist;
	struct lcr_msg *message;
	double value1, value2, v, sign1;
	int komma1, komma2, k, state, mode = 0, first;
	char *p;

	portlist = ea_endpoint->ep_portlist;

	/* remove error message */
	if (!strncmp(e_extdialing, "Error", 5)) {
		UCPY(e_extdialing, e_extdialing+5);
	}
	if (!strncmp(e_extdialing, "inf", 3)) {
		UCPY(e_extdialing, e_extdialing+3);
	}
	if (!strncmp(e_extdialing, "-inf", 4)) {
		UCPY(e_extdialing, e_extdialing+4);
	}

	/* process dialing */
	state = 0;
	value1 = 0;
	value2 = 0;
	komma1 = 0;
	komma2 = 0;
	sign1 = 1;
	p = e_extdialing;
	if (!p)
		return;
	first = 1;
	while(*p) {
		if (*p>='0' && *p<='9') {
#if 0
			if (first) {
				UCPY(p, p+1);
				continue;
			}
			if ((p[-1]<'0' || p[-1]>'0') && p[-1]!='.') {
				p--;
				UCPY(p, p+1);
				continue;
			}
#endif
			switch(state) {
				case 0: /* first number */
				if (!komma1) {
					value1 = value1*10 + (*p-'0');
				} else {
					k = komma1++;
					v = *p-'0';
					while(k--)
						v /= 10;
					value1 += v; 
				}
				break;
				case 1: /* second number */
				if (!komma2) {
					value2 = value2*10 + (*p-'0');
				} else {
					k = komma2++;
					v = *p-'0';
					while(k--)
						v /= 10;
					value2 += v; 
				}
				break;
			}
		} else
		switch(*p) {
			case '*':
			if (first) {
				UCPY(e_extdialing, "Error");
				goto done;
			}
			/* if there is a multiplication, we change to / */
			if (p[-1] == '*') {
				mode = 1;
				p[-1] = '/';
				UCPY(p, p+1);
				p--;
				break;
			}
			/* if there is a division, we change to + */
			if (p[-1] == '/') {
				mode = 2;
				p[-1] = '+';
				UCPY(p, p+1);
				p--;
				break;
			}
			/* if there is a addition, we change to - */
			if (p[-1] == '+') {
				mode = 3;
				p[-1] = '-';
				UCPY(p, p+1);
				p--;
				break;
			}
			/* if there is a substraction and a comma, we change to * */
			if (p[-1]=='-' && komma1) {
				mode = 0;
				p[-1] = '*';
				UCPY(p, p+1);
				p--;
				break;
			}
			/* if there is a substraction and no comma and the first or second value, we change to , */
			if (p[-1]=='-') {
				p[-1] = '.';
				UCPY(p, p+1);
				p--;
				komma1 = 1;
				break;
			}
			/* if there is a komma and we are at the first value, we change to * */
			if (p[-1]=='.' && state==0) {
				mode = 0;
				p[-1] = '*';
				UCPY(p, p+1);
				p--;
				komma1 = 0;
				break;
			}
			/* if there is a komma and we are at the second value, we display error */
			if (komma2 && state==1) {
				UCPY(e_extdialing, "Error");
				goto done;
			}
			/* if we are at state 1, we write a comma */
			if (state == 1) {
				*p = '.';
				komma2 = 1;
				break;
			}
			/* we assume multiplication */
			mode = 0;
			state = 1;
			komma1 = 0;
			break;

			case '#':
			/* if just a number is displayed, the input is cleared */
			if (state==0) {
				*e_extdialing = '\0';
				break;
			}
			/* calculate the result */
			switch(mode) {
				case 0: /* multiply */
				UNPRINT(e_extdialing, sizeof(e_dialinginfo.id)-strlen(e_dialinginfo.id), "%.8f", sign1*value1*value2);
				break;
				case 1: /* divide */
				UNPRINT(e_extdialing, sizeof(e_dialinginfo.id)-strlen(e_dialinginfo.id), "%.8f", sign1*value1/value2);
				break;
				case 2: /* add */
				UNPRINT(e_extdialing, sizeof(e_dialinginfo.id)-strlen(e_dialinginfo.id), "%.8f", sign1*value1+value2);
				break;
				case 3: /* substract */
				UNPRINT(e_extdialing, sizeof(e_dialinginfo.id)-strlen(e_dialinginfo.id), "%.8f", sign1*value1-value2);
				break;
			}
			e_dialinginfo.id[sizeof(e_dialinginfo.id)-1] = '\0';
			if (strchr(e_extdialing, '.')) { /* remove zeroes */
				while (e_extdialing[strlen(e_extdialing)-1] == '0')
					e_extdialing[strlen(e_extdialing)-1] = '\0';
				if (e_extdialing[strlen(e_extdialing)-1] == '.')
					e_extdialing[strlen(e_extdialing)-1] = '\0'; /* and remove dot */
			}
			p = strchr(e_extdialing,'\0')-1;
			break;

			case '.':
			if (state)
				komma2 = 1;
			else	komma1 = 1;
			break;

			case '/':
			komma2 = 0;
			state = 1;
			mode = 1;
			break;

			case '+':
			komma2 = 0;
			state = 1;
			mode = 2;
			break;

			case '-':
			if (first) {
				sign1=-1;
				break;
			}
			komma2 = 0;
			state = 1;
			mode = 3;
			break;

			default:
			UCPY(e_extdialing, "Error");
			goto done;
		}

		p++;
		first = 0;
	}
	done:

	/* display dialing */	
	message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_NOTIFY);
	SPRINT(message->param.notifyinfo.display, ">%s", e_extdialing);
	PDEBUG(DEBUG_EPOINT, "EPOINT(%d): terminal %s displaying interpreted dialing '%s' internal values: %f %f\n", ea_endpoint->ep_serial, e_ext.number, e_extdialing, value1, value2);
	message_put(message);
	logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);

}

/*
 * process dialing of timer
 */
void EndpointAppPBX::action_dialing_timer(void)
{
}


/*
 * process 'goto' or 'menu'
 */
void EndpointAppPBX::_action_goto_menu(int mode)
{
	struct port_list *portlist = ea_endpoint->ep_portlist;
	struct route_param *rparam;

	/* check given ruleset */
	if (!(rparam = routeparam(e_action, PARAM_RULESET))) {
		no_ruleset:
		trace_header("ACTION goto/menu (no ruleset given)", DIRECTION_NONE);
		end_trace();

		disconnect:
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		message_disconnect_port(portlist, CAUSE_SERVICEUNAVAIL, LOCATION_PRIVATE_LOCAL, "", NULL);
		set_tone(portlist, "cause_3f");
		e_action = NULL;
		return;
	}
	if (rparam->string_value[0] == '\0')
		goto no_ruleset;
	e_ruleset = getrulesetbyname(rparam->string_value);
	if (!e_ruleset) {
		trace_header("ACTION goto/menu (ruleset not found)", DIRECTION_NONE);
		add_trace("ruleset", NULL, "%s", rparam->string_value);
		end_trace();
		goto disconnect;
	}

	/* if the 'menu' was selected, we will flush all digits */
	if (mode) {
		e_dialinginfo.id[0] = 0;
		e_extdialing = e_dialinginfo.id;
	} else {
		/* remove digits that are required to match the rule */
		if ((rparam = routeparam(e_action, PARAM_STRIP))) {
			if (e_extdialing)
				SCPY(e_dialinginfo.id, e_extdialing);
			e_extdialing = e_dialinginfo.id;
		}
	}

	/* play sample */
	trace_header("ACTION goto/menu (change to)", DIRECTION_NONE);
	add_trace("ruleset", NULL, "%s", e_ruleset->name);
	if (e_dialinginfo.id[0]) {
		add_trace("dialing", NULL, "%s", e_dialinginfo.id);
	}
	if ((rparam = routeparam(e_action, PARAM_SAMPLE))) {
		add_trace("sample", NULL, "%s", rparam->string_value);
		end_trace();
		set_tone(ea_endpoint->ep_portlist, rparam->string_value);
	} else {
		end_trace();
	}

	/* do dialing with new ruleset */
	e_action = NULL;
	process_dialing(0);
}

/* process dialing goto
*/
void EndpointAppPBX::action_dialing_goto(void)
{
	_action_goto_menu(0);
}

/* process dialing menu
*/
void EndpointAppPBX::action_dialing_menu(void)
{
	_action_goto_menu(1);
}


/*
 * process dialing disconnect
 */
void EndpointAppPBX::action_dialing_disconnect(void)
{
	struct route_param *rparam;
	struct port_list *portlist = ea_endpoint->ep_portlist;
	struct lcr_msg *message;
	int cause = CAUSE_NORMAL; /* normal call clearing */
	int location = LOCATION_PRIVATE_LOCAL;
	char cause_string[256] = "", display[84] = "";

	/* check cause parameter */
	if ((rparam = routeparam(e_action, PARAM_CAUSE))) {
		cause = rparam->integer_value;
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d): 'cause' is given: %d\n", ea_endpoint->ep_serial, cause);
	}
	if ((rparam = routeparam(e_action, PARAM_LOCATION))) {
		location = rparam->integer_value;
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d): 'location' is given: %d\n", ea_endpoint->ep_serial, location);
	}


	/* use cause as sample, if not given later */
	SPRINT(cause_string, "cause_%02x", cause);

	/* check sample parameter */
	if ((rparam = routeparam(e_action, PARAM_SAMPLE))) {
		SCPY(cause_string, rparam->string_value);
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d): 'sample' is given: %s\n", ea_endpoint->ep_serial, cause_string);
	}

	/* check display */
	if ((rparam = routeparam(e_action, PARAM_DISPLAY))) {
		SCPY(display, rparam->string_value);
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d): 'display' is given: %s\n", ea_endpoint->ep_serial, display);
	}

	/* disconnect only if connect parameter is not given */
	trace_header("ACTION disconnect", DIRECTION_NONE);
	add_trace("cause", "value", "%d", cause);
	add_trace("cause", "location", "%d", location);
	if (cause_string[0])
		add_trace("sample", NULL, "%s", cause_string);
	if (display[0])
		add_trace("display", NULL, "%s", display);
	end_trace();
	new_state(EPOINT_STATE_OUT_DISCONNECT);
	set_tone(portlist, cause_string);
	if (!(rparam = routeparam(e_action, PARAM_CONNECT))) {
		message_disconnect_port(portlist, cause, location, display, NULL);
	} else {
		message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_portlist->port_id, EPOINT_TO_PORT, MESSAGE_NOTIFY);
		SCPY(message->param.notifyinfo.display, display);
		message_put(message);
		logmessage(message->type, &message->param, ea_endpoint->ep_portlist->port_id, DIRECTION_OUT);
	}
	e_action = NULL;
}


/*
 * process dialing release
 */
void EndpointAppPBX::action_dialing_release(void)
{
	struct route_param *rparam;
	int cause = CAUSE_NORMAL; /* normal call clearing */
	int location = LOCATION_PRIVATE_LOCAL;
	char cause_string[256] = "", display[84] = "";

	/* check cause parameter */
	if ((rparam = routeparam(e_action, PARAM_CAUSE))) {
		cause = rparam->integer_value;
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d): 'cause' is given: %d\n", ea_endpoint->ep_serial, cause);
	}
	if ((rparam = routeparam(e_action, PARAM_LOCATION))) {
		location = rparam->integer_value;
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d): 'location' is given: %d\n", ea_endpoint->ep_serial, location);
	}


	/* use cause as sample, if not given later */
	SPRINT(cause_string, "cause_%02x", cause);

	/* check display */
	if ((rparam = routeparam(e_action, PARAM_DISPLAY))) {
		SCPY(display, rparam->string_value);
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d): 'display' is given: %s\n", ea_endpoint->ep_serial, display);
	}

	/* disconnect only if connect parameter is not given */
	trace_header("ACTION release", DIRECTION_NONE);
	add_trace("cause", "value", "%d", cause);
	add_trace("cause", "location", "%d", location);
	if (display[0])
		add_trace("display", NULL, "%s", display);
	end_trace();
	e_action = NULL;
	release(RELEASE_ALL, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, location, cause, 1);
	return;
}

/*
 * process dialing help
 */
void EndpointAppPBX::action_dialing_help(void)
{
	/* show all things that would match */
#if 0
	struct numbering *numbering = numbering_int;
	char dialing[sizeof(e_dialinginfo.id)];
	int i;
	struct lcr_msg *message;
	struct route_param *rparam;

	/* in case we have no menu (this should never happen) */
	if (!numbering)
		return;

	/* scroll menu */
	if (strchr(e_dialinginfo.id,'*')) {
		e_menu--;
		e_dialinginfo.id[0] = '\0';
	}
	if (strchr(e_dialinginfo.id,'#')) {
		e_menu++;
		e_dialinginfo.id[0] = '\0';
	}
	
	/* get position in menu */
	if (e_menu < 0) {
		/* get last menu position */
		e_menu = 0;
		while(numbering->next) {
			e_menu++;
			numbering = numbering->next;
		}
	} else {
		/* get menu position */
		i = 0;
		while(i < e_menu) {
			numbering = numbering->next;
			if (!numbering) {
				e_menu = 0;
				numbering = numbering_int;
				break;
			}
			i++;
		}
	}

	/* if we dial something else we need to add the prefix and change the action */
	if (e_dialinginfo.id[0]) {
		e_action = NUMB_ACTION_NONE;
		SCPY(dialing, numbering->prefix);
		//we ignore the first digit after selecting
		//SCAT(dialing, e_dialinginfo.id);
		SCPY(e_dialinginfo.id, dialing);
		e_extdialing = e_dialinginfo.id+strlen(numbering->prefix);
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d): terminal %s selected a new menu '%s' dialing: %s\n", ea_endpoint->ep_serial, e_ext.number, numb_actions[numbering->action], e_dialinginfo.id);
nesting?:
		process_dialing(0);
		return;
	}

	/* send display message to port */
	message = message_create(ea_endpoint->ep_serial, ea_endpoint->ep_portlist->port_id, EPOINT_TO_PORT, MESSAGE_NOTIFY);
	SPRINT(message->param.notifyinfo.display, ">%s %s%s%s", numbering->prefix, numb_actions[numbering->action], (numbering->param[0])?" ":"", numbering->param);
	PDEBUG(DEBUG_EPOINT, "EPOINT(%d): terminal %s selected a new menu '%s' sending display:%s\n", ea_endpoint->ep_serial, e_ext.number, numb_actions[numbering->action], message->param.notifyinfo.display);
	message_put(message);
	logmessage(message->type, message->paramea_endpoint->ep_portlist->port_id, DIRECTION_OUT);
#endif
}


/*
 * process dialing deflect
 */
void EndpointAppPBX::action_dialing_deflect(void)
{
}


/*
 * process dialing setforward
 */
void EndpointAppPBX::action_dialing_setforward(void)
{
}

/*
 * process init 'execute'
 */ 
void EndpointAppPBX::action_init_execute(void)
{
	struct route_param *rparam;
	int executeon = INFO_ON_HANGUP;  /* Use Hangup as a default for compatibility */
	
	/* Get the execute on parameter */
	if ((rparam = routeparam(e_action, PARAM_ON)))
	executeon = rparam->integer_value;

	/* Execute this action if init was specified */
	if (executeon == INFO_ON_INIT) {
		trace_header("ACTION execute ON init", DIRECTION_NONE);
		end_trace();
		action_execute();
	}
}

/*
 * process hangup 'execute'
 */ 
void EndpointAppPBX::action_hangup_execute(void)
{
	struct route_param *rparam;
	int executeon = INFO_ON_HANGUP;  /* Use Hangup as a default for compatibility */
	
	/* Get the execute on parameter */
	if ((rparam = routeparam(e_action, PARAM_ON)))
	executeon = rparam->integer_value;

	/* Execute this action if init was specified */
	if (executeon == INFO_ON_HANGUP) {
		trace_header("ACTION execute ON hangup", DIRECTION_NONE);
		end_trace();
		action_execute();
	}
}

/*
 * process 'execute' from action_init_execute or action_hangup_execute
 */
void EndpointAppPBX::action_execute(void)
{
	struct route_param *rparam;
	pid_t pid;
	pid_t pid2;
	int iWaitStatus;
	char *command = (char *)"";
	char isdn_port[10];
	char *argv[12]; /* check also number of args below */
	int i = 0;

	/* get script / command */
	if ((rparam = routeparam(e_action, PARAM_EXECUTE)))
		command = rparam->string_value;
	if (command[0] == '\0') {
		trace_header("ACTION execute (no parameter given)", DIRECTION_NONE);
		end_trace();
		return;
	}
#if 0
	argv[i++] = (char *)"/bin/sh";
	argv[i++] = (char *)"-c";
	argv[i++] = command;
#endif
	argv[i++] = command;
	if ((rparam = routeparam(e_action, PARAM_PARAM))) {
		argv[i++] = rparam->string_value;
	}
	argv[i++] = e_extdialing;
	argv[i++] = (char *)numberrize_callerinfo(e_callerinfo.id, e_callerinfo.ntype, options.national, options.international);
	argv[i++] = e_callerinfo.extension;
	argv[i++] = e_callerinfo.name;
	SPRINT(isdn_port, "%d", e_callerinfo.isdn_port);
	argv[i++] = isdn_port;
	argv[i++] = e_callerinfo.imsi;
	argv[i++] = NULL; /* check also number of args above */
	switch (pid = fork ()) {
		case -1:
			trace_header("ACTION execute (fork failed)", DIRECTION_NONE);
			end_trace();
			break;
		case 0:
			/* To be shure there are no zombies created double fork */
			if ((pid2 = fork()) == 0) {
				execve(command, argv, environ);
			}
			/* Exit immediately and release the waiting parent. The subprocess falls to init because the parent died */
			exit(0);
			break;
		default:
			trace_header("ACTION execute", DIRECTION_NONE);
			add_trace("command", NULL, "%s", command);
			end_trace();

			/* Wait for the pid. The forked process will exit immediately so there is no problem waiting. */
			waitpid(pid, &iWaitStatus, 0);
			break;
	}
}


/*
 * process hangup 'file'
 */
void EndpointAppPBX::action_hangup_file(void)
{
	struct route_param *rparam;
	const char *file, *content, *mode;
	FILE *fp;

	/* get file / content */
	if (!(rparam = routeparam(e_action, PARAM_FILE)))
		file = rparam->string_value;
	else
		file = "";
	if (!(rparam = routeparam(e_action, PARAM_CONTENT)))
		content = rparam->string_value;
	else
		content = e_extdialing;
	if (!(rparam = routeparam(e_action, PARAM_APPEND)))
		mode = "a";
	else
		mode = "w";
	if (file[0] == '\0') {
		trace_header("ACTION file (no filename given)", DIRECTION_NONE);
		end_trace();
		return;
	}
	if (!(fp = fopen(file, mode))) {
		trace_header("ACTION file (failed to open)", DIRECTION_NONE);
		add_trace("file", "name", "%s", file);
		add_trace("file", "mode", "%s", (mode[0]=='w')?"write":"append");
		end_trace();
		return;
	}
	trace_header("ACTION file", DIRECTION_NONE);
	add_trace("file", "name", "%s", file);
	add_trace("file", "mode", "%s", (mode[0]=='w')?"write":"append");
	add_trace("content", NULL, "%s", content);
	end_trace();
	fprintf(fp, "%s\n", content);
	fclose(fp);
}


/*
 * process init 'pick'
 */
void EndpointAppPBX::action_init_pick(void)
{
	struct route_param *rparam;
	char *extensions = NULL;

	if ((rparam = routeparam(e_action, PARAM_EXTENSIONS)))
		extensions = rparam->string_value;
	
	trace_header("ACTION pick", DIRECTION_NONE);
	if (extensions) if (extensions[0])
		add_trace("extensions", NULL, "%s", extensions);
	end_trace();
	pick_join(extensions);
}


/*
 * process dialing 'password'
 */
void EndpointAppPBX::action_dialing_password(void)
{
	struct port_list *portlist = ea_endpoint->ep_portlist;

	/* prompt for password */
	if (e_extdialing[0] == '\0') {
		/* give password tone */
		set_tone(portlist, "password");
	} else // ELSE!!
	if (e_extdialing[1] == '\0') {
		/* give password tone */
		set_tone(portlist, "dialing");
	}

	/* wait until all digits are dialed */
	if (strlen(e_ext.password) != strlen(e_extdialing))
		return; /* more digits needed */

	/* check the password */
	if (e_ext.password[0]=='\0' || (strlen(e_ext.password)==strlen(e_extdialing) && !!strcmp(e_ext.password,e_extdialing))) {
		trace_header("ACTION password_write (wrong password)", DIRECTION_NONE);
		add_trace("dialed", NULL, "%s", e_extdialing);
		end_trace();
		e_connectedmode = 0;
		e_dtmf = 0;
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		message_disconnect_port(portlist, CAUSE_NORMAL, LOCATION_PRIVATE_LOCAL, "", NULL);
		set_tone(portlist, "cause_10");
		return;
	}

	/* write caller id if ACTION_PASSWORD_WRITE was selected */
	if (e_action)
	if (e_action->index == ACTION_PASSWORD_WRITE) {
		append_callbackauth(e_ext.number, &e_callbackinfo);
		trace_header("ACTION password_write (written)", DIRECTION_NONE);
		add_trace("dialed", NULL, "%s", e_extdialing);
		end_trace();
	}

	/* make call state  */
	new_state(EPOINT_STATE_IN_OVERLAP);
	e_ruleset = ruleset_main;
	if (e_ruleset)
		e_rule = e_ruleset->rule_first;
	e_action = NULL;
	e_dialinginfo.id[0] = '\0';
	e_extdialing = e_dialinginfo.id;
	set_tone(portlist, "dialpbx");
}

void EndpointAppPBX::action_dialing_password_wr(void)
{
	action_dialing_password();
}


/* process pots-retrieve
 */
void EndpointAppPBX::action_init_pots_retrieve(void)
{
#ifdef ISDN_P_FXS_POTS
	struct route_param *rparam;
	struct port_list *portlist = ea_endpoint->ep_portlist;
	class Port *port;
	class Pfxs *ourfxs, *fxs;
	int count = 0;
	class Endpoint *epoint;

	/* check given call */
	if (!(rparam = routeparam(e_action, PARAM_POTS_CALL))) {
		trace_header("ACTION pots-retrieve (no call given)", DIRECTION_NONE);
		end_trace();

		disconnect:
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		message_disconnect_port(portlist, CAUSE_UNSPECIFIED, LOCATION_PRIVATE_LOCAL, "", NULL);
		set_tone(portlist, "cause_3f");
		e_action = NULL;
		return;
	}

	/* find call */
	port = find_port_id(portlist->port_id);
	if (!port)
		goto disconnect;
	if ((port->p_type & PORT_CLASS_POTS_MASK) != PORT_CLASS_POTS_FXS) {
		trace_header("ACTION pots-retrieve (call not of FXS type)", DIRECTION_NONE);
		end_trace();
		goto disconnect;
	}
	ourfxs = (class Pfxs *)port;

	port = port_first;
	while(port) {
		if ((port->p_type & PORT_CLASS_POTS_MASK) == PORT_CLASS_POTS_FXS) {
			fxs = (class Pfxs *)port;
			if (fxs->p_m_mISDNport == ourfxs->p_m_mISDNport && fxs != ourfxs) {
				count++;
				if (count == rparam->integer_value)
					break;
			}
		}
		port = port->next;
	}
	if (!port) {
		trace_header("ACTION pots-retrieve (call # does not exist)", DIRECTION_NONE);
		end_trace();
		goto disconnect;
	}

	/* release our call */
	ourfxs->hangup_ind(0);

	/* retrieve selected call */
	fxs->retrieve_ind(0);

	/* split if selected call is member of a 3pty */
	epoint = find_epoint_id(ACTIVE_EPOINT(fxs->p_epointlist));
	if (epoint && epoint->ep_app_type == EAPP_TYPE_PBX) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d) try spliting 3pty. this may fail because we don't have a 3pty.\n", epoint->ep_serial);
		((class EndpointAppPBX *)epoint->ep_app)->split_3pty();
	}
#endif
}


/* process pots-release
 */
void EndpointAppPBX::action_init_pots_release(void)
{
#ifdef ISDN_P_FXS_POTS
	struct route_param *rparam;
	struct port_list *portlist = ea_endpoint->ep_portlist;
	class Port *port;
	class Pfxs *ourfxs, *fxs;
	int count = 0;

	/* check given call */
	if (!(rparam = routeparam(e_action, PARAM_POTS_CALL))) {
		trace_header("ACTION pots-release (no call given)", DIRECTION_NONE);
		end_trace();

		disconnect:
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		message_disconnect_port(portlist, CAUSE_UNSPECIFIED, LOCATION_PRIVATE_LOCAL, "", NULL);
		set_tone(portlist, "cause_3f");
		e_action = NULL;
		return;
	}

	/* find call */
	port = find_port_id(portlist->port_id);
	if (!port)
		goto disconnect;
	if ((port->p_type & PORT_CLASS_POTS_MASK) != PORT_CLASS_POTS_FXS) {
		trace_header("ACTION pots-release (call not of FXS type)", DIRECTION_NONE);
		end_trace();
		goto disconnect;
	}
	ourfxs = (class Pfxs *)port;

	port = port_first;
	while(port) {
		if ((port->p_type & PORT_CLASS_POTS_MASK) == PORT_CLASS_POTS_FXS) {
			fxs = (class Pfxs *)port;
			if (fxs->p_m_mISDNport == ourfxs->p_m_mISDNport && fxs != ourfxs) {
				count++;
				if (count == rparam->integer_value)
					break;
			}
		}
		port = port->next;
	}
	if (!port) {
		trace_header("ACTION pots-release (call # does not exist)", DIRECTION_NONE);
		end_trace();
		goto disconnect;
	}

#if 0
	/* disconnect our call */
	new_state(EPOINT_STATE_OUT_DISCONNECT);
	message_disconnect_port(portlist, CAUSE_NORMAL, LOCATION_PRIVATE_LOCAL, "", NULL);
	set_tone(portlist, "hangup");
	e_action = NULL;
#endif

	/* release selected call */
	fxs->hangup_ind(0);

	/* indicate timeout, so next action will be processed */
	process_dialing(1);
#endif
}


/* process pots-reject
 */
void EndpointAppPBX::action_init_pots_reject(void)
{
#ifdef ISDN_P_FXS_POTS
	struct port_list *portlist = ea_endpoint->ep_portlist;
	class Port *port;
	class Pfxs *ourfxs, *fxs;

	/* find call */
	port = find_port_id(portlist->port_id);
	if (!port)
		goto disconnect;
	if ((port->p_type & PORT_CLASS_POTS_MASK) != PORT_CLASS_POTS_FXS) {
		trace_header("ACTION pots-reject (call not of FXS type)", DIRECTION_NONE);
		end_trace();
		disconnect:
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		message_disconnect_port(portlist, CAUSE_UNSPECIFIED, LOCATION_PRIVATE_LOCAL, "", NULL);
		set_tone(portlist, "cause_3f");
		e_action = NULL;
		return;
	}
	ourfxs = (class Pfxs *)port;

	port = port_first;
	while(port) {
		if ((port->p_type & PORT_CLASS_POTS_MASK) == PORT_CLASS_POTS_FXS) {
			fxs = (class Pfxs *)port;
			if (fxs->p_m_mISDNport == ourfxs->p_m_mISDNport && fxs != ourfxs) {
				if (fxs->p_state == PORT_STATE_OUT_ALERTING)
					break;
			}
		}
		port = port->next;
	}
	if (!port) {
		trace_header("ACTION pots-reject (no call waiting)", DIRECTION_NONE);
		end_trace();
		goto disconnect;
	}

	/* reject alerting call */
	fxs->reject_ind(0);

	/* indicate timeout, so next action will be processed */
	process_dialing(1);
#endif
}


/* process pots-answer
 */
void EndpointAppPBX::action_init_pots_answer(void)
{
#ifdef ISDN_P_FXS_POTS
	struct port_list *portlist = ea_endpoint->ep_portlist;
	class Port *port;
	class Pfxs *ourfxs, *fxs;

	/* find call */
	port = find_port_id(portlist->port_id);
	if (!port)
		goto disconnect;
	if ((port->p_type & PORT_CLASS_POTS_MASK) != PORT_CLASS_POTS_FXS) {
		trace_header("ACTION pots-answer (call not of FXS type)", DIRECTION_NONE);
		end_trace();
		disconnect:
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		message_disconnect_port(portlist, CAUSE_UNSPECIFIED, LOCATION_PRIVATE_LOCAL, "", NULL);
		set_tone(portlist, "cause_3f");
		e_action = NULL;
		return;
	}
	ourfxs = (class Pfxs *)port;

	port = port_first;
	while(port) {
		if ((port->p_type & PORT_CLASS_POTS_MASK) == PORT_CLASS_POTS_FXS) {
			fxs = (class Pfxs *)port;
			if (fxs->p_m_mISDNport == ourfxs->p_m_mISDNport && fxs != ourfxs) {
				if (fxs->p_state == PORT_STATE_OUT_ALERTING)
					break;
			}
		}
		port = port->next;
	}
	if (!port) {
		trace_header("ACTION pots-answer (no call waiting)", DIRECTION_NONE);
		end_trace();
		goto disconnect;
	}

	/* release our call */
	ourfxs->hangup_ind(0);

	/* answer alerting call */
	fxs->answer_ind(0);
#endif
}


/* process pots-3pty
 */
void EndpointAppPBX::action_init_pots_3pty(void)
{
#ifdef ISDN_P_FXS_POTS
	struct port_list *portlist = ea_endpoint->ep_portlist;
	class Port *port;
	class Pfxs *ourfxs, *fxs, *fxs1 = NULL, *fxs2 = NULL;
	class Endpoint *epoint;
	int count = 0;

	/* find call */
	port = find_port_id(portlist->port_id);
	if (!port)
		goto disconnect;
	if ((port->p_type & PORT_CLASS_POTS_MASK) != PORT_CLASS_POTS_FXS) {
		trace_header("ACTION pots-3pty (call not of FXS type)", DIRECTION_NONE);
		end_trace();
		disconnect:
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		message_disconnect_port(portlist, CAUSE_UNSPECIFIED, LOCATION_PRIVATE_LOCAL, "", NULL);
		set_tone(portlist, "cause_3f");
		e_action = NULL;
		return;
	}
	ourfxs = (class Pfxs *)port;

	port = port_first;
	while(port) {
		if ((port->p_type & PORT_CLASS_POTS_MASK) == PORT_CLASS_POTS_FXS) {
			fxs = (class Pfxs *)port;
			if (fxs->p_m_mISDNport == ourfxs->p_m_mISDNport && fxs != ourfxs) {
				if (count == 0)
					fxs1 = fxs;
				if (count == 1)
					fxs2 = fxs;
				count++;
			}
		}
		port = port->next;
	}
	if (count != 2) {
		trace_header("ACTION pots-3pty (exactly two calls don't exist)", DIRECTION_NONE);
		end_trace();
		goto disconnect;
	}

	/* release our call */
	ourfxs->hangup_ind(0);

	/* retrieve latest active call */
	if (fxs2->p_m_fxs_age > fxs1->p_m_fxs_age) {
		fxs2->retrieve_ind(0);
		epoint = find_epoint_id(ACTIVE_EPOINT(fxs2->p_epointlist));
	} else {
		fxs1->retrieve_ind(0);
		epoint = find_epoint_id(ACTIVE_EPOINT(fxs2->p_epointlist));
	}

	if (!epoint) {
		trace_header("ACTION pots-3pty (interal error: no endpoint)", DIRECTION_NONE);
		end_trace();
		return;
	}

	if (epoint->ep_app_type != EAPP_TYPE_PBX) {
		trace_header("ACTION pots-3pty (interal error: endpoint not PBX type)", DIRECTION_NONE);
		end_trace();
		return;
	}

	/* bridge calls */
	if (((class EndpointAppPBX *)epoint->ep_app)->join_3pty_fxs()) {
		trace_header("ACTION pots-3pty (interal error: join_3pty_fsx failed)", DIRECTION_NONE);
		end_trace();
		return;
	}
#endif
}

/* process pots-transfer
 */
void EndpointAppPBX::action_init_pots_transfer(void)
{
#ifdef ISDN_P_FXS_POTS
	struct route_param *rparam;
	struct port_list *portlist = ea_endpoint->ep_portlist;
	class Port *port;
	class Pfxs *ourfxs, *fxs, *fxs1 = NULL, *fxs2 = NULL;
	int count = 0;

	/* check given call */
	if (!(rparam = routeparam(e_action, PARAM_POTS_CALL))) {
		trace_header("ACTION pots-transfer (no call given)", DIRECTION_NONE);
		end_trace();

		disconnect:
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		message_disconnect_port(portlist, CAUSE_UNSPECIFIED, LOCATION_PRIVATE_LOCAL, "", NULL);
		set_tone(portlist, "cause_3f");
		e_action = NULL;
		return;
	}

	/* find call */
	port = find_port_id(portlist->port_id);
	if (!port)
		goto disconnect;
	if ((port->p_type & PORT_CLASS_POTS_MASK) != PORT_CLASS_POTS_FXS) {
		trace_header("ACTION pots-transfer (call not of FXS type)", DIRECTION_NONE);
		end_trace();
		goto disconnect;
	}
	ourfxs = (class Pfxs *)port;

	port = port_first;
	while(port) {
		if ((port->p_type & PORT_CLASS_POTS_MASK) == PORT_CLASS_POTS_FXS) {
			fxs = (class Pfxs *)port;
			if (fxs->p_m_mISDNport == ourfxs->p_m_mISDNport && fxs != ourfxs) {
				if (count == 0)
					fxs1 = fxs;
				if (count == 1)
					fxs2 = fxs;
				count++;
			}
		}
		port = port->next;
	}
	if (count != 2) {
		trace_header("ACTION pots-transfer (exactly two calls don't exist)", DIRECTION_NONE);
		end_trace();
		goto disconnect;
	}

	/* retrieve call */
	if (fxs2->p_m_fxs_age > fxs1->p_m_fxs_age)
		fxs2->retrieve_ind(0);
	else
		fxs1->retrieve_ind(0);
	/* bridge calls */
	join_join_fxs();
#endif
}


/* general process dialing of incoming call
 * depending on the detected prefix, subfunctions above (action_*) will be
 * calles.
 */
void EndpointAppPBX::process_dialing(int timeout)
{
	struct port_list *portlist = ea_endpoint->ep_portlist;
	struct lcr_msg *message;
	struct route_param *rparam;
	struct timeval current_time;

	/* set if timeout is active, or if timeout value was given due to timeout action */
	if (e_action_timeout.active)
		timeout = 1;

//#warning Due to HANG-BUG somewhere here, I added some HANG-BUG-DEBUGGING output that cannot be disabled. after bug has been found, this will be removed.
//PDEBUG(~0, "HANG-BUG-DEBUGGING: entered porcess_dialing\n");
	portlist = ea_endpoint->ep_portlist;
	/* check if we have a port instance linked to our epoint */
	if (!portlist) {
		portlist_error:
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d): note: dialing call requires exactly one port object to process dialing. this case could happen due to a parked call. we end dialing here.\n", ea_endpoint->ep_serial, e_ext.number);
		unsched_timer(&e_action_timeout);
		unsched_timer(&e_match_timeout);
		return;
	}
	if (portlist->next) {
		goto portlist_error;
	}

	/* check nesting levels */
	if (++e_rule_nesting > RULE_NESTING) {
		trace_header("ACTION (nesting too deep)", DIRECTION_NONE);
		add_trace("max-levels", NULL, "%d", RULE_NESTING);
		end_trace();
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		message_disconnect_port(portlist, CAUSE_UNSPECIFIED, LOCATION_PRIVATE_LOCAL, "", NULL);
		set_tone(portlist, "cause_3f");
		unsched_timer(&e_action_timeout);
		unsched_timer(&e_match_timeout);
		goto end;
	}

//PDEBUG(~0, "HANG-BUG-DEBUGGING: before action-timeout processing\n");
	/* process timeout */
	if (e_action && timeout) { /* e_action may be NULL, but e_action_timeout may still be set and must be ignored */
		unsched_timer(&e_action_timeout);
		if (e_state == EPOINT_STATE_CONNECT) {
			PDEBUG(DEBUG_ROUTE|DEBUG_EPOINT, "EPOINT(%d): action timed out, but we already have connected, so we stop timer and continue.\n", ea_endpoint->ep_serial);
			goto end;
		}
		if (e_action->index == ACTION_DISCONNECT
		 || e_state == EPOINT_STATE_OUT_DISCONNECT) {
			/* release after disconnect */
			release(RELEASE_ALL, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, 0);
			goto end;
		}
		release(RELEASE_JOIN, LOCATION_PRIVATE_LOCAL, CAUSE_NORMAL, 0, 0, 0);
		e_action = e_action->next;
		if (!e_action) {
			/* nothing more, so we release */
			PDEBUG(DEBUG_ROUTE|DEBUG_EPOINT, "EPOINT(%d): action timed out, and we have no next action, so we disconnect.\n", ea_endpoint->ep_serial);
			new_state(EPOINT_STATE_OUT_DISCONNECT);
			message_disconnect_port(portlist, CAUSE_NORMAL, LOCATION_PRIVATE_LOCAL, "", NULL);
			set_tone(portlist, "cause_3f");
			goto end;
		}
		goto action_timeout;
	}

//PDEBUG(~0, "HANG-BUG-DEBUGGING: before setup/overlap state checking\n");
	if (e_state!=EPOINT_STATE_IN_SETUP
	 && e_state!=EPOINT_STATE_IN_OVERLAP) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d): we are not in incoming setup/overlap state, so we ignore init/dialing process.\n", ea_endpoint->ep_serial, e_rule_nesting);
		unsched_timer(&e_match_timeout);
		goto end;
	}

#if 0
	/* check if we do menu selection */
	if (e_action==NUMB_ACTION_NONE && (e_dialinginfo.id[0]=='*' || e_dialinginfo.id[0]=='#'))
	/* do menu selection */
	if (e_ext.display_menu) {
		if (portlist->port_type==PORT_TYPE_DSS1_NT_IN || portlist->port_type==PORT_TYPE_DSS1_NT_OUT) { /* only if the dialing terminal is an isdn telephone connected to an internal port */
			e_dialinginfo.id[0] = '\0';
			e_action = NUMB_ACTION_MENU;
			e_menu = 0;
			process_dialing(0);
			unsched_timer(&e_match_timeout);
			goto end;
		}
		/* invalid dialing */
		message_disconnect_port(portlist, CAUSE_INCALID, LOCATION_PRIVATE_LOCAL, "", NULL);
			message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_DISCONNECT);
			message->param.disconnectinfo.cause = CAUSE_INVALID;
			message->param.disconnectinfo.location = LOCATION_PRIVATE_LOCAL;
				} else {
					message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_NOTIFY);
					SCPY(message->param.notifyinfo.display,get_isdn_cause(LOCATION_PRIVATE_LOCAL, epoint->e_ext.display_cause, param->disconnectinfo.location, param->disconnectinfo.cause));
				}
			message_put(message);
			logmessage(message->type, message->param, portlist->port_id, DIRECTION_OUT);
		}
		new_state(EPOINT_STATE_OUT_DISCONNECT);
		set_tone(portlist,"cause_1c");
		unsched_timer(&e_match_timeout);
		goto end;
	}
#endif

//PDEBUG(~0, "HANG-BUG-DEBUGGING: before e_action==NULL\n");
	/* if no action yet, we will call try to find a matching rule */
	if (!e_action) {
		/* be sure that all selectors are initialized */
		e_select = 0;

		/* check for external call */
		if (!strncmp(e_dialinginfo.id, "extern:", 7)) {
			e_extdialing = e_dialinginfo.id+7;
			e_action = &action_external;
			goto process_action;
		}
		/* check for internal call */
		if (!strncmp(e_dialinginfo.id, "intern:", 7)) {
			e_extdialing = e_dialinginfo.id+7;
			e_action = &action_internal;
			goto process_action;
		}
		/* check for vbox call */
		if (!strncmp(e_dialinginfo.id, "vbox:", 5)) {
			e_extdialing = e_dialinginfo.id+5;
			e_action = &action_vbox;
			goto process_action;
		}

		gettimeofday(&current_time, NULL);
		if (e_match_to_action && TIME_SMALLER(&e_match_timeout.timeout, &current_time)) {
			/* return timeout rule */
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d): terminal '%s' dialing: '%s', timeout in ruleset '%s'\n", ea_endpoint->ep_serial, e_ext.number, e_dialinginfo.id, e_ruleset->name);
			unsched_timer(&e_match_timeout);
			e_action = e_match_to_action;
			e_match_to_action = NULL;
			e_extdialing = e_match_to_extdialing;
			trace_header("ROUTING (timeout)", DIRECTION_NONE);
			add_trace("action", NULL, "%s", action_defs[e_action->index].name);
			add_trace("line", NULL, "%d", e_action->line);
			end_trace();
		} else {
//PDEBUG(~0, "HANG-BUG-DEBUGGING: before routing\n");
			/* check for matching rule */
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d): terminal '%s' dialing: '%s', checking matching rule of ruleset '%s'\n", ea_endpoint->ep_serial, e_ext.number, e_dialinginfo.id, e_ruleset->name);
			if (e_ruleset) {
				e_action = route(e_ruleset);
				if (e_action) {
					trace_header("ACTION (match)", DIRECTION_NONE);
					add_trace("action", NULL, "%s", action_defs[e_action->index].name);
					add_trace("line", NULL, "%d", e_action->line);
					end_trace();
				}
			} else {
				e_action = &action_disconnect;
				if (e_action) {
					trace_header("ACTION (no main ruleset, disconnecting)", DIRECTION_NONE);
					end_trace();
				}
			}
//PDEBUG(~0, "HANG-BUG-DEBUGGING: after routing\n");
		}
		if (!e_action) {
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d): no rule within the current ruleset matches yet.\n", ea_endpoint->ep_serial, e_ext.number);
			goto display;
		}

		/* matching */
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d): a rule with action '%s' matches.\n", ea_endpoint->ep_serial, action_defs[e_action->index].name);

		action_timeout:

		/* set timeout */
		unsched_timer(&e_action_timeout);
		if (e_action->timeout) {
			schedule_timer(&e_action_timeout, e_action->timeout, 0);
			PDEBUG(DEBUG_ROUTE|DEBUG_EPOINT, "EPOINT(%d): action has a timeout of %d secods.\n", ea_endpoint->ep_serial, e_action->timeout);
		}

		process_action:
		/* check param proceeding / alerting / connect */
		if ((rparam = routeparam(e_action, PARAM_CONNECT))) {
			/* NOTE: we may not change our state to connect, because dialing will then not possible */
			e_dtmf = 1;
			memset(&e_connectinfo, 0, sizeof(e_connectinfo));
			message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_CONNECT);
			message_put(message);
			logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
		} else
		if ((rparam = routeparam(e_action, PARAM_ALERTING))) {
			/* NOTE: we may not change our state to alerting, because dialing will then not possible */
			memset(&e_connectinfo, 0, sizeof(e_connectinfo));
			message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_ALERTING);
			message_put(message);
			logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
		} else
		if ((rparam = routeparam(e_action, PARAM_PROCEEDING))) {
			/* NOTE: we may not change our state to proceeding, because dialing will then not possible */
			memset(&e_connectinfo, 0, sizeof(e_connectinfo));
			message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_PROCEEDING);
			message_put(message);
			logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
		}

		if (action_defs[e_action->index].init_func) {
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d): terminal %s: current action '%s' has a init function, so we call it...\n", ea_endpoint->ep_serial, e_ext.number, action_defs[e_action->index].name);
			(this->*(action_defs[e_action->index].init_func))();
		}
		if (e_state!=EPOINT_STATE_IN_SETUP
		 && e_state!=EPOINT_STATE_IN_OVERLAP) {
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d): AFTER init process: we are not in incoming setup/overlap state anymore, so we ignore further dialing process.\n", ea_endpoint->ep_serial, e_rule_nesting);
			goto display_action;
		}
	}

	/* show what we are doing */
	PDEBUG(DEBUG_EPOINT, "EPOINT(%d): terminal '%s' action: %s (dialing '%s')\n", ea_endpoint->ep_serial, e_ext.number, action_defs[e_action->index].name, e_extdialing);
	/* go to action's dialing function */
	if (action_defs[e_action->index].dialing_func) {
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d): terminal %s: current action '%s' has a dialing function, so we call it...\n", ea_endpoint->ep_serial, e_ext.number, action_defs[e_action->index].name);
		(this->*(action_defs[e_action->index].dialing_func))();
	}

	/* display selected dialing action if enabled and still in setup state */
	display_action:
	if (e_action) {
		if (e_action->index==ACTION_MENU
		 || e_action->index==ACTION_REDIAL
		 || e_action->index==ACTION_REPLY
		 || e_action->index==ACTION_TIMER
		 || e_action->index==ACTION_CALCULATOR
		 || e_action->index==ACTION_TEST)
			goto end;
	}
	display:
	if (!e_ext.display_dialing)
		goto end;
	if (e_state==EPOINT_STATE_IN_OVERLAP || e_state==EPOINT_STATE_IN_PROCEEDING || e_state==EPOINT_STATE_IN_ALERTING || e_state==EPOINT_STATE_CONNECT/* || e_state==EPOINT_STATE_IN_DISCONNECT || e_state==EPOINT_STATE_OUT_DISCONNECT*/)
	if (portlist->port_type==PORT_TYPE_DSS1_NT_IN || portlist->port_type==PORT_TYPE_DSS1_NT_OUT) { /* only if the dialing terminal is an isdn telephone connected to an internal port */
		message = message_create(ea_endpoint->ep_serial, portlist->port_id, EPOINT_TO_PORT, MESSAGE_NOTIFY);

		if (!e_action) {
			SPRINT(message->param.notifyinfo.display, "> %s", e_dialinginfo.id);
		} else {
			SPRINT(message->param.notifyinfo.display, "%s%s%s", action_defs[e_action->index].name, (e_extdialing[0])?" ":"", e_extdialing);
		}

		PDEBUG(DEBUG_EPOINT, "EPOINT(%d): terminal %s displaying interpreted dialing '%s'\n", ea_endpoint->ep_serial, e_ext.number, message->param.notifyinfo.display);
		message_put(message);
		logmessage(message->type, &message->param, portlist->port_id, DIRECTION_OUT);
	}

end:
	e_rule_nesting--;
	return;
}


/* some services store information after hangup */
void EndpointAppPBX::process_hangup(int cause, int location)
{
	char callertext[256], dialingtext[256];
	int writeext = 0, i;

	PDEBUG(DEBUG_EPOINT, "EPOINT(%d): terminal '%s'\n", ea_endpoint->ep_serial, e_ext.number);
	if (e_ext.number[0]) {
		if (read_extension(&e_ext, e_ext.number))
			writeext = 0x10;

		if (!e_start) {
			time(&e_start);
			e_stop = 0;
		} else
		if (!e_stop)
			time(&e_stop);
		PDEBUG(DEBUG_EPOINT, "EPOINT(%d): writing connect from %s to %s into logfile of %s\n", ea_endpoint->ep_serial, e_callerinfo.id, e_dialinginfo.id, e_ext.number);
		switch(e_dialinginfo.itype) {
			case INFO_ITYPE_CHAN:
			SPRINT(dialingtext, "chan:%s", e_dialinginfo.id);
			break;
			case INFO_ITYPE_ISDN_EXTENSION:
			SPRINT(dialingtext, "intern:%s", e_dialinginfo.id);
			break;
			case INFO_ITYPE_VBOX:
			SPRINT(dialingtext, "vbox:%s", e_dialinginfo.id);
			break;
			default:
			SPRINT(dialingtext, "%s", e_dialinginfo.id);
		}

		if (e_callerinfo.id[0])
			SPRINT(callertext, "%s", numberrize_callerinfo(e_callerinfo.id, e_callerinfo.ntype, options.national, options.international));
		else
			SPRINT(callertext, "unknown");
		/* allpy restriction */
		if (!e_ext.anon_ignore && e_callerinfo.present==INFO_PRESENT_RESTRICTED)
			SPRINT(callertext, "anonymous");
		if (e_callerinfo.extension[0]) /* add intern if present */
			UNPRINT(strchr(callertext,'\0'), sizeof(callertext)-1+strlen(callertext), " (intern %s)", e_callerinfo.extension);
		write_log(e_ext.number, callertext, dialingtext, e_start, e_stop, 0, cause, location);

		/* store last received call for reply-list */
		if (e_origin == 1) // outgoing to phone is incoming for user
		if (e_callerinfo.id[0] || e_callerinfo.extension[0])
		if (e_ext.anon_ignore || e_callerinfo.present!=INFO_PRESENT_RESTRICTED) {
			if (e_callerinfo.extension[0])
				SPRINT(callertext, "intern:%s", e_callerinfo.extension);
			else
				SPRINT(callertext, "extern:%s", numberrize_callerinfo(e_callerinfo.id, e_callerinfo.ntype, options.national, options.international));
			if (!!strcmp(callertext, e_ext.last_in[0])) {
				i = MAX_REMEMBER-1;
				while(i) {
					UCPY(e_ext.last_in[i], e_ext.last_in[i-1]);
					i--;
				}
				SCPY(e_ext.last_in[0], callertext);
				writeext |= 1; /* store extension later */
				PDEBUG(DEBUG_EPOINT, "EPOINT(%d): terminal %s: storing last received caller id '%s'.\n", ea_endpoint->ep_serial, e_ext.number, e_ext.last_in[0]);
			} else
				PDEBUG(DEBUG_EPOINT, "EPOINT(%d): terminal %s: cannot store last received id '%s' because it is identical with the last one.\n", ea_endpoint->ep_serial, e_ext.number, callertext);
		}

		/* store last made call for reply-list */
		if (e_origin == 0) // incoming from phone is outgoing for user
		if (e_dialinginfo.id[0]) {
			if (!!strcmp(e_dialinginfo.id, e_ext.last_out[0])) {
				i = MAX_REMEMBER-1;
				while(i) {
					UCPY(e_ext.last_out[i], e_ext.last_out[i-1]);
					i--;
				}
				SCPY(e_ext.last_out[0], e_dialinginfo.id);
				writeext |= 1; /* store extension later */
				PDEBUG(DEBUG_EPOINT, "EPOINT(%d): terminal %s: storing last number '%s'.\n", ea_endpoint->ep_serial, e_ext.number, e_dialinginfo.id);
			} else
				PDEBUG(DEBUG_EPOINT, "EPOINT(%d): terminal %s: cannot store last number '%s' because it is identical with the last one.\n", ea_endpoint->ep_serial, e_ext.number, e_dialinginfo.id);
		}
	}
	/* write extension if needed */
	if (writeext == 0x11)
		write_extension(&e_ext, e_ext.number);

	if (e_action) {
		if (action_defs[e_action->index].hangup_func) {
			PDEBUG(DEBUG_EPOINT, "EPOINT(%d): terminal %s: current action '%s' has a hangup function, so we call it...\n", ea_endpoint->ep_serial, e_ext.number, action_defs[e_action->index].name);
			(this->*(action_defs[e_action->index].hangup_func))();
		}
	}
}

