#ifndef RUSH_ARDA_NETWORKBRIDGE_H__
#define RUSH_ARDA_NETWORKBRIDGE_H__

#include "LoginManager.h"
#include "Spreadsheet.h"

#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QThread>

class NetworkBridge : public QObject
///
/// Encapsulates all network operations.
///
/// Is primarily responsible for the upload of detected object data to the
/// google spreadsheets driving the google data studio reports.  Allows the
/// user to login using their google account and select a data file (corrosponding
/// to the data stored by the DataModel from a previously executed run) to
/// upload.
///
{
	Q_OBJECT;

private:
	QNetworkAccessManager*     networkAccessManager_;
	LoginManager*              loginManager_;
	google::Spreadsheet*       spreadsheet_;
	QString                    sheet_name_;

public:
	NetworkBridge();
	virtual ~NetworkBridge();

public slots:
	void upload(); 

private:
	void append_data(QJsonArray arr, QString sheetName);
	bool check_for_internet_connection();
	int  create_sheet(QString sheetName);
	void initiate_authorization();
	void prune_data(QJsonArray& data, QJsonArray* out);
	void push_data_to_server(QJsonArray& arr);
	int  reauthenticate();
	void upload_file(QString filename);

private slots:
	void login_done(bool authorized);
	void reauth_done();

};

#endif // RUSH_ARDA_NETWORKBRIDGE_H__
