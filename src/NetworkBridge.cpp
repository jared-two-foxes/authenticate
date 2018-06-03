
#include "NetworkBridge.h"
#include "SpreadsheetsApi.h"

#include <QtAlgorithms>
#include <QDebug>
#include <QFileDialog>
#include <QGuiApplication>
#include <QJsonDocument>


const QString SPREADSHEED_ID = "1codsskMpDrwlIWoSxKMTjUih3VJnTbB-2RDTAZFae_0";

NetworkBridge::NetworkBridge()
{
	networkAccessManager_ = new QNetworkAccessManager(this);
	loginManager_ = new LoginManager(networkAccessManager_);
	loginManager_->InitFromClientSecretsPath("client_secret.json");
	spreadsheet_ = new google::Spreadsheet(SPREADSHEED_ID);

	connect(loginManager_, &LoginManager::loginDone, this, &NetworkBridge::login_done);
}

NetworkBridge::~NetworkBridge()
{
	disconnect(loginManager_, &LoginManager::loginDone, this, &NetworkBridge::login_done);

	if (loginManager_) delete loginManager_;
	if (spreadsheet_) delete spreadsheet_;
	if (networkAccessManager_) delete networkAccessManager_;
}

void
NetworkBridge::upload()
{
	if (loginManager_->isAuthorized())
	{
		QString filename = QFileDialog::getOpenFileName();
		if (!filename.isEmpty())
		{
			upload_file(filename);
		}
	}
	else if (check_for_internet_connection())
	{
		initiate_authorization();
	}
	else
	{
		qInfo() << "[INFO]: Unable to upload file, no internet connection";
	}
}

/// Attempts to ping the google server to check for a response, if found then we can assume that
/// have an internet connection.
bool 
NetworkBridge::check_for_internet_connection() 
{
	QNetworkRequest req(QUrl("http://www.google.com"));
	QNetworkReply *reply = networkAccessManager_->get(req);

	QEventLoop loop;
	connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
	loop.exec();

	return reply->bytesAvailable();
}

void 
NetworkBridge::initiate_authorization()
{
	loginManager_->authenticate();
}

int 
NetworkBridge::reauthenticate() 
{
	connect(loginManager_, SIGNAL(login_done()), this, SLOT(reauth_done()));
	int result = loginManager_->reauthenticate_access_token();
	return result;
}

void 
NetworkBridge::append_data(QJsonArray arr, QString sheetName = "Sheet1") 
{
	const QString RANGE = sheetName + "!A1:D";

	// Build the json object to send.
	QJsonObject data;
	data["range"] = RANGE;
	data["majorDimension"] = "ROWS";
	data["values"] = arr;

	// initiate the send to google api servers.
	google::SpreadsheetsApi api(networkAccessManager_);

	// Add in the new data.
	QJsonDocument doc(data);
	api.append(loginManager_->get_credential(), *spreadsheet_, RANGE, doc.toJson());
}

int 
NetworkBridge::create_sheet(QString sheetName) 
{
	QString str = QString(
		"{"
		" \"requests\":["
		"  {"
		"   \"addSheet\": {"
		"    \"properties\": {"
		"     \"title\": \"%1\","
		"     \"gridProperties\":{"
		"      \"rowCount\":10,"
		"      \"columnCount\":4"
		"     },"
		"     \"index\":0"
		"    }"
		"   }"
		"  }"
		" ]"
		"}"
	).arg(sheetName);

	// Build the json object to send.
	QJsonDocument doc = QJsonDocument::fromJson(
		str.toLocal8Bit()
	);

	// initiate the send to google api servers.
	google::SpreadsheetsApi api(networkAccessManager_);
	int result = api.batchUpdate(loginManager_->get_credential(), *spreadsheet_, doc.toJson());
	return result;
}

void 
NetworkBridge::prune_data(QJsonArray& data, QJsonArray* out) 
{
	std::map<QDateTime, QJsonArray > dictionary;

	for (auto& obj : data) 
	{
		Q_ASSERT(obj.isArray());

		QString   datestring = obj.toArray()[0].toString();  // Pull the date from the object.
		QDateTime date = QDateTime::fromString(datestring, "yyyyMMdd"); // Convert to a date

																		// Check dictionary for existing array for given day.
		auto it = dictionary.find(date);
		if (it == dictionary.end()) 
		{
			dictionary[date] = QJsonArray();
		}

		dictionary[date].append(obj);
	}

	// Push all the created arrays into out.
	for (auto& it : dictionary) 
	{
		(*out).append(it.second);
	}
}

void 
NetworkBridge::push_data_to_server(QJsonArray& arr)
{
	Q_ASSERT(loginManager_->isAuthorized());

	if (!arr.empty())
	{
		//sheet_name_ = QDateTime::currentDateTime().toString("yyyyMMdd");
		sheet_name_ = arr[0].toArray()[0].toString();
		int result = create_sheet(sheet_name_);

		QJsonArray entry;
		switch (result)
		{
			// if creation succeeded, this is the first write of the day
		case QNetworkReply::NoError:
			// Make some headers
			entry.append("date");
			entry.append("time");
			entry.append("speed");
			entry.append("plate");
			entry.append("confidence");
			entry.append("range");
			entry.append("candidate_image");
			entry.append("image_names");
			arr.prepend(entry);
			// intentional fall through

		case QNetworkReply::ProtocolInvalidOperationError:
			append_data(arr, sheet_name_);
			break;

		case QNetworkReply::AuthenticationRequiredError:
			result = reauthenticate();
			if (result == QNetworkReply::NoError)
			{
				push_data_to_server(arr);
				break;
			}
			break;

		default:
			qWarning() << "Unexpected Error:" + (QNetworkReply::NetworkError)result;
			break;
		}
	}
}

void 
NetworkBridge::upload_file(QString filename)
{
	Q_ASSERT(!filename.isEmpty());

	// Open the file and to pull current contents.
	QFile file(filename);
	if (file.open(QIODevice::ReadWrite | QIODevice::Text))
	{
		QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
		QJsonObject root = doc.object();
		if (root.contains("data"))
		{
			Q_ASSERT(root["data"].isArray());

			QJsonArray arr;
			prune_data(root["data"].toArray(), &arr);

			for (auto& obj : arr)
			{
				Q_ASSERT(obj.isArray());
				push_data_to_server(obj.toArray());
			}
		}

		file.close();
	}
}

void
NetworkBridge::login_done(bool authorized)
{
	if (authorized)
	{
		// Setup the google api for future calls.
		google::SpreadsheetsApi api(networkAccessManager_);
		api.get(loginManager_->get_credential(), spreadsheet_);
		// Attempt to select a file to upload?
		upload();
	}
}

void
NetworkBridge::reauth_done()
{
	// Authorization re-established.  No need to do anything, this should continue from where it left off.
}
