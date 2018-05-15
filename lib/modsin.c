/*
 * modsin.c
 *
 * Copyright (c) Chris Putnam 2004-2017
 *
 * Source code released under the GPL version 2
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "is_ws.h"
#include "str.h"
#include "str_conv.h"
#include "xml.h"
#include "xml_encoding.h"
#include "fields.h"
#include "name.h"
#include "reftypes.h"
#include "modstypes.h"
#include "marc.h"
#include "url.h"
#include "iso639_1.h"
#include "iso639_2.h"
#include "iso639_3.h"
#include "bibutils.h"
#include "bibformats.h"

static int modsin_readf( FILE *fp, char *buf, int bufsize, int *bufpos, str *line, str *reference, int *fcharset );
static int modsin_processf( fields *medin, char *data, char *filename, long nref, param *p );

/*****************************************************
 PUBLIC: void modsin_initparams()
*****************************************************/
void
modsin_initparams( param *p, const char *progname )
{

	p->readformat       = BIBL_MODSIN;
	p->format_opts      = 0;
	p->charsetin        = BIBL_CHARSET_UNICODE;
	p->charsetin_src    = BIBL_SRC_DEFAULT;
	p->latexin          = 0;
	p->utf8in           = 1;
	p->xmlin            = 1;
	p->nosplittitle     = 0;
	p->verbose          = 0;
	p->addcount         = 0;
	p->singlerefperfile = 0;
	p->output_raw       = BIBL_RAW_WITHMAKEREFID |
	                      BIBL_RAW_WITHCHARCONVERT;

	p->readf    = modsin_readf;
	p->processf = modsin_processf;
	p->cleanf   = NULL;
	p->typef    = NULL;
	p->convertf = NULL;
	p->all      = NULL;
	p->nall     = 0;

	slist_init( &(p->asis) );
	slist_init( &(p->corps) );

	if ( !progname ) p->progname = NULL;
	else p->progname = strdup( progname );
}

/*****************************************************
 PUBLIC: int modsin_processf()
*****************************************************/

static char modsns[]="mods";

static int
modsin_detailr( xml *node, str *value )
{
	int status = BIBL_OK;
	if ( node->value && node->value->len ) {
		if ( value->len ) str_addchar( value, ' ' );
		str_strcat( value, node->value );
		if ( str_memerr( value ) ) return BIBL_ERR_MEMERR;
	}
	if ( node->down ) {
		status = modsin_detailr( node->down, value );
		if ( status!=BIBL_OK ) return status;
	}
	if ( node->next )
		status = modsin_detailr( node->next, value );
	return status;
}

static int
modsin_detail( xml *node, fields *info, int level )
{
	str type, value, *tp;
	int fstatus, status = BIBL_OK;
	if ( node->down ) {
		strs_init( &type, &value, NULL );
		tp = xml_getattrib( node, "type" );
		if ( tp ) {
			str_strcpy( &type, tp );
			str_toupper( &type );
			if ( str_memerr( &type ) ) goto out;
		}
		status = modsin_detailr( node->down, &value );
		if ( status!=BIBL_OK ) goto out;
		if ( type.data && !strcasecmp( type.data, "PAGE" ) ) {
			fstatus = fields_add( info, "PAGES:START", value.data, level );
		} else {
			fstatus = fields_add( info, type.data, value.data, level );
		}
		if ( fstatus!=FIELDS_OK ) status = BIBL_ERR_MEMERR;
out:
		strs_free( &type, &value, NULL );
	}
	return status;
}

static int
modsin_date( xml *node, fields *info, int level, int part )
{
	int fstatus, status = BIBL_OK;
	char *tag, *p = NULL;
	str s;
	if ( node->value ) p = node->value->data;
	if ( p ) {
		str_init( &s );

		p = str_cpytodelim( &s, skip_ws( p ), "-", 1 );
		if ( str_memerr( &s ) ) { status = BIBL_ERR_MEMERR; goto out; }
		if ( str_has_value( &s ) ) {
			tag = ( part ) ? "PARTDATE:YEAR" : "DATE:YEAR";
			fstatus =  fields_add( info, tag, str_cstr( &s ), level );
			if ( fstatus!=FIELDS_OK ) { status = BIBL_ERR_MEMERR; goto out; }
		}

		p = str_cpytodelim( &s, skip_ws( p ), "-", 1 );
		if ( str_memerr( &s ) ) { status = BIBL_ERR_MEMERR; goto out; }
		if ( str_has_value( &s ) ) {
			tag = ( part ) ? "PARTDATE:MONTH" : "DATE:MONTH";
			fstatus =  fields_add( info, tag, str_cstr( &s ), level );
			if ( fstatus!=FIELDS_OK ) { status = BIBL_ERR_MEMERR; goto out; }
		}

		p = str_cpytodelim( &s, skip_ws( p ), "", 0 );
		if ( str_memerr( &s ) ) { status = BIBL_ERR_MEMERR; goto out; }
		if ( str_has_value( &s ) ) {
			tag = ( part ) ? "PARTDATE:DAY" : "DATE:DAY";
			fstatus =  fields_add( info, tag, str_cstr( &s ), level );
			if ( fstatus!=FIELDS_OK ) { status = BIBL_ERR_MEMERR; goto out; }
		}
out:
		str_free( &s );
	}
	return status;
}

static int
modsin_pager( xml *node, str *sp, str *ep, str *tp, str *lp )
{
	int status = BIBL_OK;
	if ( xml_tagexact( node, "start" ) ) {
		str_strcpy( sp, node->value );
		if ( str_memerr( sp ) ) return BIBL_ERR_MEMERR;
	} else if ( xml_tagexact( node, "end" ) ) {
		str_strcpy( ep, node->value );
		if ( str_memerr( ep ) ) return BIBL_ERR_MEMERR;
	} else if ( xml_tagexact( node, "total" ) ) {
		str_strcpy( tp, node->value );
		if ( str_memerr( tp ) ) return BIBL_ERR_MEMERR;
	} else if ( xml_tagexact( node, "list" ) ) {
		str_strcpy( lp, node->value );
		if ( str_memerr( lp ) ) return BIBL_ERR_MEMERR;
	}
	if ( node->down ) {
		status = modsin_pager( node->down, sp, ep, tp, lp );
		if ( status!=BIBL_OK ) return status;
	}
	if ( node->next )
		status = modsin_pager( node->next, sp, ep, tp, lp );
	return status;
}

static int
modsin_page( xml *node, fields *info, int level )
{
	int fstatus, status = BIBL_OK;
	str sp, ep, tp, lp;
	xml *dnode = node->down;

	if ( !dnode ) return BIBL_OK;

	strs_init( &sp, &ep, &tp, &lp, NULL );

	status = modsin_pager( dnode, &sp, &ep, &tp, &lp );
	if ( status!=BIBL_OK ) goto out;

	if ( str_has_value( &sp ) || str_has_value( &ep ) ) {
		if ( str_has_value( &sp ) ) {
			fstatus = fields_add( info, "PAGES:START", str_cstr( &sp ), level );
			if ( fstatus!=FIELDS_OK ) { status = BIBL_ERR_MEMERR; goto out; }
		}
		if ( str_has_value( &ep ) ) {
			fstatus = fields_add( info, "PAGES:STOP", str_cstr( &ep ), level );
			if ( fstatus!=FIELDS_OK ) { status = BIBL_ERR_MEMERR; goto out; }
		}
	} else if ( str_has_value( &lp ) ) {
		fstatus = fields_add( info, "PAGES:START", str_cstr( &lp ), level );
		if ( fstatus!=FIELDS_OK ) { status = BIBL_ERR_MEMERR; goto out; }
	}
	if ( str_has_value( &tp ) ) {
		fstatus = fields_add( info, "PAGES:TOTAL", str_cstr( &tp ), level );
		if ( fstatus!=FIELDS_OK ) { status = BIBL_ERR_MEMERR; goto out; }
	}
out:
	strs_free( &sp, &ep, &tp, &lp, NULL );
	return status;
}

static int
modsin_titler( xml *node, str *title, str *subtitle )
{
	int status = BIBL_OK;
	if ( xml_tagexact( node, "title" ) ) {
		if ( str_has_value( title ) ) str_strcatc( title, " : " );
		str_strcat( title, node->value );
		if ( str_memerr( title ) ) return BIBL_ERR_MEMERR;
	} else if ( xml_tagexact( node, "subTitle" ) ) {
		str_strcat( subtitle, node->value );
		if ( str_memerr( subtitle ) ) return BIBL_ERR_MEMERR;
	}
	if ( node->down ) {
		status = modsin_titler( node->down, title, subtitle );
		if ( status!=BIBL_OK ) return status;
	}
	if ( node->next )
		status = modsin_titler( node->next, title, subtitle );
	return status;
}

static int
modsin_title( xml *node, fields *info, int level )
{
	char *titletag[2][2] = {
		{ "TITLE",    "SHORTTITLE" },
		{ "SUBTITLE", "SHORTSUBTITLE" },
	};
	int fstatus, status = BIBL_OK;
	str title, subtitle;
	xml *dnode;
	int abbr;

	dnode = node->down;
	if ( !dnode ) return status;

	strs_init( &title, &subtitle, NULL );
	abbr = xml_tag_attrib( node, "titleInfo", "type", "abbreviated" );

	status = modsin_titler( dnode, &title, &subtitle );
	if ( status!=BIBL_OK ) goto out;

	if ( str_has_value( &title ) ) {
		fstatus = fields_add( info, titletag[0][abbr], str_cstr( &title ), level );
		if ( fstatus!=FIELDS_OK ) { status = BIBL_ERR_MEMERR; goto out; }
	}

	if ( str_has_value( &subtitle ) ) {
		fstatus = fields_add( info, titletag[1][abbr], str_cstr( &subtitle ), level );
		if ( fstatus!=FIELDS_OK ) { status = BIBL_ERR_MEMERR; goto out; }
	}

out:
	strs_free( &title, &subtitle, NULL );
	return status;
}

/* modsin_marcrole_convert()
 *
 * Map MARC-authority roles for people or organizations associated
 * with a reference to internal roles.
 *
 * Take input strings with roles separated by '|' characters, e.g.
 * "author" or "author|creator" or "edt" or "editor|edt".
 */
static int
modsin_marcrole_convert( str *s, char *suffix, str *out )
{
	convert roles[] = {
		{ "author",              "AUTHOR",        0, 0 },
		{ "aut",                 "AUTHOR",        0, 0 },
		{ "aud",                 "AUTHOR",        0, 0 },
		{ "aui",                 "AUTHOR",        0, 0 },
		{ "aus",                 "AUTHOR",        0, 0 },
		{ "creator",             "AUTHOR",        0, 0 },
		{ "cre",                 "AUTHOR",        0, 0 },
		{ "editor",              "EDITOR",        0, 0 },
		{ "edt",                 "EDITOR",        0, 0 },
		{ "degree grantor",      "DEGREEGRANTOR", 0, 0 },
		{ "dgg",                 "DEGREEGRANTOR", 0, 0 },
		{ "organizer of meeting","ORGANIZER",     0, 0 },
		{ "orm",                 "ORGANIZER",     0, 0 },
		{ "patent holder",       "ASSIGNEE",      0, 0 },
		{ "pth",                 "ASSIGNEE",      0, 0 },
		{ "pbl",                 "PUBLISHER",      0, 0 } /* added to support KTH DiVA - to support the role of publisher of a thesis */
	};
	int nroles = sizeof( roles ) / sizeof( roles[0] );
	int i, nmismatch, n = -1, status = BIBL_OK;
	char *p, *q;
	fprintf( stderr, "GQMJr::modsin_marcrole_convert string=%s, suffix=%s\n", s->data, suffix ); /* added to debug KTH DiVA */
	if ( s->len == 0 ) {
		/* ...default to author on an empty string */
		n = 0;
	} else {

		/* ...find first match in '|'-separated list */
		for ( i=0; i<nroles && n==-1; ++i ) {
			p = s->data;
			while ( *p ) {
				q = roles[i].mods;
				nmismatch = 0;
				while ( *p && *p!='|' && nmismatch == 0) {
					if ( toupper( (unsigned char)*p ) != toupper( (unsigned char)*q ) )
						nmismatch++;
					p++;
					q++;
				}
				if ( !nmismatch && !(*(q++))) n = i;
				if ( *p=='|' ) p++;
			}
		}
	}

	if ( n!=-1 ) {
		str_strcpyc( out, roles[n].internal );
		if ( suffix ) str_strcatc( out, suffix );
	} else {
		str_strcpy( out, s );
	}
	if ( str_memerr( out ) ) status = BIBL_ERR_MEMERR;
	return status;
}

static int
modsin_asis_corp_r( xml *node, str *name, str *role )
{
	int status = BIBL_OK;
	if ( xml_tagexact( node, "namePart" ) ) {
	  // str_strcpy( name, node->value );
	  if ( str_has_value( name ) ) str_strcatc( name, ", " );  /* added to support KTH DiVA - accumulate multipart names */
		
	  str_strcat( name, node->value );

		if ( str_memerr( name ) ) return BIBL_ERR_MEMERR;
		fprintf( stderr, "GQMJr::modsin_asis_corp_r string=%s\n", str_cstr(name) ); /* added to debug KTH DiVA */
	} else if ( xml_tagexact( node, "roleTerm" ) ) {
		if ( role->len ) str_addchar( role, '|' );
		str_strcat( role, node->value );
		if ( str_memerr( role ) ) return BIBL_ERR_MEMERR;
	}
	if ( node->down ) {
		status = modsin_asis_corp_r( node->down, name, role );
		if ( status!=BIBL_OK ) return status;
	}
	if ( node->next )
		status = modsin_asis_corp_r( node->next, name, role );
	return status;
}

static int
modsin_asis_corp( xml *node, fields *info, int level, char *suffix )
{
	int fstatus, status = BIBL_OK;
	str name, roles, role_out;
	xml *dnode = node->down;
	if ( dnode ) {
		strs_init( &name, &roles, &role_out, NULL );
		status = modsin_asis_corp_r( dnode, &name, &roles );
		if ( status!=BIBL_OK ) goto out;
		status = modsin_marcrole_convert( &roles, suffix, &role_out );
		if ( status!=BIBL_OK ) goto out;
		fstatus = fields_add( info, str_cstr( &role_out ), str_cstr( &name ), level );
		if ( fstatus!=FIELDS_OK ) status = BIBL_ERR_MEMERR;
out:
		strs_free( &name, &roles, &role_out, NULL );
	}
	return status;
}

/* event processing added to support KTH DiVA entries for <name type="conference">
<namePart>xxxx
</namePart>
</name>
*/

static int
modsin_event_r( xml *node, str *name, str *role )
{
	int status = BIBL_OK;
	if ( xml_tagexact( node, "namePart" ) ) {
		str_strcpy( name, node->value );
		if ( str_memerr( name ) ) return BIBL_ERR_MEMERR;
	} else if ( xml_tagexact( node, "roleTerm" ) ) {
		if ( role->len ) str_addchar( role, '|' );
		str_strcat( role, node->value );
		if ( str_memerr( role ) ) return BIBL_ERR_MEMERR;
	}
	if ( node->down ) {
		status = modsin_event_r( node->down, name, role );
		if ( status!=BIBL_OK ) return status;
	}
	if ( node->next )
		status = modsin_event_r( node->next, name, role );
	return status;
}

static int
modsin_event( xml *node, fields *info, int level)
{
	int fstatus, status = BIBL_OK;
	str name, roles, role_out;
	xml *dnode = node->down;
	if ( dnode ) {
		strs_init( &name, &roles, &role_out, NULL );
		status = modsin_event_r( dnode, &name, &roles );
		if ( status!=BIBL_OK ) goto out;

		fstatus = fields_add( info, "EVENT", str_cstr( &name ), level );
		if ( fstatus!=FIELDS_OK ) status = BIBL_ERR_MEMERR;
out:
		strs_free( &name, &roles, &role_out, NULL );
	}
	return status;
}

static int
modsin_roler( xml *node, str *roles )
{
	if ( roles->len ) str_addchar( roles, '|' );
	str_strcat( roles, node->value );
	if ( str_memerr( roles ) ) return BIBL_ERR_MEMERR;
	else return BIBL_OK;
}

static int
modsin_personr( xml *node, str *familyname, str *givenname, str *suffix )
{
	int status = BIBL_OK;

	if ( xml_tag_attrib( node, "namePart", "type", "family" ) ) {
		if ( str_has_value( familyname ) ) str_addchar( familyname, ' ' );
		str_strcat( familyname, node->value );
		if ( str_memerr( familyname ) ) status = BIBL_ERR_MEMERR;
	}

	else if ( xml_tag_attrib( node, "namePart", "type", "suffix") ||
	          xml_tag_attrib( node, "namePart", "type", "termsOfAddress" )) {
		if ( str_has_value( suffix ) ) str_addchar( suffix, ' ' );
		str_strcat( suffix, node->value );
		if ( str_memerr( suffix ) ) status = BIBL_ERR_MEMERR;
	}

	else if (xml_tag_attrib( node, "namePart", "type", "date") ){
		/* no nothing */
	}

	else {
		if ( str_has_value( givenname ) ) str_addchar( givenname, '|' );
		str_strcat( givenname, node->value );
		if ( str_memerr( givenname ) ) status = BIBL_ERR_MEMERR;
	}

	return status;
}

static int
modsin_person( xml *node, fields *info, int level )
{
	str familyname, givenname, name, suffix, roles, role_out;
	int fstatus, status = BIBL_OK;
	xml *dnode, *rnode;

	dnode = node->down;
	if ( !dnode ) return status;

	strs_init( &name, &familyname, &givenname, &suffix, &roles, &role_out, NULL );

	while ( dnode ) {

		if ( xml_tagexact( dnode, "namePart" ) ) {
			status = modsin_personr( dnode, &familyname, &givenname, &suffix );
			if ( status!=BIBL_OK ) goto out;
		}

		else if ( xml_tagexact( dnode, "role" ) ) {
			rnode = dnode->down;
			while ( rnode ) {
				if ( xml_tagexact( rnode, "roleTerm" ) ) {
					status = modsin_roler( rnode, &roles );
					if ( status!=BIBL_OK ) goto out;
				}
				rnode = rnode->next;
			}
		}

		dnode = dnode->next;

	}

	/*
	 * Handle:
	 *          <namePart type='given'>Noah A.</namePart>
	 *          <namePart type='family'>Smith</namePart>
	 * without mangling the order of "Noah A."
	 */
	if ( str_has_value( &familyname ) ) {
		str_strcpy( &name, &familyname );
		if ( givenname.len ) {
			str_addchar( &name, '|' );
			str_strcat( &name, &givenname );
		}
	}

	/*
	 * Handle:
	 *          <namePart>Noah A. Smith</namePart>
	 * with name order mangling.
	 */
	else {
		if ( str_has_value( &givenname ) )
			name_parse( &name, &givenname, NULL, NULL );
	}

	if ( str_has_value( &suffix ) ) {
		str_strcatc( &name, "||" );
		str_strcat( &name, &suffix );
	}

	if ( str_memerr( &name ) ) {
		status=BIBL_ERR_MEMERR;
		goto out;
	}

	status = modsin_marcrole_convert( &roles, NULL, &role_out );
	if ( status!=BIBL_OK ) goto out;

	fstatus = fields_add_can_dup( info, str_cstr( &role_out ), str_cstr( &name ), level );
	if ( fstatus!=FIELDS_OK ) status = BIBL_ERR_MEMERR;

out:
	strs_free( &name, &familyname, &givenname, &suffix, &roles, &role_out, NULL );
	return status;
}

static int
modsin_placeterm_text( xml *node, fields *info, int level, int school )
{
	char address_tag[] = "ADDRESS";
	char school_tag[]  = "SCHOOL";
	char *tag;
	int fstatus;

	tag = ( school ) ? school_tag : address_tag;

	fstatus = fields_add( info, tag, xml_value( node ), level );
	if ( fstatus!=FIELDS_OK ) return BIBL_ERR_MEMERR;

	return BIBL_OK;
}

static int
modsin_placeterm_code( xml *node, fields *info, int level )
{
	int fstatus, status = BIBL_OK;
	str s, *auth;

	str_init( &s );

	auth = xml_getattrib( node, "authority" );
	if ( auth && auth->len ) {
		str_strcpy( &s, auth );
		str_addchar( &s, '|' );
	}
	str_strcat( &s, node->value );

	if ( str_memerr( &s ) ) {
		status = BIBL_ERR_MEMERR;
		goto out;
	}

	fstatus = fields_add( info, "CODEDADDRESS", str_cstr( &s ), level );
	if ( fstatus!=FIELDS_OK ) status = BIBL_ERR_MEMERR;
out:
	str_free( &s );
	return status;
}

static int
modsin_placeterm( xml *node, fields *info, int level, int school )
{
	int status = BIBL_OK;
	str *type;

	type = xml_getattrib( node, "type" );
	if ( str_has_value( type ) ) {
		if ( !strcmp( str_cstr( type ), "text" ) )
			status = modsin_placeterm_text( node, info, level, school );
		else if ( !strcmp( str_cstr( type ), "code" ) )
			status = modsin_placeterm_code( node, info, level );
	}

	return status;
}

static int
modsin_placer( xml *node, fields *info, int level, int school )
{
	int status = BIBL_OK;
	if ( xml_tag_attrib( node, "place", "type", "school" ) ) {
		school = 1;
	} else if ( xml_tagexact( node, "placeTerm" ) ) {
		status = modsin_placeterm( node, info, level, school );
	}
	if ( node->down ) {
		status = modsin_placer( node->down, info, level, school );
		if ( status!=BIBL_OK ) return status;
	}
	if ( node->next ) status = modsin_placer( node->next, info, level, school );
	return status;
}

static int
modsin_origininfor( xml *node, fields *info, int level, str *pub, str *add, str *addc, str *ed, str *iss )
{
	int status = BIBL_OK;
	if ( xml_tagexact( node, "dateIssued" ) )
		status = modsin_date( node, info, level, 0 );
	else if ( xml_tagexact( node, "publisher" ) && xml_hasvalue( node ) ) {
		str_strcat( pub, node->value );
		if ( str_memerr( pub ) ) return BIBL_ERR_MEMERR;
	} else if ( xml_tagexact( node, "edition" ) && xml_hasvalue( node ) ) {
		str_strcat( ed, node->value );
		if( str_memerr( ed ) ) return BIBL_ERR_MEMERR;
	} else if ( xml_tagexact( node, "issuance" ) && xml_hasvalue( node ) ) {
		str_strcat( iss, node->value );
		if ( str_memerr( iss ) ) return BIBL_ERR_MEMERR;
	} else if ( xml_tagexact( node, "place" ) && xml_hasvalue( node ) )
		status = modsin_placer( node, info, level, 0 );
	if ( status!=BIBL_OK ) return status;
	if ( node->down ) {
		status = modsin_origininfor( node->down, info, level, pub, add, addc, ed, iss );
		if ( status!=BIBL_OK ) return status;
	}
	if ( node->next )
		status = modsin_origininfor( node->next, info, level, pub, add, addc, ed, iss );
	return status;
}

static int
modsin_origininfo( xml *node, fields *info, int level )
{
	str publisher, address, addcode, edition, issuance;
	int fstatus, status = BIBL_OK;
	if ( node->down ) {
		strs_init( &publisher, &address, &addcode, &edition, &issuance, NULL );
		status = modsin_origininfor( node->down, info, level, &publisher, 
				&address, &addcode, &edition, &issuance );
		if ( status!=BIBL_OK ) goto out;
		if ( str_has_value( &publisher ) ) {
			fstatus = fields_add( info, "PUBLISHER", str_cstr( &publisher ), level );
			if ( fstatus!=FIELDS_OK ) { status=BIBL_ERR_MEMERR; goto out; }
		}
		if ( str_has_value( &address ) ) {
			fstatus = fields_add( info, "ADDRESS", str_cstr( &address ), level );
			if ( fstatus!=FIELDS_OK ) { status=BIBL_ERR_MEMERR; goto out; }
		}
		if ( str_has_value( &addcode ) ) {
			fstatus = fields_add( info, "CODEDADDRESS", str_cstr( &addcode ), level );
			if ( fstatus!=FIELDS_OK ) { status=BIBL_ERR_MEMERR; goto out; }
		}
		if ( str_has_value( &edition ) ) {
			fstatus = fields_add( info, "EDITION", str_cstr( &edition ), level );
			if ( fstatus!=FIELDS_OK ) { status=BIBL_ERR_MEMERR; goto out; }
		}
		if ( str_has_value( &issuance ) ) {
			fstatus = fields_add( info, "ISSUANCE", str_cstr( &issuance ), level );
			if ( fstatus!=FIELDS_OK ) { status=BIBL_ERR_MEMERR; goto out; }
		}
out:
		strs_free( &publisher, &address, &addcode, &edition, &issuance, NULL );
	}
	return status;
}

static int
modsin_subjectr( xml *node, fields *info, int level )
{
	int fstatus, status = BIBL_OK;
	if ( xml_tag_attrib( node, "topic", "class", "primary" ) ) {
		fstatus = fields_add( info, "EPRINTCLASS", node->value->data, level );
		if ( fstatus!=FIELDS_OK ) return BIBL_ERR_MEMERR;
	}
	else if ( xml_tagexact( node, "topic" ) || xml_tagexact( node, "geographic" )) {
		fstatus = fields_add( info, "KEYWORD", node->value->data, level );
		if ( fstatus!=FIELDS_OK ) return BIBL_ERR_MEMERR;
	}
	if ( node->down ) {
		status = modsin_subjectr( node->down, info, level );
		if ( status!=BIBL_OK ) return status;
	}
	if ( node->next ) status = modsin_subjectr( node->next, info, level );
	return status;
}

static int
modsin_subject( xml *node, fields *info, int level )
{
	int status = BIBL_OK;
	if ( node->down ) status = modsin_subjectr( node->down, info, level );
	return status;
}

static int
modsin_id1( xml *node, fields *info, int level )
{
	int fstatus;
	str *ns;
	ns = xml_getattrib( node, "ID" );
	if ( str_has_value( ns ) ) {
		fstatus = fields_add( info, "REFNUM", str_cstr( ns ), level );
		if ( fstatus!=FIELDS_OK ) return BIBL_ERR_MEMERR;
	}
	return BIBL_OK;
}

static int
modsin_genre( xml *node, fields *info, int level )
{
	char *added[] = { "manuscript", "academic journal", "magazine",
		"hearing", "report", "Ph.D. thesis", "Masters thesis",
		"Diploma thesis", "Doctoral thesis", "Habilitation thesis",
		"collection", "handwritten note", "communication",
		"teletype", "airtel", "memo", "e-mail communication",
		"press release", "television broadcast", "electronic"
	};
	int nadded = sizeof( added ) /sizeof( char *);
	int i, ismarc = 0, isadded = 0, fstatus;
	char *d;

	if ( !xml_hasvalue( node ) ) return BIBL_OK;
	d = xml_value( node );
	if ( marc_findgenre( d )!=-1 ) ismarc = 1;
	if ( !ismarc ) {
		for ( i=0; i<nadded && ismarc==0 && isadded==0; ++i )
			if ( !strcasecmp( d, added[i] ) ) isadded = 1;
	}

	if ( ismarc || isadded ) 
		fstatus = fields_add( info, "GENRE", d, level );
	else
		fstatus = fields_add( info, "NGENRE", d, level );
	if ( fstatus!=FIELDS_OK ) return BIBL_ERR_MEMERR;

	return BIBL_OK;
}

/* in MODS version 3.5
 * <languageTerm type="text">....</languageTerm>
 * <languageTerm type="code" authority="xxx">...</languageTerm>
 * xxx = rfc3066
 * xxx = iso639-2b
 * xxx = iso639-3
 * xxx = rfc4646
 * xxx = rfc5646
 */
static int
modsin_languager( xml *node, fields *info, int level )
{
	int fstatus, status = BIBL_OK;
	char *d = NULL;
	if ( xml_tagexact( node, "languageTerm" ) ) {
		if ( xml_hasvalue( node ) ) {
			if ( xml_hasattrib( node, "type", "code" ) ) {
				if ( xml_hasattrib( node, "authority", "iso639-1" ) )
					d = iso639_1_from_code( xml_value( node ) );
				else if ( xml_hasattrib( node, "authority", "iso639-2b" ) )
					d = iso639_2_from_code( xml_value( node ) );
				else if ( xml_hasattrib( node, "authority", "iso639-3" ))
					d = iso639_3_from_code( xml_value( node ) );
			}
			if ( !d ) d  = xml_value( node );
			fstatus = fields_add( info, "LANGUAGE", d, level );
			if ( fstatus!=FIELDS_OK ) return BIBL_ERR_MEMERR;
		}
	}
	if ( node->next ) status = modsin_languager( node->next, info, level );
	return status;
}

static int
modsin_language( xml *node, fields *info, int level )
{
	int fstatus, status = BIBL_OK;
	/* Old versions of MODS had <language>English</language> */
	if ( xml_hasvalue( node ) ) {
		fstatus = fields_add( info, "LANGUAGE", xml_value( node ), level );
		if ( fstatus!=FIELDS_OK ) return BIBL_ERR_MEMERR;
	}

	/* New versions of MODS have <language><languageTerm>English</languageTerm></language> */
	if ( node->down ) status = modsin_languager( node->down, info, level );
	return status;
}

static int
modsin_simple( xml *node, fields *info, char *tag, int level )
{
	int fstatus;
	if ( xml_hasvalue( node ) ) {
		fstatus = fields_add( info, tag, xml_value( node ), level );
		if ( fstatus!=FIELDS_OK ) return BIBL_ERR_MEMERR;
	}
	return BIBL_OK;
}

static int
modsin_locationr( xml *node, fields *info, int level )
{
	int fstatus, status = BIBL_OK;
	char *url        = "URL";
	char *fileattach = "FILEATTACH";
	char *tag=NULL;

	if ( xml_tagexact( node, "url" ) ) {
		if ( xml_hasattrib( node, "access", "raw object" ) )
			tag = fileattach;
		else
			tag = url;
	}
	else if ( xml_tagexact( node, "physicalLocation" ) ) {
		if ( xml_hasattrib( node, "type", "school" ) )
			tag = "SCHOOL";
		else
			tag = "LOCATION";
	}

	if ( tag == url ) {
		status = urls_split_and_add( xml_value( node ), info, level );
		if ( status!=BIBL_OK ) return status;
	}
	else if ( tag ) {
		fstatus = fields_add( info, tag, xml_value( node ), level );
		if ( fstatus!=FIELDS_OK ) return BIBL_ERR_MEMERR;
	}

	if ( node->down ) {
		status = modsin_locationr( node->down, info, level );
		if ( status!=BIBL_OK ) return status;
	}
	if ( node->next ) status = modsin_locationr( node->next, info, level );
	return status;
}

static int
modsin_location( xml *node, fields *info, int level )
{
	int status = BIBL_OK;
	if ( node->down ) status = modsin_locationr( node->down, info, level );
	return status;
}

static int
modsin_descriptionr( xml *node, str *s )
{
	int status = BIBL_OK;
	if ( xml_tagexact( node, "extent" ) ||
	     xml_tagexact( node, "note" ) ) {
		str_strcpy( s, node->value );
		if ( str_memerr( s ) ) return BIBL_ERR_MEMERR;
	}
	if ( node->down ) {
		status = modsin_descriptionr( node->down, s );
		if ( status!=BIBL_OK ) return status;
	}
	if ( node->next ) status = modsin_descriptionr( node->next, s );
	return status;
}

static int
modsin_description( xml *node, fields *info, int level )
{
	int fstatus, status = BIBL_OK;
	str s;
	str_init( &s );
	if ( node->down ) {
		status = modsin_descriptionr( node->down, &s );
		if ( status!=BIBL_OK ) goto out;
	} else {
		if ( node->value && node->value->len > 0 )
			str_strcpy( &s, node->value );
		if ( str_memerr( &s ) ) {
			status = BIBL_ERR_MEMERR;
			goto out;
		}
	}
	if ( str_has_value( &s ) ) {
		fstatus = fields_add( info, "DESCRIPTION", str_cstr( &s ), level );
		if ( fstatus!=FIELDS_OK ) {
			status = BIBL_ERR_MEMERR;
			goto out;
		}
	}
out:
	str_free( &s );
	return status;
}

static int
modsin_partr( xml *node, fields *info, int level )
{
	int status = BIBL_OK;
	if ( xml_tagexact( node, "detail" ) )
		status = modsin_detail( node, info, level );
	else if ( xml_tag_attrib( node, "extent", "unit", "page" ) )
		status = modsin_page( node, info, level );
	else if ( xml_tag_attrib( node, "extent", "unit", "pages" ) )
		status = modsin_page( node, info, level );
	else if ( xml_tagexact( node, "date" ) )
		status = modsin_date( node, info, level, 1 );
	if ( status!=BIBL_OK ) return status;
	if ( node->next ) status = modsin_partr( node->next, info, level );
	return status;
}

static int
modsin_part( xml *node, fields *info, int level )
{
	if ( node->down ) return modsin_partr( node->down, info, level );
	return BIBL_OK;
}

/* <classification authority="lcc">Q3 .A65</classification> */
static int
modsin_classification( xml *node, fields *info, int level )
{
	int fstatus, status = BIBL_OK;
	char *tag, *d;
	if ( xml_hasvalue( node ) ) {
		d = xml_value( node );
		if ( xml_tag_attrib( node, "classification", "authority", "lcc" ) )
			tag = "LCC";
		else
			tag = "CLASSIFICATION";
		fstatus = fields_add( info, tag, d, level );
		if ( fstatus!=FIELDS_OK ) return BIBL_ERR_MEMERR;
	}
	if ( node->down ) status = modsin_classification( node->down, info, level );
	return status;
}

static int
modsin_recordinfo( xml *node, fields *info, int level )
{
	int fstatus;
	xml *curr;
	char *d;

	/* extract recordIdentifier */
	curr = node;
	while ( curr ) {
		if ( xml_tagexact( curr, "recordIdentifier" ) && xml_hasvalue( curr ) ) {
			d = xml_value( curr );
			fstatus = fields_add( info, "REFNUM", d, level );
			if ( fstatus!=FIELDS_OK ) return BIBL_ERR_MEMERR;
		}
		curr = curr->next;
	}
	return BIBL_OK;
}

static int
modsin_identifier( xml *node, fields *info, int level )
{
	convert ids[] = {
		{ "citekey",       "REFNUM",      0, 0 },
		{ "issn",          "ISSN",        0, 0 },
		{ "coden",         "CODEN",       0, 0 },
		{ "isbn",          "ISBN",        0, 0 },
		{ "doi",           "DOI",         0, 0 },
		{ "url",           "URL",         0, 0 },
		{ "uri",           "URL",         0, 0 },
		{ "pmid",          "PMID",        0, 0 },
		{ "pubmed",        "PMID",        0, 0 },
		{ "medline",       "MEDLINE",     0, 0 },
		{ "pmc",           "PMC",         0, 0 },
		{ "arXiv",         "ARXIV",       0, 0 },
		{ "MRnumber",      "MRNUMBER",    0, 0 },
		{ "pii",           "PII",         0, 0 },
		{ "isi",           "ISIREFNUM",   0, 0 },
		{ "serial number", "SERIALNUMBER",0, 0 },
		{ "accessnum",     "ACCESSNUM",   0, 0 },
		{ "jstor",         "JSTOR",       0, 0 },
		{ "issue number",  "NUMBER",      0, 0 },
	};
	int i, fstatus, n = sizeof( ids ) / sizeof( ids[0] );
	if ( !node->value || node->value->len==0 ) return BIBL_OK;
	for ( i=0; i<n; ++i ) {
		if ( xml_tag_attrib( node, "identifier", "type", ids[i].mods ) ) {
			fstatus = fields_add( info, ids[i].internal, node->value->data, level );
			if ( fstatus!=FIELDS_OK ) return BIBL_ERR_MEMERR;
		}
	}
	return BIBL_OK;
}

static int
modsin_mods( xml *node, fields *info, int level )
{
	convert simple[] = {
		{ "note",            "NOTES",    0, 0 },
		{ "abstract",        "ABSTRACT", 0, 0 },
		{ "bibtex-annote",   "ANNOTE",   0, 0 },
		{ "typeOfResource",  "RESOURCE", 0, 0 },
		{ "tableOfContents", "CONTENTS", 0, 0 },
	};
	int nsimple = sizeof( simple ) / sizeof( simple[0] );
	int i, found = 0, status = BIBL_OK;

	for ( i=0; i<nsimple && found==0; i++ ) {
		if ( xml_tagexact( node, simple[i].mods ) ) {
			status = modsin_simple( node, info, simple[i].internal, level );
			if ( status!=BIBL_OK ) return status;
			found = 1;
		}
	}

	if ( !found ) {
		if ( xml_tagexact( node, "titleInfo" ) )
			modsin_title( node, info, level );
		else if ( xml_tag_attrib( node, "name", "type", "personal" ) )
			status = modsin_person( node, info, level );
		else if ( xml_tag_attrib( node, "name", "type", "corporate" ) )
			status = modsin_asis_corp( node, info, level, ":CORP" );
		else if ( xml_tag_attrib( node, "name", "type", "conference" ) )
			status = modsin_event( node, info, level);  /* added to support KTH DiVA - specifically: conference name */
		else if ( xml_tagexact( node, "name" ) )
			status = modsin_asis_corp( node, info, level, ":ASIS" );
		else if ( xml_tagexact( node, "recordInfo" ) && node->down )
			status = modsin_recordinfo( node->down, info, level );
		else if  ( xml_tagexact( node, "part" ) )
			modsin_part( node, info, level );
		else if ( xml_tagexact( node, "identifier" ) )
			status = modsin_identifier( node, info, level );
		else if ( xml_tagexact( node, "originInfo" ) )
			status = modsin_origininfo( node, info, level );
		else if ( xml_tagexact( node, "language" ) )
			status = modsin_language( node, info, level );
		else if ( xml_tagexact( node, "genre" ) )
			status = modsin_genre( node, info, level );
		else if ( xml_tagexact( node, "date" ) )
			status = modsin_date( node, info, level, 0 );
		else if ( xml_tagexact( node, "subject" ) )
			status = modsin_subject( node, info, level );
		else if ( xml_tagexact( node, "classification" ) )
			status = modsin_classification( node, info, level );
		else if ( xml_tagexact( node, "location" ) )
			status = modsin_location( node, info, level );
		else if ( xml_tagexact( node, "physicalDescription" ) )
			status = modsin_description( node, info, level );
		else if ( xml_tag_attrib( node, "relatedItem", "type", "host" ) ||
			  xml_tag_attrib( node, "relatedItem", "type", "series" ) ) {
			if ( node->down ) status = modsin_mods( node->down, info, level+1 );
		}
		else if ( xml_tag_attrib( node, "relatedItem", "type", "original" ) ) {
			if ( node->down ) status = modsin_mods( node->down, info, LEVEL_ORIG );
		}

		if ( status!=BIBL_OK ) return status;
	}

	if ( node->next ) status = modsin_mods( node->next, info, level );

	return status;
}

static int
modsin_assembleref( xml *node, fields *info )
{
	int status = BIBL_OK;
	if ( xml_tagexact( node, "mods" ) ) {
		status = modsin_id1( node, info, 0 );
		if ( status!=BIBL_OK ) return status;
		if ( node->down ) {
			status = modsin_mods( node->down, info, 0 );
			if ( status!=BIBL_OK ) return status;
		}
	} else if ( node->down ) {
		status = modsin_assembleref( node->down, info );
		if ( status!=BIBL_OK ) return status;
	}
	if ( node->next ) status = modsin_assembleref( node->next, info );
	return status;
}

static int
modsin_processf( fields *modsin, char *data, char *filename, long nref, param *p )
{
	int status;
	xml top;

	xml_init( &top );
	xml_tree( data, &top );
	status = modsin_assembleref( &top, modsin );
	xml_free( &top );

	if ( status==BIBL_OK ) return 1;
	else return 0;
}

/*****************************************************
 PUBLIC: int modsin_readf()
*****************************************************/

static char *
modsin_startptr( char *p )
{
	char *startptr;
	startptr = xml_findstart( p, "mods:mods" );
	if ( startptr ) {
		/* set namespace if found */
		xml_pns = modsns;
	} else {
		startptr = xml_findstart( p, "mods" );
		if ( startptr ) xml_pns = NULL;
	}
	return startptr;
}

static char *
modsin_endptr( char *p )
{
	return xml_findend( p, "mods" );
}

static int
modsin_readf( FILE *fp, char *buf, int bufsize, int *bufpos, str *line, str *reference, int *fcharset )
{
	str tmp;
	int m, file_charset = CHARSET_UNKNOWN;
	char *startptr = NULL, *endptr = NULL;

	str_init( &tmp );

	do {
		if ( line->data ) str_strcat( &tmp, line );
		if ( str_has_value( &tmp ) ) {
			m = xml_getencoding( &tmp );
			if ( m!=CHARSET_UNKNOWN ) file_charset = m;
			startptr = modsin_startptr( tmp.data );
			endptr = modsin_endptr( tmp.data );
		} else startptr = endptr = NULL;
		str_empty( line );
		if ( startptr && endptr ) {
			str_segcpy( reference, startptr, endptr );
			str_strcpyc( line, endptr );
		}
	} while ( !endptr && str_fget( fp, buf, bufsize, bufpos, line ) );

	str_free( &tmp );
	*fcharset = file_charset;
	return ( reference->len > 0 );
}

