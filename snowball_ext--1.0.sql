-- Language-specific snowball dictionaries
/*
 * Create underlying C functions for Snowball stemmers
 *
 * Copyright (c) 2007-2018, PostgreSQL Global Development Group
 *
 * contrib/snowball_ext/snowball_ext_func.sql.in
 *
 * This file is combined with multiple instances of snowball_ext.sql.in to
 * build snowball_ext--1.0.sql.
 *
 * Note: this file is read in single-user -j mode, which means that the
 * command terminator is semicolon-newline-newline; whenever the backend
 * sees that, it stops and executes what it's got.  If you write a lot of
 * statements without empty lines between, they'll all get quoted to you
 * in any error message about one of them, so don't do that.  Also, you
 * cannot write a semicolon immediately followed by an empty line in a
 * string literal (including a function body!) or a multiline comment.
 */

CREATE FUNCTION dsnowball_ext_init(INTERNAL)
    RETURNS INTERNAL AS 'MODULE_PATHNAME', 'dsnowball_ext_init'
LANGUAGE C STRICT;

CREATE FUNCTION dsnowball_ext_lexize(INTERNAL, INTERNAL, INTERNAL, INTERNAL)
    RETURNS INTERNAL AS 'MODULE_PATHNAME', 'dsnowball_ext_lexize'
LANGUAGE C STRICT;

CREATE TEXT SEARCH TEMPLATE snowball_ext
	(INIT = dsnowball_ext_init,
	LEXIZE = dsnowball_ext_lexize);

COMMENT ON TEXT SEARCH TEMPLATE snowball_ext IS 'snowball stemmer';
/*
 * text search configuration for nepali language
 *
 * Copyright (c) 2007-2018, PostgreSQL Global Development Group
 *
 * contrib/snowball_ext/snowball_ext.sql.in
 *
 * nepali and certain other macros are replaced for each language;
 * see the Makefile for details.
 *
 * Note: this file is read in single-user -j mode, which means that the
 * command terminator is semicolon-newline-newline; whenever the backend
 * sees that, it stops and executes what it's got.  If you write a lot of
 * statements without empty lines between, they'll all get quoted to you
 * in any error message about one of them, so don't do that.  Also, you
 * cannot write a semicolon immediately followed by an empty line in a
 * string literal (including a function body!) or a multiline comment.
 */

CREATE TEXT SEARCH DICTIONARY nepali_stem
	(TEMPLATE = snowball_ext, Language = nepali );

COMMENT ON TEXT SEARCH DICTIONARY nepali_stem IS 'snowball stemmer for nepali language';

CREATE TEXT SEARCH CONFIGURATION nepali
	(PARSER = default);

COMMENT ON TEXT SEARCH CONFIGURATION nepali IS 'configuration for nepali language';

ALTER TEXT SEARCH CONFIGURATION nepali ADD MAPPING
	FOR email, url, url_path, host, file, version,
	    sfloat, float, int, uint,
	    numword, hword_numpart, numhword
	WITH simple;

ALTER TEXT SEARCH CONFIGURATION nepali ADD MAPPING
    FOR asciiword, hword_asciipart, asciihword
	WITH nepali_stem;

ALTER TEXT SEARCH CONFIGURATION nepali ADD MAPPING
    FOR word, hword_part, hword
	WITH nepali_stem;
