
#include "LoginDialog.h"

//ui.webView -> QWebEngineView

LoginDialog::LoginDialog()
{
	//ui.setupUi( this );

	// connect( ui.webView, SIGNAL( urlChanged( QUrl ) ), this, SLOT( urlChanged( QUrl ) ) );
}

void LoginDialog::urlChanged( const std::string/*QUrl*/ &url )
{
	qInfo() << "URL =" << url;
	QString str = url.toString();
	if ( str.indexOf( "approvalCode" ) != -1 && m_authorizationCode.isEmpty() )
	{
		QStringList lst = str.split( "&" );
		for ( int i = 0; i < lst.count(); i++ )
		{
			QStringList pair = lst[ i ].split( "=" );
			if ( pair[ 0 ] == "approvalCode" )
			{
				// This actually isnt the access token, more like an inbetween step in which we obtain approval of the login.
				m_authorizationCode = pair[ 1 ];
				emit authorizationCodeObtained();

			}
		}
	}
}

QString LoginDialog::authorizationCode()
{
	return m_authorizationCode;
}

void LoginDialog::setLoginUrl( const QString& url )
{
	ui.webView->setUrl( QUrl( url ) );
}
