# contrib/snowball_ext/Makefile

MODULE_big = snowball_ext

EXTENSION = snowball_ext
PGFILEDESC = "snowball_ext - add-on dictionary template with natural language stemmers"

REGRESS = snowball_nepali
ENCODING = UTF8
OBJS= snowball_ext.o libstemmer/api.o libstemmer/utilities.o \
	libstemmer/stem_UTF_8_nepali.o $(WIN32RES)

SQLSCRIPT= snowball_ext--1.0.sql
DATA_built = $(SQLSCRIPT)

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

override CPPFLAGS += -I$(CURDIR)/libstemmer $(CPPFLAGS)
VPATH += $(CURDIR)/libstemmer
else
subdir = contrib/snowball_ext
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk

override CPPFLAGS += -I$(top_srcdir)/$(subdir)/libstemmer $(CPPFLAGS)
VPATH += $(top_srcdir)/$(subdir)/libstemmer
endif

# first column is language name and also name of dictionary for not-all-ASCII
# words, second is name of dictionary for all-ASCII words
# Note order dependency: use of some other language as ASCII dictionary
# must come after creation of that language
LANGUAGES=  \
	nepali		nepali

all: $(SQLSCRIPT);

$(SQLSCRIPT): snowball_ext_func.sql.in snowball_ext.sql.in
	echo '-- Language-specific snowball dictionaries' > $@
	cat $(srcdir)/snowball_ext_func.sql.in >> $@
	@set -e; \
	set $(LANGUAGES) ; \
	while [ "$$#" -gt 0 ] ; \
	do \
		lang=$$1; shift; \
		nonascdictname=$$lang; \
		ascdictname=$$1; shift; \
		if [ -s $(srcdir)/stopwords/$${lang}.stop ] ; then \
			stop=", StopWords=$${lang}" ; \
		else \
			stop=""; \
		fi; \
		cat $(srcdir)/snowball_ext.sql.in | \
			sed -e "s#_LANGNAME_#$$lang#g" | \
			sed -e "s#_DICTNAME_#$${lang}_stem#g" | \
			sed -e "s#_CFGNAME_#$$lang#g" | \
			sed -e "s#_ASCDICTNAME_#$${ascdictname}_stem#g" | \
			sed -e "s#_NONASCDICTNAME_#$${nonascdictname}_stem#g" | \
			sed -e "s#_STOPWORDS_#$$stop#g" ; \
	done >> $@
