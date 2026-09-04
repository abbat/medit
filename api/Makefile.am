docparser_files =		\
	parsedocs.py		\
	mdp/__init__.py		\
	mdp/module.py		\
	mdp/docparser.py	\
	mdp/xmlwriter.py

gendefs_files =			\
	gendefs.py		\
	mpi/__init__.py		\
	mpi/module.py		\
	mpi/defswriter.py

gendocs_files =			\
	gendocs.py		\
	gendocbook.py		\
	mpi/__init__.py		\
	mpi/module.py		\
	mpi/docbookwriter.py

EXTRA_DIST =			\
	$(docparser_files)	\
	$(gendefs_files)	\
	$(gendocs_files)	\
	sourcefiles.mak

BUILT_SOURCES =

if MOO_DEV_MODE

include sourcefiles.mak

endif
