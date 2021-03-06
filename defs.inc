PROGRAM = dsbmixer
BACKEND = $${PROGRAM}_backend

isEmpty(PREFIX) {
	PREFIX=/usr/local
}

isEmpty(DATADIR) {
	DATADIR=$${PREFIX}/share/$${PROGRAM}
}

BACKEND_INSTALL_DIR = $${PREFIX}/libexec
APPSDIR		    = $${PREFIX}/share/applications
PATH_BACKEND	    = $${BACKEND_INSTALL_DIR}/$${BACKEND}
PATH_DSBWRTSYSCTL   = $${PREFIX}/bin/dsbwrtsysctl

INSTALL_SETUID	    = install -s -g wheel -m 4750 -o root

DEFINES += PATH_BACKEND=\\\"$${PATH_BACKEND}\\\"
DEFINES += PROGRAM=\\\"$${PROGRAM}\\\"
DEFINES += LOCALE_PATH=\\\"$${DATADIR}\\\"
