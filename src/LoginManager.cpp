#include "LoginManager.h"

#include "LoginDialog.h"
#include "AuthorizationCredential.h"

#include <QDir>
#include <QDebug>
#include <QApplication>
#include <QSettings>
#include <QMessageBox>
#include <QFile.h>

#include <QJsonArray.h>
#include <QJsonDocument.h>
#include <QJsonObject.h>

#include <memory>


LoginManager::LoginManager( QNetworkAccessManager* manager ) {
	m_strScope = "https://www.googleapis.com/auth/spreadsheets"; //Access to Sheets service

	m_pManager = manager;
	m_pAuthorizationCredential = new AuthorizationCredential();
	m_pLoginDialog = new LoginDialog();
	m_pLoginDialog->hide();

	refreshCredentials();
	m_pAuthorizationCredential->set_accessToken(""); //< want to make sure that we don't accidently attempt to use an old access token. 

	connect( m_pLoginDialog, SIGNAL( authorizationCodeObtained() ), this, SLOT( authorizationCodeObtained() ) );
}

LoginManager::~LoginManager()
{
	if (m_pAuthorizationCredential) delete m_pAuthorizationCredential;
	
	if (m_pLoginDialog) {
		m_pLoginDialog->close();
		delete m_pLoginDialog;
	}
}

int
LoginManager::InitFromClientSecretsPath( const QString& filename ) {
	// Open the file specified by path
	//QString filepath = QDir::current().absoluteFilePath(filename);
	QFile file(filename);
	if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
		return -1;
	}

	// Extract the contents of file and parse them as if they were valid json.
	QJsonDocument doc = QJsonDocument::fromJson( file.readAll() );
	Q_ASSERT( !doc.isEmpty() );
	QJsonObject root = doc.object();
	if ( root.isEmpty() ) {
		qCritical() << "Invalid JSON";
		return -1;
	}

	file.close();

	return InitFromClientSecretsJson( root );
}

int
LoginManager::InitFromClientSecretsJson( const QJsonObject& root ) {
	Q_ASSERT( !root.isEmpty() );

	if ( !root.contains( "installed" ) ) {
		qCritical() << "Client Secret file doesnt have an entry for installed applications required by application";
		return -1;
	}

	QJsonObject obj = root[ "installed" ].toObject();

	if ( obj.contains( "client_id" ) ) {
		m_strClientID = obj[ "client_id" ].toString();
	}
	if ( obj.contains( "client_secret" ) ) {
		m_strClientSecret = obj[ "client_secret" ].toString();
		const  QChar secret_chars[] = { m_strClientSecret.at( 0 ), m_strClientSecret.at( 1 ),
			m_strClientSecret.at( 2 ), m_strClientSecret.at( 3 ), 0 };
	}
	if ( obj.contains( "auth_uri" ) ) {
		m_strAuthorizationUri = obj[ "auth_uri" ].toString();
	}
	if ( obj.contains( "token_uri" ) ) {
		m_strTokenUri = obj[ "token_uri" ].toString();
	}
	if ( obj.contains( "redirect_uris" ) && obj["redirect_uris"].isArray() ) {
		m_strRedirectUri = obj[ "redirect_uris" ].toArray()[0].toString();
	}

	return 0;
}

void LoginManager::authenticate() {
	// If there is no refresh token loaded then attempt to start the authorization process otherwise 
	// attempt to reauthenticate the refresh token.  

	// @todo - Will possibly fail if the refresh token has expired?  Need to determine what to do in
	//			this scenario.

	if (m_pAuthorizationCredential->refreshToken() == QString("")) {
		startLogin();
	}
	else {
		reauthenticate_access_token();
	}
}

void LoginManager::startLogin() {
	m_pLoginDialog->setLoginUrl( loginUrl() );
	m_pLoginDialog->show();
}

bool LoginManager::isAuthorized() {
	return ( m_pAuthorizationCredential->accessToken() != "" );
}

AuthorizationCredential* LoginManager::get_credential() {
	return m_pAuthorizationCredential;
}

QString LoginManager::loginUrl() {
	QString str = QString( "%1?client_id=%2&redirect_uri=%3&response_type=code&scope=%4" ).
		arg( m_strAuthorizationUri ).arg( m_strClientID ).arg( m_strRedirectUri ).
		arg( m_strScope );

	return str;
}

int LoginManager::reauthenticate_access_token() {
	QUrl url(m_strTokenUri);
	QNetworkRequest request(url);

	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

	QString str = QString("client_id=%1&client_secret=%2&refresh_token=%3&grant_type=refresh_token")
		.arg(m_strClientID).arg(m_strClientSecret).arg(m_pAuthorizationCredential->refreshToken());

	QByteArray data;
	data += str;

	// Push the request to the server.
	QNetworkReply* reply = m_pManager->post(request, data);

	// Just wait for the reply.
	QEventLoop loop;
	connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
	loop.exec();

	m_pAuthorizationCredential->set_accessToken("");

	QNetworkReply::NetworkError err = reply->error();
	if (err == QNetworkReply::NoError) {
		QByteArray bts = reply->readAll();

		QJsonDocument doc = QJsonDocument::fromJson(bts);
		Q_ASSERT(doc.isObject());

		processAccessMessage(doc.object());
		refreshCredentials();
	}
	else { 
		// Handle the scenario whereby there is a communication error with google.
		qWarning() << "Failed to refresh access token";
	}


	//@todo this should probably be some kind of error code for failure instead of a bool.
	emit loginDone(isAuthorized());

	return err;
}

void LoginManager::exchangeAuthorizationCodeForAccessToken() {
	// Exchanges the received authorization code for an access token to be used to
	// "sign" all transmissions to the google api servers.

	QUrl url( m_strTokenUri );
	QNetworkRequest request( url );

	request.setHeader( QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded" );

	QString authorizationCode = m_pLoginDialog->authorizationCode().replace("%2F", "/");
	QString str = QString("code=%1&client_id=%2&client_secret=%3&redirect_uri=%4&grant_type=authorization_code")
		.arg(authorizationCode).arg(m_strClientID).arg(m_strClientSecret).arg(m_strRedirectUri);

	QByteArray data;
	data += str;

	// Push the request to the server.
	QNetworkReply* reply = m_pManager->post( request, data );

	// Just wait for the reply.
	QEventLoop loop;
	connect( reply, SIGNAL( finished() ), &loop, SLOT( quit() ) );
	loop.exec();

	QNetworkReply::NetworkError err = reply->error();

	if ( err == QNetworkReply::NoError && !isAuthorized() ) {
		QByteArray bts = reply->readAll();

		QJsonDocument doc = QJsonDocument::fromJson( bts );
		Q_ASSERT( doc.isObject() );
		
		processAccessMessage(doc.object());
		refreshCredentials();
	}
	else {
		// Handle the scenario whereby there is a communication error with google.
		qWarning() << "Failed to refresh access token";
	}

	emit loginDone(isAuthorized());
}

void LoginManager::processAccessMessage(QJsonObject& obj) {
	QString accessToken = obj["access_token"].toString();
	QString refreshToken = obj["refresh_token"].toString();
	int expires_in = obj["expires_in"].toInt();
	
	// Determine the time that expiry will occur
	time_t now = time(NULL);
	struct tm now_tm  = *localtime(&now);
	struct tm then_tm = now_tm;
	then_tm.tm_sec   += expires_in;   // add value from message.
	time_t expires_at = mktime(&then_tm);      

	// Store the access token in persistant storage.
	QSettings settings(QSettings::IniFormat, QSettings::SystemScope, "RushDigital", "RoadCone");
	if (!accessToken.isEmpty()) {
		settings.setValue("access_token", accessToken);
		settings.setValue("expires_at", expires_at);
	}

	// We only get a refresh token during the original authorization process. 
	// Note: We dont currently handle if this gets revoked for any reason.
	if (!refreshToken.isEmpty()) {
		settings.setValue("refresh_token", refreshToken);
	}
}

void LoginManager::refreshCredentials() {
	QSettings settings(QSettings::IniFormat, QSettings::SystemScope, "RushDigital", "RoadCone");

	QString accessToken = settings.value("access_token").toString();
	QString refreshToken = settings.value("refresh_token").toString();

	Q_ASSERT(m_pAuthorizationCredential);
	m_pAuthorizationCredential->set_accessToken(accessToken);
	m_pAuthorizationCredential->set_refreshToken(refreshToken);
}

void LoginManager::authorizationCodeObtained() {
	exchangeAuthorizationCodeForAccessToken();
}
