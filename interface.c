/*****************************************************************************\
**                                                                           **
** LCR                                                                       **
**                                                                           **
**---------------------------------------------------------------------------**
** Copyright: Andreas Eversberg                                              **
**                                                                           **
** reading interface.conf file and filling structure                         **
**                                                                           **
\*****************************************************************************/ 

#include "main.h"

struct interface *interface_first = NULL; /* first interface is current list */
struct interface *interface_newlist = NULL; /* first interface in new list */


#ifdef WITH_MISDN
/* set default out_channel */
void default_out_channel(struct interface_port *ifport)
{
	struct select_channel *selchannel, **selchannelp;

	selchannel = (struct select_channel *)MALLOC(sizeof(struct select_channel));
	memuse++;

	if (ifport->mISDNport->ntmode)
		selchannel->channel = CHANNEL_FREE;
	else
		selchannel->channel = CHANNEL_ANY;
	
	ifport->out_channel = selchannel;

	/* additional channel selection for multipoint NT ports */
	if (!ifport->mISDNport->ptp && ifport->mISDNport->ntmode) {
		selchannelp = &(selchannel->next);
		selchannel = (struct select_channel *)MALLOC(sizeof(struct select_channel));
		memuse++;
		selchannel->channel = CHANNEL_NO; // call waiting
		*selchannelp = selchannel;
	}
}


/* set default in_channel */
void default_in_channel(struct interface_port *ifport)
{
	struct select_channel *selchannel;

	selchannel = (struct select_channel *)MALLOC(sizeof(struct select_channel));
	memuse++;
	
	selchannel->channel = CHANNEL_FREE;
	
	ifport->in_channel = selchannel;
}
#endif


/* parse string for a positive number */
static int get_number(char *value)
{
	int val = 0;
	char text[10];

	val = atoi(value);
	
	SPRINT(text, "%d", val);

	if (!strcmp(value, text))
		return(val);

	return(-1);
}


/* remove element from buffer
 * and return pointer to next element in buffer */
static char *get_seperated(char *buffer)
{
	while(*buffer) {
		if (*buffer==',' || *buffer<=32) { /* seperate */
			*buffer++ = '\0';
			while((*buffer>'\0' && *buffer<=32) || *buffer==',')
				buffer++;
			return(buffer);
		}
		buffer++;
	}
	return(buffer);
}

/*
 * parameter processing
 */
static int inter_block(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	struct interface_port *ifport;

	/* port in chain ? */
	if (!interface->ifport) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects previous 'port' definition.\n", filename, line, parameter);
		return(-1);
	}
	/* goto end of chain */
	ifport = interface->ifport;
	while(ifport->next)
		ifport = ifport->next;
	/* add value */
	if (value[0]) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects no value.\n", filename, line, parameter);
		return(-1);
	}
	ifport->block = 1;
	return(0);
}
static int inter_extension(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	if (interface->external) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' not allowed, because interface is external interface.\n", filename, line, parameter);
		return(-1);
	}
	if (value[0]) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects no value.\n", filename, line, parameter);
		return(-1);
	}
	interface->extension = 1;
	return(0);
}
static int inter_extern(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	if (interface->extension) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' not allowed, because interface is an extension.\n", filename, line, parameter);
		return(-1);
	}
	if (value[0]) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects no value.\n", filename, line, parameter);
		return(-1);
	}
	interface->external = 1;
	return(0);
}
static int inter_ptp(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	struct interface_port *ifport;

	/* port in chain ? */
	if (!interface->ifport) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects previous 'port' definition.\n", filename, line, parameter);
		return(-1);
	}
	if (interface->ifport->ptmp) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' previously ptmp was given.\n", filename, line, parameter);
		return(-1);
	}
	/* goto end of chain */
	ifport = interface->ifport;
	while(ifport->next)
		ifport = ifport->next;
	/* add value */
	if (value[0]) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects no value.\n", filename, line, parameter);
		return(-1);
	}
	ifport->ptp = 1;
	return(0);
}
#if 0
static int inter_ptmp(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	struct interface_port *ifport;

	/* port in chain ? */
	if (!interface->ifport) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects previous 'port' definition.\n", filename, line, parameter);
		return(-1);
	}
	if (interface->ifport->ptp) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' previously ptp was given.\n", filename, line, parameter);
		return(-1);
	}
	/* goto end of chain */
	ifport = interface->ifport;
	while(ifport->next)
		ifport = ifport->next;
	/* add value */
	if (value[0]) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects no value.\n", filename, line, parameter);
		return(-1);
	}
	ifport->ptmp = 1;
	return(0);
}
#endif
static int inter_nt(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	struct interface_port *ifport;

	/* port in chain ? */
	if (!interface->ifport) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects previous 'port' definition.\n", filename, line, parameter);
		return(-1);
	}
	/* goto end of chain */
	ifport = interface->ifport;
	while(ifport->next)
		ifport = ifport->next;
	/* add value */
	if (value[0]) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects no value.\n", filename, line, parameter);
		return(-1);
	}
	ifport->nt = 1;
	return(0);
}
static int inter_tespecial(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	struct interface_port *ifport;

	/* port in chain ? */
	if (!interface->ifport) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects previous 'port' definition.\n", filename, line, parameter);
		return(-1);
	}
	/* goto end of chain */
	ifport = interface->ifport;
	while(ifport->next)
		ifport = ifport->next;
	/* add value */
	if (value[0]) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects no value.\n", filename, line, parameter);
		return(-1);
	}
	ifport->tespecial = 1;
	return(0);
}
static int inter_tones(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	if (!strcasecmp(value, "yes")) {
		interface->is_tones = IS_YES;
	} else
	if (!strcasecmp(value, "no")) {
		interface->is_tones = IS_NO;
	} else {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects value 'yes' or 'no'.\n", filename, line, parameter);
		return(-1);
	}
	return(0);
}
static int inter_earlyb(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	if (!strcasecmp(value, "yes")) {
		interface->is_earlyb = IS_YES;
	} else
	if (!strcasecmp(value, "no")) {
		interface->is_earlyb = IS_NO;
	} else {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects value 'yes' or 'no'.\n", filename, line, parameter);
		return(-1);
	}
	return(0);
}
static int inter_hunt(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	if (!strcasecmp(value, "linear")) {
		interface->hunt = HUNT_LINEAR;
	} else
	if (!strcasecmp(value, "roundrobin")) {
		interface->hunt = HUNT_ROUNDROBIN;
	} else {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects value 'linear' or 'roundrobin'.\n", filename, line, parameter);
		return(-1);
	}
	return(0);
}
static int inter_port(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	SPRINT(interface_error, "Error in %s (line %d): parameter '%s' is outdated.\nPlease use 'portnum' and decrease port number by 1! Ports are counted from 0 now.\n", filename, line, parameter);
	return(-1);
}
static int inter_portnum(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
#ifndef WITH_MISDN
	SPRINT(interface_error, "Error in %s (line %d): mISDN support is not compiled in.\n", filename, line);
	return(-1);
#else
	struct interface_port *ifport, **ifportp;
	struct interface *searchif;
	int val;

	val = get_number(value);
	if (val == -1) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects one numeric value.\n", filename, line, parameter);
		return(-1);
	}
	/* check for port already assigned */
	searchif = interface_newlist;
	while(searchif) {
		ifport = searchif->ifport;
		while(ifport) {
			if (ifport->portnum == val) {
				SPRINT(interface_error, "Error in %s (line %d): port '%d' already used above.\n", filename, line, val);
				return(-1);
			}
			ifport = ifport->next;
		}
		searchif = searchif->next;
	}
	/* alloc port substructure */
	ifport = (struct interface_port *)MALLOC(sizeof(struct interface_port));
	memuse++;
	ifport->interface = interface;
	/* set value */
	ifport->portnum = val;
	/* tail port */
	ifportp = &interface->ifport;
	while(*ifportp)
		ifportp = &((*ifportp)->next);
	*ifportp = ifport;
	return(0);
#endif
}
static int inter_portname(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
#ifndef WITH_MISDN
	SPRINT(interface_error, "Error in %s (line %d): mISDN support is not compiled in.\n", filename, line);
	return(-1);
#else
	struct interface_port *ifport, **ifportp;

	/* goto end of chain */
	ifport = interface->ifport;
	if (ifport) {
		while(ifport->next)
			ifport = ifport->next;
	}

	/* alloc port substructure */
	ifport = (struct interface_port *)MALLOC(sizeof(struct interface_port));
	memuse++;
	ifport->interface = interface;
	/* set value */
	ifport->portnum = -1; // disable until resolved
	SCPY(ifport->portname, value);
	/* tail port */
	ifportp = &interface->ifport;
	while(*ifportp)
		ifportp = &((*ifportp)->next);
	*ifportp = ifport;
	return(0);
#endif
}
static int inter_l1hold(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	struct interface_port *ifport;

	/* port in chain ? */
	if (!interface->ifport) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects previous 'port' definition.\n", filename, line, parameter);
		return(-1);
	}
	/* goto end of chain */
	ifport = interface->ifport;
	while(ifport->next)
		ifport = ifport->next;
	if (!strcmp(value, "yes")) {
		ifport->l1hold = 1;
	} else
	if (!strcmp(value, "no")) {
		ifport->l1hold = 0;
	} else {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expecting parameter 'yes' or 'no'.\n", filename, line, parameter);
		return(-1);
	}
	return(0);
}
static int inter_l2hold(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	struct interface_port *ifport;

	/* port in chain ? */
	if (!interface->ifport) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects previous 'port' definition.\n", filename, line, parameter);
		return(-1);
	}
	/* goto end of chain */
	ifport = interface->ifport;
	while(ifport->next)
		ifport = ifport->next;
	if (!strcmp(value, "yes")) {
		ifport->l2hold = 1;
	} else
	if (!strcmp(value, "no")) {
		ifport->l2hold = -1;
	} else {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expecting parameter 'yes' or 'no'.\n", filename, line, parameter);
		return(-1);
	}
	return(0);
}
static int inter_channel_out(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	struct interface_port *ifport;
	struct select_channel *selchannel, **selchannelp;
	int val;
	char *p, *el;

	/* port in chain ? */
	if (!interface->ifport) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects previous 'port' definition.\n", filename, line, parameter);
		return(-1);
	}
	/* goto end of chain */
	ifport = interface->ifport;
	while(ifport->next)
		ifport = ifport->next;
	p = value;
	while(*p) {
		el = p;
		p = get_seperated(p);
		if (!strcasecmp(el, "force")) {
			ifport->channel_force = 1;
			if (ifport->out_channel) {
				SPRINT(interface_error, "Error in %s (line %d): value 'force' may only appear as first element in list.\n", filename, line);
				return(-1);
			}
		} else
		if (!strcasecmp(el, "any")) {
			val = CHANNEL_ANY;
			goto selchannel;
		} else
		if (!strcasecmp(el, "free")) {
			val = CHANNEL_FREE;
			goto selchannel;
		} else
		if (!strcasecmp(el, "no")) {
			val = CHANNEL_NO;
			goto selchannel;
		} else {
			val = get_number(el);
			if (val == -1) {
				SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects a comma seperated list of 'force', 'any', 'free', 'no' and any channel number.\n", filename, line, parameter);
				return(-1);
			}

			if (val<1 || val==16 || val>126) {
				SPRINT(interface_error, "Error in %s (line %d): channel '%d' out of range.\n", filename, line, val);
				return(-1);
			}
			selchannel:
			/* add to select-channel list */
			selchannel = (struct select_channel *)MALLOC(sizeof(struct select_channel));
			memuse++;
			/* set value */
			selchannel->channel = val;
			/* tail port */
			selchannelp = &ifport->out_channel;
			while(*selchannelp)
				selchannelp = &((*selchannelp)->next);
			*selchannelp = selchannel;
		}
	}
	return(0);
}
static int inter_channel_in(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	struct interface_port *ifport;
	struct select_channel *selchannel, **selchannelp;
	int val;
	char *p, *el;

	/* port in chain ? */
	if (!interface->ifport) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects previous 'port' definition.\n", filename, line, parameter);
		return(-1);
	}
	/* goto end of chain */
	ifport = interface->ifport;
	while(ifport->next)
		ifport = ifport->next;
	p = value;
	while(*p) {
		el = p;
		p = get_seperated(p);
		if (ifport->in_channel) if (ifport->in_channel->channel == CHANNEL_FREE) {
			SPRINT(interface_error, "Error in %s (line %d): parameter '%s' has values behind 'free' keyword. They has no effect.\n", filename, line, parameter);
				return(-1);
		}
		if (!strcasecmp(el, "free")) {
			val = CHANNEL_FREE;
			goto selchannel;
		} else {
			val = get_number(el);
			if (val == -1) {
				SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects a comma seperated list of channel numbers and 'free'.\n", filename, line, parameter);
				return(-1);
			}

			if (val<1 || val==16 || val>126) {
				SPRINT(interface_error, "Error in %s (line %d): channel '%d' out of range.\n", filename, line, val);
				return(-1);
			}
			selchannel:
			/* add to select-channel list */
			selchannel = (struct select_channel *)MALLOC(sizeof(struct select_channel));
			memuse++;
			/* set value */
			selchannel->channel = val;
			/* tail port */
			selchannelp = &ifport->in_channel;
			while(*selchannelp)
				selchannelp = &((*selchannelp)->next);
			*selchannelp = selchannel;
		}
	}
	return(0);
}
static int inter_timeouts(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	struct interface_port *ifport;
//	struct select_channel *selchannel, **selchannelp;
//	int val;
	char *p, *el;

	/* port in chain ? */
	if (!interface->ifport) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects previous 'port' definition.\n", filename, line, parameter);
		return(-1);
	}
	/* goto end of chain */
	ifport = interface->ifport;
	while(ifport->next)
		ifport = ifport->next;
	p = value;
	if (!*p) {
		nofive:
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects five timeout values.\n", filename, line, parameter);
		return(-1);
	}
	el = p;
	p = get_seperated(p);
	ifport->tout_setup = atoi(el);
	if (!*p)
		goto nofive;
	el = p;
	p = get_seperated(p);
	ifport->tout_dialing = atoi(el);
	if (!*p)
		goto nofive;
	el = p;
	p = get_seperated(p);
	ifport->tout_proceeding = atoi(el);
	if (!*p)
		goto nofive;
	el = p;
	p = get_seperated(p);
	ifport->tout_alerting = atoi(el);
	if (!*p)
		goto nofive;
	el = p;
	p = get_seperated(p);
	ifport->tout_disconnect = atoi(el);
	return(0);
}
static int inter_msn(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	struct interface_msn *ifmsn, **ifmsnp;
	char *p, *el;

	if (!value[0]) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects one MSN number or a list.\n", filename, line, parameter);
		return(-1);
	}
	if (interface->ifscreen_in) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' not allowed with 'screen_in' parameter.\n", filename, line, parameter);
		return(-1);
	}

	/* process list */
	p = value;
	while(*p) {
		el = p;
		p = get_seperated(p);
		/* add MSN to list */
		ifmsn = (struct interface_msn *)MALLOC(sizeof(struct interface_msn));
		memuse++;
		/* set value */
		SCPY(ifmsn->msn, el);
		/* tail port */
		ifmsnp = &interface->ifmsn;
		while(*ifmsnp)
			ifmsnp = &((*ifmsnp)->next);
		*ifmsnp = ifmsn;
	}
	return(0);
}
static int inter_screen(struct interface_screen **ifscreenp, struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	struct interface_screen *ifscreen;
	char *p, *el;

	if (!value[0]) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects old caller ID and new caller ID.\n", filename, line, parameter);
		return(-1);
	}
	/* add screen entry to list*/
	ifscreen = (struct interface_screen *)MALLOC(sizeof(struct interface_screen));
	memuse++;
	ifscreen->match_type = -1; /* unchecked */
	ifscreen->match_present = -1; /* unchecked */
	ifscreen->result_type = -1; /* unchanged */
	ifscreen->result_present = -1; /* unchanged */
	/* tail port */
	while(*ifscreenp)
		ifscreenp = &((*ifscreenp)->next);
	*ifscreenp = ifscreen;
//	printf("interface=%s\n", interface->name);
	/* get match */
	p = value;
	while(*p) {
		el = p;
		p = get_seperated(p);
		if (!strcasecmp(el, "unknown")) {
			if (ifscreen->match_type != -1) {
				typeerror:
				SPRINT(interface_error, "Error in %s (line %d): number type already set earlier.\n", filename, line, parameter);
				return(-1);
			}
			ifscreen->match_type = INFO_NTYPE_UNKNOWN;
		} else
		if (!strcasecmp(el, "subscriber")) {
			if (ifscreen->match_type != -1)
				goto typeerror;
			ifscreen->match_type = INFO_NTYPE_SUBSCRIBER;
		} else
		if (!strcasecmp(el, "national")) {
			if (ifscreen->match_type != -1)
				goto typeerror;
			ifscreen->match_type = INFO_NTYPE_NATIONAL;
		} else
		if (!strcasecmp(el, "international")) {
			if (ifscreen->match_type != -1)
				goto typeerror;
			ifscreen->match_type = INFO_NTYPE_INTERNATIONAL;
		} else
		if (!strcasecmp(el, "allowed")) {
			if (ifscreen->match_present != -1) {
				presenterror:
				SPRINT(interface_error, "Error in %s (line %d): presentation type already set earlier.\n", filename, line);
				return(-1);
			}
			ifscreen->match_present = INFO_PRESENT_ALLOWED;
		} else
		if (!strcasecmp(el, "restrict") || !strcasecmp(el, "restricted")) {
			if (ifscreen->match_present != -1)
				goto presenterror;
			ifscreen->match_present = INFO_PRESENT_RESTRICTED;
		} else {
			SCPY(ifscreen->match, el);
			/* check for % at the end */
			if (strchr(el, '%')) {
				if (strchr(el, '%') != el+strlen(el)-1) {
					SPRINT(interface_error, "Error in %s (line %d): %% joker found, but must at the end.\n", filename, line, parameter);
					return(-1);
				}
			}
			break;
		}
	}
	if (ifscreen->match[0] == '\0') {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects old caller ID.\n", filename, line, parameter);
		return(-1);
	}
	/* get result */
	while(*p) {
		el = p;
		p = get_seperated(p);
		if (!strcasecmp(el, "unknown")) {
			if (ifscreen->result_type != -1)
				goto typeerror;
			ifscreen->result_type = INFO_NTYPE_UNKNOWN;
		} else
		if (!strcasecmp(el, "subscriber")) {
			if (ifscreen->result_type != -1)
				goto typeerror;
			ifscreen->result_type = INFO_NTYPE_SUBSCRIBER;
		} else
		if (!strcasecmp(el, "national")) {
			if (ifscreen->result_type != -1)
				goto typeerror;
			ifscreen->result_type = INFO_NTYPE_NATIONAL;
		} else
		if (!strcasecmp(el, "international")) {
			if (ifscreen->result_type != -1)
				goto typeerror;
			ifscreen->result_type = INFO_NTYPE_INTERNATIONAL;
		} else
		if (!strcasecmp(el, "present") || !strcasecmp(el, "presented") || !strcasecmp(el, "allowed") || !strcasecmp(el, "allow")) {
			if (ifscreen->result_present != -1)
				goto presenterror;
			ifscreen->result_present = INFO_PRESENT_ALLOWED;
		} else
		if (!strcasecmp(el, "restrict") || !strcasecmp(el, "restricted") || !strcasecmp(el, "deny") || !strcasecmp(el, "denied")) {
			if (ifscreen->result_present != -1)
				goto presenterror;
			ifscreen->result_present = INFO_PRESENT_RESTRICTED;
		} else {
			SCPY(ifscreen->result, el);
			/* check for % at the end */
			if (strchr(el, '%')) {
				if (strchr(el, '%') != el+strlen(el)-1) {
					SPRINT(interface_error, "Error in %s (line %d): %% joker found, but must at the end.\n", filename, line, parameter);
					return(-1);
				}
			}
			break;
		}
	}
	if (ifscreen->result[0] == '\0') {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects new caller ID.\n", filename, line, parameter);
		return(-1);
	}
	return(0);
}
static int inter_screen_in(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	if (interface->ifmsn) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' not allowed with 'msn' parameter.\n", filename, line, parameter);
		return(-1);
	}

	return(inter_screen(&interface->ifscreen_in, interface, filename, line, parameter, value));
}
static int inter_screen_out(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	return(inter_screen(&interface->ifscreen_out, interface, filename, line, parameter, value));
}
static int inter_nodtmf(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	struct interface_port *ifport;

	/* port in chain ? */
	if (!interface->ifport) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects previous 'port' definition.\n", filename, line, parameter);
		return(-1);
	}
	/* goto end of chain */
	ifport = interface->ifport;
	while(ifport->next)
		ifport = ifport->next;
	ifport->nodtmf = 1;
	return(0);
}
static int inter_dtmf_threshold(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	struct interface_port *ifport;

	/* port in chain ? */
	if (!interface->ifport) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects previous 'port' definition.\n", filename, line, parameter);
		return(-1);
	}
	/* goto end of chain */
	ifport = interface->ifport;
	while(ifport->next)
		ifport = ifport->next;
	ifport->dtmf_threshold = atoi(value);
	return(0);
}
static int inter_filter(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	char *p, *q;

	/* seperate parameter from filter */
	p = value;
	while(*p > 32)
		p++;
	if (*p) {
		*p++ = 0;
		while(*p > 0 && *p <= 32)
			p++;
	}

	if (!strcasecmp(value, "gain")) {
		q = p;
		while(*q > 32)
			q++;
		if (*q) {
			*q++ = 0;
			while(*q > 0 && *q <= 32)
				q++;
		}
		if (*p == 0 || *q == 0) {
			SPRINT(interface_error, "Error in %s (line %d): parameter '%s %s' expects two gain values.\n", filename, line, parameter, value);
			return(-1);
		}
		if (atoi(p)<-8 || atoi(p)>8 || atoi(q)<-8 || atoi(q)>8) {
			SPRINT(interface_error, "Error in %s (line %d): parameter '%s %s' gain values not in range. (-8...8)\n", filename, line, parameter, value);
			return(-1);
		}
		interface->tx_gain = atoi(p);
		interface->rx_gain = atoi(q);
	} else
	if (!strcasecmp(value, "pipeline")) {
		if (*p == 0) {
			SPRINT(interface_error, "Error in %s (line %d): parameter '%s %s' expects pipeline string.\n", filename, line, parameter, value);
			return(-1);
		}
		SCPY(interface->pipeline, p);
	} else
	if (!strcasecmp(value, "blowfish")) {
		unsigned char key[56];
		int l;
		
		if (!!strncmp(p, "0x", 2)) {
			SPRINT(interface_error, "Error in %s (line %d): parameter '%s %s' expects blowfish key starting with '0x'.\n", filename, line, parameter, value);
			return(-1);
		}
		p += 2;
		l = 0; 
		while(*p) {
			if (l == 56) {
				SPRINT(interface_error, "Error in %s (line %d): parameter '%s %s' key too long.\n", filename, line, parameter, value);
				return(-1);
			}
			if (*p >= '0' && *p <= '9')
				key[l] = (*p-'0')<<4;
			else if (*p >= 'a' && *p <= 'f')
				key[l] = (*p-'a'+10)<<4;
			else if (*p >= 'A' && *p <= 'F')
				key[l] = (*p-'A'+10)<<4;
			else {
				digout:
				SPRINT(interface_error, "Error in %s (line %d): parameter '%s %s' key has digits out of range. (0...9, a...f)\n", filename, line, parameter, value);
				return(-1);
			}
			p++;
			if (*p == 0) {
				SPRINT(interface_error, "Error in %s (line %d): parameter '%s %s' key must end on an 8 bit boundary (two character boundary).\n", filename, line, parameter, value);
				return(-1);
			}
			if (*p >= '0' && *p <= '9')
				key[l] = (*p-'0')<<4;
			else if (*p >= 'a' && *p <= 'f')
				key[l] = (*p-'a'+10)<<4;
			else if (*p >= 'A' && *p <= 'F')
				key[l] = (*p-'A'+10)<<4;
			else
				goto digout;
			p++;
			l++;
		}
		if (l < 4) {
			SPRINT(interface_error, "Error in %s (line %d): parameter '%s %s' key must be at least 4 bytes (8 characters).\n", filename, line, parameter, value);
			return(-1);
		}
		memcpy(interface->bf_key, key, l);
		interface->bf_len = l;
	} else {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' has unknown filter '%s'.\n", filename, line, parameter, value);
		return(-1);
	}
	return(0);
}
static int inter_dialmax(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	struct interface_port *ifport;

	/* port in chain ? */
	if (!interface->ifport) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects previous 'port' definition.\n", filename, line, parameter);
		return(-1);
	}
	/* goto end of chain */
	ifport = interface->ifport;
	while(ifport->next)
		ifport = ifport->next;
	ifport->dialmax = atoi(value);
	return(0);
}
static int inter_tones_dir(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	struct interface_port *ifport;

	/* port in chain ? */
	if (!interface->ifport) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects previous 'port' definition.\n", filename, line, parameter);
		return(-1);
	}
	/* goto end of chain */
	ifport = interface->ifport;
	while(ifport->next)
		ifport = ifport->next;
	SCPY(ifport->tones_dir, value);
	return(0);
}
static int inter_gsm(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	SPRINT(interface_error, "Error in %s (line %d): parameter '%s' is outdated.\nPlease use 'gsm-bs' for base station or 'gsm-ms' for mobile station interface!\n", filename, line, parameter);
	return(-1);
}
static int inter_gsm_bs(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
#ifndef WITH_GSM_BS
	SPRINT(interface_error, "Error in %s (line %d): GSM BS side not compiled in.\n", filename, line);
	return(-1);
#else
	struct interface *searchif;

	searchif = interface_newlist;
	while(searchif) {
		if (searchif->gsm_bs) {
			SPRINT(interface_error, "Error in %s (line %d): interface '%s' already uses gsm BS side.\n", filename, line, searchif->name);
			return(-1);
		}
		searchif = searchif->next;
	}

	/* goto end of chain again to set gsmflag */
	interface->gsm_bs = 1;

	return(0);
#endif
}
static int inter_gsm_ms(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
#ifndef WITH_GSM_MS
	SPRINT(interface_error, "Error in %s (line %d): GSM MS side not compiled in.\n", filename, line);
	return(-1);
#else
	struct interface *searchif;

	interface->gsm_ms = 1;

	/* copy values */
	if (!value || !value[0]) {
		SPRINT(interface_error, "Error in %s (line %d): Missing MS name and socket name.\n", filename, line);
		return(-1);
	}
	SCPY(interface->gsm_ms_name, value);

	/* check if name is used multiple times */
	searchif = interface_newlist;
	while(searchif) {
		if (searchif != interface && !strcmp(searchif->gsm_ms_name, interface->gsm_ms_name)) {
			SPRINT(interface_error, "Error in %s (line %d): mobile '%s' already uses the given MS name '%s', choose a different one.\n", filename, line, interface->gsm_ms_name, searchif->gsm_ms_name);
			return(-1);
		}
		searchif = searchif->next;
	}

	return(0);
#endif
}
static int inter_sip(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
#ifndef WITH_SIP
	SPRINT(interface_error, "Error in %s (line %d): SIP not compiled in.\n", filename, line);
	return(-1);
#else
	char *p;

	interface->sip = 1;

	/* copy values */
	if (!value || !value[0]) {
		SPRINT(interface_error, "Error in %s (line %d): Missing SIP local IP.\n", filename, line);
		return(-1);
	}
	p = get_seperated(value);
	if (!p[0]) {
		SPRINT(interface_error, "Error in %s (line %d): Missing SIP remote IP.\n", filename, line);
		return(-1);
	}
	SCPY(interface->sip_local_peer, value);
	SCPY(interface->sip_remote_peer, p);

	return(0);
#endif
}
static int inter_rtp_bridge(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	int supported = 0;

#ifdef WITH_GSM_BS
	if (interface->gsm_bs)
		supported = 1;
#endif
#ifdef WITH_SIP
	if (interface->sip)
		supported = 1;
#endif
	if (!supported) {
		SPRINT(interface_error, "Error in %s (line %d): Interface does not support RTP\n", filename, line);
		return(-1);
	}
	interface->rtp_bridge = 1;

	return(0);
}
#if 0
static int inter_rtp_payload(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
#ifndef WITH_GSM_BS
	SPRINT(interface_error, "Error in %s (line %d): GSM BS side not compiled in.\n", filename, line);
	return(-1);
#else
	if (!interface->gsm_bs) {
		SPRINT(interface_error, "Error in %s (line %d): This parameter only works for GSM BS side interface\n", filename, line);
		return(-1);
	}
	if (!interface->rtp_bridge) {
		SPRINT(interface_error, "Error in %s (line %d): This parameter only works here, if RTP bridging is enabled\n", filename, line);
		return(-1);
	}
	if (!value[0]) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects one payload type\n", filename, line, parameter);
		return(-1);
	}
	if (interface->gsm_bs_payloads == sizeof(interface->gsm_bs_payload_types)) {
		SPRINT(interface_error, "Error in %s (line %d): Too many payload types defined\n", filename, line);
		return(-1);
	}
	interface->gsm_bs_payload_types[interface->gsm_bs_payloads++] = atoi(value);

	return(0);
#endif
}
#endif
static int inter_nonotify(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	struct interface_port *ifport;

	/* port in chain ? */
	if (!interface->ifport) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects previous 'port' definition.\n", filename, line, parameter);
		return(-1);
	}
	/* goto end of chain */
	ifport = interface->ifport;
	while(ifport->next)
		ifport = ifport->next;
	ifport->nonotify = 1;
	return(0);
}
#ifdef WITH_SS5
static int inter_ss5(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	struct interface_port *ifport;
	char *element;

	/* port in chain ? */
	if (!interface->ifport) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects previous 'port' definition.\n", filename, line, parameter);
		return(-1);
	}
	/* goto end of chain */
	ifport = interface->ifport;
	while(ifport->next)
		ifport = ifport->next;
	ifport->ss5 |= SS5_ENABLE;
	while((element = strsep(&value, " "))) {
		if (element[0] == '\0')
			continue;
		if (!strcasecmp(element, "connect"))
			ifport->ss5 |= SS5_FEATURE_CONNECT;
		else
		if (!strcasecmp(element, "nodisconnect"))
			ifport->ss5 |= SS5_FEATURE_NODISCONNECT;
		else
		if (!strcasecmp(element, "releaseguardtimer"))
			ifport->ss5 |= SS5_FEATURE_RELEASEGUARDTIMER;
		else
		if (!strcasecmp(element, "bell"))
			ifport->ss5 |= SS5_FEATURE_BELL;
		else
		if (!strcasecmp(element, "pulsedialing"))
			ifport->ss5 |= SS5_FEATURE_PULSEDIALING;
		else
		if (!strcasecmp(element, "delay"))
			ifport->ss5 |= SS5_FEATURE_DELAY;
		else
		if (!strcasecmp(element, "starrelease"))
			ifport->ss5 |= SS5_FEATURE_STAR_RELEASE;
		else
		if (!strcasecmp(element, "suppress"))
			ifport->ss5 |= SS5_FEATURE_SUPPRESS;
		else {
			SPRINT(interface_error, "Error in %s (line %d): parameter '%s' does not allow value element '%s'.\n", filename, line, parameter, element);
			return(-1);
		}
	}
	return(0);
}
#endif
static int inter_remote(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	struct interface *searchif;

	if (!value[0]) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects application name as value.\n", filename, line, parameter);
		return(-1);
	}
	searchif = interface_newlist;
	while(searchif) {
		if (interface->remote && !strcmp(interface->remote_app, value)) {
			SPRINT(interface_error, "Error in %s (line %d): interface '%s' already uses remote application '%s'.\n", filename, line, interface->name, value);
			return(-1);
		}
		searchif = searchif->next;
	}

	/* goto end of chain again to set application name */
	interface->remote = 1;
	SCPY(interface->remote_app, value);

	return(0);
}
static int inter_context(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	if (!value[0]) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects application context as value.\n", filename, line, parameter);
		return(-1);
	}
	SCPY(interface->remote_context, value);

	return(0);
}
static int inter_pots_flash(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	struct interface_port *ifport;

	/* port in chain ? */
	if (!interface->ifport) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects previous 'port' definition.\n", filename, line, parameter);
		return(-1);
	}
	/* goto end of chain */
	ifport = interface->ifport;
	while(ifport->next)
		ifport = ifport->next;

	ifport->pots_flash = 1;
	return(0);
}
static int inter_pots_ring(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	struct interface_port *ifport;

	/* port in chain ? */
	if (!interface->ifport) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects previous 'port' definition.\n", filename, line, parameter);
		return(-1);
	}
	/* goto end of chain */
	ifport = interface->ifport;
	while(ifport->next)
		ifport = ifport->next;

	ifport->pots_ring = 1;
	return(0);
}
static int inter_pots_transfer(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	struct interface_port *ifport;

	/* port in chain ? */
	if (!interface->ifport) {
		SPRINT(interface_error, "Error in %s (line %d): parameter '%s' expects previous 'port' definition.\n", filename, line, parameter);
		return(-1);
	}
	/* goto end of chain */
	ifport = interface->ifport;
	while(ifport->next)
		ifport = ifport->next;

	ifport->pots_transfer = 1;
	return(0);
}
static int inter_shutdown(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	interface->shutdown = 1;

	return(0);
}
static int inter_bridge(struct interface *interface, char *filename, int line, char *parameter, char *value)
{
	if (!value || !value[0]) {
		SPRINT(interface_error, "Error in %s (line %d): Missing destination interface name.\n", filename, line);
		return(-1);
	}
	interface->app = EAPP_TYPE_BRIDGE;
	SCPY(interface->bridge_if, value);

	return(0);
}


/*
 * structure of parameters
 */
struct interface_param interface_param[] = {
	{ "extension", &inter_extension, "",
	"If keyword is given, calls to interface are handled as internal extensions."},

	{ "extern", &inter_extern, "",
	"If keyword is given, this interface will be used for external calls.\n"
	"Calls require an external interface, if the routing action 'extern' is used\nwithout specific interface given.\n"
	"Calls forwarded by extension's 'settings' also require an external interface."},

	{"tones", &inter_tones, "yes | no",
	"Interface generates tones during call setup and release, or not.\nBy default only NT-mode ports generate tones."},

	{"earlyb", &inter_earlyb, "yes | no",
	"Interface receives and bridges tones during call setup and release, or not.\nBy default only TE-mode ports receive tones."},

	{"hunt", &inter_hunt, "linear | roundrobin",
	"Select the algorithm for selecting port with free channel."},

	{"port", &inter_port, "<number>",
	""},
	{"portnum", &inter_portnum, "<number>",
	"Give exactly one port for this interface.\nTo give multiple ports, add more lines with port parameters."},
	{"portname", &inter_portname, "<name>",
	"Same as 'portnum', but the name is given instead.\nUse 'isdninfo' to list all available ports and names."},

	{"block", &inter_block, "",
	"If keyword is given, calls on this interface are blocked.\n"
	"This parameter must follow a 'port' parameter."},

	{"ptp", &inter_ptp, "",
	"The given port above is opened as point-to-point.\n"
	"This is required on NT-mode ports that are multipoint by default.\n"
	"This parameter must follow a 'port' parameter."},

#if 0
	{"ptmp", &inter_ptmp, "",
	"The given port above is opened as point-to-multipoint.\n"
	"This is required on PRI NT-mode ports that are point-to-point by default.\n"
	"This parameter must follow a 'port' parameter."},
#endif

	{"nt", &inter_nt, "",
	"The given port above is opened in NT-mode.\n"
	"This is required on interfaces that support both NT-mode and TE-mode.\n"
	"This parameter must follow a 'port' parameter."},

	{"te-special", &inter_tespecial, "",
	"The given port uses a modified TE-mode.\n"
	"All information elements that are allowed Network->User will then be\n"
	"transmitted User->Network also. This is usefull to pass all informations\n"
	"between two interconnected LCRs, like 'redirected number' or 'display'.\n"
	"Note that this is not compliant with ISDN protocol.\n"
	"This parameter must follow a 'port' parameter."},

	{"layer1hold", &inter_l1hold, "yes | no",
	"The given port will not release layer 1 after layer 2 is down.\n"
	"It is required to keep layer 1 of telephones up, to solve activation problems.\n"
	"This parameter must follow a 'port' parameter."},

	{"layer2hold", &inter_l2hold, "yes | no",
	"The given port will continuously try to establish layer 2 link and hold it.\n"
	"It is required for PTP links in most cases, therefore it is default.\n"
	"This parameter must follow a 'port' parameter."},

	{"channel-out", &inter_channel_out, "[force,][<number>][,...][,free][,any][,no]",
	"Channel selection list for all outgoing calls to the interface.\n"
	"A free channels is searched in order of appearance.\n"
	"This parameter must follow a 'port' parameter.\n"
	" force - Forces the selected port with no acceptable alternative (see Q.931).\n"
	"  -> this will be automatically set for multipoint (ptmp) NT-mode ports\n"
	" <number>[,...] - List of channels to search.\n"
	" free - Select any free channel\n"
	" any - On outgoing calls, signal 'any channel acceptable'. (see DSS1)\n"
	" no - Signal 'no channel available' aka 'call waiting'. (see DSS1)"},

	{"channel-in", &inter_channel_in, "[<number>][,...][,free]",
	"Channel selection list for all incoming calls from the interface.\n"
	"A free channels is accepted if in the list.\n"
	"If any channel was requested, the first free channel found is selected.\n"
	"This parameter must follow a 'port' parameter.\n"
	" <number>[,...] - List of channels to accept.\n"
	" free - Accept any free channel"},

	{"timeouts", &inter_timeouts, "<setup> <dialing> <proceeding> <alerting> <disconnect>",
	"Timeout values for call states. They are both for incoming and outgoing states.\n"
	"The default is 120 seconds for all states. Use 0 to disable.\n"
	"This parameter must follow a 'port' parameter.\n"},

	{"msn", &inter_msn, "<default MSN>,[<additional MSN>[,...]]",
	"Incoming caller ID is checked against given MSN numbers.\n"
	"If the caller ID is not found in this list, it is overwritten by the first MSN"},

	{"screen-in", &inter_screen_in, "[options] <old caller ID>[%] [options] <new caller ID>[%]",
	"Adds an entry for incoming calls to the caller ID screen list.\n"
	"If the given 'old caller ID' matches, it is replaced by the 'new caller ID'\n"
	"If '%' is given after old caller ID, it matches even if caller ID has\n"
	"additional digits.\n"
	"If '%' is given after mew caller ID, additinal digits of the 'old caller ID'\n"
	"are added.\n"
	"Options can be:\n"
	" unknown | subsciber | national | international - Change caller ID type.\n"
	" present | restrict - Change presentation of caller ID."},
		
	{"screen-out", &inter_screen_out, "[options] <old caller ID>[%] [options] <new caller ID>[%]",
	"Adds an entry for outgoing calls to the caller ID screen list.\n"
	"See 'screen-in' for help."},

	{"nodtmf", &inter_nodtmf, "",
	"Disables DTMF detection for this interface.\n"
	"This parameter must follow a 'port' parameter."},

	{"dtmf-threshold", &inter_dtmf_threshold, "",
	"Set threshold value for minimum DTMF tone level.\n"
	"This parameter must follow a 'port' parameter."},

	{"filter", &inter_filter, "<filter> <parameters>",
	"Adds/appends a filter. Filters are ordered in transmit direction.\n"
	"gain <tx-volume> <rx-volume> - Changes volume (-8 .. 8)\n"
	"pipeline <string> - Sets echo cancelation pipeline.\n"
	"blowfish <key> - Adds encryption. Key must be 4-56 bytes (8-112 hex characters."},

	{"dialmax", &inter_dialmax, "<digits>",
	"Limits the number of digits in setup/information message."},

	{"tones_dir", &inter_tones_dir, "<path>",
	"Overrides the given tone_dir in options.conf.\n"
	"To used kernel tones in mISDN_dsp.ko, say 'american', 'german', or 'oldgerman'."},

	{"gsm", &inter_gsm, "",
	""},
	{"gsm-bs", &inter_gsm_bs, "",
	"Sets up GSM base station interface for using OpenBSC."},
	{"gsm-ms", &inter_gsm_ms, "<socket>",
	"Sets up GSM mobile station interface for using Osmocom-BB.\n"
	"The name of the MS folows the interface name.\n"
	"The socket is /tmp/osmocom_l2 by default and need to be changed when multiple\n"
	"MS interfaces are used."},
	{"sip", &inter_sip, "<local IP> <remote IP>",
	"Sets up SIP interface that represents one SIP endpoint.\n"
	"Give SIP configuration file."},
	{"rtp-bridge", &inter_rtp_bridge, "",
	"Enables RTP bridging directly from this interface.\n"
	"This only works, if both ends support RTP. (like gsm-bs and sip)"},
#if 0
	not needed, since ms defines what is supports and remote (sip) tells what is selected
	{"rtp-payload", &inter_rtp_payload, "<codec>",
	"Define RTP payload to use. Only valid in conjuntion with gsm-bs!\n"
	"If multiple payloads are defined, the first has highest priority.\n"
	"If none are defined, GSM fullrate V1 (type 3) is assumed.\n"},
#endif
	{"nonotify", &inter_nonotify, "",
	"Prevents sending notify messages to this interface. A call placed on hold will\n"
	"Not affect the remote end (phone or telcom switch).\n"
	"This parameter must follow a 'port' parameter."},
	{"bridge", &inter_bridge, "<destination interface>",
	"Define bridge application for this interface. All calls received on this\n"
	"interface will be directly bridged to the given destination interface.\n"
	"There will be no PBX application, nor routing."},

#ifdef WITH_SS5
	{"ccitt5", &inter_ss5, "[<feature> [feature ...]]",
	"Interface uses CCITT No. 5 inband signalling rather than D-channel.\n"
	"This feature causes CPU load to rise and has no practical intend.\n"
	"If you don't know what it is, you don't need it.\n"
	"Features apply to protocol behaviour and blueboxing specials, they are:\n"
	" connect - Connect incomming call to throughconnect audio, if required.\n"
	" nodisconnect - Don't disconnect if incomming exchange disconnects.\n"
	" releaseguardtimer - Tries to prevent Blueboxing by a longer release-guard.\n"
	" bell - Allow releasing and pulse-dialing via 2600 Hz like old Bell systems.\n"
	" pulsedialing - Use pulse dialing on outgoing exchange. (takes long!)\n"
	" delay - Use on incomming exchange, to make you feel a delay when blueboxing.\n"
	" starrelease - Pulse dialing a star (11 pulses per digit) clears current call.\n"
	" suppress - Suppress received tones, as they will be recognized."},
#endif

	{"remote", &inter_remote, "<application>",
	"Sets up an interface that communicates with the remote application.\n"
	"Use \"asterisk\" to use chan_lcr as remote application."},
	{"context", &inter_context, "<context>",
	"Give context for calls to application."},

	{"pots-flash", &inter_pots_flash, "",
	"Allow flash button to hold an active call and setup a new call.\n"
	"Ihis parameter only appies to POTS type of interfaces\n"
	"This parameter must follow a 'port' parameter.\n"},
	{"pots-ring-after-hangup", &inter_pots_ring, "",
	"Allow ringing of last hold call after hangup. Other calls on hold will not be\n"
	"released.\n"
	"Ihis parameter only appies to POTS type of interfaces\n"
	"This parameter must follow a 'port' parameter.\n"},
	{"pots-transfer-after-hangup", &inter_pots_transfer, "",
	"If two calls on hold, both are connected after hangup.\n"
	"If one call is on hold and another one alerting, call on hold is tranfered.\n"
	"Ihis parameter only appies to POTS type of interfaces\n"
	"This parameter must follow a 'port' parameter.\n"},

	{"shutdown", &inter_shutdown, "",
	"Interface will not be loaded when processing interface.conf"},

	{NULL, NULL, NULL, NULL}
};

/* read interfaces
 *
 * read settings from interface.conf
 */
char interface_error[256];
struct interface *read_interfaces(void)
{
	FILE			*fp = NULL;
	char			filename[128];
	char			*p;
	unsigned int		line, i;
	char			buffer[256];
	struct interface	*interface = NULL, /* in case no interface */
				**interfacep = &interface_newlist;
	char			parameter[128];
	char			value[256];
	int			expecting = 1; /* expecting new interface */
	struct interface_param	*ifparam;

	if (interface_newlist != NULL)
		FATAL("list is not empty.\n");
	interface_error[0] = '\0';
	SPRINT(filename, "%s/interface.conf", CONFIG_DATA);

	if (!(fp = fopen(filename,"r"))) {
		SPRINT(interface_error, "Cannot open '%s'\n", filename);
		goto error;
	}

	line=0;
	while((GETLINE(buffer, fp))) {
		p=buffer;
		line++;

		while(*p <= 32) { /* skip spaces */
			if (*p == 0)
				break;
			p++;
		}
		if (*p==0 || *p=='#') /* ignore comments and empty line */
			continue;

		parameter[0]=0;
		value[0]=0;
		i=0; /* read parameter */
		while(*p > 32) {
			if (i+1 >= sizeof(parameter)) {
				SPRINT(interface_error, "Error in %s (line %d): parameter name too long.\n",filename,line);
				goto error;
			}
			parameter[i+1] = '\0';
			parameter[i++] = *p++;
		}

		while(*p <= 32) { /* skip spaces */
			if (*p == 0)
				break;
			p++;
		}

		if (*p!=0 && *p!='#') { /* missing name */
			i=0; /* read until end */
			while(*p!=0 && *p!='#') {
				if (i+1 >= sizeof(value)) {
					SPRINT(interface_error, "Error in %s (line %d): value too long.\n", filename, line);
					goto error;
				}
				value[i+1] = '\0';
				value[i++] = *p++;
			}

			/* remove trailing spaces from value */
			while(i) {
				if (value[i-1]==0 || value[i-1]>32)
					break;
				value[i-1] = '\0';
				i--;
			}
		}

		/* check for interface name as first statement */
		if (expecting && parameter[0]!='[') {
			SPRINT(interface_error, "Error in %s (line %d): expecting interface name inside [ and ], but got: '%s'.\n", filename, line, parameter);
			goto error;
		}
		expecting = 0;

		/* check for new interface */
		if (parameter[0] == '[') {
			if (parameter[strlen(parameter)-1] != ']') {
				SPRINT(interface_error, "Error in %s (line %d): expecting interface name inside [ and ], but got: '%s'.\n", filename, line, parameter);
				goto error;
			}
			parameter[strlen(parameter)-1] = '\0';

			/* check if interface name already exists */
			interface = interface_newlist;
			while(interface) {
				if (!strcasecmp(interface->name, parameter+1)) {
					SPRINT(interface_error, "Error in %s (line %d): interface name '%s' already defined above.\n", filename, line, parameter+1);
					goto error;
				}
				interface = interface->next;
			}

			/* append interface to new list */
			interface = (struct interface *)MALLOC(sizeof(struct interface));
			memuse++;

			/* name interface */
			SCPY(interface->name, parameter+1);

			/* attach */
			*interfacep = interface;
			interfacep = &interface->next;

			continue;
		}

		ifparam = interface_param;
		while(ifparam->name) {
			if (!strcasecmp(parameter, ifparam->name)) {
				if (ifparam->func(interface, filename, line, parameter, value))
					goto error;
				break;
			}
			ifparam++;
		}
		if (ifparam->name)
			continue;

		SPRINT(interface_error, "Error in %s (line %d): unknown parameter: '%s'.\n", filename, line, parameter);
		goto error;
	}

	if (fp) fclose(fp);
	return(interface_newlist);
error:
	PERROR_RUNTIME("%s", interface_error);
	if (fp) fclose(fp);
	free_interfaces(interface_newlist);
	interface_newlist = NULL;
	return(NULL);
}


/*
 * freeing chain of interfaces
 */
void free_interfaces(struct interface *interface)
{
	void *temp;
	struct interface_port *ifport;
	struct select_channel *selchannel;
	struct interface_msn *ifmsn;
	struct interface_screen *ifscreen;

	while(interface) {
		ifport = interface->ifport;
		while(ifport) {
			selchannel = ifport->in_channel;
			while(selchannel) {
				temp = selchannel;
				selchannel = selchannel->next;
				FREE(temp, sizeof(struct select_channel));
				memuse--;
			}
			selchannel = ifport->out_channel;
			while(selchannel) {
				temp = selchannel;
				selchannel = selchannel->next;
				FREE(temp, sizeof(struct select_channel));
				memuse--;
			}
			temp = ifport;
			ifport = ifport->next;
			FREE(temp, sizeof(struct interface_port));
			memuse--;
		}
		ifmsn = interface->ifmsn;
		while(ifmsn) {
			temp = ifmsn;
			ifmsn = ifmsn->next;
			FREE(temp, sizeof(struct interface_msn));
			memuse--;
		}
		ifscreen = interface->ifscreen_in;
		while(ifscreen) {
			temp = ifscreen;
			ifscreen = ifscreen->next;
			FREE(temp, sizeof(struct interface_screen));
			memuse--;
		}
		ifscreen = interface->ifscreen_out;
		while(ifscreen) {
			temp = ifscreen;
			ifscreen = ifscreen->next;
			FREE(temp, sizeof(struct interface_screen));
			memuse--;
		}
		temp = interface;
		interface = interface->next;
		FREE(temp, sizeof(struct interface));
		memuse--;
	}
}

#ifdef WITH_MISDN
/*
 * defaults of ports if not specified by config
 */
static void set_mISDN_defaults(struct interface_port *ifport)
{
	/* default channel selection list */
	if (!ifport->out_channel)
		default_out_channel(ifport);
	if (!ifport->in_channel)
		default_in_channel(ifport);
	/* must force the channel on PTMP/NT ports */
	if (!ifport->mISDNport->ptp && ifport->mISDNport->ntmode)
		ifport->channel_force = 1;
	/* default is_tones */
	if (ifport->interface->is_tones)
		ifport->mISDNport->tones = (ifport->interface->is_tones==IS_YES);
	else
		ifport->mISDNport->tones = (ifport->mISDNport->ntmode || ifport->mISDNport->ss5)?1:0;
	/* default is_earlyb */
	if (ifport->interface->is_earlyb)
		ifport->mISDNport->earlyb = (ifport->interface->is_earlyb==IS_YES);
	else
		ifport->mISDNport->earlyb = (ifport->mISDNport->ntmode && !ifport->mISDNport->ss5)?0:1;
	/* set locally flag */
	if (ifport->interface->extension)
		ifport->mISDNport->locally = 1;
	else
		ifport->mISDNport->locally = 0;
}
#endif


/*
 * all links between mISDNport and interface are made
 * unused mISDNports are closed, new mISDNports are opened
 * also set default select_channel lists
 */
void relink_interfaces(void)
{
#ifdef WITH_MISDN
	struct mISDNport *mISDNport;
	struct interface_port *ifport;
#endif
	struct interface *interface, *temp, *found;

	interface = interface_first;
	while(interface) {
		found = NULL;
		temp = interface_newlist;
		while(temp) {
			if (!strcmp(temp->name, interface->name))
				found = temp;
			temp = temp->next;
		}
		if (!found) {
#ifdef WITH_GSM_MS
			if (interface->gsm_ms)
				gsm_ms_delete(interface->gsm_ms_name);
#endif
#ifdef WITH_GSM_BS
			if (interface->gsm_bs)
				gsm_bs_exit(0);
#endif
#ifdef WITH_SIP
			if (interface->sip)
				sip_exit_inst(interface);
#endif
		} else {
#ifdef WITH_SIP
			if (interface->sip) {
				/* move sip instance, if we keep interface */
				found->sip_inst = interface->sip_inst;
				interface->sip_inst = NULL;
			}
#endif
			;
		}
		interface = interface->next;
	}

	interface = interface_newlist;
	while(interface) {
		found = NULL;
		temp = interface_first;
		while(temp) {
			if (!strcmp(temp->name, interface->name))
				found = temp;
			temp = temp->next;
		}
		if (!found) {
#ifdef WITH_GSM_MS
			if (interface->gsm_ms)
				gsm_ms_new(interface);
#endif
#ifdef WITH_GSM_BS
			if (interface->gsm_bs)
				gsm_bs_init(interface);
#endif
#ifdef WITH_SIP
			if (interface->sip)
				sip_init_inst(interface);
#endif
		}
		interface = interface->next;
	}

#ifdef WITH_MISDN
	/* unlink all mISDNports */
	mISDNport = mISDNport_first;
	while(mISDNport) {
		mISDNport->ifport = NULL;
		mISDNport = mISDNport->next;
	}

	/* relink existing mISDNports */
	interface = interface_newlist;
	while(interface) {
		ifport = interface->ifport;
		while(ifport) {
			mISDNport = mISDNport_first;
			while(mISDNport) {
				if (!strcmp(mISDNport->name, ifport->portname))
					ifport->portnum = mISDNport->portnum; /* same name, so we use same number */
				if (mISDNport->portnum == ifport->portnum) {
					PDEBUG(DEBUG_ISDN, "Port %d:%s relinking!\n", ifport->portnum, ifport->portname);
					ifport->mISDNport = mISDNport;
					mISDNport->ifport = ifport;
					set_mISDN_defaults(ifport);
				}
				mISDNport = mISDNport->next;
			}
			ifport = ifport->next;
		}
		interface = interface->next;
	}

	/* close unused mISDNports */
	closeagain:
	mISDNport = mISDNport_first;
	while(mISDNport) {
		if (mISDNport->ifport == NULL) {
			PDEBUG(DEBUG_ISDN, "Port %d is not used anymore and will be closed\n", mISDNport->portnum);
			/* destroy port */
			mISDNport_close(mISDNport);
			goto closeagain;
		}
		mISDNport = mISDNport->next;
	}

	/* open and link new mISDNports */
	interface = interface_newlist;
	while(interface) {
		ifport = interface->ifport;
		while(ifport) {
			if (!ifport->mISDNport) {
				if (!interface->shutdown) {
					load_mISDN_port(ifport);
				} else {
					ifport->block = 2;
				}
			}
			ifport = ifport->next;
		}
		interface = interface->next;
	}
#endif
}


#ifdef WITH_MISDN
/*
 * load port
 */
void load_mISDN_port(struct interface_port *ifport)
{
	struct mISDNport *mISDNport;

	/* open new port */
	mISDNport = mISDNport_open(ifport);
	if (mISDNport) {
		/* link port */
		ifport->mISDNport = mISDNport;
		mISDNport->ifport = ifport;
		/* set number and name */
		ifport->portnum = mISDNport->portnum;
		SCPY(ifport->portname, mISDNport->name);
		/* set defaults */
		set_mISDN_defaults(ifport);
		/* load static port instances */
		mISDNport_static(mISDNport);
	} else {
		ifport->block = 2; /* not available */
	}
}
#endif

/*
 * give summary of interface syntax
 */
void doc_interface(void)
{
	struct interface_param *ifparam;
	
	printf("Syntax overview\n");
	printf("---------------\n\n");

	printf("[<name>]\n");
	ifparam = interface_param;
	while(ifparam->name) {
		if (ifparam->name[0])
			printf("%s %s\n", ifparam->name, ifparam->usage);
		ifparam++;
	}

	ifparam = interface_param;
	while(ifparam->name) {
		if (ifparam->name[0]) {
			printf("\nParameter: %s %s\n", ifparam->name, ifparam->usage);
			printf("%s\n", ifparam->help);
		}
		ifparam++;
	}
}


/* screen caller id
 * out==0: incoming caller id, out==1: outgoing caller id
 */
void do_screen(int out, char *id, int idsize, int *type, int *present, const char *interface_name)
{
	char			*msn1;
	struct interface_msn	*ifmsn;
	struct interface_screen	*ifscreen;
	char suffix[64];
	struct interface *interface = interface_first;

	interface = getinterfacebyname(interface_name);
	if (!interface)
		return;

	/* screen incoming caller id */
	if (!out) {
		/* check for MSN numbers, use first MSN if no match */
		msn1 = NULL;
		ifmsn = interface->ifmsn;
		while(ifmsn) {
			if (!msn1)
				msn1 = ifmsn->msn;
			if (!strcmp(ifmsn->msn, id)) {
				break;
			}
			ifmsn = ifmsn->next;
		}
		if (ifmsn) {
			start_trace(-1, interface, numberrize_callerinfo(id, *type, options.national, options.international), NULL, DIRECTION_IN, 0, 0, "SCREEN (found in MSN list)");
			add_trace("msn", NULL, "%s", id);
			end_trace();
		}
		if (!ifmsn && msn1) { // not in list, first msn given
			start_trace(-1, interface, numberrize_callerinfo(id, *type, options.national, options.international), NULL, DIRECTION_IN, 0, 0, "SCREEN (not found in MSN list)");
			add_trace("msn", "given", "%s", id);
			add_trace("msn", "used", "%s", msn1);
			end_trace();
			UNCPY(id, msn1, idsize);
			id[idsize-1] = '\0';
		}
	}

	/* check screen list */
	if (out)
		ifscreen = interface->ifscreen_out;
	else
		ifscreen = interface->ifscreen_in;
	while (ifscreen) {
		if (ifscreen->match_type==-1 || ifscreen->match_type==*type)
		if (ifscreen->match_present==-1 || ifscreen->match_present==*present) {
			if (strchr(ifscreen->match,'%')) {
				if (!strncmp(ifscreen->match, id, strchr(ifscreen->match,'%')-ifscreen->match))
					break;
			} else {
				if (!strcmp(ifscreen->match, id))
					break;
			}
		}
		ifscreen = ifscreen->next;
	}
	if (ifscreen) { // match
		start_trace(-1, interface, numberrize_callerinfo(id, *type, options.national, options.international), NULL, out?DIRECTION_OUT:DIRECTION_IN, 0, 0, "SCREEN (found in screen list)");
		switch(*type) {
			case INFO_NTYPE_UNKNOWN:
			add_trace("given", "type", "unknown");
			break;
			case INFO_NTYPE_SUBSCRIBER:
			add_trace("given", "type", "subscriber");
			break;
			case INFO_NTYPE_NATIONAL:
			add_trace("given", "type", "national");
			break;
			case INFO_NTYPE_INTERNATIONAL:
			add_trace("given", "type", "international");
			break;
		}
		switch(*present) {
			case INFO_PRESENT_ALLOWED:
			add_trace("given", "present", "allowed");
			break;
			case INFO_PRESENT_RESTRICTED:
			add_trace("given", "present", "restricted");
			break;
			case INFO_PRESENT_NOTAVAIL:
			add_trace("given", "present", "not available");
			break;
		}
		add_trace("given", "id", "%s", id[0]?id:"<empty>");
		if (ifscreen->result_type != -1) {
			*type = ifscreen->result_type;
			switch(*type) {
				case INFO_NTYPE_UNKNOWN:
				add_trace("used", "type", "unknown");
				break;
				case INFO_NTYPE_SUBSCRIBER:
				add_trace("used", "type", "subscriber");
				break;
				case INFO_NTYPE_NATIONAL:
				add_trace("used", "type", "national");
				break;
				case INFO_NTYPE_INTERNATIONAL:
				add_trace("used", "type", "international");
				break;
			}
		}
		if (ifscreen->result_present != -1) {
			*present = ifscreen->result_present;
			switch(*present) {
				case INFO_PRESENT_ALLOWED:
				add_trace("used", "present", "allowed");
				break;
				case INFO_PRESENT_RESTRICTED:
				add_trace("used", "present", "restricted");
				break;
				case INFO_PRESENT_NOTAVAIL:
				add_trace("used", "present", "not available");
				break;
			}
		}
		if (strchr(ifscreen->match,'%')) {
			SCPY(suffix, strchr(ifscreen->match,'%') - ifscreen->match + id);
			UNCPY(id, ifscreen->result, idsize);
			id[idsize-1] = '\0';
			if (strchr(id,'%')) {
				*strchr(id,'%') = '\0';
				UNCAT(id, suffix, idsize);
				id[idsize-1] = '\0';
			}
		} else {
			UNCPY(id, ifscreen->result, idsize);
			id[idsize-1] = '\0';
		}
		add_trace("used", "id", "%s", id[0]?id:"<empty>");
		end_trace();
	}
}

struct interface *getinterfacebyname(const char *name)
{
	struct interface *interface = interface_first;

	while (interface) {
		if (!strcmp(interface->name, name))
			return interface;
		interface = interface->next;
	}

	return NULL;
}

