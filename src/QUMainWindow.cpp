#include "main.h"

#include "QUMainWindow.h"
#include "QULogService.h"
#include "QUMessageBox.h"
#include "QUAboutDialog.h"
#include "QURibbonBar.h"
#include "QUMonty.h"
#include "QUSongSupport.h"

#include <QActionGroup>
#include <QFileDialog>
#include <QTextStream>
#include <QClipboard>
#include <QSettings>
#include <QTimer>
#include <QTextCodec>
#include <QProcess>
#include <QDirIterator>
#include <QWhatsThis>
#include <QDesktopServices>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTemporaryFile>

//QUMainWindow::QUMainWindow(QWidget *parent): QMainWindow(parent) {
QUMainWindow::QUMainWindow(QWidget *parent): QMainWindow(parent), ui(new Ui::QUMainWindow) {
	ui->setupUi(this);

	initWindow();
	initRibbonBar();

	//initStatusBar();
	//initEventLog();
	initConfig();
	initMonty();

	// deactivate TeX syllabification for now
	ui->pushButton_SyllabificateTeX->setVisible(false);

	logSrv->add(tr("Ready."), QU::Information);
	lyricsProgressBar = new QProgressBar;
	QMainWindow::statusBar()->addPermanentWidget(lyricsProgressBar);
	QMainWindow::statusBar()->showMessage(tr("USC ready."));
	if (BASS_Init(-1, 44100, 0, 0, NULL)) {
		QMainWindow::statusBar()->showMessage(tr("BASS initialized."));
	}

	QSettings settings;
	bool firstRun = settings.value("firstRun", "true").toBool();

	if (firstRun) {
		QUMessageBox::information(0, QObject::tr("Welcome to UltraStar Creator!"),
								  QObject::tr("This tool enables you to <b>rapidly</b> create UltraStar text files <b>from scratch</b>.<br><br>To get started, simply chose a <b>song file</b> in MP3 or OGG format, insert the <b>song lyrics</b> from a file or the clipboard and divide them into syllables with '+'.<br><br><b>Important song meta information</b> such as <b>BPM</b> and <b>GAP</b> are determined <b>automatically</b> while the <b>ID3 tag</b> is used to fill in additional song details, if available.<br><br>To <b>start tapping</b>, hit the play/pause button (Keyboard: CTRL-P). Keep the <b>tap button</b> (keyboard: space bar) pressed for as long as the current syllable is sung to tap a note. <b>Undo</b> the last tap with the undo button (Keyboard: x), <b>stop tapping</b> with the stop button (Keyboard: CTRL-S), <b>restart</b> from the beginning with the reset button (Keyboard: CTRL-R). When finished, <b>save</b> the tapped song using the save button (CTRL-S).<br><br>Having successfully tapped a song, use the UltraStar internal editor for <b>finetuning the timings</b>, setting <b>note pitches</b> and <b>golden</b> or <b>freestyle notes</b>.<br><br><b>Happy creating!</b>"),
								  BTN << ":/icons/accept.png" << QObject::tr("Okay. Let's go!"),550,0);
		firstRun = false;
		settings.setValue("firstRun", firstRun);
	}

	//
	QFileInfo fi(settings.value("songfile").toString());
	if (fi.exists()) {
		settings.remove("songfile");
		fileInfo_MP3 = &fi;
		handleMP3();
	}

	// init
	numSyllables = 0;
	firstNoteStartBeat = 0;
	currentNoteBeatLength = 0;
	currentSyllableIndex = 0;
	currentCharacterIndex = 0;
	isFirstKeyPress = true;
	firstNote = true;
	clipboard = QApplication::clipboard();
	state = QUMainWindow::uninitialized;
	previewState = QUMainWindow::uninitialized;
	defaultDir = QDir::homePath();

	if (settings.value("allowUpdateCheck", QVariant(false)).toBool()) {
		this->checkForUpdate(true);
	}

	this->show();
}

/*!
 * Overloaded to ensure that all changes are saved before closing this application.
 */
void QUMainWindow::closeEvent(QCloseEvent *event) {
	QSettings settings;

	settings.setValue("allowMonty", QVariant(_menu->montyBtn->isChecked()));
	settings.setValue("geometry", saveGeometry());
	settings.setValue("windowState", saveState());
	settings.setValue("inputlyricsfontsize", ui->plainTextEdit_InputLyrics->fontInfo().pointSize());
	settings.setValue("outputlyricsfontsize", ui->plainTextEdit_OutputLyrics->fontInfo().pointSize());
	settings.setValue("defaultPitch", _menu->comboBox_DefaultPitch->currentIndex());

	// everything should be fine from now on
	QFile::remove("running.app");

	event->accept();
}

/*!
 * Set up initial window size and title text.
 */
void QUMainWindow::initWindow() {
	// create window icon
	QIcon windowIcon;
	windowIcon.addFile(":/icons/bean.png", QSize(16, 16));
	/*
	windowIcon.addFile(":/icons/bean32.png", QSize(32, 32));
	windowIcon.addFile(":/icons/bean48.png", QSize(48, 48));
	windowIcon.addFile(":/icons/bean64.png", QSize(64, 64));
	windowIcon.addFile(":/icons/bean72.png", QSize(72, 72));
	windowIcon.addFile(":/icons/bean96.png", QSize(96, 96));
	windowIcon.addFile(":/icons/bean128.png", QSize(128, 128));*/

	setWindowIcon(windowIcon);
	setWindowTitle(QString("%1%2").arg("UltraStar Creator").arg(WIP_TEXT));

	// adding languages to language combobox as I did not find a way to add itemData within designer
	// this way, foreign language names are displayed to the user while the UltraStar file will contain the
	// english language name
	ui->comboBox_Language->addItem(QIcon(":/languages/cn.png"),tr("Chinese"),"Chinese");
	ui->comboBox_Language->addItem(QIcon(":/languages/hr.png"),tr("Croatian"),"Croatian");
	ui->comboBox_Language->addItem(QIcon(":/languages/cz.png"),tr("Czech"),"Czech");
	ui->comboBox_Language->addItem(QIcon(":/languages/dk.png"),tr("Danish"),"Danish");
	ui->comboBox_Language->addItem(QIcon(":/languages/nl.png"),tr("Dutch"),"Dutch");
	ui->comboBox_Language->addItem(QIcon(":/languages/us.png"),tr("English"),"English");
	ui->comboBox_Language->addItem(QIcon(":/languages/fi.png"),tr("Finnish"),"Finnish");
	ui->comboBox_Language->addItem(QIcon(":/languages/fr.png"),tr("French"),"French");
	ui->comboBox_Language->addItem(QIcon(":/languages/de.png"),tr("German"),"German");
	ui->comboBox_Language->addItem(QIcon(":/languages/in.png"),tr("Hindi"),"Hindi");
	ui->comboBox_Language->addItem(QIcon(":/languages/it.png"),tr("Italian"),"Italian");
	ui->comboBox_Language->addItem(QIcon(":/languages/jp.png"),tr("Japanese"),"Japanese");
	ui->comboBox_Language->addItem(QIcon(":/languages/kr.png"),tr("Korean"),"Korean");
	ui->comboBox_Language->addItem(QIcon(":/languages/va.png"),tr("Latin"),"Latin");
	ui->comboBox_Language->addItem(QIcon(":/languages/no.png"),tr("Norwegian"),"Norwegian");
	ui->comboBox_Language->addItem(QIcon(":/languages/pl.png"),tr("Polish"),"Polish");
	ui->comboBox_Language->addItem(QIcon(":/languages/pt.png"),tr("Portuguese"),"Portuguese");
	ui->comboBox_Language->addItem(QIcon(":/languages/ru.png"),tr("Russian"),"Russian");
	ui->comboBox_Language->addItem(QIcon(":/languages/sk.png"),tr("Slovak"),"Slovak");
	ui->comboBox_Language->addItem(QIcon(":/languages/si.png"),tr("Slowenian"),"Slowenian");
	ui->comboBox_Language->addItem(QIcon(":/languages/es.png"),tr("Spanish"),"Spanish");
	ui->comboBox_Language->addItem(QIcon(":/languages/se.png"),tr("Swedish"),"Swedish");
	ui->comboBox_Language->addItem(QIcon(":/languages/tr.png"),tr("Turkish"),"Turkish");
	ui->comboBox_Language->addItem(QIcon(":/languages/wales.png"),tr("Welsh"),"Welsh");

	ui->comboBox_Genre->addItem("");
	ui->comboBox_Genre->addItems(QUSongSupport::availableSongGenres());

	/*
	resize(1000, 500);

	// other things
	QAction *a = new QAction(this);
	a->setShortcut(Qt::CTRL + Qt::Key_F11);
	connect(a, SIGNAL(triggered()), this, SLOT(toggleFullScreenMode()));
	addAction(a);
	*/
}

/*!
 * Create an Office2007-like menu to reduce mouse clicks.
 */
void QUMainWindow::initRibbonBar() {
	_menu = new QURibbonBar(this);
	this->setMenuWidget(_menu);

	// settings menu
	_menu->comboBox_DefaultPitch->addItems(QUSongSupport::availableDefaultPitches());
	connect(_menu->horizontalSlider_PlaybackSpeed, SIGNAL(valueChanged(int)), this, SLOT(setPlaybackSpeed(int)));

	// extras menu
	connect(_menu->freestyleTextFilesBtn, SIGNAL(clicked()), this, SLOT(generateFreestyleTextFiles()));

	// language menu
	connect(_menu->langUsBtn, SIGNAL(clicked()), this, SLOT(enableEnglish()));
	connect(_menu->langDeBtn, SIGNAL(clicked()), this, SLOT(enableGerman()));
	connect(_menu->langPlBtn, SIGNAL(clicked()), this, SLOT(enablePolish()));
	connect(_menu->langFrBtn, SIGNAL(clicked()), this, SLOT(enableFrench()));
	connect(_menu->langEsBtn, SIGNAL(clicked()), this, SLOT(enableSpanish()));
	connect(_menu->langPtBtn, SIGNAL(clicked()), this, SLOT(enablePortuguese()));

	// about menu
	connect(_menu->aboutQtBtn, SIGNAL(clicked()), this, SLOT(aboutQt()));
	connect(_menu->aboutUmanBtn, SIGNAL(clicked()), this, SLOT(aboutUSC()));
	connect(_menu->aboutTagLibBtn, SIGNAL(clicked()), this, SLOT(aboutTagLib()));
	connect(_menu->aboutBASSBtn, SIGNAL(clicked()), this, SLOT(aboutBASS()));
	connect(_menu->checkForUpdateBtn, SIGNAL(clicked(bool)), this, SLOT(checkForUpdate(bool)));

	// help menu
	connect(_menu->helpBtn, SIGNAL(clicked()), ui->montyArea, SLOT(show()));
	_menu->setShortcut(_menu->helpBtn, Qt::Key_F1);
	_menu->setShortcut(_menu->montyBtn, Qt::SHIFT + Qt::Key_F1);
	_menu->setShortcut(_menu->hideBtn, Qt::CTRL + Qt::Key_F1);
}

/*!
 * Initializes the windows registry entry for uman. Lets the user
 * choose a path where the song files are located.
 */
void QUMainWindow::initConfig() {
	QSettings settings;

	QLocale::Language lang = QLocale(settings.value("language").toString()).language();
	if (lang == QLocale::English)
		 _menu->langUsBtn->setChecked(true);
	else if (lang == QLocale::French)
		_menu->langFrBtn->setChecked(true);
	else if (lang == QLocale::German)
		_menu->langDeBtn->setChecked(true);
	else if (lang == QLocale::Polish)
		_menu->langPlBtn->setChecked(true);
	else if (lang == QLocale::Spanish)
		_menu->langEsBtn->setChecked(true);
	else if (lang == QLocale::Portuguese)
		_menu->langPtBtn->setChecked(true);

	restoreGeometry(settings.value("geometry").toByteArray());
	restoreState(settings.value("windowState").toByteArray());

	// restore creator
	ui->lineEdit_Creator->setText(settings.value("creator", "").toString());

	// restore font sizes
	QFont font = ui->plainTextEdit_InputLyrics->font();
	font.setPointSize(settings.value("inputlyricsfontsize", 10).toInt());
	ui->plainTextEdit_InputLyrics->setFont(font);
	font.setPointSize(settings.value("outputlyricsfontsize", 10).toInt());
	ui->plainTextEdit_OutputLyrics->setFont(font);

	// restore default pitch
	_menu->comboBox_DefaultPitch->setCurrentIndex(settings.value("defaultPitch", 0).toInt());

	_menu->montyBtn->setChecked(settings.value("allowMonty", true).toBool());

	/*
	updateViewButtons();

	connect(detailsDock, SIGNAL(visibilityChanged(bool)), this, SLOT(updateViewButtons()));
	connect(tasksDock, SIGNAL(visibilityChanged(bool)), this, SLOT(updateViewButtons()));
	connect(playlistDock, SIGNAL(visibilityChanged(bool)), this, SLOT(updateViewButtons()));
	connect(mediaPlayerDock, SIGNAL(visibilityChanged(bool)), this, SLOT(updateViewButtons()));
	connect(previewDock, SIGNAL(visibilityChanged(bool)), this, SLOT(updateViewButtons()));
	connect(eventsDock, SIGNAL(visibilityChanged(bool)), this, SLOT(updateViewButtons()));
	*/
}

/*!
 *
 *
 */
void QUMainWindow::initEventLog() {
	/*
	QSettings settings;
	showInfosBtn->setChecked(settings.value("showInfoMessages", true).toBool());
	showHelpBtn->setChecked(settings.value("showHelpMessages", true).toBool());
	showSaveHintsBtn->setChecked(settings.value("showSaveMessages", true).toBool());
	showWarningsBtn->setChecked(settings.value("showWarningMessages", true).toBool());
	showErrorsBtn->setChecked(settings.value("showErrorMessages", true).toBool());

	connect(logClearBtn, SIGNAL(clicked()), this, SLOT(clearLog()));
	connect(logSaveBtn, SIGNAL(clicked()), this, SLOT(saveLog()));
	*/
}

/*!
 *
 *
 */
void QUMainWindow::initMonty() {
	ui->montyArea->montyLbl->setPixmap(monty->pic(QUMonty::seated));

	ui->montyArea->helpLbl->setText(monty->welcomeMsg());

	connect(ui->montyArea->hideMontyBtn, SIGNAL(clicked()), ui->montyArea, SLOT(hide()));
	connect(ui->montyArea->talkMontyBtn, SIGNAL(clicked()), this, SLOT(montyTalkNow()));

	if(!_menu->montyBtn->isChecked())
		ui->montyArea->hide();

	ui->montyArea->askFrame->hide();
	ui->montyArea->answerFrame->hide();

	connect(ui->montyArea->askMontyBtn, SIGNAL(clicked()), this, SLOT(montyAsk()));
	connect(ui->montyArea->acceptQuestionBtn, SIGNAL(clicked()), this, SLOT(montyAnswer()));

	connect(ui->montyArea->rejectQuestionBtn, SIGNAL(clicked()), ui->montyArea->normalFrame, SLOT(show()));
	connect(ui->montyArea->rejectQuestionBtn, SIGNAL(clicked()), ui->montyArea->askFrame, SLOT(hide()));
	connect(ui->montyArea->rejectQuestionBtn, SIGNAL(clicked()), this, SLOT(montyTalkNow()));

	connect(ui->montyArea->doneBtn, SIGNAL(clicked()), ui->montyArea->normalFrame, SLOT(show()));
	connect(ui->montyArea->doneBtn, SIGNAL(clicked()), ui->montyArea->answerFrame, SLOT(hide()));
	connect(ui->montyArea->doneBtn, SIGNAL(clicked()), this, SLOT(montyTalkNow()));

	connect(ui->montyArea->nextPhraseBtn, SIGNAL(clicked()), this, SLOT(montyNext()));
	connect(ui->montyArea->prevPhraseBtn, SIGNAL(clicked()), this, SLOT(montyPrev()));

	connect(ui->montyArea->lineEdit, SIGNAL(returnPressed()), this, SLOT(montyAnswer()));
}

/*!
 *
 *
 */
void QUMainWindow::initStatusBar() {
	/*
	_timer.setSingleShot(true);
	_timer.setInterval(10000);
	connect(&_timer, SIGNAL(timeout()), this, SLOT(clearStatusMessage()));

	_statusIconLbl = new QLabel();
	_statusMessageLbl = new QLabel();

	statusBar()->addWidget(_statusIconLbl);
	statusBar()->addWidget(_statusMessageLbl);

	_statusIconLbl->hide();
	_statusMessageLbl->hide();

	_toggleEventLogBtn = new QToolButton();
	_toggleEventLogBtn->setIcon(QIcon(":/control/log.png"));
	_toggleEventLogBtn->setAutoRaise(true);
	_toggleEventLogBtn->setCheckable(true);
	connect(_toggleEventLogBtn, SIGNAL(clicked(bool)), eventsDock, SLOT(setVisible(bool)));
	connect(_menu->eventLogBtn, SIGNAL(toggled(bool)), _toggleEventLogBtn, SLOT(setChecked(bool)));

	statusBar()->addPermanentWidget(_toggleEventLogBtn);
	*/
}

bool QUMainWindow::on_pushButton_SaveToFile_clicked()
{
	QString suggestedAbsoluteFilePath = fileInfo_MP3->absolutePath() + "\\" + ui->lineEdit_Artist->text() + " - " + ui->lineEdit_Title->text() + ".txt";
	QString fileName = QFileDialog::getSaveFileName(this, tr("Please choose file"), suggestedAbsoluteFilePath, tr("Text files (*.txt)"));

	QFile file(fileName);
	if (!file.open(QFile::WriteOnly | QFile::Text)) {
		QUMessageBox::warning(this, tr("Application"),
			tr("Cannot write file %1:\n%2.")
			.arg(fileName)
			.arg(file.errorString()));
		return false;
	}

	QTextStream out(&file);
	QTextCodec *codec = QTextCodec::codecForName("Windows-1252");
	if (codec->canEncode(ui->plainTextEdit_OutputLyrics->toPlainText())) {
		out.setCodec(codec);
	}
	else {
		out.setCodec(QTextCodec::codecForName("UTF-8"));
		out.setGenerateByteOrderMark(true);
	}

	QApplication::setOverrideCursor(Qt::WaitCursor);
	out << ui->plainTextEdit_OutputLyrics->toPlainText();
	QApplication::restoreOverrideCursor();
	ui->pushButton_startUltraStar->setEnabled(true);
	ui->pushButton_startYass->setEnabled(true);
	return true;
}

void QUMainWindow::on_pushButton_PlayPause_clicked()
{
	QSettings settings;

	if (state == initialized) {
		setCursor(Qt::WaitCursor);
		state = QUMainWindow::playing;
		ui->pushButton_PlayPause->setIcon(QIcon(":/player/pause.png"));
		QWidget::setAcceptDrops(false);
		ui->groupBox_SongMetaInformationTags->setDisabled(true);
		ui->groupBox_InputLyrics->setDisabled(true);
		ui->groupBox_OutputLyrics->setEnabled(true);
		ui->pushButton_SaveToFile->setDisabled(true);
		ui->pushButton_startUltraStar->setDisabled(true);
		ui->pushButton_startYass->setDisabled(true);
		ui->groupBox_TapArea->setEnabled(true);
		ui->pushButton_UndoTap->setDisabled(true);
		ui->pushButton_Tap->setEnabled(true);
		ui->pushButton_UndoTap->setDisabled(true);
		ui->pushButton_Stop->setEnabled(true);
		if (ui->lineEdit_Title->text().isEmpty()) {
			ui->lineEdit_Title->setText(tr("Title"));
			ui->label_TitleSet->setPixmap(QPixmap(":/icons/path_ok.png"));
		}
		if (ui->lineEdit_Artist->text().isEmpty()) {
			ui->lineEdit_Artist->setText(tr("Artist"));
			ui->label_ArtistSet->setPixmap(QPixmap(":/icons/path_ok.png"));
		}
		if (ui->comboBox_Cover->currentText().isEmpty()) {
			ui->comboBox_Cover->addItem(QString("%1 - %2 [CO].jpg").arg(ui->lineEdit_Artist->text()).arg(ui->lineEdit_Title->text()));
		}
		if (ui->comboBox_Background->currentText().isEmpty()) {
			ui->comboBox_Background->addItem(QString("%1 - %2 [BG].jpg").arg(ui->lineEdit_Artist->text()).arg(ui->lineEdit_Title->text()));
		}

		//timeLineMap.insert(-15, "#ENCODING:UTF8");
		timeLineMap.insert(-14, QString("#TITLE:%1").arg(ui->lineEdit_Title->text()));
		timeLineMap.insert(-13, QString("#ARTIST:%1").arg(ui->lineEdit_Artist->text()));
		if (!ui->comboBox_Language->currentText().isEmpty()) {
			timeLineMap.insert(-12, QString("#LANGUAGE:%1").arg(ui->comboBox_Language->itemData(ui->comboBox_Language->currentIndex()).toString()));
		}
		if (!ui->lineEdit_Edition->text().isEmpty()) {
			timeLineMap.insert(-11, QString("#EDITION:%1").arg(ui->lineEdit_Edition->text()));
		}
		if (!ui->comboBox_Genre->currentText().isEmpty()) {
			timeLineMap.insert(-10, QString("#GENRE:%1").arg(ui->comboBox_Genre->currentText()));
		}
		if (!ui->comboBox_Year->currentText().isEmpty()) {
			timeLineMap.insert(-9, QString("#YEAR:%1").arg(ui->comboBox_Year->currentText()));
		}
		if (!ui->lineEdit_Creator->text().isEmpty()) {
			timeLineMap.insert(-8, QString("#CREATOR:%1").arg(ui->lineEdit_Creator->text()));
		}
		timeLineMap.insert(-7, QString("#MP3:%1").arg(ui->lineEdit_MP3->text()));
		if (!ui->comboBox_Cover->currentText().isEmpty()) {
			timeLineMap.insert(-6, QString("#COVER:%1").arg(ui->comboBox_Cover->currentText()));
		}
		if (!ui->comboBox_Background->currentText().isEmpty()) {
			timeLineMap.insert(-5, QString("#BACKGROUND:%1").arg(ui->comboBox_Background->currentText()));
		}
		if (!ui->comboBox_Video->currentText().isEmpty()) {
			timeLineMap.insert(-4, QString("#VIDEO:%1").arg(ui->comboBox_Video->currentText()));
		}
		timeLineMap.insert(-2, QString("#BPM:%1").arg(ui->doubleSpinBox_BPM->text()));
		// BPM might have been changed manually
		BPM = ui->doubleSpinBox_BPM->value();

		updateOutputLyrics();

		settings.setValue("creator", ui->lineEdit_Creator->text());

		QString rawLyricsString = ui->plainTextEdit_InputLyrics->toPlainText();

		lyricsString = cleanLyrics(rawLyricsString);

		splitLyricsIntoSyllables();
		numSyllables = lyricsSyllableList.length();
		lyricsProgressBar->setRange(0,numSyllables);
		lyricsProgressBar->setValue(0);
		lyricsProgressBar->show();

		updateSyllableButtons();

		// reset from preview play
		BASS_StopAndFree();
		_mediaStream = BASS_StreamCreateFile(FALSE, fileInfo_MP3->absoluteFilePath().toLocal8Bit().data() , 0, 0, BASS_STREAM_DECODE);

		_mediaStream = BASS_FX_TempoCreate(_mediaStream, BASS_FX_FREESOURCE);

		// playing slower results in lower pitch, just like a record played at a lower speed
		// playing slower while preserving pitch only works well for small changes in speed
		BASS_ChannelGetAttribute(_mediaStream, BASS_ATTRIB_FREQ, &sampleRate);
		if (BASS_ChannelSetAttribute(_mediaStream, BASS_ATTRIB_TEMPO_FREQ, sampleRate*_menu->horizontalSlider_PlaybackSpeed->value()/100)) {
			BASS_Play();
			updateTime();
		}

		ui->pushButton_Tap->setFocus(Qt::OtherFocusReason);
		setCursor(Qt::ArrowCursor);
	}
	else if (state == playing) {
		state = QUMainWindow::paused;
		ui->pushButton_PlayPause->setIcon(QIcon(":/player/play.png"));
		ui->pushButton_Tap->setDisabled(true);
		ui->pushButton_NextSyllable1->setDisabled(true);
		ui->pushButton_NextSyllable2->setDisabled(true);
		ui->pushButton_NextSyllable3->setDisabled(true);
		ui->pushButton_NextSyllable4->setDisabled(true);
		ui->pushButton_NextSyllable5->setDisabled(true);
		BASS_Pause();
	}
	else if (state == paused) {
		state = QUMainWindow::playing;
		ui->pushButton_PlayPause->setIcon(QIcon(":/player/pause.png"));
		ui->pushButton_Tap->setEnabled(true);
		ui->pushButton_NextSyllable1->setEnabled(true);
		ui->pushButton_NextSyllable2->setEnabled(true);
		ui->pushButton_NextSyllable3->setEnabled(true);
		ui->pushButton_NextSyllable4->setEnabled(true);
		ui->pushButton_NextSyllable5->setEnabled(true);
		ui->pushButton_Tap->setFocus(Qt::OtherFocusReason);
		BASS_Resume();
	}
	else {
		// should not be possible
	}

	montyTalk();
}

QString QUMainWindow::cleanLyrics(QString rawLyricsString) {
	// delete surplus space characters
	rawLyricsString = rawLyricsString.replace(QRegExp(" {2,}"), " ");

	// delete surplus blank lines...
	rawLyricsString = rawLyricsString.replace(QRegExp("\\n{2,}"), "\n");

	// replace misused accents (�,`) by the correct apostrophe (')
	rawLyricsString = rawLyricsString.replace("�", "'");
	rawLyricsString = rawLyricsString.replace("`", "'");

	// delete leading and trailing whitespace from each line
	QStringList rawLyricsStringList = rawLyricsString.split(QRegExp("\\n"));
	QStringList lyricsStringList;
	rawLyricsString.clear();
	QStringListIterator lyricsLineIterator(rawLyricsStringList);
	while (lyricsLineIterator.hasNext()) {
		QString currentLine = lyricsLineIterator.next();
		currentLine = currentLine.trimmed();
		currentLine.replace(0,1,currentLine.at(0).toUpper());
		lyricsStringList.append(currentLine);
	}
	rawLyricsString = lyricsStringList.join("\n");

	QString cleanedLyricsString = rawLyricsString;

	ui->plainTextEdit_InputLyrics->setPlainText(cleanedLyricsString);

	return cleanedLyricsString;
}

void QUMainWindow::on_pushButton_Tap_pressed()
{
	currentNoteStartTime = BASS_Position()*1000; // milliseconds

	// conversion from milliseconds [ms] to quarter beats [qb]: time [ms] * BPM [qb/min] * 1/60 [min/s] * 1/1000 [s/ms]
	previousNoteEndBeat = currentNoteStartBeat + currentNoteBeatLength - firstNoteStartBeat;
	currentNoteStartBeat = currentNoteStartTime * (BPM / 15000);
	ui->pushButton_Tap->setCursor(Qt::ClosedHandCursor);
}


void QUMainWindow::on_pushButton_Tap_released()
{
	double currentNoteTimeLength = BASS_Position()*1000 - currentNoteStartTime;
	ui->pushButton_Tap->setCursor(Qt::OpenHandCursor);
	lyricsProgressBar->setValue(lyricsProgressBar->value()+1);
	if (!ui->pushButton_UndoTap->isEnabled()) {
		ui->pushButton_UndoTap->setEnabled(true);
	}
	currentNoteBeatLength = qMax(1.0, currentNoteTimeLength * (BPM / 15000));
	if (firstNote){
		firstNoteStartBeat = currentNoteStartBeat;
		timeLineMap.insert(-1, QString("#GAP:%1").arg(QString::number(currentNoteStartTime, 'f', 0)));
		firstNote = false;
	}
	bool addLinebreak = false;
	QString linebreakString = "";

	QString currentSyllable = lyricsSyllableList[currentSyllableIndex];
	if (currentSyllable.startsWith("\n")) {
		addLinebreak = true;
		currentSyllable = currentSyllable.mid(1);
	}

	if (addLinebreak)
	{
		qint32 linebreakBeat = previousNoteEndBeat + (currentNoteStartBeat - firstNoteStartBeat - previousNoteEndBeat)/2;
		linebreakString = QString("- %1\n").arg(QString::number(linebreakBeat));
	}

	currentOutputTextLine = QString("%1: %2 %3 %4 %5").arg(linebreakString).arg(QString::number(currentNoteStartBeat - firstNoteStartBeat)).arg(QString::number(currentNoteBeatLength)).arg(_menu->comboBox_DefaultPitch->currentIndex()).arg(currentSyllable);

	timeLineMap.insert(currentNoteStartTime, currentOutputTextLine);

	updateOutputLyrics();

	if ((currentSyllableIndex+1) < numSyllables) {
		currentSyllableIndex++;
		updateSyllableButtons();
	}
	else {
		ui->pushButton_Tap->setText("");
		on_pushButton_Stop_clicked();

		int result = QUMessageBox::information(this,
							tr("Tapping finished"),
							tr("Congratulations! You successfully tapped this song."),
							BTN << ":/icons/cancel.png" << tr("I can do better. Let me try again!")
								<< ":/icons/accept.png" << tr("Great! Save the file."),
							340);
			if(result == 0)
				on_pushButton_Reset_clicked();
			else
				on_pushButton_SaveToFile_clicked();
	}
}

void QUMainWindow::updateOutputLyrics()
{
	ui->plainTextEdit_OutputLyrics->clear();
	// insert header
	QMapIterator<double, QString> i(timeLineMap);
		while (i.hasNext()) {
			i.next();
			 if (i.key() <= currentNoteStartTime)
			 {
				 ui->plainTextEdit_OutputLyrics->appendPlainText(i.value());
			 }
		 }
}

void QUMainWindow::on_pushButton_PasteFromClipboard_clicked()
{
	if (clipboard->mimeData()->hasText()) {
		ui->plainTextEdit_InputLyrics->setPlainText(clipboard->text());
	}

	montyTalk();
}

void QUMainWindow::on_pushButton_Stop_clicked()
{
	if (state == playing || state == paused) {
		state = QUMainWindow::stopped;
		QMainWindow::statusBar()->showMessage(tr("State: stopped."));
		BASS_StopAndFree();
		ui->label_TimeElapsed->setText("0:00");
		ui->label_TimeToRun->setText("0:00");

		ui->plainTextEdit_OutputLyrics->appendPlainText("E");
		ui->plainTextEdit_OutputLyrics->appendPlainText("");

		QMainWindow::statusBar()->showMessage(tr("USC ready."));

		ui->pushButton_PlayPause->setIcon(QIcon(":/player/play.png"));
		ui->pushButton_PlayPause->setDisabled(true);
		ui->pushButton_Stop->setDisabled(true);
		ui->pushButton_Reset->setEnabled(true);
		ui->groupBox_TapArea->setDisabled(true);
		ui->pushButton_SaveToFile->setEnabled(true);
		QWidget::setAcceptDrops(true);
	}
	else {
		// should not be possible
	}

	montyTalk();
}

void QUMainWindow::on_pushButton_BrowseMP3_clicked()
{
	QString filename_MP3 = QFileDialog::getOpenFileName ( 0, tr("Please choose MP3 file"), defaultDir, tr("Audio files (*.mp3 *.ogg)"));
	fileInfo_MP3 = new QFileInfo(filename_MP3);
	if (fileInfo_MP3->exists()) {
		defaultDir = fileInfo_MP3->absolutePath();
		handleMP3();
	}
}

void QUMainWindow::on_comboBox_Video_textChanged(QString video)
{
	if (!video.isEmpty())
		ui->label_VideoSet->setPixmap(QPixmap(":/icons/path_ok.png"));
}

void QUMainWindow::aboutUSC()
{
	QUAboutDialog(this).exec();
}

void QUMainWindow::on_actionQuit_USC_triggered()
{
	close();
}

void QUMainWindow::dragEnterEvent( QDragEnterEvent* event ) {
	const QMimeData* md = event->mimeData();
	if( event && (md->hasUrls() || md->hasText())) {
		event->acceptProposedAction();
	}
}

void QUMainWindow::dropEvent( QDropEvent* event ) {
	if(event && event->mimeData()) {
		const QMimeData *data = event->mimeData();
		if (data->hasUrls()) {
			QList<QUrl> urls = data->urls();
			while (!urls.isEmpty()) {
				QString fileName = urls.takeFirst().toLocalFile();
				if (!fileName.isEmpty()) {
					QFileInfo fileInfo(fileName);
					QString fileScheme("*." + fileInfo.suffix());

					//if (fileInfo->suffix().toLower() == "txt") {
					if(QUSongSupport::allowedSongFiles().contains(fileScheme, Qt::CaseInsensitive)) {
						QFile file(fileName);
						if (file.open(QFile::ReadOnly | QFile::Text)) {
							QTextStream lyrics(&file);
							ui->plainTextEdit_InputLyrics->setPlainText(lyrics.readAll());
						}
					}
					//else if (fileInfo->suffix().toLower() == tr("mp3") || fileInfo->suffix().toLower() == tr("ogg")) {
					else if(QUSongSupport::allowedAudioFiles().contains(fileScheme, Qt::CaseInsensitive)) {
						if (fileInfo.exists()) {
							defaultDir = fileInfo.absolutePath();
							fileInfo_MP3 = &fileInfo;
							handleMP3();
						}
					}
				}
			}
		}
		else if (data->hasText()) {
			ui->plainTextEdit_InputLyrics->setPlainText(data->text());
		}
	}
}

void QUMainWindow::aboutQt()
{
	QApplication::aboutQt();
}

void QUMainWindow::on_lineEdit_Title_textChanged(QString title)
{
	if(!title.isEmpty()) {
		ui->label_TitleSet->setPixmap(QPixmap(":/icons/path_ok.png"));
	}
	else {
		ui->label_TitleSet->setPixmap(QPixmap(":/icons/path_error.png"));
	}
}

void QUMainWindow::on_lineEdit_Artist_textChanged(QString artist)
{
	if(!artist.isEmpty()) {
		ui->label_ArtistSet->setPixmap(QPixmap(":/icons/path_ok.png"));
	}
	else {
		ui->label_ArtistSet->setPixmap(QPixmap(":/icons/path_error.png"));
	}
}

void QUMainWindow::on_comboBox_Language_currentIndexChanged(QString language)
{
	if(!language.isEmpty()) {
		ui->label_LanguageSet->setPixmap(QPixmap(":/icons/path_ok.png"));
	}
	else {
		ui->label_LanguageSet->setPixmap(QPixmap(":/icons/path_error.png"));
	}
}

void QUMainWindow::on_lineEdit_Edition_textChanged(QString edition)
{
	if(!edition.isEmpty()) {
		ui->label_EditionSet->setPixmap(QPixmap(":/icons/path_ok.png"));
	}
	else {
		ui->label_EditionSet->setPixmap(QPixmap(":/icons/path_error.png"));
	}
}

void QUMainWindow::on_comboBox_Genre_textChanged(QString genre)
{
	if(!genre.isEmpty()) {
		ui->label_GenreSet->setPixmap(QPixmap(":/icons/path_ok.png"));
	}
	else {
		ui->label_GenreSet->setPixmap(QPixmap(":/icons/path_error.png"));
	}
}

void QUMainWindow::on_lineEdit_Creator_textChanged(QString creator)
{
	if(!creator.isEmpty()) {
		ui->label_CreatorSet->setPixmap(QPixmap(":/icons/path_ok.png"));
	}
	else {
		ui->label_CreatorSet->setPixmap(QPixmap(":/icons/path_error.png"));
	}
}

void QUMainWindow::on_pushButton_BrowseLyrics_clicked()
{
	QString filename_Text = QFileDialog::getOpenFileName ( 0, tr("Please choose text file"), defaultDir, tr("Text files (*.txt)"));

	if (!filename_Text.isEmpty()) {
		QFile file(filename_Text);
		if (file.open(QFile::ReadOnly | QFile::Text)) {
			QTextStream lyrics(&file);
			ui->plainTextEdit_InputLyrics->setPlainText(lyrics.readAll());
		}
	}
}

void QUMainWindow::on_pushButton_InputLyricsIncreaseFontSize_clicked()
{
	QFont font = ui->plainTextEdit_InputLyrics->font();
	if (font.pointSize() < 20) {
		font.setPointSize(font.pointSize()+1);
		ui->plainTextEdit_InputLyrics->setFont(font);
	}
}

void QUMainWindow::on_pushButton_InputLyricsDecreaseFontSize_clicked()
{
	QFont font = ui->plainTextEdit_InputLyrics->font();
	if (font.pointSize() > 5) {
		font.setPointSize(font.pointSize()-1);
		ui->plainTextEdit_InputLyrics->setFont(font);
	}
}

void QUMainWindow::on_pushButton_OutputLyricsIncreaseFontSize_clicked()
{
	QFont font = ui->plainTextEdit_OutputLyrics->font();
	if (font.pointSize() < 20) {
		font.setPointSize(font.pointSize()+1);
		ui->plainTextEdit_OutputLyrics->setFont(font);
	}
}

void QUMainWindow::on_pushButton_OutputLyricsDecreaseFontSize_clicked()
{
	QFont font = ui->plainTextEdit_OutputLyrics->font();
	if (font.pointSize() > 5) {
		font.setPointSize(font.pointSize()-1);
		ui->plainTextEdit_OutputLyrics->setFont(font);
	}
}

void QUMainWindow::on_plainTextEdit_InputLyrics_textChanged()
{
	if (!ui->plainTextEdit_InputLyrics->toPlainText().isEmpty() && !ui->lineEdit_MP3->text().isEmpty()) {
		if (state == QUMainWindow::uninitialized) {
			state = QUMainWindow::initialized;
		}
		ui->pushButton_PlayPause->setEnabled(true);
	}
	else {
		state = QUMainWindow::uninitialized;
		ui->pushButton_PlayPause->setDisabled(true);
	}

	montyTalk();
}

/*!
 * Changes the application language to English.
 */
void QUMainWindow::enableEnglish()
{
	changeLanguage("English");
}

/*!
 * Changes the application language to French.
 */
void QUMainWindow::enableFrench()
{
	changeLanguage("French");
}

/*!
 * Changes the application language to German.
 */
void QUMainWindow::enableGerman()
{
	changeLanguage("German");
}

/*!
 * Changes the application language to Polish.
 */
void QUMainWindow::enablePolish()
{
	changeLanguage("Polish");
}

/*!
 * Changes the application language to Spanish.
 */
void QUMainWindow::enableSpanish()
{
	changeLanguage("Spanish");
}

/*!
 * Changes the application language to Portuguese.
 */
void QUMainWindow::enablePortuguese()
{
	changeLanguage("Portuguese");
}

void QUMainWindow::changeLanguage(QString language) {
	QSettings settings;
	QString translatedLanguage;

	if (language == "English") {
		settings.setValue("language", QLocale(QLocale::English, QLocale::UnitedStates).name());
		translatedLanguage = tr("English");
	} else if (language == "French") {
		settings.setValue("language", QLocale(QLocale::French, QLocale::France).name());
		translatedLanguage = tr("French");
	} else if (language == "German") {
		settings.setValue("language", QLocale(QLocale::German, QLocale::Germany).name());
		translatedLanguage = tr("German");
	} else if (language == "Polish") {
		settings.setValue("language", QLocale(QLocale::Polish, QLocale::Poland).name());
		translatedLanguage = tr("Polish");
	} else if (language == "Spanish") {
		settings.setValue("language", QLocale(QLocale::Spanish, QLocale::Spain).name());
		translatedLanguage = tr("Spanish");
	} else if (language == "Portuguese") {
		settings.setValue("language", QLocale(QLocale::Portuguese, QLocale::Portugal).name());
		translatedLanguage = tr("Portuguese");
	}

	int result = QUMessageBox::information(this,
					tr("Change Language"),
					tr("Application language changed to <b>%1</b>. You need to restart UltraStar Creator to take effect.").arg(translatedLanguage),
					BTN << ":/icons/quit.png" << tr("Quit UltraStar Creator.")
						<< ":/icons/accept.png" << tr("Continue."));
	if(result == 0)
			this->close();
}


void QUMainWindow::BASS_Stop() {
		if(!_mediaStream)
			return;

		if(!BASS_ChannelStop(_mediaStream)) {
			//logSrv->add(QString("BASS ERROR: %1").arg(BASS_ErrorGetCode()), QU::Warning);
			return;
		}
}
#

void QUMainWindow::BASS_Free() {
		if(!_mediaStream)
			return;

		if(!BASS_StreamFree(_mediaStream)) {
			//logSrv->add(QString("BASS ERROR: %1").arg(BASS_ErrorGetCode()), QU::Warning);
			return;
		}
}

void QUMainWindow::BASS_StopAndFree() {
		if(!_mediaStream)
			return;

		if(!BASS_ChannelStop(_mediaStream)) {
			return;
		}

		if(!BASS_StreamFree(_mediaStream)) {
			return;
		}
}

void QUMainWindow::BASS_Play() {
		if(!_mediaStream) {
			return;
		}

		if(!BASS_ChannelPlay(_mediaStream, TRUE)) {
			return;
		}
}

void QUMainWindow::BASS_Pause() {
		if(!_mediaStream) {
			return;
		}

		if(!BASS_ChannelPause(_mediaStream)) {
			return;
		}
}

void QUMainWindow::BASS_Resume() {
		if(!_mediaStream) {
			return;
		}

		if(!BASS_ChannelPlay(_mediaStream, FALSE)) {
			return;
		}
}

/*!
 * Get current position in seconds of the stream.
 */
double QUMainWindow::BASS_Position() {
	if(!_mediaStream)
		return -1;

	return BASS_ChannelBytes2Seconds(_mediaStream, BASS_ChannelGetPosition(_mediaStream, BASS_POS_BYTE));
}

void QUMainWindow::BASS_SetPosition(int seconds) {
	if(!_mediaStream)
		return;

	QWORD pos = BASS_ChannelSeconds2Bytes(_mediaStream, (double)seconds);

	if(!BASS_ChannelSetPosition(_mediaStream, pos, BASS_POS_BYTE)) {
		//logSrv->add(QString("BASS ERROR: %1").arg(BASS_ErrorGetCode()), QU::Warning);
		return;
	}
}

void QUMainWindow::handleMP3() {
	setCursor(Qt::WaitCursor);

	ui->lineEdit_MP3->setText(fileInfo_MP3->fileName());

	_mediaStream = BASS_StreamCreateFile(FALSE, fileInfo_MP3->absoluteFilePath().toLocal8Bit().data() , 0, 0, BASS_STREAM_DECODE);
	QWORD MP3LengthBytes = BASS_ChannelGetLength(_mediaStream, BASS_POS_BYTE); // the length in bytes
	MP3LengthTime = BASS_ChannelBytes2Seconds(_mediaStream, MP3LengthBytes); // the length in seconds
	ui->horizontalSlider_MP3->setRange(0, (int)MP3LengthTime);
	ui->horizontalSlider_PreviewMP3->setRange(0, (int)MP3LengthTime);
	ui->horizontalSlider_MP3->setValue(0);
	ui->horizontalSlider_PreviewMP3->setValue(0);
	ui->label_TimeElapsed->setText("0:00");
	ui->label_PreviewTimeElapsed->setText("0:00");
	int minutesToRun = (MP3LengthTime / 60);
	int secondsToRun = ((int)MP3LengthTime % 60);
	ui->label_TimeToRun->setText(QString("-%1:%2").arg(minutesToRun).arg(secondsToRun, 2, 10, QChar('0')));
	ui->label_PreviewTimeToRun->setText(QString("-%1:%2").arg(minutesToRun).arg(secondsToRun, 2, 10, QChar('0')));

	// add all image files to cover/background combobox
	QStringList imageFiles = QDir(defaultDir).entryList(QUSongSupport::allowedImageFiles());

	ui->comboBox_Cover->addItems(imageFiles);
	ui->comboBox_Background->addItems(imageFiles);
	if (imageFiles.length() == 1)
		ui->comboBox_Cover->setCurrentIndex(1);
	else {
		ui->comboBox_Cover->setCurrentIndex(2);
		ui->comboBox_Background->setCurrentIndex(1);
	}

	// add all video files to video combobox
	QStringList videoFiles = QDir(defaultDir).entryList(QUSongSupport::allowedVideoFiles());
	ui->comboBox_Video->addItems(videoFiles);
	if (videoFiles.length() > 0)
		ui->comboBox_Video->setCurrentIndex(1);

	BPMFromMP3 = BASS_FX_BPM_DecodeGet(_mediaStream, 0, MP3LengthTime, 0, BASS_FX_BPM_BKGRND, 0);
	BPM = BPMFromMP3;

	if (BPM == 0) {
		BPM = 50;
	}

	while (BPM <= 200) {
		BPM = BPM*2;
	}

	ui->doubleSpinBox_BPM->setValue(BPM);
	ui->label_BPMSet->setPixmap(QPixmap(":/icons/path_ok.png"));

	TagLib::FileRef ref(fileInfo_MP3->absoluteFilePath().toLocal8Bit().data());
	ui->lineEdit_Artist->setText(TStringToQString(ref.tag()->artist()));
	ui->lineEdit_Title->setText(TStringToQString(ref.tag()->title()));
	ui->comboBox_Genre->setEditText(TStringToQString(ref.tag()->genre()));
	ui->comboBox_Year->setCurrentIndex(ui->comboBox_Year->findText(QString(ref.tag()->year())));
	// TODO: lyrics from mp3 lyrics-tag

	ui->label_MP3Set->setPixmap(QPixmap(":/icons/path_ok.png"));
	previewState = QUMainWindow::initialized;

	// enable all widgets
	bool enabled = true;
	ui->pushButton_PreviewPlayPause->setEnabled(enabled);
	ui->label_PreviewTimeElapsed->setEnabled(enabled);
	ui->horizontalSlider_PreviewMP3->setEnabled(enabled);
	ui->label_PreviewTimeToRun->setEnabled(enabled);
	ui->label_TitleIcon->setEnabled(enabled);
	ui->label_Title->setEnabled(enabled);
	ui->lineEdit_Title->setEnabled(enabled);
	ui->label_TitleSet->setEnabled(enabled);
	ui->label_ArtistIcon->setEnabled(enabled);
	ui->label_Artist->setEnabled(enabled);
	ui->lineEdit_Artist->setEnabled(enabled);
	ui->label_ArtistSet->setEnabled(enabled);
	ui->label_LanguageIcon->setEnabled(enabled);
	ui->label_Language->setEnabled(enabled);
	ui->comboBox_Language->setEnabled(enabled);
	ui->label_LanguageSet->setEnabled(enabled);
	ui->label_EditionIcon->setEnabled(enabled);
	ui->label_Edition->setEnabled(enabled);
	ui->lineEdit_Edition->setEnabled(enabled);
	ui->label_EditionSet->setEnabled(enabled);
	ui->label_GenreIcon->setEnabled(enabled);
	ui->label_Genre->setEnabled(enabled);
	ui->comboBox_Genre->setEnabled(enabled);
	ui->label_GenreSet->setEnabled(enabled);
	ui->label_YearIcon->setEnabled(enabled);
	ui->label_Year->setEnabled(enabled);
	ui->pushButton_ShowWebSite->setEnabled(enabled);
	ui->comboBox_Year->setEnabled(enabled);
	ui->label_YearSet->setEnabled(enabled);
	ui->label_CreatorIcon->setEnabled(enabled);
	ui->label_Creator->setEnabled(enabled);
	ui->lineEdit_Creator->setEnabled(enabled);
	ui->label_CreatorSet->setEnabled(enabled);
	ui->label_CoverIcon->setEnabled(enabled);
	ui->label_Cover->setEnabled(enabled);
	ui->comboBox_Cover->setEnabled(enabled);
	ui->label_CoverSet->setEnabled(enabled);
	ui->label_BackgroundIcon->setEnabled(enabled);
	ui->label_Background->setEnabled(enabled);
	ui->comboBox_Background->setEnabled(enabled);
	ui->label_BackgroundSet->setEnabled(enabled);
	ui->label_VideoIcon->setEnabled(enabled);
	ui->label_Video->setEnabled(enabled);
	ui->comboBox_Video->setEnabled(enabled);
	ui->label_VideoSet->setEnabled(enabled);
	ui->label_BPMIcon->setEnabled(enabled);
	ui->label_BPM->setEnabled(enabled);
	ui->pushButton_EnableBPMEdit->setEnabled(enabled);
	ui->doubleSpinBox_BPM->setEnabled(enabled);
	ui->label_BPMSet->setEnabled(enabled);

	ui->groupBox_Control->setEnabled(enabled);
	ui->groupBox_InputLyrics->setEnabled(enabled);

	if (!ui->plainTextEdit_InputLyrics->toPlainText().isEmpty()) {
		state = QUMainWindow::initialized;
		QMainWindow::statusBar()->showMessage(tr("State: initialized."));
		ui->pushButton_PlayPause->setEnabled(true);
	}
	else {
		state = QUMainWindow::uninitialized;
		QMainWindow::statusBar()->showMessage(tr("State: uninitialized."));
		ui->pushButton_PlayPause->setDisabled(true);
	}
	setCursor(Qt::ArrowCursor);

	montyTalk();
}

void QUMainWindow::setPlaybackSpeed(int value)
{
	_menu->label_PlaybackSpeedPercentage->setText(QString("%1 \%").arg(QString::number(value)));

	if (state == playing || state == paused) {
		BASS_ChannelSetAttribute(_mediaStream, BASS_ATTRIB_TEMPO_FREQ, sampleRate*_menu->horizontalSlider_PlaybackSpeed->value()/100);
	}
}

void QUMainWindow::aboutBASS()
{
	QUMessageBox::information(this,
							tr("About BASS"),
							QString(tr("<b>BASS Audio Library</b><br><br>"
											"BASS is an audio library for use in Windows and MacOSX software. Its purpose is to provide the most powerful and efficient (yet easy to use), sample, stream, MOD music, and recording functions. All in a tiny DLL, about 100KB in size.<br><br>"
											"Version: <b>%1</b><br>"
											"<br><br><b>BASS FX Effects Extension</b><br><br>"
											"BASS FX is an extension providing several effects, including tempo & pitch control.<br><br>"
											"Version: <b>2.4.7.1</b><br><br><br>"
											"Copyright (c) 2003-2010<br><a href=\"http://www.un4seen.com/bass.html\">Un4seen Developments Ltd.</a> All rights reserved.")).arg(BASSVERSIONTEXT),
							QStringList() << ":/icons/accept.png" << "OK",
							330);
}

void QUMainWindow::aboutTagLib()
{
	QUMessageBox::information(this,
							tr("About TagLib"),
							QString(tr("<b>TagLib Audio Meta-Data Library</b><br><br>"
											"TagLib is a library for reading and editing the meta-data of several popular audio formats.<br><br>"
											"Version: <b>%1.%2.%3</b><br><br>"
											"Visit: <a href=\"http://developer.kde.org/~wheeler/taglib.html\">TagLib Homepage</a>"))
											.arg(TAGLIB_MAJOR_VERSION)
											.arg(TAGLIB_MINOR_VERSION)
											.arg(TAGLIB_PATCH_VERSION));
}

void QUMainWindow::updateTime() {
		int posSec = (int)BASS_Position();
		int minutesElapsed = posSec / 60;
		int secondsElapsed = posSec % 60;
		ui->label_TimeElapsed->setText(QString("%1:%2").arg(minutesElapsed).arg(secondsElapsed, 2, 10, QChar('0')));
		int timeToRun = MP3LengthTime - posSec;
		int minutesToRun = (timeToRun / 60);
		int secondsToRun = (timeToRun % 60);
		ui->label_TimeToRun->setText(QString("-%1:%2").arg(minutesToRun).arg(secondsToRun, 2, 10, QChar('0')));

		if(!ui->horizontalSlider_MP3->isSliderDown())
		{
			ui->horizontalSlider_MP3->setValue(posSec);
		}

		if(posSec != -1) {
			QTimer::singleShot(1000, this, SLOT(updateTime()));
		}
}

void QUMainWindow::updatePreviewTime() {
		int posSec = (int)BASS_Position();
		int minutesElapsed = posSec / 60;
		int secondsElapsed = posSec % 60;
		ui->label_PreviewTimeElapsed->setText(QString("%1:%2").arg(minutesElapsed).arg(secondsElapsed, 2, 10, QChar('0')));
		int timeToRun = MP3LengthTime - posSec;
		int minutesToRun = (timeToRun / 60);
		int secondsToRun = (timeToRun % 60);
		ui->label_PreviewTimeToRun->setText(QString("-%1:%2").arg(minutesToRun).arg(secondsToRun, 2, 10, QChar('0')));
		ui->horizontalSlider_PreviewMP3->setValue(posSec);

		if (posSec != -1) {
			QTimer::singleShot(1000, this, SLOT(updatePreviewTime()));
		}
}

void QUMainWindow::on_pushButton_Reset_clicked()
{
	if (state == stopped) {
		timeLineMap.clear();
		lyricsProgressBar->hide();
		state = QUMainWindow::initialized;
		previewState = QUMainWindow::initialized;
		numSyllables = 0;
		firstNoteStartBeat = 0;
		currentNoteBeatLength = 0;
		currentSyllableIndex = 0;
		currentCharacterIndex = 0;
		firstNote = true;
		lyricsSyllableList.clear();
		ui->plainTextEdit_OutputLyrics->clear();
		ui->pushButton_Tap->setText("");
		ui->pushButton_NextSyllable1->setText("");
		ui->pushButton_NextSyllable2->setText("");
		ui->pushButton_NextSyllable3->setText("");
		ui->pushButton_NextSyllable4->setText("");
		ui->pushButton_NextSyllable5->setText("");
		ui->horizontalSlider_MP3->setValue(0);
		lyricsProgressBar->setValue(0);
		ui->groupBox_SongMetaInformationTags->setEnabled(true);
		ui->groupBox_InputLyrics->setEnabled(true);
		ui->groupBox_OutputLyrics->setDisabled(true);
		ui->groupBox_TapArea->setDisabled(true);
		ui->pushButton_Tap->setDisabled(true);
		ui->pushButton_UndoTap->setDisabled(true);
		ui->pushButton_PlayPause->setEnabled(true);
		ui->pushButton_Stop->setDisabled(true);
		ui->pushButton_Reset->setDisabled(true);
		_mediaStream = BASS_StreamCreateFile(FALSE, fileInfo_MP3->absoluteFilePath().toLocal8Bit().data() , 0, 0, BASS_STREAM_DECODE);
	}
	else {
		// should not be possible
	}

	montyTalk();
}

void QUMainWindow::on_pushButton_UndoTap_clicked()
{
	QMutableMapIterator<double, QString> i(timeLineMap);

	if (currentSyllableIndex > 0) {
		currentSyllableIndex = currentSyllableIndex-1;
		if (currentSyllableIndex == 0) {
			firstNote = true;
			timeLineMap.remove(-1); // delete GAP
			ui->pushButton_UndoTap->setDisabled(true);
		}
		updateSyllableButtons();
		lyricsProgressBar->setValue(lyricsProgressBar->value()-1);
		i.toBack();
		i.previous();
		timeLineMap.remove(i.key());

		updateOutputLyrics();

		double undoTime = 1.0; // step 1 second back in MP3
		BASS_SetPosition(qMax(BASS_Position() - undoTime, 0.0));
	}
}

void QUMainWindow::updateSyllableButtons() {
	QString syllable = lyricsSyllableList[currentSyllableIndex];
	if (syllable.startsWith("\n")) {
		syllable.replace("\n","");
		ui->pushButton_Tap->setIcon(QIcon(":/icons/pilcrow.png"));
	}
	else {
		ui->pushButton_Tap->setIcon(QIcon());
	}
	ui->pushButton_Tap->setText(syllable);

	if (currentSyllableIndex+1 < numSyllables) {
		syllable = lyricsSyllableList[currentSyllableIndex+1];
		if (syllable.startsWith("\n")) {
			syllable.replace("\n","");
		   ui->pushButton_NextSyllable1->setIcon(QIcon(":/icons/pilcrow.png"));
		}
		else {
			ui->pushButton_NextSyllable1->setIcon(QIcon());
		}
		ui->pushButton_NextSyllable1->setText(syllable);
	}
	else {
		ui->pushButton_NextSyllable1->setText("");
	}
	if (currentSyllableIndex+2 < numSyllables) {
		syllable = lyricsSyllableList[currentSyllableIndex+2];
		if (syllable.startsWith("\n")) {
			syllable.replace("\n","");
			ui->pushButton_NextSyllable2->setIcon(QIcon(":/icons/pilcrow.png"));
		}
		else {
			ui->pushButton_NextSyllable2->setIcon(QIcon());
		}
		ui->pushButton_NextSyllable2->setText(syllable);
	}
	else {
		ui->pushButton_NextSyllable2->setText("");
	}
	if (currentSyllableIndex+3 < numSyllables) {
		syllable = lyricsSyllableList[currentSyllableIndex+3];
		if (syllable.startsWith("\n")) {
			syllable.replace("\n","");
			ui->pushButton_NextSyllable3->setIcon(QIcon(":/icons/pilcrow.png"));
		}
		else {
			ui->pushButton_NextSyllable3->setIcon(QIcon());
		}
		ui->pushButton_NextSyllable3->setText(syllable);
	}
	else {
		ui->pushButton_NextSyllable3->setText("");
	}
	if (currentSyllableIndex+4 < numSyllables) {
		syllable = lyricsSyllableList[currentSyllableIndex+4];
		if (syllable.startsWith("\n")) {
			syllable.replace("\n","");
			ui->pushButton_NextSyllable4->setIcon(QIcon(":/icons/pilcrow.png"));
		}
		else {
			ui->pushButton_NextSyllable4->setIcon(QIcon());
		}
		ui->pushButton_NextSyllable4->setText(syllable);
	}
	else {
		ui->pushButton_NextSyllable4->setText("");
	}
	if (currentSyllableIndex+5 < numSyllables) {
		syllable = lyricsSyllableList[currentSyllableIndex+5];
		if (syllable.startsWith("\n")) {
			syllable.replace("\n","");
			ui->pushButton_NextSyllable5->setIcon(QIcon(":/icons/pilcrow.png"));
		}
		else {
			ui->pushButton_NextSyllable5->setIcon(QIcon());
		}
		ui->pushButton_NextSyllable5->setText(syllable);
	}
	else {
		ui->pushButton_NextSyllable5->setText("");
	}
}

void QUMainWindow::splitLyricsIntoSyllables()
{
	int nextSeparatorIndex = lyricsString.mid(1).indexOf(QRegExp("[ +\\n]"));
	QString currentSyllable = "";

	while (nextSeparatorIndex != -1) {
		currentSyllable = lyricsString.mid(0,nextSeparatorIndex+1);
		if (currentSyllable.startsWith("+")) {
			currentSyllable = currentSyllable.mid(1);
		}
		lyricsSyllableList.append(currentSyllable);
		lyricsString = lyricsString.mid(nextSeparatorIndex+1);
		nextSeparatorIndex = lyricsString.mid(1).indexOf(QRegExp("[ +\\n]"));
		if (nextSeparatorIndex == -1) {
			currentSyllable = lyricsString;
			if (currentSyllable.startsWith("+")) {
				currentSyllable = currentSyllable.mid(1);
			}
			lyricsSyllableList.append(currentSyllable);
		}
	}
}

void QUMainWindow::keyPressEvent(QKeyEvent *event) {
	if (state == QUMainWindow::playing && isFirstKeyPress) {
		isFirstKeyPress = false;
		switch(event->key()) {
		case Qt::Key_V:
			event->ignore();
			QUMainWindow::on_pushButton_Tap_pressed();
			break;
		case Qt::Key_X:
			event->ignore();
			QUMainWindow::on_pushButton_UndoTap_clicked();
			break;
		default: QWidget::keyPressEvent(event);
		}
	}
	else {
		QWidget::keyPressEvent(event);
	}
}

void QUMainWindow::keyReleaseEvent(QKeyEvent *event) {
	if (state == QUMainWindow::playing && !isFirstKeyPress) {
		isFirstKeyPress = true;
		switch(event->key()) {
		case Qt::Key_V:
			event->ignore();
			QUMainWindow::on_pushButton_Tap_released();
		default: QWidget::keyPressEvent(event);
		}
	}
	else {
		QWidget::keyPressEvent(event);
	}
}

void QUMainWindow::on_comboBox_Year_activated(QString year)
{
	if(!year.isEmpty()) {
		ui->label_YearSet->setPixmap(QPixmap(":/icons/path_ok.png"));
	}
	else {
		ui->label_YearSet->setPixmap(QPixmap(":/icons/path_error.png"));
	}
}

void QUMainWindow::on_horizontalSlider_MP3_sliderMoved(int position)
{
	BASS_SetPosition(position);
}

void QUMainWindow::on_horizontalSlider_PreviewMP3_sliderMoved(int position)
{
	BASS_SetPosition(position);
}

void QUMainWindow::on_pushButton_startUltraStar_clicked()
{
	QSettings settings;
	QString USdxFilePath;
	QFileInfo *USdxFileInfo;

	if(settings.contains("USdxFilePath")) {
		USdxFilePath = settings.value("USdxFilePath").toString();
		USdxFileInfo = new QFileInfo(USdxFilePath);
		if(USdxFileInfo->exists()) {
			settings.setValue("USdxFilePath", USdxFilePath);
			QStringList USdxArguments;
			USdxArguments << "-SongPath" << fileInfo_MP3->absolutePath();
			QProcess::startDetached(USdxFilePath, USdxArguments, USdxFileInfo->absolutePath());
		}
		else {
			settings.remove("USdxFilePath");
			USdxFilePath = QFileDialog::getOpenFileName(0, tr("Choose UltraStar executable"), QDir::homePath(),tr("UltraStar executable (*.exe);;All files (*.*)"));
			USdxFileInfo = new QFileInfo(USdxFilePath);
			if(USdxFileInfo->exists()) {
				settings.setValue("USdxFilePath", USdxFilePath);
				QStringList USdxArguments;
				USdxArguments << "-SongPath " << fileInfo_MP3->absolutePath();
				QProcess::startDetached(USdxFilePath, USdxArguments, USdxFileInfo->absolutePath());
			}
		}
	}
	else {
		USdxFilePath = QFileDialog::getOpenFileName(0, tr("Choose UltraStar executable"), QDir::homePath(),tr("UltraStar executable (*.exe);;All files (*.*)"));
		USdxFileInfo = new QFileInfo(USdxFilePath);
		if(USdxFileInfo->exists()) {
			settings.setValue("USdxFilePath", USdxFilePath);
			QStringList USdxArguments;
			USdxArguments << "-SongPath " << fileInfo_MP3->absolutePath();
			QProcess::startDetached(USdxFilePath, USdxArguments, USdxFileInfo->absolutePath());
		}
	}
}

void QUMainWindow::on_pushButton_PreviewPlayPause_clicked()
{
	if (previewState == QUMainWindow::initialized) {
		previewState = QUMainWindow::playing;
		_mediaStream = BASS_FX_TempoCreate(_mediaStream, BASS_FX_FREESOURCE);
		bool result = BASS_ChannelSetAttribute(_mediaStream, BASS_ATTRIB_TEMPO, 0);
		if (result) {
			BASS_Play();
			updatePreviewTime();
		}
		ui->pushButton_PreviewPlayPause->setIcon(QIcon(":/icons/control_pause_blue.png"));
	}
	else if (previewState == QUMainWindow::playing) {
		previewState = QUMainWindow::paused;
		BASS_Pause();
		ui->pushButton_PreviewPlayPause->setIcon(QIcon(":/icons/control_play_blue.png"));
	}
	else if (previewState == QUMainWindow::paused) {
		previewState = QUMainWindow::playing;
		BASS_Resume();
		ui->pushButton_PreviewPlayPause->setIcon(QIcon(":/icons/control_pause_blue.png"));
	}
	else {
		// should not occur
	}
}

void QUMainWindow::on_pushButton_startYass_clicked()
{
	QSettings settings;
	QString YassFilePath;
	QFileInfo *YassFileInfo;

	if(settings.contains("YassFilePath")) {
		YassFilePath = settings.value("YassFilePath").toString();
		YassFileInfo = new QFileInfo(YassFilePath);
		if(YassFileInfo->exists()) {
			settings.setValue("YassFilePath", YassFilePath);
			QStringList YassArguments;
			YassArguments << fileInfo_MP3->absolutePath();
			QProcess::startDetached(YassFilePath, YassArguments, YassFileInfo->absolutePath());
		}
		else {
			settings.remove("YassFilePath");
			YassFilePath = QFileDialog::getOpenFileName(0, tr("Choose YASS executable"), QDir::homePath(),tr("YASS executable (*.exe);;All files (*.*)"));
			YassFileInfo = new QFileInfo(YassFilePath);
			if(YassFileInfo->exists()) {
				settings.setValue("YassFilePath", YassFilePath);
				QStringList YassArguments;
				YassArguments << fileInfo_MP3->absolutePath();
				QProcess::startDetached(YassFilePath, YassArguments, YassFileInfo->absolutePath());
			}
		}
	}
	else {
		YassFilePath = QFileDialog::getOpenFileName(0, tr("Choose YASS executable"), QDir::homePath(),tr("YASS executable (*.exe);;All files (*.*)"));
		YassFileInfo = new QFileInfo(YassFilePath);
		if(YassFileInfo->exists()) {
			settings.setValue("YassFilePath", YassFilePath);
			QStringList YassArguments;
			YassArguments << fileInfo_MP3->absolutePath();
			QProcess::startDetached(YassFilePath, YassArguments, YassFileInfo->absolutePath());
		}
	}
}

void QUMainWindow::generateFreestyleTextFiles()
{
	int result = QUMessageBox::question(0,
					QObject::tr("Freestyle text file generation."),
					QObject::tr("This function will generate UltraStar compatible freestyle text files without any lyrics for each audio file in a subsequently selectable folder.<br><br>Each MP3 will be moved into a separate subdirectory and a text file containing the bare minimum of information will be automatically created along with a standard cover and background.<br><br>If your audio files follow an 'Artist - Title.mp3' naming scheme, they will be correctly mapped in the resulting song file."),
					BTN << ":/icons/accept.png" << QObject::tr("Go ahead, I know what I am doing!")
						<< ":/icons/cancel.png" << QObject::tr("I'm not sure. I want to cancel."),400,0);

	if (result == 0) {
		QString SongCollectionPath;
		SongCollectionPath = QFileDialog::getExistingDirectory(0, tr("Choose root song folder"), QDir::homePath());

		if (SongCollectionPath.isEmpty())
		{
			return;
		}

		QApplication::setOverrideCursor(Qt::WaitCursor);
		int freestyleSongsCount = 0;
		QDirIterator it(SongCollectionPath, QDirIterator::Subdirectories);
		while (it.hasNext()) {
			it.next();
			if (it.fileInfo().suffix().toLower() == tr("mp3") || it.fileInfo().suffix().toLower() == tr("ogg")) {

				QMainWindow::statusBar()->showMessage(tr("Creating %1").arg(it.fileInfo().completeBaseName()));

				// create directory, move MP3/OGG into it
				QFile songFile(it.fileInfo().absoluteFilePath());
				QFileInfo songInfo(songFile);
				int separatorPos = songInfo.fileName().indexOf(" - ");

				QString artist;
				QString title;
				QString dirName;

				if (separatorPos != -1) {
					artist = songInfo.completeBaseName().mid(0, separatorPos);
					artist = artist.trimmed();
					QStringList tokens = artist.split(QRegExp("(\\s+)"));
						QList<QString>::iterator tokItr = tokens.begin();

						for (tokItr = tokens.begin(); tokItr != tokens.end(); ++tokItr) {
						(*tokItr) = (*tokItr).at(0).toUpper() + (*tokItr).mid(1);
						}
					artist = tokens.join(" ");
					artist.replace("Feat.", "feat.", Qt::CaseSensitive);
					artist.replace("With.", "with.", Qt::CaseSensitive);
					artist.replace("Vs.", "vs.", Qt::CaseSensitive);

					title = songInfo.completeBaseName().mid(separatorPos + 3);
					title = title.trimmed();
					tokens = title.split(QRegExp("(\\s+)"));
						tokItr = tokens.begin();

						for (tokItr = tokens.begin(); tokItr != tokens.end(); ++tokItr) {
						(*tokItr) = (*tokItr).at(0).toUpper() + (*tokItr).mid(1);
						}
					title = tokens.join(" ");

					dirName = QString("%1 - %2").arg(artist).arg(title);
				}
				else { // audio file does not follow "Artist - Title.*" naming scheme
					artist = songInfo.completeBaseName();
					title = "";

					dirName = artist;
				}

				songInfo.dir().mkdir(dirName);
				QString newFileName(QString("%1/%2/%3.%4").arg(songInfo.absolutePath()).arg(dirName).arg(dirName).arg(songInfo.suffix().toLower()));
				songFile.rename(newFileName);

				// text file
				QString textFilename(QString("%1/%2/%3.txt").arg(songInfo.absolutePath()).arg(dirName).arg(dirName));
				QFile file(textFilename);
				if (!file.open(QFile::WriteOnly | QFile::Text)) {
					QUMessageBox::warning(this, tr("Application"),
						tr("Cannot write file: %1\n%2.")
						.arg(textFilename)
						.arg(file.errorString()));
				}

				QTextStream out(&file);
				QTextCodec *codec = QTextCodec::codecForName("Windows-1252");
				//QTextCodec *codec = QTextCodec::codecForName("UTF-8");
				//out.setGenerateByteOrderMark(true); // BOM needed by UltraStar 1.1

				out.setCodec(codec);
				QString textString;
				if (separatorPos != -1) {
					textString += QString("#TITLE:%1\n").arg(title);
				}
				else {
					textString += QString("#TITLE:?\n");
				}
				textString += QString("#ARTIST:%1\n").arg(artist);
				textString += "#LANGUAGE:\n";
				textString += "#EDITION:\n";
				textString += "#GENRE:\n";
				textString += "#YEAR:\n";
				if (separatorPos != -1) {
					textString += QString("#MP3:%1 - %2.%3\n").arg(artist).arg(title).arg(songInfo.suffix().toLower());
					textString += QString("#COVER:%1 - %2 [CO].jpg\n").arg(artist).arg(title);
					textString += QString("#BACKGROUND:%1 - %2 [BG].jpg\n").arg(artist).arg(title);
				}
				else {
					textString += QString("#MP3:%1.%2\n").arg(artist).arg(songInfo.suffix().toLower());
					textString += QString("#COVER:%1 [CO].jpg\n").arg(artist);
					textString += QString("#BACKGROUND:%1 [BG].jpg\n").arg(artist);
				}
				textString += "#BPM:300\n";
				textString += "#GAP:0\n";
				textString += "F 0 0 0 \n";
				textString += "- 1\n";
				textString += "F 1 0 0 \n";
				textString += "E\n";
				out << textString;

				// Cover
				QString coverFilename(QString("%1/%2/%3 [CO].jpg").arg(songInfo.absolutePath()).arg(dirName).arg(dirName));
				QFile cover(":/NoCover.jpg");
				cover.copy(coverFilename);

				// Background
				QString backgroundFilename(QString("%1/%2/%3 [BG].jpg").arg(songInfo.absolutePath()).arg(dirName).arg(dirName));
				QFile background(":/NoBackground.jpg");
				background.copy(backgroundFilename);

				freestyleSongsCount++;
			}
		}
		QApplication::restoreOverrideCursor();

		QUMessageBox::information(0,
								  QObject::tr("Freestyle text file generation completed."),
								  QObject::tr("Successfully created %1 freestyle songs.").arg(freestyleSongsCount),
								  BTN << ":/icons/accept.png" << QObject::tr("OK"),
								  400,
								  0);
	}
	else {
		// user cancelled
	}
}

void QUMainWindow::on_actionHelp_triggered()
{
	QUMessageBox::information(0, QObject::tr("Welcome to UltraStar Creator!"),
							  QObject::tr("This tool enables you to <b>rapidly</b> create UltraStar text files <b>from scratch</b>.<br><br>To get started, simply chose a <b>song file</b> in MP3 or OGG format, insert the <b>song lyrics</b> from a file or the clipboard and divide them into syllables with '+'.<br><br><b>Important song meta information</b> such as <b>BPM</b> and <b>GAP</b> are determined <b>automatically</b> while the <b>ID3 tag</b> is used to fill in additional song details, if available.<br><br>To <b>start tapping</b>, hit the play/pause button (Keyboard: CTRL-P). Keep the <b>tap button</b> (keyboard: space bar) pressed for as long as the current syllable is sung to tap a note. <b>Undo</b> the last tap with the undo button (Keyboard: x), <b>stop tapping</b> with the stop button (Keyboard: CTRL-S), <b>restart</b> from the beginning with the reset button (Keyboard: CTRL-R). When finished, <b>save</b> the tapped song using the save button (CTRL-S).<br><br>Having successfully tapped a song, use the UltraStar internal editor for <b>finetuning the timings</b>, setting <b>note pitches</b> and <b>golden</b> or <b>freestyle notes</b>.<br><br><b>Happy creating!</b>"),
							  BTN << ":/icons/accept.png" << QObject::tr("Okay. Let's go!"),550,0);
}

// begin syllabification --> thanks to Klafhor who provided the PHP code as a basis

void QUMainWindow::on_pushButton_SyllabificateRules_clicked()
{
	QString lyrics = ui->plainTextEdit_InputLyrics->toPlainText();
	QString language = ui->comboBox_Language->itemData(ui->comboBox_Language->currentIndex()).toString();
	QChar sep = '+';
	QString syllabifiedLyrics = "";
	QString lang;

	if ((language != "English") && (language != "German") && (language != "Spanish")) {
		QString infoString;
		if (!ui->comboBox_Language->currentText().isEmpty()) {
			infoString = tr("The automatic lyrics syllabification is not (yet) available for <b>%1</b>.").arg(ui->comboBox_Language->currentText());
		}
		else {
			infoString = tr("The song language has not yet been set.");
		}
		int result = QUMessageBox::question(0,
						QObject::tr("Syllabification."),
						QObject::tr("%1 Apply...").arg(infoString),
						BTN << ":/languages/us.png"    << QObject::tr("English syllabification rules.")
							<< ":/languages/de.png"    << QObject::tr("German syllabification rules.")
							<< ":/languages/es.png"    << QObject::tr("Spanish syllabification rules.")
							<< ":/icons/cancel.png"         << QObject::tr("Cancel."));

		if (result == 0) {
			language = "English";
		}
		else if (result == 1) {
			language = "German";
		}
		else if (result == 2) {
			language = "Spanish";
		}
		else {
			syllabifiedLyrics = lyrics;
		}
	}

	if (language == "English") {
		lang = "en";

		for (int i = 0; i < lyrics.length(); i++)
		{
			QChar ch1 = lyrics.at(i);
			QChar ch2 = lyrics.at(i+1);
			QChar ch3 = lyrics.at(i+2);
			QChar ch4 = lyrics.at(i+3);
			QChar ch5 = lyrics.at(i+4);

			// "e" at the end of a word isn't considered a vowel, for example "case"
			if (isVowel(ch1, lang) && isConsonant(ch2, lang) && ch3.toLower() == 'e' && ((!isConsonant(ch4, lang) && !isVowel(ch4, lang)) || (ch4.toLower() == 's' && (!isConsonant(ch5, lang) && !isVowel(ch5, lang))))) {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + ch2 + ch3;
				i = i + 2;
			}
			// "e" at the end of a word preceded by two consonants isn't considered a vowel, for example "table"
			else if (isVowel(ch1, lang) && isConsonant(ch2, lang) && isConsonant(ch3, lang) && ch4.toLower() == 'e' && ((!isConsonant(ch5, lang) && !isVowel(ch5, lang)) || ch5.toLower() == 's')) {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + ch2 + ch3 + ch4;
				i = i + 3;
			}
			// initial consonants
			else if ((!isVowel(ch1, lang) && !isConsonant(ch1, lang)) && isConsonant(ch2, lang) && isConsonant(ch3, lang) && (isVowel(ch4, lang) || isConsonant(ch4, lang))) {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + ch2 + ch3;
				i = i + 2;
			}
			// a consonant between 2 vowels
			else if (isVowel(ch1, lang) && isConsonant(ch2, lang) && (isVowel(ch3, lang) || ch3.toLower() == 'y')) {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + sep;
			}
			// inseparable consonants between vowels
			else if ((isVowel(ch1, lang) || isConsonant(ch1, lang)) && isConsonant(ch2, lang) && isConsonant(ch3, lang) && (isVowel(ch4, lang) || ch4.toLower() == 'y') && isInseparable(ch2, ch3, lang)) {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + sep;
			}
			// separable consonants between vowels
			else if ((isVowel(ch1, lang) || isConsonant(ch1, lang)) && isConsonant(ch2, lang) && isConsonant(ch3, lang) && (isVowel(ch4, lang) || ch4.toLower() == 'y') && !isInseparable(ch2, ch3, lang)) {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + ch2 + sep;
				i = i + 1;
			}
			// "qu" is inseparable
			else if (ch1.toLower() == 'q' && ch2.toLower() == 'u') {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + ch2;
				i = i + 1;
			}
			/*
			// hiatus
			else if (isVowel(ch1) && isVowel(ch2) && isHiatus(ch1, ch2)) {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + sep;
			}*/
			// other options
			else {
				syllabifiedLyrics = syllabifiedLyrics + ch1;
			}
		}
	}
	else if (language == "German") {
		lang = "de";

		for (int i = 0; i < lyrics.length(); i++)
		{
			QChar ch1 = lyrics.at(i);
			QChar ch2 = lyrics.at(i+1);
			QChar ch3 = lyrics.at(i+2);
			QChar ch4 = lyrics.at(i+3);
			QChar ch5 = lyrics.at(i+4);
			QChar ch6 = lyrics.at(i+5);
			QChar ch7 = lyrics.at(i+6);
			QChar ch8 = lyrics.at(i+7);

			// consonants at the beginning
			if ((!isVowel(ch1, lang) && !isConsonant(ch1, lang)) && isConsonant(ch2, lang) && isConsonant(ch3, lang) && (isVowel(ch4, lang) || isConsonant(ch4, lang))) {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + ch2 + ch3;
				i = i + 2;
			}
			// consonant between two vowels
			else if (isVowel(ch1, lang) && isConsonant(ch2, lang) && (isVowel(ch3, lang) || ch3.toLower() == 'y')) {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + sep;
			}
			// double consonant isn't separated in german
			else if ((isVowel(ch1, lang) || isConsonant(ch1, lang)) && isConsonant(ch2, lang) && isConsonant(ch3, lang) && (isVowel(ch4, lang) || ch4.toLower() == 'y') && ch2.toLower() == ch3.toLower()) {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + ch2 + sep;
				i = i + 1;
			}
			// biss-chen is an exception where "sch" is separated
			else if (ch1.toLower() == 'b' && ch2.toLower() == 'i' && ch3.toLower() == 's' && ch4.toLower() == 's' && ch5.toLower() == 'c') {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + ch2 + ch3 + ch4 + sep;
				i = i + 3;
			}
			// Diminutiv 1: "�s+chen" or "�s+chen", e. g. "H�s+chen", "Mi+m�s+chen", "R�s+chen", "H�s+chen", "N�s+chen"
			else if ((ch1.toLower() == '�' || ch1.toLower() == '�') && ch2.toLower() == 's' && ch3.toLower() == 'c' && ch4.toLower() == 'h' && ch5.toLower() == 'e' && ch6.toLower() == 'n') {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + ch2 + sep + ch3 + ch4 + ch5 + ch6;
				i = i + 5;
			}
			// Diminutiv 2: "�us+chen", e. g. "M�us+chen", "P�us+chen", "H�us+chen"
			else if (ch1.toLower() == '�' && ch2.toLower() == 'u' && ch3.toLower() == 's' && ch4.toLower() == 'c' && ch5.toLower() == 'h' && ch6.toLower() == 'e' && ch7.toLower() == 'n') {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + ch2 + ch3 + sep + ch4 + ch5 + ch6 + ch7;
				i = i + 6;
			}
			// Diminutiv 3: Umlaut + "ss" + suffix "chen", e. g. "F�ss+chen", "D�s+chen", "K�ss+chen", "Fl�ss+chen", "N�ss+chen"
			else if (isUmlaut(ch1, lang) && ch2.toLower() == 's' && ch3.toLower() == 's' && ch4.toLower() == 'c' && ch5.toLower() == 'h' && ch6.toLower() == 'e' && ch7.toLower() == 'n') {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + ch2 + ch3 + sep + ch4 + ch5 + ch6 + ch7;
				i = i + 6;
			}
			// Diminutiv 4: Umlaut + "sch" + suffix "chen", e. g. "Fl�sch+chen"
			else if (isUmlaut(ch1, lang) && ch2.toLower() == 's' && ch3.toLower() == 'c' && ch4.toLower() == 'h' && ch5.toLower() == 'c' && ch6.toLower() == 'h' && ch7.toLower() == 'e' && ch8.toLower() == 'n') {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + ch2 + ch3 + ch4 + sep + ch5 + ch6 + ch7 + ch8;
				i = i + 7;
			}
			// "sch" is inseparable in all other cases
			else if ((isVowel(ch1, lang) || isConsonant(ch1, lang)) && ch2.toLower() == 's' && ch3.toLower() == 'c' && ch4.toLower() == 'h') {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + sep + ch2;
				i = i + 1;
			}
			// prefix "ab"
			else if (ch1.toLower() == 'a' && ch2.toLower() == 'b' && ch3.isLetter()) {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + ch2 + sep;
				i = i + 1;
			}
			// prefix "auf"
			else if (ch1.toLower() == 'a' && ch2.toLower() == 'u' && ch3.toLower() == 'f' && ch4.isLetter() && !((ch4.toLower() == 'e' && ch5.toLower() == 'n') || ch4.toLower() == 't')) {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + ch2 + ch3 + sep;
				i = i + 2;
			}
			// prefix "aus"
			else if (ch1.toLower() == 'a' && ch2.toLower() == 'u' && ch3.toLower() == 's' && ch4.isLetter()) {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + ch2 + ch3 + sep;
				i = i + 2;
			}
			/* problem: abnabeln
			// prefix "be"
			else if (ch1.toLower() == 'b' && ch2.toLower() == 'e') {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + ch2 + sep;
				i = i + 1;
			}*/
			// prefix "ver"
			else if (ch1.toLower() == 'v' && ch2.toLower() == 'e' && ch3.toLower() == 'r' && ch4.isLetter()) {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + ch2 + ch3 + sep;
				i = i + 2;
			}
			// "eie" is separated into "ei-e", e. g. "Klei+e", "Fei+er"
			else if (ch1.toLower() == 'e' && ch2.toLower() == 'i' && ch3.toLower() == 'e') {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + ch2 + sep;
				i = i + 1;
			}
			// "eue" is separated into "eu-e", e. g. "Reu+e"
			else if (ch1.toLower() == 'e' && ch2.toLower() == 'u' && ch3.toLower() == 'e') {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + ch2 + sep;
				i = i + 1;
			}
			// inseparable consonants between vowels
			else if ((isVowel(ch1, lang) || isConsonant(ch1, lang)) && isConsonant(ch2, lang) && isConsonant(ch3, lang) && (isVowel(ch4, lang) || ch4.toLower() == 'y') && isInseparable(ch2, ch3, lang)) {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + sep;
			}
			// separable consonants between vowels
			else if ((isVowel(ch1, lang) || isConsonant(ch1, lang)) && isConsonant(ch2, lang) && isConsonant(ch3, lang) && (isVowel(ch4, lang) || ch4.toLower() == 'y') && !isInseparable(ch2, ch3, lang)) {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + ch2 + sep;
				i = i + 1;
			}
			// "qu" is inseparable
			else if (ch1.toLower() == 'q' && ch2.toLower() == 'u') {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + ch2;
				i = i + 1;
			}
			// all other cases
			else {
				syllabifiedLyrics = syllabifiedLyrics + ch1;
			}
			/* remaining Problems:
				Situation   --> Si+tua+tion
				weshalb     --> we+shalb
				verschonen  --> ver+s+cho+nen
				symmetrisch --> symme+tri+sch */
		}
	}
	else if (language == "Spanish") {
		lang = "es";

		for (int i = 0; i < lyrics.length(); i++)
		{
			QChar ch1 = lyrics.at(i);
			QChar ch2 = lyrics.at(i+1);
			QChar ch3 = lyrics.at(i+2);
			QChar ch4 = lyrics.at(i+3);

			// initial consonants don't separate in a word, the syllable need a vowel, by example "t-X-ranslation
			if ((!isVowel(ch1, lang) && !isConsonant(ch1, lang)) && isConsonant(ch2, lang) && isConsonant(ch3, lang) && (isVowel(ch4, lang) || isConsonant(ch4, lang)))
			{
				syllabifiedLyrics = syllabifiedLyrics + ch1 + ch2 + ch3;
				i = i + 2;
			}
			// consonant between 2 vowels, we separate this, by example "e-xample"
			else if (isVowel(ch1, lang) && isConsonant(ch2, lang) && (isVowel(ch3, lang) || ch3.toLower() == 'y')) {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + sep;
			}
			// inseparable consonants between vowels, for example "hel-X-lo"
			else if ((isVowel(ch1, lang) || isConsonant(ch1, lang)) && isConsonant(ch2, lang) && isConsonant(ch3, lang) && (isVowel(ch4, lang) || ch4.toLower() == 'y') && isInseparable(ch2, ch3, lang)) {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + sep;
			}
			// separable consonants between vowels, for example "bet-ween"
			else if ((isVowel(ch1, lang) || isConsonant(ch1, lang)) && isConsonant(ch2, lang) && isConsonant(ch3, lang) && (isVowel(ch4, lang) || ch4.toLower() == 'y') && !isInseparable(ch2, ch3, lang)) {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + ch2 + sep;
				i = i + 1;
			}
			// the characters "qu" are inseparable "q-X-u"
			else if (ch1.toLower() == 'q' && ch2.toLower() == 'u') {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + ch2;
				i = i + 1;
			}
			// hiatus
			else if (isVowel(ch1, lang) && isVowel(ch2, lang) && isHiatus(ch1, ch2, lang)) {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + sep;
			}
			// synalepha first word ending in a vowel
			else if (isVowel(ch1, lang) && ch2 == ' ' && ((isVowel(ch3, lang) || (ch3.toLower() == 'y' && (!isVowel(ch4, lang) && !isConsonant(ch4, lang)))) || (ch3.toLower() == 'h' && isVowel(ch4, lang)))) {
				syllabifiedLyrics = syllabifiedLyrics + ch1 + "�";
				i = i + 1;
			}
			// synalepha first word being 'y'
			else if ((ch1 == ' ') && ch2.toLower() == 'y' && ch3 == ' ' && (isVowel(ch4, lang) || ch4.toLower() == 'h')) {
				syllabifiedLyrics = syllabifiedLyrics + " y�";
				i = i + 2;
			}
			// synalepha and the beginning of the text
			else if (i == 1 && ch1.toLower() == 'y' && ch2 == ' ' && (isVowel(ch3, lang) || ch3.toLower() == 'h')) {
				syllabifiedLyrics = syllabifiedLyrics + "y�";
				i = i + 1;
			}
			// all other cases
			else {
				syllabifiedLyrics = syllabifiedLyrics + ch1;
			}
		}
	}
	ui->plainTextEdit_InputLyrics->setPlainText(syllabifiedLyrics);
}

bool QUMainWindow::isVowel(QChar character, QString lang)
{
	QChar ch = character.toLower();

	if (lang == "en")
	{
		if (ch == 'a' || ch == 'e' || ch == 'i' || ch == 'o' || ch == 'u')
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else if (lang == "de")
	{
		if (ch == 'a' || ch == 'e' || ch == 'i' || ch == 'o' || ch == 'u' || ch == '�' || ch == '�' || ch == '�')
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else if (lang == "es")
	{
		if (ch == 'a' || ch == 'e' || ch == 'i' || ch == 'o' || ch == 'u' || ch == '�' || ch == '�' || ch == '�' || ch == '�' || ch == '�' || ch == '�' || ch == '�' || ch == '�' || ch == '�' || ch == '�' || ch == '�' || ch == '�' || ch == '�' || ch == '�')
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}

bool QUMainWindow::isUmlaut(QChar character, QString lang)
{
	QChar ch = character.toLower();

	if (lang == "en")
	{
		return false;
	}
	else if (lang == "de")
	{
		if (ch == '�' || ch == '�' || ch == '�')
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else if (lang == "es")
	{
		return false;
	}
	else
	{
		return false;
	}
}

bool QUMainWindow::isDiphthong(QChar character1, QChar character2, QString lang)
{
	QChar ch1 = character1.toLower();
	QChar ch2 = character2.toLower();
	QString str = QString("%1%2").arg(ch1).arg(ch2);

	if (lang == "en")
	{
		if (str == "ou" || str == "ie" || str == "oi" || str == "oo" || str == "ea" || str == "ee" || str == "ai")
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else if (lang == "de")
	{
		if (str == "au" || str == "ei" || str == "eu")
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else if (lang == "es")
	{
		if (str == "ey" || str == "ai" || str == "oy" || str == "eu" || str == "au" || str == "ou" || str == "ie" || str == "ia" || str == "io" || str == "iu" || str == "ui" || str == "ue" || str == "ua" || str == "uo")
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}

bool QUMainWindow::isConsonant(QChar character, QString lang)
{
	QChar ch = character.toLower();

	if (lang == "en")
	{
		if (ch == 'b' || ch == 'c' || ch == 'd' || ch == 'f' || ch == 'g' || ch == 'h' || ch == 'j' || ch == 'k' || ch == 'l' || ch == 'm' || ch == 'n' || ch == 'p' || ch == 'q' || ch == 'r' || ch == 's' || ch == 't' || ch == 'v' || ch == 'w' || ch == 'x' || ch == 'y' || ch == 'z')
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else if (lang == "de")
	{
		if (ch == 'b' || ch == 'c' || ch == 'd' || ch == 'f' || ch == 'g' || ch == 'h' || ch == 'j' || ch == 'k' || ch == 'l' || ch == 'm' || ch == 'n' || ch == 'p' || ch == 'q' || ch == 'r' || ch == 's' || ch == 't' || ch == 'v' || ch == 'w' || ch == 'x' || ch == 'y' || ch == 'z' || ch == '�')
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else if (lang == "es")
	{
		if (ch == 'b' || ch == 'c' || ch == 'd' || ch == 'f' || ch == 'g' || ch == 'h' || ch == 'j' || ch == 'k' || ch == 'l' || ch == 'm' || ch == 'n' || ch == '�' || ch == 'p' || ch == 'q' || ch == 'r' || ch == 's' || ch == 't' || ch == 'v' || ch == 'w' || ch == 'x' || ch == 'y' || ch == 'z')
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}

bool QUMainWindow::isInseparable(QChar character1, QChar character2, QString lang)
{
	QChar ch1 = character1.toLower();
	QChar ch2 = character2.toLower();
	QString str = QString("%1%2").arg(ch1).arg(ch2);

	if (lang == "en" || lang == "de" || lang == "es")
	{
		if (((ch1 == 'b' || ch1 == 'c' || ch1 == 'd' || ch1 == 'f' || ch1 == 'g' || ch1 == 'p' || ch1 == 't' || ch1 == 'w') && (ch2 == 'l' || ch2 == 'r')) || (str == "rr") || (str == "ll") || ((ch1 == 'c' || ch1 == 't' ||  ch1 == 'r' || ch1 == 's' || ch1 == 'p' || ch1 == 'w') && ch2 == 'h') || (str == "ck"))
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}

bool QUMainWindow::isStrongVowel(QChar character, QString lang)
{
	QChar ch = character.toLower();

	if (lang == "es")
	{
		if (ch == 'a' || ch == 'e' || ch == 'o' || ch == '�' || ch == '�' || ch == '�')
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}

bool QUMainWindow::isHiatus(QChar character1, QChar character2, QString lang)
{
	QChar ch1 = character1.toLower();
	QChar ch2 = character2.toLower();
	QString str = QString("%1%2").arg(ch1).arg(ch2);

	if (lang == "es")
	{
		if ((isStrongVowel(ch1, lang) && isStrongVowel(ch2, lang)) || str == "��" || str == "��" || ch1 == ch2 || str == "a�" || str == "�a" || str == "e�" || str == "�e" || str == "i�" || str == "�i" || str == "o�" || str == "�o" || str == "u�" || str == "�u")
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}

// end syllabification

void QUMainWindow::on_pushButton_SyllabificateTeX_clicked()
{
	//
	QString language = ui->comboBox_Language->itemData(ui->comboBox_Language->currentIndex()).toString();
	QChar sep = '+';
	QString syllabifiedLyrics = "";
	QFile patternFile;
	QString patternEncoding;

	if (language.isEmpty()) {
		QUMessageBox::warning(this, tr("Application"),
			tr("Please choose the song's language first."));
	}
	else if (language == "Croatian") {
		patternFile.setFileName(":/hyph/hyph_hr.dic");
		patternEncoding = "ISO-8859-2";
	}
	else if (language == "Czech") {
		patternFile.setFileName(":/hyph/hyph_cs_CZ.dic");
		patternEncoding = "ISO-8859-2";
	}
	else if (language == "Danish") {
		patternFile.setFileName(":/hyph/hyph_da_DK.dic");
		patternEncoding = "ISO-8859-1";
	}
	else if (language == "Dutch") {
		patternFile.setFileName(":/hyph/hyph_nl_NL.dic");
		patternEncoding = "ISO-8859-1";
	}
	else if (language == "English") {
		patternFile.setFileName(":/hyph/hyph_en_US.dic");
		//patternFile.setFileName(":/hyph/hyph_en_GB.dic"); // British
		//patternFile.setFileName(":/hyph/hyph_en_CA.dic"); // Canadian
		patternEncoding = "ISO-8859-1";
	}
	else if (language == "Finnish") {
		patternFile.setFileName(":/hyph/hyph_fi_FI.dic");
		patternEncoding = "ISO-8859-1";
	}
	else if (language == "French") {
		patternFile.setFileName(":/hyph/hyph_fr_FR.dic");
		patternEncoding = "ISO-8859-1";
	}
	else if (language == "German") {
		patternFile.setFileName(":/hyph/hyph_de_DE.dic");
		//patternFile.setFileName(":/hyph/hyph_de_CH.dic");
		patternEncoding = "ISO-8859-1";
	}
	else if (language == "Hindi") {
		// no pattern file available
	}
	else if (language == "Italian") {
		patternFile.setFileName(":/hyph/hyph_it_IT.dic");
		patternEncoding = "ISO-8859-1";
	}
	else if (language == "Latin") {
		// no pattern file available
	}
	else if (language == "Norwegian") {
		patternFile.setFileName(":/hyph/hyph_nb_NO.dic");
		//patternFile.setFileName(":/hyph/hyph_nn_NO.dic");
		patternEncoding = "ISO-8859-1";
	}
	else if (language == "Polish") {
		patternFile.setFileName(":/hyph/hyph_pl_PL.dic");
		patternEncoding = "ISO-8859-2";
	}
	else if (language == "Portuguese") {
		patternFile.setFileName(":/hyph/hyph_pt_PT.dic");
		//patternFile.setFileName(":/hyph/hyph_pt_BR.dic");
		patternEncoding = "ISO-8859-1";
	}
	else if (language == "Russian") {
		patternFile.setFileName(":/hyph/hyph_ru_RU.dic");
		patternEncoding = "KOI8-R";
	}
	else if (language == "Slovak") {
		patternFile.setFileName(":/hyph/hyph_sk_SK.dic");
		patternEncoding = "ISO-8859-2";
	}
	else if (language == "Slowenian") {
		patternFile.setFileName(":/hyph/hyph_sl_SI.dic");
		patternEncoding = "ISO-8859-2";
	}
	else if (language == "Spanish") {
		patternFile.setFileName(":/hyph/hyph_es_ES.dic");
		patternEncoding = "ISO-8859-1";
	}
	else if (language == "Swedish") {
		patternFile.setFileName(":/hyph/hyph_sv_SE.dic");
		patternEncoding = "ISO-8859-2";
	}
	else if (language == "Turkish") {
		// no pattern file available
	}

	else {
		//
	}

	//

	if (patternFile.exists()) {
		if (patternFile.open(QFile::ReadOnly | QFile::Text)) {
			QTextStream in(&patternFile);
			//in.setCodec(QTextCodec::codecForName("UTF-8"));
			in.setCodec(QTextCodec::codecForName(patternEncoding.toLatin1()));
			QString patterns = in.readAll();
			QString lyrics = ui->plainTextEdit_InputLyrics->toPlainText();
			QStringList words = lyrics.split(QRegExp("\\s"));
			foreach (QString word, words) {
				QString currentWord = word.toLower();
				int wordLength = currentWord.length();
				int patternCount = 0;
				QStringList patternList;
				int hyphenIndicators[wordLength-1];
				for (int h = 0; h < (wordLength-1); h++) {
					hyphenIndicators[h] = 0;
				}
				for (int i = 0; i < (wordLength-1); i++) {
					QString subString = currentWord.mid(i, wordLength-i);
					//ui->plainTextEdit_OutputLyrics->appendPlainText("subString = " + subString + "\n");
					int subStringLength = subString.length();
					// pattern must start with whitespace (beginning of line)
					QString regExpString = "\\s";
					// for the beginning of the word, a '.' must occur, possibly followed by an integer
					if (i == 0) {
						regExpString += "\\.";
					}
					// otherwise, no '.', but possibly an integer
					else {
						regExpString += "\\d?";
					}
					// first two letters of the substring are mandatory
					regExpString += subString.mid(0,1) + "\\d?";
					regExpString += subString.mid(1,1) + "\\d?";

					// the following letters are optional (Spanish syllabification patterns don't have '.' at the end of words
					QString endString = "";
					if (language == "Spanish") {
						endString += "\\.?";
					}
					else {
						endString += "\\.";
					}
					for (int j = 2; j <= (subStringLength-1); j++) {
						regExpString += "(" + subString.mid(j,1);
						if (j != (subStringLength-1)) {
							regExpString += "\\d?";
						}
						endString += ")?";
					}
					regExpString += endString;
					// pattern must end with whitespace (end of line)
					regExpString += "\\s";
					//ui->plainTextEdit_OutputLyrics->appendPlainText("regExpString = " + regExpString + "\n");
					int patternSearchPos = 0;
					int patternStartPos = patterns.indexOf(QRegExp(regExpString), patternSearchPos);
					while (patternStartPos != -1) {
						patternCount++;
						int patternEndPos = patterns.indexOf(QRegExp("\\s"), patternStartPos+1);
						int patternLength = patternEndPos - patternStartPos - 1;
						QString pattern = patterns.mid(patternStartPos+1,patternLength);
						pattern = pattern.remove('.');
						patternLength = pattern.length();
						patternList << pattern;

						QString output = word.mid(0,1);
						for (int z = 0; z < (wordLength-1); z++) {
							if (hyphenIndicators[z]%2 != 0) {
								output += "+";
								output += word.mid(z+1,1);
							}
							else {
								output += word.mid(z+1,1);
							}
						}
						ui->plainTextEdit_OutputLyrics->appendPlainText(output);

						ui->plainTextEdit_OutputLyrics->appendPlainText(pattern);


						// copy hyphen indicators into hyphenIndicators for current pattern
						int numChars = 0;
						for (int k = 0; k < patternLength; k++) {
							if (pattern.at(k).isDigit()) {
								if (hyphenIndicators[i+numChars-1] < pattern.at(k).digitValue()) {
									hyphenIndicators[i+numChars-1] = pattern.at(k).digitValue();
								}
							}
							else {
								numChars++;
							}
						}

						// step ahead for next search
						patternSearchPos = patternStartPos+patternLength;
						// search again
						patternStartPos = patterns.indexOf(QRegExp(regExpString), patternSearchPos);

					}
				}
				// don't allow a single character at the end
				hyphenIndicators[wordLength-2] = 0;

				QString output = word.mid(0,1);
				for (int z = 0; z < (wordLength-1); z++) {
					if (hyphenIndicators[z]%2 != 0) {
						output += "+";
						output += word.mid(z+1,1);
					}
					else {
						output += word.mid(z+1,1);
					}
				}
				ui->plainTextEdit_OutputLyrics->appendPlainText(output);
			}
		}
	}
	else {
		QUMessageBox::warning(this, tr("Application"),
			tr("Pattern file not available."));
	}
}



void QUMainWindow::on_pushButton_EnableBPMEdit_toggled(bool checked)
{
	ui->doubleSpinBox_BPM->setReadOnly(!checked);
	if(checked){
		ui->pushButton_EnableBPMEdit->setIcon(QIcon(":/icons/lock-unlock.png"));
	}
	else {
		ui->pushButton_EnableBPMEdit->setIcon(QIcon(":/icons/lock.png"));
		BPMFromMP3 = BASS_FX_BPM_DecodeGet(_mediaStream, 0, MP3LengthTime, 0, BASS_FX_BPM_BKGRND, 0);
		BPM = BPMFromMP3;

		if (BPM == 0) {
			BPM = 50;
		}

		while (BPM <= 200) {
			BPM = BPM*2;
		}

		ui->doubleSpinBox_BPM->setValue(BPM);
		ui->label_BPMSet->setPixmap(QPixmap(":/icons/path_ok.png"));
	}

	montyTalk();
}

void QUMainWindow::on_doubleSpinBox_BPM_valueChanged(double BPMValue)
{
	BPM = BPMValue;

	if (BPM == 0) {
		BPM = 50;
	}

	while (BPM <= 200) {
		BPM = BPM*2;
	}
}

void QUMainWindow::on_pushButton_ShowWebSite_clicked()
{
	QUrl url("http://swisscharts.com/search.asp");
	url.addQueryItem("cat", "s");
	QString queryString = ui->lineEdit_Artist->text() + " " + ui->lineEdit_Title->text();
	QStringList queryStrings = queryString.split(QRegExp("(\\s+)"));
	QByteArray encodedQuery;
	foreach(QString queryString, queryStrings) {
		encodedQuery += queryString.toLatin1().toPercentEncoding() + QString("+").toLatin1();
	}
	url.addEncodedQueryItem("search", encodedQuery);
	QDesktopServices::openUrl(url);
}

void QUMainWindow::checkForUpdate(bool silent)
{
	int currentVersion = MAJOR_VERSION*100 + MINOR_VERSION*10 + PATCH_VERSION;

	QUrl url("http://usc.svn.sourceforge.net/viewvc/usc/src/latest_version.xml");
	QNetworkAccessManager *m_NetworkMngr = new QNetworkAccessManager(this);
	QNetworkReply *reply = m_NetworkMngr->get(QNetworkRequest(url));

	QEventLoop loop;
	connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
	loop.exec();
	if(reply->error() != QNetworkReply::NoError) {
		if (!silent) {
			QUMessageBox::warning(this,
					tr("Update check failed."),
					QString(tr("Is your internet connection working?")),
					BTN << ":/icons/accept.png" << "OK",
					240);
		}
		logSrv->add(QString(tr("Update check failed. Host unreachable.")), QU::Error);
		return;
	}

	QTemporaryFile tmp;
	if (!tmp.open()) {
		if (!silent) {
			QUMessageBox::warning(this,
					tr("Update check failed."),
					QString(tr("No permission to write file %1.")).arg(tmp.fileName()),
					BTN << ":/icons/accept.png" << "OK",
					240);
		}
		logSrv->add(QString(tr("Update check failed. No permission to write file %1.")).arg(tmp.fileName()), QU::Error);
		return;
	}

	tmp.write(reply->readAll());
	tmp.seek(0);
	QString line =  QString(tmp.readLine());
	QString latestVersionString = line;
	int latestVersion = line.remove('.').toInt();
	delete reply;
	tmp.remove();

	if (currentVersion < latestVersion) {
		QUMessageBox::information(this,
				tr("Update check successful."),
				QString(tr("UltraStar Creator %1.%2.%3 is <b>outdated</b>.<br><br>"
						"Download the most recent UltraStar Creator %4 <a href='http://sourceforge.net/projects/usc/'>here</a>."))
						.arg(MAJOR_VERSION).arg(MINOR_VERSION).arg(PATCH_VERSION)
						.arg(latestVersionString),
				BTN << ":/icons/accept.png" << "OK",
				240);
		logSrv->add(QString(tr("Update check successful. A new version of UltraStar Creator is available.")), QU::Information);
	} else {
		logSrv->add(QString(tr("Update check successful. UltraStar Creator is up to date.")), QU::Information);
		if (!silent) {
			QSettings settings;
			int result = QUMessageBox::information(this,
					tr("Update check successful."),
					QString(tr("UltraStar Creator %1.%2.%3 is <b>up to date</b>!"))
							.arg(MAJOR_VERSION).arg(MINOR_VERSION).arg(PATCH_VERSION),
					BTN << ":/icons/accept.png" << tr("OK. I will check again later.")
						<< ":/icons/accept.png" << tr("OK. Check automatically on startup."),
					240);

			if(result == 0) {
				settings.setValue("allowUpdateCheck", QVariant(false));
				logSrv->add(QString(tr("Automatic check for updates disabled.")), QU::Information);
			} else {
				settings.setValue("allowUpdateCheck", QVariant(true));
				logSrv->add(QString(tr("Automatic check for updates enabled.")), QU::Information);
			}
		}
	}

	montyTalk();
}

void QUMainWindow::on_comboBox_Cover_currentIndexChanged(const QString &cover)
{
	if(!cover.isEmpty())
		ui->label_CoverSet->setPixmap(QPixmap(":/icons/path_ok.png"));
	else
		ui->label_CoverSet->setPixmap(QPixmap(":/icons/path_error.png"));
}

void QUMainWindow::on_comboBox_Background_currentIndexChanged(const QString &background)
{
	if(!background.isEmpty())
		ui->label_BackgroundSet->setPixmap(QPixmap(":/icons/path_ok.png"));
	else
		ui->label_BackgroundSet->setPixmap(QPixmap(":/icons/path_error.png"));
}

void QUMainWindow::on_comboBox_Video_currentIndexChanged(const QString &video)
{
	if(!video.isEmpty())
		ui->label_VideoSet->setPixmap(QPixmap(":/icons/path_ok.png"));
	else
		ui->label_VideoSet->setPixmap(QPixmap(":/icons/path_error.png"));
}

void QUMainWindow::montyTalk(bool force) {
	if(!force && !_menu->montyBtn->isChecked())
		return;

	ui->montyArea->show();
	monty->talk(ui->montyArea->montyLbl, ui->montyArea->helpLbl);
}

void QUMainWindow::montyTalkNow() {
	montyTalk(true);
}

void QUMainWindow::montyAsk() {
	ui->montyArea->normalFrame->hide();
	ui->montyArea->askFrame->show();

	ui->montyArea->lineEdit->selectAll();
	ui->montyArea->lineEdit->setFocus();

	ui->montyArea->helpLbl->setText(tr("You can ask me something if you put some keywords in the <i>line edit</i> below and <i>accept</i>.<br><br>I'll try to understand and answer you. Multiple answers may be possible."));
}

void QUMainWindow::montyAnswer() {
	ui->montyArea->askFrame->hide();
	ui->montyArea->answerFrame->show();

	monty->answer(ui->montyArea->montyLbl, ui->montyArea->helpLbl, ui->montyArea->lineEdit->text());
}

void QUMainWindow::montyPrev() {
	monty->answer(ui->montyArea->montyLbl, ui->montyArea->helpLbl, "", true);
}

void QUMainWindow::montyNext() {
	monty->answer(ui->montyArea->montyLbl, ui->montyArea->helpLbl);
}
