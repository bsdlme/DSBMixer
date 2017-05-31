/*-
 * Copyright (c) 2016 Marcel Kaiser. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <QTabWidget>
#include <QTimer>
#include <QMenu>
#include <QMenuBar>
#include <unistd.h>
#include "mainwin.h"
#include "thread.h"
#include "mixer.h"
#include "preferences.h"
#include "qt-helper/qt-helper.h"

MainWin::MainWin(QWidget *parent)
	: QMainWindow(parent) {
	traytimer     = new QTimer(this);
	QTimer *timer = new QTimer(this);

	cfg = dsbcfg_read(PROGRAM, "config", vardefs, CFG_NVARS);
	if (cfg == NULL && errno == ENOENT) {
		cfg = dsbcfg_new(NULL, vardefs, CFG_NVARS);
		if (cfg == NULL)
			qh_errx(0, EXIT_FAILURE, "%s", dsbcfg_strerror());
	} else if (cfg == NULL)
		qh_errx(0, EXIT_FAILURE, "%s", dsbcfg_strerror());

	posX	 = &dsbcfg_getval(cfg, CFG_POS_X).integer;
	posY	 = &dsbcfg_getval(cfg, CFG_POS_Y).integer;
	wWidth	 = &dsbcfg_getval(cfg, CFG_WIDTH).integer;
	hHeight	 = &dsbcfg_getval(cfg, CFG_HEIGHT).integer;
	lrView   = &dsbcfg_getval(cfg, CFG_LRVIEW).boolean;
	chanMask = &dsbcfg_getval(cfg, CFG_MASK).integer;

	muteIcon = qh_loadIcon("audio-volume-muted", NULL);
	hVolIcon = qh_loadIcon("audio-volume-high", NULL);

	tabs = new QTabWidget(this);

	for (int i = 0; i < dsbmixer_getndevs(); i++) {
		dsbmixer_t *dev = dsbmixer_getmixer(i);
		Mixer *mixer = new Mixer(dev, *chanMask, *lrView, this);

		mixers.append(mixer);

		tabs->addTab(mixer, dev->name);
		tabs->setTabToolTip(i, QString(dev->cardname));
		connect(mixer, SIGNAL(muteStateChanged()), this,
		    SLOT(catchMuteStateChanged()));
	}
	setCentralWidget(tabs);
	tabs->setCurrentIndex(dsbmixer_snd_settings.default_unit);

#ifndef WITHOUT_DEVD
	Thread *thread = new Thread();
	connect(thread, SIGNAL(sendNewMixer(dsbmixer_t*)), this,
	    SLOT(addNewMixer(dsbmixer_t*)));

	connect(thread, SIGNAL(sendRemoveMixer(dsbmixer_t*)), this,
	    SLOT(removeMixer(dsbmixer_t*)));
	thread->start();
#endif
	connect(timer, SIGNAL(timeout()), this, SLOT(updateMixers()));
    	timer->start(200);

	connect(tabs, SIGNAL(currentChanged(int)), this,
	    SLOT(catchCurrentChanged()));

	connect(traytimer, SIGNAL(timeout()), this, SLOT(checkForSysTray()));
    	traytimer->start(500);

	createMenuActions();
	createMainMenu();
	setWindowIcon(hVolIcon);
	setWindowTitle("DSBMixer");
	resize(*wWidth, *hHeight);	
	move(*posX, *posY);
}

void
MainWin::redrawMixers()
{
	int curIdx = tabs->currentIndex();

	for (int i = tabs->count() - 1; i >= 0; i--)
		tabs->removeTab(0);
	for (int i = mixers.count() - 1; i >= 0; i--) {
		delete mixers.at(0);
		mixers.removeAt(0);
	}
	for (int i = 0; i < dsbmixer_getndevs(); i++) {
		dsbmixer_t *dev = dsbmixer_getmixer(i);
		Mixer *mixer = new Mixer(dev, *chanMask, *lrView);
		mixers.append(mixer);
		tabs->addTab(mixer, dev->name);
		tabs->setTabToolTip(i, QString(dev->cardname));

		connect(mixer, SIGNAL(muteStateChanged()), this,
		    SLOT(catchMuteStateChanged()));
	}
	tabs->setCurrentIndex(curIdx);
	saveGeometry();
}

#ifndef WITHOUT_DEVD
void
MainWin::addNewMixer(dsbmixer_t *dev)
{
	Mixer *mixer = new Mixer(dev, *chanMask, *lrView, this);

	tabs->addTab(mixer, dev->name);
	mixers.append(mixer);
	connect(mixer, SIGNAL(muteStateChanged()), this,
	    SLOT(catchMuteStateChanged()));
	tabs->setTabToolTip(mixers.count() - 1, QString(dev->cardname));
	redrawMixers();
}

void
MainWin::removeMixer(dsbmixer_t *mixer)
{
	for (int i = 0; i < mixers.count(); i++) {
		if (mixers.at(i)->getDev() == mixer) {
			tabs->removeTab(i);
			delete mixers.at(i);
			mixers.removeAt(i);
			dsbmixer_delmixer(mixer);
			return;
		}
  	}	
}
#endif

void
MainWin::updateMixers()
{
	dsbmixer_t *mixer = dsbmixer_pollmixers();

	if (mixer == NULL)
		return;
	for (int i = 0; i < mixers.count(); i++) {
		if (mixer == mixers.at(i)->getDev()) {
			mixers.at(i)->update();
			return;
		}
	}
}

void
MainWin::trayClicked(QSystemTrayIcon::ActivationReason reason)
{
	if (reason == QSystemTrayIcon::Trigger || 
	    reason == QSystemTrayIcon::DoubleClick) {
		if (!this->isVisible()) {
			this->setVisible(true);
			saveGeometry();
		} else {
			saveGeometry();
			this->setVisible(false);
		}
	}
}

void
MainWin::closeEvent(QCloseEvent *event)
{
	setVisible(false);
	event->ignore();
}

void
MainWin::showConfigMenu()
{
	Preferences prefs(*chanMask,
	    dsbmixer_snd_settings.amplify,
	    dsbmixer_snd_settings.feeder_rate_quality,
	    dsbmixer_snd_settings.default_unit, *lrView, this);

	if (prefs.exec() != QDialog::Accepted)
		return;
	if (prefs.defaultUnit != dsbmixer_snd_settings.default_unit)
		tabs->setCurrentIndex(prefs.defaultUnit);
	if (*lrView != prefs.lrView || *chanMask != prefs.chanMask) {
		*lrView   = prefs.lrView;
		*chanMask = prefs.chanMask;
		redrawMixers();
	}
	dsbcfg_write(PROGRAM, "config", cfg);

	if (dsbmixer_snd_settings.amplify != prefs.amplify ||
	    dsbmixer_snd_settings.default_unit != prefs.defaultUnit ||
	    dsbmixer_snd_settings.feeder_rate_quality !=
	    prefs.feederRateQuality) {
		if (dsbmixer_change_settings(prefs.defaultUnit, prefs.amplify,
		    prefs.feederRateQuality) == -1)
			qh_warnx(0, dsbmixer_error());
	}
}

void
MainWin::quit()
{
	saveGeometry();
	dsbcfg_write(PROGRAM, "config", cfg);
	dsbmixer_cleanup();
	QApplication::quit();
}

void
MainWin::saveGeometry()
{
	if (isVisible()) {
		*posX = this->x(); *posY = this->y();
		*wWidth = this->width(); *hHeight = this->height();
	}
}

void
MainWin::createMenuActions()
{
	QIcon quitIcon = qh_loadIcon("application-exit", NULL); 
	QIcon prefsIcon = qh_loadIcon("preferences-desktop-multimedia", NULL);

	quitAction = new QAction(quitIcon, tr("&Quit"), this);
	preferencesAction = new QAction(prefsIcon, tr("&Preferences"), this);

	connect(quitAction, SIGNAL(triggered()), this, SLOT(quit()));
	connect(preferencesAction, SIGNAL(triggered()), this,
	    SLOT(showConfigMenu()));
}

void
MainWin::createMainMenu()
{
	mainMenu = menuBar()->addMenu(tr("&File"));
	mainMenu->addAction(preferencesAction);
	mainMenu->addAction(quitAction);
}

void
MainWin::checkForSysTray()
{
	static int tries = 60;

	if (QSystemTrayIcon::isSystemTrayAvailable()) {
		traytimer->stop();
		createTrayIcon();
	} else if (tries-- <= 0)
		traytimer->stop();
}

void
MainWin::createTrayIcon()
{
	QMenu *menu = new QMenu(this);

	trayIcon = new QSystemTrayIcon(hVolIcon, this);

	menu->addAction(preferencesAction);
	menu->addAction(quitAction);
	trayIcon->setContextMenu(menu);

	connect(trayIcon,
	    SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
	    this,
	    SLOT(trayClicked(QSystemTrayIcon::ActivationReason)));
	trayIcon->show();
}

void
MainWin::catchMuteStateChanged()
{
	updateTrayIcon();
}

void
MainWin::catchCurrentChanged()
{
	updateTrayIcon();
}

void
MainWin::updateTrayIcon()
{
	int idx = tabs->currentIndex();

	if (idx == -1)
		return;
	if (mixers.at(idx)->muted)
		trayIcon->setIcon(muteIcon);
	else
		trayIcon->setIcon(hVolIcon);
}

