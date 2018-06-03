#ifndef FRAMEWORK_LOGINMANAGER_H
#define FRAMEWORK_LOGINMANAGER_H

class LoginDialog;
class AuthorizationCredential;

class LoginManager
{
public:
	LoginManager( QNetworkAccessManager* manager );
	virtual ~LoginManager();

	int InitFromClientSecretsPath( const QString& path );
	int InitFromClientSecretsJson( const QJsonObject& json );

	bool isAuthorized();
	AuthorizationCredential* get_credential();

	void authenticate();
	void startLogin();
	int  reauthenticate_access_token();

signals:
	void loginDone(bool status);

private:
	QString loginUrl();
	void exchangeAuthorizationCodeForAccessToken();
	void processAccessMessage(QJsonObject& obj);
	void refreshCredentials();

private slots:
	void authorizationCodeObtained();

private:
	QString m_strScope;
	QString m_strClientID;
	QString m_strClientSecret;
	QString m_strAuthorizationUri;
	QString m_strTokenUri;
	QString m_strRedirectUri;

	QNetworkAccessManager* m_pManager;
	AuthorizationCredential* m_pAuthorizationCredential;
	LoginDialog* m_pLoginDialog;

};

#endif // FRAMEWORK_LOGINMANAGER_H
