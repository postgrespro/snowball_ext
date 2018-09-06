
/*-------------------------------------------------------------------------
 *
 * snowball_ext.c
 *	  Snowball dictionary as extension
 *
 * Copyright (c) 2007-2018, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  contrib/snowball_ext/snowball_ext.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "commands/defrem.h"
#include "tsearch/ts_locale.h"
#include "tsearch/ts_utils.h"

/* Now we can include the original Snowball header.h */
#include "snowball/libstemmer/header.h"
#include "libstemmer/stem_UTF_8_nepali.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(dsnowball_ext_init);
PG_FUNCTION_INFO_V1(dsnowball_ext_lexize);

/* List of supported modules */
typedef struct stemmer_module
{
	const char *name;
	pg_enc		enc;
	struct SN_env *(*create) (void);
	void		(*close) (struct SN_env *);
	int			(*stem) (struct SN_env *);
} stemmer_module;

static const stemmer_module stemmer_modules[] =
{
	/*
	 * Stemmers list from Snowball distribution
	 */
	{"nepali", PG_UTF8, nepali_UTF_8_create_env, nepali_UTF_8_close_env, nepali_UTF_8_stem},

	{NULL, 0, NULL, NULL, NULL} /* list end marker */
};

typedef struct DictSnowball
{
	struct SN_env *z;
	StopList	stoplist;
	bool		needrecode;		/* needs recoding before/after call stem */
	int			(*stem) (struct SN_env *z);

	/*
	 * snowball saves alloced memory between calls, so we should run it in our
	 * private memory context. Note, init function is executed in long lived
	 * context, so we just remember CurrentMemoryContext
	 */
	MemoryContext dictCtx;
} DictSnowball;

static void
locate_stem_module(DictSnowball *d, char *lang)
{
	const stemmer_module *m;

	/*
	 * First, try to find exact match of stemmer module. Stemmer with
	 * PG_SQL_ASCII encoding is treated as working with any server encoding
	 */
	for (m = stemmer_modules; m->name; m++)
	{
		if ((m->enc == PG_SQL_ASCII || m->enc == GetDatabaseEncoding()) &&
			pg_strcasecmp(m->name, lang) == 0)
		{
			d->stem = m->stem;
			d->z = m->create();
			d->needrecode = false;
			return;
		}
	}

	/*
	 * Second, try to find stemmer for needed language for UTF8 encoding.
	 */
	for (m = stemmer_modules; m->name; m++)
	{
		if (m->enc == PG_UTF8 && pg_strcasecmp(m->name, lang) == 0)
		{
			d->stem = m->stem;
			d->z = m->create();
			d->needrecode = true;
			return;
		}
	}

	ereport(ERROR,
			(errcode(ERRCODE_UNDEFINED_OBJECT),
			 errmsg("no Snowball stemmer available for language \"%s\" and encoding \"%s\"",
					lang, GetDatabaseEncodingName())));
}

Datum
dsnowball_ext_init(PG_FUNCTION_ARGS)
{
	List	   *dictoptions = (List *) PG_GETARG_POINTER(0);
	DictSnowball *d;
	bool		stoploaded = false;
	ListCell   *l;

	d = (DictSnowball *) palloc0(sizeof(DictSnowball));

	foreach(l, dictoptions)
	{
		DefElem    *defel = (DefElem *) lfirst(l);

		if (pg_strcasecmp("StopWords", defel->defname) == 0)
		{
			if (stoploaded)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("multiple StopWords parameters")));
			readstoplist(defGetString(defel), &d->stoplist, lowerstr);
			stoploaded = true;
		}
		else if (pg_strcasecmp("Language", defel->defname) == 0)
		{
			if (d->stem)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("multiple Language parameters")));
			locate_stem_module(d, defGetString(defel));
		}
		else
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("unrecognized Snowball parameter: \"%s\"",
							defel->defname)));
		}
	}

	if (!d->stem)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("missing Language parameter")));

	d->dictCtx = CurrentMemoryContext;

	PG_RETURN_POINTER(d);
}

Datum
dsnowball_ext_lexize(PG_FUNCTION_ARGS)
{
	DictSnowball *d = (DictSnowball *) PG_GETARG_POINTER(0);
	char	   *in = (char *) PG_GETARG_POINTER(1);
	int32		len = PG_GETARG_INT32(2);
	char	   *txt = lowerstr_with_len(in, len);
	TSLexeme   *res = palloc0(sizeof(TSLexeme) * 2);

	if (*txt == '\0' || searchstoplist(&(d->stoplist), txt))
	{
		pfree(txt);
	}
	else
	{
		MemoryContext saveCtx;

		/*
		 * recode to utf8 if stemmer is utf8 and doesn't match server encoding
		 */
		if (d->needrecode)
		{
			char	   *recoded;

			recoded = pg_server_to_any(txt, strlen(txt), PG_UTF8);
			if (recoded != txt)
			{
				pfree(txt);
				txt = recoded;
			}
		}

		/* see comment about d->dictCtx */
		saveCtx = MemoryContextSwitchTo(d->dictCtx);
		SN_set_current(d->z, strlen(txt), (symbol *) txt);
		d->stem(d->z);
		MemoryContextSwitchTo(saveCtx);

		if (d->z->p && d->z->l)
		{
			txt = repalloc(txt, d->z->l + 1);
			memcpy(txt, d->z->p, d->z->l);
			txt[d->z->l] = '\0';
		}

		/* back recode if needed */
		if (d->needrecode)
		{
			char	   *recoded;

			recoded = pg_any_to_server(txt, strlen(txt), PG_UTF8);
			if (recoded != txt)
			{
				pfree(txt);
				txt = recoded;
			}
		}

		res->lexeme = txt;
	}

	PG_RETURN_POINTER(res);
}