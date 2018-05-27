/*
 * bibtexout.c
 *
 * Copyright (c) Chris Putnam 2003-2017
 *
 * Program and source code released under the GPL version 2
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "str.h"
#include "strsearch.h"
#include "utf8.h"
#include "xml.h"
#include "fields.h"
#include "name.h"
#include "title.h"
#include "url.h"
#include "bibformats.h"

/* the macro below is to comment out or in statements for debugging purposes */
#define Da1  if (0)
#define Da3  if (1)

static int  bibtexout_write( fields *in, FILE *fp, param *p, unsigned long refnum );
static void bibtexout_writeheader( FILE *outptr, param *p );

void
bibtexout_initparams( param *p, const char *progname )
{
	p->writeformat      = BIBL_BIBTEXOUT;
	p->format_opts      = 0;
	p->charsetout       = BIBL_CHARSET_DEFAULT;
	p->charsetout_src   = BIBL_SRC_DEFAULT;
	p->latexout         = 1;
	p->utf8out          = BIBL_CHARSET_UTF8_DEFAULT;
	p->utf8bom          = BIBL_CHARSET_BOM_DEFAULT;
	p->xmlout           = BIBL_XMLOUT_FALSE;
	p->nosplittitle     = 0;
	p->verbose          = 0;
	p->addcount         = 0;
	p->singlerefperfile = 0;

	p->headerf = bibtexout_writeheader;
	p->footerf = NULL;
	p->writef  = bibtexout_write;
	p->language = BIBL_LANGUAGE_ENGLISH; /* default language */


	if ( !p->progname && progname )
		p->progname = strdup( progname );
}

enum {
	TYPE_UNKNOWN = 0,
	TYPE_ARTICLE,
	TYPE_INBOOK,
	TYPE_INPROCEEDINGS,
	TYPE_PROCEEDINGS,
	TYPE_INCOLLECTION,
	TYPE_COLLECTION,
	TYPE_BOOK,
	TYPE_PHDTHESIS,
	TYPE_MASTERSTHESIS,
	TYPE_REPORT,
	TYPE_MANUAL,
	TYPE_UNPUBLISHED,
	TYPE_ELECTRONIC,
	TYPE_MISC,
	NUM_TYPES
};

static int
bibtexout_type( fields *in, char *filename, int refnum, param *p )
{
	int type = TYPE_UNKNOWN, i, maxlevel, n, level;
	char *tag, *genre;

	/* determine bibliography type */
	for ( i=0; i<in->n; ++i ) {
		tag = fields_tag( in, i, FIELDS_CHRP );
		if ( strcasecmp( tag, "GENRE" ) && strcasecmp( tag, "NGENRE" ) ) continue;

		genre = fields_value( in, i, FIELDS_CHRP );
		Da1 fprintf( stderr, "GQMJr %d - genre %s\n", refnum+1, genre ); /* added to debug KTH DiVA */
		level = in->level[i];
		if ( !strcasecmp( genre, "periodical" ) ||
		     !strcasecmp( genre, "academic journal" ) ||
		     !strcasecmp( genre, "magazine" ) ||
		     !strcasecmp( genre, "newspaper" ) ||
		     !strcasecmp( genre, "article" ) )
		        type = TYPE_ARTICLE;
		else if ( !strcasecmp( genre, "instruction" ) )
			type = TYPE_MANUAL;
		else if ( !strcasecmp( genre, "unpublished" ) )
			type = TYPE_UNPUBLISHED;
		else if ( !strcasecmp( genre, "manuscript" ) ) { /* added to support KTH DiVA - to handle manuscript   */
			type = TYPE_UNPUBLISHED;
			return type;
		} else if (!strcasecmp( genre, "conferencePaper" )  ) { /* added to support KTH DiVA - to handle conference papers */
		        type = TYPE_INPROCEEDINGS;
			Da1 fprintf( stderr, "GQMJr %d - genre %s\n", refnum+1, genre ); /* added to debug KTH DiVA */
			return type;
		} else if (!strcasecmp( genre, "conferenceProceedings" )  ) { /* added to support KTH DiVA - to handle conferenceProceedings */
		        type = TYPE_INPROCEEDINGS;
			return type;
		} else if (!strcasecmp( genre, "artisticOutput" )  ) { /* added to support KTH DiVA - to handle conferenceProceedings */
		        type = TYPE_MISC;
			return type;
		} else if ( !strcasecmp( genre, "conference publication" ) ) {
			if ( level==0 ) type=TYPE_PROCEEDINGS;
			else type = TYPE_INPROCEEDINGS;
		} else if ( !strcasecmp( genre, "collection" ) ) {
			if ( level==0 ) type=TYPE_COLLECTION;
			else type = TYPE_INCOLLECTION;
		} else if ( !strcasecmp( genre, "report" ) )
			type = TYPE_REPORT;
		else if ( !strcasecmp( genre, "book chapter" ) )
			type = TYPE_INBOOK;
		else if (!strcasecmp( genre, "studentThesis" )  ) { /* added to support KTH DiVA - to handle student theses as books */
			type = TYPE_BOOK;			    /* treat as a book as it has a series and number */
			Da1 fprintf( stderr, "GQMJr %d - genre %s, type=%d\n", refnum+1, genre, type ); /* added to debug KTH DiVA */
			return type;
		} else if (!strcasecmp( genre, "monographDoctoralThesis" )  ) { /* added to support KTH DiVA's monograph type of dissertation */
			type = TYPE_PHDTHESIS;
			Da1 fprintf( stderr, "GQMJr %d - genre %s, type=%d\n", refnum+1, genre, type ); /* added to debug KTH DiVA */
			return type;
		} else if (!strcasecmp( genre, "comprehensiveDoctoralThesis" )  ) { /* added to support KTH DiVA's monograph type of dissertation */
			type = TYPE_PHDTHESIS;
			Da1 fprintf( stderr, "GQMJr %d - genre %s, type=%d\n", refnum+1, genre, type ); /* added to debug KTH DiVA */
			return type;
		} else if (!strcasecmp( genre, "comprehensiveLicentiateThesis" )  ) { /* added to support KTH DiVA's monograph type of dissertation */
			type = TYPE_PHDTHESIS;
			Da1 fprintf( stderr, "GQMJr %d - genre %s, type=%d\n", refnum+1, genre, type ); /* added to debug KTH DiVA */
			return type;
		} else if (!strcasecmp( genre, "monographLicentiateThesis" )  ) { /* added to support KTH DiVA's monograph type of dissertation */
			type = TYPE_PHDTHESIS;
			Da1 fprintf( stderr, "GQMJr %d - genre %s, type=%d\n", refnum+1, genre, type ); /* added to debug KTH DiVA */
			return type;
		} else if ( !strcasecmp( genre, "book" ) ) {
		  if ( level==0 ) {
		    type = TYPE_BOOK;
		    Da1 fprintf( stderr, "GQMJr %d - genre %s, type=%d\n", refnum+1, genre, type ); /* added to debug KTH DiVA */
		  } else type = TYPE_INBOOK;
		} else if ( !strcasecmp( genre, "thesis" ) ) {
			if ( type==TYPE_UNKNOWN ) type=TYPE_PHDTHESIS;
		} else if ( !strcasecmp( genre, "Ph.D. thesis" ) )
			type = TYPE_PHDTHESIS;
		else if ( !strcasecmp( genre, "Masters thesis" ) )
			type = TYPE_MASTERSTHESIS;
		else  if ( !strcasecmp( genre, "electronic" ) )
			type = TYPE_ELECTRONIC;
		else  if ( !strcasecmp( genre, "other" ) ) /* added to support KTH DiVA - to support "other" genre */
			type = TYPE_MISC;

	}
	if ( type==TYPE_UNKNOWN ) {
		for ( i=0; i<in->n; ++i ) {
			tag = fields_tag( in, i, FIELDS_CHRP );
			if ( strcasecmp( tag, "ISSUANCE" ) ) continue;
			genre = fields_value( in, i, FIELDS_CHRP );
			if ( !strcasecmp( genre, "monographic" ) ) {
				if ( in->level[i]==0 ) type = TYPE_BOOK;
				else if ( in->level[i]==1 ) type = TYPE_MISC;
			}
		}
	}

	/* default to TYPE_MISC */
	if ( type==TYPE_UNKNOWN ) {
		maxlevel = fields_maxlevel( in );
		if ( maxlevel > 0 ) type = TYPE_MISC;
		else {
			if ( p->progname ) fprintf( stderr, "%s: ", p->progname );
			fprintf( stderr, "Cannot identify TYPE in reference %d ", refnum+1 );
			n = fields_find( in, "REFNUM", LEVEL_ANY );
			if ( n!=-1 ) 
				fprintf( stderr, " %s", (char*) fields_value( in, n, FIELDS_CHRP ) );
			fprintf( stderr, " (defaulting to @Misc)\n" );
			type = TYPE_MISC;
		}
	}
	return type;
}

static void
output( FILE *fp, fields *out, int format_opts )
{
	int i, j, len, nquotes;
	char *tag, *value, ch;

	/* ...output type information "@article{" */
	value = ( char * ) fields_value( out, 0, FIELDS_CHRP );
	if ( !(format_opts & BIBL_FORMAT_BIBOUT_UPPERCASE) ) fprintf( fp, "@%s{", value );
	else {
		len = strlen( value );
		fprintf( fp, "@" );
		for ( i=0; i<len; ++i )
			fprintf( fp, "%c", toupper((unsigned char)value[i]) );
		fprintf( fp, "{" );
	}

	/* ...output refnum "Smith2001" */
	value = ( char * ) fields_value( out, 1, FIELDS_CHRP );
	fprintf( fp, "%s", value );

	/* ...rest of the references */
	for ( j=2; j<out->n; ++j ) {
		nquotes = 0;
		tag   = ( char * ) fields_tag( out, j, FIELDS_CHRP );
		value = ( char * ) fields_value( out, j, FIELDS_CHRP );
		fprintf( fp, ",\n" );
		if ( format_opts & BIBL_FORMAT_BIBOUT_WHITESPACE ) fprintf( fp, "  " );
		if ( !(format_opts & BIBL_FORMAT_BIBOUT_UPPERCASE ) ) fprintf( fp, "%s", tag );
		else {
			len = strlen( tag );
			for ( i=0; i<len; ++i )
				fprintf( fp, "%c", toupper((unsigned char)tag[i]) );
		}
		if ( format_opts & BIBL_FORMAT_BIBOUT_WHITESPACE ) fprintf( fp, " = \t" );
		else fprintf( fp, "=" );

		if ( format_opts & BIBL_FORMAT_BIBOUT_BRACKETS ) fprintf( fp, "{" );
		else fprintf( fp, "\"" );

		len = strlen( value );
		for ( i=0; i<len; ++i ) {
			ch = value[i];
			if ( ch!='\"' ) fprintf( fp, "%c", ch );
			else {
				if ( format_opts & BIBL_FORMAT_BIBOUT_BRACKETS || ( i>0 && value[i-1]=='\\' ) )
					fprintf( fp, "\"" );
				else {
					if ( nquotes % 2 == 0 )
						fprintf( fp, "``" );
					else    fprintf( fp, "\'\'" );
					nquotes++;
				}
			}
		}

		if ( format_opts & BIBL_FORMAT_BIBOUT_BRACKETS ) fprintf( fp, "}" );
		else fprintf( fp, "\"" );
	}

	/* ...finish reference */
	if ( format_opts & BIBL_FORMAT_BIBOUT_FINALCOMMA ) fprintf( fp, "," );
	fprintf( fp, "\n}\n\n" );

	fflush( fp );
}

static void
append_type( int type, fields *out, int *status )
{
	char *typenames[ NUM_TYPES ] = {
		[ TYPE_ARTICLE       ] = "Article",
		[ TYPE_INBOOK        ] = "Inbook",
		[ TYPE_PROCEEDINGS   ] = "Proceedings",
		[ TYPE_INPROCEEDINGS ] = "InProceedings",
		[ TYPE_BOOK          ] = "Book",
		[ TYPE_PHDTHESIS     ] = "PhdThesis",
		[ TYPE_MASTERSTHESIS ] = "MastersThesis",
		[ TYPE_REPORT        ] = "TechReport",
		[ TYPE_MANUAL        ] = "Manual",
		[ TYPE_COLLECTION    ] = "Collection",
		[ TYPE_INCOLLECTION  ] = "InCollection",
		[ TYPE_UNPUBLISHED   ] = "Unpublished",
		[ TYPE_ELECTRONIC    ] = "Electronic",
		[ TYPE_MISC          ] = "Misc",
	};
	int fstatus;
	char *s;

	if ( type < 0 || type >= NUM_TYPES ) type = TYPE_MISC;
	s = typenames[ type ];

	fstatus = fields_add( out, "TYPE", s, LEVEL_MAIN );
	if ( fstatus!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
}

static void
append_citekey( fields *in, fields *out, int format_opts, int *status )
{
	int n, fstatus;
	str s;
	char *p;

	n = fields_find( in, "REFNUM", LEVEL_ANY );
	if ( ( format_opts & BIBL_FORMAT_BIBOUT_DROPKEY ) || n==-1 ) {
		fstatus = fields_add( out, "REFNUM", "", LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
	}

	else {
		str_init( &s );
		p = fields_value( in, n, FIELDS_CHRP );
		while ( p && *p && *p!='|' ) {
			if ( format_opts & BIBL_FORMAT_BIBOUT_STRICTKEY ) {
				if ( isdigit((unsigned char)*p) || (*p>='A' && *p<='Z') ||
				     (*p>='a' && *p<='z' ) ) {
					str_addchar( &s, *p );
				}
			}
			else {
				if ( *p!=' ' && *p!='\t' ) {
					str_addchar( &s, *p );
				}
			}
			p++;
		}
		if ( str_memerr( &s ) )  { *status = BIBL_ERR_MEMERR; str_free( &s ); return; }
		fstatus = fields_add( out, "REFNUM", str_cstr( &s ), LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
		str_free( &s );
	}
}

static void
append_simple( fields *in, char *intag, char *outtag, fields *out, int *status )
{
	int n, fstatus;

	n = fields_find( in, intag, LEVEL_ANY );
	if ( n!=-1 ) {
		fields_setused( in, n );
		fstatus = fields_add( out, outtag, fields_value( in, n, FIELDS_CHRP ), LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
	}
}

static void
append_simpleall( fields *in, char *intag, char *outtag, fields *out, int *status )
{
	int i, fstatus;

	for ( i=0; i<in->n; ++i ) {
		if ( fields_match_tag( in, i, intag ) ) {
			fields_setused( in, i );
			fstatus = fields_add( out, outtag, fields_value( in, i, FIELDS_CHRP ), LEVEL_MAIN );
			if ( fstatus!=FIELDS_OK ) {
				*status = BIBL_ERR_MEMERR;
				return;
			}
		}
	}
}

static void
append_keywords( fields *in, fields *out, int *status, int lang )
{
	str keywords, *word;
	vplist_index i;
	int fstatus;
	vplist a;

	str_init( &keywords );
	vplist_init( &a );

	if (lang & BIBL_LANGUAGE_ENGLISH )
	  fields_findv_each( in, LEVEL_ANY, FIELDS_STRP, &a, "KEYWORD:EN" );
	else if (lang & BIBL_LANGUAGE_SWEDISH )
	  fields_findv_each( in, LEVEL_ANY, FIELDS_STRP, &a, "KEYWORD:SV" );
	else 
	  fields_findv_each( in, LEVEL_ANY, FIELDS_STRP, &a, "KEYWORD" );

	if ( a.n ) {

		for ( i=0; i<a.n; ++i ) {
			word = vplist_get( &a, i );
			if ( i>0 ) str_strcatc( &keywords, "; " );
			str_strcat( &keywords, word );
		}

		if ( str_memerr( &keywords ) ) { *status = BIBL_ERR_MEMERR; goto out; }

		fstatus = fields_add( out, "keywords", str_cstr( &keywords ), LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) {
			*status = BIBL_ERR_MEMERR;
			goto out;
		}


	}

out:
	str_free( &keywords );
	vplist_free( &a );
}

static void
append_fileattach( fields *in, fields *out, int *status )
{
	char *tag, *value;
	int i, fstatus;
	str data;

	str_init( &data );

	for ( i=0; i<in->n; ++i ) {

		tag = fields_tag( in, i, FIELDS_CHRP );
		if ( strcasecmp( tag, "FILEATTACH" ) ) continue;

		value = fields_value( in, i, FIELDS_CHRP );
		str_strcpyc( &data, ":" );
		str_strcatc( &data, value );
		if ( strsearch( value, ".pdf" ) )
			str_strcatc( &data, ":PDF" );
		else if ( strsearch( value, ".html" ) )
			str_strcatc( &data, ":HTML" );
		else str_strcatc( &data, ":TYPE" );

		if ( str_memerr( &data ) ) {
			*status = BIBL_ERR_MEMERR;
			goto out;
		}

		fields_setused( in, i );
		fstatus = fields_add( out, "file", str_cstr( &data ), LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) {
			*status = BIBL_ERR_MEMERR;
			goto out;
		}

		str_empty( &data );
	}
out:
	str_free( &data );
}

/* added to debug KTH DiVA - simply prints out the fields of a MODS record as represented internally */
static void
print_fields(fields *in)
{
  int i;
  //typedef struct fields {
  //  	str       *tag;
  //	str       *data;
  //	int       *used;
  //	int       *level;
  //	int       n;
  //	int       max;
  //} fields;
 fprintf( stderr, "GQMJr::print_fields n = %d\n", in->n); /* added to debug KTH DiVA */
 fprintf( stderr, "GQMJr::print_fields max = %d\n", in->max); /* added to debug KTH DiVA */

  for ( i=0; i<in->n; ++i ) {
    fprintf( stderr, "GQMJr::print_fields in->tag[%d].data = %s\n", i, in->tag[i].data); /* added to debug KTH DiVA */
    fprintf( stderr, "GQMJr::print_fields in->data[%d].data = %s\n", i, in->data[i].data); /* added to debug KTH DiVA */

    if ((in->level[i]) == 0)
      fprintf( stderr, "GQMJr::print_fields in->level[%d] = LEVEL_MAIN\n", i); /* added to debug KTH DiVA */
    else if ((in->level[i]) == 1)
      fprintf( stderr, "GQMJr::print_fields in->level[%d] = LEVEL_HOST\n", i); /* added to debug KTH DiVA */
    else
      fprintf( stderr, "GQMJr::print_fields in->level[%d] = %d\n", i, in->level[i]); /* added to debug KTH DiVA */

    fprintf( stderr, "GQMJr::print_fields in->used[%d] = %d\n", i, in->used[i]); /* added to debug KTH DiVA */
  }
  
}

static void
append_people( fields *in, char *tag, char *ctag, char *atag,
		char *bibtag, int level, fields *out, int format_opts )
{
	str allpeople, oneperson;
	int i, npeople, person, corp, asis;

	strs_init( &allpeople, &oneperson, NULL );

	Da1 print_fields(in);/* added to debug KTH DiVA */

	/* primary citation authors */
	npeople = 0;
	for ( i=0; i<in->n; ++i ) {
		if ( level!=LEVEL_ANY && in->level[i]!=level ) continue;
		person = ( strcasecmp( in->tag[i].data, tag ) == 0 );
		corp   = ( strcasecmp( in->tag[i].data, ctag ) == 0 );
		asis   = ( strcasecmp( in->tag[i].data, atag ) == 0 );
		if ( person || corp || asis ) {
		  if (in->tag[i].data) {
		    Da1 fprintf( stderr, "GQMJr::append_people in->tag[%d].data = %s\n", i, in->tag[i].data); /* added to debug KTH DiVA */
		    Da1 fprintf( stderr, "GQMJr::append_people fields_value( in, %d, FIELDS_CHRP ) = %s\n", i, (char *)fields_value( in, i, FIELDS_CHRP )); /* added to debug KTH DiVA */
		    
		  }
			if ( npeople>0 ) {
				if ( format_opts & BIBL_FORMAT_BIBOUT_WHITESPACE )
					str_strcatc( &allpeople, "\n\t\tand " );
				else str_strcatc( &allpeople, "\nand " );
			}
			if ( corp ) {
				str_addchar( &allpeople, '{' );
				str_strcat( &allpeople, fields_value( in, i, FIELDS_STRP ) );
				str_addchar( &allpeople, '}' );
			} else if ( asis ) {
				str_addchar( &allpeople, '{' );
				str_strcat( &allpeople, fields_value( in, i, FIELDS_STRP ) );
				str_addchar( &allpeople, '}' );
			} else {
				name_build_withcomma( &oneperson, fields_value( in, i, FIELDS_CHRP ) );
				str_strcat( &allpeople, &oneperson );
			}
			npeople++;
		}
	}
	if ( npeople ) {
		fields_add( out, bibtag, allpeople.data, LEVEL_MAIN );
	}

	strs_free( &allpeople, &oneperson, NULL );
}

static int
append_title_chosen( fields *in, char *bibtag, fields *out, int nmainttl, int nsubttl )
{
	str fulltitle, *mainttl = NULL, *subttl = NULL;
	int status, ret = BIBL_OK;

	str_init( &fulltitle );

	if ( nmainttl!=-1 ) {
		mainttl = fields_value( in, nmainttl, FIELDS_STRP );
		fields_setused( in, nmainttl );
	}

	if ( nsubttl!=-1 ) {
		subttl = fields_value( in, nsubttl, FIELDS_STRP );
		fields_setused( in, nsubttl );
	}

	title_combine( &fulltitle, mainttl, subttl );

	if ( str_memerr( &fulltitle ) ) {
		ret = BIBL_ERR_MEMERR;
		goto out;
	}

	if ( str_has_value( &fulltitle ) ) {
		status = fields_add( out, bibtag, str_cstr( &fulltitle ), LEVEL_MAIN );
		if ( status!=FIELDS_OK ) ret = BIBL_ERR_MEMERR;
	}

out:
	str_free( &fulltitle );
	return ret;
}

/* for use with KTH DIVA - adapted from append_title_chosen() to simply return the combined fulltile,
 * rather than storing it in a field */
static int
construct_title_chosen( fields *in, char *bibtag, fields *out, int nmainttl, int nsubttl, str *fulltitle )
{
	str *mainttl = NULL, *subttl = NULL;
	int ret = BIBL_OK;

	if ( nmainttl!=-1 ) {
		mainttl = fields_value( in, nmainttl, FIELDS_STRP );
		fields_setused( in, nmainttl );
	}

	if ( nsubttl!=-1 ) {
		subttl = fields_value( in, nsubttl, FIELDS_STRP );
		fields_setused( in, nsubttl );
	}

	title_combine( fulltitle, mainttl, subttl );

	if ( str_memerr( fulltitle ) ) {
		ret = BIBL_ERR_MEMERR;
		goto out;
	}

out:
	return ret;
}



/*
 * In KTH's DiVA there are a number of works that have titles in multiple languages,
 * hence there is a need to decide how these titles should appear in the text.
 *
 * Actually a number of theses at KTH have the title in two languages and a very large number of
 * official Swedish government documents written in Swedish have both and Swedish & English title
 * and an English summary.  (We currently can report the title in Swedish and English in Ladok,
 *  but not everyone does.) In DiVA the alternate title have the form: 
 *    <titleInfo type="alternative" lang="eng"><title>xxxx</title><subTitle>yyyy</subTitle></titleInfo> 
 *
 * Consider the case of a document written in English (i.e., the LANGUAGE file is set to "English"),
 * then this document should have an English title. Similarly if the LANGUAGE is "Swedish" and the
 * document has a Swedish title, then we would expect this to appear in the reference.
 *
 * However, if the document is being used as a reference in a document being written in English,
 * then one might expect to see the title also appear in English as well as in Swedish:
 * For example Patrik Olsson's Master's thesis (from urn:nbn:se:kth:diva-205573): 
 *   Virtuell verklighet för förmedling av ett företags produkter: En undersökning av kunders beslutsprocess
 *   i valet av köksmoduler gjord ur ett lärandeperspektiv [Virtual Reality for communicating a company's
 *   products: A survey of customers' decision-making in the process of choosing kitchen modules made from
 *   a learning perspective] (in Swedish with English summary)
 *
 * The general form (based upon The Chicago Manual of Style and 
 * "General guide for Referencing and avoiding plagiarism: Foreign Language Material"
 * topc: Can I use Foreign Language material? AIT Library Guides, Athlone Institute of Technology,
 * Dublin Road, Athlone, Co. Westmeath, Ireland. 16 January 2018
 * https://ait.libguides.com/c.php?g=280093&p=1866392) is:
 *    original_title [translated_title] (in orignal_language with translated_language summary)
 *
 * The corresponding form in Swedish would be:
 *    original_title [translated_title] (på svenska med engelska sammanfattning)
 *
 * The [xxx] (yyyy) can be generated via BibLaTex's field titleaddon={[xxx] (yyyy)}
 *  (the above is from "bath-bst: Harvard referencing style as recommended by the University of Bath Library"
 *    Maintainer: Alex Ball, Package v2.0 – 23 April 2018
 *    http://mirror.hmc.edu/ctan/biblio/bibtex/contrib/bath-bst/bath-bst-v1.pdf  )
 *
 * Note that the [xxxx] denotes the translation,
 * while the (yyyy) describes the original language with _optional_ description of whether
 * there is a summary in the translated language.
 * 
 * This leads to the following interesting combinations
 * (where EN_title or SV_title means the existence of title and possible subtitle in the indicated language;
 *  LANGUAGE=xxxx indicates the original language
 *  ABSTRACT:xx indicates the existence of an abstract in the indicated language;
 * ):
 * English:
 *      English_title                                                          EN_title          LANGUAGE=English
 *      English_title                                                          EN_title          LANGUAGE=English ABSTRACT:EN
 *      English_title (with Swedish summary)                                   EN_title          LANGUAGE=English ABSTRACT:SV
 *      English_title (in Swedish)                                             EN_title          LANGUAGE=Swedish
 *      English_title (in Swedish with English summary)                        EN_title          LANGUAGE=Swedish ABSTRACT:EN
 *      English_title (in Swedish with Swedish summary)                        EN_title          LANGUAGE=Swedish ABSTRACT:SV
 *
 *      Swedish_title                                                                   SV_title LANGUAGE=Swedish
 *      Swedish_title (with English summary)                                            SV_title LANGUAGE=Swedish ABSTRACT:EN
 *      Swedish_title                                                                   SV_title LANGUAGE=Swedish ABSTRACT:SV
 *      Swedish_title (in English)                                                      SV_title LANGUAGE=English
 *      Swedish_title (in English with English summary)                                 SV_title LANGUAGE=English ABSTRACT:EN
 *      Swedish_title (in English with Swedish summary)                                 SV_title LANGUAGE=English ABSTRACT:SV
 *
 *      English_title [Swedish_title]                                          EN_title SV_title LANGUAGE=English
 *      English_title [Swedish_title]                                          EN_title SV_title LANGUAGE=English ABSTRACT:EN
 *      English_title [Swedish_title] (with Swedish summary)                   EN_title SV_title LANGUAGE=English ABSTRACT:SV
 *
 *      Swedish_title [English_title]                                          EN_title SV_title LANGUAGE=Swedish
 *      Swedish_title [English_title] (with English summary)                   EN_title SV_title LANGUAGE=Swedish ABSTRACT:EN
 *      Swedish_title [English_title]                                          EN_title SV_title LANGUAGE=Swedish ABSTRACT:SV
 *
 *
 * Swedish:
 *      English_title                                                          EN_title          LANGUAGE=English
 *      English_title                                                          EN_title          LANGUAGE=English ABSTRACT:EN
 *      English_title (med svenska sammanfattning)                             EN_title          LANGUAGE=English ABSTRACT:SV
 *      English_title (på svenska)                                             EN_title          LANGUAGE=Swedish
 *      English_title (på svenska med engelska sammanfattning)                 EN_title          LANGUAGE=Swedish ABSTRACT:EN
 *      English_title (på svenska med svenska sammanfattning)                  EN_title          LANGUAGE=Swedish ABSTRACT:SV
 *
 *      Swedish_title                                                                    SV_title LANGUAGE=Swedish
 *      Swedish_title (med engelska sammanfattning)                                      SV_title LANGUAGE=Swedish ABSTRACT:EN
 *      Swedish_title                                                                    SV_title LANGUAGE=Swedish ABSTRACT:SV
 *      Swedish_title (på engelska)                                                      SV_title LANGUAGE=English
 *      Swedish_title (på engelska med engelska sammanfattning)                          SV_title LANGUAGE=English ABSTRACT:EN
 *      Swedish_title (på engelska med svenska sammanfattning)                           SV_title LANGUAGE=English ABSTRACT:SV

 *
 *      English_title [Swedish_title]                                           EN_title SV_title LANGUAGE=English
 *      English_title [Swedish_title]                                           EN_title SV_title LANGUAGE=English ABSTRACT:EN
 *      English_title [Swedish_title] (med svenska sammanfattning)              EN_title SV_title LANGUAGE=English ABSTRACT:SV
 *
 *      Swedish_title [English_title]                                           EN_title SV_title LANGUAGE=Swedish
 *      Swedish_title [English_title] (med engelska sammanfattning)             EN_title SV_title LANGUAGE=Swedish ABSTRACT:EN
 *      Swedish_title [English_title]                                           EN_title SV_title LANGUAGE=Swedish ABSTRACT:SV
 *
 * Note that in the above there are some cases which reduce down to a simpler one, hence they are not shown. For example,
 *      English_title (with Swedish summary)                                   EN_title          LANGUAGE=English ABSTRACT:SV ABSTRACT:EN
 * does not have to be shown as having an English summary, since this is to be expected for a document in English.
 *
 * It is also useful to see that the pattern is simple, when these are two titles you choose as the original title the one matching the value of LANGUAGE.
 * Generate the other information: "in Swedish" or  "in English" when the LANGUAGE does not match the single title.
 * Generate the other information: "with Swedish Summary" or  "with English Summary" when the LANGUAGE does not match the single title.
 *
 */

static int
append_title( fields *in, char *bibtag, int level, fields *out, int format_opts, int lang )
{

#ifdef NEVER
	int use_title = -1, use_subtitle = -1;
	int title = -1,     short_title = -1;
	int subtitle = -1,  short_subtitle = -1;

	if (lang & BIBL_LANGUAGE_ENGLISH ) {
	  title          = fields_find( in, "TITLE:EN",         level );
	} else if (lang & BIBL_LANGUAGE_SWEDISH ) {
	  title          = fields_find( in, "TITLE:SV",         level );
	} else
	  title          = fields_find( in, "TITLE",         level );

	/* need to check that one of the titles was found, if not then check other language */

	if (lang & BIBL_LANGUAGE_ENGLISH ) {
	  subtitle       = fields_find( in, "SUBTITLE:EN",      level );
	} else if (lang & BIBL_LANGUAGE_SWEDISH ) {
	  subtitle       = fields_find( in, "SUBTITLE:SV",      level );
	} else
	  subtitle       = fields_find( in, "SUBTITLE",      level );

	/* need to check that one of the subtitles was found, if not then check other language */

	short_title    = fields_find( in, "SHORTTITLE",    level );
	short_subtitle = fields_find( in, "SHORTSUBTITLE", level );

	if ( title==-1 || ( ( format_opts & BIBL_FORMAT_BIBOUT_SHORTTITLE ) && level==1 ) ) {
		use_title    = short_title;
		use_subtitle = short_subtitle;
	}

	else {
		use_title    = title;
		use_subtitle = subtitle;
	}

	return append_title_chosen( in, bibtag, out, use_title, use_subtitle );
#endif
	int status, ret = BIBL_OK;

	int title    = -1, subtitle = -1;
	int short_title = -1, short_subtitle = -1;

	int en_title    = -1, sv_title = -1;
	int en_subtitle = -1, sv_subtitle = -1;
	int en_abstract = -1, sv_abstract = -1;
	int language_fieldindex  = -1;
	char *language;
	str en_fulltitle, sv_fulltitle, fulltitle, completetitle;
	str lang_addon, summary_addon, complete_addon;
	strs_init(&en_fulltitle, &sv_fulltitle, &fulltitle, &completetitle, &lang_addon, &summary_addon, &complete_addon, NULL);
	int original_lang = -1;

	Da1 fprintf( stderr, "GQMJr::append_title bibtag=%s, level=%d\n", bibtag, level); /* added to debug KTH DiVA */

	title       = fields_find( in, "TITLE",         level );
	en_title    = fields_find( in, "TITLE:EN",      level );
	sv_title    = fields_find( in, "TITLE:SV",      level );
	short_title = fields_find( in, "SHORTTITLE",    level );

	subtitle       = fields_find( in, "SUBTITLE",      level );
	en_subtitle    = fields_find( in, "SUBTITLE:EN",   level );
	sv_subtitle    = fields_find( in, "SUBTITLE:SV",   level );
	short_subtitle = fields_find( in, "SHORTSUBTITLE", level );

	en_abstract = fields_find( in, "ABSTRACT:EN",      level );
	sv_abstract = fields_find( in, "ABSTRACT:SV",      level );

	language_fieldindex = fields_find( in, "LANGUAGE", LEVEL_MAIN );
	Da1 fprintf( stderr, "GQMJr::append_title language_fieldindex=%d\n", language_fieldindex); /* added to debug KTH DiVA */
	if (language_fieldindex != -1) {
	  Da1 fprintf( stderr, "GQMJr::append_title language defined\n");
	  language=fields_value( in, language_fieldindex, FIELDS_CHRP );
	  Da1 fprintf( stderr, "GQMJr::append_title language=%s\n", language);
	  if (strncmp(language, "English", strlen(language)) ==0)
	    original_lang = 1;
	  else if (strncmp(language, "Swedish", strlen(language)) ==0)
	    original_lang = 2;

	}

	if ((en_title == -1) && (sv_title == -1) && (title == -1) && (short_title == -1) ) {
	  Da1 fprintf( stderr, "GQMJr::append_title ERROR no title of any kind\n");
	  ret = BIBL_ERR_MEMERR;
	  goto out;
	}

	/* only a shorttile is available, use it appropriately */
	if ((en_title == -1) && (sv_title == -1) && (title == -1) && (short_title != -1) ) {
	  if ((format_opts & BIBL_FORMAT_BIBOUT_SHORTTITLE) && level==1) {
	    construct_title_chosen(in, bibtag, out, short_title,  short_subtitle, &fulltitle );
	    str_strcpy(&completetitle, &fulltitle);
	    goto addons;
	  }
	}

	/* only a title is avaialble, so use it */
	if ((en_title == -1) && (sv_title == -1) && (title != -1)) {
	  construct_title_chosen(in, bibtag, out, title, subtitle, &fulltitle );
	  str_strcpy(&completetitle, &fulltitle);
	  goto addons;
	}

	/* if the title is only available in one language, just use it! */
	if ((en_title == -1) && (sv_title != -1)) {
	  construct_title_chosen(in, bibtag, out, sv_title, sv_subtitle, &sv_fulltitle );
	  str_strcpy(&completetitle, &sv_fulltitle);
	  goto addons;
	} else if ((en_title != -1) && (sv_title == -1)) {
	  construct_title_chosen(in, bibtag, out, en_title, en_subtitle, &en_fulltitle );
	  str_strcpy( &completetitle, &en_fulltitle);
	  goto addons;
	}

	/* based upon the language of the document choose the appropriate of the titles */
	if ((en_title != -1) && (sv_title != -1)) {	
	  construct_title_chosen(in, bibtag, out, en_title, en_subtitle, &en_fulltitle );
	  construct_title_chosen(in, bibtag, out, sv_title, sv_subtitle, &sv_fulltitle );

	  Da1 fprintf( stderr, "GQMJr::append_title en_fulltitle=%s\n", en_fulltitle.data);
	  Da1 fprintf( stderr, "GQMJr::append_title sv_fulltitle=%s\n", sv_fulltitle.data);

	  if (language_fieldindex != -1) {
	    if (strlen(language) > 0) {
	      if (original_lang == 1) {
		str_strcpy(&completetitle, &en_fulltitle);
		str_strcatc(&completetitle, " [");
		str_strcat(&completetitle, &sv_fulltitle );
		str_strcatc(&completetitle, "]");
	      } else {
		str_strcpy(&completetitle, &sv_fulltitle);
		str_strcatc(&completetitle, " [");
		str_strcat(&completetitle, &en_fulltitle );
		str_strcatc(&completetitle, "]");
	      }
	    }
	  } else
	    fprintf( stderr, "append_title:: language not defined\n");
	}

 addons:
	/* deal with addons */
	/* qqq1 */
	/* str_strcat( str *s, str *from ) */

	/* addons are split into lang_addon and summary_addon */

	/* lang_addon */
	if (lang & BIBL_LANGUAGE_ENGLISH ) {
	  if ((en_title != -1) && (sv_title == -1) && (original_lang == 2) ) {
	    str_strcatc(&lang_addon, "in Swedish");
	  } else if ((en_title == -1) && (sv_title != -1) && (original_lang == 1) ) {
	    str_strcatc(&lang_addon, "in English");
	  }
	} else if (lang & BIBL_LANGUAGE_SWEDISH ) {
	  if ((en_title != -1) && (sv_title == -1) && (original_lang == 2) ) {
	    str_strcatc(&lang_addon, "på svenska");
	  } else if ((en_title == -1) && (sv_title != -1) && (original_lang == 1) ) {
	    str_strcatc(&lang_addon, "på engelska");
	  }
	} else
	  fprintf( stderr, "append_title:: unsupported lanaguage combination in lang_addon\n");

	/* summary_addon */
	if (lang & BIBL_LANGUAGE_ENGLISH ) {
	  if ((en_title != -1) && (sv_title == -1) && (original_lang == 1) && (sv_abstract != -1)) {
	    str_strcatc(&summary_addon, "with Swedish summary");
	  } else if ((en_title != -1) && (sv_title == -1) && (original_lang == 2) && (sv_abstract != -1)) {
	    str_strcatc(&summary_addon, "with Swedish summary");
	  } else if ((en_title != -1) && (sv_title == -1) && (original_lang == 2) && (en_abstract != -1)) {
	    str_strcatc(&summary_addon, "with English summary");
	  } else if ((en_title == -1) && (sv_title != -1) && (original_lang == 2) && (en_abstract != -1)) {
	    str_strcatc(&summary_addon, "with English summary");
	  } else if ((en_title == -1) && (sv_title != -1) && (original_lang == 1) && (en_abstract != -1)) {
	    str_strcatc(&summary_addon, "with English summary");
	  } else if ((en_title == -1) && (sv_title != -1) && (original_lang == 1) && (sv_abstract != -1)) {
	    str_strcatc(&summary_addon, "with Swedish summary");
	  } else if ((en_title != -1) && (sv_title != -1) && (original_lang == 1) && (sv_abstract != -1)) {
	    str_strcatc(&summary_addon, "with Swedish summary");
	  } else if ((en_title != -1) && (sv_title != -1) && (original_lang == 2) && (en_abstract != -1)) {
	    str_strcatc(&summary_addon, "with English summary");
	  } else if ((en_title == -1) && (sv_title != -1) && (original_lang == 1) ) {
	    /* nothing to do */
	  }
	}  else if (lang & BIBL_LANGUAGE_SWEDISH ) {
	  if ((en_title != -1) && (sv_title == -1) && (original_lang == 1) && (sv_abstract != -1)) {
	    str_strcatc(&summary_addon, "med svensk sammanfattning");
	  } else if ((en_title != -1) && (sv_title == -1) && (original_lang == 2) && (sv_abstract != -1)) {
	    str_strcatc(&summary_addon, "med svensk sammanfattning");
	  } else if ((en_title != -1) && (sv_title == -1) && (original_lang == 2) && (en_abstract != -1)) {
	    str_strcatc(&summary_addon, "med engelsk sammanfattning");
	  } else if ((en_title == -1) && (sv_title != -1) && (original_lang == 2) && (en_abstract != -1)) {
	    str_strcatc(&summary_addon, "med engelsk sammanfattning");
	  } else if ((en_title == -1) && (sv_title != -1) && (original_lang == 1) && (en_abstract != -1)) {
	    str_strcatc(&summary_addon, "med engelsk sammanfattning");
	  } else if ((en_title == -1) && (sv_title != -1) && (original_lang == 1) && (sv_abstract != -1)) {
	    str_strcatc(&summary_addon, "med svensk sammanfattning");
	  } else if ((en_title != -1) && (sv_title != -1) && (original_lang == 1) && (sv_abstract != -1)) {
	    str_strcatc(&summary_addon, "med svensk sammanfattning");
	  } else if ((en_title != -1) && (sv_title != -1) && (original_lang == 2) && (en_abstract != -1)) {
	    str_strcatc(&summary_addon, "med engelsk sammanfattning");
	  } else if ((en_title == -1) && (sv_title != -1) && (original_lang == 1) ) {
	    /* nothing to do */
	  }
	}

	if ((!str_is_empty(&lang_addon)) || (!str_is_empty(&summary_addon))) {
	  str_strcatc(&complete_addon, " (");
	  if (!str_is_empty(&lang_addon)) {
	    str_strcat(&complete_addon, &lang_addon); 
	  }
	  if (!str_is_empty(&summary_addon)) {
	    str_strcat(&complete_addon, &summary_addon); 
	  }
	  str_strcatc(&complete_addon, ")");

	  str_strcat(&completetitle, &complete_addon);
	}


	/* if there is no title, then set error code and goto out, otherwise store the completetitle */
	if ( str_memerr(&completetitle) ) {
	  ret = BIBL_ERR_MEMERR;
	  goto out;
	}

	if ( str_has_value(&completetitle) ) {
	  status = fields_add( out, bibtag, str_cstr(&completetitle), LEVEL_MAIN );
	  if ( status!=FIELDS_OK ) ret = BIBL_ERR_MEMERR;
	}

 out:
	strs_free(&en_fulltitle, &sv_fulltitle, &fulltitle, &completetitle, &lang_addon, &summary_addon, NULL );	
	return ret;
}

static void
append_titles( fields *in, int type, fields *out, int format_opts, int *status, int lang )
{
	/* item=main level title */
        *status = append_title( in, "title", 0, out, format_opts, lang );
	if ( *status!=BIBL_OK ) return;

	switch( type ) {

		case TYPE_ARTICLE:
		*status = append_title( in, "journal", 1, out, format_opts, lang );
		break;

		case TYPE_INBOOK:
		*status = append_title( in, "bookTitle", 1, out, format_opts, lang );
		if ( *status!=BIBL_OK ) return;
		*status = append_title( in, "series",    2, out, format_opts, lang );
		break;

		case TYPE_INCOLLECTION:
		case TYPE_INPROCEEDINGS:
		*status = append_title( in, "booktitle", 1, out, format_opts, lang );
		if ( *status!=BIBL_OK ) return;
		*status = append_title( in, "series",    2, out, format_opts, lang );
		break;

		case TYPE_PHDTHESIS:
		case TYPE_MASTERSTHESIS:
		*status = append_title( in, "series", 1, out, format_opts, lang );
		break;

		case TYPE_BOOK:
		case TYPE_REPORT:
		case TYPE_COLLECTION:
		case TYPE_PROCEEDINGS:
		*status = append_title( in, "series", 1, out, format_opts, lang );
		if ( *status!=BIBL_OK ) return;
		*status = append_title( in, "series", 2, out, format_opts, lang );
		break;

		default:
		/* do nothing */
		break;

	}
}

static int
find_date( fields *in, char *date_element )
{
	char date[100], partdate[100];
	int n;

	sprintf( date, "DATE:%s", date_element );
	n = fields_find( in, date, LEVEL_ANY );

	if ( n==-1 ) {
		sprintf( partdate, "PARTDATE:%s", date_element );
		n = fields_find( in, partdate, LEVEL_ANY );
	}

	return n;
}

static void
append_date( fields *in, fields *out, int *status )
{
	char *months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", 
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	int n, month, fstatus;

	n = find_date( in, "YEAR" );
	if ( n!=-1 ) {
		fields_setused( in, n );
		fstatus = fields_add( out, "year", in->data[n].data, LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) {
			*status = BIBL_ERR_MEMERR;
			return;
		}
	}

	n = find_date( in, "MONTH" );
	if ( n!=-1 ) {
		fields_setused( in, n );
		month = atoi( in->data[n].data );
		if ( month>0 && month<13 )
			fstatus = fields_add( out, "month", months[month-1], LEVEL_MAIN );
		else
			fstatus = fields_add( out, "month", in->data[n].data, LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) {
			*status = BIBL_ERR_MEMERR;
			return;
		}
	}

	n = find_date( in, "DAY" );
	if ( n!=-1 ) {
		fields_setused( in, n );
		fstatus = fields_add( out, "day", in->data[n].data, LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) {
			*status = BIBL_ERR_MEMERR;
			return;
		}
	}

}

static void
append_arxiv( fields *in, fields *out, int *status )
{
	int n, fstatus1, fstatus2;
	str url;

	n = fields_find( in, "ARXIV", LEVEL_ANY );
	if ( n==-1 ) return;

	fields_setused( in, n );

	/* ...write:
	 *     archivePrefix = "arXiv",
	 *     eprint = "#####",
	 * ...for arXiv references
	 */
	fstatus1 = fields_add( out, "archivePrefix", "arXiv", LEVEL_MAIN );
	fstatus2 = fields_add( out, "eprint", fields_value( in, n, FIELDS_CHRP ), LEVEL_MAIN );
	if ( fstatus1!=FIELDS_OK || fstatus2!=FIELDS_OK ) {
		*status = BIBL_ERR_MEMERR;
		return;
	}

	/* ...also write:
	 *     url = "http://arxiv.org/abs/####",
	 * ...to maximize compatibility
	 */
	str_init( &url );
	arxiv_to_url( in, n, "URL", &url );
	if ( str_has_value( &url ) ) {
		fstatus1 = fields_add( out, "url", str_cstr( &url ), LEVEL_MAIN );
		if ( fstatus1!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
	}
	str_free( &url );
}

static void
append_urls( fields *in, fields *out, int *status )
{
	int lstatus;
	slist types;

	lstatus = slist_init_valuesc( &types, "URL", "DOI", "PMID", "PMC", "JSTOR", NULL );
	if ( lstatus!=SLIST_OK ) {
		*status = BIBL_ERR_MEMERR;
		return;
	}

	*status = urls_merge_and_add( in, LEVEL_ANY, out, "url", LEVEL_MAIN, &types );

	slist_free( &types );
}

static void
append_isi( fields *in, fields *out, int *status )
{
	int n, fstatus;

	n = fields_find( in, "ISIREFNUM", LEVEL_ANY );
	if ( n!=-1 ) {
		fstatus = fields_add( out, "note", fields_value( in, n, FIELDS_CHRP ), LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
	}
}

static int
append_articlenumber( fields *in, fields *out )
{
	int n, fstatus;

	n = fields_find( in, "ARTICLENUMBER", LEVEL_ANY );
	if ( n!=-1 ) {
		fields_setused( in, n );
		fstatus = fields_add( out, "pages", fields_value( in, n, FIELDS_CHRP ), LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) return BIBL_ERR_MEMERR;
	}
	return BIBL_OK;
}

static int
pages_build_pagestr( str *pages, fields *in, int sn, int en, int format_opts )
{
	/* ...append if starting page number is defined */
	if ( sn!=-1 ) {
		str_strcat( pages, fields_value( in, sn, FIELDS_STRP ) );
		fields_setused( in, sn );
	}

	/* ...append dashes if both starting and ending page numbers are defined */
	if ( sn!=-1 && en!=-1 ) {
		if ( format_opts & BIBL_FORMAT_BIBOUT_SINGLEDASH )
			str_strcatc( pages, "-" );
		else
			str_strcatc( pages, "--" );
	}

	/* ...append ending page number is defined */
	if ( en!=-1 ) {
		str_strcat( pages, fields_value( in, en, FIELDS_STRP ) );
		fields_setused( in, en );
	}

	if ( str_memerr( pages ) ) return BIBL_ERR_MEMERR;
	else return BIBL_OK;
}

static int
pages_are_defined( fields *in, int *sn, int *en )
{
	*sn = fields_find( in, "PAGES:START", LEVEL_ANY );
	*en = fields_find( in, "PAGES:STOP",  LEVEL_ANY );
	if ( *sn==-1 && *en==-1 ) return 0;
	else return 1;
}

static void
append_pages( fields *in, fields *out, int format_opts, int *status )
{
	int sn, en, fstatus;
	str pages;

	if ( !pages_are_defined( in, &sn, &en ) ) {
		*status = append_articlenumber( in, out );
		return;
	}

	str_init( &pages );
	*status = pages_build_pagestr( &pages, in, sn, en, format_opts );
	if ( *status==BIBL_OK ) {
		fstatus = fields_add( out, "pages", str_cstr( &pages ), LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
	}
	str_free( &pages );
}

/*
 * from Tim Hicks:
 * I'm no expert on bibtex, but those who know more than I on our mailing 
 * list suggest that 'issue' isn't a recognised key for bibtex and 
 * therefore that bibutils should be aliasing IS to number at some point in 
 * the conversion.
 *
 * Therefore prefer outputting issue/number as number and only keep
 * a distinction if both issue and number are present for a particular
 * reference.
 */

static void
append_issue_number( fields *in, fields *out, int *status )
{
	int nissue  = fields_find( in, "ISSUE",  LEVEL_ANY );
	int nnumber = fields_find( in, "NUMBER", LEVEL_ANY );
	int fstatus;

	if ( nissue!=-1 && nnumber!=-1 ) {
		fields_setused( in, nissue );
		fields_setused( in, nnumber );
		fstatus = fields_add( out, "issue", fields_value( in, nissue, FIELDS_CHRP ), LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) {
			*status = BIBL_ERR_MEMERR;
			return;
		}
		fstatus = fields_add( out, "number", fields_value( in, nnumber, FIELDS_CHRP ), LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
	} else if ( nissue!=-1 ) {
		fields_setused( in, nissue );
		fstatus = fields_add( out, "number", fields_value( in, nissue, FIELDS_CHRP ), LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
	} else if ( nnumber!=-1 ) {
		fields_setused( in, nnumber );
		fstatus = fields_add( out, "number", fields_value( in, nnumber, FIELDS_CHRP ), LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
	}
}

/* added to support KTH DiVA - to support the publisher of the DiVA record */
static void
append_divapublisher( fields *in, char *intag, char *outtag, fields *out, int *status )
{
	int n, fstatus;
	int npublisher;

	npublisher  = fields_find( in, "PUBLISHER",  LEVEL_ANY );
	Da1 fprintf( stderr, "GQMJr::append_divapublisher = %d\n", npublisher); /* added to debug KTH DiVA */

	n = fields_find( in, intag, LEVEL_ANY );
	if ( n != -1 ) {
		fields_setused( in, n );
		if (npublisher == -1) {/* if there is no publisher specified, turn the DIVAPUBLISHER:CORP into publisher. */
		  fstatus = fields_add( out, "publisher", fields_value( in, n, FIELDS_CHRP ), LEVEL_MAIN );
		} else
		  fstatus = fields_add( out, outtag, fields_value( in, n, FIELDS_CHRP ), LEVEL_MAIN );

		if ( fstatus!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
	}
}



/* decode roman numerals from https://rosettacode.org/wiki/Roman_numerals/Decode#C */
int digits[26] = { 0, 0, 100, 500, 0, 0, 0, 0, 1, 1, 0, 50, 1000, 0, 0, 0, 0, 0, 0, 0, 5, 5, 0, 10, 0, 0 };
 
/* assuming ASCII, do upper case and get index in alphabet. could also be
        inline int VALUE(char x) { return digits [ (~0x20 & x) - 'A' ]; }
   if you think macros are evil */
#define VALUE(x) digits[(~0x20 & (x)) - 'A']
 
int romain_numeral_decode(const char * roman)
{
        const char *bigger;
        int current;
        int arabic = 0;
        while (*roman != '\0') {
                current = VALUE(*roman);
                /*      if (!current) return -1;
                        note: -1 can be used as error code; Romans didn't even have zero
                */
                bigger = roman;
 
                /* look for a larger digit, like IV or XM */
                while (VALUE(*bigger) <= current && *++bigger != '\0');
 
                if (*bigger == '\0')
                        arabic += current;
                else {
                        arabic += VALUE(*bigger);
                        while (roman < bigger)
                                arabic -= VALUE(* (roman++) );
                }
 
                roman ++;
        }
        return arabic;
}

/* Check is a string is only digits, if so return 1, else 0 */
int only_arabic_numberal(const char *s)
{
    while (*s) {
        if (isdigit(*s++) == 0) return 0;
    }

    return 1;
}

/* added to support KTH DiVA - to support notes of type "degree" */
/* if p->langauge is set, then use the chosen language for the note about the the degree */
static void
append_note_degree( fields *in, char *intag, char *outtag, fields *out, int *status, int lang )
{
	int i, fstatus;
	int found = 0;
	char extended_intag[256];
	str *field_value;
	char education_program[]="Educational program: ";
	char subject_course[]   ="Subject/course: ";

	extended_intag[0]='\0';

	Da1 fprintf( stderr, "GQMJr::append_note_degree ENTERING\n");

	if (lang & BIBL_LANGUAGE_ENGLISH ) {
	  strcpy(extended_intag, intag);
	  strcat(extended_intag, ":EN");
	} else if (lang & BIBL_LANGUAGE_SWEDISH ) {
	  strcpy(extended_intag, intag);
	  strcat(extended_intag, ":SV");
	}
	Da1 fprintf( stderr, "GQMJr::append_note_degree extended_intag=%s\n", extended_intag);

	if ((lang & BIBL_LANGUAGE_ENGLISH ) || (lang & BIBL_LANGUAGE_SWEDISH )) {
	  for ( i=0; i<in->n; ++i ) {
	    if ( fields_match_tag( in, i, extended_intag ) ) {
	      found = 1;
	      fields_setused( in, i );
	      field_value=fields_value( in, i, FIELDS_STRP );
	      Da1 fprintf( stderr, "GQMJr::append_note_degree field_value=%s\n", field_value->data);
	      if (strncmp(str_cstr(field_value), education_program, strlen(education_program)) ==0) {
		if (lang & BIBL_LANGUAGE_SWEDISH ) {
		  str_findreplace(field_value, education_program, "Utbildningsprogram: " );
		}
	      } else if (strncmp(str_cstr(field_value), subject_course, strlen(subject_course)) ==0) {
		if (lang & BIBL_LANGUAGE_SWEDISH ) {
		  str_findreplace(field_value, subject_course, "Ämne/kurs: " );
		}
	      }

	      fstatus = fields_add( out, outtag, str_cstr( field_value), LEVEL_MAIN );
	      if ( fstatus!=FIELDS_OK ) {
		*status = BIBL_ERR_MEMERR;
		return;
	      }


	    }
	  }
	} else {
	  for ( i=0; i<in->n; ++i ) {
	    if ( fields_match_tag( in, i, intag ) ) {
	      found = 1;
	      fields_setused( in, i );
	      fstatus = fields_add( out, outtag, fields_value( in, i, FIELDS_CHRP ), LEVEL_MAIN );
	      if ( fstatus!=FIELDS_OK ) {
		*status = BIBL_ERR_MEMERR;
		return;
	      }
	    }
	  }
	}

	if (found == 0) {	/* if nothing found above, then try looking for the other language */
	  if (lang & BIBL_LANGUAGE_ENGLISH ) {
	    strcpy(extended_intag, intag);
	    strcat(extended_intag, ":SV");
	    Da1 fprintf( stderr, "GQMJr::append_note_degree extended_intag=%s\n", extended_intag);
	  } else if (lang & BIBL_LANGUAGE_SWEDISH ) {
	    strcpy(extended_intag, intag);
	    strcat(extended_intag, ":EN");
	    Da1 fprintf( stderr, "GQMJr::append_note_degree extended_intag=%s\n", extended_intag);
	  }

	  for ( i=0; i<in->n; ++i ) {
	    if ( fields_match_tag( in, i, extended_intag ) ) {
	      found = 1;
	      fields_setused( in, i );
	      fstatus = fields_add( out, outtag, fields_value( in, i, FIELDS_CHRP ), LEVEL_MAIN );
	      if ( fstatus!=FIELDS_OK ) {
		*status = BIBL_ERR_MEMERR;
		return;
	      }
	    }
	  }
	}
}

/* added to support KTH DiVA - to support notes of type "level" */
/* if p->langauge is set, then use the chosen language for the note about the the level */
typedef struct eng_to_swe {
  char *eng;     /* English version; */
  char *swe;	 /* Swedish version */
  char *swetex;	 /* Swedish version in TeX format */
} eng_to_swe;


static void
append_note_level( fields *in, char *intag, char *outtag, fields *out, int *status, int lang )
{
  /*
   * Note that this translation table is needed to go from the Swedish names for the degree to the English form.
   * These strings were taken from the Javascript for the page to manually enter a new publication in DiVA.
   *  The URL to the page is: https://kth.diva-portal.org/dream/add/add2.jsf?rvn=1
   *
   * GQMJr: I am not sure why one gets the TeX version of the character strings.
   */
  eng_to_swe translate_level[]=
    {
      {"Independent thesis Advanced level (degree of Master (One Year))",
       "Självständigt arbete på avancerad nivå (magisterexamen)",
       "Sj{\\\"a}lvst{\\\"a}ndigt arbete pp{\\aa} avancerad nivp{\\aa} (magisterexamen)" }, /* H1 */
      {"Independent thesis Advanced level (degree of Master (Two Years))",
       "Självständigt arbete på avancerad nivå (masterexamen)", 
       "Sj{\\\"a}lvst{\\\"a}ndigt arbete p{\\aa} avancerad niv{\\aa} (masterexamen)"}, /* H2 */
      {"Independent thesis Advanced level (professional degree)", 
       "Självständigt arbete på avancerad nivå (yrkesexamen)",
       "Sj{\\\"a}lvst{\\\"a}ndigt arbete p{\\aa} avancerad niv{\\aa} (yrkesexamen)"  }, /* H3 */

      {"Independent thesis Basic level (university diploma)",
       "Självständigt arbete på grundnivå (högskoleexamen)", 
       "Sj{\\\"a}lvst{\\\"a}ndigt arbete p{\\aa} grundniv{\\aa} (högskoleexamen)"}, /* M1 */
      {"Independent thesis Basic level (degree of Bachelor)",
       "Självständigt arbete på grundnivå (kandidatexamen)",
       "Sj{\\\"a}lvst{\\\"a}ndigt arbete p{\\aa} grundniv{\\aa} (kandidatexamen)"},  /* M2 */
      {"Independent thesis Basic level (professional degree)", 
       "Självständigt arbete på grundnivå (yrkesexamen)", 
       "Sj{\\\"a}lvst{\\\"a}ndigt arbete p{\\aa} grundniv{\\aa} (yrkesexamen)"}, /* M3 */
      {"Independent thesis Basic level (Higher Education Diploma (Fine Arts))", 
       "Självständigt arbete på grundnivå (konstnärlig högskoleexamen)", 
      "Sj{\\\"a}lvst{\\\"a}ndigt arbete p{\\aa} grundniv{\\aa} (konstnärlig högskoleexamen)"}, /* M4 */
      {"Independent thesis Basic level (degree of Bachelor of Fine Arts)",
       "Självständigt arbete på grundnivå (konstnärlig kandidatexamen)",
       "Sj{\\\"a}lvst{\\\"a}ndigt arbete p{\\aa} grundniv{\\aa} (konstnärlig kandidatexamen)"}, /* M5 */

      {"Student paper first term", 
       "Studentarbete första termin",
       "Studentarbete första termin"}, /* L1 */
      {"Student paper second term",
       "Studentarbete andra termin",
       "Studentarbete andra termin"}, /* L2 */
      {"Student paper other", 
       "Studentarbete övrigt",
       "Studentarbete övrigt"}, /* L3 */
    };

  int ntranslate_level = sizeof (translate_level) / sizeof (translate_level[0]);
  int i, j, fstatus;
  char extended_intag[256];
  int swe_found = 0;
  int found = 0;
  char *field_value;

  extended_intag[0]='\0';

  Da1 fprintf( stderr, "GQMJr::append_note_level ntranslate_level=%d\n", ntranslate_level);

  /* Note that this value only seems to be given in Swedish */
#ifdef NEVER
  if (lang & BIBL_LANGUAGE_ENGLISH ) {
    strcpy(extended_intag, intag);
    strcat(extended_intag, ":EN");
  }
  if (lang & BIBL_LANGUAGE_SWEDISH ) {
#endif
    strcpy(extended_intag, intag);
    strcat(extended_intag, ":SV");
#ifdef NEVER
  }
#endif

  Da1 fprintf( stderr, "GQMJr::append_note_level extended_intag=%s\n", extended_intag);

  if ((lang & BIBL_LANGUAGE_ENGLISH ) || (lang & BIBL_LANGUAGE_SWEDISH )) {
    for ( i=0; i<in->n; ++i ) {
      Da1 fprintf( stderr, "GQMJr::append_note_level i=%d, fields_tag=%s\n", i, (char *)fields_tag( in, i, FIELDS_CHRP ));
      if ( fields_match_tag( in, i, extended_intag ) ) {
	found = 1;
	Da1 fprintf( stderr, "GQMJr::append_note_level found=%d\n", i);

	Da1 fprintf( stderr, "GQMJr::append_note_level found = %d\n", found); /* added to debug KTH DiVA */
	fields_setused( in, i );
	field_value=fields_value( in, i, FIELDS_CHRP );
	Da1 fprintf( stderr, "GQMJr::append_note_level field_value = %s\n", field_value); /* added to debug KTH DiVA */

	if ((lang & BIBL_LANGUAGE_ENGLISH )) {
	  for ( j=0; j<ntranslate_level && swe_found==0; j++ ) {
	    Da1 fprintf( stderr, "GQMJr::append_note_level swetex = %s and eng = %s\n", translate_level[j].swetex, translate_level[j].eng); /* added to debug KTH DiVA */
	    if (strncmp(field_value, translate_level[j].swetex, strlen(translate_level[j].swetex)) == 0)  {
	      Da1 fprintf( stderr, "GQMJr::append_note_level *** swetex = %s and eng = %s\n", translate_level[j].swetex, translate_level[j].eng); /* added to debug KTH DiVA */
	      field_value = translate_level[j].eng;
	      swe_found=1;
	    }
	  }
	}

	fstatus = fields_add( out, outtag, field_value, LEVEL_MAIN );
	if ( fstatus!=FIELDS_OK ) {
	  *status = BIBL_ERR_MEMERR;
	  return;
	}
      }
    }
  } else {
    for ( i=0; i<in->n; ++i ) {
      if ( fields_match_tag( in, i, intag ) ) {
	fields_setused( in, i );
	fstatus = fields_add( out, outtag, fields_value( in, i, FIELDS_CHRP ), LEVEL_MAIN );
	if ( fstatus!=FIELDS_OK ) {
	  *status = BIBL_ERR_MEMERR;
	  return;
	}
      }
    }
  }

  if (found == 0) {	/* if nothing found above, then try looking for the other language */
    if (lang & BIBL_LANGUAGE_ENGLISH ) {
      strcpy(extended_intag, intag);
      strcat(extended_intag, ":SV");
      Da1 fprintf( stderr, "GQMJr::append_note_level extended_intag=%s\n", extended_intag);
    } else if (lang & BIBL_LANGUAGE_SWEDISH ) {
      strcpy(extended_intag, intag);
      strcat(extended_intag, ":EN");
      Da1 fprintf( stderr, "GQMJr::append_note_level extended_intag=%s\n", extended_intag);
    }

    for ( i=0; i<in->n; ++i ) {
      if ( fields_match_tag( in, i, extended_intag ) ) {
	found = 1;
	fields_setused( in, i );
	fstatus = fields_add( out, outtag, fields_value( in, i, FIELDS_CHRP ), LEVEL_MAIN );
	if ( fstatus!=FIELDS_OK ) {
	  *status = BIBL_ERR_MEMERR;
	  return;
	}
      }
    }
  }

}

static void
append_subject( fields *in, char *intag, char *outtag, fields *out, int *status, int lang )
{
	int fstatus;
	char extended_intag[256];
	int found = 0;
	vplist_index i;
	vplist a;
	str subjects, *word;

	str_init( &subjects );
	vplist_init( &a );

	extended_intag[0]='\0';

	if (lang & BIBL_LANGUAGE_ENGLISH ) {
	  strcpy(extended_intag, intag);
	  strcat(extended_intag, ":EN");
	} else if (lang & BIBL_LANGUAGE_SWEDISH ) {
	  strcpy(extended_intag, intag);
	  strcat(extended_intag, ":SV");
	} else
	  strcpy(extended_intag, intag);

	Da1 fprintf( stderr, "GQMJr::append_subject extended_intag=%s\n", extended_intag);

	if ((lang & BIBL_LANGUAGE_ENGLISH ) || (lang & BIBL_LANGUAGE_SWEDISH )) {
	  fields_findv_each( in, LEVEL_ANY, FIELDS_STRP, &a, extended_intag );

	  if ( a.n ) {
	    found = 1;
	    for ( i=0; i<a.n; ++i ) {
	      word = vplist_get( &a, i );
	      if ( i>0 ) str_strcatc( &subjects, "; " );
	      str_strcat( &subjects, word );
	    }

	    if ( str_memerr( &subjects ) ) { *status = BIBL_ERR_MEMERR; goto out; }

	    fstatus = fields_add( out, outtag, str_cstr( &subjects ), LEVEL_MAIN );
	    if ( fstatus!=FIELDS_OK ) {
	      *status = BIBL_ERR_MEMERR;
	      goto out;
	    }
	  }
	}

	if (found == 0) {	/* if nothing found above, then try looking for the other language */
	  if (lang & BIBL_LANGUAGE_ENGLISH ) {
	    strcpy(extended_intag, intag);
	    strcat(extended_intag, ":SV");
	  } else if (lang & BIBL_LANGUAGE_SWEDISH ) {
	    strcpy(extended_intag, intag);
	    strcat(extended_intag, ":EN");
	  }
	  Da1 fprintf( stderr, "GQMJr::append_subject extended_intag=%s\n", extended_intag);

	  fields_findv_each( in, LEVEL_ANY, FIELDS_STRP, &a, extended_intag );

	  if ( a.n ) {
	    found = 1;
	    for ( i=0; i<a.n; ++i ) {
	      word = vplist_get( &a, i );
	      if ( i>0 ) str_strcatc( &subjects, "; " );
	      str_strcat( &subjects, word );
	    }

	    if ( str_memerr( &subjects ) ) { *status = BIBL_ERR_MEMERR; goto out; }

	    fstatus = fields_add( out, outtag, str_cstr( &subjects ), LEVEL_MAIN );
	    if ( fstatus!=FIELDS_OK ) {
	      *status = BIBL_ERR_MEMERR;
	      goto out;
	    }
	  }
	}
 out:
	str_free( &subjects );
	vplist_free( &a );
}

/* added to support KTH DiVA - to support abstract with a given/optional language */
/* if p->langauge is set, then use the chosen language for the note about the the level */
static void
append_abstract( fields *in, char *intag, char *outtag, fields *out, int *status, int lang )
{
	int i, fstatus;
	char extended_intag[256];
	int found = 0;

	extended_intag[0]='\0';

	if (lang & BIBL_LANGUAGE_ENGLISH ) {
	  strcpy(extended_intag, intag);
	  strcat(extended_intag, ":EN");
	  Da1 fprintf( stderr, "GQMJr::append_abstract extended_intag=%s\n", extended_intag);
	} else if (lang & BIBL_LANGUAGE_SWEDISH ) {
	  strcpy(extended_intag, intag);
	  strcat(extended_intag, ":SV");
	  Da1 fprintf( stderr, "GQMJr::append_abstract extended_intag=%s\n", extended_intag);
	}

	if ((lang & BIBL_LANGUAGE_ENGLISH ) || (lang & BIBL_LANGUAGE_SWEDISH )) {
	  for ( i=0; i<in->n; ++i ) {
	    if ( fields_match_tag( in, i, extended_intag ) ) {
	      found = 1;
	      fields_setused( in, i );
	      fstatus = fields_add( out, outtag, fields_value( in, i, FIELDS_CHRP ), LEVEL_MAIN );
	      if ( fstatus!=FIELDS_OK ) {
		*status = BIBL_ERR_MEMERR;
		return;
	      }
	    }
	  }
	} else {
	  for ( i=0; i<in->n; ++i ) {
	    if ( fields_match_tag( in, i, intag ) ) {
	      fields_setused( in, i );
	      fstatus = fields_add( out, outtag, fields_value( in, i, FIELDS_CHRP ), LEVEL_MAIN );
	      if ( fstatus!=FIELDS_OK ) {
		*status = BIBL_ERR_MEMERR;
		return;
	      }
	    }
	  }
	}

	if (found == 0) {	/* if nothing found above, then try looking for the other language */
	  if (lang & BIBL_LANGUAGE_ENGLISH ) {
	    strcpy(extended_intag, intag);
	    strcat(extended_intag, ":SV");
	    Da1 fprintf( stderr, "GQMJr::append_abstract extended_intag=%s\n", extended_intag);
	  } else if (lang & BIBL_LANGUAGE_SWEDISH ) {
	    strcpy(extended_intag, intag);
	    strcat(extended_intag, ":EN");
	    Da1 fprintf( stderr, "GQMJr::append_abstract extended_intag=%s\n", extended_intag);
	  }

	  for ( i=0; i<in->n; ++i ) {
	    if ( fields_match_tag( in, i, extended_intag ) ) {
	      found = 1;
	      fields_setused( in, i );
	      fstatus = fields_add( out, outtag, fields_value( in, i, FIELDS_CHRP ), LEVEL_MAIN );
	      if ( fstatus!=FIELDS_OK ) {
		*status = BIBL_ERR_MEMERR;
		return;
	      }
	    }
	  }
	}

}



/* added to support KTH DiVA - to support a description (from an <extent>) */
static void
append_description(int type, fields *in, char *intag, char *outtag, fields *out, int *status )
{
	int n, fstatus;
	char *description_string;

	n = fields_find( in, intag, LEVEL_ANY );

	if ( n!=-1 ) {
		fields_setused( in, n );
		if (type == TYPE_BOOK) {  /* added to support KTH DiVA */
		  Da1 fprintf( stderr, "GQMJr::append_description type=%d\n", type);
		  Da1 fprintf( stderr, "GQMJr::append_description intage=%s\n", intag);
		  Da1 fprintf( stderr, "GQMJr::append_description outtage=%s\n", outtag);
		  Da1 fprintf( stderr, "GQMJr::append_description n=%d\n", n);
		  description_string=in->data[n].data;
		  Da1 fprintf( stderr, "GQMJr::append_description description_string=%s\n", description_string);
		  /* check the description string to see if it of the form roman_numerals,arabic_numerals or arabic_numerals  */
		  /* if so, then this is a set of page numbers or simply the number of pages in the book */

		  /* check for a comma; if there is check if the string is of the form roman_numeral,arabic_numeral */
		  char *commaPtr = strchr(description_string, ',');
		  if (commaPtr == NULL) {
		    Da1 fprintf( stderr, "GQMJr::append_description comma not found\n");
		    // check is the string is just an Arabic numeral
		    int digitsp = only_arabic_numberal(description_string);
		    Da1 fprintf( stderr, "GQMJr::append_description digitsp=%d\n", digitsp);
		    if (digitsp > 0) {
		      fstatus = fields_add( out, "pages", description_string, LEVEL_MAIN );
		    } else {	/* otherwise output the description */
		      fstatus = fields_add( out, outtag, fields_value( in, n, FIELDS_CHRP ), LEVEL_MAIN );
		      if ( fstatus!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
		    }
		  } else {	/* there was a comma, now check the two strings */
		    Da1 fprintf( stderr, "GQMJr::append_description comma found arabic pages %s\n", commaPtr+1 );

		    int position = commaPtr - description_string;
		    char* romanValue = (char*) malloc((position + 1) * sizeof(char));
		    memcpy(romanValue, description_string, position);
		    romanValue[position] = '\0';
		  
		    Da1 fprintf( stderr, "GQMJr::append_description comma found arabic numeral for pages %s\n", romanValue );
		    int rnv=romain_numeral_decode(romanValue);
		    if (rnv > 0) {
		      Da1 fprintf( stderr, "GQMJr::append_description romain numeral equivalent to %d\n", rnv );
		      // fstatus = fields_add( out, outtag, description_string, LEVEL_MAIN );
		      fstatus = fields_add( out, "pages", description_string, LEVEL_MAIN );
		      if ( fstatus!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
		    } else {	/* otherwise output the description */
		      fstatus = fields_add( out, outtag, fields_value( in, n, FIELDS_CHRP ), LEVEL_MAIN );
		      if ( fstatus!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
		    }
		  }
		} else {	/* for non-books, simply output the description */
		  fstatus = fields_add( out, outtag, fields_value( in, n, FIELDS_CHRP ), LEVEL_MAIN );
		  if ( fstatus!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
		}
	}
}

static int
append_data( fields *in, fields *out, param *p, unsigned long refnum )
{
	int type, status = BIBL_OK;

	type = bibtexout_type( in, "", refnum, p );
	Da1 fprintf( stderr, "GQMJr::append_data type = %d\n", type); /* added to debug KTH DiVA */
	Da1 fprintf( stderr, "GQMJr::append_data p->language = %d\n", p->language); /* added to debug KTH DiVA */

	append_type        ( type, out, &status );
	append_citekey     ( in, out, p->format_opts, &status );
	append_people      ( in, "AUTHOR",     "AUTHOR:CORP",     "AUTHOR:ASIS",     "author", 0, out, p->format_opts );
	append_people      ( in, "EDITOR",     "EDITOR:CORP",     "EDITOR:ASIS",     "editor", -1, out, p->format_opts );
	append_people      ( in, "TRANSLATOR", "TRANSLATOR:CORP", "TRANSLATOR:ASIS", "translator", -1, out, p->format_opts );
	append_titles      ( in, type, out, p->format_opts, &status, p->language );
	append_date        ( in, out, &status );
	append_simple      ( in, "EDITION",            "edition",   out, &status );
	append_simple      ( in, "PUBLISHER",          "publisher", out, &status );
	append_simple      ( in, "PUBLISHER:CORP",     "publisher", out, &status ); /*  KTH DiVA - to support the publisher of a thesis */
	append_divapublisher( in, "DIVAPUBLISHER:CORP", "divapublisher", out, &status ); /*  KTH DiVA - to support the publisher of the DiVA record */
	append_people      ( in, "THESIS_ADVISOR", "", "", "supervisor", 0, out, p->format_opts ); /* KTH DiVA - to support the role advisor of a thesis */
	append_people      ( in, "THESIS_EXAMINER", "", "", "examiner", 0, out, p->format_opts ); /* KTH DiVA - to support the role of examiner of a thesis */
	append_people      ( in, "THESIS_OTHER:CORP", "", "", "other", 0, out, p->format_opts ); /*  KTH DiVA - to support the role of "other" of a thesis (this seems to represent the research group where the project was done) */
	append_people      ( in, "THESIS_OPPONENT", "", "", "opponent", 0, out, p->format_opts ); /*  KTH DiVA - to support the role of opponent of a thesis */
	append_simple      ( in, "ADDRESS",            "address",   out, &status );
	append_simple      ( in, "VOLUME",             "volume",    out, &status );
	append_issue_number( in, out, &status );
	append_pages       ( in, out, p->format_opts, &status );
	append_keywords    ( in, out, &status, p->language );
	append_simple      ( in, "CONTENTS",           "contents",  out, &status );
	append_simple      ( in, "ABSTRACT",           "abstract",  out, &status );
	append_simple      ( in, "LOCATION",           "location",  out, &status );
	append_simple      ( in, "DEGREEGRANTOR",      "school",    out, &status );
	append_simple      ( in, "DEGREEGRANTOR:ASIS", "school",    out, &status );
	append_simple      ( in, "DEGREEGRANTOR:CORP", "school",    out, &status );
	append_simple      ( in, "NOTES:THESIS",       "note_thesis",    out, &status );         /* KTH DiVA */
	append_simple      ( in, "NOTES:VENUE",        "venue",     out, &status );               /* KTH DiVA */
	append_simple      ( in, "NOTES:UNIVERSITYCREDITS",        "credits",    out, &status ); /* KTH DiVA */
	append_simple      ( in, "NOTES:COOPERATION",  "cooperation",    out, &status );               /* KTH DiVA */
	append_note_degree ( in, "NOTES:DEGREE",       "degree",    out, &status, p->language ); /* KTH DiVA */
	append_note_level  ( in, "NOTES:LEVEL",        "level",     out, &status, p->language );  /* KTH DiVA */
	append_abstract    ( in, "ABSTRACT",           "abstract",  out, &status, p->language );  /* KTH DiVA */
	append_subject     ( in, "SUBJECT",            "subject",   out, &status, p->language );  /* KTH DiVA */

	append_simple      ( in, "URI",                "uri",       out, &status );
	append_simple      ( in, "recordOrigin",       "recordOrigin",    out, &status );         /* KTH DiVA */
	append_simple      ( in, "recordContentSource","recordContentSource",    out, &status );         /* KTH DiVA */
	append_simple      ( in, "recordCreationDate", "recordCreationDate",    out, &status );         /* KTH DiVA */
	append_simple      ( in, "recordChangeDate",   "recordChangeDate",    out, &status );         /* KTH DiVA */

	append_simpleall   ( in, "NOTES",              "note",      out, &status );
	append_simpleall   ( in, "ANNOTE",             "annote",    out, &status );
	append_simple      ( in, "ISBN",               "isbn",      out, &status );
	append_simple      ( in, "ISSN",               "issn",      out, &status );
	append_simple      ( in, "MRNUMBER",           "mrnumber",  out, &status );
	append_simple      ( in, "CODEN",              "coden",     out, &status );
	append_simple      ( in, "DOI",                "doi",       out, &status );
	append_urls        ( in, out, &status );
	append_fileattach  ( in, out, &status );
	append_arxiv       ( in, out, &status );
	append_simple      ( in, "EPRINTCLASS",        "primaryClass", out, &status );
	append_isi         ( in, out, &status );
	append_simple      ( in, "LANGUAGE",           "language",  out, &status );
	append_simple      ( in, "EVENT",              "eventtitle",  out, &status ); /* added to support KTH DiVA - the name of a conference */
	append_description (type, in, "DESCRIPTION",   "description", out, &status ); /* added to support KTH DiVA - to support <extent>*/

	return status;
}

static int
bibtexout_write( fields *in, FILE *fp, param *p, unsigned long refnum )
{
	int status;
	fields out;

	fields_init( &out );

	status = append_data( in, &out, p, refnum );
	if ( status==BIBL_OK ) output( fp, &out, p->format_opts );

	fields_free( &out );

	return status;
}

static void
bibtexout_writeheader( FILE *outptr, param *p )
{
	if ( p->utf8bom ) utf8_writebom( outptr );
}

